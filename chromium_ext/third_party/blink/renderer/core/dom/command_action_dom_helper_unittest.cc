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

#include <array>
#include <string>
#include <string_view>

#include "arkweb/chromium_ext/third_party/blink/renderer/core/dom/command_action_dom_helper.h"
#include "arkweb/ohos_nweb/include/nweb_errors.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

using OHOS::NWeb::WebCommandErrorCode;

namespace blink {

// FindElementById needs a WebLocalFrame. PageTestBase only creates an internal
// LocalFrame, for which WebLocalFrameImpl::FromFrame() may return nullptr.
class CommandActionDOMHelperSimTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    ResizeView(gfx::Size(800, 600));
  }

  WebLocalFrame* WebFrame() { return &MainFrame(); }

  void SetBodyContent(const std::string& body_content) {
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
        String::FromUTF8(body_content));
    UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }
};

class CommandActionDOMHelperTest : public CommandActionDOMHelperSimTest {
 protected:
  void SetUp() override {
    CommandActionDOMHelperSimTest::SetUp();
    WebFrame()->SetEmbeddingToken(base::UnguessableToken::Create());
  }

  std::string ElementId(Element& element,
                        std::string_view frame_token_prefix = {}) {
    const FrameToken& token = WebFrame()->GetFrameToken();
    EXPECT_TRUE(token.Is<LocalFrameToken>());
    EXPECT_TRUE(WebFrame()->GetEmbeddingToken().has_value());
    if (!token.Is<LocalFrameToken>() ||
        !WebFrame()->GetEmbeddingToken().has_value()) {
      return std::string();
    }
    return std::string(frame_token_prefix) +
           token.GetAs<LocalFrameToken>().ToString() + "|" +
           WebFrame()->GetEmbeddingToken()->ToString() + "|" +
           std::to_string(element.GetDomNodeId());
  }
};

class CommandActionDOMHelperWithoutEmbeddingTokenTest
    : public CommandActionDOMHelperSimTest {};

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

TEST_F(CommandActionDOMHelperTest, FindElementByXPath_NonElementNode) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><div>hello</div></body>");

  Element* result = CommandActionDOMHelper::FindElementByXPath(&GetDocument(),
                                                               "//div/text()");
  EXPECT_EQ(nullptr, result);
}

TEST_F(CommandActionDOMHelperTest, FindElementById_InvalidIdentifiers) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  const std::array<std::string_view, 7> invalid_ids = {
      "",
      "frame|scope",
      "frame|scope|",
      "|scope|1",
      "frame||1",
      "frame|scope|0",
      "frame|scope|not-a-number"};

  for (std::string_view id : invalid_ids) {
    int32_t error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
    EXPECT_EQ(nullptr,
              CommandActionDOMHelper::FindElementById(
                  WebFrame(), &GetDocument(), std::string(id), &error_code));
    EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::PARAM_INVALID),
              error_code);
  }
}

TEST_F(CommandActionDOMHelperTest, FindElementById_TokenMismatch) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  Element* const input = GetDocument().getElementById(AtomicString("name"));
  ASSERT_NE(nullptr, input);
  const std::string valid_id = ElementId(*input);
  ASSERT_FALSE(valid_id.empty());
  const size_t first_separator = valid_id.find('|');
  ASSERT_NE(std::string::npos, first_separator);

  int32_t error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
  EXPECT_EQ(nullptr, CommandActionDOMHelper::FindElementById(
                         WebFrame(), &GetDocument(),
                         "different-frame" + valid_id.substr(first_separator),
                         &error_code));
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND),
            error_code);

  const size_t last_separator = valid_id.rfind('|');
  ASSERT_NE(std::string::npos, last_separator);
  const std::string wrong_document_id =
      valid_id.substr(0, first_separator + 1) + "different-document" +
      valid_id.substr(last_separator);
  EXPECT_EQ(nullptr,
            CommandActionDOMHelper::FindElementById(
                WebFrame(), &GetDocument(), wrong_document_id, &error_code));
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND),
            error_code);
}

TEST_F(CommandActionDOMHelperTest, FindElementById_ElementFoundWithPrefixes) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  Element* const input = GetDocument().getElementById(AtomicString("name"));
  ASSERT_NE(nullptr, input);

  for (std::string_view prefix :
       {std::string_view(), std::string_view("local:"),
        std::string_view("remote:")}) {
    int32_t error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
    EXPECT_EQ(input, CommandActionDOMHelper::FindElementById(
                         WebFrame(), &GetDocument(), ElementId(*input, prefix),
                         &error_code));
    EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SUCCESS), error_code);
  }

  EXPECT_EQ(input, CommandActionDOMHelper::FindElementById(
                       WebFrame(), &GetDocument(), ElementId(*input), nullptr));
}

TEST_F(CommandActionDOMHelperTest, FindElementById_NodeNotFound) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  Element* const input = GetDocument().getElementById(AtomicString("name"));
  ASSERT_NE(nullptr, input);
  std::string id = ElementId(*input);
  id.replace(id.rfind('|') + 1, std::string::npos, "2147483647");

  int32_t error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
  EXPECT_EQ(nullptr, CommandActionDOMHelper::FindElementById(
                         WebFrame(), &GetDocument(), id, &error_code));
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND),
            error_code);
}

TEST_F(CommandActionDOMHelperTest, FindElementById_NullErrorCodeOnFailures) {
  SetBodyContent("<input id='name' type='text'>");
  Element* const input = GetDocument().getElementById(AtomicString("name"));
  ASSERT_NE(nullptr, input);
  const std::string valid_id = ElementId(*input);
  ASSERT_FALSE(valid_id.empty());

  EXPECT_EQ(nullptr, CommandActionDOMHelper::FindElementById(
                         nullptr, &GetDocument(), valid_id, nullptr));
  EXPECT_EQ(nullptr, CommandActionDOMHelper::FindElementById(
                         WebFrame(), &GetDocument(), "invalid", nullptr));

  const size_t first_separator = valid_id.find('|');
  ASSERT_NE(std::string::npos, first_separator);
  EXPECT_EQ(nullptr,
            CommandActionDOMHelper::FindElementById(
                WebFrame(), &GetDocument(),
                "different-frame" + valid_id.substr(first_separator), nullptr));

  std::string missing_node_id = valid_id;
  missing_node_id.replace(missing_node_id.rfind('|') + 1, std::string::npos,
                          "2147483647");
  EXPECT_EQ(nullptr, CommandActionDOMHelper::FindElementById(
                         WebFrame(), &GetDocument(), missing_node_id, nullptr));
}

TEST_F(CommandActionDOMHelperWithoutEmbeddingTokenTest,
       FindElementById_WithoutEmbeddingToken) {
  SetBodyContent("<input id='name' type='text'>");
  Element* const input = GetDocument().getElementById(AtomicString("name"));
  ASSERT_NE(nullptr, input);
  ASSERT_FALSE(WebFrame()->GetEmbeddingToken().has_value());
  const FrameToken& token = WebFrame()->GetFrameToken();
  ASSERT_TRUE(token.Is<LocalFrameToken>());
  const std::string id = token.GetAs<LocalFrameToken>().ToString() +
                         "|document-scope|" +
                         std::to_string(input->GetDomNodeId());

  int32_t error_code = static_cast<int32_t>(WebCommandErrorCode::FAILED);
  EXPECT_EQ(nullptr, CommandActionDOMHelper::FindElementById(
                         WebFrame(), &GetDocument(), id, &error_code));
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND),
            error_code);
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

TEST_F(CommandActionDOMHelperTest, ExecuteInput_EmptySanitizedValue) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");

  EXPECT_EQ(
      static_cast<int32_t>(WebCommandErrorCode::INPUT_VALUE_FORMAT_INVALID),
      CommandActionDOMHelper::ExecuteInput(
          &GetDocument(), "//input[@id='name']", "", "input-text"));
}

// ===== ExecuteAIPageInput Tests =====

TEST_F(CommandActionDOMHelperTest,
       ExecuteAIPageInputByXPath_DefaultTypeSuccess) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><input id='name' type='text'></body>");
  UpdateAllLifecyclePhasesForTest();

  int32_t result = CommandActionDOMHelper::ExecuteAIPageInputByXPath(
      &GetDocument(), "//input[@id='name']", "hello-default", "default");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));

  auto* input =
      To<HTMLInputElement>(GetDocument().getElementById(AtomicString("name")));
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

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputByXPath_ElementNotFound) {
  SetBodyContent("<div>no input</div>");
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND),
            CommandActionDOMHelper::ExecuteAIPageInputByXPath(
                &GetDocument(), "//input", "hello", "default"));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputByXPath_NotInputElement) {
  SetBodyContent("<div id='target'>not input</div>");
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH),
            CommandActionDOMHelper::ExecuteAIPageInputByXPath(
                &GetDocument(), "//div[@id='target']", "hello", "default"));
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteAIPageInputByXPath_ActualInputTypeUnsupported) {
  SetBodyContent("<input id='target' type='checkbox'>");
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::INPUT_TYPE_INVALID),
            CommandActionDOMHelper::ExecuteAIPageInputByXPath(
                &GetDocument(), "//input[@id='target']", "true", "default"));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputByXPath_TypeMismatch) {
  SetBodyContent("<input id='target' type='email'>");
  EXPECT_EQ(
      static_cast<int32_t>(WebCommandErrorCode::INPUT_EVENT_TYPE_MISMATCH),
      CommandActionDOMHelper::ExecuteAIPageInputByXPath(
          &GetDocument(), "//input[@id='target']", "user@example.com", "text"));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputByXPath_InvalidValue) {
  SetBodyContent("<input id='target' type='number'>");
  EXPECT_EQ(
      static_cast<int32_t>(WebCommandErrorCode::INPUT_VALUE_FORMAT_INVALID),
      CommandActionDOMHelper::ExecuteAIPageInputByXPath(
          &GetDocument(), "//input[@id='target']", "not-a-number", "number"));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAIPageInputById_Success) {
  SetBodyContent("<input id='target' type='email'>");
  auto* const input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_NE(nullptr, input);

  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SUCCESS),
            CommandActionDOMHelper::ExecuteAIPageInputById(
                WebFrame(), &GetDocument(), ElementId(*input),
                "user@example.com", "email"));
  EXPECT_EQ("user@example.com", input->Value());
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
  auto* const select = To<HTMLSelectElement>(
      GetDocument().getElementById(AtomicString("color")));
  ASSERT_NE(nullptr, select);
  EXPECT_TRUE(select->item(0)->Selected());
  EXPECT_FALSE(select->item(1)->Selected());
  EXPECT_TRUE(select->item(2)->Selected());
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
  auto* const select = To<HTMLSelectElement>(
      GetDocument().getElementById(AtomicString("color")));
  ASSERT_NE(nullptr, select);
  EXPECT_TRUE(select->item(0)->Selected());
  EXPECT_FALSE(select->item(1)->Selected());
  EXPECT_TRUE(select->item(2)->Selected());
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteSelect_MultipleSelect_IndexOutOfRange) {
  SetBodyContent(
      "<select id='color' multiple>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "</select>");
  EXPECT_EQ(
      static_cast<int32_t>(WebCommandErrorCode::SELECT_INDEX_OUT_OF_RANGE),
      CommandActionDOMHelper::ExecuteSelect(
          &GetDocument(), "//select[@id='color']", {0, 2}, {}));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_MultipleSelect_DisabledIndex) {
  SetBodyContent(
      "<select id='color' multiple>"
      "<option value='red'>Red</option>"
      "<option value='blue' disabled>Blue</option>"
      "</select>");
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED),
            CommandActionDOMHelper::ExecuteSelect(
                &GetDocument(), "//select[@id='color']", {0, 1}, {}));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_MultipleSelect_ValueNotFound) {
  SetBodyContent(
      "<select id='color' multiple>"
      "<option value='red'>Red</option>"
      "</select>");
  EXPECT_EQ(
      static_cast<int32_t>(WebCommandErrorCode::SELECT_VALUE_NOT_FOUND),
      CommandActionDOMHelper::ExecuteSelect(
          &GetDocument(), "//select[@id='color']", {}, {"red", "missing"}));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelect_MultipleSelect_DisabledValue) {
  SetBodyContent(
      "<select id='color' multiple>"
      "<option value='red'>Red</option>"
      "<option value='blue' disabled>Blue</option>"
      "</select>");
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SELECT_OPTION_DISABLED),
            CommandActionDOMHelper::ExecuteSelect(
                &GetDocument(), "//select[@id='color']", {}, {"red", "blue"}));
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteSelect_MultipleSelect_DuplicateValueUsesFirstOption) {
  SetBodyContent(
      "<select id='color' multiple>"
      "<option value='red'>First red</option>"
      "<option value='red'>Second red</option>"
      "</select>");
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SUCCESS),
            CommandActionDOMHelper::ExecuteSelect(
                &GetDocument(), "//select[@id='color']", {}, {"red"}));
  auto* const select = To<HTMLSelectElement>(
      GetDocument().getElementById(AtomicString("color")));
  ASSERT_NE(nullptr, select);
  EXPECT_TRUE(select->item(0)->Selected());
  EXPECT_FALSE(select->item(1)->Selected());
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelectById_PageNotReady) {
  GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
      "<body><select id='color'><option "
      "value='red'>Red</option></select></body>");
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

TEST_F(CommandActionDOMHelperTest, ExecuteSelectById_SuccessAndEmptyOptions) {
  SetBodyContent(
      "<select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "</select>");
  auto* const select = To<HTMLSelectElement>(
      GetDocument().getElementById(AtomicString("color")));
  ASSERT_NE(nullptr, select);
  const std::string id = ElementId(*select);

  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SUCCESS),
            CommandActionDOMHelper::ExecuteSelectById(
                WebFrame(), &GetDocument(), id, {}, {"blue"}));
  EXPECT_EQ(1, select->selectedIndex());
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SELECT_EMPTY_OPTIONS),
            CommandActionDOMHelper::ExecuteSelectById(
                WebFrame(), &GetDocument(), id, {}, {}));
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelectById_IndexSuccess) {
  SetBodyContent(
      "<select id='color'>"
      "<option value='red'>Red</option>"
      "<option value='blue'>Blue</option>"
      "</select>");
  auto* const select = To<HTMLSelectElement>(
      GetDocument().getElementById(AtomicString("color")));
  ASSERT_NE(nullptr, select);

  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::SUCCESS),
            CommandActionDOMHelper::ExecuteSelectById(
                WebFrame(), &GetDocument(), ElementId(*select), {1}, {}));
  EXPECT_EQ(1, select->selectedIndex());
}

TEST_F(CommandActionDOMHelperTest, ExecuteSelectById_ElementTypeMismatch) {
  SetBodyContent("<input id='target' type='text'>");
  Element* const input = GetDocument().getElementById(AtomicString("target"));
  ASSERT_NE(nullptr, input);
  EXPECT_EQ(static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH),
            CommandActionDOMHelper::ExecuteSelectById(
                WebFrame(), &GetDocument(), ElementId(*input), {0}, {}));
}

// ===== ExecuteAutoFill Tests =====

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_ElementNotFound) {
  SetBodyContent("<div>no inputs</div>");
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//input[@id='missing']", 0, "hello", 0, 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_FOUND));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_NodeIdNotFound_XPathFallback) {
  SetBodyContent("<input id='target' type='text'>");
  auto* const input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_NE(nullptr, input);
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//input[@id='target']", 2147483647, "hello", 0, 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ("hello", input->Value());
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_NotInputOrTextarea) {
  SetBodyContent("<div id='target'>text</div>");
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//div[@id='target']", 0, "hello", 0, 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_DisabledInput) {
  SetBodyContent("<input id='target' type='text' disabled>");
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//input[@id='target']", 0, "hello", 0, 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_INTERACTABLE));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_DisabledTextarea) {
  SetBodyContent("<textarea id='target' disabled></textarea>");
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//textarea[@id='target']", 0, "hello", 0, 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_INTERACTABLE));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_ReadonlyInput) {
  SetBodyContent("<input id='target' type='text' readonly>");
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//input[@id='target']", 0, "hello", 0, 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_INTERACTABLE));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_ReadonlyTextarea) {
  SetBodyContent("<textarea id='target' readonly></textarea>");
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//textarea[@id='target']", 0, "hello", 0, 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_NOT_INTERACTABLE));
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_InsertInput) {
  SetBodyContent("<input id='target' type='text' value='world'>");
  auto* const input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_NE(nullptr, input);
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//input[@id='target']", 0, "hello ", 0, 1);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ("hello world", input->Value());
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_InsertTextarea) {
  SetBodyContent("<textarea id='target'>abc</textarea>");
  auto* const textarea = To<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_NE(nullptr, textarea);
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//textarea[@id='target']", 0, "XYZ", 1, 1);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ("aXYZbc", textarea->Value());
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_OverwriteInput) {
  SetBodyContent("<input id='target' type='text' value='old'>");
  auto* const input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_NE(nullptr, input);
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//input[@id='target']", 0, "new", 0, 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ("new", input->Value());
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_OverwriteTextarea) {
  SetBodyContent("<textarea id='target'>old</textarea>");
  auto* const textarea = To<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_NE(nullptr, textarea);
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "//textarea[@id='target']", 0, "new", 0, 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ("new", textarea->Value());
}

TEST_F(CommandActionDOMHelperTest, ExecuteAutoFill_ResolvedByNodeId) {
  SetBodyContent("<input id='target' type='text' value='old'>");
  auto* const input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("target")));
  ASSERT_NE(nullptr, input);
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteAutoFill(
      &GetDocument(), "", input->GetDomNodeId(), "new", 0, 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ("new", input->Value());
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteInputSetCursor_ContentEditableWithBr) {
  SetBodyContent("<div id='editor' contenteditable='true'>ab<br>cd</div>");
  auto* const el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(nullptr, el);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                  mojom::blink::FocusType::kNone,
                                                  nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 3);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteInputSetCursor_ContentEditableBrFirstNode) {
  SetBodyContent("<div id='editor' contenteditable='true'><br>cd</div>");
  auto* const el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(nullptr, el);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                  mojom::blink::FocusType::kNone,
                                                  nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteInputSetCursor_ContentEditableOffsetExceeds) {
  SetBodyContent("<div id='editor' contenteditable='true'>hello</div>");
  auto* const el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(nullptr, el);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                  mojom::blink::FocusType::kNone,
                                                  nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 100);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest, ExecuteInputInsert_TextareaReadonly) {
  SetBodyContent("<textarea id='desc' readonly>abc</textarea>");
  auto* const textarea = To<HTMLTextAreaElement>(
      GetDocument().getElementById(AtomicString("desc")));
  ASSERT_NE(nullptr, textarea);
  GetDocument().SetFocusedElement(textarea, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                        mojom::blink::FocusType::kNone,
                                                        nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), "x", 0);
  EXPECT_EQ(result,
            static_cast<int32_t>(WebCommandErrorCode::ELEMENT_TYPE_MISMATCH));
}

TEST_F(CommandActionDOMHelperTest, ExecuteScroll_ScrollableAncestor) {
  SetBodyContent(
      "<div id='scroller' style='overflow:scroll;height:50px'>"
      "<div style='height:1000px'><div id='target'>content</div></div>"
      "</div>");
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteScroll(
      &GetDocument(), "//div[@id='target']", "top", 0);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteInputModify_ContentEditablePlaintextOnly) {
  SetBodyContent("<div id='editor' contenteditable='plaintext-only'></div>");
  auto* const el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(nullptr, el);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                  mojom::blink::FocusType::kNone,
                                                  nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputModify(
      &GetDocument(), "new content");
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
  EXPECT_EQ("new content", el->innerText());
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteInputInsert_ContentEditablePlaintextOnly) {
  SetBodyContent("<div id='editor' contenteditable='plaintext-only'>hello</div>");
  auto* const el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(nullptr, el);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                  mojom::blink::FocusType::kNone,
                                                  nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputInsert(
      &GetDocument(), " world", 5);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteInputSelect_ContentEditablePlaintextOnly) {
  SetBodyContent("<div id='editor' contenteditable='plaintext-only'>hello</div>");
  auto* const el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(nullptr, el);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                  mojom::blink::FocusType::kNone,
                                                  nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputSelect(
      &GetDocument(), 0, 3);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

TEST_F(CommandActionDOMHelperTest,
       ExecuteInputSetCursor_ContentEditablePlaintextOnly) {
  SetBodyContent("<div id='editor' contenteditable='plaintext-only'>hello</div>");
  auto* const el = GetDocument().getElementById(AtomicString("editor"));
  ASSERT_NE(nullptr, el);
  GetDocument().SetFocusedElement(el, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                  mojom::blink::FocusType::kNone,
                                                  nullptr));
  UpdateAllLifecyclePhasesForTest();
  int32_t result = CommandActionDOMHelper::ExecuteInputSetCursor(
      &GetDocument(), 3);
  EXPECT_EQ(result, static_cast<int32_t>(WebCommandErrorCode::SUCCESS));
}

}  // namespace blink

#endif  // BUILDFLAG(ARKWEB_INPUT_EVENTS)
