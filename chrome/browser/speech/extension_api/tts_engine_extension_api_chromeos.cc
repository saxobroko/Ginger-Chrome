// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_api_chromeos.h"

#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"

TtsExtensionEngine* TtsExtensionEngine::GetInstance() {
  static base::NoDestructor<TtsExtensionEngineChromeOS> tts_extension_engine;
  return tts_extension_engine.get();
}

TtsExtensionEngineChromeOS::TtsExtensionEngineChromeOS() = default;
TtsExtensionEngineChromeOS::~TtsExtensionEngineChromeOS() = default;

void TtsExtensionEngineChromeOS::SendAudioBuffer(
    int utterance_id,
    const std::vector<float>& audio_buffer,
    int char_index,
    bool is_last_buffer) {
  if (utterance_id != current_utterance_id_)
    return;

  playback_tts_stream_->SendAudioBuffer(audio_buffer, char_index,
                                        is_last_buffer);
}

void TtsExtensionEngineChromeOS::Speak(content::TtsUtterance* utterance,
                                       const content::VoiceData& voice) {
  Profile* profile =
      Profile::FromBrowserContext(utterance->GetBrowserContext());
  const std::string& engine_id = utterance->GetEngineId();
  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  if (!event_router->ExtensionHasEventListener(
          engine_id, tts_engine_events::kOnSpeakWithAudioStream)) {
    TtsExtensionEngine::Speak(utterance, voice);
    return;
  }

  current_utterance_id_ = utterance->GetId();
  current_utterance_length_ = utterance->GetText().size();
  current_volume_ = utterance->GetContinuousParameters().volume;
  if (!current_utterance_profile_observer_.IsObservingSource(profile)) {
    current_utterance_profile_observer_.Reset();
    current_utterance_profile_observer_.Observe(profile);
  }

  std::unique_ptr<base::ListValue> args = BuildSpeakArgs(utterance, voice);
  if (playback_tts_stream_) {
    Play(event_router, std::move(args), engine_id, profile);
    return;
  }

  TtsEngineExtensionObserverChromeOS::GetInstance(profile)
      ->BindPlaybackTtsStream(
          playback_tts_stream_.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](extensions::EventRouter* event_router,
                 std::unique_ptr<base::ListValue> args,
                 const std::string& engine_id, Profile* profile,
                 TtsExtensionEngineChromeOS* owner, int32_t sample_rate,
                 int32_t buffer_size) {
                // |owner| is always valid because TtsExtensionEngine is a
                // singleton.
                owner->UpdateAudioStreamOptions(sample_rate, buffer_size);
                owner->Play(event_router, std::move(args), engine_id, profile);
              },
              event_router, std::move(args), engine_id, profile, this));

  playback_tts_stream_.set_disconnect_handler(base::BindOnce(
      [](mojo::Remote<chromeos::tts::mojom::PlaybackTtsStream>* stream) {
        stream->reset();
      },
      &playback_tts_stream_));
}

void TtsExtensionEngineChromeOS::Stop(content::TtsUtterance* utterance) {
  TtsExtensionEngine::Stop(utterance);
  if (playback_tts_stream_)
    playback_tts_stream_->Stop();
}

void TtsExtensionEngineChromeOS::Pause(content::TtsUtterance* utterance) {
  TtsExtensionEngine::Pause(utterance);
  if (playback_tts_stream_)
    playback_tts_stream_->Pause();
}

void TtsExtensionEngineChromeOS::Resume(content::TtsUtterance* utterance) {
  TtsExtensionEngine::Resume(utterance);
  if (playback_tts_stream_)
    playback_tts_stream_->Resume();
}

void TtsExtensionEngineChromeOS::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {
  if (disable_built_in_tts_engine_for_testing_)
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Load the component extensions into this profile.
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extension_service);
  extension_service->component_loader()->AddChromeOsSpeechSynthesisExtensions();
}

bool TtsExtensionEngineChromeOS::IsBuiltInTtsEngineInitialized(
    content::BrowserContext* browser_context) {
  if (!browser_context || disable_built_in_tts_engine_for_testing_)
    return true;

  std::vector<content::VoiceData> voices;
  GetVoices(browser_context, &voices);
  bool saw_google_tts = false;
  bool saw_espeak = false;
  for (const auto& voice : voices) {
    saw_google_tts |=
        voice.engine_id == extension_misc::kGoogleSpeechSynthesisExtensionId;
    saw_espeak |=
        voice.engine_id == extension_misc::kEspeakSpeechSynthesisExtensionId;
  }

  // When running on a real Chrome OS environment, require both Google tts and
  // Espeak to be initialized; otherwise, only check for Espeak (i.e. on a
  // non-Chrome OS linux system running the Chrome OS variant of Chrome).
  return base::SysInfo::IsRunningOnChromeOS() ? (saw_google_tts && saw_espeak)
                                              : saw_espeak;
}

void TtsExtensionEngineChromeOS::OnStart() {
  content::TtsController::GetInstance()->OnTtsEvent(
      current_utterance_id_, content::TTS_EVENT_START, 0, -1 /* length */,
      std::string());
}

void TtsExtensionEngineChromeOS::OnTimepoint(int32_t char_index) {
  content::TtsController::GetInstance()->OnTtsEvent(
      current_utterance_id_, content::TTS_EVENT_WORD, char_index,
      -1 /* length */, std::string());
}

void TtsExtensionEngineChromeOS::OnEnd() {
  content::TtsController::GetInstance()->OnTtsEvent(
      current_utterance_id_, content::TTS_EVENT_END, current_utterance_length_,
      -1 /* length */, std::string());
}

void TtsExtensionEngineChromeOS::OnError() {}

void TtsExtensionEngineChromeOS::OnProfileWillBeDestroyed(Profile* profile) {
  current_utterance_profile_observer_.Reset();
}

void TtsExtensionEngineChromeOS::UpdateAudioStreamOptions(int sample_rate,
                                                          int buffer_size) {
  sample_rate_ = sample_rate;
  buffer_size_ = buffer_size;
}

void TtsExtensionEngineChromeOS::Play(extensions::EventRouter* event_router,
                                      std::unique_ptr<base::ListValue> args,
                                      const std::string& engine_id,
                                      Profile* profile) {
  // This function can be called from a callback where args are bound, so the
  // below check guards against any unexpected profiles as well as profile
  // destruction, making most of these args pending deletion.
  if (!current_utterance_profile_observer_.IsObservingSource(profile))
    return;

  // Add audio stream options.
  auto audio_stream_options = std::make_unique<base::DictionaryValue>();
  audio_stream_options->SetInteger(tts_extension_api_constants::kSampleRateKey,
                                   sample_rate_);
  audio_stream_options->SetInteger(tts_extension_api_constants::kBufferSizeKey,
                                   buffer_size_);
  args->Append(std::move(audio_stream_options));

  // Disconnect any previous receivers.
  tts_event_observer_receiver_set_.Clear();

  // Update volume if needed.
  playback_tts_stream_->SetVolume(current_volume_);

  playback_tts_stream_->Play(base::BindOnce(
      [](mojo::ReceiverSet<chromeos::tts::mojom::TtsEventObserver>*
             receiver_set,
         TtsExtensionEngineChromeOS* owner,
         mojo::PendingReceiver<chromeos::tts::mojom::TtsEventObserver>
             pending_receiver) {
        receiver_set->Add(owner, std::move(pending_receiver));
      },
      &tts_event_observer_receiver_set_, this));

  // Finally, also notify the engine extension.
  event_router->DispatchEventToExtension(
      engine_id, std::make_unique<extensions::Event>(
                     extensions::events::TTS_ENGINE_ON_SPEAK_WITH_AUDIO_STREAM,
                     tts_engine_events::kOnSpeakWithAudioStream,
                     args->TakeList(), profile));
}
