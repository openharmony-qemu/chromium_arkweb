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
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/public/cpp/resource_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/url_loader_context_for_tests.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

#define private public
#define protected public
#include "arkweb/chromium_ext/services/network/prp_preload/src/preload_runner/prpp_request_loader_factory_impl.h"
#include "arkweb/chromium_ext/net/base/page_res_request_info.h"
#undef protected
#undef private

namespace ohos_prp_preload {

constexpr size_t kMaxIterations = 20;
constexpr size_t kMaxUrlLen = 256;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY);
    }
    ~Environment() {
        task_environment_.reset();
    }
};

void RunUntilIdle() {
    if (task_environment_) {
        task_environment_->RunUntilIdle();
    }
}

const char* kValidHeaderNames[] = {
    "Accept", "Accept-Encoding", "Accept-Language", "Authorization",
    "Cache-Control", "Content-Type", "Cookie", "Host",
    "If-None-Match", "Origin", "Referer", "User-Agent",
    "X-Custom-Header", "X-Request-Id", "X-Auth-Token", "X-Session-Id"
};

const char* kValidHeaderValues[] = {
    "application/json", "gzip, deflate", "en-US", "Bearer token123",
    "no-cache", "text/html", "session=abc123", "example.com",
    "etag123", "https://origin.com", "https://referer.com", "Mozilla/5.0",
    "custom-value", "req-123", "auth-token-value", "session-id-123"
};

std::shared_ptr<PRRequestInfo> CreatePRRequestInfo(FuzzedDataProvider* fdp) {
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    if (url_str.empty()) {
        url_str = "https://example.com";
    }
    GURL url(url_str);
    auto info = std::make_shared<PRRequestInfo>(url, fdp->ConsumeBool());
    
    if (fdp->ConsumeBool()) {
        std::string method = fdp->ConsumeBool() ? "GET" : "POST";
        info->set_method(method);
    }
    if (fdp->ConsumeBool()) {
        info->set_cache_type(static_cast<PRRequestCacheType>(fdp->ConsumeIntegralInRange<int>(0, 2)));
    }
    if (fdp->ConsumeBool()) {
        info->set_preload_flag(static_cast<PRRequestFlags>(fdp->ConsumeIntegral<uint32_t>()));
    }
    if (fdp->ConsumeBool()) {
        net::HttpRequestHeaders headers;
        size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
        for (size_t i = 0; i < header_count; ++i) {
            size_t name_idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(kValidHeaderNames)/sizeof(kValidHeaderNames[0]) - 1);
            size_t value_idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(kValidHeaderValues)/sizeof(kValidHeaderValues[0]) - 1);
            headers.SetHeader(kValidHeaderNames[name_idx], kValidHeaderValues[value_idx]);
        }
        info->set_extra_request_headers(headers);
    }
    if (fdp->ConsumeBool()) {
        std::set<std::string> dynamic_keys;
        size_t key_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
        for (size_t i = 0; i < key_count; ++i) {
            size_t name_idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(kValidHeaderNames)/sizeof(kValidHeaderNames[0]) - 1);
            dynamic_keys.insert(kValidHeaderNames[name_idx]);
        }
        info->set_dynamic_header_keys(dynamic_keys);
    }
    if (fdp->ConsumeBool()) {
        info->set_parent_for_dynamic_header(GURL(fdp->ConsumeRandomLengthString(kMaxUrlLen)));
    }
    
    return info;
}

void FuzzCreateReqLoaderAndStartBasic(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::shared_ptr<PRRequestInfo> info = CreatePRRequestInfo(fdp);
    
    std::set<std::string> need_record_header_urls;
    size_t header_count = fdp->ConsumeIntegralInRange<size_t>(0, 5);
    for (size_t i = 0; i < header_count; ++i) {
        need_record_header_urls.insert(fdp->ConsumeRandomLengthString(kMaxUrlLen));
    }
    
    factory.CreateReqLoaderAndStart(info, need_record_header_urls);
}

void FuzzCreateReqLoaderAndStartNullContext(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    
    PRPPRequestLoaderFactoryImpl factory(main_url, base::WeakPtr<net::URLRequestContext>());
    
    std::shared_ptr<PRRequestInfo> info = CreatePRRequestInfo(fdp);
    
    factory.CreateReqLoaderAndStart(info, {});
}

void FuzzCreateReqLoaderAndStartNullInfo(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    factory.CreateReqLoaderAndStart(nullptr, {});
}

void FuzzCreateReqLoaderAndStartDuplicateUrl(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string dup_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto info1 = std::make_shared<PRRequestInfo>(GURL(dup_url), true);
    auto info2 = std::make_shared<PRRequestInfo>(GURL(dup_url), true);
    
    factory.CreateReqLoaderAndStart(info1, {});
    factory.CreateReqLoaderAndStart(info2, {});
}

void FuzzCreateReqLoaderAndStartDynamicHeaders(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string parent_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::string child_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    
    auto info = std::make_shared<PRRequestInfo>(GURL(child_url), true);
    info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
    info->set_parent_for_dynamic_header(GURL(parent_url));
    std::set<std::string> dynamic_keys{"X-Auth-Token"};
    info->set_dynamic_header_keys(dynamic_keys);
    
    net::HttpRequestHeaders headers;
    headers.SetHeader("X-Auth-Token", "value");
    
    std::set<std::string> need_record_header_urls;
    need_record_header_urls.insert(parent_url);
    
    factory.CreateReqLoaderAndStart(info, need_record_header_urls);
}

void FuzzSetPRPPIsolationValid(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    url::Origin origin = url::Origin::Create(GURL(url_str));
    net::IsolationInfo isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin, origin, net::SiteForCookies());
    
    factory.SetPRPPIsolation(isolation_info);
}

void FuzzSetPRPPIsolationEmpty(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    net::IsolationInfo empty_isolation;
    
    factory.SetPRPPIsolation(empty_isolation);
}

void FuzzGetPRPPReqLoaderNotFound(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::shared_ptr<PRRequestInfo> info = CreatePRRequestInfo(fdp);
    network::URLLoaderContextForTests url_loader_context_for_tests;
    network::ResourceRequest resource_request;
    
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    resource_request.url = GURL(url_str);
    
    auto loader = factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request, info);
}

void FuzzGetPRPPReqLoaderUnsafeMethod(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::shared_ptr<PRRequestInfo> info = CreatePRRequestInfo(fdp);
    network::URLLoaderContextForTests url_loader_context_for_tests;
    network::ResourceRequest resource_request;
    
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    resource_request.url = GURL(url_str);
    resource_request.method = "POST";
    
    auto loader = factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request, info);
}

void FuzzGetPRPPReqLoaderPreflight(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::shared_ptr<PRRequestInfo> info = CreatePRRequestInfo(fdp);
    network::URLLoaderContextForTests url_loader_context_for_tests;
    network::ResourceRequest resource_request;
    
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    resource_request.url = GURL(url_str);
    resource_request.is_preflight = true;
    
    auto loader = factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request, info);
}

void FuzzGetPRPPReqLoaderIsolationMismatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    url::Origin origin1 = url::Origin::Create(GURL("https://origin1.com"));
    url::Origin origin2 = url::Origin::Create(GURL("https://origin2.com"));
    
    net::IsolationInfo isolation_info1 = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin1, origin1, net::SiteForCookies());
    
    factory.SetPRPPIsolation(isolation_info1);
    
    std::shared_ptr<PRRequestInfo> info = CreatePRRequestInfo(fdp);
    network::URLLoaderContextForTests url_loader_context_for_tests;
    network::ResourceRequest resource_request;
    
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    resource_request.url = GURL(url_str);
    
    auto loader = factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request, info);
}

void FuzzGetPRPPReqLoaderWithRequestBody(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::shared_ptr<PRRequestInfo> info = CreatePRRequestInfo(fdp);
    network::URLLoaderContextForTests url_loader_context_for_tests;
    network::ResourceRequest resource_request;
    
    std::string url_str = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    resource_request.url = GURL(url_str);
    resource_request.request_body = base::MakeRefCounted<network::ResourceRequestBody>();
    
    auto loader = factory.GetPRPPReqLoader(url_loader_context_for_tests, resource_request, info);
}

void FuzzIsInfoMatchedNullReqInfo(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::shared_ptr<PRRequestInfo> null_info;
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::URLLoaderContextForTests context;
    network::ResourceRequest request;
    
    factory.IsInfoMatched(null_info, context, request, binding_info);
}

void FuzzIsInfoMatchedMethodMismatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    req_info->set_method("GET");
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::URLLoaderContextForTests context;
    network::ResourceRequest request;
    request.method = "POST";
    
    factory.IsInfoMatched(req_info, context, request, binding_info);
}

void FuzzMatchRequestHeadersSizeMismatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("Accept", "application/json");
    info_headers.SetHeader("Authorization", "Bearer token");
    req_info->set_extra_request_headers(info_headers);
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::ResourceRequest request;
    request.headers.SetHeader("Accept", "application/json");
    
    factory.MatchRequestHeaders(req_info, request, binding_info);
}

void FuzzMatchRequestHeadersKeyMismatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("Accept", "application/json");
    req_info->set_extra_request_headers(info_headers);
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::ResourceRequest request;
    request.headers.SetHeader("Content-Type", "text/html");
    
    factory.MatchRequestHeaders(req_info, request, binding_info);
}

void FuzzMatchRequestHeadersValueMismatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("Accept", "application/json");
    info_headers.SetHeader("Authorization", "Bearer old_token");
    req_info->set_extra_request_headers(info_headers);
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::ResourceRequest request;
    request.headers.SetHeader("Accept", "application/json");
    request.headers.SetHeader("Authorization", "Bearer new_token");
    
    factory.MatchRequestHeaders(req_info, request, binding_info);
}

void FuzzMatchRequestHeadersMatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("Accept", "application/json");
    info_headers.SetHeader("Authorization", "Bearer token");
    req_info->set_extra_request_headers(info_headers);
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::ResourceRequest request;
    request.headers.SetHeader("Accept", "application/json");
    request.headers.SetHeader("Authorization", "Bearer token");
    
    factory.MatchRequestHeaders(req_info, request, binding_info);
}

void FuzzReplaceHeadersScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    net::HttpRequestHeaders record_headers;
    record_headers.SetHeader("X-Auth-Token", "new_token_value");
    record_headers.SetHeader("X-Session-Id", "session123");
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("X-Auth-Token", "old_token_value");
    info_headers.SetHeader("Content-Type", "text/html");
    info->set_extra_request_headers(info_headers);
    
    std::set<std::string> dynamic_keys{"X-Auth-Token"};
    info->set_dynamic_header_keys(dynamic_keys);
    
    factory.ReplaceHeaders(record_headers, info);
}

void FuzzRecordPendingPRPPLoaderScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string parent_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://child1.com"), true);
    info1->set_parent_for_dynamic_header(GURL(parent_url));
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://child2.com"), true);
    info2->set_parent_for_dynamic_header(GURL(parent_url));
    
    factory.RecordPendingPRPPLoader(info1);
    factory.RecordPendingPRPPLoader(info2);
}

void FuzzDoPendingCreateTaskScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string key = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    
    factory.DoPendingCreateTask(key);
    factory.DoPendingCreateTask("non_exist_key");
}

void FuzzRecordNeedRecordHeaderUrlsScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string record_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    
    std::set<std::string> need_record_urls;
    need_record_urls.insert(record_url);
    
    auto info = std::make_shared<PRRequestInfo>(GURL(record_url), true);
    factory.CreateReqLoaderAndStart(info, need_record_urls);
    
    network::URLLoaderContextForTests context;
    network::ResourceRequest request;
    request.url = GURL(record_url);
    
    factory.RecordNeedRecordHeaderUrls(request);
}

void FuzzNeedRecordHeaderUrlsEmpty(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    
    std::set<std::string> empty_urls;
    factory.CreateReqLoaderAndStart(info, empty_urls);
    
    std::set<std::string> non_empty_urls;
    non_empty_urls.insert("https://record.com");
    factory.CreateReqLoaderAndStart(info, non_empty_urls);
}

void FuzzOnResPreloadedScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    factory.OnResPreloaded(nullptr);
}

void FuzzGetWeakScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto weak_ptr = factory.GetWeak();
}

void FuzzGetMainUrlScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    const std::string& url = factory.GetMainUrl();
}

void FuzzCreatePendingReqLoaderAndStartScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto info = CreatePRRequestInfo(fdp);
    
    factory.CreatePendingReqLoaderAndStart(info);
}

void FuzzCreatePendingReqLoaderAndStartNullContext(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    
    PRPPRequestLoaderFactoryImpl factory(main_url, base::WeakPtr<net::URLRequestContext>());
    
    auto info = CreatePRRequestInfo(fdp);
    
    factory.CreatePendingReqLoaderAndStart(info);
}

void FuzzMultipleCreateAndGet(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(1, 5); ++i) {
        auto info = CreatePRRequestInfo(fdp);
        factory.CreateReqLoaderAndStart(info, {});
    }
    
    network::URLLoaderContextForTests context;
    network::ResourceRequest request;
    request.url = GURL(fdp->ConsumeRandomLengthString(kMaxUrlLen));
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    factory.GetPRPPReqLoader(context, request, binding_info);
}

void FuzzHeaderCaseInsensitive(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("Accept", "application/json");
    req_info->set_extra_request_headers(info_headers);
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::ResourceRequest request;
    request.headers.SetHeader("Accept", "application/json");
    
    factory.MatchRequestHeaders(req_info, request, binding_info);
}

void FuzzEmptyDynamicHeaderKeys(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    net::HttpRequestHeaders record_headers;
    record_headers.SetHeader("X-Test", "value");
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    info->set_dynamic_header_keys({});
    
    factory.ReplaceHeaders(record_headers, info);
}

void FuzzCanReuseHeadersMapScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string parent_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    
    std::set<std::string> need_record_urls;
    need_record_urls.insert(parent_url);
    
    auto parent_info = std::make_shared<PRRequestInfo>(GURL(parent_url), true);
    factory.CreateReqLoaderAndStart(parent_info, need_record_urls);
    
    auto child_info = std::make_shared<PRRequestInfo>(GURL("https://child.com"), true);
    child_info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
    child_info->set_parent_for_dynamic_header(GURL(parent_url));
    std::set<std::string> dynamic_keys{"X-Test"};
    child_info->set_dynamic_header_keys(dynamic_keys);
    
    factory.CreateReqLoaderAndStart(child_info, {});
}

void FuzzMatchRequestHeadersEmpty(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::ResourceRequest request;
    
    factory.MatchRequestHeaders(req_info, request, binding_info);
}

void FuzzMatchRequestHeadersBothEmpty(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    req_info->set_extra_request_headers(net::HttpRequestHeaders());
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::ResourceRequest request;
    
    factory.MatchRequestHeaders(req_info, request, binding_info);
}

void FuzzReplaceHeadersNoMatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    net::HttpRequestHeaders record_headers;
    record_headers.SetHeader("X-Other", "value");
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("X-Auth-Token", "old_value");
    info->set_extra_request_headers(info_headers);
    
    std::set<std::string> dynamic_keys{"X-Auth-Token"};
    info->set_dynamic_header_keys(dynamic_keys);
    
    factory.ReplaceHeaders(record_headers, info);
}

void FuzzReplaceHeadersAllMatch(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    net::HttpRequestHeaders record_headers;
    record_headers.SetHeader("X-Auth-Token", "new_value");
    
    auto info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    net::HttpRequestHeaders info_headers;
    info_headers.SetHeader("X-Auth-Token", "old_value");
    info->set_extra_request_headers(info_headers);
    
    std::set<std::string> dynamic_keys{"X-Auth-Token"};
    info->set_dynamic_header_keys(dynamic_keys);
    
    factory.ReplaceHeaders(record_headers, info);
}

void FuzzPendingPrppLoaderListScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string parent_url = "https://parent.com";
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://child1.com"), true);
    info1->set_parent_for_dynamic_header(GURL(parent_url));
    factory.RecordPendingPRPPLoader(info1);
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://child2.com"), true);
    info2->set_parent_for_dynamic_header(GURL(parent_url));
    factory.RecordPendingPRPPLoader(info2);
    
    factory.DoPendingCreateTask(parent_url);
    
    RunUntilIdle();
}

void FuzzIsInfoMatchedNoRequestBody(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto req_info = std::make_shared<PRRequestInfo>(GURL("https://test.com"), true);
    req_info->set_method("GET");
    
    net::HttpRequestHeaders headers;
    headers.SetHeader("Accept", "application/json");
    req_info->set_extra_request_headers(headers);
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    network::URLLoaderContextForTests context;
    network::ResourceRequest request;
    request.method = "GET";
    request.headers.SetHeader("Accept", "application/json");
    
    factory.IsInfoMatched(req_info, context, request, binding_info);
}

void FuzzRequestsAlreadyStartSetScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string url1 = "https://url1.com";
    std::string url2 = "https://url2.com";
    
    factory.requests_already_start_set_.insert(url1);
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL(url1), true);
    factory.CreateReqLoaderAndStart(info1, {});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL(url2), true);
    factory.CreateReqLoaderAndStart(info2, {});
}

void FuzzCanReuseHeadersMapWithHeaders(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::string parent_url = "https://parent.com";
    
    net::HttpRequestHeaders stored_headers;
    stored_headers.SetHeader("X-Auth-Token", "stored_token");
    factory.can_reuse_headers_map_[parent_url] = stored_headers;
    
    auto child_info = std::make_shared<PRRequestInfo>(GURL("https://child.com"), true);
    child_info->set_preload_flag(PRPP_FLAGS_HDR_DYNAMIC);
    child_info->set_parent_for_dynamic_header(GURL(parent_url));
    std::set<std::string> dynamic_keys{"X-Auth-Token"};
    child_info->set_dynamic_header_keys(dynamic_keys);
    
    net::HttpRequestHeaders child_headers;
    child_headers.SetHeader("X-Auth-Token", "old_token");
    child_info->set_extra_request_headers(child_headers);
    
    factory.CreateReqLoaderAndStart(child_info, {});
}

void FuzzPrppReqLoadersMapScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto info1 = std::make_shared<PRRequestInfo>(GURL("https://url1.com"), true);
    factory.CreateReqLoaderAndStart(info1, {});
    
    auto info2 = std::make_shared<PRRequestInfo>(GURL("https://url2.com"), true);
    factory.CreateReqLoaderAndStart(info2, {});
    
    network::URLLoaderContextForTests context;
    network::ResourceRequest request1;
    request1.url = GURL("https://url1.com");
    request1.method = "GET";
    
    std::shared_ptr<PRRequestInfo> binding_info = std::make_shared<PRRequestInfo>();
    factory.GetPRPPReqLoader(context, request1, binding_info);
}

void FuzzNeedRecordHeaderUrlsMultiple(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    std::set<std::string> urls;
    for (int i = 0; i < fdp->ConsumeIntegralInRange<int>(1, 5); ++i) {
        urls.insert("https://url" + std::to_string(i) + ".com");
    }
    factory.need_record_header_urls_ = urls;
    
    auto info = CreatePRRequestInfo(fdp);
    factory.CreateReqLoaderAndStart(info, urls);
}

void FuzzEmptyMainUrlScenario(FuzzedDataProvider* fdp) {
    std::string main_url = "";
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    auto info = CreatePRRequestInfo(fdp);
    factory.CreateReqLoaderAndStart(info, {});
}

void FuzzIsolationInfoSetScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    
    PRPPRequestLoaderFactoryImpl factory(main_url, url_request_context->GetWeakPtr());
    
    url::Origin origin = url::Origin::Create(GURL("https://test.com"));
    net::IsolationInfo isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin, origin, net::SiteForCookies());
    
    factory.SetPRPPIsolation(isolation_info);
    
    auto info = CreatePRRequestInfo(fdp);
    factory.CreateReqLoaderAndStart(info, {});
}

} //namespace ohos_prp_preload

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if ((data == nullptr) || (size == 0)) {
        return 0;
    }

    base::CommandLine::Init(0, nullptr);
    static ohos_prp_preload::Environment env;
    FuzzedDataProvider fdp(data, size);
    size_t iterations = std::min(fdp.ConsumeIntegralInRange<size_t>(1, ohos_prp_preload::kMaxIterations), size);
    
    for (size_t i = 0; i < iterations && fdp.remaining_bytes() > 0; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 37);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartBasic(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartNullContext(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartNullInfo(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartDuplicateUrl(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzCreateReqLoaderAndStartDynamicHeaders(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzSetPRPPIsolationValid(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzSetPRPPIsolationEmpty(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzGetPRPPReqLoaderNotFound(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzGetPRPPReqLoaderUnsafeMethod(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzGetPRPPReqLoaderPreflight(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzGetPRPPReqLoaderIsolationMismatch(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzGetPRPPReqLoaderWithRequestBody(&fdp);
                break;
            case 12:
                ohos_prp_preload::FuzzIsInfoMatchedNullReqInfo(&fdp);
                break;
            case 13:
                ohos_prp_preload::FuzzIsInfoMatchedMethodMismatch(&fdp);
                break;
            case 14:
                ohos_prp_preload::FuzzMatchRequestHeadersSizeMismatch(&fdp);
                break;
            case 15:
                ohos_prp_preload::FuzzMatchRequestHeadersKeyMismatch(&fdp);
                break;
            case 16:
                ohos_prp_preload::FuzzMatchRequestHeadersValueMismatch(&fdp);
                break;
            case 17:
                ohos_prp_preload::FuzzMatchRequestHeadersMatch(&fdp);
                break;
            case 18:
                ohos_prp_preload::FuzzReplaceHeadersScenario(&fdp);
                break;
            case 19:
                ohos_prp_preload::FuzzRecordPendingPRPPLoaderScenario(&fdp);
                break;
            case 20:
                ohos_prp_preload::FuzzDoPendingCreateTaskScenario(&fdp);
                break;
            case 21:
                ohos_prp_preload::FuzzRecordNeedRecordHeaderUrlsScenario(&fdp);
                break;
            case 22:
                ohos_prp_preload::FuzzNeedRecordHeaderUrlsEmpty(&fdp);
                break;
            case 23:
                ohos_prp_preload::FuzzOnResPreloadedScenario(&fdp);
                break;
            case 24:
                ohos_prp_preload::FuzzGetWeakScenario(&fdp);
                break;
            case 25:
                ohos_prp_preload::FuzzGetMainUrlScenario(&fdp);
                break;
            case 26:
                ohos_prp_preload::FuzzCreatePendingReqLoaderAndStartScenario(&fdp);
                break;
            case 27:
                ohos_prp_preload::FuzzCreatePendingReqLoaderAndStartNullContext(&fdp);
                break;
            case 28:
                ohos_prp_preload::FuzzMultipleCreateAndGet(&fdp);
                break;
            case 29:
                ohos_prp_preload::FuzzHeaderCaseInsensitive(&fdp);
                break;
            case 30:
                ohos_prp_preload::FuzzEmptyDynamicHeaderKeys(&fdp);
                break;
            case 31:
                ohos_prp_preload::FuzzCanReuseHeadersMapScenario(&fdp);
                break;
            case 32:
                ohos_prp_preload::FuzzMatchRequestHeadersEmpty(&fdp);
                break;
            case 33:
                ohos_prp_preload::FuzzMatchRequestHeadersBothEmpty(&fdp);
                break;
            case 34:
                ohos_prp_preload::FuzzReplaceHeadersNoMatch(&fdp);
                break;
            case 35:
                ohos_prp_preload::FuzzReplaceHeadersAllMatch(&fdp);
                break;
            case 36:
                ohos_prp_preload::FuzzPendingPrppLoaderListScenario(&fdp);
                break;
            case 37:
                ohos_prp_preload::FuzzIsInfoMatchedNoRequestBody(&fdp);
                break;
            case 38:
                ohos_prp_preload::FuzzRequestsAlreadyStartSetScenario(&fdp);
                break;
            case 39:
                ohos_prp_preload::FuzzCanReuseHeadersMapWithHeaders(&fdp);
                break;
            case 40:
                ohos_prp_preload::FuzzPrppReqLoadersMapScenario(&fdp);
                break;
            case 41:
                ohos_prp_preload::FuzzNeedRecordHeaderUrlsMultiple(&fdp);
                break;
            case 42:
                ohos_prp_preload::FuzzEmptyMainUrlScenario(&fdp);
                break;
            case 43:
                ohos_prp_preload::FuzzIsolationInfoSetScenario(&fdp);
                break;
            default:
                break;
        }
        ohos_prp_preload::RunUntilIdle();
    }
    ohos_prp_preload::RunUntilIdle();
    return 0;
}