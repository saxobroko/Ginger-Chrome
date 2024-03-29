// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/cast_streaming_session.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/cast_streaming/browser/cast_message_port_impl.h"
#include "components/cast_streaming/browser/config_conversions.h"
#include "components/cast_streaming/browser/stream_consumer.h"
#include "components/openscreen_platform/network_util.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/timestamp_constants.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/openscreen/src/cast/streaming/receiver.h"
#include "third_party/openscreen/src/cast/streaming/receiver_session.h"

namespace {

// Timeout to stop the Session when no data is received.
constexpr base::TimeDelta kNoDataTimeout = base::TimeDelta::FromSeconds(15);

bool CreateDataPipeForStreamType(media::DemuxerStream::Type type,
                                 mojo::ScopedDataPipeProducerHandle* producer,
                                 mojo::ScopedDataPipeConsumerHandle* consumer) {
  const MojoCreateDataPipeOptions data_pipe_options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE,
      1u /* element_num_bytes */,
      media::GetDefaultDecoderBufferConverterCapacity(type)};
  MojoResult result =
      mojo::CreateDataPipe(&data_pipe_options, *producer, *consumer);
  return result == MOJO_RESULT_OK;
}

// Timeout to end the Session when no offer message is sent.
constexpr base::TimeDelta kInitTimeout = base::TimeDelta::FromSeconds(5);

}  // namespace

namespace cast_streaming {

// Owns the Open Screen ReceiverSession. The Cast Streaming Session is tied to
// the lifespan of this object.
class CastStreamingSession::Internal
    : public openscreen::cast::ReceiverSession::Client {
 public:
  Internal(CastStreamingSession::Client* client,
           std::unique_ptr<cast_api_bindings::MessagePort> message_port,
           scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(task_runner),
        environment_(&openscreen::Clock::now, &task_runner_),
        cast_message_port_impl_(
            std::move(message_port),
            base::BindOnce(&CastStreamingSession::Internal::OnCastChannelClosed,
                           base::Unretained(this))),
        client_(client) {
    DCHECK(task_runner);
    DCHECK(client_);

    // TODO(crbug.com/1087520): Add streaming session Constraints and
    // DisplayDescription.
    receiver_session_ = std::make_unique<openscreen::cast::ReceiverSession>(
        this, &environment_, &cast_message_port_impl_,
        openscreen::cast::ReceiverSession::Preferences(
            {openscreen::cast::VideoCodec::kH264,
             openscreen::cast::VideoCodec::kVp8},
            {openscreen::cast::AudioCodec::kAac,
             openscreen::cast::AudioCodec::kOpus}));

    init_timeout_timer_.Start(
        FROM_HERE, kInitTimeout,
        base::BindOnce(&CastStreamingSession::Internal::OnInitializationTimeout,
                       base::Unretained(this)));
  }

  ~Internal() final = default;

  Internal(const Internal&) = delete;
  Internal& operator=(const Internal&) = delete;

 private:
  void OnInitializationTimeout() {
    DVLOG(1) << __func__;
    DCHECK(!is_initialized_);
    client_->OnSessionEnded();
    is_initialized_ = true;
  }

  // Initializes the audio consumer with |audio_capture_config|. Returns an
  // empty Optional on failure.
  absl::optional<AudioStreamInfo> InitializeAudioConsumer(
      openscreen::cast::Receiver* audio_receiver,
      const openscreen::cast::AudioCaptureConfig& audio_capture_config) {
    DCHECK(audio_receiver);

    // Create the audio data pipe.
    mojo::ScopedDataPipeProducerHandle data_pipe_producer;
    mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
    if (!CreateDataPipeForStreamType(media::DemuxerStream::Type::AUDIO,
                                     &data_pipe_producer,
                                     &data_pipe_consumer)) {
      return absl::nullopt;
    }

    // We can use unretained pointers here because StreamConsumer is owned by
    // this object and |client_| is guaranteed to outlive this object. Here,
    // the duration is set to kNoTimestamp so the audio renderer does not block.
    // Audio frames duration is not known ahead of time in mirroring.
    audio_consumer_ = std::make_unique<StreamConsumer>(
        audio_receiver, media::kNoTimestamp, std::move(data_pipe_producer),
        base::BindRepeating(
            &CastStreamingSession::Client::OnAudioBufferReceived,
            base::Unretained(client_)),
        base::BindRepeating(&base::OneShotTimer::Reset,
                            base::Unretained(&data_timeout_timer_)));

    return AudioStreamInfo{
        AudioCaptureConfigToAudioDecoderConfig(audio_capture_config),
        std::move(data_pipe_consumer)};
  }

  // Initializes the video consumer with |video_capture_config|. Returns an
  // empty Optional on failure.
  absl::optional<VideoStreamInfo> InitializeVideoConsumer(
      openscreen::cast::Receiver* video_receiver,
      const openscreen::cast::VideoCaptureConfig& video_capture_config) {
    DCHECK(video_receiver);

    // Creare the video data pipe.
    mojo::ScopedDataPipeProducerHandle data_pipe_producer;
    mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
    if (!CreateDataPipeForStreamType(media::DemuxerStream::Type::VIDEO,
                                     &data_pipe_producer,
                                     &data_pipe_consumer)) {
      return absl::nullopt;
    }

    // We can use unretained pointers here because StreamConsumer is owned by
    // this object and |client_| is guaranteed to outlive this object.
    // |data_timeout_timer_| is also owned by this object and will outlive both
    // StreamConsumers.
    // The frame duration is set to 10 minutes to work around cases where
    // senders do not send data for a long period of time. We end up with
    // overlapping video frames but this is fine since the media pipeline mostly
    // considers the playout time when deciding which frame to present or play
    video_consumer_ = std::make_unique<StreamConsumer>(
        video_receiver, base::TimeDelta::FromMinutes(10),
        std::move(data_pipe_producer),
        base::BindRepeating(
            &CastStreamingSession::Client::OnVideoBufferReceived,
            base::Unretained(client_)),
        base::BindRepeating(&base::OneShotTimer::Reset,
                            base::Unretained(&data_timeout_timer_)));

    return VideoStreamInfo{
        VideoCaptureConfigToVideoDecoderConfig(video_capture_config),
        std::move(data_pipe_consumer)};
  }

  // openscreen::cast::ReceiverSession::Client implementation.
  void OnNegotiated(
      const openscreen::cast::ReceiverSession* session,
      openscreen::cast::ReceiverSession::ConfiguredReceivers receivers) final {
    DVLOG(1) << __func__;
    DCHECK_EQ(session, receiver_session_.get());
    init_timeout_timer_.Stop();

    bool is_new_offer = is_initialized_;
    if (is_new_offer) {
      // This is a second offer message, reinitialize the streams.
      bool existing_session_has_audio = audio_consumer_ != nullptr;
      bool existing_session_has_video = video_consumer_ != nullptr;
      audio_consumer_.reset();
      video_consumer_.reset();

      bool new_offer_has_audio = receivers.audio_receiver != nullptr;
      bool new_offer_has_video = receivers.video_receiver != nullptr;

      if (new_offer_has_audio != existing_session_has_audio ||
          new_offer_has_video != existing_session_has_video) {
        // Different audio/video configuration than in the first offer message.
        // Return early here.
        client_->OnSessionEnded();
        return;
      }
    }

    // Set |is_initialized_| now so we can return early on failure.
    is_initialized_ = true;

    absl::optional<AudioStreamInfo> audio_stream_info;
    if (receivers.audio_receiver) {
      audio_stream_info = InitializeAudioConsumer(receivers.audio_receiver,
                                                  receivers.audio_config);
      if (audio_stream_info) {
        DVLOG(1) << "Initialized audio stream. "
                 << audio_stream_info->decoder_config.AsHumanReadableString();
      } else {
        client_->OnSessionEnded();
        return;
      }
    }

    absl::optional<VideoStreamInfo> video_stream_info;
    if (receivers.video_receiver) {
      video_stream_info = InitializeVideoConsumer(receivers.video_receiver,
                                                  receivers.video_config);
      if (video_stream_info) {
        DVLOG(1) << "Initialized video stream. "
                 << video_stream_info->decoder_config.AsHumanReadableString();
      } else {
        audio_consumer_.reset();
        audio_stream_info.reset();
        client_->OnSessionEnded();
        return;
      }
    }

    // This is necessary in case the offer message had no audio and no video
    // stream.
    if (!audio_stream_info && !video_stream_info) {
      client_->OnSessionEnded();
      return;
    }

    if (is_new_offer) {
      client_->OnSessionReinitialization(std::move(audio_stream_info),
                                         std::move(video_stream_info));
    } else {
      client_->OnSessionInitialization(std::move(audio_stream_info),
                                       std::move(video_stream_info));
      data_timeout_timer_.Start(
          FROM_HERE, kNoDataTimeout,
          base::BindOnce(&CastStreamingSession::Internal::OnDataTimeout,
                         base::Unretained(this)));
    }
  }

  void OnReceiversDestroying(const openscreen::cast::ReceiverSession* session,
                             ReceiversDestroyingReason reason) final {
    // This can be called when |receiver_session_| is being destroyed, so we
    // do not sanity-check |session| here.
    DVLOG(1) << __func__;

    switch (reason) {
      case ReceiversDestroyingReason::kEndOfSession:
        audio_consumer_.reset();
        video_consumer_.reset();
        client_->OnSessionEnded();
        break;
      case ReceiversDestroyingReason::kRenegotiated:
        break;
    }
  }

  void OnError(const openscreen::cast::ReceiverSession* session,
               openscreen::Error error) final {
    DCHECK_EQ(session, receiver_session_.get());
    LOG(ERROR) << error;
    if (!is_initialized_) {
      client_->OnSessionEnded();
      is_initialized_ = true;
    }
  }

  void OnDataTimeout() {
    DVLOG(1) << __func__;
    receiver_session_.reset();
  }

  void OnCastChannelClosed() {
    DVLOG(1) << __func__;
    receiver_session_.reset();
  }

  openscreen_platform::TaskRunner task_runner_;
  openscreen::cast::Environment environment_;
  CastMessagePortImpl cast_message_port_impl_;
  std::unique_ptr<openscreen::cast::ReceiverSession> receiver_session_;
  base::OneShotTimer init_timeout_timer_;

  // Timer to trigger connection closure if no data is received for 15 seconds.
  base::OneShotTimer data_timeout_timer_;

  bool is_initialized_ = false;
  CastStreamingSession::Client* const client_;
  std::unique_ptr<StreamConsumer> audio_consumer_;
  std::unique_ptr<StreamConsumer> video_consumer_;
};

CastStreamingSession::Client::~Client() = default;
CastStreamingSession::CastStreamingSession() = default;
CastStreamingSession::~CastStreamingSession() = default;

void CastStreamingSession::Start(
    Client* client,
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DVLOG(1) << __func__;
  DCHECK(client);
  DCHECK(!internal_);
  internal_ =
      std::make_unique<Internal>(client, std::move(message_port), task_runner);
}

void CastStreamingSession::Stop() {
  DVLOG(1) << __func__;
  DCHECK(internal_);
  internal_.reset();
}

}  // namespace cast_streaming
