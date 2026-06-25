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

#include "baseline_policy_ohos.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"

namespace sandbox {

namespace {

using bpf_dsl::ResultExpr;

class BaselinePolicyOhosTest : public ::testing::Test {
protected:
    BaselinePolicyOhos policy_;
};

TEST_F(BaselinePolicyOhosTest, AllowedSyscallScheduleYield) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_yield);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPipe2) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_pipe2);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRead) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_read);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallWrite) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_write);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetpid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getpid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetuid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getuid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallClose) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_close);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMmap) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_mmap);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMunmap) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_munmap);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSchedYield) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_yield);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallNanosleep) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_nanosleep);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallEpollCreate1) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_epoll_create1);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallEventfd2) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_eventfd2);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetrandom) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getrandom);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMemfdCreate) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_memfd_create);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPrlimit64) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_prlimit64);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, RestrictedSyscallSchedSetaffinity) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_setaffinity);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, RestrictedSyscallSchedGetaffinity) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_getaffinity);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallClone) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_clone);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallPtrace) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_ptrace);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallAcct) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_acct);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallChroot) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_chroot);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallFanotifyInit) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_fanotify_init);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallFgetxattr) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_fgetxattr);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallGetitimer) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getitimer);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallInitModule) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_init_module);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallIoCancel) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_io_cancel);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallKeyctl) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_keyctl);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallMqOpen) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_mq_open);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallSetpgid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_setpgid);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallSwapon) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_swapon);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallSysinfo) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sysinfo);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallSyslog) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_syslog);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallTimerCreate) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_timer_create);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallSetrlimit) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_setrlimit);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallExecve) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_execve);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallCapset) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_capset);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallSetsid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_setsid);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallRseq) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_rseq);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, TrapSyscallProcessVmReadv) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_process_vm_readv);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSocket) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_socket);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSetsockopt) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_setsockopt);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallClockGetres) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_clock_getres);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMadvise) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_madvise);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallIoctl) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_ioctl);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallTimerfdCreate) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_timerfd_create);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallTimerfdSettime) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_timerfd_settime);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetsockopt) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getsockopt);
    EXPECT_FALSE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPrctl) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_prctl);
    EXPECT_FALSE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallFdatasync) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_fdatasync);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallFsync) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_fsync);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallFtruncate) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_ftruncate);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMremap) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_mremap);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPwrite64) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_pwrite64);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetpriority) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getpriority);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSetpriority) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_setpriority);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallUname) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_uname);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSigaltstack) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sigaltstack);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallBrk) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_brk);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMlock) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_mlock);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMunlock) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_munlock);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallEpollPwait) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_epoll_pwait);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallEpollCtl) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_epoll_ctl);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallLseek) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_lseek);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallFstat) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_fstat);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPpoll) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_ppoll);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPselect6) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_pselect6);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallReadv) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_readv);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPread64) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_pread64);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRecvfrom) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_recvfrom);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRecvmsg) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_recvmsg);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallWritev) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_writev);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGettimeofday) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_gettimeofday);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallExit) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_exit);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallExitGroup) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_exit_group);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallWait4) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_wait4);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallWaitid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_waitid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRtSigaction) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_rt_sigaction);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRtSigprocmask) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_rt_sigprocmask);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRtSigreturn) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_rt_sigreturn);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRtSigtimedwait) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_rt_sigtimedwait);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallCapget) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_capget);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetegid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getegid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGeteuid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_geteuid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetgid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getgid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetgroups) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getgroups);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetppid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getppid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetsid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getsid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGettid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_gettid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetresgid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getresgid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetresuid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getresuid);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallRestartSyscall) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_restart_syscall);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallDup) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_dup);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallDup3) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_dup3);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallShutdown) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_shutdown);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMincore) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_mincore);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallFaccessat) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_faccessat);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallOpenat) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_openat);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallConnect) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_connect);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallReadlinkat) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_readlinkat);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSetTidAddress) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_set_tid_address);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetdents64) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getdents64);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSchedSetscheduler) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_setscheduler);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSchedGetscheduler) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_getscheduler);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMembarrier) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_membarrier);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSetitimer) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_setitimer);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallMsync) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_msync);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSetRobustList) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_set_robust_list);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSchedGetparam) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_getparam);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetrlimit) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getrlimit);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallNewfstatat) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_newfstatat);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallFstatfs) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_fstatfs);
    EXPECT_TRUE(result->IsAllow());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSchedGetPriorityMax) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_get_priority_max);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSchedGetPriorityMin) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_sched_get_priority_min);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallTimes) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_times);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetRobustList) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_get_robust_list);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallSetresuid) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_setresuid);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallUnlinkat) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_unlinkat);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallFlock) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_flock);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallGetrusage) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_getrusage);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPkeyFree) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_pkey_free);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPkeyMprotect) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_pkey_mprotect);
    EXPECT_TRUE(result->IsDeny());
}

TEST_F(BaselinePolicyOhosTest, AllowedSyscallPkeyAlloc) {
    ResultExpr result = policy_.EvaluateSyscall(__NR_pkey_alloc);
    EXPECT_TRUE(result->IsDeny());
}

}  // namespace

}  // namespace sandbox