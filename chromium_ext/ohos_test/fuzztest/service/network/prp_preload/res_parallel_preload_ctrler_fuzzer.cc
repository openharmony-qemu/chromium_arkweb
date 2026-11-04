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
#include "arkweb/chromium_ext/services/network/prp_preload/src/res_parallel_preload_ctrler.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"

namespace ohos_prp_preload {
constexpr size_t kMaxIterations = 10;
constexpr size_t kMaxUrlLen = 256;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>();
    }
    ~Environment() {
        task_environment_.reset();
    }
};

void FuzzInitAndStartScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    net::NetworkAnonymizationKey key;
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    PRPPCtrlerTimeoutCB timeout_cb = base::DoNothing();
    
    ResParallelPreloadCtrler ctrler(url, key, task_runner.get(), timeout_cb);
    
    std::unique_ptr<net::URLRequestContext> url_request_context = 
        net::CreateTestURLRequestContextBuilder()->Build();
    ctrler.Init(task_runner.get(), url_request_context->GetWeakPtr(), base::DoNothing());
    ctrler.Start();
}

void FuzzStopScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    net::NetworkAnonymizationKey key;
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    PRPPCtrlerTimeoutCB timeout_cb = base::DoNothing();
    
    ResParallelPreloadCtrler ctrler(url, key, task_runner.get(), timeout_cb);
    ctrler.Stop();
}

void FuzzUpdateResRequestInfoScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    net::NetworkAnonymizationKey key;
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    PRPPCtrlerTimeoutCB timeout_cb = base::DoNothing();
    
    ResParallelPreloadCtrler ctrler(url, key, task_runner.get(), timeout_cb);
    
    auto info = std::make_shared<PRRequestInfo>();
    ctrler.UpdateResRequestInfo(info);
}

void FuzzSetPageOriginScenario(FuzzedDataProvider* fdp) {
    std::string url = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    net::NetworkAnonymizationKey key;
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    PRPPCtrlerTimeoutCB timeout_cb = base::DoNothing();
    
    ResParallelPreloadCtrler ctrler(url, key, task_runner.get(), timeout_cb);
    
    std::string page_origin = fdp->ConsumeRandomLengthString(kMaxUrlLen);
    ctrler.SetPageOrigin(page_origin);
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
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 3);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzInitAndStartScenario(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzStopScenario(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzUpdateResRequestInfoScenario(&fdp);
                break;
            case 3:
                ohos_prp_preload::FuzzSetPageOriginScenario(&fdp);
                break;
            default:
                break;
        }
    }
    return 0;
}
