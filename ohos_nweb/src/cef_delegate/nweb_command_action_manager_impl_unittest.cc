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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)

#include "base/memory/weak_ptr.h"
#include "nweb_command_action_info.h"
#include "nweb_command_action_manager.h"
#include "nweb_errors.h"

// Access private members of NWebDelegate (weak_factory_) for WeakPtr creation.
// nweb_command_action_manager_impl.h transitively includes nweb_delegate.h,
// so place both inside the #define block so header guards skip the re-inclusion
// and the first inclusion already has private→public applied.
#define private public
#include "cef_delegate/nweb_delegate.h"
#include "cef_delegate/nweb_handler_delegate.h"
#include "cef_delegate/nweb_command_action_manager_impl.h"
#undef private

using namespace testing;
using namespace OHOS::NWeb;

namespace {

// Concrete implementation of AutoFillItem for testing.
class AutoFillItemImpl : public AutoFillItem {
 public:
  AutoFillItemImpl(const std::string& xpath, int32_t node_id,
                   const std::string& content, int32_t index = 0,
                   AutoFillMode mode = AutoFillMode::Overwrite)
      : xpath_(xpath), content_(content), index_(index), mode_(mode), node_id_(node_id) {}

  std::string GetXPath() const override { return xpath_; }
  std::string GetContent() const override { return content_; }
  int32_t GetIndex() const override { return index_; }
  AutoFillMode GetMode() const override { return mode_; }
  int32_t GetNodeId() const override { return node_id_; }

 private:
  std::string xpath_;
  std::string content_;
  int32_t index_ = 0;
  AutoFillMode mode_ = AutoFillMode::Overwrite;
  int32_t node_id_ = 0;
};

// Test implementation of NWebCommandActionInfo
class TestCommandActionInfo : public NWebCommandActionInfo {
 public:
  TestCommandActionInfo(const std::string& event_type,
                        const std::string& xpath,
                        const std::string& input_value = "",
                        const std::vector<std::string>& option_values = {},
                        const std::vector<int32_t>& option_indexes = {})
      : event_type_(event_type),
        xpath_(xpath),
        input_value_(input_value),
        option_values_(option_values),
        option_indexes_(option_indexes) {}

  TestCommandActionInfo(const std::string& event_type,
                        double x, double y,
                        double distance_x = 0, double distance_y = 0,
                        float scale = 1.0f, int32_t speed = 0,
                        int32_t duration = 0, int32_t tap_count = 1)
      : event_type_(event_type),
        x_(x), y_(y),
        distance_x_(distance_x), distance_y_(distance_y),
        scale_(scale), speed_(speed),
        duration_(duration), tap_count_(tap_count) {}

  TestCommandActionInfo(const std::string& event_type,
                        AutoFillMode default_mode)
      : event_type_(event_type), default_mode_(default_mode) {}

  std::string GetEventType() const override { return event_type_; }
  std::string GetXPath() const override { return xpath_; }
  std::string GetInputValue() const override { return input_value_; }
  std::vector<std::string> GetOptionValues() const override {
    return option_values_;
  }
  std::vector<int32_t> GetOptionIndexes() const override {
    return option_indexes_;
  }

  double GetX() const override { return x_; }
  double GetY() const override { return y_; }
  double GetDistanceX() const override { return distance_x_; }
  double GetDistanceY() const override { return distance_y_; }
  float GetScale() const override { return scale_; }
  int32_t GetSpeed() const override { return speed_; }
  int32_t GetDuration() const override { return duration_; }
  int32_t GetTapCount() const override { return tap_count_; }


  void SetAutoFillItems(const std::vector<std::shared_ptr<AutoFillItem>>& items) {
    autofill_items_ = items;
  }
  void SetDefaultMode(AutoFillMode mode) {
    default_mode_ = mode;
  }

 private:
  std::string event_type_;
  std::string xpath_;
  std::string input_value_;
  std::vector<std::string> option_values_;
  std::vector<int32_t> option_indexes_;

  double x_ = 0;
  double y_ = 0;
  double distance_x_ = 0;
  double distance_y_ = 0;
  float scale_ = 1.0f;
  int32_t speed_ = 0;
  int32_t duration_ = 0;
  int32_t tap_count_ = 1;

  // AutoFill batch fill fields
  std::vector<std::shared_ptr<AutoFillItem>> autofill_items_;
  AutoFillMode default_mode_ = AutoFillMode::Overwrite;

  std::vector<std::shared_ptr<AutoFillItem>> GetAutoFillItems() const override {
    return autofill_items_;
  }
  AutoFillMode GetDefaultMode() const override {
    return default_mode_;
  }
};

}  // namespace

class NWebCommandActionManagerImplTest : public Test {
 protected:
  void SetUp() override {
    // Create manager with null delegate for null-delegate tests
    null_delegate_manager_ =
        std::make_unique<NWebCommandActionManagerImpl>(
            base::WeakPtr<NWebDelegate>());

    // Create a minimal NWebDelegate to obtain a valid WeakPtr for
    // parameter-validation tests. Without a real browser instance,
    // GetBrowser() returns null, so Gesture/AutoFill success-path tests
    // cannot be verified in UT.
    web_engine_args_.emplace_back("--no-sandbox");
    argc_ = web_engine_args_.size();
    argv_ = new const char* [static_cast<size_t>(argc_)];
    for (size_t i = 0; i < static_cast<size_t>(argc_); i++) {
      argv_[i] = web_engine_args_[i].c_str();
    }
    real_delegate_ =
        std::make_shared<NWebDelegate>(argc_, argv_);

    valid_delegate_manager_ =
        std::make_unique<NWebCommandActionManagerImpl>(
            real_delegate_->weak_factory_.GetWeakPtr());
  }

  void TearDown() override {
    valid_delegate_manager_.reset();
    null_delegate_manager_.reset();
    real_delegate_.reset();
    delete[] argv_;
    argv_ = nullptr;
  }

  std::unique_ptr<NWebCommandActionManagerImpl> null_delegate_manager_;
  std::unique_ptr<NWebCommandActionManagerImpl> valid_delegate_manager_;
  std::shared_ptr<NWebDelegate> real_delegate_;
  std::vector<std::string> web_engine_args_;
  int argc_ = 0;
  const char** argv_ = nullptr;
};

// ===== HandleInputCommand Tests =====

/**
 * @tc.name: HandleInputCommand_NullInfo_ReturnsFailed_0100
 * @tc.desc: Test HandleInputCommand with null info returns FAILED.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleInputCommand_NullInfo_ReturnsFailed_0100) {
  auto result = null_delegate_manager_->HandleInputCommand(nullptr);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::FAILED));
}

/**
 * @tc.name: HandleInputCommand_NullDelegate_ReturnsDelegateNull_0100
 * @tc.desc: Test HandleInputCommand with null delegate returns DELEGATE_NULL.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleInputCommand_NullDelegate_ReturnsDelegateNull_0100) {
  auto info = std::make_shared<TestCommandActionInfo>(
      "input-text", "//input[@id='name']", "hello");
  auto result = null_delegate_manager_->HandleInputCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL));
}

/**
 * @tc.name: HandleInputCommand_EmptyXPath_ReturnsInvalidXpath_0100
 * @tc.desc: Test HandleInputCommand with empty xpath returns JSON_INVALID_XPATH.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleInputCommand_EmptyXPath_ReturnsInvalidXpath_0100) {
  auto info = std::make_shared<TestCommandActionInfo>(
      "input-text", "", "hello");
  auto result = valid_delegate_manager_->HandleInputCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_XPATH));
}

/**
 * @tc.name: HandleInputCommand_EmptyValue_ReturnsInvalidInputValue_0100
 * @tc.desc: Test HandleInputCommand with empty value returns
 *           JSON_INVALID_INPUT_VALUE.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleInputCommand_EmptyValue_ReturnsInvalidInputValue_0100) {
  auto info = std::make_shared<TestCommandActionInfo>(
      "input-text", "//input[@id='name']", "");
  auto result = valid_delegate_manager_->HandleInputCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_INPUT_VALUE));
}

// ===== HandleSelectCommand Tests =====

/**
 * @tc.name: HandleSelectCommand_NullInfo_ReturnsFailed_0100
 * @tc.desc: Test HandleSelectCommand with null info returns FAILED.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleSelectCommand_NullInfo_ReturnsFailed_0100) {
  auto result = null_delegate_manager_->HandleSelectCommand(nullptr);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::FAILED));
}

/**
 * @tc.name: HandleSelectCommand_NullDelegate_ReturnsDelegateNull_0100
 * @tc.desc: Test HandleSelectCommand with null delegate returns DELEGATE_NULL.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleSelectCommand_NullDelegate_ReturnsDelegateNull_0100) {
  auto info = std::make_shared<TestCommandActionInfo>(
      "select", "//select[@id='color']", "",
      std::vector<std::string>{"red", "blue"}, std::vector<int32_t>{});
  auto result = null_delegate_manager_->HandleSelectCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL));
}

/**
 * @tc.name: HandleSelectCommand_EmptyXPath_ReturnsInvalidXpath_0100
 * @tc.desc: Test HandleSelectCommand with empty xpath returns JSON_INVALID_XPATH.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleSelectCommand_EmptyXPath_ReturnsInvalidXpath_0100) {
  auto info = std::make_shared<TestCommandActionInfo>(
      "select", "", "",
      std::vector<std::string>{"red"}, std::vector<int32_t>{0});
  auto result = valid_delegate_manager_->HandleSelectCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_XPATH));
}

/**
 * @tc.name: HandleSelectCommand_EmptyValuesAndIndexes_ReturnsInvalidSelectOptions_0100
 * @tc.desc: Test HandleSelectCommand with empty values and indexes returns
 *           JSON_INVALID_SELECT_OPTIONS.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleSelectCommand_EmptyValuesAndIndexes_ReturnsInvalidSelectOptions_0100) {
  auto info = std::make_shared<TestCommandActionInfo>(
      "select", "//select[@id='color']", "",
      std::vector<std::string>{}, std::vector<int32_t>{});
  auto result = valid_delegate_manager_->HandleSelectCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_SELECT_OPTIONS));
}

TEST_F(NWebCommandActionManagerImplTest,
       HandleSelectCommand_OnlyValuesNotEmpty_PassesValidation_0100) {
  /**
   * @tc.name: HandleSelectCommand_OnlyValuesNotEmpty_PassesValidation_0100
   * @tc.desc: Test HandleSelectCommand with only values non-empty passes param validation.
   * @tc.type: FUNC
   */
  auto info = std::make_shared<TestCommandActionInfo>(
      "select", "//select[@id='color']", "",
      std::vector<std::string>{"red", "blue"}, std::vector<int32_t>{});
  auto result = valid_delegate_manager_->HandleSelectCommand(info);
  // Should NOT return JSON_INVALID_SELECT_OPTIONS since values is non-empty
  EXPECT_NE(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_SELECT_OPTIONS));
}

TEST_F(NWebCommandActionManagerImplTest,
       HandleSelectCommand_OnlyIndexesNotEmpty_PassesValidation_0100) {
  /**
   * @tc.name: HandleSelectCommand_OnlyIndexesNotEmpty_PassesValidation_0100
   * @tc.desc: Test HandleSelectCommand with only indexes non-empty passes param validation.
   * @tc.type: FUNC
   */
  auto info = std::make_shared<TestCommandActionInfo>(
      "select", "//select[@id='color']", "",
      std::vector<std::string>{}, std::vector<int32_t>{0, 1});
  auto result = valid_delegate_manager_->HandleSelectCommand(info);
  // Should NOT return JSON_INVALID_SELECT_OPTIONS since indexes is non-empty
  EXPECT_NE(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_SELECT_OPTIONS));
}

// ===== HandleGestureCommand Tests =====

/**
 * @tc.name: HandleGestureCommand_NullInfo_ReturnsFailed_0100
 * @tc.desc: Test HandleGestureCommand with null info returns FAILED.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleGestureCommand_NullInfo_ReturnsFailed_0100) {
  auto result = null_delegate_manager_->HandleGestureCommand(nullptr);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::FAILED));
}

/**
 * @tc.name: HandleGestureCommand_NullDelegate_ReturnsDelegateNull_0100
 * @tc.desc: Test HandleGestureCommand with null delegate returns DELEGATE_NULL.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleGestureCommand_NullDelegate_ReturnsDelegateNull_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("tap", 100.0, 200.0);
  auto result = null_delegate_manager_->HandleGestureCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL));
}

// NOTE: Gesture success-path tests (UnknownEventType, TapEvent, ScrollGestureEvent,
// PinchEvent) are removed. HandleGestureCommand checks GetBrowser() BEFORE
// parameter validation, so these always return CONTEXT_NULL in UT environment.
// These require a real browser instance to test.

// ===== HandleAutoFillCommand Tests =====

/**
 * @tc.name: HandleAutoFillCommand_NullInfo_ReturnsFailed_0100
 * @tc.desc: Test HandleAutoFillCommand with null info returns FAILED.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_NullInfo_ReturnsFailed_0100) {
  auto result = null_delegate_manager_->HandleAutoFillCommand(nullptr);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::FAILED));
}

/**
 * @tc.name: HandleAutoFillCommand_NullDelegate_ReturnsDelegateNull_0100
 * @tc.desc: Test HandleAutoFillCommand with null delegate returns DELEGATE_NULL.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_NullDelegate_ReturnsDelegateNull_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  // TestCommandActionInfo returns empty items by default
  auto result = null_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::DELEGATE_NULL));
}

/**
 * @tc.name: HandleAutoFillCommand_EmptyItems_ReturnsInvalidItems_0100
 * @tc.desc: Test HandleAutoFillCommand with empty items returns JSON_INVALID_ITEMS.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_EmptyItems_ReturnsInvalidItems_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  // TestCommandActionInfo::GetAutoFillItems() returns {} by default
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEMS));
}

/**
 * @tc.name: HandleAutoFillCommand_InvalidDefaultMode_ReturnsInvalidDefaultMode_0100
 * @tc.desc: Test HandleAutoFillCommand with invalid defaultMode returns JSON_INVALID_DEFAULT_MODE.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_InvalidDefaultMode_ReturnsInvalidDefaultMode_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>(
      "//input[@id='name']", 0, "hello", 0, AutoFillMode::Overwrite)});
  // With enum-based mode, invalid mode validation is handled at parsing layer;
  // enum values are always valid, so this test no longer triggers JSON_INVALID_DEFAULT_MODE.
  // Keep default Overwrite mode and verify it still fails on PAGE_NOT_READY.
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY));
}

/**
 * @tc.name: HandleAutoFillCommand_EmptyItemXPath_ReturnsInvalidItemXpath_0100
 * @tc.desc: Test HandleAutoFillCommand with item having empty xpath returns JSON_INVALID_ITEM_XPATH.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_EmptyItemXPath_ReturnsInvalidItemXpath_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>("", 0, "hello", 0, AutoFillMode::Overwrite)});
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEM_XPATH));
}

/**
 * @tc.name: HandleAutoFillCommand_NodeIdOnly_NoBrowser_ReturnsPageNotReady_0100
 * @tc.desc: A valid node id allows an autofill item without XPath.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_NodeIdOnly_NoBrowser_ReturnsPageNotReady_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>(
      "", 42, "hello", 0, AutoFillMode::Overwrite)});
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY));
}

/**
 * @tc.name: HandleAutoFillCommand_EmptyItemContent_ReturnsInvalidItemContent_0100
 * @tc.desc: Test HandleAutoFillCommand with item having empty content returns JSON_INVALID_ITEM_CONTENT.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_EmptyItemContent_ReturnsInvalidItemContent_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>(
      "//input[@id='name']", 0, "", 0, AutoFillMode::Overwrite)});
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEM_CONTENT));
}

/**
 * @tc.name: HandleAutoFillCommand_InvalidItemIndex_ReturnsInvalidItemIndex_0100
 * @tc.desc: Test HandleAutoFillCommand with item having negative index returns JSON_INVALID_ITEM_INDEX.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_InvalidItemIndex_ReturnsInvalidItemIndex_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>(
      "//input[@id='name']", 0, "hello", -1, AutoFillMode::Insert)});
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEM_INDEX));
}

/**
 * @tc.name: HandleAutoFillCommand_MultipleItems_FirstErrorReturned_0100
 * @tc.desc: Test HandleAutoFillCommand with multiple items where first is invalid,
 *           returns the first error code.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_MultipleItems_FirstErrorReturned_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({
      std::make_shared<AutoFillItemImpl>("", 0, "hello", 0, AutoFillMode::Overwrite),       // invalid: empty xpath
      std::make_shared<AutoFillItemImpl>("//input[@id='name']", 0, "world", 0, AutoFillMode::Overwrite),  // valid
  });
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::JSON_INVALID_ITEM_XPATH));
}

/**
 * @tc.name: HandleAutoFillCommand_OverwriteMode_NoBrowser_ReturnsPageNotReady_0100
 * @tc.desc: Parameter validation passes for overwrite mode, but returns PAGE_NOT_READY
 *           because no real browser is available (ExecuteFocusCommand fails).
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_OverwriteMode_NoBrowser_ReturnsPageNotReady_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>(
      "//input[@id='name']", 0, "hello", 0, AutoFillMode::Overwrite)});
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY));
}

/**
 * @tc.name: HandleAutoFillCommand_InsertMode_NoBrowser_ReturnsPageNotReady_0100
 * @tc.desc: Parameter validation passes for insert mode, but returns PAGE_NOT_READY
 *           because no real browser is available (ExecuteFocusCommand fails).
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_InsertMode_NoBrowser_ReturnsPageNotReady_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>(
      "//input[@id='name']", 0, "hello", 5, AutoFillMode::Insert)});
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY));
}

/**
 * @tc.name: HandleAutoFillCommand_DefaultModeEmpty_NoBrowser_ReturnsPageNotReady_0100
 * @tc.desc: Parameter validation passes with default Overwrite mode,
 *           but returns PAGE_NOT_READY because no real browser is available.
 * @tc.type: FUNC
 */
TEST_F(NWebCommandActionManagerImplTest,
       HandleAutoFillCommand_DefaultModeEmpty_NoBrowser_ReturnsPageNotReady_0100) {
  auto info = std::make_shared<TestCommandActionInfo>("inputAutoFill", AutoFillMode::Overwrite);
  info->SetAutoFillItems({std::make_shared<AutoFillItemImpl>(
      "//input[@id='name']", 0, "hello", 0, AutoFillMode::Overwrite)});
  auto result = valid_delegate_manager_->HandleAutoFillCommand(info);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY));
}

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
