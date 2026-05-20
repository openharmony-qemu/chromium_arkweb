/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define private public
#include "arkweb/ohos_adapter_ndk/security_adapter/arkts_security_adapter_impl.h"
#undef private

#include <fuzzer/FuzzedDataProvider.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

void FuzzGetEnv(FuzzedDataProvider* fdp) {
    (void)ArktsSecurityAdapter::GetEnv();
}

void FuzzGetSecurityGuardModule(FuzzedDataProvider* fdp) {
    (void)ArktsSecurityAdapter::GetSecurityGuardModule();
}

void FuzzRegisterEvent(FuzzedDataProvider* fdp) {
    std::string event = fdp->ConsumeRandomLengthString(256);
    (void)ArktsSecurityAdapter::RegisterEvent(event);

    std::string longEvent;
    size_t len = fdp->ConsumeIntegralInRange<size_t>(0, 4096);
    for (size_t i = 0; i < len; i++) {
        longEvent += static_cast<char>(fdp->ConsumeIntegralInRange<int>(0, 255));
    }
    (void)ArktsSecurityAdapter::RegisterEvent(longEvent);

    std::string specialCharsEvent;
    std::vector<char> specialChars = {'"', '\\', '\n', '\r', '\t', '\0', '{', '}', ':', ','};
    for (size_t i = 0; i < fdp->ConsumeIntegralInRange<size_t>(0, 100); i++) {
        if (fdp->ConsumeBool() && !specialChars.empty()) {
            specialCharsEvent += specialChars[fdp->ConsumeIntegralInRange<size_t>(0, specialChars.size() - 1)];
        } else {
            specialCharsEvent += static_cast<char>(fdp->ConsumeIntegralInRange<char>(32, 126));
        }
    }
    (void)ArktsSecurityAdapter::RegisterEvent(specialCharsEvent);
}

void ArktsSecurityAdapterImplFuzzTest(FuzzedDataProvider* fdp) {
    int32_t selector = fdp->ConsumeIntegralInRange<int32_t>(0, 2);
    switch (selector) {
        case 0:
            FuzzGetEnv(fdp);
            break;
        case 1:
            FuzzGetSecurityGuardModule(fdp);
            break;
        case 2:
            FuzzRegisterEvent(fdp);
            break;
        default:
            FuzzRegisterEvent(fdp);
            break;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);
    ArktsSecurityAdapterImplFuzzTest(&fdp);

    return 0;
}
