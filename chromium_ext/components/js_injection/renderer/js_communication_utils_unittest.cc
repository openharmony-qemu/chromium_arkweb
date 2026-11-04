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

#include "components/js_injection/renderer/js_communication.h"
#define private public
#include "arkweb/chromium_ext/components/js_injection/renderer/js_communication_utils.cc"
#include "arkweb/chromium_ext/components/js_injection/renderer/js_communication_utils.h"
#undef private

namespace js_injection {

class JsCommunicationUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    js_communication_utils_ = std::make_unique<JsCommunicationUtils>(nullptr);
  }

  void TearDown() override { js_communication_utils_.reset(); }

  mojom::DocumentEndJavaScriptPtr CreateEndScript(
      int32_t id,
      const std::u16string& script_content) {
    auto ptr = mojom::DocumentEndJavaScript::New();
    ptr->script_id = id;
    ptr->script = script_content;
    return ptr;
  }

  mojom::DocumentStartJavaScriptPtr CreateStartScript(
      int32_t id,
      const std::u16string& script_content) {
    auto ptr = mojom::DocumentStartJavaScript::New();
    ptr->script_id = id;
    ptr->script = script_content;
    return ptr;
  }

  mojom::DocumentJavaScriptRegexRulesPtr CreateRegexRules(
      int32_t id,
      const std::u16string& script_content,
      const std::vector<std::pair<std::string, std::string>>& rules = {}) {
    auto ptr = mojom::DocumentJavaScriptRegexRules::New();
    ptr->script_id = id;
    ptr->script = script_content;
    for (const auto& rule_pair : rules) {
      auto rule = mojom::ScriptRegexRule::New();

      rule->second_level_domain = base::UTF8ToUTF16(rule_pair.first);
      rule->rule = base::UTF8ToUTF16(rule_pair.second);
      ptr->script_regex_rules.push_back(std::move(rule));
    }
    return ptr;
  }

  std::unique_ptr<JsCommunicationUtils> js_communication_utils_;
};

bool MatchUrlRegex(const std::string& url, const std::string& regex);

TEST_F(JsCommunicationUtilsTest, MatchUrlRegex_001) {
  EXPECT_FALSE(MatchUrlRegex("", "pattern"));
  EXPECT_FALSE(MatchUrlRegex("http://example.com", ""));
  EXPECT_FALSE(MatchUrlRegex("", ""));
}

TEST_F(JsCommunicationUtilsTest, MatchUrlRegex_002) {
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "*abc"));
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "+abc"));
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "?abc"));
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "{abc"));
}

TEST_F(JsCommunicationUtilsTest, MatchUrlRegex_003) {
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "(unclosed"));
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "\\"));
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "[]"));
  EXPECT_TRUE(MatchUrlRegex("http://example.com/script.js", ".*\\.js$"));
  EXPECT_FALSE(MatchUrlRegex("http://example.com/page.html", ".*\\.js$"));
}

TEST_F(JsCommunicationUtilsTest, RemoveDocumentEndScript_001) {
  auto script1 = CreateEndScript(1, u"script1");
  auto script2 = CreateEndScript(2, u"script2");
  js_communication_utils_->AddDocumentEndScript(script1);
  js_communication_utils_->AddDocumentEndScript(script2);

  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 2u);

  js_communication_utils_->RemoveDocumentEndScript(1);

  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[0]->script_id, 2);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[0]->script,
            blink::WebString::FromUTF16(u"script2"));
}

TEST_F(JsCommunicationUtilsTest, RemoveDocumentEndScript_002) {
  auto script = CreateEndScript(1, u"script");
  js_communication_utils_->AddDocumentEndScript(script);
  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 1u);

  js_communication_utils_->RemoveDocumentEndScript(99);

  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[0]->script_id, 1);
}

TEST_F(JsCommunicationUtilsTest, RemoveDocumentEndScript_003) {
  ASSERT_TRUE(js_communication_utils_->document_end_scripts_.empty());

  js_communication_utils_->RemoveDocumentEndScript(100);

  EXPECT_TRUE(js_communication_utils_->document_end_scripts_.empty());
}

TEST_F(JsCommunicationUtilsTest, RemoveHeadReadyScript_001) {
  auto script1 = CreateStartScript(1, u"script1");
  auto script2 = CreateStartScript(2, u"script2");
  js_communication_utils_->AddHeadReadyScript(script1);
  js_communication_utils_->AddHeadReadyScript(script2);

  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 2u);

  js_communication_utils_->RemoveHeadReadyScript(1);

  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[0]->script_id, 2);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[0]->script,
            blink::WebString::FromUTF16(u"script2"));
}

TEST_F(JsCommunicationUtilsTest, RemoveHeadReadyScript_002) {
  auto script = CreateStartScript(1, u"script");
  js_communication_utils_->AddHeadReadyScript(script);
  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 1u);

  js_communication_utils_->RemoveHeadReadyScript(99);

  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[0]->script_id, 1);
}

TEST_F(JsCommunicationUtilsTest, RemoveHeadReadyScript_003) {
  ASSERT_TRUE(js_communication_utils_->head_ready_scripts_.empty());

  js_communication_utils_->RemoveHeadReadyScript(100);

  EXPECT_TRUE(js_communication_utils_->head_ready_scripts_.empty());
}

TEST_F(JsCommunicationUtilsTest, CommitPendingJavascriptsAtDocumentEnd_001) {
  auto existing_script = CreateEndScript(99, u"existing");
  js_communication_utils_->AddDocumentEndScript(existing_script);
  auto existing_regex = CreateRegexRules(99, u"existing");
  auto regex_obj = js_communication_utils_->CreateDocumentJavaScriptRegexRules(
      existing_regex);
  js_communication_utils_->end_scripts_regex_rules_.push_back(
      std::move(regex_obj));

  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 1u);
  ASSERT_EQ(js_communication_utils_->end_scripts_regex_rules_.size(), 1u);
  ASSERT_TRUE(js_communication_utils_->swap_document_end_scripts_.empty());
  ASSERT_TRUE(js_communication_utils_->swap_end_scripts_regex_rules_.empty());

  js_communication_utils_->CommitPendingJavascriptsAtDocumentEnd();

  EXPECT_TRUE(js_communication_utils_->document_end_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->end_scripts_regex_rules_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_document_end_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_end_scripts_regex_rules_.empty());
}

TEST_F(JsCommunicationUtilsTest, CommitPendingJavascriptsAtDocumentEnd_002) {
  auto script = CreateEndScript(1, u"script");
  auto regex = CreateRegexRules(1, u"regex");
  js_communication_utils_->AddPendingJavascriptAtDocumentEnd(script, regex);

  ASSERT_EQ(js_communication_utils_->swap_document_end_scripts_.size(), 1u);
  ASSERT_EQ(js_communication_utils_->swap_end_scripts_regex_rules_.size(), 1u);

  js_communication_utils_->CommitPendingJavascriptsAtDocumentEnd();

  EXPECT_TRUE(js_communication_utils_->swap_document_end_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_end_scripts_regex_rules_.empty());
}

TEST_F(JsCommunicationUtilsTest, CommitPendingJavascriptsAtHeadReady_001) {
  auto existing_script = CreateStartScript(99, u"existing");
  js_communication_utils_->AddHeadReadyScript(existing_script);
  auto existing_regex = CreateRegexRules(99, u"existing");
  auto regex_obj = js_communication_utils_->CreateDocumentJavaScriptRegexRules(
      existing_regex);
  js_communication_utils_->head_ready_regex_rules_.push_back(
      std::move(regex_obj));

  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 1u);
  ASSERT_EQ(js_communication_utils_->head_ready_regex_rules_.size(), 1u);
  ASSERT_TRUE(js_communication_utils_->swap_head_ready_scripts_.empty());
  ASSERT_TRUE(js_communication_utils_->swap_head_ready_regex_rules_.empty());

  js_communication_utils_->CommitPendingJavascriptsAtHeadReady();

  EXPECT_TRUE(js_communication_utils_->head_ready_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->head_ready_regex_rules_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_head_ready_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_head_ready_regex_rules_.empty());
}

TEST_F(JsCommunicationUtilsTest, CommitPendingJavascriptsAtHeadReady_002) {
  auto script = CreateStartScript(1, u"script");
  auto regex = CreateRegexRules(1, u"regex");
  js_communication_utils_->AddPendingJavascriptAtHeadReady(script, regex);

  ASSERT_EQ(js_communication_utils_->swap_head_ready_scripts_.size(), 1u);
  ASSERT_EQ(js_communication_utils_->swap_head_ready_regex_rules_.size(), 1u);

  js_communication_utils_->CommitPendingJavascriptsAtHeadReady();

  EXPECT_TRUE(js_communication_utils_->swap_head_ready_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_head_ready_regex_rules_.empty());
}

TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtDocumentStartRegexRules_001) {
  auto existing_regex = CreateRegexRules(99, u"existing", {{"old.com", ".*"}});
  auto regex_obj = js_communication_utils_->CreateDocumentJavaScriptRegexRules(
      existing_regex);
  js_communication_utils_->start_scripts_regex_rules_.push_back(
      std::move(regex_obj));

  ASSERT_EQ(js_communication_utils_->start_scripts_regex_rules_.size(), 1u);
  ASSERT_TRUE(js_communication_utils_->swap_start_scripts_regex_rules_.empty());

  js_communication_utils_->CommitPendingJavascriptsAtDocumentStartRegexRules();

  EXPECT_TRUE(js_communication_utils_->start_scripts_regex_rules_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_start_scripts_regex_rules_.empty());
}

TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtDocumentStartRegexRules_002) {
  auto regex = CreateRegexRules(1, u"regex");
  js_communication_utils_->AddPendingJavascriptAtDocumentStartRegexRules(regex);

  ASSERT_EQ(js_communication_utils_->swap_start_scripts_regex_rules_.size(),
            1u);

  js_communication_utils_->CommitPendingJavascriptsAtDocumentStartRegexRules();

  EXPECT_TRUE(js_communication_utils_->swap_start_scripts_regex_rules_.empty());
}

// Test CreateDocumentJavaScriptRegexRules directly
TEST_F(JsCommunicationUtilsTest, CreateDocumentJavaScriptRegexRules_Basic) {
  auto rules_ptr = CreateRegexRules(
      42, u"test_script",
      {{"example.com", ".*\\.js$"}, {"test.com", ".*\\.html$"}});
  auto result =
      js_communication_utils_->CreateDocumentJavaScriptRegexRules(rules_ptr);

  ASSERT_TRUE(result);
  EXPECT_EQ(result->script_id, 42);
  EXPECT_EQ(result->script, blink::WebString::FromUTF16(u"test_script"));
  ASSERT_EQ(result->script_regex_rules.size(), 2u);
  EXPECT_EQ(result->script_regex_rules[0].second_level_domain, "example.com");
  EXPECT_EQ(result->script_regex_rules[0].rule, ".*\\.js$");
  EXPECT_EQ(result->script_regex_rules[1].second_level_domain, "test.com");
  EXPECT_EQ(result->script_regex_rules[1].rule, ".*\\.html$");
}

TEST_F(JsCommunicationUtilsTest, CreateDocumentJavaScriptRegexRules_Empty) {
  auto rules_ptr = CreateRegexRules(1, u"empty_script");
  auto result =
      js_communication_utils_->CreateDocumentJavaScriptRegexRules(rules_ptr);

  ASSERT_TRUE(result);
  EXPECT_EQ(result->script_id, 1);
  EXPECT_EQ(result->script, blink::WebString::FromUTF16(u"empty_script"));
  EXPECT_EQ(result->script_regex_rules.size(), 0u);
}

// Test AddDocumentEndScriptRegexRules directly
TEST_F(JsCommunicationUtilsTest, AddDocumentEndScriptRegexRules) {
  auto regex = CreateRegexRules(1, u"script1", {{"host1", "rule1"}});
  js_communication_utils_->AddDocumentEndScriptRegexRules(regex);

  ASSERT_EQ(js_communication_utils_->end_scripts_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->end_scripts_regex_rules_[0]->script_id, 1);
}

// Test AddHeadReadyScriptRegexRules directly
TEST_F(JsCommunicationUtilsTest, AddHeadReadyScriptRegexRules) {
  auto regex = CreateRegexRules(2, u"script2", {{"host2", "rule2"}});
  js_communication_utils_->AddHeadReadyScriptRegexRules(regex);

  ASSERT_EQ(js_communication_utils_->head_ready_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->head_ready_regex_rules_[0]->script_id, 2);
}

// Test AddDocumentStartScriptRegexRules directly
TEST_F(JsCommunicationUtilsTest, AddDocumentStartScriptRegexRules) {
  auto regex = CreateRegexRules(3, u"script3", {{"host3", "rule3"}});
  js_communication_utils_->AddDocumentStartScriptRegexRules(regex);

  ASSERT_EQ(js_communication_utils_->start_scripts_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->start_scripts_regex_rules_[0]->script_id,
            3);
}

// Test CommitPendingJavascriptsAtDocumentEnd with actual data movement
TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtDocumentEnd_DataMove) {
  auto script = CreateEndScript(1, u"pending_script");
  auto regex = CreateRegexRules(1, u"pending_regex", {{"host.com", ".*"}});
  js_communication_utils_->AddPendingJavascriptAtDocumentEnd(script, regex);

  ASSERT_EQ(js_communication_utils_->swap_document_end_scripts_.size(), 1u);
  ASSERT_EQ(js_communication_utils_->swap_end_scripts_regex_rules_.size(), 1u);
  ASSERT_TRUE(js_communication_utils_->document_end_scripts_.empty());
  ASSERT_TRUE(js_communication_utils_->end_scripts_regex_rules_.empty());

  js_communication_utils_->CommitPendingJavascriptsAtDocumentEnd();

  EXPECT_EQ(js_communication_utils_->document_end_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->end_scripts_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[0]->script_id, 1);
  EXPECT_EQ(js_communication_utils_->end_scripts_regex_rules_[0]->script_id, 1);
  EXPECT_TRUE(js_communication_utils_->swap_document_end_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_end_scripts_regex_rules_.empty());
}

// Test CommitPendingJavascriptsAtHeadReady with actual data movement
TEST_F(JsCommunicationUtilsTest, CommitPendingJavascriptsAtHeadReady_DataMove) {
  auto script = CreateStartScript(2, u"pending_head_script");
  auto regex =
      CreateRegexRules(2, u"pending_head_regex", {{"host2.com", ".*"}});
  js_communication_utils_->AddPendingJavascriptAtHeadReady(script, regex);

  ASSERT_EQ(js_communication_utils_->swap_head_ready_scripts_.size(), 1u);
  ASSERT_EQ(js_communication_utils_->swap_head_ready_regex_rules_.size(), 1u);

  js_communication_utils_->CommitPendingJavascriptsAtHeadReady();

  EXPECT_EQ(js_communication_utils_->head_ready_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->head_ready_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[0]->script_id, 2);
  EXPECT_EQ(js_communication_utils_->head_ready_regex_rules_[0]->script_id, 2);
  EXPECT_TRUE(js_communication_utils_->swap_head_ready_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_head_ready_regex_rules_.empty());
}

// Test CommitPendingJavascriptsAtDocumentStartRegexRules with data move
TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtDocumentStartRegexRules_DataMove) {
  auto regex = CreateRegexRules(3, u"start_regex", {{"host3.com", ".*\\.js$"}});
  js_communication_utils_->AddPendingJavascriptAtDocumentStartRegexRules(regex);

  ASSERT_EQ(js_communication_utils_->swap_start_scripts_regex_rules_.size(),
            1u);
  ASSERT_TRUE(js_communication_utils_->start_scripts_regex_rules_.empty());

  js_communication_utils_->CommitPendingJavascriptsAtDocumentStartRegexRules();

  EXPECT_EQ(js_communication_utils_->start_scripts_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->start_scripts_regex_rules_[0]->script_id,
            3);
  EXPECT_TRUE(js_communication_utils_->swap_start_scripts_regex_rules_.empty());
}

// Test MatchUrlRegex with more patterns
TEST_F(JsCommunicationUtilsTest, MatchUrlRegex_MatchSuccess) {
  EXPECT_TRUE(
      MatchUrlRegex("http://example.com/path.js", "example\\.com.*\\.js$"));
  EXPECT_TRUE(MatchUrlRegex("https://test.org/page", "test\\.org"));
}

TEST_F(JsCommunicationUtilsTest, MatchUrlRegex_PartialMatch) {
  EXPECT_TRUE(MatchUrlRegex("http://example.com/path", "example"));
  EXPECT_FALSE(MatchUrlRegex("http://other.com/path", "^example$"));
}

// Test CommitPendingJavascriptsAtDocumentEnd replaces existing scripts
TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtDocumentEnd_ReplacesExisting) {
  auto existing_script = CreateEndScript(99, u"old_script");
  js_communication_utils_->AddDocumentEndScript(existing_script);
  auto existing_regex = CreateRegexRules(99, u"old_regex");
  auto regex_obj = js_communication_utils_->CreateDocumentJavaScriptRegexRules(
      existing_regex);
  js_communication_utils_->end_scripts_regex_rules_.push_back(
      std::move(regex_obj));

  auto new_script = CreateEndScript(1, u"new_script");
  auto new_regex = CreateRegexRules(1, u"new_regex");
  js_communication_utils_->AddPendingJavascriptAtDocumentEnd(new_script,
                                                             new_regex);

  js_communication_utils_->CommitPendingJavascriptsAtDocumentEnd();

  EXPECT_EQ(js_communication_utils_->document_end_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[0]->script_id, 1);
  EXPECT_EQ(js_communication_utils_->end_scripts_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->end_scripts_regex_rules_[0]->script_id, 1);
}

// Test CommitPendingJavascriptsAtHeadReady replaces existing scripts
TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtHeadReady_ReplacesExisting) {
  auto existing_script = CreateStartScript(99, u"old_head_script");
  js_communication_utils_->AddHeadReadyScript(existing_script);
  auto existing_regex = CreateRegexRules(99, u"old_head_regex");
  auto regex_obj = js_communication_utils_->CreateDocumentJavaScriptRegexRules(
      existing_regex);
  js_communication_utils_->head_ready_regex_rules_.push_back(
      std::move(regex_obj));

  auto new_script = CreateStartScript(1, u"new_head_script");
  auto new_regex = CreateRegexRules(1, u"new_head_regex");
  js_communication_utils_->AddPendingJavascriptAtHeadReady(new_script,
                                                           new_regex);

  js_communication_utils_->CommitPendingJavascriptsAtHeadReady();

  EXPECT_EQ(js_communication_utils_->head_ready_scripts_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[0]->script_id, 1);
  EXPECT_EQ(js_communication_utils_->head_ready_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->head_ready_regex_rules_[0]->script_id, 1);
}

// Test CommitPendingJavascriptsAtDocumentStartRegexRules replaces existing
TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtDocumentStartRegexRules_ReplacesExisting) {
  auto existing_regex = CreateRegexRules(99, u"old_start", {{"old.com", ".*"}});
  auto old_obj = js_communication_utils_->CreateDocumentJavaScriptRegexRules(
      existing_regex);
  js_communication_utils_->start_scripts_regex_rules_.push_back(
      std::move(old_obj));

  auto new_regex = CreateRegexRules(1, u"new_start", {{"new.com", ".*\\.js$"}});
  js_communication_utils_->AddPendingJavascriptAtDocumentStartRegexRules(
      new_regex);

  js_communication_utils_->CommitPendingJavascriptsAtDocumentStartRegexRules();

  EXPECT_EQ(js_communication_utils_->start_scripts_regex_rules_.size(), 1u);
  EXPECT_EQ(js_communication_utils_->start_scripts_regex_rules_[0]->script_id,
            1);
}

// Test AddDocumentEndScript with multiple scripts
TEST_F(JsCommunicationUtilsTest, AddDocumentEndScript_Multiple) {
  auto script1 = CreateEndScript(1, u"script1");
  auto script2 = CreateEndScript(2, u"script2");
  auto script3 = CreateEndScript(3, u"script3");
  js_communication_utils_->AddDocumentEndScript(script1);
  js_communication_utils_->AddDocumentEndScript(script2);
  js_communication_utils_->AddDocumentEndScript(script3);

  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 3u);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[0]->script_id, 1);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[1]->script_id, 2);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[2]->script_id, 3);
}

// Test AddHeadReadyScript with multiple scripts
TEST_F(JsCommunicationUtilsTest, AddHeadReadyScript_Multiple) {
  auto script1 = CreateStartScript(1, u"head1");
  auto script2 = CreateStartScript(2, u"head2");
  js_communication_utils_->AddHeadReadyScript(script1);
  js_communication_utils_->AddHeadReadyScript(script2);

  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 2u);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[0]->script_id, 1);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[1]->script_id, 2);
}

// Test MatchUrlRegex with valid regex patterns
TEST_F(JsCommunicationUtilsTest, MatchUrlRegex_ValidPatterns) {
  EXPECT_TRUE(
      MatchUrlRegex("http://example.com/script.js", "example\\.com/.*\\.js$"));
  EXPECT_TRUE(MatchUrlRegex("https://test.org/page.html", "test\\.org"));
  EXPECT_TRUE(
      MatchUrlRegex("http://sub.example.com/path", "sub\\.example\\.com"));
  EXPECT_FALSE(MatchUrlRegex("http://other.com/path", "^example$"));
}

// Test MatchUrlRegex with edge cases
TEST_F(JsCommunicationUtilsTest, MatchUrlRegex_EdgeCases) {
  // Single character regex
  EXPECT_TRUE(MatchUrlRegex("http://example.com", "e"));
  // Regex that matches the entire string
  EXPECT_TRUE(MatchUrlRegex("abc", "abc"));
  // Regex with special chars
  EXPECT_FALSE(MatchUrlRegex("http://example.com", "["));
}

// Test RemoveDocumentEndScript removes middle element
TEST_F(JsCommunicationUtilsTest, RemoveDocumentEndScript_Middle) {
  auto script1 = CreateEndScript(1, u"script1");
  auto script2 = CreateEndScript(2, u"script2");
  auto script3 = CreateEndScript(3, u"script3");
  js_communication_utils_->AddDocumentEndScript(script1);
  js_communication_utils_->AddDocumentEndScript(script2);
  js_communication_utils_->AddDocumentEndScript(script3);

  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 3u);

  js_communication_utils_->RemoveDocumentEndScript(2);

  ASSERT_EQ(js_communication_utils_->document_end_scripts_.size(), 2u);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[0]->script_id, 1);
  EXPECT_EQ(js_communication_utils_->document_end_scripts_[1]->script_id, 3);
}

// Test RemoveHeadReadyScript removes middle element
TEST_F(JsCommunicationUtilsTest, RemoveHeadReadyScript_Middle) {
  auto script1 = CreateStartScript(1, u"head1");
  auto script2 = CreateStartScript(2, u"head2");
  auto script3 = CreateStartScript(3, u"head3");
  js_communication_utils_->AddHeadReadyScript(script1);
  js_communication_utils_->AddHeadReadyScript(script2);
  js_communication_utils_->AddHeadReadyScript(script3);

  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 3u);

  js_communication_utils_->RemoveHeadReadyScript(2);

  ASSERT_EQ(js_communication_utils_->head_ready_scripts_.size(), 2u);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[0]->script_id, 1);
  EXPECT_EQ(js_communication_utils_->head_ready_scripts_[1]->script_id, 3);
}

// Test CommitPendingJavascriptsAtDocumentEnd with multiple pending scripts
TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtDocumentEnd_MultiplePending) {
  auto script1 = CreateEndScript(1, u"pending1");
  auto regex1 = CreateRegexRules(1, u"regex1", {{"h1.com", ".*"}});
  js_communication_utils_->AddPendingJavascriptAtDocumentEnd(script1, regex1);

  auto script2 = CreateEndScript(2, u"pending2");
  auto regex2 = CreateRegexRules(2, u"regex2", {{"h2.com", ".*"}});
  js_communication_utils_->AddPendingJavascriptAtDocumentEnd(script2, regex2);

  ASSERT_EQ(js_communication_utils_->swap_document_end_scripts_.size(), 2u);
  ASSERT_EQ(js_communication_utils_->swap_end_scripts_regex_rules_.size(), 2u);

  js_communication_utils_->CommitPendingJavascriptsAtDocumentEnd();

  EXPECT_EQ(js_communication_utils_->document_end_scripts_.size(), 2u);
  EXPECT_EQ(js_communication_utils_->end_scripts_regex_rules_.size(), 2u);
  EXPECT_TRUE(js_communication_utils_->swap_document_end_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_end_scripts_regex_rules_.empty());
}

// Test CommitPendingJavascriptsAtHeadReady with multiple pending scripts
TEST_F(JsCommunicationUtilsTest,
       CommitPendingJavascriptsAtHeadReady_MultiplePending) {
  auto script1 = CreateStartScript(1, u"head_pending1");
  auto regex1 = CreateRegexRules(1, u"head_regex1");
  js_communication_utils_->AddPendingJavascriptAtHeadReady(script1, regex1);

  auto script2 = CreateStartScript(2, u"head_pending2");
  auto regex2 = CreateRegexRules(2, u"head_regex2");
  js_communication_utils_->AddPendingJavascriptAtHeadReady(script2, regex2);

  ASSERT_EQ(js_communication_utils_->swap_head_ready_scripts_.size(), 2u);
  ASSERT_EQ(js_communication_utils_->swap_head_ready_regex_rules_.size(), 2u);

  js_communication_utils_->CommitPendingJavascriptsAtHeadReady();

  EXPECT_EQ(js_communication_utils_->head_ready_scripts_.size(), 2u);
  EXPECT_EQ(js_communication_utils_->head_ready_regex_rules_.size(), 2u);
  EXPECT_TRUE(js_communication_utils_->swap_head_ready_scripts_.empty());
  EXPECT_TRUE(js_communication_utils_->swap_head_ready_regex_rules_.empty());
}

// Test that JsCommunicationUtils constructor stores the impl pointer
TEST_F(JsCommunicationUtilsTest, Constructor_StoresImpl) {
  EXPECT_EQ(js_communication_utils_->jsCommunication_, nullptr);
  JsCommunication* mock_impl = reinterpret_cast<JsCommunication*>(0x1);
  auto utils = std::make_unique<JsCommunicationUtils>(mock_impl);
  EXPECT_EQ(utils->jsCommunication_, mock_impl);
}
}  // namespace js_injection
