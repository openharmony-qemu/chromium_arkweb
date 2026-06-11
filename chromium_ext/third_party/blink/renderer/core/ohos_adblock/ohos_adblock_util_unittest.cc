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
#include "arkweb/chromium_ext/third_party/blink/renderer/core/ohos_adblock/ohos_adblock_util.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class OhosAdblockUtilTest : public EditingTestBase {};

TEST_F(OhosAdblockUtilTest, IsValidTarget_001) {
  const char* body_content =
      "<a id=one href='http://www.msn.com'>one</a><b id=two>two</b>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));
  Element* target = To<Element>(one);
  EXPECT_NE(target, nullptr);
  int top = target->OffsetTop();
  int left = target->OffsetLeft();
  int height = target->OffsetHeight();
  int width = target->OffsetWidth();
  EXPECT_FALSE(one->CanStartSelection());
  EXPECT_FALSE(one->firstChild()->CanStartSelection());
  EXPECT_TRUE(two->CanStartSelection());
  EXPECT_TRUE(two->firstChild()->CanStartSelection());
  auto result = IsValidTarget(one, top, left, width, height);
  EXPECT_FALSE(result);
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_002) {
  const char* body_content = "<div id=host><span id=one>one</span></div>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Element* target = To<Element>(one);
  EXPECT_NE(target, nullptr);
  int top = target->OffsetTop();
  int left = target->OffsetLeft();
  int height = target->OffsetHeight();
  int width = target->OffsetWidth();
  auto result = IsValidTarget(one, top, left, width, height);
  EXPECT_TRUE(result);
  result = IsValidTarget(one, top + 1, left, width, height);
  EXPECT_FALSE(result);
  result = IsValidTarget(one, top, left + 1, width, height);
  EXPECT_FALSE(result);
  result = IsValidTarget(one, top, left, width - 1, height);
  EXPECT_FALSE(result);
  result = IsValidTarget(one, top, left, width, height - 1);
  EXPECT_FALSE(result);
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_003) {
  const char* body_content =
      "<div id=host><span id=one>one</span><span id=two>two</span></div>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Element* target = To<Element>(one);
  EXPECT_NE(target, nullptr);
  int top = target->OffsetTop();
  int left = target->OffsetLeft();
  int height = target->OffsetHeight();
  int width = target->OffsetWidth();
  auto result = IsValidTarget(one, top, left, width, height);
  EXPECT_FALSE(result);
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_004) {
  const char* body_content = "<div id=host><span id=one>one</span></div>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Element* target = To<Element>(one);
  EXPECT_NE(target, nullptr);
  int top = target->OffsetTop();
  int left = target->OffsetLeft();
  int height = target->OffsetHeight();
  int width = target->OffsetWidth();
  auto result = IsValidTarget(nullptr, top, left, width, height);
  EXPECT_FALSE(result);
  result = IsValidTarget(one, top, left, width, height);
  EXPECT_TRUE(result);
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_005) {
  SetBodyContent(R"HTML(<body><span id="id"></span></body>)HTML");
  Node* one = GetDocument().getElementById(AtomicString("id"));
  auto result = IsValidTarget(one, 0, 0, 0, 0);
  EXPECT_FALSE(result);
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_006) {
  SetBodyContent(
    "<div id='host'><span id='one'></span><span id='two' style='display:block;width:0px;height:10px;'></span></div>");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));
  auto result = IsValidTarget(one, 0, 0, 10, 10);
  EXPECT_TRUE(result);
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_007) {
  SetBodyContent(
    "<div id='host'><span id='one'></span><span id='two' style='display:block;width:10px;height:0px;'></span></div>");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));
  auto result = IsValidTarget(one, 0, 0, 10, 10);
  EXPECT_TRUE(result);
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_008) {
  SetBodyContent("<div>hello</div>");
  Node* html_node = GetDocument().documentElement();
  ASSERT_NE(html_node, nullptr);
  EXPECT_EQ(html_node->parentNode(), &GetDocument());
  Element* html_element = To<Element>(html_node);
  int top = html_element->OffsetTop();
  int left = html_element->OffsetLeft();
  int width = html_element->OffsetWidth();
  int height = html_element->OffsetHeight();
  bool result = IsValidTarget(html_node, top, left, width, height);
  EXPECT_FALSE(result);
}

TEST_F(OhosAdblockUtilTest, PrefixedElementClassNames_001) {
  const char* body_content = "<div id=host><span id=one>one</span></div>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  PrefixedElementClassNames(*one, true);
  PrefixedElementClassNames(*one, false);
}

TEST_F(OhosAdblockUtilTest, PrefixedElementClassNames_002) {
  const char* body_content = "<div id=host><span id=one>one</span></div>";
  SetBodyContent(body_content);
  Node* one = GetDocument().getElementById(AtomicString("one"));
  PrefixedElementClassNames(*one, false);
}

TEST_F(OhosAdblockUtilTest, PrefixedElementClassNames_003) {
  SetBodyContent("<div id=\"root\"><b id=two></b><b id=t>t</b></div>");
  Element* host = GetDocument().firstElementChild();
  PrefixedElementClassNames(*host, false);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_001) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <body>
      <template shadowrootmode="open">
        <slot id="slot1">
          <span id="fallback1"></span>
        </slot>
        <slot id="slot2">
          <span id="fallback2"></span>
        </slot>
      </template>
      <div id="slotted">one</div>
      <div id="not_slotted" slot="notfound"></div>
    </body>
  )HTML");
  Element* host = GetDocument().firstElementChild();
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  ExceptionState exception_state(isolate);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<div><slot name=x></slot></div>", exception_state);
  UpdateAllLifecyclePhasesForTest();
  GetDomPathStep(*host, true, true, true);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_002) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <input type="text">"hello"</input>
  )HTML");
  Element* host = GetDocument().firstElementChild();
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  ExceptionState exception_state(isolate);
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<div><slot name=x></slot></div>", exception_state);
  UpdateAllLifecyclePhasesForTest();
  GetDomPathStep(*host, false, true, true);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_003) {
  SetBodyContent("<div id=\"root\"><b id=two></b><b id=t>t</b></div>");
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* host = root->firstElementChild();
  host->CreateUserAgentShadowRoot();
  GetDomPathStep(*host, false, true, true);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_004) {
  SetBodyContent("Text");
  Node* node = GetDocument().body()->firstChild();
  GetDomPathStep(*node, false, true, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_005) {
  SetBodyContent("<input id=\"text\">\"hello\"</input>");
  Node* node = GetDocument().body()->firstChild();
  Element* root = To<Element>(node);
  root->CreateUserAgentShadowRoot();
  GetDomPathStep(*root, false, true, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_006) {
  SetBodyContent("<input id=\"text\">\"hello\"</input>");
  Node* node = GetDocument().body()->firstChild();
  Element* root = To<Element>(node);
  root->CreateUserAgentShadowRoot();
  GetDomPathStep(*root, true, true, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_060) {
  SetBodyContent(
      "<html><head>o</head><body><b id=\"root\">one</b></body></html>");
  Node* node = GetDocument().getElementById(AtomicString("root"));
  Node* body = node->parentNode();
  Node* html = body->parentNode();
  GetDomPathStep(*html, true, true, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_061) {
  SetBodyContent("<html><body id=\"root\">\"hello\"</body></html>");
  Node* node = GetDocument().body()->firstChild();
  Node* root = node->parentNode();
  Node* root1 = root->parentNode();
  GetDomPathStep(*root, true, true, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_062) {
  SetBodyContent(
      "<html><head>o</head><body><b id=\"root\">one</b></body></html>");
  Node* node = GetDocument().getElementById(AtomicString("root"));
  Node* up = node->parentNode();
  Node* root = up->parentNode();
  Node* child = root->firstChild();
  GetDomPathStep(*child, true, true, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_007) {
  SetBodyContent(
      "<html><head>o</head><body><b id=\"root\">one</b></body></html>");
  Node* node = GetDocument().getElementById(AtomicString("root"));
  Node* up = node->parentNode();
  Node* html = up->parentNode();
  GetDomPathStep(*html, false, true, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_008) {
  SetBodyContent(
      "<div id=\"root\"><b id=\"one\"></b><b id=\"two\"></b></div>");
  Element* root = GetDocument().getElementById(AtomicString("one"));
  root->CreateUserAgentShadowRoot();
  Element* root1 = GetDocument().getElementById(AtomicString("two"));
  root1->CreateUserAgentShadowRoot();
  GetDomPathStep(*root, false, true, true);
  EXPECT_TRUE(root->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_009) {
  SetBodyContent(
      "<div id=\"root\"><div id=\"one\"></div><div id=\"two\"></div></div>");
  Element* root = GetDocument().getElementById(AtomicString("two"));
  root->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  Element* root1 = GetDocument().getElementById(AtomicString("one"));
  root1->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  GetDomPathStep(*root, false, true, true);
  EXPECT_TRUE(root->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPath_001) {
  SetBodyContent(
      "<html><head>o</head><body><b id=\"root\">one</b></body></html>");
  Node* node = GetDocument().getElementById(AtomicString("root"));
  Node* up = node->parentNode();
  Node* html = up->parentNode();
  GetDomPath(*html, false, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPath_002) {
  SetBodyContent(
      "<html><head>o</head><body><b id=\"root\">one</b></body></html>");
  Node* node = GetDocument().body()->firstChild();
  GetDomPath(*node, false, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPath_003) {
  SetBodyContent(
      "<html><head>o</head><body><b id=\"root\">one</b></body></html>");
  Node* node = GetDocument().getElementById(AtomicString("root"));
  Node* up = node->parentNode();
  Node* html = up->parentNode();
  GetDomPath(*html, false, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPath_004) {
  SetBodyContent("<input id=\"text\">\"hello\"</input>");
  Node* node = GetDocument().body()->firstChild();
  Element* root = To<Element>(node);
  root->CreateUserAgentShadowRoot();
  GetDomPath(*root, false, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPath_005) {
  SetBodyContent("<input id=\"text\">\"hello\"</input>");
  Node* node = GetDocument().body()->firstChild();
  Element* root = To<Element>(node);
  root->CreateUserAgentShadowRoot();
  GetDomPath(*root, false, true);
  EXPECT_TRUE(node->CanStartSelection());
}

TEST_F(OhosAdblockUtilTest, GetDomPath_006) {
  SetBodyContent("<form><input type='email' value='test@'></form>");
  HTMLCollection* inputs = GetDocument().getElementsByTagName(AtomicString("input"));
  ASSERT_GT(inputs->length(), 0);
  Element* input = To<Element>(inputs->item(0));
  ASSERT_NE(input, nullptr);
  std::string result = GetDomPath(*input, false, true);
  EXPECT_EQ(result, "HTML>BODY>FORM>INPUT[type=email]");
}

TEST_F(OhosAdblockUtilTest, GetDomPath_007) {
  SetBodyContent("<div id='app'><main><section><p class='intro'>Hello</p></section></main></div>");
  Element* app = GetDocument().getElementById(AtomicString("app"));
  ASSERT_NE(app, nullptr);
  HTMLCollection* ps = app->getElementsByTagName(AtomicString("p"));
  ASSERT_GT(ps->length(), 0);
  Element* p = To<Element>(ps->item(0));
  ASSERT_NE(p, nullptr);
  std::string result = GetDomPath(*p, false, false);
  EXPECT_EQ(result, "DIV#app>MAIN>SECTION>P");
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_SiblingsNull) {
  SetBodyContent("<div id=host><span id=one>one</span></div>");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  CHECK(one);
  Element* target = To<Element>(one);
  CHECK(target);
  int top = target->OffsetTop();
  int left = target->OffsetLeft();
  int height = target->OffsetHeight();
  int width = target->OffsetWidth();
  auto result = IsValidTarget(one, top, left, width, height);
  EXPECT_TRUE(result);
}

TEST_F(OhosAdblockUtilTest, PrefixedElementClassNames_WithClasses) {
  SetBodyContent("<div id='host'><span id='one' class='foo bar baz'>one</span></div>");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  CHECK(one);
  auto result = PrefixedElementClassNames(*one, true);
  EXPECT_TRUE(result.empty());
  result = PrefixedElementClassNames(*one, false);
  EXPECT_TRUE(result.empty());
}

TEST_F(OhosAdblockUtilTest, PrefixedElementClassNames_NoClass) {
  SetBodyContent("<div id='host'><span id='one'>one</span></div>");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  CHECK(one);
  auto result = PrefixedElementClassNames(*one, false);
  EXPECT_TRUE(result.empty());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_InputWithTypeNoIdNoClass) {
  SetBodyContent("<form><input type='password'></form>");
  HTMLCollection* inputs = GetDocument().getElementsByTagName(AtomicString("input"));
  ASSERT_GT(inputs->length(), 0u);
  Element* input_elem = inputs->item(0);
  CHECK(input_elem);
  auto result = GetDomPathStep(*input_elem, false, true, true);
  EXPECT_TRUE(result.first.find("[type=password]") != std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_InputWithTypeHasId) {
  SetBodyContent("<form><input id='pw' type='password'></form>");
  HTMLCollection* inputs = GetDocument().getElementsByTagName(AtomicString("input"));
  ASSERT_GT(inputs->length(), 0u);
  Element* input_elem = inputs->item(0);
  CHECK(input_elem);
  auto result = GetDomPathStep(*input_elem, false, true, true);
  EXPECT_TRUE(result.first.find("[type=") == std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_InputWithTypeHasClass) {
  SetBodyContent("<form><input class='cls' type='password'></form>");
  HTMLCollection* inputs = GetDocument().getElementsByTagName(AtomicString("input"));
  ASSERT_GT(inputs->length(), 0u);
  Element* input_elem = inputs->item(0);
  CHECK(input_elem);
  auto result = GetDomPathStep(*input_elem, false, true, true);
  EXPECT_TRUE(result.first.find("[type=") == std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_InputNotTargetNode) {
  SetBodyContent("<form><input type='password'></form>");
  HTMLCollection* inputs = GetDocument().getElementsByTagName(AtomicString("input"));
  ASSERT_GT(inputs->length(), 0u);
  Element* input_elem = inputs->item(0);
  CHECK(input_elem);
  auto result = GetDomPathStep(*input_elem, false, false, true);
  EXPECT_TRUE(result.first.find("[type=") == std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_OptimizedNoIdNotSpecial) {
  SetBodyContent("<div id='root'><b id='one'>text</b></div>");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  CHECK(one);
  auto result = GetDomPathStep(*one, true, false, true);
  EXPECT_EQ(result.first, "#one");
  EXPECT_TRUE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_OptimizedBody) {
  SetBodyContent("<div id='root'>text</div>");
  Node* body = GetDocument().body();
  CHECK(body);
  auto result = GetDomPathStep(*body, true, false, true);
  EXPECT_TRUE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_OptimizedHead) {
  SetBodyContent("<html><head>o</head><body></body></html>");
  Node* html_node = GetDocument().documentElement();
  CHECK(html_node);
  Node* head = html_node->firstChild();
  CHECK(head);
  auto result = GetDomPathStep(*head, true, false, true);
  EXPECT_TRUE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_OptimizedHtml) {
  Node* html_node = GetDocument().documentElement();
  CHECK(html_node);
  auto result = GetDomPathStep(*html_node, true, false, true);
  EXPECT_TRUE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_NonOptimizedWithId) {
  SetBodyContent("<div id='root'><span id='myid'>text</span></div>");
  Node* span = GetDocument().getElementById(AtomicString("myid"));
  CHECK(span);
  auto result = GetDomPathStep(*span, false, false, false);
  EXPECT_TRUE(result.first.find("#myid") != std::string::npos);
  EXPECT_TRUE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_NonOptimizedWithIdPureDomPath) {
  SetBodyContent("<div id='root'><span id='myid'>text</span></div>");
  Node* span = GetDocument().getElementById(AtomicString("myid"));
  CHECK(span);
  auto result = GetDomPathStep(*span, false, false, true);
  EXPECT_TRUE(result.first.find("#") == std::string::npos);
  EXPECT_FALSE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_SameNodeNameSiblingsNeedsNthChild) {
  SetBodyContent("<div id='root'><b id='a'></b><b id='b'></b></div>");
  Node* b_a = GetDocument().getElementById(AtomicString("a"));
  Node* b_b = GetDocument().getElementById(AtomicString("b"));
  CHECK(b_a);
  CHECK(b_b);
  auto result_a = GetDomPathStep(*b_a, false, false, true);
  EXPECT_TRUE(result_a.first.find(":nth-child(") != std::string::npos);
  auto result_b = GetDomPathStep(*b_b, false, false, true);
  EXPECT_TRUE(result_b.first.find(":nth-child(") != std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_DifferentNodeNameNoNthChild) {
  SetBodyContent("<div id='root'><b id='a'></b><span id='b'></span></div>");
  Node* b_elem = GetDocument().getElementById(AtomicString("a"));
  CHECK(b_elem);
  auto result = GetDomPathStep(*b_elem, false, false, true);
  EXPECT_TRUE(result.first.find(":nth-child(") == std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_NeedsClassNames) {
  SetBodyContent("<div id='root'>"
                 "<b class='cls1 cls2'></b>"
                 "<b class='cls3'></b>"
                 "</div>");
  HTMLCollection* bs = GetDocument().getElementsByTagName(AtomicString("b"));
  ASSERT_GE(bs->length(), 2u);
  Element* first_b = bs->item(0);
  CHECK(first_b);
  auto result = GetDomPathStep(*first_b, false, false, false);
  EXPECT_TRUE(result.first.find(":nth-child(") != std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_ShadowRootOpen) {
  SetBodyContent("<div id='host'></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  CHECK(host);
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  auto result = GetDomPathStep(*host, false, false, true);
  EXPECT_FALSE(result.first.empty());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_SiblingShadowRootOpen) {
  SetBodyContent("<div id='root'><div id='a'></div><div id='b'></div></div>");
  Element* a_elem = GetDocument().getElementById(AtomicString("a"));
  Element* b_elem = GetDocument().getElementById(AtomicString("b"));
  CHECK(a_elem);
  CHECK(b_elem);
  a_elem->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  b_elem->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  auto result = GetDomPathStep(*a_elem, false, false, true);
  EXPECT_TRUE(result.first.find(":nth-child(") != std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_ParentNull) {
  SetBodyContent("<div id='root'>text</div>");
  Node* doc_elem = GetDocument().documentElement();
  CHECK(doc_elem);
  auto result = GetDomPathStep(*doc_elem, false, false, true);
  EXPECT_FALSE(result.first.empty());
  EXPECT_TRUE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_SingleChildNoNthChild) {
  SetBodyContent("<div id='root'><b id='a'></b></div>");
  Node* b_elem = GetDocument().getElementById(AtomicString("a"));
  CHECK(b_elem);
  auto result = GetDomPathStep(*b_elem, false, false, true);
  EXPECT_TRUE(result.first.find(":nth-child(") == std::string::npos);
  EXPECT_TRUE(result.first == "B");
}

TEST_F(OhosAdblockUtilTest, GetDomPath_TextNode) {
  SetBodyContent("plain text");
  Node* text_node = GetDocument().body()->firstChild();
  CHECK(text_node);
  std::string result = GetDomPath(*text_node, false, true);
  EXPECT_EQ(result, "");
}

TEST_F(OhosAdblockUtilTest, GetDomPath_OptimizedTrue) {
  SetBodyContent("<div id='root'><span id='child'>text</span></div>");
  Node* child = GetDocument().getElementById(AtomicString("child"));
  CHECK(child);
  std::string result = GetDomPath(*child, true, true);
  EXPECT_EQ(result, "#child");
}

TEST_F(OhosAdblockUtilTest, GetDomPath_WithPureDomPathFalse) {
  SetBodyContent("<div id='app'><span id='child'>text</span></div>");
  Node* child = GetDocument().getElementById(AtomicString("child"));
  CHECK(child);
  std::string result = GetDomPath(*child, false, false);
  EXPECT_TRUE(result.find("SPAN#child") != std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_OptimizedNoIdSpecialBody) {
  Node* body = GetDocument().body();
  CHECK(body);
  auto result = GetDomPathStep(*body, true, false, true);
  EXPECT_TRUE(result.second);
  EXPECT_FALSE(result.first.empty());
}

TEST_F(OhosAdblockUtilTest, GetDomPath_ElementTypeAttr) {
  SetBodyContent("<div><button type='submit'>click</button></div>");
  HTMLCollection* buttons = GetDocument().getElementsByTagName(AtomicString("button"));
  ASSERT_GT(buttons->length(), 0u);
  Element* btn = buttons->item(0);
  CHECK(btn);
  std::string result = GetDomPath(*btn, false, true);
  EXPECT_TRUE(result.find("BUTTON") != std::string::npos);
}

TEST_F(OhosAdblockUtilTest, Escape_SpecialChars) {
  std::string input = "test.id";
  std::string result = Escape(input);
  EXPECT_FALSE(result.empty());
  std::string input2 = "simple";
  result = Escape(input2);
  EXPECT_FALSE(result.empty());
}

TEST_F(OhosAdblockUtilTest, IsValidTarget_ParentIsBody) {
  SetBodyContent("<span id='one'>one</span>");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  CHECK(one);
  Element* target = To<Element>(one);
  CHECK(target);
  int top = target->OffsetTop();
  int left = target->OffsetLeft();
  int height = target->OffsetHeight();
  int width = target->OffsetWidth();
  auto result = IsValidTarget(one, top, left, width, height);
  EXPECT_FALSE(result);
}

TEST_F(OhosAdblockUtilTest, GetDomPath_WithClassesOnSiblings) {
  SetBodyContent("<div id='root'>"
                 "<b class='alpha beta'></b>"
                 "<b class='gamma'></b>"
                 "</div>");
  HTMLCollection* bs = GetDocument().getElementsByTagName(AtomicString("b"));
  ASSERT_GE(bs->length(), 2u);
  Element* first_b = bs->item(0);
  CHECK(first_b);
  std::string result = GetDomPath(*first_b, false, false);
  EXPECT_FALSE(result.empty());
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_NonElementNode) {
  SetBodyContent("hello");
  Node* text_node = GetDocument().body()->firstChild();
  CHECK(text_node);
  auto result = GetDomPathStep(*text_node, false, false, true);
  EXPECT_EQ(result.first, "");
  EXPECT_FALSE(result.second);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_SiblingsWithSameClasses) {
  SetBodyContent("<div id='root'>"
                 "<b class='shared'></b>"
                 "<b class='shared'></b>"
                 "</div>");
  HTMLCollection* bs = GetDocument().getElementsByTagName(AtomicString("b"));
  ASSERT_GE(bs->length(), 2u);
  Element* first_b = bs->item(0);
  CHECK(first_b);
  auto result = GetDomPathStep(*first_b, false, false, false);
  EXPECT_TRUE(result.first.find(":nth-child(") != std::string::npos);
}

TEST_F(OhosAdblockUtilTest, GetDomPathStep_OptimizedNoIdNoSpecialName) {
  SetBodyContent("<div id='root'><span>text</span></div>");
  HTMLCollection* spans = GetDocument().getElementsByTagName(AtomicString("span"));
  ASSERT_GT(spans->length(), 0u);
  Element* span = spans->item(0);
  CHECK(span);
  auto result = GetDomPathStep(*span, true, false, true);
  EXPECT_FALSE(result.first.empty());
  EXPECT_FALSE(result.second);
}
}  // namespace blink