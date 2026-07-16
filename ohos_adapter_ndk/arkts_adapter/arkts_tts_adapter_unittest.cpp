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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../mock_ndk_api/include/mock_napi_api.h"

using namespace testing;
namespace OHOS {
namespace NWeb {

extern "C" {
OH_NativeBundle_ElementName mockName;
char mockBundle[] = "tts";
OH_NativeBundle_ElementName OH_NativeBundle_GetMainElementName() {
  mockName.abilityName = mockBundle;
  mockName.bundleName = mockBundle;
  mockName.moduleName = mockBundle;
  return mockName;
}
}

class ArktsTtsAdapterTest : public testing::Test {
 protected:
  void SetUp() override { g_MockNapiFunctions = new MockNapiFunctions(); }

  void TearDown() override {
    delete g_MockNapiFunctions;
    g_MockNapiFunctions = nullptr;
  }

  const int TEST_NWEB = 1001;
};

TEST_F(ArktsTtsAdapterTest, TtsEngineParams_001) {
  std::map<std::string, std::string> extraParams{
      {"style", "interaction-broadcast"},
      {"locate", "CN"},
      {"name", "EngineName"}};
  const std::string language = "english";
  int person = 0;
  TtsEngineParams param(language, person, 1, extraParams);
  EXPECT_EQ(param.GetLanguage(), language);
  EXPECT_EQ(param.GetPerson(), person);
}

TEST_F(ArktsTtsAdapterTest, NapiSpeakListener_DefineProperties_001_Success) {
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value object = reinterpret_cast<napi_value>(0x2);

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_define_properties(env, object, 5, testing::_))
      .WillOnce(testing::Return(napi_ok));

  napi_status status = NapiSpeakListener::DefineProperties(env, &object);
  EXPECT_EQ(status, napi_ok);
}

TEST_F(ArktsTtsAdapterTest,
       NapiSpeakListener_JS_OnComplete_001_RequestIdParseFail) {
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value argv[2];

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_cb_info(env, testing::_, testing::_, testing::_,
                               testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(2),
                               testing::SetArrayArgument<3>(argv, argv + 2),
                               testing::Return(napi_ok)));

  EXPECT_CALL(*g_MockNapiFunctions, napi_typeof(env, argv[0], testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(napi_number),
                               testing::Return(napi_ok)));

  napi_value result = NapiSpeakListener::JS_OnComplete(env, nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, NapiSpeakListener_JS_OnError_001_ArgCountNotThree) {
  napi_env env = reinterpret_cast<napi_env>(0x1);

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_cb_info(env, testing::_, testing::_, testing::_,
                               testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(2),
                               testing::Return(napi_ok)));

  napi_value result = NapiSpeakListener::JS_OnError(env, nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, GetInstance_001_SingletonPattern) {
  ArktsTtsAdapter& instance1 = ArktsTtsAdapter::GetInstance();
  ArktsTtsAdapter& instance2 = ArktsTtsAdapter::GetInstance();
  EXPECT_EQ(&instance1, &instance2);
}

TEST_F(ArktsTtsAdapterTest, SetNwebAndHasNweb_001_SetAndCheck) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;

  EXPECT_FALSE(adapter.HasNweb(nweb));
  adapter.SetNweb(nweb);
  EXPECT_TRUE(adapter.HasNweb(nweb));
  adapter.EraseNweb(nweb);
  EXPECT_FALSE(adapter.HasNweb(nweb));
}

TEST_F(ArktsTtsAdapterTest, EraseNweb_001_EraseExisting) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;

  adapter.SetNweb(nweb);
  adapter.SetText(nweb, "test");
  adapter.SetRef(nweb, reinterpret_cast<napi_ref>(0x1234));
  adapter.TransitionTo(nweb, TtsState::SPEAK_START);

  adapter.EraseNweb(nweb);
  EXPECT_FALSE(adapter.HasNweb(nweb));
  EXPECT_TRUE(adapter.GetText(nweb).empty());
}

TEST_F(ArktsTtsAdapterTest, SetTextAndGetText_001_SetAndGet) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;
  std::string testText = "Hello TTS World";

  adapter.SetNweb(nweb);
  adapter.SetText(nweb, testText);

  std::string result = adapter.GetText(nweb);
  EXPECT_EQ(result, testText);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, TransitionToAndGetState_001_ValidState) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;

  adapter.SetNweb(nweb);
  adapter.TransitionTo(nweb, TtsState::START);
  TtsState result = adapter.GetState(nweb);
  EXPECT_EQ(result, TtsState::START);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, TransitionTo_002_StopWorkAllNwebs) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb1 = TEST_NWEB;
  int nweb2 = TEST_NWEB + 1;

  adapter.SetNweb(nweb1);
  adapter.SetNweb(nweb2);
  adapter.TransitionTo(nweb1, TtsState::SPEAK);
  adapter.TransitionTo(nweb2, TtsState::LISTENER);

  adapter.TransitionTo(nweb1, TtsState::STOP);
  adapter.TransitionTo(nweb2, TtsState::STOP);

  EXPECT_EQ(adapter.GetState(nweb1), TtsState::STOP);
  EXPECT_EQ(adapter.GetState(nweb2), TtsState::STOP);

  adapter.Exit(nweb1);
  adapter.Exit(nweb2);

  adapter.EraseNweb(nweb1);
  adapter.EraseNweb(nweb2);
}

TEST_F(ArktsTtsAdapterTest, ExitAndIsExit_001_ExitNotSet) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;

  adapter.SetNweb(nweb);
  bool result = adapter.IsExit(nweb);
  EXPECT_TRUE(result);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, ExitAndIsExit_002_ExitSet) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;

  adapter.SetNweb(nweb);
  adapter.Exit(nweb);
  bool result = adapter.IsExit(nweb);
  EXPECT_TRUE(result);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, SetRefAndGetRef_001_SetAndGet) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;
  napi_ref testRef = reinterpret_cast<napi_ref>(0x1234);

  adapter.SetNweb(nweb);
  adapter.SetRef(nweb, testRef);
  napi_ref result = adapter.GetRef(nweb);
  EXPECT_EQ(result, testRef);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, RegisterAndCallback_001_RegisterAndCall) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;
  bool callbackCalled = false;
  bool callbackStatus = false;
  TtsState state = TtsState::NONE;

  adapter.SetNweb(nweb);
  adapter.Register(nweb, [&](int id, TtsState ttsState, bool status) {
    callbackCalled = true;
    state = ttsState;
    EXPECT_EQ(id, nweb);
    callbackStatus = status;
  });

  adapter.Callback(nweb, state, true);
  EXPECT_TRUE(callbackCalled);
  EXPECT_TRUE(callbackStatus);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, RegisterAndCallback_002_CallbackMultipleTimes) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;
  int callCount = 0;
  TtsState state = TtsState::FINISHED_SUCCESS;

  adapter.SetNweb(nweb);
  adapter.Register(nweb, [&](int id, TtsState ttsState, bool status) {
    if (status) {
      callCount++;
    }
  });

  adapter.Callback(nweb, state, true);
  adapter.Callback(nweb, state, false);
  EXPECT_EQ(callCount, 1);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, GetVoiceInfos_001_EmptyVector) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  std::vector<VoiceInfo> voices = adapter.GetVoiceInfos();
  EXPECT_TRUE(voices.empty());
}

TEST_F(ArktsTtsAdapterTest, AddVoiceInfos_001_SingleVoice) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  VoiceInfo voice = {"zh-CN", 1, "news", "female", "Test", "available"};

  adapter.AddVoiceInfos(voice);
  std::vector<VoiceInfo> voices = adapter.GetVoiceInfos();
  EXPECT_EQ(voices.size(), 1);
  EXPECT_EQ(voices[0].language, "zh-CN");
}

TEST_F(ArktsTtsAdapterTest, IsVoiceQuerying_001_InitialFalse) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  bool result = adapter.IsVoiceQuerying();
  EXPECT_FALSE(result);
}

TEST_F(ArktsTtsAdapterTest, SetVoiceQuery_001_SetToTrue) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  adapter.SetVoiceQuery(true);
  bool result = adapter.IsVoiceQuerying();
  EXPECT_TRUE(result);
}

TEST_F(ArktsTtsAdapterTest, TtsEngineParams_001_ConstructorAndGetters) {
  std::map<std::string, std::string> extraParams = {{"key1", "value1"},
                                                    {"key2", "value2"}};
  TtsEngineParams params("en-US", 2, 1, extraParams);

  EXPECT_EQ(params.GetLanguage(), "en-US");
  EXPECT_EQ(params.GetPerson(), 2);
  EXPECT_EQ(params.GetOnline(), 1);

  auto retrievedParams = params.GetExtraParams();
  EXPECT_EQ(retrievedParams.size(), 2);
  EXPECT_EQ(retrievedParams["key1"], "value1");
  EXPECT_EQ(retrievedParams["key2"], "value2");
}

TEST_F(ArktsTtsAdapterTest, CreateEngine_001_LoadModuleFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  int nweb = TEST_NWEB;

  EXPECT_CALL(
      *g_MockNapiFunctions,
      napi_load_module_with_info(env, testing::StrEq("@hms.ai.textToSpeech"),
                                 testing::_, testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  napi_value result = adapter.CreateEngine(env, nweb);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, CreateEngine_002_GetNamedPropertyFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  int nweb = TEST_NWEB;
  napi_value ttsModule = reinterpret_cast<napi_value>(0x2);

  EXPECT_CALL(
      *g_MockNapiFunctions,
      napi_load_module_with_info(env, testing::_, testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(ttsModule),
                               testing::Return(napi_ok)));

  EXPECT_CALL(
      *g_MockNapiFunctions,
      napi_get_named_property(env, ttsModule, "createEngine", testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  napi_value result = adapter.CreateEngine(env, nweb);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, SetListener_001_NullEngine) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = nullptr;
  int nweb = TEST_NWEB;

  napi_value result = adapter.SetListener(env, ttsEngine, nweb);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, SetListener_002_GetNamedPropertyFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = reinterpret_cast<napi_value>(0x2);
  int nweb = TEST_NWEB;

  EXPECT_CALL(
      *g_MockNapiFunctions,
      napi_get_named_property(env, ttsEngine, "setListener", testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  napi_value result = adapter.SetListener(env, ttsEngine, nweb);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, Speak_001_NullEngine) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = nullptr;
  int nweb = TEST_NWEB;
  std::string text = "Hello";

  napi_value result = adapter.Speak(env, ttsEngine, nweb, text);
  EXPECT_EQ(result, nullptr);
  adapter.Exit(nweb);
}

TEST_F(ArktsTtsAdapterTest, Speak_002_EmptyText) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = reinterpret_cast<napi_value>(0x2);
  int nweb = TEST_NWEB;
  std::string text = "";

  napi_value result = adapter.Speak(env, ttsEngine, nweb, text);
  EXPECT_EQ(result, nullptr);
  adapter.Exit(nweb);
}

TEST_F(ArktsTtsAdapterTest, Stop_001_NullEngine) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = nullptr;
  napi_value result = adapter.Stop(env, ttsEngine);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, Stop_002_GetNamedPropertyFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = reinterpret_cast<napi_value>(0x2);

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_named_property(env, ttsEngine, "stop", testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  napi_value result = adapter.Stop(env, ttsEngine);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, GetEngine_001_GetReferenceFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_ref ref = reinterpret_cast<napi_ref>(0x2);

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_reference_value(env, ref, testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  napi_value result = adapter.GetEngine(env, ref);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, GetEngine_002_Success) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_ref ref = reinterpret_cast<napi_ref>(0x2);
  napi_value ttsEngine = reinterpret_cast<napi_value>(0x3);

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_reference_value(env, ref, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(ttsEngine),
                               testing::Return(napi_ok)));

  napi_value result = adapter.GetEngine(env, ref);
  EXPECT_EQ(result, ttsEngine);
}

TEST_F(ArktsTtsAdapterTest, ListVoices_001_NullEngine) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = nullptr;
  int nweb = TEST_NWEB;

  napi_value result = adapter.ListVoices(env, ttsEngine, nweb);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, ListVoices_002_GetNamedPropertyFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value ttsEngine = reinterpret_cast<napi_value>(0x2);
  int nweb = TEST_NWEB;

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_named_property(env, ttsEngine, "listVoices", testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  napi_value result = adapter.ListVoices(env, ttsEngine, nweb);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, CreateEngineCallback_001_GetCbInfoSuccess) {
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value argv[2] = {nullptr};
  argv[0] = reinterpret_cast<napi_value>(0x0);  // BusinessError (undefined)
  argv[1] = reinterpret_cast<napi_value>(0x2);  // engine instance
  int nweb = TEST_NWEB;

  // Set up nweb and current env so GetCurrentId returns valid nweb
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  adapter.SetNweb(nweb);
  adapter.SetCurrentId(env, nweb);

  // Mock napi_get_cb_info to return argc=2 and fill argv
  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_cb_info(env, testing::_, testing::_, testing::_,
                               testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(2),
                               testing::SetArrayArgument<3>(argv, argv + 2),
                               testing::Return(napi_ok)));

  // Mock napi_create_reference (refcount=1 matches real implementation)
  EXPECT_CALL(*g_MockNapiFunctions,
              napi_create_reference(env, argv[1], 1, testing::_))
      .WillOnce(testing::Return(napi_ok));

  // Mock napi_typeof for BusinessError check (argv[0] should be undefined)
  EXPECT_CALL(*g_MockNapiFunctions,
              napi_typeof(env, argv[0], testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(napi_undefined),
                               testing::Return(napi_ok)));

  napi_value result = ArktsTtsAdapter::CreateEngineCallback(env, nullptr);
  EXPECT_EQ(result, nullptr);

  // Verify the state was transitioned to ENGINE_OK
  EXPECT_EQ(adapter.GetState(nweb), TtsState::ENGINE_OK);

  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, CreateEngineCallback_002_GetNamedPropertyFailure) {
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value argv[2];

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_cb_info(env, testing::_, testing::_, testing::_,
                               testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(2),
                               testing::SetArrayArgument<3>(argv, argv + 2),
                               testing::Return(napi_ok)));

  napi_value result = ArktsTtsAdapter::CreateEngineCallback(env, nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, ListVoicesCallback_001_GetCbInfoFailure) {
  napi_env env = reinterpret_cast<napi_env>(0x1);

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_cb_info(env, testing::_, testing::_, testing::_,
                               testing::_, testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  napi_value result = ArktsTtsAdapter::ListVoicesCallback(env, nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, ListVoicesCallback_002_GetBusinessErrorFailure) {
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_value argv[2];

  EXPECT_CALL(*g_MockNapiFunctions,
              napi_get_cb_info(env, testing::_, testing::_, testing::_,
                               testing::_, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(2),
                               testing::SetArrayArgument<3>(argv, argv + 2),
                               testing::Return(napi_ok)));

  napi_value result = ArktsTtsAdapter::ListVoicesCallback(env, nullptr);
  EXPECT_EQ(result, nullptr);
}

TEST_F(ArktsTtsAdapterTest, Process_001_CreateArkRuntimeFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;

  EXPECT_CALL(*g_MockNapiFunctions, napi_create_ark_runtime(testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  adapter.SetNweb(nweb);
  adapter.Process(nweb, TtsState::NONE);
  adapter.Exit(nweb);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, Process_002_OpenHandleScopeFailure) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;
  napi_env env = reinterpret_cast<napi_env>(0x1);

  EXPECT_CALL(*g_MockNapiFunctions, napi_create_ark_runtime(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(env),
                               testing::Return(napi_ok)));

  EXPECT_CALL(*g_MockNapiFunctions, napi_open_handle_scope(env, testing::_))
      .WillOnce(testing::Return(napi_invalid_arg));

  EXPECT_CALL(*g_MockNapiFunctions, napi_destroy_ark_runtime(testing::_))
      .WillOnce(testing::Return(napi_ok));

  adapter.SetNweb(nweb);
  adapter.Process(nweb, TtsState::NONE);
  adapter.Exit(nweb);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, Process_003_ExitImmediately) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;
  napi_env env = reinterpret_cast<napi_env>(0x1);
  napi_handle_scope scope = reinterpret_cast<napi_handle_scope>(0x2);

  EXPECT_CALL(*g_MockNapiFunctions, napi_create_ark_runtime(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(env),
                               testing::Return(napi_ok)));

  EXPECT_CALL(*g_MockNapiFunctions, napi_open_handle_scope(env, testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<1>(scope),
                               testing::Return(napi_ok)));

  EXPECT_CALL(*g_MockNapiFunctions, napi_close_handle_scope(env, scope))
      .WillOnce(testing::Return(napi_ok));

  EXPECT_CALL(*g_MockNapiFunctions, napi_destroy_ark_runtime(testing::_))
      .WillOnce(testing::Return(napi_ok));

  adapter.SetNweb(nweb);
  adapter.Exit(nweb);
  adapter.Process(nweb, TtsState::NONE);
  adapter.Exit(nweb);
  adapter.EraseNweb(nweb);
}

TEST_F(ArktsTtsAdapterTest, Process_004_AllStateEnums) {
  ArktsTtsAdapter& adapter = ArktsTtsAdapter::GetInstance();
  int nweb = TEST_NWEB;

  adapter.SetNweb(nweb);
  for (int i = 0; i <= static_cast<int>(TtsState::NONE); i++) {
    TtsState state = static_cast<TtsState>(i);
    adapter.TransitionTo(nweb, state);
    EXPECT_EQ(adapter.GetState(nweb), state);
  }
  adapter.EraseNweb(nweb);
}
}  // namespace NWeb
}  // namespace OHOS
