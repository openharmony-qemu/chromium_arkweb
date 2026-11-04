// Copyright (c) 2022-2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/content/browser/speech/tts_ohos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {
constexpr int INIT_NWEB = 1000000;
}
namespace content {

// static
TtsPlatformImplOHOS* TtsPlatformImplOHOS::GetInstance() {
  static base::NoDestructor<TtsPlatformImplOHOS> tts_platform;
  return tts_platform.get();
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplOHOS::GetInstance();
}

void TtsPlatformImplOHOS::CallCallback(int nweb, TtsState state, bool status) {
  LOG(INFO) << "TTS call callback nweb:" << nweb
            << " state:" << static_cast<int>(state);
  std::lock_guard<std::mutex> lck(callback_mutex_);
  switch (state) {
    case TtsState::SPEAK_START:
      TtsPlatformImplOHOS::GetInstance()->OnSpeechEvent(nweb, TTS_EVENT_START);
      break;
    case TtsState::ERROR:
      TtsPlatformImplOHOS::GetInstance()->OnSpeechEvent(nweb, TTS_EVENT_ERROR);
      TtsPlatformImplOHOS::GetInstance()->OnSpeakFinished(nweb);
      break;
    case TtsState::STOPPED:
      TtsPlatformImplOHOS::GetInstance()->OnSpeechEvent(nweb, TTS_EVENT_INTERRUPTED);
      TtsPlatformImplOHOS::GetInstance()->OnSpeakFinished(nweb);
      break;
    case TtsState::FINISHED_SUCCESS:
      if (nweb < 0) {
        break;
      }
      TtsPlatformImplOHOS::GetInstance()->OnSpeechEvent(nweb, TTS_EVENT_END);
      std::move(callback_[nweb]).Run(true);
      TtsPlatformImplOHOS::GetInstance()->OnSpeakFinished(nweb);
      break;
    default:
      break;
  }
}

bool TtsPlatformImplOHOS::PlatformImplSupported() {
  return true;
}

void TtsPlatformImplOHOS::Pause() {
  LOG(INFO) << "TTS Pause";
}
void TtsPlatformImplOHOS::Resume() {
  LOG(INFO) << "TTS Resume";
}

int TtsPlatformImplOHOS::GetCurrentId() {
  return utterance_id_;
}

int TtsPlatformImplOHOS::GetCurrentPos() {
  return utterance_char_position_;
}

void TtsPlatformImplOHOS::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  LOG(INFO) << "TTS pause_:" << paused_ << " is_speaking_:" << is_speaking_
            << "  " << __func__;
  engine_id_ = voice.engine_id;
  if (paused_ || is_speaking_) {
    std::move(on_speak_finished).Run(false);
    return;
  }

  is_speaking_ = true;
  utterance_id_ = utterance_id;

  {
    LOG(INFO) << "TTS register callback nweb:" << utterance_id;
    std::lock_guard<std::mutex> lck(callback_mutex_);
    callback_[utterance_id] = std::move(on_speak_finished);
  }
  ArktsTtsAdapter::GetInstance().SetText(utterance_id, utterance);

  ArktsTtsAdapter::GetInstance().Register(
      utterance_id, [this](int nweb, TtsState state, bool status) {
        this->CallCallback(nweb, state, status);
      });

  int32_t person = ArktsTtsAdapter::GetInstance().GetPersonFromDesc(voice.name);
  TtsSpeakParameters tts_params = {params.rate, params.pitch, params.volume,
                                   person, lang};
  if (lang == "") {
    auto tts = ArktsTtsAdapter::GetInstance().GetSpeakParameters();
    tts_params.language = tts.language;
  }
  ArktsTtsAdapter::GetInstance().SetSpeakParameters(tts_params);
  if (!ArktsTtsAdapter::GetInstance().HasNweb(utterance_id)) {
    utterance_char_position_ = 0;
    utterance_length_ = utterance.size();
    ArktsTtsAdapter::GetInstance().SetNweb(utterance_id);
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
         base::TaskPriority::USER_BLOCKING},
        base::BindOnce(
            [](int utteranceId) {
              ArktsTtsAdapter::GetInstance().Process(utteranceId,
                                                     TtsState::LISTENER);
            },
            utterance_id));
  }
}

void TtsPlatformImplOHOS::GetVoices(std::vector<VoiceData>* out_voices) {
  std::vector<VoiceInfo> voiceInfos =
  ArktsTtsAdapter::GetInstance().GetVoiceInfos();
  LOG(INFO) << "TTS GetVoices:" << voiceInfos.size();
  if (voiceInfos.size() > 0) {
    for (auto& it : voiceInfos) {
      out_voices->push_back(VoiceData());
      VoiceData& voice = out_voices->back();
      voice.native = true;
      voice.name = it.description;
      voice.lang = it.language;
      voice.engine_id = engine_id_;
      voice.events.insert(TTS_EVENT_START);
      voice.events.insert(TTS_EVENT_END);
      voice.events.insert(TTS_EVENT_CANCELLED);
      voice.events.insert(TTS_EVENT_MARKER);
      voice.events.insert(TTS_EVENT_PAUSE);
      voice.events.insert(TTS_EVENT_RESUME);
    }
  }
}

bool TtsPlatformImplOHOS::IsSpeaking() {
  return is_speaking_;
}

bool TtsPlatformImplOHOS::StopSpeaking() {
  LOG(INFO) << "TTS StopSpeaking";
  paused_ = false;

  is_speaking_ = false;
  utterance_id_ = -1;
  ArktsTtsAdapter::GetInstance().TransitionTo(0, TtsState::STOP);
  return true;
}

void TtsPlatformImplOHOS::OnSpeakFinished(int utterance_id) {
  LOG(INFO) << "TTS utterance_id:" << utterance_id
            << " utterance_id_:" << utterance_id_ << " " << __func__;
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (utterance_id != utterance_id_)
    return;

  DCHECK(is_speaking_);
  DCHECK_NE(utterance_id_, -1);
  is_speaking_ = false;
  utterance_id_ = -1;
}

void TtsPlatformImplOHOS::OnSpeechEvent(int utterance_id, TtsEventType type) {
  LOG(INFO) <<  "TTS type:" << static_cast<int>(type);
  utterance_char_position_ = 0;
  switch (type) {
    case TTS_EVENT_START:
      SendTtsEvent(utterance_id, TTS_EVENT_START, utterance_char_position_,
                   -1);
      break;
    case TTS_EVENT_ERROR:
      SendTtsEvent(utterance_id, TTS_EVENT_ERROR, utterance_char_position_,
                   -1);
      break;
    case TTS_EVENT_END:
      utterance_char_position_ = utterance_length_;
      SendTtsEvent(utterance_id, TTS_EVENT_END, utterance_char_position_, -1);
      break;
    default:
      break;
  }
}

void TtsPlatformImplOHOS::SendTtsEvent(int utterance_id,
                                       TtsEventType event_type,
                                       int char_index,
                                       int length) {
  LOG(INFO) << "TTS event_type:" << static_cast<int>(event_type) << " " << __func__;
  utterance_id_ = utterance_id;
  if (utterance_id < 0) {
    return;
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&TtsController::OnTtsEvent,
                                base::Unretained(TtsController::GetInstance()),
                                utterance_id, event_type, char_index, length,
                                std::string()));
}

bool TtsPlatformImplOHOS::PlatformImplInitialized() {
  static bool instance = false;
  auto infos = ArktsTtsAdapter::GetInstance().GetVoiceInfos();
  LOG(INFO) << "TTS voice size:" << infos.size() << " " << __func__;
  if (!instance && infos.size() == 0) {
    instance = true;
    ArktsTtsAdapter::GetInstance().SetVoiceQuery(true);

    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
         base::TaskPriority::USER_BLOCKING},
        base::BindOnce([]() {
          ArktsTtsAdapter::GetInstance().Process(INIT_NWEB,
                                                 TtsState::LISTVOICES);
        }));
    TtsController::GetInstance()->VoicesChanged();
  }

  return true;
}
}  // namespace content
