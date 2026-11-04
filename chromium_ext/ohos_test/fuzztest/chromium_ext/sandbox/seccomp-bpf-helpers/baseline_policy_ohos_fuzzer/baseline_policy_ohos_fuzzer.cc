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

#include <fuzzer/FuzzedDataProvider.h>
#include <cstdint>
#include <memory>

#include "arkweb/chromium_ext/sandbox/seccomp-bpf-helpers/baseline_policy_ohos.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using namespace sandbox;

namespace {

// Test syscalls that are allowed on both ARM and ARM64
void FuzzAllowedSyscallsCommon(FuzzedDataProvider* fdp) {
  auto policy = std::make_unique<BaselinePolicyOhos>(true);

  int allowed_common[] = {
    __NR_fdatasync, __NR_fsync, __NR_ftruncate, __NR_mremap,
    __NR_pwrite64, __NR_getpriority, __NR_setpriority, __NR_sysinfo,
    __NR_uname, __NR_sigaltstack, __NR_brk, __NR_mlock, __NR_munlock,
    __NR_munmap, __NR_sched_yield, __NR_nanosleep, __NR_epoll_pwait,
    __NR_epoll_create1, __NR_epoll_ctl, __NR_lseek, __NR_eventfd2,
    __NR_fstat, __NR_ppoll, __NR_pselect6, __NR_read, __NR_readv,
    __NR_pread64, __NR_recvfrom, __NR_recvmsg, __NR_sendto, __NR_write,
    __NR_writev, __NR_pipe2, __NR_gettimeofday, __NR_exit, __NR_exit_group,
    __NR_wait4, __NR_waitid, __NR_rt_sigaction, __NR_rt_sigprocmask,
    __NR_rt_sigreturn, __NR_rt_sigtimedwait, __NR_capget, __NR_getegid,
    __NR_geteuid, __NR_getgid, __NR_getgroups, __NR_getpid, __NR_getppid,
    __NR_getresgid, __NR_getsid, __NR_gettid, __NR_getuid, __NR_getresuid,
    __NR_restart_syscall, __NR_close, __NR_dup, __NR_dup3, __NR_shutdown,
    __NR_mincore, __NR_memfd_create, __NR_faccessat, __NR_openat,
    __NR_connect, __NR_readlinkat, __NR_mkdirat, __NR_set_tid_address,
    __NR_getdents64, __NR_getrandom, __NR_prlimit64, __NR_sched_setscheduler,
    __NR_sched_getscheduler, __NR_membarrier, __NR_setitimer, __NR_msync,
    __NR_set_robust_list, __NR_sched_getparam,
  };

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(allowed_common) / sizeof(allowed_common[0]) - 1);
  policy->EvaluateSyscall(allowed_common[idx]);
}

#if defined(__aarch64__)
// Test syscalls that are allowed only on ARM64
void FuzzAllowedSyscallsAarch64(FuzzedDataProvider* fdp) {
  auto policy = std::make_unique<BaselinePolicyOhos>(true);

  int allowed_aarch64[] = {
    __NR_getrlimit, __NR_newfstatat, __NR_fstatfs,
  };

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(allowed_aarch64) / sizeof(allowed_aarch64[0]) - 1);
  policy->EvaluateSyscall(allowed_aarch64[idx]);
}
#endif

#if defined(__arm__)
// Test syscalls that are allowed only on ARM32
void FuzzAllowedSyscallsArm(FuzzedDataProvider* fdp) {
  auto policy = std::make_unique<BaselinePolicyOhos>(true);

  int allowed_arm[] = {
    __NR_sched_getaffinity, __NR_ftruncate64, __NR_ugetrlimit,
    __NR_pause, __NR_epoll_create, __NR_epoll_wait, __NR_eventfd,
    __NR_fstat64, __NR__llseek, __NR_poll, __NR__newselect,
    __NR_send, __NR_pipe, __NR_getegid32, __NR_geteuid32,
    __NR_getgid32, __NR_getgroups32, __NR_getresgid32, __NR_getuid32,
    __NR_getresuid32, __NR_dup2, __NR_open, __NR_readlink,
    __ARM_NR_cacheflush, __ARM_NR_set_tls, __NR_mkdir,
    __NR_sigreturn, __NR_fork, __NR_access, __NR_statx,
  };

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(allowed_arm) / sizeof(allowed_arm[0]) - 1);
  policy->EvaluateSyscall(allowed_arm[idx]);
}
#endif

// Test conditional syscalls (with parameter checks)
void FuzzConditionalSyscalls(FuzzedDataProvider* fdp) {
  auto policy = std::make_unique<BaselinePolicyOhos>(true);

  int conditional[] = {
#if defined(__aarch64__)
    __NR_mmap,
#endif
    __NR_socket, __NR_setsockopt, __NR_clock_getres, __NR_ptrace,
    __NR_madvise, __NR_ioctl, __NR_clone, __NR_prctl,
    __NR_timerfd_create, __NR_timerfd_settime, __NR_getsockopt
  };

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(conditional) / sizeof(conditional[0]) - 1);
  policy->EvaluateSyscall(conditional[idx]);
}

// Test trapped syscalls
void FuzzTrappedSyscallsCommon(FuzzedDataProvider* fdp) {
  bool allow_affinity = fdp->ConsumeBool();
  auto policy = std::make_unique<BaselinePolicyOhos>(allow_affinity);

  int trapped_common[] = {
    __NR_setrlimit, __NR_sched_get_priority_max, __NR_sched_get_priority_min,
    __NR_times, __NR_get_robust_list, __NR_setresuid, __NR_unlinkat,
    __NR_flock, __NR_sched_setaffinity, __NR_getrusage, __NR_process_vm_readv,
    __NR_pkey_free, __NR_pkey_mprotect, __NR_pkey_alloc, __NR_execve,
    __NR_capset, __NR_setsid
  };

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(trapped_common) / sizeof(trapped_common[0]) - 1);
  policy->EvaluateSyscall(trapped_common[idx]);
}

#if defined(__arm__)
// Test trapped syscalls specific to ARM32
void FuzzTrappedSyscallsArm(FuzzedDataProvider* fdp) {
  auto policy = std::make_unique<BaselinePolicyOhos>(true);

  int trapped_arm[] = {
    __NR_sigaction, __NR_futex_time64, __NR_setresuid32,
    __NR_recv, __NR_getdents, __NR_unlink, __NR_sigprocmask,
    __NR_rt_sigtimedwait_time64, __NR_ppoll_time64, __NR_pselect6_time64,
    __NR_fstatat64, __NR_fstatfs64,
  };

  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(trapped_arm) / sizeof(trapped_arm[0]) - 1);
  policy->EvaluateSyscall(trapped_arm[idx]);
}
#endif

// Test random syscall numbers
void FuzzRandomSyscall(FuzzedDataProvider* fdp) {
  bool allow_affinity = fdp->ConsumeBool();
  auto policy = std::make_unique<BaselinePolicyOhos>(allow_affinity);
  int32_t sysno = fdp->ConsumeIntegral<int32_t>();
  policy->EvaluateSyscall(sysno);
}

// Test ranges of syscalls
void FuzzSyscallRange(FuzzedDataProvider* fdp) {
  auto policy = std::make_unique<BaselinePolicyOhos>(true);
  int32_t start = fdp->ConsumeIntegralInRange<int32_t>(0, 450);
  int32_t count = fdp->ConsumeIntegralInRange<int32_t>(1, 50);
  for (int32_t i = 0; i < count; i++) {
    policy->EvaluateSyscall(start + i);
  }
}

// Test edge cases
void FuzzEdgeCases(FuzzedDataProvider* fdp) {
  bool allow_affinity = fdp->ConsumeBool();
  auto policy = std::make_unique<BaselinePolicyOhos>(allow_affinity);
  int32_t edge_values[] = { -1, 0, 1, INT32_MIN, INT32_MAX, 100, 200, 300, 400 };
  size_t idx = fdp->ConsumeIntegralInRange<size_t>(0, sizeof(edge_values) / sizeof(edge_values[0]) - 1);
  policy->EvaluateSyscall(edge_values[idx]);
}

// Test both constructor variants
void FuzzBothAffinityModes(FuzzedDataProvider* fdp) {
  int32_t sysno = fdp->ConsumeIntegral<int32_t>();
  auto policy_allow = std::make_unique<BaselinePolicyOhos>(true);
  auto policy_disallow = std::make_unique<BaselinePolicyOhos>(false);
  policy_allow->EvaluateSyscall(sysno);
  policy_disallow->EvaluateSyscall(sysno);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!data || size < sizeof(int32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  // Select which fuzzing function to use
  uint32_t selector = fdp.ConsumeIntegralInRange<uint32_t>(0, 9);

  switch (selector) {
    case 0:
      FuzzAllowedSyscallsCommon(&fdp);
      break;
    case 1:
#if defined(__aarch64__)
      FuzzAllowedSyscallsAarch64(&fdp);
#else
      FuzzAllowedSyscallsCommon(&fdp);
#endif
      break;
    case 2:
#if defined(__arm__)
      FuzzAllowedSyscallsArm(&fdp);
#else
      FuzzAllowedSyscallsCommon(&fdp);
#endif
      break;
    case 3:
      FuzzConditionalSyscalls(&fdp);
      break;
    case 4:
      FuzzTrappedSyscallsCommon(&fdp);
      break;
    case 5:
#if defined(__arm__)
      FuzzTrappedSyscallsArm(&fdp);
#else
      FuzzTrappedSyscallsCommon(&fdp);
#endif
      break;
    case 6:
      FuzzRandomSyscall(&fdp);
      break;
    case 7:
      FuzzSyscallRange(&fdp);
      break;
    case 8:
      FuzzEdgeCases(&fdp);
      break;
    case 9:
      FuzzBothAffinityModes(&fdp);
      break;
    default:
      FuzzRandomSyscall(&fdp);
      break;
  }

  return 0;
}
