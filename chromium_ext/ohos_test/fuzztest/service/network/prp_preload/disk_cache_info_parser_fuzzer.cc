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

#include <fuzzer/FuzzedDataProvider.h>
#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_info_parser.h"

namespace ohos_prp_preload {
constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxJsonLen = 512;

static std::shared_ptr<PRRequestInfo> g_info = nullptr;

void InitializeGlobalResourcesDiskCacheInfoParser() {
    if (!g_info) {
        g_info = std::make_shared<PRRequestInfo>();
    }
}

void FuzzParseResReqPreloadInfoForPreconnectScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    dict.Set("url", fdp->ConsumeRandomLengthString(kMaxJsonLen));
    dict.Set("method", fdp->ConsumeRandomLengthString(10));
    DiskCacheInfoParser::ParseResReqPreloadInfoForPreconnect(g_info, dict);
}

void FuzzParseResReqPreloadInfoForPreloadScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    dict.Set("url", fdp->ConsumeRandomLengthString(kMaxJsonLen));
    dict.Set("priority", fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::ParseResReqPreloadInfoForPreload(g_info, dict);
}

void FuzzParseResReqPreloadInfoForPreconnectLimitScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    dict.Set("limit", fdp->ConsumeIntegral<int>());
    dict.Set("timeout", fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::ParseResReqPreloadInfoForPreconnectLimit(g_info, dict);
}

void FuzzParseJsonForPreconnectScenario(FuzzedDataProvider* fdp) {
    std::string json_str = fdp->ConsumeRandomLengthString(kMaxJsonLen);
    base::Value::Dict dict;
    dict.Set(json_str, fdp->ConsumeBool());
    base::Value json(std::move(dict));
    DiskCacheInfoParser::ParseJsonForPreconnect(json, g_info);
}

void FuzzParseJsonForPreloadScenario(FuzzedDataProvider* fdp) {
    std::string json_str = fdp->ConsumeRandomLengthString(kMaxJsonLen);
    base::Value::Dict dict;
    dict.Set(json_str, fdp->ConsumeBool());
    base::Value json(std::move(dict));
    DiskCacheInfoParser::ParseJsonForPreload(json, g_info);
}

void FuzzParseJsonForPreconnectLimitScenario(FuzzedDataProvider* fdp) {
    std::string json_str = fdp->ConsumeRandomLengthString(kMaxJsonLen);
    base::Value::Dict dict;
    dict.Set(json_str, fdp->ConsumeBool());
    base::Value json(std::move(dict));
    DiskCacheInfoParser::ParseJsonForPreconnectLimit(json, g_info);
}

void FuzzSetAndGetUrlScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxJsonLen);
    g_info->set_url(GURL(url_str));
    DiskCacheInfoParser::SetUrlToJson(g_info, "url", dict);
    DiskCacheInfoParser::GetUrlFromJson(base::Value(std::move(dict)), "url", g_info);
}

void FuzzSetAndGetMethodScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_method(fdp->ConsumeRandomLengthString(16));
    DiskCacheInfoParser::SetMethodToJson(g_info, "method", dict);
    DiskCacheInfoParser::GetMethodFromJson(base::Value(std::move(dict)), "method", g_info);
}

void FuzzSetAndGetLoadFlagsScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_load_flags(fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::SetLoadFlagsToJson(g_info, "load_flags", dict);
    DiskCacheInfoParser::GetLoadFlagsFromJson(base::Value(std::move(dict)), "load_flags", g_info);
}

void FuzzSetAndGetReferrerScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_referrer(fdp->ConsumeRandomLengthString(kMaxJsonLen));
    DiskCacheInfoParser::SetReferrerToJson(g_info, "referrer", dict);
    DiskCacheInfoParser::GetReferrerFromJson(base::Value(std::move(dict)), "referrer", g_info);
}

void FuzzSetAndGetReferrerPolicyScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_referrer_policy(static_cast<net::ReferrerPolicy>(fdp->ConsumeIntegralInRange<int>(0, 7)));
    DiskCacheInfoParser::SetReferrerPolicyToJson(g_info, "referrer_policy", dict);
    DiskCacheInfoParser::GetReferrerPolicyFromJson(base::Value(std::move(dict)), "referrer_policy", g_info);
}

void FuzzSetAndGetKeepaliveScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_keepalive(fdp->ConsumeBool());
    DiskCacheInfoParser::SetKeepaliveToJson(g_info, "keepalive", dict);
    DiskCacheInfoParser::GetKeepaliveFromJson(base::Value(std::move(dict)), "keepalive", g_info);
}

void FuzzSetAndGetResourceTypeScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_resource_type(fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::SetResourceTypeToJson(g_info, "resource_type", dict);
    DiskCacheInfoParser::GetResourceTypeFromJson(base::Value(std::move(dict)), "resource_type", g_info);
}

void FuzzSetAndGetPreloadFlagsScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    int flag = fdp->ConsumeIntegralInRange<int>(0, 7);
    g_info->set_preload_flag(static_cast<PRRequestFlags>(flag));
    DiskCacheInfoParser::SetPreloadFlagsToJson(g_info, "preload_flag", dict);
    DiskCacheInfoParser::GetPreloadFlagsFromJson(base::Value(std::move(dict)), "preload_flag", g_info);
}

void FuzzSetAndGetRequestModeScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_request_mode(fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::SetRequestModeToJson(g_info, "request_mode", dict);
    DiskCacheInfoParser::GetRequestModeFromJson(base::Value(std::move(dict)), "request_mode", g_info);
}

void FuzzSetAndGetRequestDestinationScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_request_destination(fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::SetRequestDestinationToJson(g_info, "request_destination", dict);
    DiskCacheInfoParser::GetRequestDestinationFromJson(base::Value(std::move(dict)), "request_destination", g_info);
}

void FuzzSetAndGetPageIndexScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_page_index(fdp->ConsumeIntegral<int64_t>());
    DiskCacheInfoParser::SetPageIndexToJson(g_info, "page_index", dict);
    DiskCacheInfoParser::GetPageIndexFromJson(base::Value(std::move(dict)), "page_index", g_info);
}

void FuzzSetAndGetPageOriginScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_page_origin(fdp->ConsumeRandomLengthString(kMaxJsonLen));
    DiskCacheInfoParser::SetPageOriginToJson(g_info, "page_origin", dict);
    DiskCacheInfoParser::GetPageOriginFromJson(base::Value(std::move(dict)), "page_origin", g_info);
}

void FuzzSetAndGetPreloadSeqNumScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_preload_seq_num(fdp->ConsumeRandomLengthString(kMaxJsonLen));
    DiskCacheInfoParser::SetPreloadSeqNumToJson(g_info, "preload_seq_num", dict);
    DiskCacheInfoParser::GetPreloadSeqNumFromJson(base::Value(std::move(dict)), "preload_seq_num", g_info);
}

void FuzzSetAndGetRequestInfoTypeScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    int type = fdp->ConsumeIntegralInRange<int>(0, 3);
    g_info->set_type(static_cast<PRRequestInfoType>(type));
    DiskCacheInfoParser::SetRequestInfoTypeToJson(g_info, "request_info_type", dict);
    DiskCacheInfoParser::GetRequestInfoTypeFromJson(base::Value(std::move(dict)), "request_info_type", g_info);
}

void FuzzSetAndGetRequestStartTimeScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_request_start_time(fdp->ConsumeIntegral<int64_t>());
    DiskCacheInfoParser::SetRequestStartTimeToJson(g_info, "request_start_time", dict);
    DiskCacheInfoParser::GetRequestStartTimeFromJson(base::Value(std::move(dict)), "request_start_time", g_info);
}

void FuzzSetAndGetRequestEndTimeScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_request_end_time(fdp->ConsumeIntegral<int64_t>());
    DiskCacheInfoParser::SetRequestEndTimeToJson(g_info, "request_end_time", dict);
    DiskCacheInfoParser::GetRequestEndTimeFromJson(base::Value(std::move(dict)), "request_end_time", g_info);
}

void FuzzSetAndGetInitiatorScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    std::string initiator_str = fdp->ConsumeRandomLengthString(kMaxJsonLen);
    g_info->set_initiator(url::Origin::Create(GURL(initiator_str)));
    DiskCacheInfoParser::SetInitiatorToJson(g_info, "initiator", dict);
    DiskCacheInfoParser::GetInitiatorFromJson(base::Value(std::move(dict)), "initiator", g_info);
}

void FuzzSetAndGetFirstPartyUrlPolicyScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_first_party_url_policy(static_cast<net::RedirectInfo::FirstPartyURLPolicy>(fdp->ConsumeIntegralInRange<int>(0, 1)));
    DiskCacheInfoParser::SetFirstPartyUrlPolicyToJson(g_info, "first_party_url_policy", dict);
    DiskCacheInfoParser::GetFirstPartyUrlPolicyFromJson(base::Value(std::move(dict)), "first_party_url_policy", g_info);
}

void FuzzSetAndGetUpgradeIfInsecureScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_upgrade_if_insecure(fdp->ConsumeBool());
    DiskCacheInfoParser::SetUpgradeIfInsecureToJson(g_info, "upgrade_if_insecure", dict);
    DiskCacheInfoParser::GetUpgradeIfInsecureFromJson(base::Value(std::move(dict)), "upgrade_if_insecure", g_info);
}

void FuzzSetAndGetForceIgnoreSiteForCookiesScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_force_ignore_site_for_cookies(fdp->ConsumeBool());
    DiskCacheInfoParser::SetForceIgnoreSiteForCookiesToJson(g_info, "force_ignore_site_for_cookies", dict);
    DiskCacheInfoParser::GetForceIgnoreSiteForCookiesFromJson(base::Value(std::move(dict)), "force_ignore_site_for_cookies", g_info);
}

void FuzzSetAndGetForceMainFrameForSameSiteCookiesScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_force_main_frame_for_same_site_cookies(fdp->ConsumeBool());
    DiskCacheInfoParser::SetForceMainFrameForSameSiteCookiesToJson(g_info, "force_main_frame_for_same_site_cookies", dict);
    DiskCacheInfoParser::GetForceMainFrameForSameSiteCookiesFromJson(base::Value(std::move(dict)), "force_main_frame_for_same_site_cookies", g_info);
}

void FuzzSetAndGetAllowCredentialsScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_allow_credentials(fdp->ConsumeBool());
    DiskCacheInfoParser::SetAllowCredentialsToJson(g_info, "allow_credentials", dict);
    DiskCacheInfoParser::GetAllowCredentialsFromJson(base::Value(std::move(dict)), "allow_credentials", g_info);
}

void FuzzSetAndGetAdTaggedScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_ad_tagged(fdp->ConsumeBool());
    DiskCacheInfoParser::SetAdTaggedToJson(g_info, "ad_tagged", dict);
    DiskCacheInfoParser::GetAdTaggedFromJson(base::Value(std::move(dict)), "ad_tagged", g_info);
}

void FuzzSetAndGetIsForWebsocketsScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_is_for_websockets(fdp->ConsumeBool());
    DiskCacheInfoParser::SetIsForWebsocketsToJson(g_info, "is_for_websockets", dict);
    DiskCacheInfoParser::GetIsForWebsocketsFromJson(base::Value(std::move(dict)), "is_for_websockets", g_info);
}

void FuzzSetAndGetIdempotencyScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_idempotency(static_cast<net::Idempotency>(fdp->ConsumeIntegralInRange<int>(0, 2)));
    DiskCacheInfoParser::SetIdempotencyToJson(g_info, "idempotency", dict);
    DiskCacheInfoParser::GetIdempotencyFromJson(base::Value(std::move(dict)), "idempotency", g_info);
}

void FuzzSetAndGetRequestCredentialsModeScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_request_credentials_mode(fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::SetRequestCredentialsModeToJson(g_info, "request_credentials_mode", dict);
    DiskCacheInfoParser::GetRequestCredentialsModeFromJson(base::Value(std::move(dict)), "request_credentials_mode", g_info);
}

void FuzzSetAndGetDoNotPromptForLoginScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_do_not_prompt_for_login(fdp->ConsumeBool());
    DiskCacheInfoParser::SetDoNotPromptForLoginToJson(g_info, "do_not_prompt_for_login", dict);
    DiskCacheInfoParser::GetDoNotPromptForLoginFromJson(base::Value(std::move(dict)), "do_not_prompt_for_login", g_info);
}

void FuzzSetAndGetSendClientCertsScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_send_client_certs(fdp->ConsumeBool());
    DiskCacheInfoParser::SetSendClientCertsToJson(g_info, "send_client_certs", dict);
    DiskCacheInfoParser::GetSendClientCertsFromJson(base::Value(std::move(dict)), "send_client_certs", g_info);
}

void FuzzSetAndGetSecureDnsPolicyScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_secure_dns_policy(static_cast<net::SecureDnsPolicy>(fdp->ConsumeIntegralInRange<int>(0, 2)));
    DiskCacheInfoParser::SetSecureDnsPolicyToJson(g_info, "secure_dns_policy", dict);
    DiskCacheInfoParser::GetSecureDnsPolicyFromJson(base::Value(std::move(dict)), "secure_dns_policy", g_info);
}

void FuzzSetAndGetCacheInfoScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    PRRequestCacheType cache_type = static_cast<PRRequestCacheType>(fdp->ConsumeIntegralInRange<int>(0, 2));
    int64_t freshness_life_times = fdp->ConsumeIntegral<int64_t>();
    std::string e_tag = fdp->ConsumeRandomLengthString(kMaxJsonLen);
    std::string last_modified = fdp->ConsumeRandomLengthString(kMaxJsonLen);
    g_info->set_cache_info(cache_type, freshness_life_times, e_tag, last_modified);
    DiskCacheInfoParser::SetCacheInfoToJson(g_info, "cache_info", dict);
    DiskCacheInfoParser::GetCacheInfoFromJson(base::Value(std::move(dict)), "cache_info", g_info);
}

void FuzzSetAndGetLimitNumScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_limit_num(fdp->ConsumeIntegral<int>());
    DiskCacheInfoParser::SetLimitNumToJson(g_info, "limit_num", dict);
    DiskCacheInfoParser::GetLimitNumFromJson(base::Value(std::move(dict)), "limit_num", g_info);
}

void FuzzSetAndGetExtraRequestHeadersScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    net::HttpRequestHeaders headers;
    headers.SetHeader("Accept", "application/json");
    headers.SetHeader("Content-Type", "text/html");
    g_info->set_extra_request_headers(headers);
    DiskCacheInfoParser::SetExtraRequestHeadersToJson(g_info, "extra_request_headers", dict);
    DiskCacheInfoParser::GetExtraRequestHeadersFromJson(base::Value(std::move(dict)), "extra_request_headers", g_info);
}

void FuzzSetAndGetDynamicHeaderKeysScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    std::set<std::string> keys;
    size_t count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
    for (size_t i = 0; i < count; ++i) {
        keys.insert(fdp->ConsumeRandomLengthString(32));
    }
    g_info->set_dynamic_header_keys(keys);
    DiskCacheInfoParser::SetDynamicHeaderKeysToJson(g_info, "dynamic_header_keys", dict);
    DiskCacheInfoParser::GetDynamicHeaderKeysFromJson(base::Value(std::move(dict)), "dynamic_header_keys", g_info);
}

void FuzzSetAndGetAcceptedStreamTypesScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_accepted_stream_types(std::nullopt);
    DiskCacheInfoParser::SetAcceptedStreamTypesToJson(g_info, "accepted_stream_types", dict);
    DiskCacheInfoParser::GetAcceptedStreamTypesFromJson(base::Value(std::move(dict)), "accepted_stream_types", g_info);
}

void FuzzSetAndGetCookieSettingOverridesScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    net::CookieSettingOverrides overrides;
    int override_value = fdp->ConsumeIntegralInRange<int>(0, 7);
    overrides.Put(static_cast<net::CookieSettingOverride>(override_value));
    g_info->set_cookie_setting_overrides(overrides);
    DiskCacheInfoParser::SetCookieSettingOverridesToJson(g_info, "cookie_setting_overrides", dict);
    DiskCacheInfoParser::GetCookieSettingOverridesFromJson(base::Value(std::move(dict)), "cookie_setting_overrides", g_info);
}

void FuzzSetAndGetStorageAccessStatusScenario(FuzzedDataProvider* fdp) {
    base::Value::Dict dict;
    g_info->set_storage_access_status(static_cast<net::cookie_util::StorageAccessStatus>(fdp->ConsumeIntegralInRange<int>(0, 2)));
    DiskCacheInfoParser::SetStorageAccessStatusToJson(g_info, "storage_access_status", dict);
    DiskCacheInfoParser::GetStorageAccessStatusFromJson(base::Value(std::move(dict)), "storage_access_status", g_info);
}

void DiskCacheInfoParserFuzzTest(const uint8_t* data, size_t size) {
    InitializeGlobalResourcesDiskCacheInfoParser();
    
    FuzzedDataProvider fdp(data, size);
    size_t iterations = std::min(fdp.ConsumeIntegralInRange<size_t>(1, kMaxIterations), size);
    
    for (size_t i = 0; i < iterations; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 44);
        
        switch (scenario) {
            case 0:
                FuzzParseResReqPreloadInfoForPreconnectScenario(&fdp);
                break;
            case 1:
                FuzzParseResReqPreloadInfoForPreloadScenario(&fdp);
                break;
            case 2:
                FuzzParseResReqPreloadInfoForPreconnectLimitScenario(&fdp);
                break;
            case 3:
                FuzzParseJsonForPreconnectScenario(&fdp);
                break;
            case 4:
                FuzzParseJsonForPreloadScenario(&fdp);
                break;
            case 5:
                FuzzParseJsonForPreconnectLimitScenario(&fdp);
                break;
            case 6:
                FuzzSetAndGetUrlScenario(&fdp);
                break;
            case 7:
                FuzzSetAndGetMethodScenario(&fdp);
                break;
            case 8:
                FuzzSetAndGetLoadFlagsScenario(&fdp);
                break;
            case 9:
                FuzzSetAndGetReferrerScenario(&fdp);
                break;
            case 10:
                FuzzSetAndGetReferrerPolicyScenario(&fdp);
                break;
            case 11:
                FuzzSetAndGetKeepaliveScenario(&fdp);
                break;
            case 12:
                FuzzSetAndGetResourceTypeScenario(&fdp);
                break;
            case 13:
                FuzzSetAndGetPreloadFlagsScenario(&fdp);
                break;
            case 14:
                FuzzSetAndGetRequestModeScenario(&fdp);
                break;
            case 15:
                FuzzSetAndGetRequestDestinationScenario(&fdp);
                break;
            case 16:
                FuzzSetAndGetPageIndexScenario(&fdp);
                break;
            case 17:
                FuzzSetAndGetPageOriginScenario(&fdp);
                break;
            case 18:
                FuzzSetAndGetPreloadSeqNumScenario(&fdp);
                break;
            case 19:
                FuzzSetAndGetRequestInfoTypeScenario(&fdp);
                break;
            case 20:
                FuzzSetAndGetRequestStartTimeScenario(&fdp);
                break;
            case 21:
                FuzzSetAndGetRequestEndTimeScenario(&fdp);
                break;
            case 22:
                FuzzSetAndGetInitiatorScenario(&fdp);
                break;
            case 23:
                FuzzSetAndGetFirstPartyUrlPolicyScenario(&fdp);
                break;
            case 24:
                FuzzSetAndGetUpgradeIfInsecureScenario(&fdp);
                break;
            case 25:
                FuzzSetAndGetForceIgnoreSiteForCookiesScenario(&fdp);
                break;
            case 26:
                FuzzSetAndGetForceMainFrameForSameSiteCookiesScenario(&fdp);
                break;
            case 27:
                FuzzSetAndGetAllowCredentialsScenario(&fdp);
                break;
            case 28:
                FuzzSetAndGetAdTaggedScenario(&fdp);
                break;
            case 29:
                FuzzSetAndGetIsForWebsocketsScenario(&fdp);
                break;
            case 30:
                FuzzSetAndGetIdempotencyScenario(&fdp);
                break;
            case 31:
                FuzzSetAndGetRequestCredentialsModeScenario(&fdp);
                break;
            case 32:
                FuzzSetAndGetDoNotPromptForLoginScenario(&fdp);
                break;
            case 33:
                FuzzSetAndGetSendClientCertsScenario(&fdp);
                break;
            case 34:
                FuzzSetAndGetSecureDnsPolicyScenario(&fdp);
                break;
            case 35:
                FuzzSetAndGetCacheInfoScenario(&fdp);
                break;
            case 36:
                FuzzSetAndGetLimitNumScenario(&fdp);
                break;
            case 37:
                FuzzSetAndGetExtraRequestHeadersScenario(&fdp);
                break;
            case 38:
                FuzzSetAndGetDynamicHeaderKeysScenario(&fdp);
                break;
            case 39:
                FuzzSetAndGetAcceptedStreamTypesScenario(&fdp);
                break;
            case 40:
                FuzzSetAndGetCookieSettingOverridesScenario(&fdp);
                break;
            case 41:
                FuzzSetAndGetStorageAccessStatusScenario(&fdp);
                break;
            default:
                break;
        }
    }
}
} //namespace ohos_prp_preload

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if ((data == nullptr) || (size == 0)) {
        return 0;
    }
    ohos_prp_preload::DiskCacheInfoParserFuzzTest(data, size);
    return 0;
}