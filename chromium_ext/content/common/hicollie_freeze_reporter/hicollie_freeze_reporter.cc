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

#include <mutex>

#include "hicollie_freeze_reporter.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "hicollie/hicollie.h"
#include "third_party/bounds_checking_function/include/securec.h"

namespace {

base::NoDestructor<std::string>& FreezeMsgStorage() {
  static base::NoDestructor<std::string> g_freezeMsg("arkweb freeze");
  return g_freezeMsg;
}

std::mutex& MsgMutexStorage() {
  static std::mutex g_msgMutex;
  return g_msgMutex;
}

bool& IsCallbackRegisteredStorage() {
  static bool g_isCallbackRegistered = false;
  return g_isCallbackRegistered;
}

bool& IsFreezeEventStorage() {
  static bool g_isFreezeEvent = false;
  return g_isFreezeEvent;
}

std::mutex& HicollieMutexStorage() {
  static std::mutex g_hicollieMutex;
  return g_hicollieMutex;
}

size_t FreezeCallback(HiCollie_Freeze_Type type, void* buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    std::string msg = HicollieReporter::GetFreezeMessage();
    size_t msgSize = msg.size() + 1;
    if (size < msgSize) {
        return 0;
    }

    errno_t err = memcpy_s(buffer, size, msg.c_str(), msgSize);
    if (err != 0) {
        LOG(ERROR) << "memcpy_s failed: " << err;
        return 0;
    }

    return msg.size();
}

} // anonymous namespace

namespace HicollieReporter {

void SetFreezeMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(MsgMutexStorage());
    *FreezeMsgStorage() = msg;
}

std::string GetFreezeMessage() {
    std::lock_guard<std::mutex> lock(MsgMutexStorage());
    return *FreezeMsgStorage();
}

void ReportFreezeToHicollie() {
    std::lock_guard<std::mutex> lock(HicollieMutexStorage());

    static int reportCount = 2;
    if (reportCount <= 0) {
        return;
    }

    if (!IsCallbackRegisteredStorage()) {
        OH_HiCollie_SetFreezeCallback(&FreezeCallback);
        IsCallbackRegisteredStorage() = true;
    }

    HiCollie_ErrorCode code = OH_HiCollie_AssociateProcessReport(IsFreezeEventStorage());
    if (code != HICOLLIE_SUCCESS) {
        LOG(ERROR) << "HiCollie report failed, error: " << code;
    }

    IsFreezeEventStorage() = !IsFreezeEventStorage();
    --reportCount;
}

} // namespace HicollieReporter