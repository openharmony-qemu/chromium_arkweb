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
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html_names.h"
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

// ===== ExecuteFocus Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_ElementNotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no inputs</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//input[@id='missing']", false);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_InputSuppressKeyboard_NotReadonly) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//input[@id='name']", true);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  // After focus with suppress, the readonly attribute should be removed
  EXPECT_FALSE(input->hasAttribute(html_names::kReadonlyAttr));
  // Element should be focused
  EXPECT_EQ(GetDocument().FocusedElement(), input);
}

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_InputSuppressKeyboard_AlreadyReadonly) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' readonly></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//input[@id='name']", true);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  // Readonly attribute should remain
  EXPECT_TRUE(input->hasAttribute(html_names::kReadonlyAttr));
}

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_TextareaSuppressKeyboard) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><textarea id='desc'></textarea></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//textarea[@id='desc']", true);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().getElementById(AtomicString("desc")));
  ASSERT_NE(textarea, nullptr);
  EXPECT_FALSE(textarea->hasAttribute(html_names::kReadonlyAttr));
}

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_ContentEditableSuppressKeyboard_NoTabindex) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='editor' contenteditable='true'></div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//div[@id='editor']", true);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(el, nullptr);
  // tabindex should be restored (removed since it wasn't there before)
  EXPECT_FALSE(el->hasAttribute(html_names::kTabindexAttr));
  // contentEditable should be restored to "true"
  EXPECT_EQ(el->getAttribute(html_names::kContenteditableAttr), "true");
}

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_ContentEditableSuppressKeyboard_WithTabindex) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='editor' contenteditable='true' tabindex='0'></div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//div[@id='editor']", true);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(el, nullptr);
  // tabindex should be restored to original "0"
  EXPECT_EQ(el->getAttribute(html_names::kTabindexAttr), "0");
  // contentEditable should be restored
  EXPECT_EQ(el->getAttribute(html_names::kContenteditableAttr), "true");
}

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_NoSuppress_FocusAndClick) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//input[@id='name']", false);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(GetDocument().FocusedElement(), input);
}

TEST_F(CommandActionDOMHelperTest, ExecuteFocus_NonHTMLElement) {
  // SVG elements are not HTMLElements
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><svg id='svg'><rect id='rect'/></svg></body>");
  UpdateAllLifecyclePhasesForTest();

  // SVG rect is not an HTMLElement, but ExecuteFocus doesn't check for that
  // explicitly - it falls through to the else branch and tries Focus().
  // The test just verifies it doesn't crash.
  int32_t result = CommandActionDOMHelper::ExecuteFocus(
      &GetDocument(), "//*[@id='rect']", false);
  // The method always returns SUCCESS in the current implementation
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

// ===== ExecuteInputModify Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteInputModify_NoFocusedElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no focusable elements</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "hello");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputModify_InputReadonly) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' readonly></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "hello");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputModify_InputSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "hello");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(input->Value(), "hello");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputModify_TextareaReadonly) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><textarea id='desc' readonly></textarea></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().getElementById(AtomicString("desc")));
  ASSERT_NE(textarea, nullptr);
  GetDocument().SetFocusedElement(textarea, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                         mojom::blink::FocusType::kNone,
                                                         nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "hello");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputModify_TextareaSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><textarea id='desc'></textarea></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().getElementById(AtomicString("desc")));
  ASSERT_NE(textarea, nullptr);
  GetDocument().SetFocusedElement(textarea, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                         mojom::blink::FocusType::kNone,
                                                         nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "hello world");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(textarea->Value(), "hello world");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputModify_ContentEditableSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='editor' contenteditable='true'></div></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(el, nullptr);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                   mojom::blink::FocusType::kNone,
                                                   nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "new content");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(el->innerText(), "new content");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputModify_FocusedNonEditable) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='static'>text</div></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* el = GetDocument().getElementById(AtomicString("static"));
  ASSERT_NE(el, nullptr);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                   mojom::blink::FocusType::kNone,
                                                   nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "hello");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

// ===== ExecuteInputInsert Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_NoFocusedElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no focusable elements</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), "hello", 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_InputAtStart) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' value='world'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), "hello ", 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(input->Value(), "hello world");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_InputAtMiddle) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' value='helloworld'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), " ", 5);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(input->Value(), "hello world");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_InputIndexClamp) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' value='hi'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  // Index 100 > length("hi")=2, should clamp to end
  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), " world", 100);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(input->Value(), "hi world");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_TextareaSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><textarea id='desc'>abc</textarea></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().getElementById(AtomicString("desc")));
  ASSERT_NE(textarea, nullptr);
  GetDocument().SetFocusedElement(textarea, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                         mojom::blink::FocusType::kNone,
                                                         nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), "XYZ", 1);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ(textarea->Value(), "aXYZbc");
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_InputReadonly) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' readonly value='hello'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), "x", 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_ContentEditableSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='editor' contenteditable='true'>hello</div></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(el, nullptr);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                   mojom::blink::FocusType::kNone,
                                                   nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), " world", 5);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

// ===== ExecuteClick Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteClick_ElementNotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no buttons</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteClick(
      &GetDocument(), "//button[@id='submit']");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteClick_Success) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><button id='submit'>Click</button></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteClick(
      &GetDocument(), "//button[@id='submit']");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteClick_NonHTMLElement) {
  // Test with a non-HTMLElement (SVG element)
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><svg id='svg'><rect id='rect'/></svg></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteClick(
      &GetDocument(), "//*[@id='rect']");
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

// ===== ExecuteScroll Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_ElementNotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no target</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "top", 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_AlignmentTop) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='target'>content</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "top", 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_AlignmentMid) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='target'>content</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "mid", 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_AlignmentCenter) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='target'>content</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "center", 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_AlignmentBottom) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='target'>content</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "bottom", 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_UnknownAlignment_DefaultTop) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='target'>content</div></body>");
  UpdateAllLifecyclePhasesForTest();

  // Unknown alignment should default to "top"
  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "unknown", 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_WithOffset) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='target'>content</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "top", 50);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_NegativeOffset) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='target'>content</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "top", -20);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

// ===== ExecuteInputSelect Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteInputSelect_NoFocusedElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no focus</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSelect(
      &GetDocument(), 0, 5);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSelect_InputSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' value='hello world'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSelect(
      &GetDocument(), 0, 5);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSelect_TextareaSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><textarea id='desc'>hello world</textarea></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().getElementById(AtomicString("desc")));
  ASSERT_NE(textarea, nullptr);
  GetDocument().SetFocusedElement(textarea, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                         mojom::blink::FocusType::kNone,
                                                         nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSelect(
      &GetDocument(), 0, 5);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSelect_ContentEditableSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='editor' contenteditable='true'>hello</div></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(el, nullptr);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                   mojom::blink::FocusType::kNone,
                                                   nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSelect(
      &GetDocument(), 0, 3);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSelect_NegativeIndexClamp) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' value='hello'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  // Negative start/finish should be clamped to 0
  int32_t result = CommandActionDOMHelper::ExecuteInputSelect(
      &GetDocument(), -5, -1);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSelect_NonEditableElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='static' tabindex='0'>text</div></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* el = GetDocument().getElementById(AtomicString("static"));
  ASSERT_NE(el, nullptr);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                   mojom::blink::FocusType::kNone,
                                                   nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSelect(
      &GetDocument(), 0, 3);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

// ===== ExecuteInputSetCursor Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteInputSetCursor_NoFocusedElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>no focus</div></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSetCursor_InputSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' value='hello'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 3);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSetCursor_TextareaSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><textarea id='desc'>hello</textarea></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().getElementById(AtomicString("desc")));
  ASSERT_NE(textarea, nullptr);
  GetDocument().SetFocusedElement(textarea, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                         mojom::blink::FocusType::kNone,
                                                         nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 2);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSetCursor_ContentEditableSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='editor' contenteditable='true'>hello</div></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(el, nullptr);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                   mojom::blink::FocusType::kNone,
                                                   nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 3);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSetCursor_NegativeIndexClamp) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text' value='hello'></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
  ASSERT_NE(input, nullptr);
  GetDocument().SetFocusedElement(input, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                      mojom::blink::FocusType::kNone,
                                                      nullptr));
  UpdateAllLifecyclePhasesForTest();

  // Negative index should be clamped to 0
  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), -5);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputSetCursor_NonEditableElement) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div id='static' tabindex='0'>text</div></body>");
  UpdateAllLifecyclePhasesForTest();

  auto* el = GetDocument().getElementById(AtomicString("static"));
  ASSERT_NE(el, nullptr);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                   mojom::blink::FocusType::kNone,
                                                   nullptr));
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
