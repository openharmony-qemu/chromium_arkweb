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

#include "src/base/platform/jitfort.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "src/base/platform/platform.h"
#include "hilog.h"

namespace v8 {
namespace base {
#define JITFORT_PRCTL_OPTION 0x6a6974
#define JITFORT_SET_ENABLED			1
#define JITFORT_SWITCH_IN			3
#define JITFORT_SWITCH_OUT			4
#define JITFORT_CREATE_COPGTABLE	5
#define JITFORT_QUERY_VERSION 8

#define XPM_SET_JITFORT_ENABLE _IOW('x', 0x3, unsigned long)

void JITFort::EnableJitfort()
{
    int fd = open("/dev/xpm", O_RDWR);
    if (fd < 0) {
        return;
    }
    ioctl(fd, XPM_SET_JITFORT_ENABLE, 0);
    close(fd);
    return;
}

__attribute__((always_inline)) static inline long Syscall(
    unsigned long n, unsigned long a, unsigned long b,
    unsigned long c, unsigned long d, unsigned long e)
{
#ifdef V8_HOST_ARCH_ARM64
    register unsigned long x8 __asm__("x8") = n;
    register unsigned long x0 __asm__("x0") = a;
    register unsigned long x1 __asm__("x1") = b;
    register unsigned long x2 __asm__("x2") = c;
    register unsigned long x3 __asm__("x3") = d;
    register unsigned long x4 __asm__("x4") = e;
    asm volatile("svc 0" : "=r"(x0) : "r"(x8), "0"(x0), "r"(x1), \
        "r"(x2), "r"(x3), "r"(x4) : "memory", "cc");
    return x0;
#else
    return -1;
#endif
}

__attribute__((always_inline)) static int inline PrctlWrapper(
    int op, unsigned long a, unsigned long b = 0)
{
#ifdef V8_HOST_ARCH_ARM64
    return static_cast<int>(Syscall(SYS_prctl, op, a, b, 0, 0));
#else
    return -1;
#endif
}

bool JITFort::IsSupported()
{
    static bool isSupported = []() -> bool {
        long version = PrctlWrapper(JITFORT_PRCTL_OPTION, JITFORT_QUERY_VERSION);
        HilogPrint(INFO, "[jitfort]: JitFort os version is %{public}ld", version);
        if (version >= 2) {
            HilogPrint(INFO, "[jitfort]: JitFort is supported in current os");
            return true;
        } else {
            HilogPrint(INFO, "[jitfort]: JitFort is not supported in current os");
            return false;
        }
    }();
    return isSupported;
}

void JITFort::Init()
{
    // TODO: Open EnableJitfort in hdc param.
    if (!IsSupported()) return;
    JITFort::EnableJitfort();
    long ret = PrctlWrapper(JITFORT_PRCTL_OPTION, JITFORT_CREATE_COPGTABLE);
    if (ret != 0) {
        HilogPrint(FATAL, "[jitfort]: InitJitfort create copytable faild, return value is %{public}ld", ret);
    }
}

void JITFort::SetJITMemoryWritable()
{
    if (!IsSupported()) return;
    long ret = PrctlWrapper(JITFORT_PRCTL_OPTION, JITFORT_SWITCH_IN);
	if (ret != 0) {
        HilogPrint(FATAL, "[jitfort]: Writable prctl in failed, return value is %{public}ld", ret);
	}
}

void JITFort::SetJITMemoryExecutable()
{
    if (!IsSupported()) return;
    long ret = PrctlWrapper(JITFORT_PRCTL_OPTION, JITFORT_SWITCH_OUT);
	if (ret != 0) {
        HilogPrint(FATAL, "[jitfort]: Executable prctl out failed, return value is %{public}ld", ret);
	}
}

RwxMemoryNonWriteScope::RwxMemoryNonWriteScope(const char *comment) {}

RwxMemoryNonWriteScope::~RwxMemoryNonWriteScope() {}
}
}
