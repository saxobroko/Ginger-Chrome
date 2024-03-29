// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SYNCHRONOUS_COMPOSITOR_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SYNCHRONOUS_COMPOSITOR_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/writable_shared_memory_region.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/synchronous_compositor.mojom-blink.h"
#include "third_party/blink/public/platform/input/synchronous_input_handler_proxy.h"
#include "third_party/blink/renderer/platform/widget/compositing/android_webview/synchronous_layer_tree_frame_sink.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"

namespace power_scheduler {
class PowerModeVoter;
}  // namespace power_scheduler

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace blink {

class SynchronousCompositorProxy : public blink::SynchronousInputHandler,
                                   public SynchronousLayerTreeFrameSinkClient,
                                   public mojom::blink::SynchronousCompositor {
 public:
  SynchronousCompositorProxy(
      blink::SynchronousInputHandlerProxy* input_handler_proxy);
  ~SynchronousCompositorProxy() override;

  void Init();
  void BindChannel(
      mojo::PendingRemote<mojom::blink::SynchronousCompositorControlHost>
          control_host,
      mojo::PendingAssociatedRemote<mojom::blink::SynchronousCompositorHost>
          host,
      mojo::PendingAssociatedReceiver<mojom::blink::SynchronousCompositor>
          compositor_request);

  // blink::SynchronousInputHandler overrides.
  void UpdateRootLayerState(const gfx::ScrollOffset& total_scroll_offset,
                            const gfx::ScrollOffset& max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) final;

  // SynchronousLayerTreeFrameSinkClient overrides.
  void DidActivatePendingTree() final;
  void Invalidate(bool needs_draw) final;
  void SubmitCompositorFrame(
      uint32_t layer_tree_frame_sink_id,
      const viz::LocalSurfaceId& local_surface_id,
      absl::optional<viz::CompositorFrame> frame,
      absl::optional<viz::HitTestRegionList> hit_test_region_list) final;
  void SetNeedsBeginFrames(bool needs_begin_frames) final;
  void SinkDestroyed() final;

  void SetLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink);

  mojom::blink::SyncCompositorCommonRendererParamsPtr PopulateNewCommonParams();

  // blink::mojom::SynchronousCompositor overrides.
  void DemandDrawHwAsync(
      mojom::blink::SyncCompositorDemandDrawHwParamsPtr draw_params) final;
  void DemandDrawHw(mojom::blink::SyncCompositorDemandDrawHwParamsPtr params,
                    DemandDrawHwCallback callback) final;
  void SetSharedMemory(base::WritableSharedMemoryRegion shm_region,
                       SetSharedMemoryCallback callback) final;
  void DemandDrawSw(mojom::blink::SyncCompositorDemandDrawSwParamsPtr params,
                    DemandDrawSwCallback callback) final;
  void WillSkipDraw() final;
  void ZeroSharedMemory() final;
  void ZoomBy(float zoom_delta, const gfx::Point& anchor, ZoomByCallback) final;
  void SetMemoryPolicy(uint32_t bytes_limit) final;
  void ReclaimResources(uint32_t layer_tree_frame_sink_id,
                        Vector<viz::ReturnedResource> resources) final;
  void SetScroll(const gfx::ScrollOffset& total_scroll_offset) final;
  void BeginFrame(const viz::BeginFrameArgs& args,
                  const WTF::HashMap<uint32_t, viz::FrameTimingDetails>&
                      timing_details) final;
  void SetBeginFrameSourcePaused(bool paused) final;

 protected:
  void SendAsyncRendererStateIfNeeded();
  void LayerTreeFrameSinkCreated();
  void SendBeginFrameResponse(
      mojom::blink::SyncCompositorCommonRendererParamsPtr);
  void SendDemandDrawHwAsyncReply(
      mojom::blink::SyncCompositorCommonRendererParamsPtr,
      uint32_t layer_tree_frame_sink_id,
      uint32_t metadata_version,
      const absl::optional<viz::LocalSurfaceId>& local_surface_id,
      absl::optional<viz::CompositorFrame>,
      absl::optional<viz::HitTestRegionList> hit_test_region_list);

  DemandDrawHwCallback hardware_draw_reply_;
  DemandDrawSwCallback software_draw_reply_;
  ZoomByCallback zoom_by_reply_;
  SynchronousLayerTreeFrameSink* layer_tree_frame_sink_ = nullptr;
  bool begin_frame_paused_ = false;

 private:
  void DoDemandDrawSw(mojom::blink::SyncCompositorDemandDrawSwParamsPtr params);
  uint32_t NextMetadataVersion();
  void HostDisconnected();

  struct SharedMemoryWithSize;

  blink::SynchronousInputHandlerProxy* const input_handler_proxy_;
  mojo::Remote<mojom::blink::SynchronousCompositorControlHost> control_host_;
  mojo::AssociatedRemote<mojom::blink::SynchronousCompositorHost> host_;
  mojo::AssociatedReceiver<mojom::blink::SynchronousCompositor> receiver_{this};
  bool use_in_process_zero_copy_software_draw_ = false;

  std::unique_ptr<power_scheduler::PowerModeVoter> animation_power_mode_voter_;

  const bool viz_frame_submission_enabled_;

  bool needs_begin_frames_ = false;

  // From browser.
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;

  // To browser.
  uint32_t version_ = 0;
  // |total_scroll_offset_| and |max_scroll_offset_| are in physical pixel when
  // use-zoom-for-dsf is enabled, otherwise in dip.
  gfx::ScrollOffset total_scroll_offset_;  // Modified by both.
  gfx::ScrollOffset max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  uint32_t need_invalidate_count_;
  bool invalidate_needs_draw_;
  uint32_t did_activate_pending_tree_count_;
  uint32_t metadata_version_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_INPUT_SYNCHRONOUS_COMPOSITOR_PROXY_H_
