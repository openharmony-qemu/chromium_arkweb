/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#define private public
#include "arkweb/ohos_adapter_ndk/inputmethodframework_adapter/imf_adapter_impl.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <inputmethod/inputmethod_controller_capi.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "arkweb/ohos_nweb/src/nweb_hilog.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ohos_adapter_helper.h"
#include "third_party/cJSON/cJSON.h"

using namespace OHOS::NWeb;
namespace OHOS::NWeb {
const std::string AUTO_FILL_CANCEL_PRIVATE_COMMAND = "autofill.cancel";
const std::string PREVIEW_TEXT_STYLE_KEY = "previewTextStyle";
const std::string PREVIEW_TEXT_STYLE_UNDERLINE = "underline";
const std::string AUTO_FILL_PARAMS_USERNAME = "com.autofill.params.userName";
const std::string AUTO_FILL_PARAMS_OTHERACCOUNT = "com.autofill.params.otherAccount";

class MockIMFTextListenerAdapter : public IMFTextListenerAdapter {
 public:
  void InsertText(const std::u16string& text) override {}
  void DeleteForward(int32_t length) override {}
  void DeleteBackward(int32_t length) override {}
  void SendKeyEventFromInputMethod() override {}
  void SendKeyboardStatus(const IMFAdapterKeyboardStatus& status) override {}
  void SendFunctionKey(
      std::shared_ptr<IMFAdapterFunctionKeyAdapter> key) override {}
  void SetKeyboardStatus(bool status) override {}
  void MoveCursor(const IMFAdapterDirection direction) override {}
  void HandleSetSelection(int32_t start, int32_t end) override {}
  void HandleExtendAction(int32_t action) override {}
  void HandleSelect(int32_t keyCode, int32_t skip) override {}
  int32_t GetTextIndexAtCursor() override { return 0; }
  std::u16string GetLeftTextOfCursor(int32_t number) override { return u""; }
  std::u16string GetRightTextOfCursor(int32_t number) override { return u""; }
  int32_t SetPreviewText(const std::u16string& text,
                         int32_t start,
                         int32_t end) override {
    return 0;
  }
  void FinishTextPreview() override {}
  int32_t ReceivePrivateCommand(InputMethod_PrivateCommand* cmd[],
                                size_t size) {
    return 0;
  }
  void NotifyPanelStatusInfo(const OHOS::MiscServices::PanelStatusInfo& info) {}
  void SetNeedUnderLine(bool need) override {}
  void KeyboardUpperRightCornerHide() override {}
  void AutoFillWithIMFEvent(bool isFill, bool isFillSuccess, bool isUserName, const std::string& fillContent) override {}
};

class MockIMFCursorInfoAdapter : public IMFCursorInfoAdapter {
 public:
  MockIMFCursorInfoAdapter(double x, double y, double width, double height)
      : x_(x), y_(y), width_(width), height_(height) {}

  double GetLeft() override { return x_; }
  double GetTop() override { return y_; }
  double GetWidth() override { return width_; }
  double GetHeight() override { return height_; }

 private:
  double x_ = 0.0;
  double y_ = 0.0;
  double width_ = 0.0;
  double height_ = 0.0;
};

class MockIMFAdapterFunctionKeyAdapter : public IMFAdapterFunctionKeyAdapter {
 public:
  IMFAdapterEnterKeyType GetEnterKeyType() override {
    return IMFAdapterEnterKeyType::UNSPECIFIED;
  }
  void SetEnterKeyType(IMFAdapterEnterKeyType keyType) {}
};

class MockIMFInputAttributeAdapter : public IMFInputAttributeAdapter {
 public:
  MockIMFInputAttributeAdapter(
      IMFAdapterTextInputType pattern = IMFAdapterTextInputType::TEXT,
      IMFAdapterEnterKeyType keyType = IMFAdapterEnterKeyType::UNSPECIFIED)
      : inputPattern_(pattern), enterKeyType_(keyType) {}

  int32_t GetInputPattern() override {
    return static_cast<int32_t>(inputPattern_);
  }

  int32_t GetEnterKeyType() override {
    return static_cast<int32_t>(enterKeyType_);
  }

  void SetInputPattern(IMFAdapterTextInputType pattern) {
    inputPattern_ = pattern;
  }

  void SetEnterKeyType(IMFAdapterEnterKeyType keyType) {
    enterKeyType_ = keyType;
  }

 private:
  IMFAdapterTextInputType inputPattern_;
  IMFAdapterEnterKeyType enterKeyType_;
};

class MockIMFSelectionRangeAdapter : public IMFSelectionRangeAdapter {
 public:
  MockIMFSelectionRangeAdapter(int start = 0, int end = 0)
      : start_(start), end_(end) {}

  int32_t GetStart() override { return start_; }
  int32_t GetEnd() override { return end_; }

  void SetRange(int start, int end) {
    start_ = start;
    end_ = end;
  }

 private:
  int start_;
  int end_;
};

class MockIMFTextConfigAdapter : public IMFTextConfigAdapter {
 public:
  MockIMFTextConfigAdapter(uint32_t windowId = 0,
                           double positionY = 0.0,
                           double height = 0.0)
      : windowId_(windowId), positionY_(positionY), height_(height) {
    inputAttribute_ = std::make_shared<MockIMFInputAttributeAdapter>();
    cursorInfo_ = std::make_shared<MockIMFCursorInfoAdapter>(0, 0, 10, 20);
    selectionRange_ = std::make_shared<MockIMFSelectionRangeAdapter>(0, 5);
  }

  std::shared_ptr<IMFInputAttributeAdapter> GetInputAttribute() override {
    return inputAttribute_;
  }

  std::shared_ptr<IMFCursorInfoAdapter> GetCursorInfo() override {
    return cursorInfo_;
  }

  std::shared_ptr<IMFSelectionRangeAdapter> GetSelectionRange() override {
    return selectionRange_;
  }

  uint32_t GetWindowId() override { return windowId_; }

  double GetPositionY() override { return positionY_; }

  double GetHeight() override { return height_; }

  void SetInputPattern(IMFAdapterTextInputType pattern) {
    if (inputAttribute_) {
      std::static_pointer_cast<MockIMFInputAttributeAdapter>(inputAttribute_)
          ->SetInputPattern(pattern);
    }
  }

  void SetEnterKeyType(IMFAdapterEnterKeyType keyType) {
    if (inputAttribute_) {
      std::static_pointer_cast<MockIMFInputAttributeAdapter>(inputAttribute_)
          ->SetEnterKeyType(keyType);
    }
  }

  void SetInputAttributeNull() { inputAttribute_ = nullptr; }
  void SetCursorInfoNull() { cursorInfo_ = nullptr; }

 private:
  std::shared_ptr<IMFInputAttributeAdapter> inputAttribute_;
  std::shared_ptr<IMFCursorInfoAdapter> cursorInfo_;
  std::shared_ptr<IMFSelectionRangeAdapter> selectionRange_;
  uint32_t windowId_ = 0;
  double positionY_ = 0.0;
  double height_ = 0.0;
};

std::u16string GenerateRandomU16String(FuzzedDataProvider* fdp,
                                       size_t maxLength = 256) {
  size_t len = fdp->ConsumeIntegralInRange<size_t>(0, maxLength);
  std::u16string result;
  result.reserve(len);

  for (size_t i = 0; i < len; i++) {
    char16_t codePoint = fdp->ConsumeIntegralInRange<char16_t>(0x0020, 0xD7FF);
    result += codePoint;
  }
  return result;
}

InputMethod_TextInputType AdapterTextInputTypeToTextInputTypeEx(
    int32_t inputType) {
  const static std::unordered_map<IMFAdapterTextInputType,
                                  InputMethod_TextInputType>
      INPUT_TYPE_MAP = {
          /* The text input type is NONE. */
          {IMFAdapterTextInputType::NONE, IME_TEXT_INPUT_TYPE_NONE},
          /* The text input type is TEXT. */
          {IMFAdapterTextInputType::TEXT, IME_TEXT_INPUT_TYPE_TEXT},
          /* The text input type is MULTILINE. */
          {IMFAdapterTextInputType::MULTILINE, IME_TEXT_INPUT_TYPE_MULTILINE},
          /* The text input type is NUMBER. */
          {IMFAdapterTextInputType::NUMBER, IME_TEXT_INPUT_TYPE_NUMBER},
          /* The text input type is PHONE. */
          {IMFAdapterTextInputType::PHONE, IME_TEXT_INPUT_TYPE_PHONE},
          /* The text input type is DATETIME. */
          {IMFAdapterTextInputType::DATETIME, IME_TEXT_INPUT_TYPE_DATETIME},
          /* The text input type is EMAIL ADDRESS. */
          {IMFAdapterTextInputType::EMAIL_ADDRESS,
           IME_TEXT_INPUT_TYPE_EMAIL_ADDRESS},
          /* The text input type is URL. */
          {IMFAdapterTextInputType::URL, IME_TEXT_INPUT_TYPE_URL},
          /* The text input type is VISIBLE PASSWORD. */
          {IMFAdapterTextInputType::VISIBLE_PASSWORD,
           IME_TEXT_INPUT_TYPE_VISIBLE_PASSWORD},
          /* The text input type is NUMBER PASSWORD. */
          {IMFAdapterTextInputType::NUMBER_PASSWORD,
           IME_TEXT_INPUT_TYPE_NUMBER_PASSWORD},
          /* The text input type is SCREEN LOCK PASSWORD. */
          {IMFAdapterTextInputType::SCREEN_LOCK_PASSWORD,
           IME_TEXT_INPUT_TYPE_SCREEN_LOCK_PASSWORD},
          /* The text input type is USER NAME. */
          {IMFAdapterTextInputType::USER_NAME, IME_TEXT_INPUT_TYPE_USER_NAME},
          /* The text input type is NEW PASSWORD. */
          {IMFAdapterTextInputType::NEW_PASSWORD,
           IME_TEXT_INPUT_TYPE_NEW_PASSWORD},
          /* The text input type is NUMBER DECIMAL. */
          {IMFAdapterTextInputType::NUMBER_DECIMAL,
           IME_TEXT_INPUT_TYPE_NUMBER_DECIMAL},
      };
  auto checkIter = INPUT_TYPE_MAP.find((IMFAdapterTextInputType)inputType);
  if (checkIter != INPUT_TYPE_MAP.end()) {
    return checkIter->second;
  }
  return IME_TEXT_INPUT_TYPE_NONE;
}

InputMethod_EnterKeyType AdapterEnterKeyTypeToOhEnterKeyTypeEx(
    int32_t enterKeyType) {
  const static std::unordered_map<IMFAdapterEnterKeyType,
                                  InputMethod_EnterKeyType>
      ENTER_KEY_TYPE_MAP = {
          /* The enter key type is UNSPECIFIED. */
          {IMFAdapterEnterKeyType::UNSPECIFIED, IME_ENTER_KEY_UNSPECIFIED},
          /* The enter key type is NONE. */
          {IMFAdapterEnterKeyType::NONE, IME_ENTER_KEY_NONE},
          /* The enter key type is GO. */
          {IMFAdapterEnterKeyType::GO, IME_ENTER_KEY_GO},
          /* The enter key type is SEARCH. */
          {IMFAdapterEnterKeyType::SEARCH, IME_ENTER_KEY_SEARCH},
          /* The enter key type is SEND. */
          {IMFAdapterEnterKeyType::SEND, IME_ENTER_KEY_SEND},
          /* The enter key type is NEXT. */
          {IMFAdapterEnterKeyType::NEXT, IME_ENTER_KEY_NEXT},
          /* The enter key type is DONE. */
          {IMFAdapterEnterKeyType::DONE, IME_ENTER_KEY_DONE},
          /* The enter key type is PREVIOUS. */
          {IMFAdapterEnterKeyType::PREVIOUS, IME_ENTER_KEY_PREVIOUS},
          /* The enter key type is NEWLINE. */
          {IMFAdapterEnterKeyType::NEW_LINE, IME_ENTER_KEY_NEWLINE},
      };
  auto checkIter =
      ENTER_KEY_TYPE_MAP.find((IMFAdapterEnterKeyType)enterKeyType);
  if (checkIter != ENTER_KEY_TYPE_MAP.end()) {
    return checkIter->second;
  }
  return IME_ENTER_KEY_UNSPECIFIED;
}

IMFAdapterKeyboardStatus ohKeyboardStatusToAdapterKeyboardStatusEx(
    InputMethod_KeyboardStatus keyboardStatus) {
  const static std::unordered_map<InputMethod_KeyboardStatus,
                                  IMFAdapterKeyboardStatus>
      KEY_BOARD_STATUS_MAP = {
          {IME_KEYBOARD_STATUS_NONE, IMFAdapterKeyboardStatus::NONE},
          {IME_KEYBOARD_STATUS_HIDE, IMFAdapterKeyboardStatus::HIDE},
          {IME_KEYBOARD_STATUS_SHOW, IMFAdapterKeyboardStatus::SHOW},
      };
  auto checkIter = KEY_BOARD_STATUS_MAP.find(keyboardStatus);
  if (checkIter != KEY_BOARD_STATUS_MAP.end()) {
    return checkIter->second;
  }
  return IMFAdapterKeyboardStatus::NONE;
}

std::shared_ptr<std::string> PrivateCommandGetStrValueEx(InputMethod_PrivateCommand *privateCommand)
{
    const char *value = nullptr;
    size_t length = 0;

    InputMethod_ErrorCode ret = OH_PrivateCommand_GetStrValue(privateCommand, &value, &length);
    if (ret != IME_ERR_OK) {
        return nullptr;
    }
    return std::make_shared<std::string>(value);
}

void FuzzIMFAdapterImpl(FuzzedDataProvider* fdp) {
  auto listener = std::make_shared<MockIMFTextListenerAdapter>();
  uint32_t windowId = fdp->ConsumeIntegral<uint32_t>();
  double positionY = fdp->ConsumeFloatingPoint<double>();
  double height = fdp->ConsumeFloatingPoint<double>();
  IMFAdapterImpl adapter;

  for (int32_t inputType = static_cast<int32_t>(IMFAdapterTextInputType::NONE);
       inputType <=
       static_cast<int32_t>(IMFAdapterTextInputType::NUMBER_DECIMAL);
       ++inputType) {
    for (int32_t enterKey =
             static_cast<int32_t>(IMFAdapterEnterKeyType::UNSPECIFIED);
         enterKey <= static_cast<int32_t>(IMFAdapterEnterKeyType::NEW_LINE);
         ++enterKey) {
      auto config = std::make_shared<MockIMFTextConfigAdapter>(
          windowId, positionY, height);

      config->SetInputPattern(static_cast<IMFAdapterTextInputType>(inputType));
      config->SetEnterKeyType(static_cast<IMFAdapterEnterKeyType>(enterKey));

      std::u16string selectionText = u"Hello, World!";
      adapter.OnSelectionChange(selectionText, 2, selectionText.length());
      adapter.HideTextInput();
      adapter.Close();
      adapter.ShowCurrentInput(static_cast<IMFAdapterTextInputType>(inputType));
    }
  }

  {
    const int32_t minType = static_cast<int32_t>(IMFAdapterTextInputType::NONE);
    const int32_t maxType =
        static_cast<int32_t>(IMFAdapterTextInputType::NUMBER_DECIMAL);

    std::vector<int32_t> testTypes;
    for (int32_t i = minType; i <= maxType; ++i) {
      testTypes.push_back(i);
    }

    for (int32_t type : testTypes) {
      InputMethod_TextInputType result =
          AdapterTextInputTypeToTextInputTypeEx(type);
      (void)result;
    }
  }

  {
    const int32_t minType =
        static_cast<int32_t>(IMFAdapterEnterKeyType::UNSPECIFIED);
    const int32_t maxType =
        static_cast<int32_t>(IMFAdapterEnterKeyType::NEW_LINE);

    std::vector<int32_t> testTypes;
    for (int32_t i = minType; i <= maxType; ++i) {
      testTypes.push_back(i);
    }

    for (int32_t type : testTypes) {
      InputMethod_EnterKeyType result =
          AdapterEnterKeyTypeToOhEnterKeyTypeEx(type);
      (void)result;
    }
  }

  {
    const std::vector<InputMethod_KeyboardStatus> keyboardStatuses = {
        IME_KEYBOARD_STATUS_NONE, IME_KEYBOARD_STATUS_HIDE,
        IME_KEYBOARD_STATUS_SHOW, static_cast<InputMethod_KeyboardStatus>(-1),
        static_cast<InputMethod_KeyboardStatus>(3)};
    for (InputMethod_KeyboardStatus status : keyboardStatuses) {
      IMFAdapterKeyboardStatus result =
          ohKeyboardStatusToAdapterKeyboardStatusEx(status);
      (void)result;
    }
  }

  IMFAdapterTextInputType inputType =
      static_cast<IMFAdapterTextInputType>(fdp->ConsumeIntegralInRange<int32_t>(
          0, static_cast<int32_t>(IMFAdapterTextInputType::NUMBER_DECIMAL)));
  InputMethod_TextEditorProxy* proxy = 
      IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
  static InputMethod_TextConfig *textConfig_;
  InputMethod_TextConfig* textConfig = OH_TextConfig_Create();
  if (textConfig) {
    IMFTextEditorProxyImpl::textConfig_ = textConfig;
  }
  auto config = std::make_shared<MockIMFTextConfigAdapter>(
          windowId, positionY, height);
  InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(config);
  (void)ret;
  InputMethod_TextConfig* funcDestConfig = OH_TextConfig_Create();
  if (funcDestConfig) {
    IMFTextEditorProxyImpl::GetTextConfigFunc(proxy, funcDestConfig);
    OH_TextConfig_Destroy(funcDestConfig);
  }
  adapter.AttachParamsCheck(listener, true, config, true);
  adapter.AttachParamsCheck(listener, true, config, false);
  adapter.AttachParamsCheck(listener, false, config, true);
  adapter.AttachParamsCheck(listener, false, config, false);
  adapter.ShowCurrentInput(inputType);
  adapter.HideTextInput();
  std::shared_ptr<IMFCursorInfoAdapter> cursorInfo =
      std::make_shared<MockIMFCursorInfoAdapter>(10.2, 33.2, 44.8, 55.2);
  adapter.OnCursorUpdate(cursorInfo);
  std::u16string selectionText = GenerateRandomU16String(fdp, 100);
  int start = fdp->ConsumeIntegralInRange<int>(0, selectionText.length());
  int end = fdp->ConsumeIntegralInRange<int>(start, selectionText.length());
  adapter.OnSelectionChange(selectionText, start, end);
  std::string cmdKey = fdp->ConsumeRandomLengthString(32);
  std::string cmdValue = fdp->ConsumeRandomLengthString(256);
  std::string validKey = AUTO_FILL_CANCEL_PRIVATE_COMMAND;
  std::string validValue = R"({"fillContent": "test", "action": "cancel"})";
  adapter.SendPrivateCommand(cmdKey, cmdValue);
  adapter.SendPrivateCommand(validKey, validValue);
  InputMethod_PrivateCommand *cmd = OH_PrivateCommand_Create(const_cast<char*>("init_key"), strlen("init_key"));
  std::vector<InputMethod_PrivateCommand*> privateCommands;
  privateCommands.push_back(cmd);
  std::string JsonValue = R"({"userName": "test_user", "hasAccount": "true"})";
  adapter.ParseFillContentJsonValue(JsonValue, privateCommands);
  for (size_t i = 0; i < privateCommands.size(); ++i) {
    std::shared_ptr<std::string> style = PrivateCommandGetStrValueEx(privateCommands[i]);
    (void)style;
  }
  OHOS::MiscServices::PanelStatusInfo panelInfo{};
  panelInfo.trigger = fdp->ConsumeBool()
                          ? OHOS::MiscServices::Trigger::IME_APP
                          : static_cast<OHOS::MiscServices::Trigger>(
                                fdp->ConsumeIntegralInRange<int>(0, 5));
  listener->NotifyPanelStatusInfo(panelInfo);
  adapter.Close();
}

void FixedIMFAdapterImpl() {
  struct TestCase {
    uint32_t windowId;
    double positionY;
    double height;
  };
  const std::vector<TestCase> testCases = {
      {12345, 100.0, 200.0},
      {0, 50.0, 150.0},
      {UINT32_MAX, 200.0, 300.0},
      {4294967295, 0.0, 1000.0},
      {1, -100.0, 500.0},
      {1000, 1000.0, 100.0},
  };
  for (const auto& test : testCases) {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    for (int32_t inputType = -1; inputType <= 13; ++inputType) {
      for (int32_t enterKey = 0; enterKey <= 8; ++enterKey) {
        auto config = std::make_shared<MockIMFTextConfigAdapter>(
            test.windowId, test.positionY, test.height);

        IMFAdapterTextInputType inputTypeEnum;
        if (inputType == static_cast<int32_t>(IMFAdapterTextInputType::NONE)) {
          inputTypeEnum = IMFAdapterTextInputType::NONE;
        } else if (inputType >=
                       static_cast<int32_t>(IMFAdapterTextInputType::TEXT) &&
                   inputType <= static_cast<int32_t>(
                                    IMFAdapterTextInputType::NUMBER_DECIMAL)) {
          inputTypeEnum = static_cast<IMFAdapterTextInputType>(inputType);
        } else {
          inputTypeEnum = IMFAdapterTextInputType::NUMBER_DECIMAL;
        }
        config->SetInputPattern(inputTypeEnum);

        IMFAdapterEnterKeyType enterKeyEnum;
        if (enterKey >=
                static_cast<int32_t>(IMFAdapterEnterKeyType::UNSPECIFIED) &&
            enterKey <=
                static_cast<int32_t>(IMFAdapterEnterKeyType::NEW_LINE)) {
          enterKeyEnum = static_cast<IMFAdapterEnterKeyType>(enterKey);
        } else {
          enterKeyEnum = IMFAdapterEnterKeyType::NEW_LINE;
        }
        config->SetEnterKeyType(enterKeyEnum);
      }
    }
    {
      const int32_t minType =
          static_cast<int32_t>(IMFAdapterTextInputType::NONE);
      const int32_t maxType =
          static_cast<int32_t>(IMFAdapterTextInputType::NUMBER_DECIMAL);
      std::vector<int32_t> testTypes;
      for (int32_t i = minType; i <= maxType; ++i) {
        testTypes.push_back(i);
      }
      for (int32_t type : testTypes) {
        InputMethod_TextInputType result =
            AdapterTextInputTypeToTextInputTypeEx(type);
        (void)result;
      }
    }
    {
      const int32_t minType =
          static_cast<int32_t>(IMFAdapterEnterKeyType::UNSPECIFIED);
      const int32_t maxType =
          static_cast<int32_t>(IMFAdapterEnterKeyType::NEW_LINE);
      std::vector<int32_t> testTypes;
      for (int32_t i = minType; i <= maxType; ++i) {
        testTypes.push_back(i);
      }
      for (int32_t type : testTypes) {
        InputMethod_EnterKeyType result =
            AdapterEnterKeyTypeToOhEnterKeyTypeEx(type);
        (void)result;
      }
    }
    {
      const std::vector<InputMethod_KeyboardStatus> keyboardStatuses = {
          IME_KEYBOARD_STATUS_NONE, IME_KEYBOARD_STATUS_HIDE,
          IME_KEYBOARD_STATUS_SHOW, static_cast<InputMethod_KeyboardStatus>(0),
          static_cast<InputMethod_KeyboardStatus>(2)};
      for (InputMethod_KeyboardStatus status : keyboardStatuses) {
        IMFAdapterKeyboardStatus result =
            ohKeyboardStatusToAdapterKeyboardStatusEx(status);
        (void)result;
      }
    }
    {
      std::vector<IMFAdapterTextInputType> inputTypes = {
          IMFAdapterTextInputType::TEXT, IMFAdapterTextInputType::NUMBER,
          IMFAdapterTextInputType::EMAIL_ADDRESS,
          IMFAdapterTextInputType::VISIBLE_PASSWORD};
      for (auto type : inputTypes) {
        adapter.ShowCurrentInput(type);
        adapter.HideTextInput();
      }
      std::vector<std::tuple<double, double, double>> cursorParams = {
          {50.0, 100.0, 20.0},
          {0.0, 0.0, 10.0},
          {1000.0, 1000.0, 50.0},
          {-50.0, 50.0, 30.0}};
      for (const auto& [x, y, width] : cursorParams) {
        std::shared_ptr<IMFCursorInfoAdapter> cursorInfo =
            std::make_shared<MockIMFCursorInfoAdapter>(x, y, width,
                                                       test.height);
        adapter.OnCursorUpdate(cursorInfo);
      }
      std::u16string selectionText = u"Hello, World!";
      adapter.OnSelectionChange(selectionText, 0, selectionText.length());
      adapter.OnSelectionChange(selectionText, selectionText.length(),
                                selectionText.length());
      adapter.OnSelectionChange(selectionText, 0, 0);
      std::vector<std::pair<std::string, std::string>> commands = {
          {"testCommand", "testValue"},
          {"empty", ""},
          {"longCommand", std::string(256, 'a')},
          {"unicode", "testCommand"}};
      for (const auto& [key, value] : commands) {
        adapter.SendPrivateCommand(key, value);
      }
      OHOS::MiscServices::PanelStatusInfo panelInfo{};
      panelInfo.trigger = static_cast<OHOS::MiscServices::Trigger>(2);
      panelInfo.visible = true;
      panelInfo.panelInfo.panelFlag = static_cast<OHOS::MiscServices::PanelFlag>(500);
      panelInfo.panelInfo.panelType = static_cast<OHOS::MiscServices::PanelType>(2);
      listener->NotifyPanelStatusInfo(panelInfo);
    }
    adapter.Close();
  }
}

void FuzzIMFTextEditorProxyImpl(FuzzedDataProvider* fdp) {
  auto listener = std::make_shared<MockIMFTextListenerAdapter>();
  InputMethod_TextEditorProxy* proxy =
      IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
  if (!proxy) {
    return;
  }

  InputMethod_ErrorCode ret =
      IMFTextEditorProxyImpl::TextEditorProxy_SetTextFunc(proxy);
  if (ret != IME_ERR_OK) {
    IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    return;
  }

  auto mockConfig = std::make_shared<MockIMFTextConfigAdapter>(
      fdp->ConsumeIntegral<uint32_t>(),
      fdp->ConsumeFloatingPoint<double>(),
      fdp->ConsumeFloatingPoint<double>()
  );

  std::u16string randomText = GenerateRandomU16String(fdp, 100);
  IMFTextEditorProxyImpl::InsertTextFunc(proxy, randomText.c_str(),
                                         randomText.length());
  int32_t deleteLength = fdp->ConsumeIntegralInRange<int32_t>(0, 50);
  IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, deleteLength);
  IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, deleteLength);
  listener->SendKeyEventFromInputMethod();
  listener->SetKeyboardStatus(true);
  int32_t keyCode = fdp->ConsumeIntegralInRange<int32_t>(100, 255);
  int32_t cursorMoveSkip = fdp->ConsumeIntegralInRange<int32_t>(50, 100);
  listener->HandleSelect(keyCode, cursorMoveSkip);
  InputMethod_Direction direction = static_cast<InputMethod_Direction>(
      fdp->ConsumeIntegralInRange<int32_t>(0, 4));
  IMFTextEditorProxyImpl::MoveCursorFunc(proxy, direction);
  int32_t start = fdp->ConsumeIntegralInRange<int32_t>(0, 50);
  int32_t end = fdp->ConsumeIntegralInRange<int32_t>(start, 100);
  IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, start, end);
  InputMethod_ExtendAction action = static_cast<InputMethod_ExtendAction>(
      fdp->ConsumeIntegralInRange<int32_t>(0, 5));
  IMFTextEditorProxyImpl::HandleExtendActionFunc(proxy, action);
  int32_t textNumber = fdp->ConsumeIntegralInRange<int32_t>(0, 99);
  char16_t leftText[100] = {0};
  size_t leftLength = fdp->ConsumeIntegralInRange<size_t>(0, 99);
  leftLength = std::min(leftLength, static_cast<size_t>(99));
  IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, textNumber, leftText,
                                                  &leftLength);
  char16_t rightText[100] = {0};
  size_t rightLength = fdp->ConsumeIntegralInRange<size_t>(0, 99);
  rightLength = std::min(rightLength, static_cast<size_t>(99));
  IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, textNumber, rightText,
                                                   &rightLength);
  IMFTextEditorProxyImpl::GetTextIndexAtCursorFunc(proxy);
  InputMethod_PrivateCommand* privateCmd[5] = {nullptr};
  size_t cmdSize = 50;
  IMFTextEditorProxyImpl::ReceivePrivateCommandFunc(proxy, privateCmd, cmdSize);
  std::u16string previewText = GenerateRandomU16String(fdp, 150);
  int32_t previewStart = fdp->ConsumeIntegralInRange<int32_t>(-10, 200);
  int32_t previewEnd = fdp->ConsumeIntegralInRange<int32_t>(previewStart, 250);
  IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, previewText.c_str(),
                                             previewText.length(), previewStart,
                                             previewEnd);
  IMFTextEditorProxyImpl::FinishTextPreviewFunc(proxy);

  {
    for (int32_t i = 0; i <= 2; ++i) {
      InputMethod_KeyboardStatus kbStatus =
          static_cast<InputMethod_KeyboardStatus>(i);
      IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy, kbStatus);
    }
  }

  {
    for (int32_t i = 0; i <= 8; ++i) {
      InputMethod_EnterKeyType enterKey =
          static_cast<InputMethod_EnterKeyType>(i);
      IMFTextEditorProxyImpl::SendEnterKeyFunc(proxy, enterKey);
    }
  }

  IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
}

void FixedIMFTextEditorProxyImpl() {
  auto listener = std::make_shared<MockIMFTextListenerAdapter>();
  InputMethod_TextEditorProxy* proxy =
      IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
  if (!proxy) {
    return;
  }

  InputMethod_ErrorCode ret =
      IMFTextEditorProxyImpl::TextEditorProxy_SetTextFunc(proxy);
  if (ret != IME_ERR_OK) {
    IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    return;
  }

  std::u16string testText = u"Hello, Fixed World!";
  IMFTextEditorProxyImpl::InsertTextFunc(proxy, testText.c_str(),
                                         testText.length());
  IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, 5);
  IMFTextEditorProxyImpl::MoveCursorFunc(
      proxy, InputMethod_Direction::IME_DIRECTION_RIGHT);
  IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, 0, 10);
  IMFTextEditorProxyImpl::HandleExtendActionFunc(
      proxy, InputMethod_ExtendAction::IME_EXTEND_ACTION_PASTE);
  char16_t leftText[100] = {0};
  size_t leftLength = 0;
  IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 10, leftText,
                                                  &leftLength);
  char16_t rightText[100] = {0};
  size_t rightLength = 0;
  IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 10, rightText,
                                                   &rightLength);
  IMFTextEditorProxyImpl::GetTextIndexAtCursorFunc(proxy);
  std::u16string previewText = u"Preview Text";
  IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, previewText.c_str(),
                                             previewText.length(), 0, 5);
  IMFTextEditorProxyImpl::FinishTextPreviewFunc(proxy);
  IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
}

}  // namespace OHOS::NWeb

void FuzzIMFTextListenerAdapterImpl(FuzzedDataProvider* fdp) {
  auto listener = std::make_shared<MockIMFTextListenerAdapter>();
  IMFTextListenerAdapterImpl impl(listener);

  impl.InsertText(u"Test text");
  impl.InsertText(u"");
  impl.DeleteForward(10);
  impl.DeleteBackward(5);
  impl.SendKeyEventFromInputMethod();
  impl.SendKeyboardStatus(IMFAdapterKeyboardStatus::SHOW);
  impl.SendKeyboardStatus(IMFAdapterKeyboardStatus::HIDE);
  impl.SendKeyboardStatus(IMFAdapterKeyboardStatus::NONE);
  impl.SetKeyboardStatus(true);
  impl.SetKeyboardStatus(false);

  auto funcKey = std::make_shared<IMFAdapterFunctionKeyAdapterImpl>();
  funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::GO);
  impl.SendFunctionKey(funcKey);

  impl.MoveCursor(IMFAdapterDirection::UP);
  impl.MoveCursor(IMFAdapterDirection::DOWN);
  impl.MoveCursor(IMFAdapterDirection::LEFT);
  impl.MoveCursor(IMFAdapterDirection::RIGHT);
  impl.MoveCursor(IMFAdapterDirection::NONE);

  impl.HandleSetSelection(0, 10);
  impl.HandleSetSelection(-5, 20);

  impl.HandleExtendAction(0);
  impl.HandleExtendAction(1);
  impl.HandleExtendAction(2);
  impl.HandleExtendAction(3);
  impl.HandleExtendAction(4);
  impl.HandleExtendAction(5);
  impl.HandleExtendAction(100);

  impl.HandleSelect(10, 5);
  impl.HandleSelect(-1, 0);

  impl.GetTextIndexAtCursor();
  impl.GetLeftTextOfCursor(10);
  impl.GetLeftTextOfCursor(0);
  impl.GetLeftTextOfCursor(-5);
  impl.GetRightTextOfCursor(10);
  impl.GetRightTextOfCursor(0);
  impl.GetRightTextOfCursor(-5);

  impl.SetPreviewText(u"Preview", 0, 7);
  impl.SetPreviewText(u"", 0, 0);
  impl.SetPreviewText(u"Test", -1, 5);

  impl.FinishTextPreview();

  {
    std::vector<InputMethod_PrivateCommand*> cmds;
    InputMethod_PrivateCommand* cmd = OH_PrivateCommand_Create(const_cast<char*>("test"), 4);
    cmds.push_back(cmd);
    impl.ReceivePrivateCommand(cmds.data(), cmds.size());
    OH_PrivateCommand_Destroy(cmd);
  }

  OHOS::MiscServices::PanelStatusInfo info;
  info.trigger = OHOS::MiscServices::Trigger::IME_APP;
  info.visible = true;
  impl.NotifyPanelStatusInfo(info);
}

void FuzzIMFAdapterAttachBranches(FuzzedDataProvider* fdp) {
  IMFAdapterImpl adapter;

  auto validConfig = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
  auto validListener = std::make_shared<MockIMFTextListenerAdapter>();

  adapter.Attach(validListener, true, validConfig, true);
  adapter.Attach(validListener, false, validConfig, false);
  adapter.Attach(validListener, true, validConfig, false);
  adapter.Attach(validListener, false, validConfig, true);

  adapter.AttachWithRequestKeyboardReason(validListener, true, validConfig, true, 0);
  adapter.AttachWithRequestKeyboardReason(validListener, false, validConfig, false, 1);
  adapter.AttachWithRequestKeyboardReason(validListener, true, validConfig, false, 2);
  adapter.AttachWithRequestKeyboardReason(validListener, false, validConfig, true, -1);

  auto nullListener = std::shared_ptr<MockIMFTextListenerAdapter>(nullptr);
  adapter.Attach(nullListener, true, validConfig, true);

  auto nullConfig = std::shared_ptr<MockIMFTextConfigAdapter>(nullptr);
  adapter.Attach(validListener, true, nullConfig, true);

  adapter.Close();
}

void FuzzDestroyPrivateCommand(FuzzedDataProvider* fdp) {
  std::vector<InputMethod_PrivateCommand*> commands;

  for (int i = 0; i < 5; i++) {
    std::string key = "key" + std::to_string(i);
    InputMethod_PrivateCommand* cmd = OH_PrivateCommand_Create(
        const_cast<char*>(key.c_str()), key.length());
    if (cmd) {
      commands.push_back(cmd);
    }
  }

  for (auto it = commands.begin(); it != commands.end(); it++) {
    OH_PrivateCommand_Destroy(*it);
  }
}

void DestroyPrivateCommand(std::vector<InputMethod_PrivateCommand*>& privateCommands)
{
  for (auto it = privateCommands.begin(); it != privateCommands.end(); it++) {
    OH_PrivateCommand_Destroy(*it);
  }
}

void FuzzErrorHandlingBranches(FuzzedDataProvider* fdp) {
  auto listener = std::make_shared<MockIMFTextListenerAdapter>();
  IMFAdapterImpl adapter;

  for (int32_t inputType = 0; inputType <= 13; inputType++) {
    adapter.ShowCurrentInput(static_cast<IMFAdapterTextInputType>(inputType));
  }

  adapter.HideTextInput();
  adapter.Close();

  auto cursorInfo = std::make_shared<MockIMFCursorInfoAdapter>(0, 0, 0, 0);
  adapter.OnCursorUpdate(cursorInfo);

  cursorInfo = std::make_shared<MockIMFCursorInfoAdapter>(-100, -100, -10, -10);
  adapter.OnCursorUpdate(cursorInfo);

  cursorInfo = std::make_shared<MockIMFCursorInfoAdapter>(10000, 10000, 5000, 5000);
  adapter.OnCursorUpdate(cursorInfo);

  adapter.OnSelectionChange(u"", 0, 0);
  adapter.OnSelectionChange(u"test", 0, 4);
  adapter.OnSelectionChange(u"test", -1, 10);
  adapter.OnSelectionChange(u"test", 2, 2);

  adapter.SendPrivateCommand("", "");
  adapter.SendPrivateCommand("key", "");
  adapter.SendPrivateCommand("", "value");
  adapter.SendPrivateCommand("testkey", "testvalue");

  std::string jsonValues[] = {
    R"({"key": "value"})",
    R"({})",
    R"({"fillContent": ""})",
    R"({"fillContent": "test", "invalid_field": "value"})",
    R"([{"fillContent": "test"}])",
    R"("invalid json")",
    ""
  };

  std::vector<InputMethod_PrivateCommand*> cmds;
  for (const auto& json : jsonValues) {
    adapter.ParseFillContentJsonValue(json, cmds);
  }
}

void FuzzMoreEditorProxyBranches(FuzzedDataProvider* fdp) {
  auto listener = std::make_shared<MockIMFTextListenerAdapter>();
  InputMethod_TextEditorProxy* proxy = fdp->ConsumeBool() ?
      IMFTextEditorProxyImpl::TextEditorProxyCreate(listener) : nullptr;

  IMFTextEditorProxyImpl::TextEditorProxy_SetTextFunc(proxy);

  IMFTextEditorProxyImpl::InsertTextFunc(proxy, u"", 0);
  IMFTextEditorProxyImpl::InsertTextFunc(proxy, u"Test", 4);
  IMFTextEditorProxyImpl::InsertTextFunc(proxy, u"", 0);

  IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, 0);
  IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, 100);
  IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, -1);

  IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, 0);
  IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, 100);
  IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, -1);

  for (int i = 0; i <= 4; i++) {
    IMFTextEditorProxyImpl::MoveCursorFunc(proxy, static_cast<InputMethod_Direction>(i));
  }

  IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, -1, 10);
  IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, 0, -1);
  IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, 100, 50);

  for (int i = 0; i <= 5; i++) {
    IMFTextEditorProxyImpl::HandleExtendActionFunc(proxy, static_cast<InputMethod_ExtendAction>(i));
  }

  char16_t buffer[10] = {0};
  size_t len = 0;
  IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 0, buffer, &len);
  IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 100, buffer, &len);

  IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 0, buffer, &len);
  IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 100, buffer, &len);

  IMFTextEditorProxyImpl::GetTextIndexAtCursorFunc(proxy);

  InputMethod_PrivateCommand* nullCmds[1] = {nullptr};
  IMFTextEditorProxyImpl::ReceivePrivateCommandFunc(proxy, nullCmds, 0);

  IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, u"", 0, 0, 0);
  IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, u"Preview", 7, -1, 5);
  IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, u"Preview", 7, 10, 20);

  for (int i = 0; i <= 2; i++) {
    IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy,
        static_cast<InputMethod_KeyboardStatus>(i));
  }

  for (int i = 0; i <= 8; i++) {
    IMFTextEditorProxyImpl::SendEnterKeyFunc(proxy,
        static_cast<InputMethod_EnterKeyType>(i));
  }

  IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
}

void FuzzEdgeCases(FuzzedDataProvider* fdp) {
  auto listener = std::make_shared<MockIMFTextListenerAdapter>();
  InputMethod_TextEditorProxy* proxy =
      IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);

  if (proxy) {
    char16_t text[1000] = {0};
    for (size_t i = 0; i < 999 && i < fdp->ConsumeIntegralInRange<size_t>(0, 999); i++) {
      text[i] = static_cast<char16_t>(fdp->ConsumeIntegralInRange<uint16_t>(32, 126));
    }
    size_t len = fdp->ConsumeIntegralInRange<size_t>(0, 1000);
    IMFTextEditorProxyImpl::InsertTextFunc(proxy, text, len);

    char16_t leftText[200] = {0};
    size_t leftLen = fdp->ConsumeIntegralInRange<size_t>(0, 199);
    IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 50, leftText, &leftLen);

    char16_t rightText[200] = {0};
    size_t rightLen = fdp->ConsumeIntegralInRange<size_t>(0, 199);
    IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 50, rightText, &rightLen);

    IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
  }

  IMFAdapterImpl adapter;
  auto config = std::make_shared<MockIMFTextConfigAdapter>(
      fdp->ConsumeIntegral<uint32_t>(),
      fdp->ConsumeFloatingPoint<double>(),
      fdp->ConsumeFloatingPoint<double>()
  );

  adapter.AttachParamsCheck(listener, fdp->ConsumeBool(), config, fdp->ConsumeBool());

  std::string key = fdp->ConsumeRandomLengthString(64);
  std::string value = fdp->ConsumeRandomLengthString(256);
  adapter.SendPrivateCommand(key, value);

  std::string json = fdp->ConsumeRandomLengthString(512);
  std::vector<InputMethod_PrivateCommand*> cmds;
  adapter.ParseFillContentJsonValue(json, cmds);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size == 0) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);
  FuzzIMFAdapterImpl(&fdp);
  FuzzIMFTextEditorProxyImpl(&fdp);
  FuzzIMFTextListenerAdapterImpl(&fdp);
  FuzzIMFAdapterAttachBranches(&fdp);
  FuzzDestroyPrivateCommand(&fdp);
  FuzzErrorHandlingBranches(&fdp);
  FuzzMoreEditorProxyBranches(&fdp);
  FuzzEdgeCases(&fdp);
  FixedIMFAdapterImpl();
  FixedIMFTextEditorProxyImpl();

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    adapter.Attach(listener, true, config, true);
    adapter.Attach(listener, false, config, false);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(0, 0.0, 100.0);
    adapter.AttachWithRequestKeyboardReason(listener, true, config, true, 0);
    adapter.AttachWithRequestKeyboardReason(listener, false, config, false, 1);
    adapter.AttachWithRequestKeyboardReason(listener, true, config, false, 2);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>();
    config->SetInputPattern(IMFAdapterTextInputType::TEXT);
    config->SetEnterKeyType(IMFAdapterEnterKeyType::GO);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::TEXT);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::NUMBER);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::PHONE);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::EMAIL_ADDRESS);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::URL);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::VISIBLE_PASSWORD);
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    auto cursorInfo = std::make_shared<MockIMFCursorInfoAdapter>(0.0, 0.0, 0.0, 0.0);
    adapter.OnCursorUpdate(cursorInfo);
    cursorInfo = std::make_shared<MockIMFCursorInfoAdapter>(-1.0, -1.0, -1.0, -1.0);
    adapter.OnCursorUpdate(cursorInfo);
    cursorInfo = std::make_shared<MockIMFCursorInfoAdapter>(10000.0, 10000.0, 5000.0, 5000.0);
    adapter.OnCursorUpdate(cursorInfo);
  }

  {
    IMFAdapterImpl adapter;
    adapter.OnSelectionChange(u"", 0, 0);
    adapter.OnSelectionChange(u"a", 0, 1);
    adapter.OnSelectionChange(u"test", 0, 4);
    adapter.OnSelectionChange(u"test", -1, 5);
    adapter.OnSelectionChange(u"test", 2, 2);
    adapter.OnSelectionChange(u"test", 5, 10);
  }

  {
    IMFAdapterImpl adapter;
    adapter.SendPrivateCommand("", "");
    adapter.SendPrivateCommand("key", "");
    adapter.SendPrivateCommand("", "value");
    adapter.SendPrivateCommand("autofill.cancel", "");
    adapter.SendPrivateCommand("autofill.cancel", R"({"fillContent": "test"})");
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      IMFTextEditorProxyImpl::TextEditorProxy_SetTextFunc(proxy);
      IMFTextEditorProxyImpl::TextEditorProxy_SetFunc(proxy);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      auto mockConfig = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
      mockConfig->SetInputPattern(IMFAdapterTextInputType::TEXT);
      mockConfig->SetEnterKeyType(IMFAdapterEnterKeyType::GO);
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(mockConfig);
      (void)ret;
      IMFTextEditorProxyImpl::DestroyTextConfig();
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(nullptr);
      (void)ret;
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      std::u16string text = u"Test text for insert";
      IMFTextEditorProxyImpl::InsertTextFunc(proxy, text.c_str(), text.length());
  IMFTextEditorProxyImpl::InsertTextFunc(proxy, u"", 0);
      IMFTextEditorProxyImpl::InsertTextFunc(proxy, u"", 0);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, 0);
      IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, 1);
      IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, 100);
      IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, -1);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, 0);
      IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, 1);
      IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, 100);
      IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, -1);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      for (int i = 0; i <= 4; i++) {
        IMFTextEditorProxyImpl::MoveCursorFunc(proxy, static_cast<InputMethod_Direction>(i));
      }
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, 0, 0);
      IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, -1, 5);
      IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, 5, -1);
      IMFTextEditorProxyImpl::HandleSetSelectionFunc(proxy, 10, 5);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      for (int i = 0; i <= 3; i++) {
        IMFTextEditorProxyImpl::HandleExtendActionFunc(proxy, static_cast<InputMethod_ExtendAction>(i));
      }
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      char16_t leftText[50] = {0};
      size_t leftLength = 0;
      IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 0, leftText, &leftLength);
      IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 5, leftText, &leftLength);
      IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 100, leftText, &leftLength);
      IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, -1, leftText, &leftLength);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      char16_t rightText[50] = {0};
      size_t rightLength = 0;
      IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 0, rightText, &rightLength);
      IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 5, rightText, &rightLength);
      IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 100, rightText, &rightLength);
      IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, -1, rightText, &rightLength);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      int32_t index = IMFTextEditorProxyImpl::GetTextIndexAtCursorFunc(proxy);
      (void)index;
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      InputMethod_PrivateCommand* nullCmds[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
      IMFTextEditorProxyImpl::ReceivePrivateCommandFunc(proxy, nullCmds, 0);
      IMFTextEditorProxyImpl::ReceivePrivateCommandFunc(proxy, nullCmds, 5);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      std::u16string preview = u"preview";
      IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, preview.c_str(), preview.length(), 0, 7);
      IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, u"", 0, 0, 0);
      IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, preview.c_str(), preview.length(), -1, 5);
      IMFTextEditorProxyImpl::SetPreviewTextFunc(proxy, preview.c_str(), preview.length(), 10, 20);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy, IME_KEYBOARD_STATUS_NONE);
      IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy, IME_KEYBOARD_STATUS_SHOW);
      IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy, IME_KEYBOARD_STATUS_HIDE);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      for (int i = 0; i <= 8; i++) {
        IMFTextEditorProxyImpl::SendEnterKeyFunc(proxy, static_cast<InputMethod_EnterKeyType>(i));
      }
      IMFTextEditorProxyImpl::SendEnterKeyFunc(proxy, static_cast<InputMethod_EnterKeyType>(100));
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    impl.InsertText(u"");
    impl.InsertText(u"test");
    impl.DeleteForward(0);
    impl.DeleteForward(100);
    impl.DeleteBackward(0);
    impl.DeleteBackward(100);
    impl.SendKeyEventFromInputMethod();
    impl.SendKeyboardStatus(IMFAdapterKeyboardStatus::NONE);
    impl.SetKeyboardStatus(true);
    impl.SetKeyboardStatus(false);
  }

  {
    auto funcKey = std::make_shared<IMFAdapterFunctionKeyAdapterImpl>();
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::GO);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::SEARCH);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::SEND);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::NEXT);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::DONE);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::PREVIOUS);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::NEW_LINE);
    impl.SendFunctionKey(funcKey);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    impl.MoveCursor(IMFAdapterDirection::UP);
    impl.MoveCursor(IMFAdapterDirection::DOWN);
    impl.MoveCursor(IMFAdapterDirection::LEFT);
    impl.MoveCursor(IMFAdapterDirection::RIGHT);
    impl.HandleSetSelection(0, 10);
    impl.HandleSetSelection(-5, 5);
    impl.HandleSetSelection(5, -5);
    impl.HandleExtendAction(0);
    impl.HandleExtendAction(1);
    impl.HandleExtendAction(2);
    impl.HandleExtendAction(3);
    impl.HandleSelect(0, 0);
    impl.HandleSelect(-1, 5);
    impl.HandleSelect(5, -1);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    impl.GetTextIndexAtCursor();
    impl.GetLeftTextOfCursor(0);
    impl.GetLeftTextOfCursor(100);
    impl.GetLeftTextOfCursor(-1);
    impl.GetRightTextOfCursor(0);
    impl.GetRightTextOfCursor(100);
    impl.GetRightTextOfCursor(-1);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    impl.SetPreviewText(u"preview", 0, 7);
    impl.SetPreviewText(u"", 0, 0);
    impl.SetPreviewText(u"test", -1, 5);
    impl.SetPreviewText(u"test", 5, 2);
    impl.FinishTextPreview();
  }

  {
    IMFAdapterImpl adapter;
    std::vector<InputMethod_PrivateCommand*> cmds;
    adapter.ParseFillContentJsonValue(R"({"userName": "test"})", cmds);
    adapter.ParseFillContentJsonValue(R"({"hasAccount": "true"})", cmds);
    adapter.ParseFillContentJsonValue(R"({"userName": "test", "hasAccount": "true"})", cmds);
    adapter.ParseFillContentJsonValue(R"({})", cmds);
    adapter.ParseFillContentJsonValue("invalid", cmds);
    adapter.ParseFillContentJsonValue("", cmds);
    DestroyPrivateCommand(cmds);
  }

  {
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    adapter.AttachParamsCheck(nullptr, true, config, true);
    adapter.AttachParamsCheck(nullptr, false, config, false);
    adapter.AttachParamsCheck(std::make_shared<MockIMFTextListenerAdapter>(), true, nullptr, true);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    adapter.textEditorProxy_ = nullptr;
    adapter.Attach(listener, true);
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    config->SetInputPattern(IMFAdapterTextInputType::TEXT);
    config->SetEnterKeyType(IMFAdapterEnterKeyType::GO);
    adapter.inputMethodProxy_ = nullptr;
    adapter.OnCursorUpdate(std::make_shared<MockIMFCursorInfoAdapter>(10, 20, 30, 40));
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    adapter.textEditorProxy_ = nullptr;
    adapter.inputMethodProxy_ = nullptr;
    adapter.OnSelectionChange(u"test", 0, 4);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    adapter.textEditorProxy_ = nullptr;
    adapter.inputMethodProxy_ = nullptr;
    adapter.ShowCurrentInput(IMFAdapterTextInputType::TEXT);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(nullptr);
      (void)ret;
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    impl.ReceivePrivateCommand(nullptr, 0);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    InputMethod_PrivateCommand* badCmd = OH_PrivateCommand_Create(const_cast<char*>("bad"), 3);
    InputMethod_PrivateCommand* cmds[1] = { badCmd };
    impl.ReceivePrivateCommand(cmds, 1);
    OH_PrivateCommand_Destroy(badCmd);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    InputMethod_PrivateCommand* previewCmd = OH_PrivateCommand_Create(
        const_cast<char*>(PREVIEW_TEXT_STYLE_KEY.c_str()), PREVIEW_TEXT_STYLE_KEY.length());
    if (previewCmd) {
      char underlineValue[] = "underline";
      OH_PrivateCommand_SetStrValue(previewCmd, underlineValue, strlen(underlineValue));
      InputMethod_PrivateCommand* cmds[1] = { previewCmd };
      impl.ReceivePrivateCommand(cmds, 1);
      OH_PrivateCommand_Destroy(previewCmd);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    InputMethod_PrivateCommand* usernameCmd = OH_PrivateCommand_Create(
        const_cast<char*>(AUTO_FILL_PARAMS_USERNAME.c_str()), AUTO_FILL_PARAMS_USERNAME.length());
    if (usernameCmd) {
      char usernameValue[] = "testuser";
      OH_PrivateCommand_SetStrValue(usernameCmd, usernameValue, strlen(usernameValue));
      InputMethod_PrivateCommand* cmds[1] = { usernameCmd };
      impl.ReceivePrivateCommand(cmds, 1);
      OH_PrivateCommand_Destroy(usernameCmd);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    InputMethod_PrivateCommand* otherCmd = OH_PrivateCommand_Create(
        const_cast<char*>(AUTO_FILL_PARAMS_OTHERACCOUNT.c_str()), AUTO_FILL_PARAMS_OTHERACCOUNT.length());
    if (otherCmd) {
      InputMethod_PrivateCommand* cmds[1] = { otherCmd };
      impl.ReceivePrivateCommand(cmds, 1);
      OH_PrivateCommand_Destroy(otherCmd);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    auto impl = std::make_shared<IMFTextListenerAdapterImpl>(listener);
    impl->InsertText(u"");
    impl->InsertText(u"test");
    impl->DeleteForward(0);
    impl->DeleteBackward(0);
    impl->SendKeyEventFromInputMethod();
    impl->SendKeyboardStatus(IMFAdapterKeyboardStatus::SHOW);
    impl->SetKeyboardStatus(true);
    auto funcKey = std::make_shared<IMFAdapterFunctionKeyAdapterImpl>();
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::GO);
    impl->SendFunctionKey(funcKey);
    impl->MoveCursor(IMFAdapterDirection::UP);
    impl->HandleSetSelection(0, 10);
    impl->HandleExtendAction(0);
    impl->HandleSelect(0, 0);
    impl->GetTextIndexAtCursor();
    impl->GetLeftTextOfCursor(10);
    impl->GetRightTextOfCursor(10);
    impl->SetPreviewText(u"preview", 0, 7);
    impl->FinishTextPreview();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    auto impl = std::make_shared<IMFTextListenerAdapterImpl>(listener);
    impl->NotifyPanelStatusInfo(OHOS::MiscServices::PanelStatusInfo{});
  }

  {
    IMFAdapterImpl adapter;
    adapter.inputMethodProxy_ = nullptr;
    adapter.HideTextInput();
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    adapter.inputMethodProxy_ = nullptr;
    adapter.SendPrivateCommand("autofill.cancel", R"({"userName": "test", "hasAccount": "true"})");
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      auto mockConfig = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
      mockConfig->SetInputPattern(IMFAdapterTextInputType::TEXT);
      mockConfig->SetEnterKeyType(IMFAdapterEnterKeyType::GO);
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(mockConfig);
      (void)ret;
      InputMethod_TextConfig* funcDestConfig = OH_TextConfig_Create();
      if (funcDestConfig) {
        IMFTextEditorProxyImpl::GetTextConfigFunc(proxy, funcDestConfig);
        OH_TextConfig_Destroy(funcDestConfig);
      }
      IMFTextEditorProxyImpl::DestroyTextConfig();
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::TextEditorProxy_SetTextFunc(proxy);
      if (ret != IME_ERR_OK) {
        IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
        return 0;
      }
      ret = IMFTextEditorProxyImpl::TextEditorProxy_SetFunc(proxy);
      if (ret != IME_ERR_OK) {
        IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
        return 0;
      }
      for (int32_t i = 0; i <= 10; i++) {
        IMFTextEditorProxyImpl::SendEnterKeyFunc(proxy, static_cast<InputMethod_EnterKeyType>(i));
      }
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    IMFAdapterImpl adapter;
    std::vector<InputMethod_PrivateCommand*> cmds;
    adapter.ParseFillContentJsonValue(R"({"userName": null})", cmds);
    adapter.ParseFillContentJsonValue(R"({"userName": 123})", cmds);
    adapter.ParseFillContentJsonValue(R"({"hasAccount": null})", cmds);
    adapter.ParseFillContentJsonValue(R"({"hasAccount": 123})", cmds);
    adapter.ParseFillContentJsonValue(R"({"userName": "", "hasAccount": ""})", cmds);
    DestroyPrivateCommand(cmds);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      std::u16string longText(500, u'x');
      IMFTextEditorProxyImpl::InsertTextFunc(proxy, longText.c_str(), longText.length());
      
      char16_t leftText[300] = {0};
      size_t leftLen = 300;
      IMFTextEditorProxyImpl::GetLeftTextOfCursorFunc(proxy, 100, leftText, &leftLen);
      
      char16_t rightText[300] = {0};
      size_t rightLen = 300;
      IMFTextEditorProxyImpl::GetRightTextOfCursorFunc(proxy, 100, rightText, &rightLen);
      
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    adapter.Attach(listener, true);
    adapter.Attach(listener, false);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(0, 0.0, 100.0);
    adapter.Attach(listener, true, config, false);
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    auto nullListener = std::shared_ptr<IMFTextListenerAdapter>(nullptr);
    adapter.Attach(nullListener, true);
  }

  {
    IMFAdapterImpl adapter;
    auto validListener = std::make_shared<MockIMFTextListenerAdapter>();
    auto nullConfig = std::shared_ptr<IMFTextConfigAdapter>(nullptr);
    adapter.Attach(validListener, true, nullConfig, true);
  }

  {
    IMFAdapterImpl adapter;
    auto validListener = std::make_shared<MockIMFTextListenerAdapter>();
    auto configWithNullAttr = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    adapter.Attach(validListener, true, configWithNullAttr, false);
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    adapter.inputMethodProxy_ = nullptr;
    adapter.ShowCurrentInput(IMFAdapterTextInputType::NUMBER);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      IMFTextEditorProxyImpl::InsertTextFunc(proxy, u"test", 4);
      IMFTextEditorProxyImpl::DeleteForwardFunc(proxy, 10);
      IMFTextEditorProxyImpl::DeleteBackwardFunc(proxy, 10);
      IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy, IME_KEYBOARD_STATUS_NONE);
      IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy, IME_KEYBOARD_STATUS_SHOW);
      IMFTextEditorProxyImpl::SendKeyboardStatusFunc(proxy, IME_KEYBOARD_STATUS_HIDE);
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextListenerAdapterImpl impl(listener);
    auto funcKey = std::make_shared<IMFAdapterFunctionKeyAdapterImpl>();
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::UNSPECIFIED);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::NONE);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::DONE);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::PREVIOUS);
    impl.SendFunctionKey(funcKey);
    funcKey->SetEnterKeyType(IMFAdapterEnterKeyType::NEW_LINE);
    impl.SendFunctionKey(funcKey);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    adapter.AttachParamsCheck(listener, true, nullptr, true);
    adapter.AttachParamsCheck(listener, false, nullptr, false);
    adapter.AttachParamsCheck(nullptr, true, nullptr, true);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFTextEditorProxyImpl::textListener_ = nullptr;
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    bool result = adapter.AttachParamsCheck(listener, true, config, true);
    (void)result;
    result = adapter.AttachParamsCheck(listener, false, config, false);
    (void)result;
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    adapter.inputMethodProxy_ = nullptr;
    adapter.ShowCurrentInput(IMFAdapterTextInputType::EMAIL_ADDRESS);
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    adapter.ShowCurrentInput(IMFAdapterTextInputType::TEXT);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::NUMBER);
    adapter.ShowCurrentInput(IMFAdapterTextInputType::EMAIL_ADDRESS);
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    adapter.inputMethodProxy_ = nullptr;
    adapter.OnCursorUpdate(nullptr);
  }

  {
    IMFAdapterImpl adapter;
    auto cursorInfo = std::make_shared<MockIMFCursorInfoAdapter>(10, 20, 30, 40);
    adapter.OnCursorUpdate(cursorInfo);
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      InputMethod_TextConfig* funcDestConfig = OH_TextConfig_Create();
      if (funcDestConfig) {
        IMFTextEditorProxyImpl::GetTextConfigFunc(proxy, funcDestConfig);
        OH_TextConfig_Destroy(funcDestConfig);
      }
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      auto mockConfig = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
      mockConfig->SetInputPattern(IMFAdapterTextInputType::NUMBER);
      mockConfig->SetEnterKeyType(IMFAdapterEnterKeyType::SEARCH);
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(mockConfig);
      (void)ret;
      IMFTextEditorProxyImpl::DestroyTextConfig();
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      auto mockConfig = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
      mockConfig->SetInputPattern(IMFAdapterTextInputType::PHONE);
      mockConfig->SetEnterKeyType(IMFAdapterEnterKeyType::SEND);
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(mockConfig);
      (void)ret;
      IMFTextEditorProxyImpl::DestroyTextConfig();
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    InputMethod_TextEditorProxy* proxy = IMFTextEditorProxyImpl::TextEditorProxyCreate(listener);
    if (proxy) {
      auto mockConfig = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
      mockConfig->SetInputPattern(IMFAdapterTextInputType::URL);
      mockConfig->SetEnterKeyType(IMFAdapterEnterKeyType::NEXT);
      InputMethod_ErrorCode ret = IMFTextEditorProxyImpl::ConstructTextConfig(mockConfig);
      (void)ret;
      IMFTextEditorProxyImpl::DestroyTextConfig();
      IMFTextEditorProxyImpl::TextEditorProxyDestroy(proxy);
    }
  }

  {
    IMFAdapterImpl adapter;
    adapter.SendPrivateCommand("test", "");
    adapter.SendPrivateCommand("", "value");
    adapter.SendPrivateCommand("invalid_key", R"({"userName": "test"})");
    adapter.SendPrivateCommand("autofill.cancel", "");
    adapter.SendPrivateCommand("autofill.cancel", "invalid_json");
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    adapter.AttachWithRequestKeyboardReason(listener, true, nullptr, true, 0);
    adapter.AttachWithRequestKeyboardReason(listener, false, nullptr, false, 1);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    config->SetInputPattern(IMFAdapterTextInputType::MULTILINE);
    config->SetEnterKeyType(IMFAdapterEnterKeyType::DONE);
    adapter.AttachWithRequestKeyboardReason(listener, true, config, true, 0);
    adapter.Close();
  }

  {
    auto listener = std::make_shared<MockIMFTextListenerAdapter>();
    IMFAdapterImpl adapter;
    auto config = std::make_shared<MockIMFTextConfigAdapter>(100, 50.0, 200.0);
    config->SetInputPattern(IMFAdapterTextInputType::DATETIME);
    config->SetEnterKeyType(IMFAdapterEnterKeyType::PREVIOUS);
    adapter.AttachWithRequestKeyboardReason(listener, false, config, false, 2);
    adapter.Close();
  }

  {
    IMFAdapterImpl adapter;
    std::vector<InputMethod_PrivateCommand*> cmds;
    adapter.ParseFillContentJsonValue("null", cmds);
    adapter.ParseFillContentJsonValue("true", cmds);
    adapter.ParseFillContentJsonValue("false", cmds);
    adapter.ParseFillContentJsonValue("[]", cmds);
    adapter.ParseFillContentJsonValue("123", cmds);
    adapter.ParseFillContentJsonValue(R"({"userName": "test", "hasAccount": "test", "extra": "value"})", cmds);
    DestroyPrivateCommand(cmds);
  }

  return 0;
}
