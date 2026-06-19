// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <mutex>

#include "arkweb/ohos_adapter_ndk/arkts_adapter/arkts_tts_adapter.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "content/browser/speech/tts_platform_impl.h"

namespace content {

class TtsPlatformImplOHOS : public TtsPlatformImpl {
 public:
  TtsPlatformImplOHOS() = default;
  TtsPlatformImplOHOS(const TtsPlatformImplOHOS&) = delete;
  TtsPlatformImplOHOS& operator=(const TtsPlatformImplOHOS&) = delete;

  // TtsPlatform implementation.
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;
  bool StopSpeaking() override;
  bool IsSpeaking() override;
  void GetVoices(std::vector<VoiceData>* out_voices) override;
  void Pause() override;
  void Resume() override;

  void OnSpeakFinished(int utterance_id);
  void OnSpeechEvent(int utterance_id, TtsEventType type);
  // Send an TTS event notification to the TTS controller.
  void SendTtsEvent(int utterance_id,
                    TtsEventType event_type,
                    int char_index,
                    int length = -1);
  int GetCurrentId();
  int GetCurrentPos();
  // Get the single instance of this class.
  static TtsPlatformImplOHOS* GetInstance();

  void CallCallback(int nweb, TtsState state, bool status);

 private:
  std::mutex callback_mutex_;
  std::map<int, base::OnceCallback<void(bool)>> callback_;
  base::Value::List current_args_;
  bool is_speaking_ = false;
  bool paused_ = false;
  int utterance_id_ = -1;
  std::string engine_id_;
  size_t utterance_char_position_ = 0;
  size_t utterance_length_ = 0;
};

}  // namespace content
