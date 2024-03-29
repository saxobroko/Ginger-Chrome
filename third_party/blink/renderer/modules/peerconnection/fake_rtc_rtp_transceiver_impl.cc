// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/peerconnection/fake_rtc_rtp_transceiver_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"

namespace blink {

MediaStreamComponent* CreateMediaStreamComponent(
    const std::string& id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      String::FromUTF8(id), MediaStreamSource::kTypeAudio,
      String::FromUTF8("audio_track"), false);
  auto audio_source = std::make_unique<blink::MediaStreamAudioSource>(
      std::move(task_runner), true /* is_local_source */);
  auto* audio_source_ptr = audio_source.get();
  source->SetPlatformSource(std::move(audio_source));

  auto* component =
      MakeGarbageCollected<MediaStreamComponent>(source->Id(), source);
  audio_source_ptr->ConnectToTrack(component);
  return component;
}

FakeRTCRtpSenderImpl::FakeRTCRtpSenderImpl(
    absl::optional<std::string> track_id,
    std::vector<std::string> stream_ids,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : track_id_(std::move(track_id)),
      stream_ids_(std::move(stream_ids)),
      task_runner_(task_runner) {}

FakeRTCRtpSenderImpl::FakeRTCRtpSenderImpl(const FakeRTCRtpSenderImpl&) =
    default;

FakeRTCRtpSenderImpl::~FakeRTCRtpSenderImpl() {}

FakeRTCRtpSenderImpl& FakeRTCRtpSenderImpl::operator=(
    const FakeRTCRtpSenderImpl&) = default;

std::unique_ptr<blink::RTCRtpSenderPlatform> FakeRTCRtpSenderImpl::ShallowCopy()
    const {
  return std::make_unique<FakeRTCRtpSenderImpl>(*this);
}

uintptr_t FakeRTCRtpSenderImpl::Id() const {
  NOTIMPLEMENTED();
  return 0;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRTCRtpSenderImpl::DtlsTransport() {
  NOTIMPLEMENTED();
  return nullptr;
}

webrtc::DtlsTransportInformation
FakeRTCRtpSenderImpl::DtlsTransportInformation() {
  NOTIMPLEMENTED();
  static webrtc::DtlsTransportInformation dummy(
      webrtc::DtlsTransportState::kNew);
  return dummy;
}

MediaStreamComponent* FakeRTCRtpSenderImpl::Track() const {
  return track_id_ ? CreateMediaStreamComponent(*track_id_, task_runner_)
                   : nullptr;
}

Vector<String> FakeRTCRtpSenderImpl::StreamIds() const {
  Vector<String> wtf_stream_ids(
      static_cast<WTF::wtf_size_t>(stream_ids_.size()));
  for (wtf_size_t i = 0; i < stream_ids_.size(); ++i) {
    wtf_stream_ids[i] = String::FromUTF8(stream_ids_[i]);
  }
  return wtf_stream_ids;
}

void FakeRTCRtpSenderImpl::ReplaceTrack(MediaStreamComponent* with_track,
                                        RTCVoidRequest* request) {
  NOTIMPLEMENTED();
}

std::unique_ptr<blink::RtcDtmfSenderHandler>
FakeRTCRtpSenderImpl::GetDtmfSender() const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpSenderImpl::GetParameters()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeRTCRtpSenderImpl::SetParameters(
    Vector<webrtc::RtpEncodingParameters>,
    absl::optional<webrtc::DegradationPreference>,
    blink::RTCVoidRequest*) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSenderImpl::GetStats(RTCStatsReportCallback,
                                    const Vector<webrtc::NonStandardGroupId>&) {
  NOTIMPLEMENTED();
}

void FakeRTCRtpSenderImpl::SetStreams(const Vector<String>& stream_ids) {
  NOTIMPLEMENTED();
}

FakeRTCRtpReceiverImpl::FakeRTCRtpReceiverImpl(
    const std::string& track_id,
    std::vector<std::string> stream_ids,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : component_(CreateMediaStreamComponent(track_id, task_runner)),
      stream_ids_(std::move(stream_ids)) {}

FakeRTCRtpReceiverImpl::FakeRTCRtpReceiverImpl(const FakeRTCRtpReceiverImpl&) =
    default;

FakeRTCRtpReceiverImpl::~FakeRTCRtpReceiverImpl() {}

FakeRTCRtpReceiverImpl& FakeRTCRtpReceiverImpl::operator=(
    const FakeRTCRtpReceiverImpl&) = default;

std::unique_ptr<RTCRtpReceiverPlatform> FakeRTCRtpReceiverImpl::ShallowCopy()
    const {
  return std::make_unique<FakeRTCRtpReceiverImpl>(*this);
}

uintptr_t FakeRTCRtpReceiverImpl::Id() const {
  NOTIMPLEMENTED();
  return 0;
}

rtc::scoped_refptr<webrtc::DtlsTransportInterface>
FakeRTCRtpReceiverImpl::DtlsTransport() {
  NOTIMPLEMENTED();
  return nullptr;
}

webrtc::DtlsTransportInformation
FakeRTCRtpReceiverImpl::DtlsTransportInformation() {
  NOTIMPLEMENTED();
  static webrtc::DtlsTransportInformation dummy(
      webrtc::DtlsTransportState::kNew);
  return dummy;
}

MediaStreamComponent* FakeRTCRtpReceiverImpl::Track() const {
  return component_;
}

Vector<String> FakeRTCRtpReceiverImpl::StreamIds() const {
  Vector<String> wtf_stream_ids(stream_ids_.size());
  for (size_t i = 0; i < stream_ids_.size(); ++i) {
    wtf_stream_ids[i] = String::FromUTF8(stream_ids_[i]);
  }
  return wtf_stream_ids;
}

Vector<std::unique_ptr<RTCRtpSource>> FakeRTCRtpReceiverImpl::GetSources() {
  NOTIMPLEMENTED();
  return {};
}

void FakeRTCRtpReceiverImpl::GetStats(
    RTCStatsReportCallback,
    const Vector<webrtc::NonStandardGroupId>&) {
  NOTIMPLEMENTED();
}

std::unique_ptr<webrtc::RtpParameters> FakeRTCRtpReceiverImpl::GetParameters()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeRTCRtpReceiverImpl::SetJitterBufferMinimumDelay(
    absl::optional<double> delay_seconds) {
  NOTIMPLEMENTED();
}

FakeRTCRtpTransceiverImpl::FakeRTCRtpTransceiverImpl(
    absl::optional<std::string> mid,
    FakeRTCRtpSenderImpl sender,
    FakeRTCRtpReceiverImpl receiver,
    bool stopped,
    webrtc::RtpTransceiverDirection direction,
    absl::optional<webrtc::RtpTransceiverDirection> current_direction)
    : mid_(std::move(mid)),
      sender_(std::move(sender)),
      receiver_(std::move(receiver)),
      stopped_(stopped),
      direction_(std::move(direction)),
      current_direction_(std::move(current_direction)) {}

FakeRTCRtpTransceiverImpl::~FakeRTCRtpTransceiverImpl() {}

RTCRtpTransceiverPlatformImplementationType
FakeRTCRtpTransceiverImpl::ImplementationType() const {
  return RTCRtpTransceiverPlatformImplementationType::kFullTransceiver;
}

uintptr_t FakeRTCRtpTransceiverImpl::Id() const {
  NOTIMPLEMENTED();
  return 0u;
}

String FakeRTCRtpTransceiverImpl::Mid() const {
  return mid_ ? String::FromUTF8(*mid_) : String();
}

std::unique_ptr<blink::RTCRtpSenderPlatform> FakeRTCRtpTransceiverImpl::Sender()
    const {
  return sender_.ShallowCopy();
}

std::unique_ptr<RTCRtpReceiverPlatform> FakeRTCRtpTransceiverImpl::Receiver()
    const {
  return receiver_.ShallowCopy();
}

bool FakeRTCRtpTransceiverImpl::Stopped() const {
  return stopped_;
}

webrtc::RtpTransceiverDirection FakeRTCRtpTransceiverImpl::Direction() const {
  return direction_;
}

webrtc::RTCError FakeRTCRtpTransceiverImpl::SetDirection(
    webrtc::RtpTransceiverDirection direction) {
  NOTIMPLEMENTED();
  return webrtc::RTCError::OK();
}

absl::optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiverImpl::CurrentDirection() const {
  return current_direction_;
}

absl::optional<webrtc::RtpTransceiverDirection>
FakeRTCRtpTransceiverImpl::FiredDirection() const {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

webrtc::RTCError FakeRTCRtpTransceiverImpl::SetOfferedRtpHeaderExtensions(
    Vector<webrtc::RtpHeaderExtensionCapability> header_extensions) {
  return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_OPERATION);
}

Vector<webrtc::RtpHeaderExtensionCapability>
FakeRTCRtpTransceiverImpl::HeaderExtensionsNegotiated() const {
  return {};
}

Vector<webrtc::RtpHeaderExtensionCapability>
FakeRTCRtpTransceiverImpl::HeaderExtensionsToOffer() const {
  return {};
}

}  // namespace blink
