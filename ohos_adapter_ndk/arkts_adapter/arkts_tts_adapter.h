/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ARKTS_TTS_ADAPTER_H
#define ARKTS_TTS_ADAPTER_H

#include "napi/native_api.h"
#include "js_native_api.h"
#include <functional>
#include <string>
#include <map>
#include <variant>
#include <vector>
#include <mutex>

enum class TtsState {
    START,
    WAIT_ENGINE_CALLBACK,
    ENGINE_OK,
    LISTENER,
    SPEAK_START,
    SPEAK,
    STOP,
    STOPPED,
    PAUSE,
    RESUME,
    ERROR,
    FINISHED_SUCCESS,
    LISTVOICES,
    DOWNLOAD,
    NONE
};

typedef struct VoiceInfo {
  std::string language;
  int32_t person;
  std::string style;
  std::string gender;
  std::string description;
  std::string status;
} VoiceInfo;

typedef struct TtsSpeakParameters {
  double rate;
  double pitch;
  double volume;
  int32_t person;
  std::string language;
} TtsSpeakParameters;

typedef struct StartResponse {
  std::string audioType;
  int32_t sampleRate;
  int32_t sampleBit;
  int32_t audioChannel;
  int32_t compressRate;
} StartResponse;

typedef struct StopResponse {
  int32_t type;
  std::string message;
} StopResponse;

typedef struct CompleteResponse {
  int32_t type;
  std::string message;
} CompleteResponse;

typedef struct SynthesisResponse {
  int32_t sequence;
  std::string audioType;
} SynthesisResponse;

class TtsSpeakListener {
public:
    void OnStart(const std::string &requestId, StartResponse &response) {}
    void OnComplete(const std::string &requestId, const CompleteResponse &response) {}
    void OnStop(const std::string &requestId, const StopResponse &response) {}
    void OnError(const std::string &requestId, int32_t errorCode, const std::string &errorMessage) {}
    void OnData(const std::string &requestId, uint8_t *audio, const SynthesisResponse &response) {}
};

class NapiSpeakListener {
public:
    NapiSpeakListener() = default;
    ~NapiSpeakListener() = default;
 
    static napi_status DefineProperties(napi_env env, napi_value *object);
    static napi_value JS_OnStart(napi_env env, napi_callback_info cbinfo);
    static napi_value JS_OnComplete(napi_env env, napi_callback_info cbinfo);
    static napi_value JS_OnStop(napi_env env, napi_callback_info cbinfo);
    static napi_value JS_OnError(napi_env env, napi_callback_info cbinfo);
    static napi_value JS_OnData(napi_env env, napi_callback_info cbinfo);
};

class TtsEngineParams {
 public:
  TtsEngineParams() = default;
  TtsEngineParams(const std::string& language,
                  int32_t person,
                  int32_t online,
                  const std::map<std::string, std::string>& extraParams);
  ~TtsEngineParams() = default;

  const std::string& GetLanguage();

  int32_t GetPerson();
  int32_t GetOnline();
  const std::map<std::string, std::string> GetExtraParams();

 private:
  std::string language_;
  int32_t person_;
  int32_t online_;
  std::map<std::string, std::string> extraParams_;
};

class ArktsTtsAdapter {
 public:
  ArktsTtsAdapter() = default;
  ~ArktsTtsAdapter() = default;

  std::string GetBundleName();
  static ArktsTtsAdapter& GetInstance();

  napi_value CreateEngine(napi_env env, int nweb);
  napi_value SetListener(napi_env env, napi_value ttsEngine, int nweb);
  napi_value Speak(napi_env env,
                   napi_value ttsEngine,
                   int nweb,
                   std::string text);
  napi_value Stop(napi_env env, napi_value ttsEngine);
  napi_value GetEngine(napi_env env, napi_ref ref);
  napi_value ListVoices(napi_env env, napi_value ttsEngine, int nweb);
  napi_value SpeakExtraParamsString(napi_env env, napi_value jsExtraParams);
  napi_value SpeakExtraParamsNumber(napi_env env, napi_value jsExtraParams);

  void Exit(int nweb);
  bool IsExit(int nweb);
  void TransitionTo(int nweb, TtsState status);
  TtsState GetState(int nweb);
  void SetRef(int nweb, napi_ref ref);
  napi_ref GetRef(int nweb);
  void SetText(int nweb, const std::string& text);
  std::string GetText(int nweb);
  void SetNweb(int nweb);
  bool HasNweb(int nweb);
  void EraseNweb(int nweb);
  void Register(int nweb,
                std::function<void(int, TtsState, bool)> callback);
  void Callback(int nweb, TtsState state, bool status);
  void Process(int nweb, TtsState state);

  int32_t GetPersonFromDesc(const std::string& desc);
  void SetSpeakParameters(const TtsSpeakParameters& params);
  TtsSpeakParameters GetSpeakParameters();
  void SetCurrentId(napi_env env, int nweb);
  int GetCurrentId(napi_env env);

  std::vector<VoiceInfo> GetVoiceInfos();
  void AddVoiceInfos(VoiceInfo& voiceInfo);
  void ClearVoices();
  bool IsVoiceQuerying();
  void SetVoiceQuery(bool status);

  static napi_value CreateEngineCallback(napi_env env, napi_callback_info info);
  static napi_value ListVoicesCallback(napi_env env, napi_callback_info info);
 private:
  std::map<int, napi_ref> ttsRef_;
  std::map<int, TtsState> state_;
  std::map<int, std::string> text_;
  std::map<int, bool> exit_;
  std::map<int, int> nwebs_;
  std::map<int, napi_env> nenv_;
  std::map<int, std::function<void(int, TtsState, bool)>> callback_;
  std::mutex nwebsMtx_;
  std::vector<VoiceInfo> voiceInfos_;
  bool bVoiceQuerying_ = false;
  TtsSpeakParameters params_{1.0, 1.0, 1.0, 0, "zh-CN"};
  std::map<std::string, int32_t> langMap_;
  static uint32_t count_;
};

#endif // ARKTS_TTS_ADAPTER_H
