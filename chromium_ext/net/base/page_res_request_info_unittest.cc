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

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "gtest/gtest.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/dns/public/secure_dns_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ARKWEB_EXT)
#include "arkweb/ohos_nweb_ex/build/features/features.h"
#endif
#include "arkweb/build/features/features.h"

#define private public
#include "arkweb/chromium_ext/net/base/page_res_request_info.h"
#undef private

namespace ohos_prp_preload {

class PRRequestInfoTest : public ::testing::Test {
 protected:
  PRRequestInfo info_;
};

// ---- Constructor tests ----

TEST_F(PRRequestInfoTest, DefaultConstructor) {
  PRRequestInfo info;
  EXPECT_EQ(info.url(), GURL());
  EXPECT_FALSE(info.allow_credentials());
  EXPECT_EQ(info.cache_type(), PRRequestCacheType::DISABLE_CACHE);
  EXPECT_EQ(info.freshness_life_times(), 0);
  EXPECT_FALSE(info.is_for_websockets());
  EXPECT_FALSE(info.ad_tagged());
  EXPECT_EQ(info.load_flags(), net::LOAD_NORMAL);
  EXPECT_EQ(info.secure_dns_policy(), net::SecureDnsPolicy::kAllow);
  EXPECT_FALSE(info.upgrade_if_insecure());
  EXPECT_TRUE(info.send_client_certs());
  EXPECT_EQ(info.idempotency(), net::DEFAULT_IDEMPOTENCY);
  EXPECT_FALSE(info.force_ignore_site_for_cookies());
  EXPECT_FALSE(info.force_main_frame_for_same_site_cookies());
  EXPECT_EQ(info.referrer_policy(),
            net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
  EXPECT_EQ(info.first_party_url_policy(),
            net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL);
  EXPECT_EQ(info.resource_type(), 0);
  EXPECT_FALSE(info.keepalive());
  EXPECT_FALSE(info.do_not_prompt_for_login());
  EXPECT_EQ(info.request_mode(), 0);
  EXPECT_EQ(info.request_credentials_mode(), 0);
  EXPECT_EQ(info.request_destination(), 0);
  EXPECT_EQ(info.type(), PRRequestInfoType::TYPE_DEFAULT);
  EXPECT_FALSE(info.only_send_reuse_request());
  EXPECT_EQ(info.request_start_time(), 0);
  EXPECT_EQ(info.preload_flag(), PRRequestFlags::PRPP_FLAGS_NONE);
  EXPECT_EQ(info.limit_num(), 0);
  EXPECT_EQ(info.preconnect_num(), 0);
  EXPECT_EQ(info.reused_preconnect_num(), 0);
  EXPECT_FALSE(info.is_preconnect());
  EXPECT_FALSE(info.is_reused_sokcet());
}

TEST_F(PRRequestInfoTest, ConstructorWithUrlAndCredentials) {
  GURL url("https://example.com/path");
  PRRequestInfo info(url, true);
  EXPECT_EQ(info.url(), url);
  EXPECT_TRUE(info.allow_credentials());
}

// ---- Simple setter/getter tests ----

TEST_F(PRRequestInfoTest, UrlSetterGetter) {
  GURL url("https://example.com/test");
  info_.set_url(url);
  EXPECT_EQ(info_.url(), url);

  GURL empty_url;
  info_.set_url(empty_url);
  EXPECT_EQ(info_.url(), empty_url);
}

TEST_F(PRRequestInfoTest, AllowCredentialsSetterGetter) {
  info_.set_allow_credentials(true);
  EXPECT_TRUE(info_.allow_credentials());
  info_.set_allow_credentials(false);
  EXPECT_FALSE(info_.allow_credentials());
}

TEST_F(PRRequestInfoTest, CacheTypeSetterGetter) {
  info_.set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
  EXPECT_EQ(info_.cache_type(), PRRequestCacheType::NEGOTIATION_CACHE);
  info_.set_cache_type(PRRequestCacheType::FORCE_CACHE);
  EXPECT_EQ(info_.cache_type(), PRRequestCacheType::FORCE_CACHE);
  info_.set_cache_type(PRRequestCacheType::DISABLE_CACHE);
  EXPECT_EQ(info_.cache_type(), PRRequestCacheType::DISABLE_CACHE);
}

TEST_F(PRRequestInfoTest, FreshnessLifeTimesSetterGetter) {
  info_.set_freshness_life_times(999);
  EXPECT_EQ(info_.freshness_life_times(), 999);
  info_.set_freshness_life_times(0);
  EXPECT_EQ(info_.freshness_life_times(), 0);
  info_.set_freshness_life_times(-1);
  EXPECT_EQ(info_.freshness_life_times(), -1);
}

TEST_F(PRRequestInfoTest, ETagSetterGetter) {
  info_.set_e_tag("w/\"etag123\"");
  EXPECT_EQ(info_.e_tag(), "w/\"etag123\"");
  info_.set_e_tag("");
  EXPECT_EQ(info_.e_tag(), "");
}

TEST_F(PRRequestInfoTest, LastModifiedSetterGetter) {
  info_.set_last_modified("Wed, 21 Oct 2025 07:28:00 GMT");
  EXPECT_EQ(info_.last_modified(), "Wed, 21 Oct 2025 07:28:00 GMT");
  info_.set_last_modified("");
  EXPECT_EQ(info_.last_modified(), "");
}

TEST_F(PRRequestInfoTest, IsForWebsocketsSetterGetter) {
  info_.set_is_for_websockets(true);
  EXPECT_TRUE(info_.is_for_websockets());
  info_.set_is_for_websockets(false);
  EXPECT_FALSE(info_.is_for_websockets());
}

TEST_F(PRRequestInfoTest, InitiatorSetterGetter) {
  auto origin = url::Origin::Create(GURL("https://initiator.com"));
  info_.set_initiator(origin);
  EXPECT_TRUE(info_.initiator().has_value());
  EXPECT_EQ(info_.initiator().value(), origin);

  info_.set_initiator(std::nullopt);
  EXPECT_FALSE(info_.initiator().has_value());
}

TEST_F(PRRequestInfoTest, ReferrerSetterGetter) {
  info_.set_referrer("https://referrer.com");
  EXPECT_EQ(info_.referrer(), "https://referrer.com");
  info_.set_referrer("");
  EXPECT_EQ(info_.referrer(), "");
}

TEST_F(PRRequestInfoTest, MethodSetterGetter) {
  info_.set_method("POST");
  EXPECT_EQ(info_.method(), "POST");
  info_.set_method("GET");
  EXPECT_EQ(info_.method(), "GET");
}

TEST_F(PRRequestInfoTest, LoadFlagsSetterGetter) {
  info_.set_load_flags(net::LOAD_VALIDATE_CACHE);
  EXPECT_EQ(info_.load_flags(), net::LOAD_VALIDATE_CACHE);
  info_.set_load_flags(net::LOAD_NORMAL);
  EXPECT_EQ(info_.load_flags(), net::LOAD_NORMAL);
}

TEST_F(PRRequestInfoTest, SecureDnsPolicySetterGetter) {
  info_.set_secure_dns_policy(net::SecureDnsPolicy::kDisable);
  EXPECT_EQ(info_.secure_dns_policy(), net::SecureDnsPolicy::kDisable);
  info_.set_secure_dns_policy(net::SecureDnsPolicy::kAllow);
  EXPECT_EQ(info_.secure_dns_policy(), net::SecureDnsPolicy::kAllow);
}

TEST_F(PRRequestInfoTest, UpgradeIfInsecureSetterGetter) {
  info_.set_upgrade_if_insecure(true);
  EXPECT_TRUE(info_.upgrade_if_insecure());
  info_.set_upgrade_if_insecure(false);
  EXPECT_FALSE(info_.upgrade_if_insecure());
}

TEST_F(PRRequestInfoTest, SendClientCertsSetterGetter) {
  info_.set_send_client_certs(false);
  EXPECT_FALSE(info_.send_client_certs());
  info_.set_send_client_certs(true);
  EXPECT_TRUE(info_.send_client_certs());
}

TEST_F(PRRequestInfoTest, IdempotencySetterGetter) {
  info_.set_idempotency(net::IDEMPOTENT);
  EXPECT_EQ(info_.idempotency(), net::IDEMPOTENT);
  info_.set_idempotency(net::NOT_IDEMPOTENT);
  EXPECT_EQ(info_.idempotency(), net::NOT_IDEMPOTENT);
}

TEST_F(PRRequestInfoTest, ForceIgnoreSiteForCookiesSetterGetter) {
  info_.set_force_ignore_site_for_cookies(true);
  EXPECT_TRUE(info_.force_ignore_site_for_cookies());
  info_.set_force_ignore_site_for_cookies(false);
  EXPECT_FALSE(info_.force_ignore_site_for_cookies());
}

TEST_F(PRRequestInfoTest, ForceMainFrameForSameSiteCookiesSetterGetter) {
  info_.set_force_main_frame_for_same_site_cookies(true);
  EXPECT_TRUE(info_.force_main_frame_for_same_site_cookies());
  info_.set_force_main_frame_for_same_site_cookies(false);
  EXPECT_FALSE(info_.force_main_frame_for_same_site_cookies());
}

TEST_F(PRRequestInfoTest, CookieSettingOverridesSetterGetter) {
  net::CookieSettingOverrides overrides;
  overrides.Put(
      net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
  info_.set_cookie_setting_overrides(overrides);
  EXPECT_TRUE(info_.cookie_setting_overrides().Has(
      net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible));
}

TEST_F(PRRequestInfoTest, ReferrerPolicySetterGetter) {
  info_.set_referrer_policy(net::ReferrerPolicy::NO_REFERRER);
  EXPECT_EQ(info_.referrer_policy(), net::ReferrerPolicy::NO_REFERRER);
  info_.set_referrer_policy(
      net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
  EXPECT_EQ(info_.referrer_policy(),
            net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
}

TEST_F(PRRequestInfoTest, FirstPartyUrlPolicySetterGetter) {
  info_.set_first_party_url_policy(
      net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
  EXPECT_EQ(info_.first_party_url_policy(),
            net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT);
  info_.set_first_party_url_policy(
      net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL);
  EXPECT_EQ(info_.first_party_url_policy(),
            net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL);
}

TEST_F(PRRequestInfoTest, ResourceTypeSetterGetter) {
  info_.set_resource_type(5);
  EXPECT_EQ(info_.resource_type(), 5);
  info_.set_resource_type(0);
  EXPECT_EQ(info_.resource_type(), 0);
}

TEST_F(PRRequestInfoTest, KeepaliveSetterGetter) {
  info_.set_keepalive(true);
  EXPECT_TRUE(info_.keepalive());
  info_.set_keepalive(false);
  EXPECT_FALSE(info_.keepalive());
}

TEST_F(PRRequestInfoTest, DoNotPromptForLoginSetterGetter) {
  info_.set_do_not_prompt_for_login(true);
  EXPECT_TRUE(info_.do_not_prompt_for_login());
  info_.set_do_not_prompt_for_login(false);
  EXPECT_FALSE(info_.do_not_prompt_for_login());
}

TEST_F(PRRequestInfoTest, RequestModeSetterGetter) {
  info_.set_request_mode(3);
  EXPECT_EQ(info_.request_mode(), 3);
  info_.set_request_mode(0);
  EXPECT_EQ(info_.request_mode(), 0);
}

TEST_F(PRRequestInfoTest, RequestCredentialsModeSetterGetter) {
  info_.set_request_credentials_mode(2);
  EXPECT_EQ(info_.request_credentials_mode(), 2);
  info_.set_request_credentials_mode(0);
  EXPECT_EQ(info_.request_credentials_mode(), 0);
}

TEST_F(PRRequestInfoTest, RequestDestinationSetterGetter) {
  info_.set_request_destination(1);
  EXPECT_EQ(info_.request_destination(), 1);
  info_.set_request_destination(0);
  EXPECT_EQ(info_.request_destination(), 0);
}

TEST_F(PRRequestInfoTest, AdTaggedSetterGetter) {
  info_.set_ad_tagged(true);
  EXPECT_TRUE(info_.ad_tagged());
  info_.set_ad_tagged(false);
  EXPECT_FALSE(info_.ad_tagged());
}

// ---- LoaderInfo-related tests ----

TEST_F(PRRequestInfoTest, LoaderInfoSetterGetter) {
  LoaderInfo loader_info;
  loader_info.resource_type_ = 7;
  loader_info.keepalive_ = true;
  loader_info.do_not_prompt_for_login_ = true;
  loader_info.request_mode_ = 2;
  loader_info.request_credentials_mode_ = 1;
  loader_info.request_destination_ = 3;

  info_.set_loader_info(loader_info);
  EXPECT_EQ(info_.resource_type(), 7);
  EXPECT_TRUE(info_.keepalive());
  EXPECT_TRUE(info_.do_not_prompt_for_login());
  EXPECT_EQ(info_.request_mode(), 2);
  EXPECT_EQ(info_.request_credentials_mode(), 1);
  EXPECT_EQ(info_.request_destination(), 3);
}

// ---- Type and page-origin tests ----

TEST_F(PRRequestInfoTest, TypeSetterGetter) {
  info_.set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
  EXPECT_EQ(info_.type(), PRRequestInfoType::TYPE_PAGE_ORIGIN);
  info_.set_type(PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
  EXPECT_EQ(info_.type(), PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
  info_.set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
  EXPECT_EQ(info_.type(), PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
  info_.set_type(PRRequestInfoType::TYPE_DEFAULT);
  EXPECT_EQ(info_.type(), PRRequestInfoType::TYPE_DEFAULT);
}

TEST_F(PRRequestInfoTest, PageOriginSetterGetter) {
  info_.set_page_origin("https://example.com");
  EXPECT_EQ(info_.page_origin(), "https://example.com");
  info_.set_page_origin("");
  EXPECT_EQ(info_.page_origin(), "");
}

TEST_F(PRRequestInfoTest, OnlySendReuseRequestSetterGetter) {
  info_.set_only_send_reuse_request(true);
  EXPECT_TRUE(info_.only_send_reuse_request());
  info_.set_only_send_reuse_request(false);
  EXPECT_FALSE(info_.only_send_reuse_request());
}

// ---- Dynamic header tests ----

TEST_F(PRRequestInfoTest, DynamicHeaderKeysSetterGetter) {
  std::set<std::string> keys = {"Authorization", "Cookie"};
  info_.set_dynamic_header_keys(keys);
  EXPECT_EQ(info_.dynamic_header_keys(), keys);
  EXPECT_EQ(info_.dynamic_header_keys().size(), 2u);
}

TEST_F(PRRequestInfoTest, ParentForDynamicHeaderSetterGetter) {
  GURL url("https://parent.example.com");
  info_.set_parent_for_dynamic_header(url);
  EXPECT_EQ(info_.parent_for_dynamic_header(), url);
}

// ---- Request time tests ----

TEST_F(PRRequestInfoTest, RequestStartTimeSetterGetter) {
  info_.set_request_start_time(42);
  EXPECT_EQ(info_.request_start_time(), 42);
  info_.set_request_start_time(0);
  EXPECT_EQ(info_.request_start_time(), 0);
}

// ---- CacheInfo tests ----

TEST_F(PRRequestInfoTest, CacheInfoSetAndGet) {
  info_.set_cache_info(PRRequestCacheType::NEGOTIATION_CACHE, 12345, "etag",
                       "lastmod");
  CacheInfo ci = info_.cache_info();
  EXPECT_EQ(ci.cache_type_, PRRequestCacheType::NEGOTIATION_CACHE);
  EXPECT_EQ(ci.freshness_life_times_, 12345);
  EXPECT_EQ(ci.e_tag_, "etag");
  EXPECT_EQ(ci.last_modified_, "lastmod");
}

TEST_F(PRRequestInfoTest, CacheInfo_DefaultValues) {
  CacheInfo ci = info_.cache_info();
  EXPECT_EQ(ci.cache_type_, PRRequestCacheType::DISABLE_CACHE);
  EXPECT_EQ(ci.freshness_life_times_, 0);
  EXPECT_EQ(ci.e_tag_, "");
  EXPECT_EQ(ci.last_modified_, "");
}

TEST_F(PRRequestInfoTest, CacheInfo_ForceCache) {
  info_.set_cache_info(PRRequestCacheType::FORCE_CACHE, 500, "etag2", "lm2");
  CacheInfo ci = info_.cache_info();
  EXPECT_EQ(ci.cache_type_, PRRequestCacheType::FORCE_CACHE);
  EXPECT_EQ(ci.freshness_life_times_, 500);
}

// ---- Request end time tests ----

TEST_F(PRRequestInfoTest, RequestEndTime) {
  info_.set_request_start_time(100);
  info_.set_request_end_time(100);
  EXPECT_EQ(info_.request_end_time(), 0);

  info_.set_request_end_time(101);
  EXPECT_EQ(info_.request_end_time(), 101);

  info_.set_request_end_time(10);
  EXPECT_EQ(info_.request_end_time(), 0);
}

TEST_F(PRRequestInfoTest, RequestEndTime_StartTimeZero) {
  info_.set_request_start_time(0);
  info_.set_request_end_time(1);
  // req_end_time_ (1) > req_start_time_ (0) -> returns 1
  EXPECT_EQ(info_.request_end_time(), 1);
}

// ---- Preload flag tests ----

TEST_F(PRRequestInfoTest, PreloadFlagSetAndOr) {
  info_.set_preload_flag(PRRequestFlags::PRPP_FLAGS_HDR_NOT_MATCH);
  EXPECT_EQ(info_.preload_flag(), PRRequestFlags::PRPP_FLAGS_HDR_NOT_MATCH);
  info_.or_preload_flag(PRRequestFlags::PRPP_FLAGS_VISIBLE);
  EXPECT_EQ(info_.preload_flag(), PRRequestFlags::PRPP_FLAGS_VISIBLE |
                                      PRRequestFlags::PRPP_FLAGS_HDR_NOT_MATCH);
}

TEST_F(PRRequestInfoTest, PreloadFlag_None) {
  info_.set_preload_flag(PRRequestFlags::PRPP_FLAGS_NONE);
  EXPECT_EQ(info_.preload_flag(), PRRequestFlags::PRPP_FLAGS_NONE);
}

TEST_F(PRRequestInfoTest, PreloadFlag_OrSameFlag) {
  info_.set_preload_flag(PRRequestFlags::PRPP_FLAGS_VISIBLE);
  info_.or_preload_flag(PRRequestFlags::PRPP_FLAGS_VISIBLE);
  EXPECT_EQ(info_.preload_flag(), PRRequestFlags::PRPP_FLAGS_VISIBLE);
}

TEST_F(PRRequestInfoTest, PreloadFlag_OrMultiple) {
  info_.set_preload_flag(PRRequestFlags::PRPP_FLAGS_HDR_NOT_MATCH);
  info_.or_preload_flag(PRRequestFlags::PRPP_FLAGS_VISIBLE);
  info_.or_preload_flag(PRRequestFlags::PRPP_FLAGS_HDR_DYNAMIC);
  EXPECT_EQ(info_.preload_flag(), PRRequestFlags::PRPP_FLAGS_HDR_NOT_MATCH |
                                      PRRequestFlags::PRPP_FLAGS_VISIBLE |
                                      PRRequestFlags::PRPP_FLAGS_HDR_DYNAMIC);
}

TEST_F(PRRequestInfoTest, PreloadFlag_Unexpected) {
  info_.set_preload_flag(PRRequestFlags::PRPP_FLAGS_UNSUPPORT);
  EXPECT_EQ(info_.preload_flag(), PRRequestFlags::PRPP_FLAGS_UNSUPPORT);
}

// ---- Preconnect counter tests ----

TEST_F(PRRequestInfoTest, PreconnectNumCounters) {
  info_.add_preconnect_num(true);
  info_.add_preconnect_num(false);
  info_.add_reused_preconnect_num(true);
  info_.add_reused_preconnect_num(false);
  info_.compute_limit_num();
  // reused_preconnect_num_ = 1
  EXPECT_EQ(info_.limit_num(), 1);
}

TEST_F(PRRequestInfoTest, PreconnectNumCounters_Max) {
  info_.add_preconnect_num(true);
  for (int i = 0; i < MAX_PRECONNECT_COUNT; i++) {
    info_.add_reused_preconnect_num(true);
  }
  info_.compute_limit_num();
  // reused_preconnect_num_ = MAX_PRECONNECT_COUNT
  EXPECT_EQ(info_.limit_num(), MAX_PRECONNECT_COUNT);
}

TEST_F(PRRequestInfoTest, PreconnectNumCounters_Zero) {
  info_.add_preconnect_num(true);
  info_.add_reused_preconnect_num(false);
  info_.compute_limit_num();
  // reused_preconnect_num_ = 0
  EXPECT_EQ(info_.limit_num(), 0);
}

TEST_F(PRRequestInfoTest, PreconnectNumCounters_Zero1) {
  info_.add_preconnect_num(false);
  info_.add_reused_preconnect_num(true);
  info_.compute_limit_num();
  // preconnect_num_ = 0
  EXPECT_EQ(info_.limit_num(), 0);
}

TEST_F(PRRequestInfoTest, PreconnectNumExceedMax) {
  info_.add_preconnect_num(true);
  for (int i = 0; i < MAX_PRECONNECT_COUNT + 5; i++) {
    info_.add_reused_preconnect_num(true);
  }
  info_.compute_limit_num();
  // reused_preconnect_num_ > MAX_PRECONNECT_COUNT -> capped to
  // MAX_PRECONNECT_COUNT
  EXPECT_EQ(info_.limit_num(), MAX_PRECONNECT_COUNT);
}

TEST_F(PRRequestInfoTest, PreconnectAndReusedPreconnectDirectSetterGetter) {
  info_.set_preconnect_num(3);
  EXPECT_EQ(info_.preconnect_num(), 3);
  info_.set_reused_preconnect_num(2);
  EXPECT_EQ(info_.reused_preconnect_num(), 2);
}

TEST_F(PRRequestInfoTest, LimitNumDirectSetterGetter) {
  info_.set_limit_num(5);
  EXPECT_EQ(info_.limit_num(), 5);
}

// ---- Preconnect and reused socket flags tests ----

TEST_F(PRRequestInfoTest, PreconnectAndReusedSocketFlags) {
  info_.set_is_preconnect(true);
  EXPECT_TRUE(info_.is_preconnect());
  info_.set_is_preconnect(false);
  EXPECT_FALSE(info_.is_preconnect());

  info_.set_is_reused_sokcet(true);
  EXPECT_TRUE(info_.is_reused_sokcet());
  info_.set_is_reused_sokcet(false);
  EXPECT_FALSE(info_.is_reused_sokcet());
}

// ---- Extra request headers tests ----

TEST_F(PRRequestInfoTest, ExtraRequestHeadersSetAndGet) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("test1", "value1");
  info_.set_extra_request_headers(headers);
  EXPECT_TRUE(info_.extra_request_headers().HasHeader("test1"));
  EXPECT_TRUE(info_.extra_request_headers().GetHeader("test1").has_value());
  EXPECT_EQ(info_.extra_request_headers().GetHeader("test1").value(), "value1");
}

TEST_F(PRRequestInfoTest, ExtraRequestHeaders_MultipleHeaders) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Custom-1", "val1");
  headers.SetHeader("X-Custom-2", "val2");
  info_.set_extra_request_headers(headers);

  auto retrieved = info_.extra_request_headers();
  EXPECT_TRUE(retrieved.HasHeader("X-Custom-1"));
  EXPECT_TRUE(retrieved.HasHeader("X-Custom-2"));
  EXPECT_EQ(retrieved.GetHeader("X-Custom-1").value(), "val1");
  EXPECT_EQ(retrieved.GetHeader("X-Custom-2").value(), "val2");
}

TEST_F(PRRequestInfoTest, ExtraRequestHeaders_ReplaceHeaders) {
  net::HttpRequestHeaders headers1;
  headers1.SetHeader("key1", "val1");
  info_.set_extra_request_headers(headers1);
  EXPECT_TRUE(info_.extra_request_headers().HasHeader("key1"));

  net::HttpRequestHeaders headers2;
  headers2.SetHeader("key2", "val2");
  info_.set_extra_request_headers(headers2);
  EXPECT_FALSE(info_.extra_request_headers().HasHeader("key1"));
  EXPECT_TRUE(info_.extra_request_headers().HasHeader("key2"));
}

// ---- Page index and preload seq num tests ----

TEST_F(PRRequestInfoTest, PageIndexSetterGetter) {
  info_.set_page_index(42);
  EXPECT_EQ(info_.page_index(), 42);
  info_.set_page_index(0);
  EXPECT_EQ(info_.page_index(), 0);
}

TEST_F(PRRequestInfoTest, PreloadSeqNumSetterGetter) {
  info_.set_preload_seq_num("seq_001");
  EXPECT_EQ(info_.preload_seq_num(), "seq_001");
  info_.set_preload_seq_num("");
  EXPECT_EQ(info_.preload_seq_num(), "");
}

// ---- Accepted stream types test ----

TEST_F(PRRequestInfoTest, AcceptedStreamTypesSetterGetter) {
  base::flat_set<net::SourceStreamType> types = {
      net::SourceStreamType::kGzip, net::SourceStreamType::kBrotli};
  info_.set_accepted_stream_types(types);
  EXPECT_TRUE(info_.accepted_stream_types().has_value());
  EXPECT_EQ(info_.accepted_stream_types().value().size(), 2u);

  info_.set_accepted_stream_types(std::nullopt);
  EXPECT_FALSE(info_.accepted_stream_types().has_value());
}

// ---- CacheInfo struct default values ----

TEST_F(PRRequestInfoTest, CacheInfoStructDefaultValues) {
  CacheInfo ci;
  EXPECT_EQ(ci.cache_type_, PRRequestCacheType::DISABLE_CACHE);
  EXPECT_EQ(ci.freshness_life_times_, 0);
  EXPECT_EQ(ci.e_tag_, "");
  EXPECT_EQ(ci.last_modified_, "");
}

}  // namespace ohos_prp_preload
