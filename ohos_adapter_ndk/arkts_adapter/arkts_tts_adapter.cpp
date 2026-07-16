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

#include "arkts_tts_adapter.h"

#include <bundle/native_interface_bundle.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>

#include "arkweb/chromium_ext/base/ohos/render_uid_define.h"
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#include "base/logging.h"

#define DECLARE_NAPI_FUNCTION(name, func) \
  {(name), nullptr, (func), nullptr, nullptr, nullptr, napi_default, nullptr}

namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200
#define LOG_TAG "NAPI_TTS"
constexpr uint32_t TTS_INTERVAL = 10;
constexpr double RATE_MIN = 0.5;
constexpr double RATE_MAX = 2.0;
constexpr double PITCH_MIN = 0.5;
constexpr double PITCH_MAX = 2.0;
constexpr double VOLUME_MIN = 0.0;
constexpr double VOLUME_MAX = 2.0;
constexpr int32_t DEFAULT_SOUND_CHANNEL = 3;
const std::string STR_ZERO = "0";

bool NapiParseInt32(napi_env env, napi_value argv, int32_t& outValue) {
  napi_valuetype valueType = napi_undefined;

  napi_typeof(env, argv, &valueType);
  if (valueType != napi_number) {
    return false;
  }

  int32_t number = 0;
  napi_get_value_int32(env, argv, &number);
  outValue = number;

  return true;
}

bool NapiParseString(napi_env env, napi_value argv, std::string& outValue) {
  size_t bufferSize = 0;
  napi_valuetype valueType = napi_undefined;

  napi_typeof(env, argv, &valueType);
  if (valueType != napi_string) {
    LOG(ERROR) << "TTS Not a valid napi string";
    return false;
  }
  napi_get_value_string_utf8(env, argv, nullptr, 0, &bufferSize);
  if (bufferSize + 1 > UINT_MAX) {
    LOG(ERROR) << "String length is too long";
    return false;
  }
  size_t jsStringLength = 0;
  char stringValue[bufferSize + 1];
  napi_get_value_string_utf8(env, argv, stringValue, bufferSize + 1,
                             &jsStringLength);
  if (jsStringLength != bufferSize) {
    LOG(ERROR) << "TTS The length values obtained twice are different";
    return false;
  }
  outValue = stringValue;
  return true;
}

std::string GetLastSubstring(const std::string& str) {
  size_t pos = str.find_last_of(':');
  if (pos == std::string::npos) {
    return STR_ZERO;
  } else {
    return str.substr(pos + 1);
  }
}

int String2Int(const std::string& str) {
  int num = -1;
  if (str.empty()) {
    return -1;
  }
  for (char const& c : str) {
    if (std::isdigit(c) == 0) {
      return -1;
    }
  }

  num = std::atoi(str.c_str());
  return num;
}
};  // namespace

using namespace OHOS::NWeb;
uint32_t ArktsTtsAdapter::count_ = 0;

TtsEngineParams::TtsEngineParams(
    const std::string& language,
    int32_t person,
    int32_t online,
    const std::map<std::string, std::string>& extraParams)
    : language_(language),
      person_(person),
      online_(online),
      extraParams_(extraParams) {}

const std::string& TtsEngineParams::GetLanguage() {
  return language_;
}

int32_t TtsEngineParams::GetPerson() {
  return person_;
}

int32_t TtsEngineParams::GetOnline() {
  return online_;
}

const std::map<std::string, std::string> TtsEngineParams::GetExtraParams() {
  return extraParams_;
}

napi_value NapiSpeakListener::JS_OnStart(napi_env env,
                                         napi_callback_info cbinfo) {
  LOG(INFO) << "TTS NAPI speak JS_OnStart ";
  napi_value thisVar = nullptr;
  napi_value result = nullptr;
  size_t argc = 2;
  napi_value argv[2];
  auto napi_status = napi_get_cb_info(env, cbinfo, &argc, argv, &thisVar, nullptr);
  if (argc < 2 || napi_status != napi_ok) {
    LOG(ERROR) << "TTS arguments, got " << argc;
    return result;
  }
  std::string requestId;
  if (!NapiParseString(env, argv[0], requestId)) {
    return result;
  }
  requestId = GetLastSubstring(requestId);
  LOG(INFO) << "TTS requestId:" << requestId;
  ArktsTtsAdapter::GetInstance().TransitionTo(String2Int(requestId),
                                              TtsState::SPEAK_START);
  return nullptr;
}
napi_value NapiSpeakListener::JS_OnComplete(napi_env env,
                                            napi_callback_info cbinfo) {
  LOG(INFO) << "TTS NAPI speak JS_OnComplete ";
  napi_value thisVar = nullptr;
  napi_value result = nullptr;
  int32_t type = 0;
  size_t argc = 2;
  napi_value argv[2];
  auto napi_status = napi_get_cb_info(env, cbinfo, &argc, argv, &thisVar, nullptr);
  if (argc != 2 || napi_status != napi_ok) {
    LOG(ERROR) << "TTS argc error :" << argc;
    return result;
  }
  std::string requestId;
  if (!NapiParseString(env, argv[0], requestId)) {
    LOG(ERROR) << "TTS JS_OnError requestId error ";
    return result;
  }
  requestId = GetLastSubstring(requestId);
  LOG(INFO) << "TTS requestId:" << requestId;
  napi_value objType;
  auto status = napi_get_named_property(env, argv[1], "type", &objType);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi_get_named_property CompleteResponse from JS fail";
    ArktsTtsAdapter::GetInstance().TransitionTo(String2Int(requestId),
                                                TtsState::ERROR);
    return result;
  }
  if (!NapiParseInt32(env, objType, type)) {
    LOG(ERROR) << "TTS NAPI speak complete. type error. ";
    ArktsTtsAdapter::GetInstance().TransitionTo(String2Int(requestId),
                                                TtsState::ERROR);
    return result;
  }
  if (type == 1) {
    LOG(INFO) << "TTS NAPI speak complete. TYPE is 1, stop. ID:" << requestId;
    ArktsTtsAdapter::GetInstance().TransitionTo(String2Int(requestId),
                                                TtsState::FINISHED_SUCCESS);
  }
  return result;
}

napi_value NapiSpeakListener::JS_OnStop(napi_env env,
                                        napi_callback_info cbinfo) {
  LOG(INFO) << "TTS NAPI JS_OnStop ";
  return nullptr;
}

napi_value NapiSpeakListener::JS_OnError(napi_env env,
                                         napi_callback_info cbinfo) {
  LOG(INFO) << "TTS NAPI speak JS_OnError ";
  size_t argc = 3;
  napi_value value = nullptr;
  napi_value argv[3];
  auto napi_status = napi_get_cb_info(env, cbinfo, &argc, argv, &value, nullptr);
  if (argc != 3 || napi_status != napi_ok) {
    LOG(ERROR) << "TTS arg count " << argc << " is not equal";
    return nullptr;
  }
  std::string requestId;
  if (!NapiParseString(env, argv[0], requestId)) {
    LOG(ERROR) << "TTS JS_OnError requestId error ";
    return nullptr;
  }
  requestId = GetLastSubstring(requestId);
  int errorCode;
  if (!NapiParseInt32(env, argv[1], errorCode)) {
    LOG(ERROR) << "TTS JS_OnError errorCode error ";
  }
  LOG(INFO) << "TTS speak onError:" << errorCode << " requestId:" << requestId;
  ArktsTtsAdapter::GetInstance().TransitionTo(String2Int(requestId),
                                              TtsState::ERROR);
  return nullptr;
}

napi_value NapiSpeakListener::JS_OnData(napi_env env,
                                        napi_callback_info cbinfo) {
  LOG(INFO) << "TTS NAPI speak JS_OnData ";
  return nullptr;
}

napi_status NapiSpeakListener::DefineProperties(napi_env env,
                                                napi_value* object) {
  LOG(INFO) << "TTS NAPI DefinePropertiesSpeakListener start ";
  napi_property_descriptor properties[] = {
      DECLARE_NAPI_FUNCTION("onStart", NapiSpeakListener::JS_OnStart),
      DECLARE_NAPI_FUNCTION("onComplete", NapiSpeakListener::JS_OnComplete),
      DECLARE_NAPI_FUNCTION("onStop", NapiSpeakListener::JS_OnStop),
      DECLARE_NAPI_FUNCTION("onError", NapiSpeakListener::JS_OnError),
      DECLARE_NAPI_FUNCTION("onData", NapiSpeakListener::JS_OnData),
  };
  return napi_define_properties(
      env, *object, sizeof(properties) / sizeof(properties[0]), properties);
};

ArktsTtsAdapter& ArktsTtsAdapter::GetInstance() {
  static ArktsTtsAdapter instance;
  return instance;
}

void ArktsTtsAdapter::SetNweb(int nweb) {
  LOG(INFO) << "TTS set nweb process, nweb:" << nweb;
  {
    std::lock_guard<std::mutex> lck(nwebsMtx_);
    nwebs_[nweb] = nweb;
  }
}

bool ArktsTtsAdapter::HasNweb(int nweb) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  return nwebs_.find(nweb) != nwebs_.end();
}

void ArktsTtsAdapter::EraseNweb(int nweb) {
  LOG(INFO) << "TTS erase nweb:" << nweb;
  {
    std::lock_guard<std::mutex> lck(nwebsMtx_);
    text_.erase(nweb);
    auto itRef = ttsRef_.find(nweb);
    if (itRef != ttsRef_.end()) {
      auto itEnv = nenv_.find(nweb);
      if (itEnv != nenv_.end()) {
        napi_delete_reference(itEnv->second, itRef->second);
      }
    }
    ttsRef_.erase(nweb);
    state_.erase(nweb);
    exit_.erase(nweb);
    nwebs_.erase(nweb);
    callback_.erase(nweb);
    nenv_.erase(nweb);
  }
}

void ArktsTtsAdapter::SetText(int nweb, const std::string& text) {
  LOG(INFO) << "TTS speak, nweb:" << nweb << " text length:" << text.length();
  {
    std::lock_guard<std::mutex> lck(nwebsMtx_);
    text_[nweb] = text;
  }
}

std::string ArktsTtsAdapter::GetText(int nweb) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  auto it = text_.find(nweb);
  if (it != text_.end()) {
    return text_[nweb];
  }
  return "";
}

void ArktsTtsAdapter::Exit(int nweb) {
  LOG(INFO) << "TTS web:" << nweb << " exit";
  {
    std::lock_guard<std::mutex> lck(nwebsMtx_);
    exit_[nweb] = true;
  }
}

bool ArktsTtsAdapter::IsExit(int nweb) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  auto it = exit_.find(nweb);
  if (it != exit_.end()) {
    return exit_[nweb];
  }
  return true;
}

void ArktsTtsAdapter::TransitionTo(int nweb, TtsState state) {
  std::string strState[] = {"START",       "WAIT_ENGINE_CALLBACK",
                            "ENGINE_OK",   "LISTENER",
                            "SPEAK_START", "SPEAK",
                            "STOP",        "STOPPED",
                            "PAUSE",       "RESUME",
                            "ERROR",       "FINISHED_SUCCESS",
                            "LISTVOICES",  "DOWNLOAD",
                            "NONE"};
  if (nweb == -1) {
    LOG(ERROR) << "TTS transtionTo nweb is -1";
    return;
  }
  int iState = static_cast<int>(state);
  if (iState > static_cast<int>(TtsState::NONE) || iState < 0) {
    LOG(ERROR) << "TTS state error:" << iState;
    state = TtsState::NONE;
    iState = static_cast<int>(TtsState::NONE);
  }
  LOG(INFO) << "TTS web:" << nweb << " TransitionTo state:" << strState[iState]
            << " iState:" << iState;
  {
    std::lock_guard<std::mutex> lck(nwebsMtx_);
    if (state == TtsState::STOP) {
      for (auto it = state_.begin(); it != state_.end(); it++) {
        it->second = TtsState::STOP;
      }
    } else if (state == TtsState::START) {
      state_[nweb] = state;
      exit_[nweb] = false;
    } else {
      state_[nweb] = state;
    }
  }
}

TtsState ArktsTtsAdapter::GetState(int nweb) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  auto it = state_.find(nweb);
  if (it != state_.end()) {
    return state_[nweb];
  }
  return TtsState::NONE;
}

void ArktsTtsAdapter::SetRef(int nweb, napi_ref ref) {
  LOG(INFO) << "TTS web:" << nweb << " ref";
  {
    std::lock_guard<std::mutex> lck(nwebsMtx_);
    ttsRef_[nweb] = ref;
  }
}

napi_ref ArktsTtsAdapter::GetRef(int nweb) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  auto it = ttsRef_.find(nweb);
  if (it != ttsRef_.end()) {
    return ttsRef_[nweb];
  }
  return nullptr;
}

void ArktsTtsAdapter::Register(
    int nweb,
    std::function<void(int, TtsState, bool)> callback) {
  LOG(INFO) << "TTS web:" << nweb << " Register callback";
  {
    std::lock_guard<std::mutex> lck(nwebsMtx_);
    callback_[nweb] = callback;
  }
}

void ArktsTtsAdapter::Callback(int nweb, TtsState state, bool status) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  auto it = callback_.find(nweb);
  if (it != callback_.end()) {
    it->second(nweb, state, status);
  }
}

void ArktsTtsAdapter::Process(int nweb, TtsState state) {
  napi_env env;
  napi_status ret = napi_create_ark_runtime(&env);
  if (ret != napi_ok) {
    LOG(ERROR) << "TTS Failed to create ark runtime";
    env = nullptr;
    return;
  }
  napi_handle_scope scope;
  ret = napi_open_handle_scope(env, &scope);
  if (ret != napi_ok) {
    napi_destroy_ark_runtime(&env);
    env = nullptr;
    return;
  }
  LOG(INFO) << "TTS process nweb:" << nweb << " env:" << env;
  TransitionTo(nweb, TtsState::START);
  while (!IsExit(nweb)) {
    switch (GetState(nweb)) {
      case TtsState::START: {
        auto result = CreateEngine(env, nweb);
        if (result == nullptr) {
          Exit(nweb);
        } else {
          TransitionTo(nweb, TtsState::WAIT_ENGINE_CALLBACK);
        }
        break;
      }
      case TtsState::ENGINE_OK:
        TransitionTo(nweb, state);
        break;
      case TtsState::LISTENER:
        if (SetListener(env, GetEngine(env, GetRef(nweb)), nweb) != nullptr) {
          TransitionTo(nweb, TtsState::SPEAK);
        } else {
          Callback(nweb, TtsState::ERROR, false);
          Exit(nweb);
        }
        break;
      case TtsState::SPEAK_START:
        Callback(nweb, TtsState::SPEAK_START, true);
        TransitionTo(nweb, TtsState::NONE);
        break;
      case TtsState::SPEAK:
        Speak(env, GetEngine(env, GetRef(nweb)), nweb, GetText(nweb));
        TransitionTo(nweb, TtsState::NONE);
        break;
      case TtsState::STOP:
        LOG(INFO) << "TTS event stop, then exit. nweb:" << nweb;
        Stop(env, GetEngine(env, GetRef(nweb)));
        TransitionTo(nweb, TtsState::NONE);
        Exit(nweb);
        break;
      case TtsState::ERROR:
        LOG(INFO) << "TTS event ERROR";
        Callback(nweb, TtsState::ERROR, false);
        TransitionTo(nweb, TtsState::NONE);
        Exit(nweb);
        break;
      case TtsState::FINISHED_SUCCESS:
        LOG(INFO) << "TTS event FINISHED_SUCCESS";
        Callback(nweb, TtsState::FINISHED_SUCCESS, true);
        TransitionTo(nweb, TtsState::NONE);
        Exit(nweb);
        break;
      case TtsState::LISTVOICES:
        ListVoices(env, GetEngine(env, GetRef(nweb)), nweb);
        TransitionTo(nweb, TtsState::NONE);
        Exit(nweb);
        break;
      default:
        break;
    }
    napi_run_event_loop(env, napi_event_mode_nowait);
    std::this_thread::sleep_for(std::chrono::milliseconds(TTS_INTERVAL));
  }
  EraseNweb(nweb);

  if (scope != nullptr) {
    napi_close_handle_scope(env, scope);
    scope = nullptr;
  }
  napi_destroy_ark_runtime(&env);
}

std::string ArktsTtsAdapter::GetBundleName() {
  auto module = OH_NativeBundle_GetMainElementName();
  std::string bundleName = module.bundleName;
  std::string moduleName = module.moduleName;
  bundleName += "/";
  bundleName += moduleName;
  return bundleName;
}

void ArktsTtsAdapter::SetCurrentId(napi_env env, int nweb) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  nenv_[nweb] = env;
}

int ArktsTtsAdapter::GetCurrentId(napi_env env) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  for (auto it = nenv_.begin(); it != nenv_.end(); it++) {
    if (it->second == env) {
      return it->first;
    }
  }
  return -1;
}

napi_value ArktsTtsAdapter::CreateEngine(napi_env env, int nweb) {
  LOG(INFO) << "Test NAPI TTS create engine.";

  std::string bundleName = GetBundleName();
  napi_value ttsModule = nullptr;

  napi_status status = napi_load_module_with_info(
      env, "@hms.ai.textToSpeech", bundleName.c_str(), &ttsModule);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS open error ";
    return nullptr;
  }

  napi_value ttsFuncCreateEngine;
  status = napi_get_named_property(env, ttsModule, "createEngine",
                                   &ttsFuncCreateEngine);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS createEngine error ";
    return nullptr;
  }

  napi_value ttsCreateEngine;
  status = napi_create_function(env, "createEngine", NAPI_AUTO_LENGTH,
                       ArktsTtsAdapter::CreateEngineCallback, nullptr,
                       &ttsCreateEngine);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS createEngine napi_create_function error ";
    return nullptr;
  }
  std::map<std::string, std::string> extraParams{
      {"style", "interaction-broadcast"},
      {"locate", "CN"},
      {"name", "EngineName"}};
  napi_value jsInitParamsInfo = nullptr;

  napi_create_object(env, &jsInitParamsInfo);
  status = napi_wrap(
      env, jsInitParamsInfo,
      new TtsEngineParams(params_.language, params_.person, 1, extraParams),
      [](napi_env /* env */, void* data, void* /* hint */) {
        if (data) {
          TtsEngineParams* ttsEngineParam = static_cast<TtsEngineParams*>(data);
          delete ttsEngineParam;
        }
      },
      nullptr, nullptr);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi wrap initparas error ";
    return nullptr;
  }

  napi_value argv[2] = {jsInitParamsInfo, ttsCreateEngine};
  SetCurrentId(env, nweb);
  status = napi_call_function(env, NULL, ttsFuncCreateEngine, 2, argv, nullptr);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS createEngine napi_call_function error ";
    return nullptr;
  }
  return ttsCreateEngine;
}

napi_value ArktsTtsAdapter::CreateEngineCallback(napi_env env,
                                                 napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr};
  void* data = nullptr;

  auto napi_status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  if (argc != 2 || napi_status != napi_ok) {
    LOG(ERROR) << "TTS CreateEngineCallback argc is error";
    return nullptr;
  }
  napi_ref ttsEngineRef;
  auto status = napi_create_reference(env, argv[1], 1, &ttsEngineRef);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi_create_reference error ";
    return nullptr;
  }
  int nweb = ArktsTtsAdapter::GetInstance().GetCurrentId(env);
  if (nweb < 0) {
    napi_delete_reference(env, ttsEngineRef);
    LOG(ERROR) << "TTS nweb is error:" << nweb << " env:" << env;
    return nullptr;
  }
  LOG(INFO) << "TTS engine callback nweb:" << nweb << " env:" << env;

  ArktsTtsAdapter::GetInstance().SetRef(nweb, ttsEngineRef);

  napi_valuetype value_type = napi_undefined;
  status = napi_typeof(env, argv[0], &value_type);
  if (!(value_type == napi_undefined || value_type == napi_null) ||
      status != napi_ok) {
    LOG(ERROR) << "TTS CreateEngineCallback BusinessError typeof error: "
               << value_type;
    return nullptr;
  }
  ArktsTtsAdapter::GetInstance().TransitionTo(nweb, TtsState::ENGINE_OK);
  return nullptr;
}

napi_value ArktsTtsAdapter::SetListener(napi_env env,
                                        napi_value ttsEngine,
                                        int nweb) {
  LOG(INFO) << "TTS NAPI setListener";
  if (!ttsEngine) {
    LOG(ERROR) << "TTS NAPI ttsEngine is null";
    return nullptr;
  }
  napi_value ttsFuncSetListener;
  napi_status status = napi_get_named_property(env, ttsEngine, "setListener",
                                               &ttsFuncSetListener);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS get Listener error ";
    return nullptr;
  }
  napi_value jsSpeakListener = nullptr;
  napi_create_object(env, &jsSpeakListener);
  status = napi_wrap(
      env, jsSpeakListener, new TtsSpeakListener(),
      [](napi_env /* env */, void* data, void* /* hint */) {
        if (data) {
          TtsSpeakListener* speakListener =
              static_cast<TtsSpeakListener*>(data);
          delete speakListener;
        }
      },
      nullptr, nullptr);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi wrap speakListener error ";
    return nullptr;
  }
  status = NapiSpeakListener::DefineProperties(env, &jsSpeakListener);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS NapiSpeakListener::DefineProperties error ";
    return nullptr;
  }
  napi_value argv[1] = {jsSpeakListener};
  status =
      napi_call_function(env, ttsEngine, ttsFuncSetListener, 1, argv, nullptr);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi speakListener error ";
    return nullptr;
  }

  return ttsFuncSetListener;
}

napi_value ArktsTtsAdapter::GetEngine(napi_env env, napi_ref ref) {
  napi_value ttsEngine;
  napi_status status = napi_get_reference_value(env, ref, &ttsEngine);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS Failed to get reference engine";
    return nullptr;
  }
  return ttsEngine;
}

napi_value ArktsTtsAdapter::SpeakExtraParamsString(napi_env env,
                                                   napi_value jsExtraParams) {
  if (!jsExtraParams) {
    LOG(ERROR) << "TTS NAPI jsExtraParams is null";
    return nullptr;
  }

  napi_value languageContext;
  napi_create_string_utf8(env, params_.language.c_str(),
                          params_.language.length(), &languageContext);
  napi_status status = napi_set_named_property(
      env, jsExtraParams, "languageContext", languageContext);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak languageContext error ";
    return nullptr;
  }
  napi_value audioType;
  napi_create_string_utf8(env, "pcm", 3, &audioType);
  status = napi_set_named_property(env, jsExtraParams, "audioType", audioType);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak audioType error ";
    return nullptr;
  }

  return jsExtraParams;
}

napi_value ArktsTtsAdapter::SpeakExtraParamsNumber(napi_env env,
                                                   napi_value jsExtraParams) {
  if (!jsExtraParams) {
    LOG(ERROR) << "TTS NAPI jsExtraParams is null";
    return nullptr;
  }
  napi_value queueMode;
  napi_create_int32(env, 0, &queueMode);
  napi_status status =
      napi_set_named_property(env, jsExtraParams, "queueMode", queueMode);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak queueMode error ";
    return nullptr;
  }
  napi_value speed;
  napi_create_double(env, params_.rate, &speed);
  status = napi_set_named_property(env, jsExtraParams, "speed", speed);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak speed error ";
    return nullptr;
  }
  napi_value volume;
  napi_create_double(env, params_.volume, &volume);
  status = napi_set_named_property(env, jsExtraParams, "volume", volume);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak volume error ";
    return nullptr;
  }
  napi_value pitch;
  napi_create_double(env, params_.pitch, &pitch);
  status = napi_set_named_property(env, jsExtraParams, "pitch", pitch);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak pitch error ";
    return nullptr;
  }
  napi_value soundChannel;
  status = napi_create_int32(env, DEFAULT_SOUND_CHANNEL, &soundChannel);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi_create_int32 error ";
    return nullptr;
  }
  status =
      napi_set_named_property(env, jsExtraParams, "soundChannel", soundChannel);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak soundChannel error ";
    return nullptr;
  }
  napi_value playType;
  napi_create_int32(env, 1, &playType);
  status = napi_set_named_property(env, jsExtraParams, "playType", playType);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak playType error ";
    return nullptr;
  }
  return jsExtraParams;
}

napi_value ArktsTtsAdapter::Speak(napi_env env,
                                  napi_value ttsEngine,
                                  int nweb,
                                  std::string text) {
  LOG(INFO) << "TTS NAPI speak text length:" << text.length();

  if (!ttsEngine || text == "") {
    LOG(ERROR) << "TTS NAPI ttsEngine is null";
    return nullptr;
  }

  napi_value ttsFuncSpeak;
  napi_status status =
      napi_get_named_property(env, ttsEngine, "speak", &ttsFuncSpeak);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS get speak error ";
    return nullptr;
  }

  napi_value jsSpeakParams = nullptr;
  napi_create_object(env, &jsSpeakParams);

  std::string requestId = std::to_string(count_++) + ":" + std::to_string(nweb);
  LOG(INFO) << "TTS speak requestId:" << requestId;
  napi_value jsRequestId;
  napi_create_string_utf8(env, requestId.c_str(), requestId.length(),
                          &jsRequestId);
  status =
      napi_set_named_property(env, jsSpeakParams, "requestId", jsRequestId);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS set speak requestId error ";
    return nullptr;
  }

  napi_value jsExtraParams = nullptr;
  napi_create_object(env, &jsExtraParams);
  napi_value resultString = SpeakExtraParamsString(env, jsExtraParams);
  napi_value resultNumber = SpeakExtraParamsNumber(env, jsExtraParams);
  if (!resultString || !resultNumber) {
    LOG(ERROR) << "TTS get speak jsExtraParams error ";
    return nullptr;
  }

  status =
      napi_set_named_property(env, jsSpeakParams, "extraParams", jsExtraParams);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS get speak extraParams error ";
    return nullptr;
  }

  napi_value jsText;
  napi_create_string_utf8(env, text.c_str(), text.length(), &jsText);
  napi_value argv[2] = {jsText, jsSpeakParams};
  status = napi_call_function(env, ttsEngine, ttsFuncSpeak, 2, argv, nullptr);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi speak error ";
    return nullptr;
  }

  return nullptr;
}

napi_value ArktsTtsAdapter::Stop(napi_env env, napi_value ttsEngine) {
  LOG(INFO) << "TTS napi stop ";
  napi_value ttsFuncStop;
  if (!ttsEngine) {
    LOG(ERROR) << "TTS NAPI ttsEngine is null";
    return nullptr;
  }
  napi_status status =
      napi_get_named_property(env, ttsEngine, "stop", &ttsFuncStop);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS get stop error ";
    return nullptr;
  }
  status = napi_call_function(env, ttsEngine, ttsFuncStop, 0, nullptr, nullptr);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi stop error  ";
    return nullptr;
  }
  return nullptr;
}

napi_value ArktsTtsAdapter::ListVoices(napi_env env,
                                       napi_value ttsEngine,
                                       int nweb) {
  LOG(INFO) << "TTS napi ListVoices nweb:" << nweb;
  napi_value ttsFuncListVoices;
  if (!ttsEngine) {
    LOG(ERROR) << "TTS NAPI ttsEngine is null";
    return nullptr;
  }
  napi_status status =
      napi_get_named_property(env, ttsEngine, "listVoices", &ttsFuncListVoices);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS get listVoices error ";
    return nullptr;
  }
  napi_value jsVoiceQuery = nullptr;
  napi_create_object(env, &jsVoiceQuery);
  time_t now = time(nullptr);
  std::string requestId = std::to_string(now) + std::to_string(count_++);
  napi_value jsRequestId;
  napi_create_string_utf8(env, requestId.c_str(), requestId.length(),
                          &jsRequestId);
  LOG(INFO) << "TTS listVoice requestId:" << requestId;
  status = napi_set_named_property(env, jsVoiceQuery, "requestId", jsRequestId);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS VoiceQuery requestId error ";
    return nullptr;
  }
  napi_value online;
  napi_create_int32(env, 1, &online);
  status = napi_set_named_property(env, jsVoiceQuery, "online", online);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS VoiceQuery online error ";
    return nullptr;
  }

  napi_value ttsListVoices;
  status = napi_create_function(env, "listVoice", NAPI_AUTO_LENGTH,
                       ArktsTtsAdapter::ListVoicesCallback, nullptr,
                       &ttsListVoices);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS listVoice napi_create_function error ";
    return nullptr;
  }
  napi_value argv[2] = {jsVoiceQuery, ttsListVoices};
  status =
      napi_call_function(env, ttsEngine, ttsFuncListVoices, 2, argv, nullptr);
  if (status != napi_ok) {
    LOG(ERROR) << "TTS napi listVoice error  ";
    return nullptr;
  }
  return nullptr;
}

napi_value ArktsTtsAdapter::ListVoicesCallback(napi_env env,
                                               napi_callback_info info) {
  LOG(INFO) << "TTS ListVoicesCallback";
  size_t argc = 2;
  napi_value argv[2] = {nullptr};
  void* data = nullptr;
  auto napi_status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  if (argc != 2 || napi_status != napi_ok) {
    LOG(ERROR) << "TTS listVoiceCallback argc:" << argc;
    return nullptr;
  }

  napi_valuetype value_type = napi_undefined;
  auto result = napi_typeof(env, argv[0], &value_type);
  if (!(value_type == napi_undefined || value_type == napi_null) ||
      result != napi_ok) {
    LOG(ERROR) << "TTS ListVoicesCallback BusinessError typeof error: "
               << value_type;
    return nullptr;
  }

  napi_value array = argv[1];
  bool isArray = false;
  napi_is_array(env, array, &isArray);
  if (!isArray) {
    LOG(ERROR) << "TTS is not array";
    ArktsTtsAdapter::GetInstance().SetVoiceQuery(false);
    return nullptr;
  }

  uint32_t arrayLength = 0;
  napi_get_array_length(env, array, &arrayLength);
  LOG(INFO) << "TTS voices array length:" << arrayLength;
  for (uint32_t i = 0; i < arrayLength; ++i) {
    std::string language;
    int32_t person;
    std::string style;
    std::string gender;
    std::string description;
    std::string status;
    napi_value obj = nullptr;
    napi_value languageObj = nullptr;
    napi_value personObj = nullptr;
    napi_value styleObj = nullptr;
    napi_value genderObj = nullptr;
    napi_value descriptionObj = nullptr;
    napi_value statusObj = nullptr;
    result = napi_get_element(env, array, i, &obj);
    if (result != napi_ok) {
      LOG(ERROR) << "TTS failed to get array element";
      continue;
    }
    VoiceInfo voiceInfo;
    if ((napi_get_named_property(env, obj, "language", &languageObj) !=
         napi_ok) ||
        (napi_get_named_property(env, obj, "person", &personObj) != napi_ok) ||
        (napi_get_named_property(env, obj, "style", &styleObj) != napi_ok) ||
        (napi_get_named_property(env, obj, "gender", &genderObj) != napi_ok) ||
        (napi_get_named_property(env, obj, "status", &statusObj) != napi_ok) ||
        (napi_get_named_property(env, obj, "description", &descriptionObj) !=
         napi_ok)) {
      continue;
    }
    if (!NapiParseString(env, languageObj, voiceInfo.language) ||
        !NapiParseString(env, styleObj, voiceInfo.style) ||
        !NapiParseString(env, genderObj, voiceInfo.gender) ||
        !NapiParseString(env, descriptionObj, voiceInfo.description) ||
        !NapiParseString(env, statusObj, voiceInfo.status) ||
        !NapiParseInt32(env, personObj, voiceInfo.person)) {
      LOG(ERROR) << "TTS Unable to parse string from headers array object.";
      ArktsTtsAdapter::GetInstance().SetVoiceQuery(false);
      return nullptr;
    }
    ArktsTtsAdapter::GetInstance().AddVoiceInfos(voiceInfo);
  }
  ArktsTtsAdapter::GetInstance().SetVoiceQuery(false);
  return nullptr;
}

std::vector<VoiceInfo> ArktsTtsAdapter::GetVoiceInfos() {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  return voiceInfos_;
}

void ArktsTtsAdapter::ClearVoices() {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  voiceInfos_.clear();
}

void ArktsTtsAdapter::AddVoiceInfos(VoiceInfo& voiceInfo) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  voiceInfos_.push_back(voiceInfo);
  langMap_.insert({voiceInfo.description, voiceInfo.person});
}

int32_t ArktsTtsAdapter::GetPersonFromDesc(const std::string& desc) {
  std::lock_guard<std::mutex> lck(nwebsMtx_);
  auto it = langMap_.find(desc);
  if (it != langMap_.end()) {
    return it->second;
  }
  return 0;
}

bool ArktsTtsAdapter::IsVoiceQuerying() {
  return bVoiceQuerying_;
}

void ArktsTtsAdapter::SetVoiceQuery(bool status) {
  bVoiceQuerying_ = status;
}

void ArktsTtsAdapter::SetSpeakParameters(const TtsSpeakParameters& params) {
  params_.person = params.person;
  params_.language = params.language;
  params_.rate = params.rate;
  params_.pitch = params.pitch;
  params_.volume = params.volume;
  if (params.rate < RATE_MIN) {
    params_.rate = RATE_MIN;
  } else if (params.rate > RATE_MAX) {
    params_.rate = RATE_MAX;
  }
  if (params.pitch < PITCH_MIN) {
    params_.pitch = PITCH_MIN;
  } else if (params.pitch > PITCH_MAX) {
    params_.pitch = PITCH_MAX;
  }
  if (params.volume < VOLUME_MIN) {
    params_.volume = VOLUME_MIN;
  } else if (params.volume > VOLUME_MAX) {
    params_.volume = VOLUME_MAX;
  }
  LOG(INFO) << "TTS Speak rate:" << params_.rate << " pitch:" << params_.pitch
            << " volume:" << params_.volume << " person:" << params_.person
            << " lang:" << params_.language;
}

TtsSpeakParameters ArktsTtsAdapter::GetSpeakParameters() {
  return params_;
}
