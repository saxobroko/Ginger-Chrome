// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_content_renderer_client.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "media/base/demuxer.h"

namespace chromecast {

// static
std::unique_ptr<shell::CastContentRendererClient>
shell::CastContentRendererClient::Create() {
  return std::make_unique<CastRuntimeContentRendererClient>();
}

CastRuntimeContentRendererClient::CastRuntimeContentRendererClient() = default;

CastRuntimeContentRendererClient::~CastRuntimeContentRendererClient() = default;

void CastRuntimeContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  CastContentRendererClient::RenderFrameCreated(render_frame);
  cast_streaming_renderer_client_.RenderFrameCreated(render_frame);
}

std::unique_ptr<::media::Demuxer>
CastRuntimeContentRendererClient::OverrideDemuxerForUrl(
    content::RenderFrame* render_frame,
    const GURL& url,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) {
  if (!render_frame->GetRenderFrameMediaPlaybackOptions()
           .is_remoting_renderer_enabled()) {
    return nullptr;
  }

  return cast_streaming_renderer_client_.OverrideDemuxerForUrl(
      render_frame, url, std::move(media_task_runner));
}

}  // namespace chromecast
