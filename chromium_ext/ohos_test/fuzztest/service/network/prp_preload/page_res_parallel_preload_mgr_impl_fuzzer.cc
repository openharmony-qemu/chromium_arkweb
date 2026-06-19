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
#include "arkweb/chromium_ext/services/network/prp_preload/src/page_res_parallel_preload_mgr_impl.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace ohos_prp_preload {
constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxUrlLen = 256;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;
static std::unique_ptr<PRParallelPreloadMgrImpl> g_preload_mgr = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>();
        if (!g_preload_mgr) {
            g_preload_mgr = std::make_unique<PRParallelPreloadMgrImpl>();
        }
    }
    ~Environment() {
        task_environment_.reset();
    }
};

void FuzzInitScenario(FuzzedDataProvider* fdp) {
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    g_preload_mgr->Init(task_runner.get());
}

void FuzzStartPageScenario(FuzzedDataProvider* fdp) {
    std::string page_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    net::NetworkAnonymizationKey networkAnonymizationKey;
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    uint64_t navigator = fdp->ConsumeIntegral<uint64_t>();
    
    g_preload_mgr->StartPage(page_url, networkAnonymizationKey, 
        url_request_context->GetWeakPtr(), navigator,
        base::BindOnce([](const std::string&){}), fdp->ConsumeBool());
}

void FuzzStopPageScenario(FuzzedDataProvider* fdp) {
    uint64_t navigator = fdp->ConsumeIntegral<uint64_t>();
    g_preload_mgr->StopPage(navigator);
}

void FuzzUpdateResRequestInfoScenario(FuzzedDataProvider* fdp) {
    std::shared_ptr<PRRequestInfo> info = std::make_shared<PRRequestInfo>();
    std::string page_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    g_preload_mgr->UpdateResRequestInfo(page_url, info);
}

void FuzzUpdateRedirectUrlScenario(FuzzedDataProvider* fdp) {
    std::string org_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::string redirect_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    bool replace = fdp->ConsumeBool();
    g_preload_mgr->UpdateRedirectUrl(org_url, redirect_url, replace);
}

void FuzzSetPageOriginScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    url::Origin origin = url::Origin::Create(GURL(url));
    net::IsolationInfo isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin, origin, net::SiteForCookies());
    g_preload_mgr->SetPageOrigin(url, isolation_info);
}

void FuzzOnPageOriginScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    g_preload_mgr->OnPageOrigin(url, page_origin);
}

void FuzzGetRequestLoaderFactoryScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    auto factory = g_preload_mgr->GetRequestLoaderFactory(main_url);
}

void FuzzUpdateIdlePrerequestCountScenario(FuzzedDataProvider* fdp) {
    std::string key = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    g_preload_mgr->UpdateIdlePrerequestCount(key);
}

void FuzzRemoveCacheScenario(FuzzedDataProvider* fdp) {
    int64_t start_delta = fdp->ConsumeIntegralInRange<int64_t>(0, 1000);
    int64_t end_delta = fdp->ConsumeIntegralInRange<int64_t>(start_delta, start_delta + 1000);
    base::Time start_time = base::Time::Now() + base::Seconds(start_delta);
    base::Time end_time = base::Time::Now() + base::Seconds(end_delta);
    g_preload_mgr->RemoveCache(start_time, end_time);
}

void FuzzGetPRParallelPreloadModeScenario(FuzzedDataProvider* fdp) {
    auto mode = g_preload_mgr->GetPRParallelPreloadMode();
}

void FuzzSetURLLoaderFactoryParamScenario(FuzzedDataProvider* fdp) {
    std::string main_url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    network::mojom::URLLoaderFactoryParamsPtr params = 
        network::mojom::URLLoaderFactoryParams::New();
    params->main_url = main_url;
    url::Origin origin = url::Origin::Create(GURL(main_url));
    params->isolation_info = net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther, origin, origin, net::SiteForCookies());
    g_preload_mgr->SetURLLoaderFactoryParam(std::move(params));
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
    
    for (size_t i = 0; i < iterations; ++i) {
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 11);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzInitScenario(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzStartPageScenario(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzStopPageScenario(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzUpdateResRequestInfoScenario(&fdp);
                break;
            case 4:
                ohos_prp_preload::FuzzUpdateRedirectUrlScenario(&fdp);
                break;
            case 5:
                ohos_prp_preload::FuzzSetPageOriginScenario(&fdp);
                break;
            case 6:
                ohos_prp_preload::FuzzOnPageOriginScenario(&fdp);
                break;
            case 7:
                ohos_prp_preload::FuzzGetRequestLoaderFactoryScenario(&fdp);
                break;
            case 8:
                ohos_prp_preload::FuzzUpdateIdlePrerequestCountScenario(&fdp);
                break;
            case 9:
                ohos_prp_preload::FuzzRemoveCacheScenario(&fdp);
                break;
            case 10:
                ohos_prp_preload::FuzzGetPRParallelPreloadModeScenario(&fdp);
                break;
            case 11:
                ohos_prp_preload::FuzzSetURLLoaderFactoryParamScenario(&fdp);
                break;
            default:
                break;
        }
    }
    return 0;
}