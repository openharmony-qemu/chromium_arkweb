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

#include <cstdlib>
#include <cstdint>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/ohos/task_scheduler/task_runner_ohos.h"
#include "base/ohos/task_scheduler/task_traits_ohos.h"
#include "base/time/time.h"

namespace {

class MockSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
public:
    bool PostDelayedTask(const base::Location &from_here,
                         base::OnceClosure task,
                         base::TimeDelta delay) override
    {
        if (task) {
            std::move(task).Run();
        }
        return true;
    }

    bool PostNonNestableDelayedTask(const base::Location &from_here,
                                    base::OnceClosure task,
                                    base::TimeDelta delay) override
    {
        return true;
    }

    bool RunsTasksInCurrentSequence() const override { return true; }

protected:
    ~MockSingleThreadTaskRunner() override = default;
};

void NoopTask() {}

void SetCallbackOnce()
{
    static bool callback_set = false;
    if (!callback_set) {
        callback_set = true;
        base::TaskRunnerOHOS::SetUiThreadTaskRunnerCallback(
            base::BindRepeating(
                [](::TaskTraits traits)
                    -> scoped_refptr<base::SingleThreadTaskRunner> {
                    return base::MakeRefCounted<MockSingleThreadTaskRunner>();
                }));
    }
}

constexpr int MAX_TASK_TRAITS = 10;
constexpr int MAX_CREATE_CALLS = 5;
constexpr int MAX_DELAY_MS = 1000;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        SetCallbackOnce();
    }

    FuzzedDataProvider dataProvider(data, size);

    uint8_t scenario = dataProvider.ConsumeIntegralInRange<uint8_t>(0, 4);
    switch (scenario) {
    case 0: {
        int create_count =
            dataProvider.ConsumeIntegralInRange<int>(1, MAX_CREATE_CALLS);
        for (int i = 0; i < create_count; i++) {
            int task_traits =
                dataProvider.ConsumeIntegralInRange<int>(0, MAX_TASK_TRAITS);
            auto runner = base::TaskRunnerOHOS::Create(task_traits);
            if (runner) {
                runner->BelongsToCurrentThread();
                runner.release()->Destroy();
            }
        }
        break;
    }
    case 1: {
        int task_traits =
            dataProvider.ConsumeIntegralInRange<int>(0, MAX_TASK_TRAITS);
        auto runner = base::TaskRunnerOHOS::Create(task_traits);
        if (runner) {
            int delay_ms =
                dataProvider.ConsumeIntegralInRange<int>(0, MAX_DELAY_MS);
            runner->PostDelayedTask(FROM_HERE,
                                    base::BindOnce(&NoopTask), delay_ms);
            runner.release()->Destroy();
        }
        break;
    }
    case 2: {
        int task_traits =
            dataProvider.ConsumeIntegralInRange<int>(0, MAX_TASK_TRAITS);
        auto runner = base::TaskRunnerOHOS::Create(task_traits);
        if (runner) {
            runner->PostTask(FROM_HERE, base::BindOnce(&NoopTask));
            runner.release()->Destroy();
        }
        break;
    }
    case 3: {
        auto runner =
            base::TaskRunnerOHOS::Create(static_cast<int>(::UI_DEFAULT));
        if (runner) {
            runner->PostTask(FROM_HERE, base::BindOnce(&NoopTask));
            runner->PostDelayedTask(FROM_HERE,
                                    base::BindOnce(&NoopTask), 100);
            bool belongs = runner->BelongsToCurrentThread();
            (void)belongs;
            delete runner.release();
        }
        break;
    }
    case 4: {
        auto runner =
            base::TaskRunnerOHOS::GetUIThreadTaskRunner(
                static_cast<int>(::UI_DEFAULT));
        (void)runner;
        for (int trait = static_cast<int>(UI_BEST_EFFORT);
             trait <= static_cast<int>(UI_USER_BLOCKING); trait++) {
            auto r =
                base::TaskRunnerOHOS::GetUIThreadTaskRunner(trait);
            (void)r;
        }
        break;
    }
    default:
        break;
    }

    return 0;
}
