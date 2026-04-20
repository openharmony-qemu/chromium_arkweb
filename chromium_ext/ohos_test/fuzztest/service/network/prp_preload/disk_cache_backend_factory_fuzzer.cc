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
#include "arkweb/chromium_ext/services/network/prp_preload/src/disk_cache_backend_factory.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"

namespace ohos_prp_preload {
constexpr size_t kMaxIterations = 10;
constexpr int kMaxTimeDelta = 1000;

static std::unique_ptr<base::test::TaskEnvironment> task_environment_ = nullptr;

struct Environment {
    Environment() {
        task_environment_ = std::make_unique<base::test::TaskEnvironment>();
    }
    ~Environment() {
        task_environment_.reset();
    }
};

void FuzzCreateBackendScenario(FuzzedDataProvider* fdp) {
    DiskCacheBackendFactory factory;
    factory.CreateBackend();
}

void FuzzCheckBackendAsyncScenario(FuzzedDataProvider* fdp) {
    DiskCacheBackendFactory factory;
    factory.CheckBackendAsync(base::BindOnce([](){}));
}

void FuzzRemoveCacheScenario(FuzzedDataProvider* fdp) {
    DiskCacheBackendFactory factory;
    int64_t start_delta = fdp->ConsumeIntegralInRange<int64_t>(0, kMaxTimeDelta);
    int64_t end_delta = fdp->ConsumeIntegralInRange<int64_t>(start_delta, start_delta + kMaxTimeDelta);
    base::Time start_time = base::Time::Now() + base::Seconds(start_delta);
    base::Time end_time = base::Time::Now() + base::Seconds(end_delta);
    factory.RemoveCache(start_time, end_time);
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
        int scenario = fdp.ConsumeIntegralInRange<int>(0, 2);
        
        switch (scenario) {
            case 0:
                ohos_prp_preload::FuzzCreateBackendScenario(&fdp);
                break;
            case 1:
                ohos_prp_preload::FuzzCheckBackendAsyncScenario(&fdp);
                break;
            case 2:
                ohos_prp_preload::FuzzRemoveCacheScenario(&fdp);
                break;
            default:
                break;
        }
    }
    return 0;
}
