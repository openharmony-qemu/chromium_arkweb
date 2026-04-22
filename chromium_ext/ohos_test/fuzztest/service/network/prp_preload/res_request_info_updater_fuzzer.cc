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

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_request_headers.h"

#define private public
#define protected public
#include "arkweb/chromium_ext/services/network/prp_preload/src/res_request_info_updater.h"
#include "arkweb/chromium_ext/services/network/prp_preload/src/res_request_info_cache_mgr.h"
#include "arkweb/chromium_ext/services/network/prp_preload/include/page_res_parallel_preload_mgr.h"
#include "arkweb/chromium_ext/net/base/page_res_request_info.h"
#undef protected
#undef private

#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_backend_factory.h"

namespace ohos_prp_preload {

constexpr size_t kMaxIterations = 20;
constexpr size_t kMaxStringLen = 256;
constexpr int64_t kMaxTimeValue = 1000000000000LL;
constexpr int32_t kMaxLevelIntervalUs = 100000;
constexpr int32_t kMaxPreconnectCount = 6;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;
static scoped_refptr<DiskCacheBackendFactory> g_disk_cache_backend_factory = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::DEFAULT);
        if (!g_disk_cache_backend_factory) {
            g_disk_cache_backend_factory = base::WrapRefCounted(new DiskCacheBackendFactory());
        }
    }
    ~Environment() {
        task_environment_.reset();
        g_disk_cache_backend_factory.reset();
    }
};

std::shared_ptr<PRRequestInfo> CreatePRRequestInfo(FuzzedDataProvider* fdp) {
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLen);
    if (url_str.empty()) {
        url_str = "https://example.com/path";
    }
    GURL url(url_str);
    bool allow_credentials = fdp->ConsumeBool();
    
    auto info = std::make_shared<PRRequestInfo>(url, allow_credentials);
    
    int type_int = fdp->ConsumeIntegralInRange<int>(0, 3);
    info->set_type(static_cast<PRRequestInfoType>(type_int));
    
    if (fdp->ConsumeBool()) {
        info->set_method(fdp->ConsumeRandomLengthString(16));
    }
    if (fdp->ConsumeBool()) {
        info->set_cache_type(static_cast<PRRequestCacheType>(fdp->ConsumeIntegralInRange<int>(0, 2)));
    }
    if (fdp->ConsumeBool()) {
        info->set_freshness_life_times(fdp->ConsumeIntegralInRange<int64_t>(0, kMaxTimeValue));
    }
    if (fdp->ConsumeBool()) {
        info->set_preload_flag(static_cast<PRRequestFlags>(fdp->ConsumeIntegral<uint32_t>()));
    }
    if (fdp->ConsumeBool()) {
        info->set_preload_seq_num(fdp->ConsumeRandomLengthString(64));
    }
    if (fdp->ConsumeBool()) {
        info->set_request_start_time(fdp->ConsumeIntegralInRange<int64_t>(0, kMaxTimeValue));
    }
    if (fdp->ConsumeBool()) {
        info->set_request_end_time(fdp->ConsumeIntegralInRange<int64_t>(0, kMaxTimeValue));
    }
    if (fdp->ConsumeBool()) {
        std::set<std::string> dynamic_keys;
        for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(0, 5); ++i) {
            dynamic_keys.insert(fdp->ConsumeRandomLengthString(32));
        }
        info->set_dynamic_header_keys(dynamic_keys);
    }
    
    return info;
}

void FuzzStartScenario(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    updater.Start();
}

void FuzzUpdateResRequestInfoScenario(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxStringLen);
    GURL request_url(url_str);
    std::shared_ptr<PRRequestInfo> request_info = 
        std::make_shared<PRRequestInfo>(request_url, fdp->ConsumeBool());
    updater.UpdateResRequestInfo(request_info);
}

void FuzzStopScenario(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    updater.Stop();
}

void FuzzSetPageOriginScenario(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    std::string new_page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    updater.SetPageOrigin(new_page_origin);
}

void FuzzOnResRequestInfoCacheLoadedBasic(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = fdp->ConsumeRandomLengthString(64);
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    auto info1 = CreatePRRequestInfo(fdp);
    load_info_list.insert({info1->url().spec(), info1});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzOnResRequestInfoCacheLoadedEmptyCb(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB empty_cb;
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, empty_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzOnResRequestInfoCacheLoadedWithPreloadTree(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(1, 5); ++i) {
        auto info = CreatePRRequestInfo(fdp);
        info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
        info->set_freshness_life_times(base_time + fdp->ConsumeIntegralInRange<int64_t>(1000000, 100000000));
        info->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
        info->set_request_start_time(base_time + i * 50000);
        info->set_request_end_time(base_time + i * 50000 + 30000);
        info->set_type(PRRequestInfoType::TYPE_DEFAULT);
        load_info_list.insert({info->url().spec(), info});
    }
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreconnectListSafeMethod(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    info->set_method("GET");
    info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info->set_freshness_life_times(base::Time::Now().ToInternalValue() + 1000000000);
    load_info_list.insert({info->url().spec(), info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreconnectListUnsafeMethod(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://unsafe.com"), true);
    info->set_method("POST");
    load_info_list.insert({info->url().spec(), info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreconnectListNonForceCache(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://negotiation.com"), true);
    info->set_method("GET");
    info->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
    load_info_list.insert({info->url().spec(), info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreconnectListExpiredFreshness(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://expired.com"), true);
    info->set_method("GET");
    info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info->set_freshness_life_times(base::Time::Now().ToInternalValue() - 1000000);
    load_info_list.insert({info->url().spec(), info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreconnectListWithLimit(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    GURL origin_url("https://limit.com");
    std::string key;
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, true, key);
    
    auto limit_info = std::make_shared<PRRequestInfo>(origin_url, true);
    limit_info->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    int32_t limit_num = fdp->ConsumeIntegralInRange<int32_t>(1, kMaxPreconnectCount);
    limit_info->set_limit_num(limit_num);
    preconnect_limit_info_list.insert({key, limit_info});
    
    for (int i = 0; i < limit_num + 2; ++i) {
        auto info = std::make_shared<PRRequestInfo>(origin_url, true);
        info->set_method("POST");
        load_info_list.insert({info->url().spec() + "/" + std::to_string(i), info});
    }
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreconnectListExceedMax(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    GURL origin_url("https://exceed.com");
    std::string key;
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, true, key);
    
    auto limit_info = std::make_shared<PRRequestInfo>(origin_url, true);
    limit_info->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    limit_info->set_limit_num(100);
    preconnect_limit_info_list.insert({key, limit_info});
    
    for (int i = 0; i < 10; ++i) {
        auto info = std::make_shared<PRRequestInfo>(origin_url, true);
        info->set_method("POST");
        load_info_list.insert({info->url().spec() + "/" + std::to_string(i), info});
    }
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreloadTreeBasic(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://tree-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://tree1.com"), true);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info1->set_freshness_life_times(base_time + 100000000000);
    info1->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info1->set_request_start_time(base_time);
    info1->set_request_end_time(base_time + 50000);
    load_info_list.insert({info1->url().spec(), info1});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://tree2.com"), true);
    info2->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info2->set_freshness_life_times(base_time + 100000000000);
    info2->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info2->set_request_start_time(base_time + 60000);
    info2->set_request_end_time(base_time + 100000);
    load_info_list.insert({info2->url().spec(), info2});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreloadTreeNewLevel(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://level-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://level1.com"), true);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info1->set_freshness_life_times(base_time + 100000000000);
    info1->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info1->set_request_start_time(base_time);
    info1->set_request_end_time(base_time + 50000);
    load_info_list.insert({info1->url().spec(), info1});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://level2.com"), true);
    info2->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info2->set_freshness_life_times(base_time + 100000000000);
    info2->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info2->set_request_start_time(base_time + kMaxLevelIntervalUs + 1000);
    info2->set_request_end_time(base_time + kMaxLevelIntervalUs + 100000);
    load_info_list.insert({info2->url().spec(), info2});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreloadTreeSeqNumMismatch(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "5, ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://mismatch-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://mismatch1.com"), true);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info1->set_freshness_life_times(base_time + 100000000000);
    info1->set_preload_seq_num("1, 2, ");
    info1->set_request_start_time(base_time);
    info1->set_request_end_time(base_time + 50000);
    load_info_list.insert({info1->url().spec(), info1});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreloadTreeNonForceCache(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://nonforce-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://nonforce1.com"), true);
    info1->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
    info1->set_freshness_life_times(base_time + 100000000000);
    info1->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    load_info_list.insert({info1->url().spec(), info1});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreloadTreeExpiredFreshness(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://expired-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://expired1.com"), true);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info1->set_freshness_life_times(base_time - 1000000);
    info1->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    load_info_list.insert({info1->url().spec(), info1});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzUpdateResRequestInfoForDynamicHeaders(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://dynamic-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    net::HttpRequestHeaders headers;
    headers.SetHeader("X-Custom-Header", "value1");
    headers.SetHeader("Authorization", "Bearer token");
    
    auto parent_info = std::make_shared<PRRequestInfo>(GURL("https://dynamic-parent.com"), true);
    parent_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    parent_info->set_freshness_life_times(base_time + 100000000000);
    parent_info->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    parent_info->set_request_start_time(base_time);
    parent_info->set_request_end_time(base_time + 50000);
    load_info_list.insert({parent_info->url().spec(), parent_info});
    
    auto child_info = std::make_shared<PRRequestInfo>(GURL("https://dynamic-child.com"), true);
    child_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    child_info->set_freshness_life_times(base_time + 100000000000);
    child_info->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    child_info->set_request_start_time(base_time + 60000);
    child_info->set_request_end_time(base_time + 100000);
    child_info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
    std::set<std::string> dynamic_keys{"X-Custom-Header"};
    child_info->set_dynamic_header_keys(dynamic_keys);
    child_info->set_parent_for_dynamic_header(GURL("https://dynamic-parent.com"));
    load_info_list.insert({child_info->url().spec(), child_info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzIsDynamicHeadersMatch(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://match-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    net::HttpRequestHeaders headers;
    bool has_header = fdp->ConsumeBool();
    if (has_header) {
        headers.SetHeader("X-Match-Header", "match_value");
    }
    
    auto parent_info = std::make_shared<PRRequestInfo>(GURL("https://match-parent.com"), true);
    parent_info->set_cache_type(PRRequestCacheType::NEGOTIATION_CACHE);
    parent_info->set_extra_request_headers(headers);
    parent_info->set_freshness_life_times(base_time + 100000000000);
    parent_info->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    parent_info->set_request_start_time(base_time);
    parent_info->set_request_end_time(base_time + 50000);
    load_info_list.insert({parent_info->url().spec(), parent_info});
    
    auto child_info = std::make_shared<PRRequestInfo>(GURL("https://match-child.com"), true);
    child_info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    child_info->set_freshness_life_times(base_time + 100000000000);
    child_info->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    child_info->set_request_start_time(base_time + 60000);
    child_info->set_request_end_time(base_time + 100000);
    child_info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
    std::set<std::string> dynamic_keys;
    if (fdp->ConsumeBool()) {
        dynamic_keys.insert("X-Match-Header");
    }
    if (fdp->ConsumeBool()) {
        dynamic_keys.insert("X-NonExist-Header");
    }
    child_info->set_dynamic_header_keys(dynamic_keys);
    child_info->set_parent_for_dynamic_header(GURL("https://match-parent.com"));
    load_info_list.insert({child_info->url().spec(), child_info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreloadTreeLevelEndTime(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://endtime-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    int64_t first_end_time = base_time + 50000;
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://endtime1.com"), true);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info1->set_freshness_life_times(base_time + 100000000000);
    info1->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info1->set_request_start_time(base_time);
    info1->set_request_end_time(first_end_time);
    load_info_list.insert({info1->url().spec(), info1});
    
    int64_t second_end_time = fdp->ConsumeBool() ? 
        (first_end_time - 10000) : (first_end_time + 10000);
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://endtime2.com"), true);
    info2->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info2->set_freshness_life_times(base_time + 100000000000);
    info2->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info2->set_request_start_time(base_time + 40000);
    info2->set_request_end_time(second_end_time);
    load_info_list.insert({info2->url().spec(), info2});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreloadTreeZeroEndTime(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://zeroend-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://zeroend1.com"), true);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info1->set_freshness_life_times(base_time + 100000000000);
    info1->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info1->set_request_start_time(base_time);
    info1->set_request_end_time(0);
    load_info_list.insert({info1->url().spec(), info1});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://zeroend2.com"), true);
    info2->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info2->set_freshness_life_times(base_time + 100000000000);
    info2->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info2->set_request_start_time(base_time + 40000);
    info2->set_request_end_time(0);
    load_info_list.insert({info2->url().spec(), info2});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzBuildPreconnectListExistingKey(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    GURL origin_url("https://existing.com");
    for (int i = 0; i < 5; ++i) {
        auto info = std::make_shared<PRRequestInfo>(origin_url, true);
        info->set_method("POST");
        load_info_list.insert({origin_url.spec() + "/" + std::to_string(i), info});
    }
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzNullResReqInfoCacheMgr(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        nullptr, preload_infos_cb);
    
    updater.Start();
    updater.Stop();
    updater.SetPageOrigin("test");
    updater.UpdateResRequestInfo(std::make_shared<PRRequestInfo>());
}

void FuzzPreflightUrlHandling(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    auto preflight_info = std::make_shared<PRRequestInfo>(
        GURL("preflight+https://preflight.com/api"), false);
    preflight_info->set_type(PRRequestInfoType::TYPE_PAGE_PREFLIGHT);
    preflight_info->set_method("OPTIONS");
    load_info_list.insert({preflight_info->url().spec(), preflight_info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzAllowCredentialsVariation(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    GURL origin_url("https://creds.com");
    bool allow_creds = fdp->ConsumeBool();
    
    auto info = std::make_shared<PRRequestInfo>(origin_url, allow_creds);
    info->set_method("POST");
    load_info_list.insert({origin_url.spec(), info});
    
    std::string key;
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, allow_creds, key);
    
    auto limit_info = std::make_shared<PRRequestInfo>(origin_url, allow_creds);
    limit_info->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    limit_info->set_limit_num(3);
    preconnect_limit_info_list.insert({key, limit_info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzMultiplePreconnectSameOrigin(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    GURL origin_url("https://multi.com");
    std::string key;
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, true, key);
    
    auto limit_info = std::make_shared<PRRequestInfo>(origin_url, true);
    limit_info->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    limit_info->set_preconnect_num(4);
    limit_info->compute_limit_num();
    preconnect_limit_info_list.insert({key, limit_info});
    
    for (int i = 0; i < 8; ++i) {
        auto info = std::make_shared<PRRequestInfo>(origin_url, true);
        info->set_method("POST");
        load_info_list.insert({origin_url.spec() + "/" + std::to_string(i), info});
    }
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzNewLevelByEndTime(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://newlevel-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://newlevel1.com"), true);
    info1->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info1->set_freshness_life_times(base_time + 100000000000);
    info1->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info1->set_request_start_time(base_time);
    info1->set_request_end_time(base_time + 30000);
    load_info_list.insert({info1->url().spec(), info1});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://newlevel2.com"), true);
    info2->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info2->set_freshness_life_times(base_time + 100000000000);
    info2->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info2->set_request_start_time(base_time + 50000);
    info2->set_request_end_time(base_time + 100000);
    load_info_list.insert({info2->url().spec(), info2});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzEmptyPreloadTree(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzOnlyOriginInfo(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://only-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzDynamicHeadersNoParent(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    int64_t page_index = fdp->ConsumeIntegralInRange<int64_t>(1, 10);
    std::string page_seq_num = std::to_string(page_index) + ", ";
    
    auto origin_info = std::make_shared<PRRequestInfo>(GURL("https://noparent-origin.com"), true);
    origin_info->set_type(PRRequestInfoType::TYPE_PAGE_ORIGIN);
    load_info_list.insert({origin_info->url().spec(), origin_info});
    
    int64_t base_time = base::Time::Now().ToInternalValue();
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://noparent.com"), true);
    info->set_cache_type(PRRequestCacheType::FORCE_CACHE);
    info->set_freshness_life_times(base_time + 100000000000);
    info->set_preload_seq_num(std::to_string(page_index - 1) + ", " + page_seq_num);
    info->set_request_start_time(base_time);
    info->set_request_end_time(base_time + 50000);
    info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
    std::set<std::string> dynamic_keys{"X-Test"};
    info->set_dynamic_header_keys(dynamic_keys);
    info->set_parent_for_dynamic_header(GURL("https://nonexist-parent.com"));
    load_info_list.insert({info->url().spec(), info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

void FuzzPreconnectWithZeroPreconnectNum(FuzzedDataProvider* fdp) {
    net::NetworkAnonymizationKey networkAnonymizationKey;
    
    ResPreloadInfosCB preload_infos_cb = base::BindRepeating(
        [](const PRPPPreconnectInfoList&, const std::shared_ptr<PRPPReqInfoTreeNode>&,
           const std::set<std::string>&){});
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxStringLen);
    ResRequestInfoUpdater updater(page_origin, networkAnonymizationKey, 
        g_disk_cache_backend_factory, preload_infos_cb);
    
    LinkedHashMap load_info_list;
    LinkedHashMap preconnect_limit_info_list;
    std::string page_seq_num = "1, ";
    
    GURL origin_url("https://zero-preconnect.com");
    std::string key;
    ResReqInfoCacheMgr::GetPreconnectLimitListKey(origin_url, true, key);
    
    auto limit_info = std::make_shared<PRRequestInfo>(origin_url, true);
    limit_info->set_type(PRRequestInfoType::TYPE_PAGE_PRECONNECT_LIMIT);
    limit_info->set_preconnect_num(0);
    limit_info->compute_limit_num();
    preconnect_limit_info_list.insert({key, limit_info});
    
    auto info = std::make_shared<PRRequestInfo>(origin_url, true);
    info->set_method("POST");
    load_info_list.insert({origin_url.spec(), info});
    
    updater.OnResRequestInfoCacheLoaded(load_info_list, preconnect_limit_info_list, 
        networkAnonymizationKey, page_seq_num);
}

} // namespace ohos_prp_preload

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if ((data == nullptr) || (size < 4)) {
        return 0;
    }

    base::CommandLine::Init(0, nullptr);
    static ohos_prp_preload::Environment env;
    FuzzedDataProvider fdp(data, size);
    size_t iterations = std::min(
        fdp.ConsumeIntegralInRange<size_t>(1, ohos_prp_preload::kMaxIterations), size);
    
    for (size_t i = 0; i < iterations && fdp.remaining_bytes() > 0; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 30);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzStartScenario(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzUpdateResRequestInfoScenario(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzStopScenario(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzSetPageOriginScenario(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzOnResRequestInfoCacheLoadedBasic(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzOnResRequestInfoCacheLoadedEmptyCb(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzOnResRequestInfoCacheLoadedWithPreloadTree(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzBuildPreconnectListSafeMethod(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzBuildPreconnectListUnsafeMethod(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzBuildPreconnectListNonForceCache(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzBuildPreconnectListExpiredFreshness(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzBuildPreconnectListWithLimit(&fdp);
                break;
            case 12:
                ohos_prp_preload::FuzzBuildPreconnectListExceedMax(&fdp);
                break;
            case 13:
                ohos_prp_preload::FuzzBuildPreloadTreeBasic(&fdp);
                break;
            case 14:
                ohos_prp_preload::FuzzBuildPreloadTreeNewLevel(&fdp);
                break;
            case 15:
                ohos_prp_preload::FuzzBuildPreloadTreeSeqNumMismatch(&fdp);
                break;
            case 16:
                ohos_prp_preload::FuzzBuildPreloadTreeNonForceCache(&fdp);
                break;
            case 17:
                ohos_prp_preload::FuzzBuildPreloadTreeExpiredFreshness(&fdp);
                break;
            case 18:
                ohos_prp_preload::FuzzUpdateResRequestInfoForDynamicHeaders(&fdp);
                break;
            case 19:
                ohos_prp_preload::FuzzIsDynamicHeadersMatch(&fdp);
                break;
            case 20:
                ohos_prp_preload::FuzzBuildPreloadTreeLevelEndTime(&fdp);
                break;
            case 21:
                ohos_prp_preload::FuzzBuildPreloadTreeZeroEndTime(&fdp);
                break;
            case 22:
                ohos_prp_preload::FuzzBuildPreconnectListExistingKey(&fdp);
                break;
            case 23:
                ohos_prp_preload::FuzzNullResReqInfoCacheMgr(&fdp);
                break;
            case 24:
                ohos_prp_preload::FuzzPreflightUrlHandling(&fdp);
                break;
            case 25:
                ohos_prp_preload::FuzzAllowCredentialsVariation(&fdp);
                break;
            case 26:
                ohos_prp_preload::FuzzMultiplePreconnectSameOrigin(&fdp);
                break;
            case 27:
                ohos_prp_preload::FuzzNewLevelByEndTime(&fdp);
                break;
            case 28:
                ohos_prp_preload::FuzzEmptyPreloadTree(&fdp);
                break;
            case 29:
                ohos_prp_preload::FuzzOnlyOriginInfo(&fdp);
                break;
            case 30:
                ohos_prp_preload::FuzzDynamicHeadersNoParent(&fdp);
                break;
            default:
                break;
        }
    }
    return 0;
}