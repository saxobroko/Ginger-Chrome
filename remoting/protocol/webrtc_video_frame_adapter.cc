// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_frame_adapter.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace remoting {
namespace protocol {

WebrtcVideoFrameAdapter::WebrtcVideoFrameAdapter(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats)
    : frame_(std::move(frame)),
      frame_size_(frame_->size()),
      frame_stats_(std::move(frame_stats)) {}

WebrtcVideoFrameAdapter::~WebrtcVideoFrameAdapter() = default;

// static
webrtc::VideoFrame WebrtcVideoFrameAdapter::CreateVideoFrame(
    std::unique_ptr<webrtc::DesktopFrame> desktop_frame,
    std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats) {
  rtc::scoped_refptr<WebrtcVideoFrameAdapter> adapter =
      new rtc::RefCountedObject<WebrtcVideoFrameAdapter>(
          std::move(desktop_frame), std::move(frame_stats));
  return webrtc::VideoFrame::Builder().set_video_frame_buffer(adapter).build();
}

std::unique_ptr<webrtc::DesktopFrame>
WebrtcVideoFrameAdapter::TakeDesktopFrame() {
  return std::move(frame_);
}

std::unique_ptr<WebrtcVideoEncoder::FrameStats>
WebrtcVideoFrameAdapter::TakeFrameStats() {
  return std::move(frame_stats_);
}

webrtc::VideoFrameBuffer::Type WebrtcVideoFrameAdapter::type() const {
  return Type::kNative;
}

int WebrtcVideoFrameAdapter::width() const {
  return frame_size_.width();
}

int WebrtcVideoFrameAdapter::height() const {
  return frame_size_.height();
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebrtcVideoFrameAdapter::ToI420() {
  // Strictly speaking all adapters must implement ToI420(), so that if the
  // external encoder fails, an internal libvpx could be used. But the
  // remoting encoder already uses libvpx, so there's no reason for fallback to
  // happen.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace protocol
}  // namespace remoting
