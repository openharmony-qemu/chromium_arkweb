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

#include "arkweb/build/features/features.h"

#if BUILDFLAG(ARKWEB_INPUT_EVENTS)

#include "arkweb/chromium_ext/third_party/blink/renderer/core/dom/command_action_dom_helper.h"
#include "arkweb/ohos_nweb/include/nweb_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/size.h"

using OHOS::NWeb::WebCommandErrorCode;

namespace blink {

class CommandActionDOMHelperTest : public PageTestBase {
 protected:
  void SetUp() override { PageTestBase::SetUp(gfx::Size(800, 600)); }
};

// ===== FindElementByXPath Tests =====

TEST_F(CommandActionDOMHelperTest, FindElementByXPath_NullDocument) {
  Element* result =
      CommandActionDOMHelper::FindElementByXPath(nullptr, "/html/body");
  EXPECT_EQ(result, nullptr);
}

TEST_F(CommandActionDOMHelperTest, FindElementByXPath_ElementFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  Element* result = CommandActionDOMHelper::FindElementByXPath(
      &GetDocument(), "//input[@id='name']");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetIdAttribute(), "name");
}

TEST_F(CommandActionDOMHelperTest, FindElementByXPath_NotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>hello</div></body>");
  UpdateAllLifecyclePhasesForTest();

  Element* result = CommandActionDOMHelper::FindElementByXPath(
      &GetDocument(), "//input[@id='missing']");
  EXPECT_EQ(result, nullptr);
}

TEST_F(CommandActionDOMHelperTest, FindElementByXPath_InvalidXPath) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>hello</div></body>");
  UpdateAllLifecyclePhasesForTest();

  Element* result = CommandActionDOMHelper::FindElementByXPath(
      &GetDocument(), "///[invalid[[[");
  EXPECT_EQ(result, nullptr);
}

// ===== ExecuteInput Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteInput_ElementNotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no inputs</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInput(
      &GetDocument(), "//input[@id='name']", "hello", "input-text");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInput_NotInputElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='name'>text</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInput(
      &GetDocument(), "//div[@id='name']", "hello", "input-text");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInput_EventTypeMismatch) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInput(
      &GetDocument(), "//input[@id='name']", "hello", "input-password");
  EXPECT_EQ(result, static_cast<int32_t>(
                        WebCommandErrorCode::INPUT_EVENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInput_Success) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInput(
      &GetDocument(), "//input[@id='name']", "hello", "input-text");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(input->Value(), "hello");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInput_InvalidValueForNumberType) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='age' type='number'></body>");
  UpdateAllLifecyclePhasesForTest();

  // "abc" is not a valid value for type=number
  int32_t result = CommandActionDOMHelper::ExecuteInput(
      &GetDocument(), "//input[@id='age']", "abc", "input-number");
  EXPECT_EQ(result, static_cast<int32_t>(
                        WebCommandErrorCode::INPUT_VALUE_FORMAT_INVALID));
}

// ===== ExecuteAIPageInput Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputByXPath_DefaultTypeSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteAIPageInputByXPath(
      &GetDocument(), "//input[@id='name']", "hello-default", "default");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(input->Value(), "hello-default");
}

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputByXPath_UnsupportedType) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteAIPageInputByXPath(
      &GetDocument(), "//input[@id='name']", "hello", "datetime");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_INVALID));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputById_PageNotReady) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteAIPageInputById(
      nullptr, &GetDocument(), "invalid", "hello", "default");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY));
}

// ===== ExecuteSelect Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ElementNotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no selects</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {0}, {});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_NotSelectElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='color'>text</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//div[@id='color']", {0}, {});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_EmptyIndexesAndValues) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {}, {});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::SELECT_EMPTY_OPTIONS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ByIndex_Success) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {1}, {});
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ByIndex_OutOfRange) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {5}, {});
  EXPECT_EQ(result, static_cast<int32_t>(
                        WebCommandErrorCode::SELECT_INDEX_OUT_OF_RANGE));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ByIndex_NegativeIndex) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {-1}, {});
  EXPECT_EQ(result, static_cast<int32_t>(
                        WebCommandErrorCode::SELECT_INDEX_OUT_OF_RANGE));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ByValue_Success) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {}, {"blue"});
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ByValue_NotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {}, {"green"});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::SELECT_VALUE_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_SingleSelect_MultipleIndexes) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {0, 1}, {});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::SELECT_NOT_MULTIPLE));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_SingleSelect_MultipleValues) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {}, {"red", "blue"});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::SELECT_NOT_MULTIPLE));
}

// --- Disabled option tests ---

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ByIndex_DisabledOption) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue' disabled>Blue</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {1}, {});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_ByValue_DisabledOption) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue' disabled>Blue</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {}, {"blue"});
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED));
}

// --- Multiple select tests ---

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_MultipleSelect_ByIndexes) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color' multiple>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "<option value='green'>Green</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {0, 2}, {});
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_MultipleSelect_ByValues) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color' multiple>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "<option value='green'>Green</option>"
      "</select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelect(
      &GetDocument(), "//select[@id='color']", {}, {"red", "green"});
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelectById_PageNotReady) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'><option value='red'>Red</option></select></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteSelectById(
      nullptr, &GetDocument(), "invalid", {0}, {});
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::PAGE_NOT_READY));
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
