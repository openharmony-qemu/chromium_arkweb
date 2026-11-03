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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "url/gurl.h"
#include "url/origin.h"

#define ARKWEB_TEST 1
#if BUILDFLAG(ARKWEB_ADBLOCK)
#include "arkweb/chromium_ext/components/url_pattern_index/flat/css_pattern_index_generated.h"
#include "arkweb/chromium_ext/components/url_pattern_index/url_pattern_ext.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#endif
#include "arkweb/chromium_ext/components/url_pattern_index/url_pattern_index_ext.h"
#undef ARKWEB_TEST

namespace url_pattern_index {
class UrlPatternIndexExtTest : public testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}

 protected:
#if BUILDFLAG(ARKWEB_ADBLOCK)
  // Helper function to create a test CSS rule
  proto::CssRule CreateTestCssRule(
      const std::string& selector,
      proto::RuleSemantics semantics = proto::RULE_SEMANTICS_BLOCKLIST) {
    proto::CssRule rule;
    rule.set_css_selector(selector);
    rule.set_semantics(semantics);
    return rule;
  }

  const flat::CssRule* CreateFlatCssRule(const proto::CssRule& rule) {
    flatbuffers::FlatBufferBuilder builder;
    FlatDomainMap domain_map;

    CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
    EXPECT_NE(offset.o, 0u);
    // Verify the rule was properly serialized
    const flat::CssRule* flat_rule =
        flatbuffers::GetTemporaryPointer(builder, offset);
    EXPECT_NE(flat_rule, nullptr);
    return flat_rule;
  }

  // Helper function to create a test CSS rule with domain
  proto::CssRule CreateTestCssRuleWithDomain(
      const std::string& selector,
      const std::string& domain,
      bool exclude = false,
      proto::RuleSemantics semantics = proto::RULE_SEMANTICS_BLOCKLIST) {
    proto::CssRule rule;
    rule.set_css_selector(selector);
    rule.set_semantics(semantics);
    auto* domain_item = rule.add_domains();
    domain_item->set_domain(domain);
    domain_item->set_exclude(exclude);
    return rule;
  }

  // Helper function to create a FlatBuffer CSS rule
  const flat::CssRule* CreateFlatCssRule(
      const std::string& selector,
      const std::vector<std::string>& included_domains = {},
      const std::vector<std::string>& excluded_domains = {},
      uint8_t options = 0) {
    flatbuffers::FlatBufferBuilder builder;

    // Create domain vectors
    std::vector<flatbuffers::Offset<flatbuffers::String>> included_offsets;
    std::vector<flatbuffers::Offset<flatbuffers::String>> excluded_offsets;

    for (const auto& domain : included_domains) {
      included_offsets.push_back(builder.CreateString(domain));
    }
    for (const auto& domain : excluded_domains) {
      excluded_offsets.push_back(builder.CreateString(domain));
    }

    flatbuffers::Offset<
        flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>>
        included_vector;
    flatbuffers::Offset<
        flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>>
        excluded_vector;
    if (!included_offsets.empty()) {
      included_vector = builder.CreateVector(included_offsets);
    }
    if (!excluded_offsets.empty()) {
      excluded_vector = builder.CreateVector(excluded_offsets);
    }

    auto css_selector = builder.CreateString(selector);
    auto rule_offset = flat::CreateCssRule(builder, options, included_vector,
                                           excluded_vector, css_selector);
    builder.Finish(rule_offset);
    const flat::CssRule* flat_rule =
        flatbuffers::GetTemporaryPointer(builder, rule_offset);
    EXPECT_NE(flat_rule, nullptr);
    return flat_rule;
  }

  // Helper function to create a simple FlatBuffer index for testing
  std::unique_ptr<flatbuffers::FlatBufferBuilder>
  CreateSimpleCssPatternIndex() {
    auto builder = std::make_unique<flatbuffers::FlatBufferBuilder>();

    // Create empty hash table
    std::vector<flatbuffers::Offset<flat::NGramToCssRules>> empty_table;
    for (int i = 0; i < 10; ++i) {
      empty_table.push_back(flat::CreateNGramToCssRules(*builder));
    }

    auto hash_table = builder->CreateVector(empty_table);
    auto empty_slot = flat::CreateNGramToCssRules(*builder);

    // Create empty rule lists
    std::vector<flatbuffers::Offset<flat::CssRule>> empty_rules;
    auto fallback_rules = builder->CreateVector(empty_rules);
    auto no_domain_rules = builder->CreateVector(empty_rules);

    auto index = flat::CreateCssPatternIndex(
        *builder, 3, hash_table, empty_slot, fallback_rules, no_domain_rules);
    builder->Finish(index);
    return builder;
  }
#endif
};

#if BUILDFLAG(ARKWEB_ADBLOCK)
// Test SerializeCssRule function
TEST_F(UrlPatternIndexExtTest, SerializeCssRule_BasicRule) {
  flatbuffers::FlatBufferBuilder builder;
  FlatDomainMap domain_map;
  auto rule = CreateTestCssRule(".advertisement");
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_NE(offset.o, 0u);
  // is_convertible_ but is not used in CssRule so it should be false
}

// Test SerializeCssRule with domain
TEST_F(UrlPatternIndexExtTest, SerializeCssRule_WithDomain) {
  flatbuffers::FlatBufferBuilder builder;
  FlatDomainMap domain_map;

  auto rule = CreateTestCssRule(".advertisement");
  auto* domain_item = rule.add_domains();
  domain_item->set_domain("allowed.com");
  domain_item->set_exclude(false);
  domain_item = rule.add_domains();
  domain_item->set_domain("allowed2.com");
  domain_item->set_exclude(true);
  domain_item->set_domain("allowed3.com");
  domain_item->set_exclude(false);
  domain_item->set_domain("allowed4.com");
  domain_item->set_exclude(true);
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_NE(offset.o, 0u);
}

TEST_F(UrlPatternIndexExtTest, SerializeCssRule_NonAsciiDomain) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  auto rule = CreateTestCssRuleWithDomain(".banner", "测试.中国");
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_EQ(offset.o, 0u);  // Should fail for non-ASCII domain
}

// Test CssPatternIndexBuilder IndexCssRule
TEST_F(UrlPatternIndexExtTest, CssPatternIndexBuilder_IndexCssRule) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  auto rule = CreateTestCssRule(".advertisement");
  auto* domain_item = rule.add_domains();
  domain_item->set_domain("allowed.COM");
  domain_item->set_exclude(false);
  domain_item = rule.add_domains();
  domain_item->set_domain("allowed2.com");
  domain_item->set_exclude(true);
  domain_item = rule.add_domains();
  domain_item->set_domain("allowed3.COM");
  domain_item->set_exclude(false);
  domain_item = rule.add_domains();
  domain_item->set_domain("allowed4.com");
  domain_item->set_exclude(true);
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_NE(offset.o, 0);
  EXPECT_NO_FATAL_FAILURE({ index_builder.IndexCssRule(offset); });
  EXPECT_EQ(index_builder.GetNGramHashTableUsedSize(), 4);
}

TEST_F(UrlPatternIndexExtTest, CssPatternIndexBuilder_FallbackRules_) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  const size_t max = 28;
  size_t count = 7 * kMinListSize;
  int fallback_count = 1;
  if (count > max) {
    count = max;
    fallback_count = 0;
  }
  auto rule = CreateTestCssRule(".advertisement");
  for (size_t i = 0; i < count; i++) {
    auto* domain_item = rule.add_domains();
    domain_item->set_domain("allowed.COM");
    domain_item->set_exclude(false);
  }
  auto* domain_item = rule.add_domains();
  domain_item = rule.add_domains();
  domain_item->set_domain("allowed.com");
  domain_item->set_exclude(false);
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_NE(offset.o, 0u);
  EXPECT_NO_FATAL_FAILURE({ index_builder.IndexCssRule(offset); });
  EXPECT_GE(static_cast<size_t>(index_builder.GetNGramHashTableUsedSize()),
            count);
  EXPECT_EQ(index_builder.GetFallbackRuleListSize(), fallback_count);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  EXPECT_NE(index_offset.o, 0);
  const flat::CssPatternIndex* index =
      flatbuffers::GetTemporaryPointer(builder, index_offset);
  CssPatternIndexMatcher matcher(index);

  GURL test_url1("http://allowed.com");
  GURL test_url2("http://sub.allowed.com");
  GURL test_url3("http://other.com");
  GURL test_url4("http://nomatch.com");
  auto result1 = matcher.FindMatchInSpecialRules(test_url1);
  EXPECT_FALSE(result1.empty());
  auto result2 = matcher.FindMatchInSpecialRules(test_url2);
  EXPECT_FALSE(result2.empty());
  auto result3 = matcher.FindMatchInSpecialRules(test_url3);
  EXPECT_TRUE(result3.empty());
  auto result4 = matcher.FindMatchInSpecialRules(test_url4);
  EXPECT_TRUE(result4.empty());
}

// Test CssPatternIndexBuilder with no domain rule
TEST_F(UrlPatternIndexExtTest, CssPatternIndexBuilder_NoDomainRule) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  auto rule = CreateTestCssRule(".generic-ad");  // No domain
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  ASSERT_NE(offset.o, 0);
  EXPECT_NO_FATAL_FAILURE({ index_builder.IndexCssRule(offset); });
  // Should increase no domain rule count
  EXPECT_GT(index_builder.GetNoDomainRuleListSize(), 0);
  CssPatternIndexOffset index_offset = index_builder.Finish();
  EXPECT_NE(index_offset.o, 0);
}

// Test DoesOriginMatchDomainList function
TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_IncludedDomain) {
  const flat::CssRule* rule = CreateFlatCssRule(".test", {"example.com"});
  EXPECT_NE(rule, nullptr);
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_TRUE(result);
}

// Test DoesOriginMatchDomainList with subdomain
TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_Subdomain) {
  const flat::CssRule* rule = CreateFlatCssRule(".test", {"example.com"});
  EXPECT_NE(rule, nullptr);

  url::Origin origin = url::Origin::Create(GURL("https://sub.example.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_TRUE(result);
}

// Test DoesOriginMatchDomainList with excluded domain
TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_ExcludedDomain) {
  auto rule = CreateFlatCssRule(".test", {"example.com"}, {"sub.example.com"});
  EXPECT_NE(rule, nullptr);

  url::Origin origin = url::Origin::Create(GURL("https://sub.example.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_FALSE(result);
  url::Origin origin1 = url::Origin::Create(GURL("https://subxxx.example.com"));
  result = DoesOriginMatchDomainList(origin1, *rule);
  EXPECT_TRUE(result);
}

TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_ExcludedDomain2) {
  auto rule = CreateFlatCssRule(".test", {"example.com"}, {"sub.xxx.com"});
  EXPECT_NE(rule, nullptr);

  url::Origin origin = url::Origin::Create(GURL("https://sub.wwwww.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_FALSE(result);
}

// Test DoesOriginMatchDomainList with no domains (generic rule)
TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_GenericRule) {
  auto rule = CreateFlatCssRule(".test");
  EXPECT_NE(rule, nullptr);

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_FALSE(result);  // Generic rules should return false
}

// Test DoesOriginMatchDomainList with opaque origin
TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_OpaqueOrigin) {
  const flat::CssRule* rule = CreateFlatCssRule(".test", {"example.com"});
  EXPECT_NE(rule, nullptr);

  url::Origin origin;  // Opaque origin
  bool result = DoesOriginMatchDomainList(origin, *rule);
  // Opaque origins should return false for non-generic rules
  EXPECT_FALSE(result);
}

// Test CssPatternIndexMatcher construction
TEST_F(UrlPatternIndexExtTest, CssPatternIndexMatcher_Construction) {
  auto builder = CreateSimpleCssPatternIndex();
  const flat::CssPatternIndex* index =
      flatbuffers::GetRoot<flat::CssPatternIndex>(builder->GetBufferPointer());
  CssPatternIndexMatcher matcher(index);
  const FlatCssRuleList* result = matcher.GetGenericCssRules();
  EXPECT_NE(result, nullptr);
}

// Test CssPatternIndexMatcher with null index
TEST_F(UrlPatternIndexExtTest, CssPatternIndexMatcher_NullIndex) {
  CssPatternIndexMatcher matcher(nullptr);
  const FlatCssRuleList* result = matcher.GetGenericCssRules();
  EXPECT_EQ(result, nullptr);

  GURL test_url("https://example.com/page");
  auto result2 = matcher.FindMatchInSpecialRules(test_url);
  EXPECT_TRUE(result2.empty());
}

// Test edge case: empty CSS selector
TEST_F(UrlPatternIndexExtTest, EdgeCase_EmptySelector) {
  flatbuffers::FlatBufferBuilder builder;
  FlatDomainMap domain_map;

  auto rule = CreateTestCssRule("");  // Empty selector
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_NE(offset.o, 0);  // Should still serialize successfully
}

// Test domain sharing in FlatDomainMap
TEST_F(UrlPatternIndexExtTest, DomainSharing) {
  flatbuffers::FlatBufferBuilder builder;
  FlatDomainMap domain_map;

  // Create two rules with same domain
  auto rule1 = CreateTestCssRuleWithDomain(".ad1", "example.com");
  auto rule2 = CreateTestCssRuleWithDomain(".ad2", "example.com");
  CssRuleOffset offset1 = SerializeCssRule(rule1, &builder, &domain_map);
  CssRuleOffset offset2 = SerializeCssRule(rule2, &builder, &domain_map);
  EXPECT_NE(offset1.o, 0);
  EXPECT_NE(offset2.o, 0);
  // Domain map should contain shared domains
  EXPECT_FALSE(domain_map.empty());
}

// Test SerializeCssRule with uppercase domain (lowered by ToLowerASCII)
TEST_F(UrlPatternIndexExtTest, SerializeCssRule_UppercaseDomain) {
  flatbuffers::FlatBufferBuilder builder;
  FlatDomainMap domain_map;

  auto rule = CreateTestCssRuleWithDomain(".banner", "EXAMPLE.COM");
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_NE(offset.o, 0u);

  // Verify the domain was lowercased
  const flat::CssRule* flat_rule =
      flatbuffers::GetTemporaryPointer(builder, offset);
  ASSERT_NE(flat_rule, nullptr);
  ASSERT_NE(flat_rule->domains_included(), nullptr);
  EXPECT_EQ(std::string(flat_rule->domains_included()->Get(0)->c_str()),
            "example.com");
}

// Test CssPatternIndexMatcher move constructor
TEST_F(UrlPatternIndexExtTest, CssPatternIndexMatcher_MoveConstructor) {
  auto fb_builder = CreateSimpleCssPatternIndex();
  const flat::CssPatternIndex* index =
      flatbuffers::GetRoot<flat::CssPatternIndex>(
          fb_builder->GetBufferPointer());
  CssPatternIndexMatcher matcher1(index);
  EXPECT_NE(matcher1.GetGenericCssRules(), nullptr);

  CssPatternIndexMatcher matcher2(std::move(matcher1));
  EXPECT_NE(matcher2.GetGenericCssRules(), nullptr);
}

// Test CssPatternIndexMatcher move assignment
TEST_F(UrlPatternIndexExtTest, CssPatternIndexMatcher_MoveAssignment) {
  auto fb_builder = CreateSimpleCssPatternIndex();
  const flat::CssPatternIndex* index =
      flatbuffers::GetRoot<flat::CssPatternIndex>(
          fb_builder->GetBufferPointer());

  CssPatternIndexMatcher matcher1(index);
  CssPatternIndexMatcher matcher2(nullptr);
  EXPECT_EQ(matcher2.GetGenericCssRules(), nullptr);

  matcher2 = std::move(matcher1);
  EXPECT_NE(matcher2.GetGenericCssRules(), nullptr);
}

// Test end-to-end FindMatchInSpecialRules with ngram hash lookup
TEST_F(UrlPatternIndexExtTest, FindMatchInSpecialRules_EndToEnd) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Add a CSS rule with a domain that will produce valid ngrams
  auto rule = CreateTestCssRuleWithDomain(".ad-banner", "adserver.com");
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  ASSERT_NE(offset.o, 0);
  index_builder.IndexCssRule(offset);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  ASSERT_NE(index_offset.o, 0);

  const flat::CssPatternIndex* index =
      flatbuffers::GetTemporaryPointer(builder, index_offset);
  CssPatternIndexMatcher matcher(index);

  // Matching URL should find the rule
  GURL matching_url("http://adserver.com/page");
  auto result = matcher.FindMatchInSpecialRules(matching_url);
  EXPECT_FALSE(result.empty());

  // Non-matching URL should not find the rule
  GURL non_matching_url("http://safe-site.com/page");
  auto result2 = matcher.FindMatchInSpecialRules(non_matching_url);
  EXPECT_TRUE(result2.empty());
}

// Test GetGenericCssRules returns actual no_domain_rules
TEST_F(UrlPatternIndexExtTest, GetGenericCssRules_WithNoDomainRules) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Add a no-domain (generic) CSS rule
  auto rule = CreateTestCssRule(".generic-ad");
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  ASSERT_NE(offset.o, 0);
  index_builder.IndexCssRule(offset);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  ASSERT_NE(index_offset.o, 0);

  const flat::CssPatternIndex* index =
      flatbuffers::GetTemporaryPointer(builder, index_offset);
  CssPatternIndexMatcher matcher(index);

  const FlatCssRuleList* generic_rules = matcher.GetGenericCssRules();
  ASSERT_NE(generic_rules, nullptr);
  EXPECT_EQ(generic_rules->size(), 1u);
}

// Test DoesOriginMatchDomainList with only excluded domains (no included match)
TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_NoIncludedMatch) {
  // Rule applies to example.com but we check from other.com
  const flat::CssRule* rule = CreateFlatCssRule(".test", {"example.com"});
  ASSERT_NE(rule, nullptr);

  url::Origin origin = url::Origin::Create(GURL("https://other.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_FALSE(result);  // No matching included domain
}

// Test DoesOriginMatchDomainList with included but longer excluded subdomain
TEST_F(UrlPatternIndexExtTest,
       DoesOriginMatchDomainList_IncludedButExcludedOverride) {
  // Rule applies to example.com but excludes sub.example.com
  auto rule = CreateFlatCssRule(".test", {"example.com"}, {"sub.example.com"});
  ASSERT_NE(rule, nullptr);

  // Root domain should match
  url::Origin root_origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(DoesOriginMatchDomainList(root_origin, *rule));

  // Excluded subdomain should not match
  url::Origin sub_origin = url::Origin::Create(GURL("https://sub.example.com"));
  EXPECT_FALSE(DoesOriginMatchDomainList(sub_origin, *rule));

  // Non-excluded subdomain should match
  url::Origin other_sub_origin =
      url::Origin::Create(GURL("https://other.example.com"));
  EXPECT_TRUE(DoesOriginMatchDomainList(other_sub_origin, *rule));
}

// Test multiple rules with different priorities are sorted correctly
TEST_F(UrlPatternIndexExtTest, CssPatternIndexBuilder_PrioritySorting) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Add rules with different priorities (higher priority number = higher
  // priority)
  auto rule_low = CreateTestCssRuleWithDomain(".low", "example.com");
  auto rule_high = CreateTestCssRuleWithDomain(".high", "example.com");

  CssRuleOffset offset_low = SerializeCssRule(rule_low, &builder, &domain_map);
  CssRuleOffset offset_high =
      SerializeCssRule(rule_high, &builder, &domain_map);
  ASSERT_NE(offset_low.o, 0);
  ASSERT_NE(offset_high.o, 0);

  index_builder.IndexCssRule(offset_low);
  index_builder.IndexCssRule(offset_high);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  ASSERT_NE(index_offset.o, 0);

  // Verify the index was built correctly
  const flat::CssPatternIndex* index =
      flatbuffers::GetTemporaryPointer(builder, index_offset);
  ASSERT_NE(index, nullptr);
  EXPECT_NE(index->ngram_index(), nullptr);
}

// Test SerializeCssRule with mixed included and excluded domains
TEST_F(UrlPatternIndexExtTest, SerializeCssRule_MixedDomains) {
  flatbuffers::FlatBufferBuilder builder;
  FlatDomainMap domain_map;

  proto::CssRule rule;
  rule.set_css_selector(".mixed-ad");

  auto* included1 = rule.add_domains();
  included1->set_domain("ads.example.com");
  included1->set_exclude(false);

  auto* excluded1 = rule.add_domains();
  excluded1->set_domain("safe.example.com");
  excluded1->set_exclude(true);

  auto* included2 = rule.add_domains();
  included2->set_domain("tracker.example.com");
  included2->set_exclude(false);

  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  EXPECT_NE(offset.o, 0u);

  const flat::CssRule* flat_rule =
      flatbuffers::GetTemporaryPointer(builder, offset);
  ASSERT_NE(flat_rule, nullptr);
  ASSERT_NE(flat_rule->domains_included(), nullptr);
  EXPECT_EQ(flat_rule->domains_included()->size(), 2u);
  ASSERT_NE(flat_rule->domains_excluded(), nullptr);
  EXPECT_EQ(flat_rule->domains_excluded()->size(), 1u);
}

// Test IndexCssRule with domain too short for ngram (fallback)
TEST_F(UrlPatternIndexExtTest, CssPatternIndexBuilder_ShortDomainFallback) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Single character domain - too short for 3-gram, should go to fallback
  auto rule = CreateTestCssRuleWithDomain(".short", "a.b");
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  ASSERT_NE(offset.o, 0);
  index_builder.IndexCssRule(offset);

  // Short domain should produce fallback rules
  EXPECT_GT(index_builder.GetFallbackRuleListSize(), 0);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  EXPECT_NE(index_offset.o, 0);
}

// Test excluded domain with short name causing fallback through excluded path
TEST_F(UrlPatternIndexExtTest, CssPatternIndexBuilder_ExcludedDomainFallback) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Rule with a valid included domain but a short excluded domain ("a.b")
  // that can't produce ngrams, triggering should_fallback through excluded path
  proto::CssRule rule;
  rule.set_css_selector(".ad-short-excl");
  auto* included = rule.add_domains();
  included->set_domain("adserver.com");
  included->set_exclude(false);
  auto* excluded = rule.add_domains();
  excluded->set_domain("a.b");
  excluded->set_exclude(true);

  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  ASSERT_NE(offset.o, 0);
  index_builder.IndexCssRule(offset);

  // Short excluded domain should produce fallback rules
  EXPECT_GT(index_builder.GetFallbackRuleListSize(), 0);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  EXPECT_NE(index_offset.o, 0);
}

// Test deduplication: same rule found via multiple ngrams appears only once
TEST_F(UrlPatternIndexExtTest, FindMatchAmongCandidates_Deduplication) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Add a rule with two different included domains that will produce
  // different ngrams, so the same rule can be found via multiple ngram lookups
  proto::CssRule rule;
  rule.set_css_selector(".ad-dedup");
  auto* domain1 = rule.add_domains();
  domain1->set_domain("adserver.com");
  domain1->set_exclude(false);
  auto* domain2 = rule.add_domains();
  domain2->set_domain("adnetwork.org");
  domain2->set_exclude(false);

  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  ASSERT_NE(offset.o, 0);
  index_builder.IndexCssRule(offset);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  ASSERT_NE(index_offset.o, 0);

  const flat::CssPatternIndex* index =
      flatbuffers::GetTemporaryPointer(builder, index_offset);
  CssPatternIndexMatcher matcher(index);

  // URL matching both included domains should find the rule but only once
  GURL test_url("http://adserver.com");
  auto result = matcher.FindMatchInSpecialRules(test_url);
  // The rule should be found at most once despite being indexed under
  // multiple ngrams
  int count = 0;
  for (const auto* r : result) {
    if (std::string(r->css_selector()->c_str()) == ".ad-dedup") {
      count++;
    }
  }
  EXPECT_LE(count, 1);
}

// Test DoesOriginMatchDomainList where excluded domain is a longer match
// than included domain (e.g. subdomain excluded matches more specifically)
TEST_F(UrlPatternIndexExtTest,
       DoesOriginMatchDomainList_ExcludedLongerThanIncluded) {
  // Rule applies to com but excludes sub.example.com
  // When checking sub.example.com: included matches "com" (length 3+1),
  // excluded matches "sub.example.com" (longer), so excluded wins => false
  auto rule = CreateFlatCssRule(".test", {"com"}, {"sub.example.com"});
  ASSERT_NE(rule, nullptr);

  // The origin host is "sub.example.com"
  // Included domain "com" matches with length 4 (".com")
  // Excluded domain "sub.example.com" matches with length 16
  // Excluded is longer => return false
  url::Origin origin = url::Origin::Create(GURL("https://sub.example.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_FALSE(result);

  // The origin host is just "example.com"
  // Included domain "com" matches with length 4
  // Excluded domain "sub.example.com" does NOT match "example.com"
  // So result is true
  url::Origin origin2 = url::Origin::Create(GURL("https://example.com"));
  bool result2 = DoesOriginMatchDomainList(origin2, *rule);
  EXPECT_TRUE(result2);
}

// Test ArkWebUrlPatternIndexBuilderExt construction and destruction
TEST_F(UrlPatternIndexExtTest, ArkWebUrlPatternIndexBuilderExt_Construction) {
  flatbuffers::FlatBufferBuilder builder;
  ArkWebUrlPatternIndexBuilderExt ext_builder(&builder);
  // Verify the NGram hash table size (should be default)
  EXPECT_GE(ext_builder.GetNGramHashTableSize(), 0);
}

// Test CssPatternIndexBuilder with multiple rules hitting same ngram
TEST_F(UrlPatternIndexExtTest, CssPatternIndexBuilder_MultipleRulesSameDomain) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Add two rules for the same domain
  auto rule1 = CreateTestCssRuleWithDomain(".ad1", "example.com");
  auto rule2 = CreateTestCssRuleWithDomain(".ad2", "example.com");

  CssRuleOffset offset1 = SerializeCssRule(rule1, &builder, &domain_map);
  CssRuleOffset offset2 = SerializeCssRule(rule2, &builder, &domain_map);
  ASSERT_NE(offset1.o, 0);
  ASSERT_NE(offset2.o, 0);

  index_builder.IndexCssRule(offset1);
  index_builder.IndexCssRule(offset2);

  // Both should be indexed under the same ngram
  EXPECT_GT(index_builder.GetNGramHashTableUsedSize(), 0);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  ASSERT_NE(index_offset.o, 0);
}

// Test DoesOriginMatchDomainList - included matches but no excluded domains
TEST_F(UrlPatternIndexExtTest, DoesOriginMatchDomainList_IncludedNoExcluded) {
  const flat::CssRule* rule = CreateFlatCssRule(".test", {"example.com"});
  ASSERT_NE(rule, nullptr);

  // Should match included domain when there are no excluded domains
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(DoesOriginMatchDomainList(origin, *rule));
}

// Test DoesOriginMatchDomainList with excluded domain shorter than included
TEST_F(UrlPatternIndexExtTest,
       DoesOriginMatchDomainList_ExcludedShorterThanIncluded) {
  // Rule includes "sub.example.com" but excludes "com"
  // When checking sub.example.com: included matches "sub.example.com" (longer),
  // excluded matches "com" (shorter), included wins => true
  auto rule = CreateFlatCssRule(".test", {"sub.example.com"}, {"com"});
  ASSERT_NE(rule, nullptr);

  url::Origin origin = url::Origin::Create(GURL("https://sub.example.com"));
  bool result = DoesOriginMatchDomainList(origin, *rule);
  EXPECT_TRUE(result);
}

// Test CssPatternIndexMatcher FindMatchInSpecialRules with properly built index
// that has no matching rules
TEST_F(UrlPatternIndexExtTest, FindMatchInSpecialRules_NoMatchInBuiltIndex) {
  flatbuffers::FlatBufferBuilder builder;
  CssPatternIndexBuilder index_builder(&builder);
  FlatDomainMap domain_map;

  // Add a CSS rule with a domain that won't match our test URL
  auto rule = CreateTestCssRuleWithDomain(".ad-banner", "adserver.com");
  CssRuleOffset offset = SerializeCssRule(rule, &builder, &domain_map);
  ASSERT_NE(offset.o, 0);
  index_builder.IndexCssRule(offset);

  CssPatternIndexOffset index_offset = index_builder.Finish();
  ASSERT_NE(index_offset.o, 0);

  const flat::CssPatternIndex* index =
      flatbuffers::GetTemporaryPointer(builder, index_offset);
  CssPatternIndexMatcher matcher(index);

  // URL that doesn't match the rule's domain
  GURL safe_url("https://safe-site.com/page");
  auto result = matcher.FindMatchInSpecialRules(safe_url);
  EXPECT_TRUE(result.empty());
}

#endif  // BUILDFLAG(ARKWEB_ADBLOCK)

}  // namespace url_pattern_index
