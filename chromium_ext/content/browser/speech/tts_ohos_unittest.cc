// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "arkweb/chromium_ext/content/browser/speech/tts_ohos.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::WithArgs;
using ::testing::Property;
using ::testing::HasSubstr;

namespace content {

class MockTtsController : public TtsController {
 public:
  MockTtsController() = default;
  ~MockTtsController() override = default;
  static TtsController* GetInstance() {
    static MockTtsController instance;
    return &instance;
  }
  MOCK_METHOD(bool, IsSpeaking, (), (override));
  MOCK_METHOD(void,
              SpeakOrEnqueue,
              (std::unique_ptr<TtsUtterance>),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, Stop, (const GURL&), (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(void,
              UpdateLanguageStatus,
              (BrowserContext*,
               const std::string&,
               LanguageInstallStatus,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              AddUpdateLanguageStatusDelegate,
              (UpdateLanguageStatusDelegate*),
              (override));
  MOCK_METHOD(void,
              RemoveUpdateLanguageStatusDelegate,
              (UpdateLanguageStatusDelegate*),
              (override));
  MOCK_METHOD(void,
              UninstallLanguageRequest,
              (content::BrowserContext*,
               const std::string&,
               const std::string&,
               int,
               bool),
              (override));
  MOCK_METHOD(
      void,
      InstallLanguageRequest,
      (content::BrowserContext*, const std::string&, const std::string&, int),
      (override));
  MOCK_METHOD(
      void,
      LanguageStatusRequest,
      (content::BrowserContext*, const std::string&, const std::string&, int),
      (override));
  MOCK_METHOD(void,
              OnTtsEvent,
              (int, TtsEventType, int, int, const std::string&),
              (override));
  MOCK_METHOD(void, OnTtsUtteranceBecameInvalid, (int), (override));
  MOCK_METHOD(void,
              GetVoices,
              (BrowserContext*, const GURL&, std::vector<VoiceData>*),
              (override));
  MOCK_METHOD(void, VoicesChanged, (), (override));
  MOCK_METHOD(void,
              AddVoicesChangedDelegate,
              (VoicesChangedDelegate*),
              (override));
  MOCK_METHOD(void,
              RemoveVoicesChangedDelegate,
              (VoicesChangedDelegate*),
              (override));
  MOCK_METHOD(void,
              RemoveUtteranceEventDelegate,
              (UtteranceEventDelegate*),
              (override));
  MOCK_METHOD(void, SetTtsEngineDelegate, (TtsEngineDelegate*), (override));

  MOCK_METHOD(TtsEngineDelegate*, GetTtsEngineDelegate, (), (override));
  MOCK_METHOD(void, RefreshVoices, (), (override));
  MOCK_METHOD(void, SetTtsPlatform, (TtsPlatform*), (override));
  MOCK_METHOD(int, QueueSize, (), (override));
  MOCK_METHOD(void,
              StripSSML,
              (const std::string&,
               base::OnceCallback<void(const std::string&)>),
              (override));
};

// Mock ArktsTtsAdapter
class MockArktsTtsAdapter : public ArktsTtsAdapter {
 public:
  MockArktsTtsAdapter() = default;
  ~MockArktsTtsAdapter() = default;

  MOCK_METHOD(std::string, GetBundleName, (), ());
  MOCK_METHOD(napi_value, CreateEngine, (napi_env env, int nweb), ());
  MOCK_METHOD(napi_value, SetListener, (napi_env env, napi_value ttsEngine, int nweb), ());
  MOCK_METHOD(napi_value, Speak, (napi_env env, napi_value ttsEngine, int nweb, std::string text), ());
  MOCK_METHOD(napi_value, Stop, (napi_env env, napi_value ttsEngine), ());
  MOCK_METHOD(napi_value, GetEngine, (napi_env env, napi_ref ref), ());
  MOCK_METHOD(napi_value, ListVoices, (napi_env env, napi_value ttsEngine, int nweb), ());
  MOCK_METHOD(napi_value, SpeakExtraParamsString, (napi_env env, napi_value jsExtraParams), ());
  MOCK_METHOD(napi_value, SpeakExtraParamsNumber, (napi_env env, napi_value jsExtraParams), ());
  MOCK_METHOD(void, Exit, (int nweb), ());
  MOCK_METHOD(bool, IsExit, (int nweb), ());
  MOCK_METHOD(void, TransitionTo, (int nweb, TtsState status), ());
  MOCK_METHOD(TtsState, GetState, (int nweb), ());
  MOCK_METHOD(void, SetRef, (int nweb, napi_ref ref), ());
  MOCK_METHOD(napi_ref, GetRef, (int nweb), ());
  MOCK_METHOD(void, SetText, (int nweb, const std::string& text), ());
  MOCK_METHOD(std::string, GetText, (int nweb), ());
  MOCK_METHOD(void, SetNweb, (int nweb), ());
  MOCK_METHOD(bool, HasNweb, (int nweb), ());
  MOCK_METHOD(void, EraseNweb, (int nweb), ());
  MOCK_METHOD(void, Register, (int nweb, std::function<void(int, TtsState, bool)> callback), ());
  MOCK_METHOD(void, Callback, (int nweb, TtsState state, bool status), ());
  MOCK_METHOD(void, Process, (int nweb, TtsState state), ());
  MOCK_METHOD(int32_t, GetPersonFromDesc, (const std::string& desc), ());
  MOCK_METHOD(void, SetSpeakParameters, (const TtsSpeakParameters& params), ());
  MOCK_METHOD(TtsSpeakParameters, GetSpeakParameters, (), ());
  MOCK_METHOD(std::vector<VoiceInfo>, GetVoiceInfos, (), ());
  MOCK_METHOD(void, AddVoiceInfos, (VoiceInfo& voiceInfo), ());
  MOCK_METHOD(bool, IsVoiceQuerying, (), ());
  MOCK_METHOD(void, SetVoiceQuery, (bool status), ());
};

// Global pointer for mock instance
MockArktsTtsAdapter* g_mock_adapter = nullptr;

ArktsTtsAdapter& MockGetInstance() {
  if (!g_mock_adapter) {
    g_mock_adapter = new MockArktsTtsAdapter();
  }
  return *g_mock_adapter;
}

class TtsPlatformImplOHOSTest : public ::testing::Test {
 protected:
  void SetUp() override { tts_platform_ = TtsPlatformImplOHOS::GetInstance(); }

  void TearDown() override {}

  TtsPlatformImplOHOS* tts_platform_;
  int TEST_UTTERANCE_ID = 1001;
  int TEST_UTTERANCE_9999 = 9999;
  int TEST_UTTERANCE_NO = -1;
  std::string TEST_UTTERANCE = "Test speech text";
  std::string TEST_LANG = "en-US";
};

TEST_F(TtsPlatformImplOHOSTest, GetInstance_001_SingletonPattern) {
  TtsPlatformImplOHOS* instance1 = TtsPlatformImplOHOS::GetInstance();
  TtsPlatformImplOHOS* instance2 = TtsPlatformImplOHOS::GetInstance();
  EXPECT_EQ(instance1, instance2);
}

TEST_F(TtsPlatformImplOHOSTest, PlatformImplSupported_001_AlwaysTrue) {
  bool result = tts_platform_->PlatformImplSupported();
  EXPECT_TRUE(result);
}

TEST_F(TtsPlatformImplOHOSTest, PlatformImplInitialized_001_FirstCall) {
  VoiceInfo voice = {"zh-CN", 1, "news", "female", "Test", "available"};

  ArktsTtsAdapter::GetInstance().AddVoiceInfos(voice);
  bool result = tts_platform_->PlatformImplInitialized();
  EXPECT_TRUE(result);
  ArktsTtsAdapter::GetInstance().ClearVoices();
}

TEST_F(TtsPlatformImplOHOSTest, Speak_001_PausedState) {
  VoiceData voice;
  UtteranceContinuousParameters params;
  bool callback_called = false;

  tts_platform_->Pause();

  ArktsTtsAdapter::GetInstance().SetNweb(TEST_UTTERANCE_ID);
  tts_platform_->Speak(
      TEST_UTTERANCE_ID, TEST_UTTERANCE, TEST_LANG, voice, params,
      base::BindOnce([](bool status) { EXPECT_FALSE(status); }));

  tts_platform_->Resume();
}

TEST_F(TtsPlatformImplOHOSTest, StopSpeaking_001_NormalStop) {
  bool result = tts_platform_->StopSpeaking();
  EXPECT_TRUE(result);
}

TEST_F(TtsPlatformImplOHOSTest, IsSpeaking_001_AlwaysFalse) {
  bool result = tts_platform_->IsSpeaking();
  EXPECT_FALSE(result);
}

TEST_F(TtsPlatformImplOHOSTest, GetVoices_001_EmptyVoiceList) {
  std::vector<VoiceData> voices;

  tts_platform_->GetVoices(&voices);

  EXPECT_TRUE(voices.empty());
}

TEST_F(TtsPlatformImplOHOSTest, GetVoices_002_WithVoiceInfos) {
  std::vector<VoiceData> voices;
  VoiceInfo testVoice = {"zh-CN",         1,          "news", "female",
                         "Chinese Voice", "INSTALLED"};

  std::vector<VoiceInfo> mockVoiceInfos = {testVoice};

  tts_platform_->GetVoices(&voices);

  EXPECT_TRUE(voices.empty());
}

TEST_F(TtsPlatformImplOHOSTest, GetVoices_003_GAStatus) {
  std::vector<VoiceData> voices;
  VoiceInfo testVoice = {"en-US",         2,   "conversation", "male",
                         "English Voice", "GA"};

  std::vector<VoiceInfo> mockVoiceInfos = {testVoice};

  tts_platform_->GetVoices(&voices);

  EXPECT_TRUE(voices.empty());
}

TEST_F(TtsPlatformImplOHOSTest, GetVoices_004_UnknownStatus) {
  std::vector<VoiceData> voices;
  VoiceInfo testVoice = {"fr-FR",        3,        "story", "female",
                         "French Voice", "UNKNOWN"};

  std::vector<VoiceInfo> mockVoiceInfos = {testVoice};

  tts_platform_->GetVoices(&voices);

  EXPECT_TRUE(voices.empty());
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeakFinished_001_CorrectUtteranceId) {
  tts_platform_->OnSpeakFinished(TEST_UTTERANCE_ID);

  EXPECT_FALSE(tts_platform_->IsSpeaking());
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeakFinished_002_IncorrectUtteranceId) {
  tts_platform_->OnSpeakFinished(TEST_UTTERANCE_ID);

  tts_platform_->OnSpeakFinished(TEST_UTTERANCE_ID + 1);

  EXPECT_FALSE(tts_platform_->IsSpeaking());
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeechEvent_001_TTS_EVENT_BEGIN) {
  tts_platform_->OnSpeechEvent(TEST_UTTERANCE_NO, TTS_EVENT_START);
  EXPECT_EQ(tts_platform_->GetCurrentPos(), 0);
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeechEvent_002_TTS_EVENT_RESUME) {
  tts_platform_->OnSpeechEvent(TEST_UTTERANCE_NO, TTS_EVENT_RESUME);
  EXPECT_EQ(tts_platform_->GetCurrentPos(), 0);
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeechEvent_003_TTS_EVENT_END) {
  tts_platform_->OnSpeechEvent(TEST_UTTERANCE_NO, TTS_EVENT_END);
  EXPECT_EQ(tts_platform_->GetCurrentPos(), 0);
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeechEvent_004_TTS_EVENT_PAUSE) {
  tts_platform_->OnSpeechEvent(TEST_UTTERANCE_NO, TTS_EVENT_PAUSE);
  EXPECT_EQ(tts_platform_->GetCurrentPos(), 0);
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeechEvent_005_TTS_EVENT_CANCELLED) {
  tts_platform_->OnSpeechEvent(TEST_UTTERANCE_NO, TTS_EVENT_CANCELLED);
  EXPECT_EQ(tts_platform_->GetCurrentPos(), 0);
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeechEvent_006_TTS_EVENT_INTERRUPTED) {
  tts_platform_->OnSpeechEvent(TEST_UTTERANCE_NO, TTS_EVENT_INTERRUPTED);
  EXPECT_EQ(tts_platform_->GetCurrentPos(), 0);
}

TEST_F(TtsPlatformImplOHOSTest, OnSpeechEvent_007_UnknownEventType) {
  tts_platform_->OnSpeechEvent(
      TEST_UTTERANCE_NO, static_cast<TtsEventType>(100));
  EXPECT_EQ(tts_platform_->GetCurrentPos(), 0);
}

TEST_F(TtsPlatformImplOHOSTest, SendTtsEvent_001_TTS_EVENT_START) {
  tts_platform_->SendTtsEvent(TEST_UTTERANCE_NO, TTS_EVENT_START, 0, 10);
  EXPECT_EQ(tts_platform_->GetCurrentId(), TEST_UTTERANCE_NO);
}

TEST_F(TtsPlatformImplOHOSTest, StaticMembers_003_GetInstanceFromBase) {
  TtsPlatformImpl* base_instance = TtsPlatformImpl::GetInstance();
  EXPECT_NE(base_instance, nullptr);
  EXPECT_EQ(static_cast<TtsPlatformImplOHOS*>(base_instance), tts_platform_);
}
}  // namespace content
