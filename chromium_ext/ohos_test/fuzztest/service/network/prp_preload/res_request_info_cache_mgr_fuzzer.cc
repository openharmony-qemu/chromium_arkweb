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
#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "base/json/json_writer.h"
#include "base/json/json_reader.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_request_headers.h"

#define private public
#define protected public
#include "arkweb/chromium_ext/net/base/page_res_request_info.h"
#include "arkweb/chromium_ext/services/network/prp_preload/src/res_request_info_cache_mgr.h"
#undef protected
#undef private

#include "arkweb/chromium_ext/services/network/prp_preload/include/page_res_parallel_preload_mgr.h"
#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_info_parser.h"
#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_backend_factory.h"

namespace ohos_prp_preload {

bool DoNotNeedToStore(const std::shared_ptr<PRRequestInfo>& info, int64_t page_index);
bool ParseResReqInfoList(LinkedHashMap& info_list,
    base::Value::List& list, PRPPreloadMode mode, int64_t page_index, bool is_preconnect_limit_list);
bool ResReqPreloadInfoListToJson(const std::string& page_origin,
    LinkedHashMap& preload_info_list,
    LinkedHashMap& load_prelaod_info_list,
    LinkedHashMap& precommect_limit_list,
    int64_t page_index,
    std::string& entry_content);
void JsonToResReqPreloadInfoList(const std::string& json_content,
    LinkedHashMap& prelaod_info_list,
    LinkedHashMap& preconnect_limit_info_list,
    int64_t& page_index);

constexpr size_t kMaxIterations = 20;
constexpr size_t kMaxStringLength = 256;
constexpr int64_t kMaxPageIndex = 100;
constexpr int64_t MICRO_SECONDS_PER_DAY = 1000LL * 1000 * 3600 * 24;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;
static scoped_refptr<DiskCacheBackendFactory> g_backend_factory = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::DEFAULT);
        if (!g_backend_factory) {
            g_backend_factory = base::WrapRefCounted(new DiskCacheBackendFactory());
        }
    }
    ~Environment() {
        task_environment_.reset();
        g_backend_factory.reset();
    }
};

std::shared_ptr<PRRequestInfo> CreatePRRequestInfo(FuzzedDataProvider* fdp) {
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLength);
    if (url_str.empty()) {
        url_str = "https://example.com/path";
    }
    GURL url(url_str);
    bool allow_credentials = fdp->ConsumeBool();
    
    auto info = std::make_shared<PRRequestInfo>(url, allow_credentials);
    
    int type_int = fdp->ConsumeIntegralInRange<int>(0, 3);
    info->set_type(static_cast<PRRequestInfoType>(type_int));
    
    if (fdp->ConsumeBool()) {
        info->set_is_preconnect(fdp->ConsumeBool());
    }
    if (fdp->ConsumeBool()) {
        info->set_is_reused_sokcet(fdp->ConsumeBool());
    }
    if (fdp->ConsumeBool()) {
        info->set_preload_flag(static_cast<PRRequestFlags>(fdp->ConsumeIntegral<uint32_t>()));
    }
    if (fdp->ConsumeBool()) {
        info->set_cache_type(static_cast<PRRequestCacheType>(fdp->ConsumeIntegralInRange<int>(0, 2)));
    }
    if (fdp->ConsumeBool()) {
        info->set_limit_num(fdp->ConsumeIntegralInRange<int64_t>(0, 100));
    }
    if (fdp->ConsumeBool()) {
        info->set_page_index(fdp->ConsumeIntegralInRange<int64_t>(0, kMaxPageIndex));
    }
    if (fdp->ConsumeBool()) {
        info->set_preload_seq_num(fdp->ConsumeRandomLengthString(64));
    }
    
    return info;
}

// 直接测试 DoNotNeedToStore 函数
void FuzzDoNotNeedToStore(FuzzedDataProvider* fdp) {
    auto info = std::make_shared<PRRequestInfo>(GURL("https://example.com"), false);
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(0, kMaxPageIndex + 10);
    
    info->set_preload_seq_num("");
    bool result = DoNotNeedToStore(info, page_index);
    
    info->set_preload_seq_num(std::to_string(page_index - 1) + ", ");
    result = DoNotNeedToStore(info, page_index);
    
    info->set_preload_seq_num(std::to_string(page_index) + ", ");
    result = DoNotNeedToStore(info, page_index);
    
    info->set_preload_seq_num("1, 2, " + std::to_string(page_index - 1) + ", ");
    result = DoNotNeedToStore(info, page_index);
    
    info->set_preload_seq_num("invalid_seq");
    result = DoNotNeedToStore(info, page_index);
}

// 直接测试 ParseResReqInfoList 函数
void FuzzParseResReqInfoList(FuzzedDataProvider* fdp) {
    LinkedHashMap info_list;
    base::Value::List list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(0, kMaxPageIndex);
    PRPPreloadMode mode = static_cast<PRPPreloadMode>(fdp->ConsumeIntegralInRange<int>(0, 2));
    bool is_preconnect_limit_list = fdp->ConsumeBool();
    
    auto info1 = CreatePRRequestInfo(fdp);
    info1->set_preload_flag(PRPP_FLAGS_VISIBLE);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info_list.insert({info1->url().spec(), info1});
    
    auto info2 = CreatePRRequestInfo(fdp);
    info2->set_limit_num(fdp->ConsumeIntegralInRange<int64_t>(0, 100));
    info_list.insert({info2->url().spec(), info2});
    
    bool need_store = ParseResReqInfoList(info_list, list, mode, page_index, is_preconnect_limit_list);
}

// 直接测试 ResReqPreloadInfoListToJson 函数
void FuzzResReqPreloadInfoListToJson(FuzzedDataProvider* fdp) {
    LinkedHashMap preload_info_list;
    LinkedHashMap load_prelaod_info_list;
    LinkedHashMap preconnect_limit_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(0, kMaxPageIndex);
    std::string entry_content;
    
    std::string page_origin = fdp->ConsumeRandomLengthString(64);
    
    auto info1 = CreatePRRequestInfo(fdp);
    info1->set_preload_flag(PRPP_FLAGS_VISIBLE);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    preload_info_list.insert({info1->url().spec(), info1});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://limit.com"), true);
    info2->set_limit_num(fdp->ConsumeIntegralInRange<int64_t>(1, 100));
    info2->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    preconnect_limit_list.insert({info2->url().spec(), info2});
    
    auto info3 = std::make_shared<PRRequestInfo>();
    info3->set_preload_seq_num(std::to_string(page_index - 1) + ", ");
    load_prelaod_info_list.insert({"https://loaded.com", info3});
    
    bool result = ResReqPreloadInfoListToJson(
        page_origin, preload_info_list, load_prelaod_info_list,
        preconnect_limit_list, page_index, entry_content);
    
    result = ResReqPreloadInfoListToJson(
        "", preload_info_list, load_prelaod_info_list,
        preconnect_limit_list, page_index, entry_content);
}

// 直接测试 JsonToResReqPreloadInfoList 和 ParseJson 函数
void FuzzJsonToResReqPreloadInfoList(FuzzedDataProvider* fdp) {
    LinkedHashMap prelaod_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = 0;
    
    JsonToResReqPreloadInfoList("", prelaod_info_list, preconnect_limit_info_list, page_index);
    
    JsonToResReqPreloadInfoList("invalid json", prelaod_info_list, preconnect_limit_info_list, page_index);
    
    std::string valid_json = R"json([
        {
            "request_info_type": 2,
            "page_index": "0",
            "page_origin": "https://example.com",
            "url": "https://test.com"
        },
        {
            "request_info_type": 1,
            "url": "https://origin.com",
            "allow_credentials": true
        },
        {
            "request_info_type": 3,
            "allow_credentials": true,
            "limit_num": "10",
            "url": "https://limit.com"
        }
    ])json";
    JsonToResReqPreloadInfoList(valid_json, prelaod_info_list, preconnect_limit_info_list, page_index);
    
    std::string invalid_type_json = R"json([
        {
            "request_info_type": 0,
            "page_index": "0"
        }
    ])json";
    JsonToResReqPreloadInfoList(invalid_type_json, prelaod_info_list, preconnect_limit_info_list, page_index);
    
    std::string empty_list_json = "[]";
    JsonToResReqPreloadInfoList(empty_list_json, prelaod_info_list, preconnect_limit_info_list, page_index);
}

// 测试 GetPreconnectLimitListKey 静态函数
void FuzzGetPreconnectLimitListKey(FuzzedDataProvider* fdp) {
    GURL url1("https://example.com");
    GURL url2("http://example.com");
    GURL url3("https://sub.example.com");
    GURL url4("https://example.com:8080");
    GURL url5(fdp->ConsumeRandomLengthString(kMaxStringLength));
    
    std::string key1, key2, key3, key4, key5;
    
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(url1, true, key1);
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(url1, false, key2);
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(url2, true, key3);
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(url4, false, key4);
    
    if (url5.is_valid()) {
        ResReqInfoCacheMgr::GetPreconnectLimitListKey(url5, fdp->ConsumeBool(), key5);
    }
}

// 测试 GetPreconnectOriginUrl 静态函数
void FuzzGetPreconnectOriginUrl(FuzzedDataProvider* fdp) {
    GURL url1("https://example.com/path/to/resource");
    GURL url2("preflight+https://example.com/api");
    GURL url3("http://example.com");
    
    auto info1 = std::make_shared<PRRequestInfo>(url1, false);
    auto info2 = std::make_shared<PRRequestInfo>(url2, false);
    info2->set_type(PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
    auto info3 = std::make_shared<PRRequestInfo>(url3, true);
    
    GURL origin1, origin2, origin3;
    
    ResReqInfoCacheMgr::GetPreconnectOriginUrl(info1, origin1);
    ResReqInfoCacheMgr::GetPreconnectOriginUrl(info2, origin2);
    ResReqInfoCacheMgr::GetPreconnectOriginUrl(info3, origin3);
    
    GURL custom_url(fdp->ConsumeRandomLengthString(kMaxStringLength));
    if (custom_url.is_valid()) {
        auto custom_info = std::make_shared<PRRequestInfo>(custom_url, fdp->ConsumeBool());
        custom_info->set_type(static_cast<PRRequestInfoType>(fdp->ConsumeIntegralInRange<int>(0, 3)));
        GURL custom_origin;
        ResReqInfoCacheMgr::GetPreconnectOriginUrl(custom_info, custom_origin);
    }
}

// 测试 IsRequestWithCookie 函数
void FuzzIsRequestWithCookie(FuzzedDataProvider* fdp) {
    auto info_without_cookie = CreatePRRequestInfo(fdp);
    
    auto info_with_cookie = std::make_shared<PRRequestInfo>(GURL("https://example.com"), false);
    net::HttpRequestHeaders headers;
    headers.SetHeader("Cookie", "session=abc123");
    info_with_cookie->set_extra_request_headers(headers);
    
    auto info_with_other_header = std::make_shared<PRRequestInfo>(GURL("https://example.com"), false);
    net::HttpRequestHeaders other_headers;
    other_headers.SetHeader("Authorization", "Bearer token");
    info_with_other_header->set_extra_request_headers(other_headers);
}

// 测试 UpdatePreconnectInfo 逻辑
void FuzzUpdatePreconnectInfoLogic(FuzzedDataProvider* fdp) {
    LinkedHashMap new_preconnect_limit_list;
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://example.com"), true);
    info1->set_is_preconnect(true);
    info1->set_is_reused_sokcet(fdp->ConsumeBool());
    
    GURL origin_url;
    std::string key;
    ResReqInfoCacheMgr::GetPreconnectOriginUrl(info1, origin_url);
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, info1->allow_credentials(), key);
    
    auto preconnect_limit_info = std::make_shared<PRRequestInfo>(origin_url, info1->allow_credentials());
    preconnect_limit_info->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    preconnect_limit_info->add_preconnect_num(info1->is_preconnect());
    preconnect_limit_info->add_reused_preconnect_num(info1->is_preconnect() && info1->is_reused_sokcet());
    preconnect_limit_info->compute_limit_num();
    new_preconnect_limit_list.emplace(key, preconnect_limit_info);
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://example.com"), true);
    info2->set_is_preconnect(true);
    info2->set_is_reused_sokcet(true);
    
    auto it = new_preconnect_limit_list.find(key);
    if (it != new_preconnect_limit_list.end()) {
        it->second->add_preconnect_num(info2->is_preconnect());
        it->second->add_reused_preconnect_num(info2->is_preconnect() && info2->is_reused_sokcet());
        it->second->compute_limit_num();
    }
    
    auto info3 = std::make_shared<PRRequestInfo>(GURL("https://example.com"), false);
    info3->set_is_preconnect(false);
    info3->set_is_reused_sokcet(false);
}

// 测试 UpdatePreloadInfo 逻辑
void FuzzUpdatePreloadInfoLogic(FuzzedDataProvider* fdp) {
    LinkedHashMap load_preload_info_list;
    LinkedHashMap new_preload_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, kMaxPageIndex);
    
    auto info1 = CreatePRRequestInfo(fdp);
    std::string key = info1->url().spec();
    
    auto existing_info = std::make_shared<PRRequestInfo>();
    existing_info->set_preload_seq_num("1, 2, 3, ");
    load_preload_info_list.insert({key, existing_info});
    
    auto it = load_preload_info_list.find(key);
    if (it != load_preload_info_list.end()) {
        std::string preload_seq_num = it->second->preload_seq_num();
        size_t last_comma_pos = preload_seq_num.rfind(',');
        if (last_comma_pos != std::string::npos) {
            size_t second_last_comma_pos = preload_seq_num.rfind(',', last_comma_pos - 1);
            if (second_last_comma_pos != std::string::npos) {
                preload_seq_num = preload_seq_num.substr(second_last_comma_pos + 1, last_comma_pos - second_last_comma_pos) + " ";
            }
        }
        info1->set_preload_seq_num(preload_seq_num + std::to_string(page_index) + ", ");
        load_preload_info_list.erase(it);
    } else {
        info1->set_preload_seq_num(std::to_string(page_index) + ", ");
    }
    new_preload_info_list.insert({key, info1});
    
    auto info2 = CreatePRRequestInfo(fdp);
    std::string new_key = "https://new.com";
    info2->set_preload_seq_num(std::to_string(page_index) + ", ");
    new_preload_info_list.insert({new_key, info2});
}

// 测试 URL 处理逻辑（username/password/ref）
void FuzzUrlCredentialHandling(FuzzedDataProvider* fdp) {
    std::string url_str;
    bool with_username = fdp->ConsumeBool();
    bool with_password = fdp->ConsumeBool();
    bool with_ref = fdp->ConsumeBool();
    
    if (with_username && with_password) {
        url_str = "http://user:pass@example.com/path";
    } else if (with_username) {
        url_str = "http://user@example.com/path";
    } else if (with_ref) {
        url_str = "https://example.com/path#ref";
    } else {
        url_str = "https://example.com/path";
    }
    
    GURL url(url_str);
    auto info = std::make_shared<PRRequestInfo>(url, fdp->ConsumeBool());
    info->set_type(static_cast<PRRequestInfoType>(fdp->ConsumeIntegralInRange<int>(0, 3)));
    
    if (info->url().has_username() || info->url().has_password() || info->url().has_ref()) {
        GURL origin_url;
        ResReqInfoCacheMgr::GetPreconnectOriginUrl(info, origin_url);
        info->set_url(origin_url);
        info->or_preload_flag(PRPP_FLAGS_UNSUPPORT);
    }
    
    std::string key = info->url().spec();
    if ((info->type() == PRRequestInfoType::TYPE_PAGE_PREFLIGHT) &&
         !key.starts_with(PRPP_PREFLIGHT_PREFIX)) {
        key = std::string(PRPP_PREFLIGHT_PREFIX) + key;
    }
}

// 测试 preflight URL 处理
void FuzzPreflightUrlHandling(FuzzedDataProvider* fdp) {
    GURL preflight_url("preflight+https://example.com/api");
    auto info = std::make_shared<PRRequestInfo>(preflight_url, false);
    info->set_type(PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
    
    GURL origin_url;
    ResReqInfoCacheMgr::GetPreconnectOriginUrl(info, origin_url);
    
    std::string key = info->url().spec();
    if (!key.starts_with(PRPP_PREFLIGHT_PREFIX)) {
        key = std::string(PRPP_PREFLIGHT_PREFIX) + key;
    }
    
    GURL normal_url("https://example.com/normal");
    auto normal_info = std::make_shared<PRRequestInfo>(normal_url, false);
    normal_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    
    GURL normal_origin;
    ResReqInfoCacheMgr::GetPreconnectOriginUrl(normal_info, normal_origin);
}

// 测试 limit_num <= 0 的分支
void FuzzLimitNumZeroNegative(FuzzedDataProvider* fdp) {
    LinkedHashMap preconnect_limit_list;
    base::Value::List list;
    
    auto info_zero = std::make_shared<PRRequestInfo>(GURL("https://zero.com"), true);
    info_zero->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    info_zero->set_limit_num(0);
    preconnect_limit_list.insert({info_zero->url().spec(), info_zero});
    
    auto info_negative = std::make_shared<PRRequestInfo>(GURL("https://negative.com"), true);
    info_negative->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    info_negative->set_limit_num(-1);
    preconnect_limit_list.insert({info_negative->url().spec(), info_negative});
    
    auto info_positive = std::make_shared<PRRequestInfo>(GURL("https://positive.com"), true);
    info_positive->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    info_positive->set_limit_num(fdp->ConsumeIntegralInRange<int64_t>(1, 100));
    preconnect_limit_list.insert({info_positive->url().spec(), info_positive});
    
    bool need_store = ParseResReqInfoList(preconnect_limit_list, list, PRPPreloadMode::PRECONNECT, 0, true);
}

// 测试 preload_flag 和 cache_type 分支
void FuzzPreloadFlagCacheTypeBranch(FuzzedDataProvider* fdp) {
    LinkedHashMap info_list;
    base::Value::List list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(3, kMaxPageIndex);
    
    auto info_visible_force = std::make_shared<PRRequestInfo>(GURL("https://visible.com"), false);
    info_visible_force->set_preload_flag(PRPP_FLAGS_VISIBLE);
    info_visible_force->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info_visible_force->set_preload_seq_num(std::to_string(page_index - 1) + ", ");
    info_list.insert({info_visible_force->url().spec(), info_visible_force});
    
    auto info_not_visible = std::make_shared<PRRequestInfo>(GURL("https://notvisible.com"), false);
    info_not_visible->set_preload_flag(PRPP_FLAGS_NONE);
    info_not_visible->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info_list.insert({info_not_visible->url().spec(), info_not_visible});
    
    auto info_not_force = std::make_shared<PRRequestInfo>(GURL("https://notforce.com"), false);
    info_not_force->set_preload_flag(PRPP_FLAGS_VISIBLE);
    info_not_force->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
    info_list.insert({info_not_force->url().spec(), info_not_force});
    
    auto info_dont_store = std::make_shared<PRRequestInfo>(GURL("https://dontstore.com"), false);
    info_dont_store->set_preload_flag(PRPP_FLAGS_VISIBLE);
    info_dont_store->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info_dont_store->set_preload_seq_num("0, "); // 不匹配 page_index-1
    info_list.insert({info_dont_store->url().spec(), info_dont_store});
    
    bool need_store = ParseResReqInfoList(info_list, list, PRPPreloadMode::PRELOAD, page_index, false);
}

// 测试 page_index < MIN_INDEX 分支
void FuzzPageIndexMinIndexBranch(FuzzedDataProvider* fdp) {
    auto info = std::make_shared<PRRequestInfo>(GURL("https://example.com"), false);
    
    int64_t page_index_low = fdp->ConsumeIntegralInRange<int64_t>(0, 2);
    bool result = DoNotNeedToStore(info, page_index_low);
    
    info->set_preload_seq_num("invalid_seq");
    result = DoNotNeedToStore(info, page_index_low);
    
    int64_t page_index_high = fdp->ConsumeIntegralInRange<int64_t>(3, kMaxPageIndex);
    result = DoNotNeedToStore(info, page_index_high);
}

// 测试完整的 JSON 解析流程
void FuzzCompleteJsonParsing(FuzzedDataProvider* fdp) {
    std::string json_with_all_types = R"json([
        {
            "request_info_type": 2,
            "page_index": "5",
            "page_origin": "https://example.com",
            "url": "https://origin.com"
        },
        {
            "request_info_type": 1,
            "url": "https://preconnect1.com",
            "allow_credentials": true
        },
        {
            "request_info_type": 1,
            "url": "https://preconnect2.com",
            "allow_credentials": false
        },
        {
            "request_info_type": 3,
            "url": "https://limit1.com",
            "allow_credentials": true,
            "limit_num": "10"
        },
        {
            "request_info_type": 3,
            "url": "https://limit2.com",
            "allow_credentials": false,
            "limit_num": "0"
        }
    ])json";
    
    LinkedHashMap prelaod_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = 0;
    
    JsonToResReqPreloadInfoList(json_with_all_types, prelaod_info_list, preconnect_limit_info_list, page_index);
    
    std::string json_invalid_request_type = R"json([
        {
            "request_info_type": 100,
            "page_index": "0"
        }
    ])json";
    JsonToResReqPreloadInfoList(json_invalid_request_type, prelaod_info_list, preconnect_limit_info_list, page_index);
}

// 测试 info_cache_loaded_cb 回调
void FuzzInfoCacheLoadedCallback(FuzzedDataProvider* fdp) {
    LinkedHashMap load_preload_info_list;
    LinkedHashMap load_preconnect_limit_list;
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    auto info1 = CreatePRRequestInfo(fdp);
    load_preload_info_list.insert({info1->url().spec(), info1});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://limit.com"), true);
    info2->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    info2->set_limit_num(fdp->ConsumeIntegralInRange<int64_t>(1, 100));
    std::string key;
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(info2->url(), info2->allow_credentials(), key);
    load_preconnect_limit_list.insert({key, info2});
    
    int64_t load_page_index = fdp->ConsumeIntegralInRange<int64_t>(1, kMaxPageIndex);
    std::string page_seq_num = (load_page_index == 1) ? 
        (std::to_string(load_page_index) + ", ") :
        (std::to_string(load_page_index - 1) + ", " + std::to_string(load_page_index) + ", ");
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap& load_info_list,
           const LinkedHashMap& preconnect_limit_info_list,
           const net::NetworkAnonymizationKey& networkAnonymizationKey,
           const std::string& page_seq_num) {});
    
    if (!callback.is_null()) {
        callback.Run(load_preload_info_list, load_preconnect_limit_list, networkAnonymizationKey, page_seq_num);
    }
}

// 测试 ResReqInfoCacheMgr 构造函数和 Start/Stop
void FuzzCacheMgrStartStop(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->Start();
    cache_mgr->Start();
    
    std::string page_origin = fdp->ConsumeRandomLengthString(64);
    cache_mgr->SetPageOrigin(page_origin);
    
    cache_mgr->Stop();
    cache_mgr->Stop();
}

// 测试 ResReqInfoCacheMgr 空回调
void FuzzCacheMgrNullCallback(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB null_cb;
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, null_cb));
    
    cache_mgr->Start();
    cache_mgr->Stop();
}

// 测试 ResReqInfoCacheMgr nullptr backend
void FuzzCacheMgrNullBackend(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, nullptr, callback));
    
    cache_mgr->Start();
    cache_mgr->Stop();
    cache_mgr->SetPageOrigin("test");
}

// 测试 UpdateResRequestInfo 各种场景
void FuzzUpdateResRequestInfo(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->Start();
    
    auto info1 = CreatePRRequestInfo(fdp);
    cache_mgr->UpdateResRequestInfo(info1);
    
    auto info2 = CreatePRRequestInfo(fdp);
    cache_mgr->UpdateResRequestInfo(info2);
    
    GURL url_with_user("http://user:pass@example.com/path");
    auto info_with_creds = std::make_shared<PRRequestInfo>(url_with_user, fdp->ConsumeBool());
    cache_mgr->UpdateResRequestInfo(info_with_creds);
    
    GURL preflight_url("preflight+https://example.com/api");
    auto preflight_info = std::make_shared<PRRequestInfo>(preflight_url, false);
    preflight_info->set_type(PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
    cache_mgr->UpdateResRequestInfo(preflight_info);
    
    cache_mgr->Stop();
}

// 测试 UpdateResRequestInfo 未启动状态
void FuzzUpdateResRequestInfoNotStarted(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    auto info = CreatePRRequestInfo(fdp);
    cache_mgr->UpdateResRequestInfo(info);
}

// 测试 UpdatePreconnectInfo 分支
void FuzzUpdatePreconnectInfoBranches(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->Start();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://preconnect1.com"), true);
    info1->set_is_preconnect(true);
    info1->set_is_reused_sokcet(false);
    cache_mgr->UpdateResRequestInfo(info1);
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://preconnect1.com"), true);
    info2->set_is_preconnect(true);
    info2->set_is_reused_sokcet(true);
    cache_mgr->UpdateResRequestInfo(info2);
    
    auto info3 = std::make_shared<PRRequestInfo>(GURL("https://preconnect2.com"), false);
    info3->set_is_preconnect(true);
    info3->set_is_reused_sokcet(fdp->ConsumeBool());
    cache_mgr->UpdateResRequestInfo(info3);
    
    auto info4 = std::make_shared<PRRequestInfo>(GURL("https://normal.com"), true);
    info4->set_is_preconnect(false);
    info4->set_is_reused_sokcet(false);
    cache_mgr->UpdateResRequestInfo(info4);
    
    cache_mgr->Stop();
}

// 测试 IsRequestWithCookie
void FuzzIsRequestWithCookieBranches(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->Start();
    
    auto info_with_cookie = std::make_shared<PRRequestInfo>(GURL("https://cookie.com"), true);
    net::HttpRequestHeaders headers;
    headers.SetHeader("Cookie", "session=abc123");
    info_with_cookie->set_extra_request_headers(headers);
    cache_mgr->UpdateResRequestInfo(info_with_cookie);
    
    auto info_without_cookie = std::make_shared<PRRequestInfo>(GURL("https://nocookie.com"), true);
    net::HttpRequestHeaders empty_headers;
    info_without_cookie->set_extra_request_headers(empty_headers);
    cache_mgr->UpdateResRequestInfo(info_without_cookie);
    
    cache_mgr->Stop();
}

// 测试 OnEntryLoadedCallback
void FuzzOnEntryLoadedCallback(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->Start();
    
    std::string entry_content = R"json([
        {"request_info_type": 2, "page_index": "1", "page_origin": "https://test.com"},
        {"request_info_type": 1, "url": "https://preload.com", "allow_credentials": true}
    ])json";
    cache_mgr->OnEntryLoadedCallback(entry_content);
    
    cache_mgr->OnEntryLoadedCallback("");
    cache_mgr->OnEntryLoadedCallback("invalid json");
    
    cache_mgr->Stop();
}

// 测试 OnEntryLoadedCallback 未启动状态
void FuzzOnEntryLoadedCallbackNotStarted(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    std::string entry_content = R"json([{"request_info_type": 2, "page_index": "1"}])json";
    cache_mgr->OnEntryLoadedCallback(entry_content);
}

// 测试 CheckFlush 方法
void FuzzCheckFlushMethod(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->Start();
    cache_mgr->SetPageOrigin(fdp->ConsumeRandomLengthString(64));
    
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(1, 5); ++i) {
        auto info = CreatePRRequestInfo(fdp);
        cache_mgr->UpdateResRequestInfo(info);
    }
    
    cache_mgr->CheckFlush(true);
    cache_mgr->CheckFlush(false);
    
    cache_mgr->Stop();
}

// 测试 SetPageOrigin
void FuzzSetPageOriginMethod(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->SetPageOrigin("not_started_origin");
    
    cache_mgr->Start();
    
    cache_mgr->SetPageOrigin(fdp->ConsumeRandomLengthString(64));
    cache_mgr->SetPageOrigin("");
    
    cache_mgr->Stop();
}

// 测试 Stop 方法的各种分支
void FuzzStopMethodBranches(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResRequestInfoCacheLoadedCB callback = base::BindRepeating(
        [](const LinkedHashMap&, const LinkedHashMap&,
           const net::NetworkAnonymizationKey&, const std::string&) {});
    
    std::string url = fdp->ConsumeRandomLengthString(kMaxStringLength);
    auto cache_mgr = base::WrapRefCounted(new ResReqInfoCacheMgr(
        url, networkAnonymizationKey, g_backend_factory, callback));
    
    cache_mgr->Stop();
    
    cache_mgr->Start();
    cache_mgr->SetPageOrigin(fdp->ConsumeRandomLengthString(64));
    
    auto info1 = CreatePRRequestInfo(fdp);
    cache_mgr->UpdateResRequestInfo(info1);
    
    cache_mgr->Stop();
    
    cache_mgr->Start();
    cache_mgr->Stop();
}

// 测试 ParseJson 各种分支
void FuzzParseJsonBranches(FuzzedDataProvider* fdp) {
    std::string json_empty_list = "[]";
    LinkedHashMap preload_list;
    LinkedHashMap limit_list;
    int64_t page_index = 0;
    JsonToResReqPreloadInfoList(json_empty_list, preload_list, limit_list, page_index);
    
    std::string json_no_origin_type = R"json([
        {"request_info_type": 1, "url": "https://test.com"}
    ])json";
    JsonToResReqPreloadInfoList(json_no_origin_type, preload_list, limit_list, page_index);
    
    std::string json_invalid_type = R"json([
        {"request_info_type": 0}
    ])json";
    JsonToResReqPreloadInfoList(json_invalid_type, preload_list, limit_list, page_index);
    
    std::string json_missing_page_index = R"json([
        {"request_info_type": 2}
    ])json";
    JsonToResReqPreloadInfoList(json_missing_page_index, preload_list, limit_list, page_index);
}

// 测试 ResReqPreloadInfoListToJson 各分支
void FuzzResReqPreloadInfoListToJsonBranches(FuzzedDataProvider* fdp) {
    LinkedHashMap preload_list;
    LinkedHashMap load_list;
    LinkedHashMap limit_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(0, kMaxPageIndex);
    std::string entry_content;
    
    bool result = ResReqPreloadInfoListToJson("", preload_list, load_list, limit_list, page_index, entry_content);
    
    preload_list.clear();
    load_list.clear();
    limit_list.clear();
    result = ResReqPreloadInfoListToJson(fdp->ConsumeRandomLengthString(64), preload_list, load_list, limit_list, page_index, entry_content);
    
    auto info = CreatePRRequestInfo(fdp);
    preload_list.insert({info->url().spec(), info});
    result = ResReqPreloadInfoListToJson("https://origin.com", preload_list, load_list, limit_list, page_index, entry_content);
}

} // namespace ohos_prp_preload

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if ((data == nullptr) || (size < 4)) {
        return 0;
    }

    static ohos_prp_preload::Environment env;
    FuzzedDataProvider fdp(data, size);
    size_t iterations = std::min(
        fdp.ConsumeIntegralInRange<size_t>(1, ohos_prp_preload::kMaxIterations), size);
    
    for (size_t i = 0; i < iterations && fdp.remaining_bytes() > 0; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 29);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzDoNotNeedToStore(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzParseResReqInfoList(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzResReqPreloadInfoListToJson(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzJsonToResReqPreloadInfoList(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzGetPreconnectLimitListKey(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzGetPreconnectOriginUrl(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzIsRequestWithCookie(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzUpdatePreconnectInfoLogic(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzUpdatePreloadInfoLogic(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzUrlCredentialHandling(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzPreflightUrlHandling(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzLimitNumZeroNegative(&fdp);
                break;
            case 12:
                ohos_prp_preload::FuzzPreloadFlagCacheTypeBranch(&fdp);
                break;
            case 13:
                ohos_prp_preload::FuzzPageIndexMinIndexBranch(&fdp);
                break;
            case 14:
                ohos_prp_preload::FuzzCompleteJsonParsing(&fdp);
                break;
            case 15:
                ohos_prp_preload::FuzzInfoCacheLoadedCallback(&fdp);
                break;
            case 16:
                ohos_prp_preload::FuzzCacheMgrStartStop(&fdp);
                break;
            case 17:
                ohos_prp_preload::FuzzCacheMgrNullCallback(&fdp);
                break;
            case 18:
                ohos_prp_preload::FuzzCacheMgrNullBackend(&fdp);
                break;
            case 19:
                ohos_prp_preload::FuzzUpdateResRequestInfo(&fdp);
                break;
            case 20:
                ohos_prp_preload::FuzzUpdateResRequestInfoNotStarted(&fdp);
                break;
            case 21:
                ohos_prp_preload::FuzzUpdatePreconnectInfoBranches(&fdp);
                break;
            case 22:
                ohos_prp_preload::FuzzIsRequestWithCookieBranches(&fdp);
                break;
            case 23:
                ohos_prp_preload::FuzzOnEntryLoadedCallback(&fdp);
                break;
            case 24:
                ohos_prp_preload::FuzzOnEntryLoadedCallbackNotStarted(&fdp);
                break;
            case 25:
                ohos_prp_preload::FuzzCheckFlushMethod(&fdp);
                break;
            case 26:
                ohos_prp_preload::FuzzSetPageOriginMethod(&fdp);
                break;
            case 27:
                ohos_prp_preload::FuzzStopMethodBranches(&fdp);
                break;
            case 28:
                ohos_prp_preload::FuzzParseJsonBranches(&fdp);
                break;
            case 29:
                ohos_prp_preload::FuzzResReqPreloadInfoListToJsonBranches(&fdp);
                break;
            default:
                break;
        }
    }
    return 0;
}