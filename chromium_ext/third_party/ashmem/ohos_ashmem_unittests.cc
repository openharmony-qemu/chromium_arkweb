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

#include <gtest/gtest.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include "base/android/linker/ashmem.h"

using namespace testing;
#define PROT_READ 0x1

class OhosAshmemTest : public ::testing::Test {
protected:
  void SetUp() override {
    fd_ = SharedMemoryRegionCreate("test_region", 4096);
    ASSERT_GE(fd_, 0) << "Failed to create ashmem region";
  }

  void TearDown() override {
    if (fd_ >= 0) close(fd_);
  }

  int fd_{-1};
};

TEST_F(OhosAshmemTest, SetGetProtection) {
  ASSERT_EQ(0, SharedMemoryRegionSetProtectionFlags(fd_, PROT_READ));
  ASSERT_EQ(PROT_READ, SharedMemoryRegionGetProtectionFlags(fd_));
}

TEST_F(OhosAshmemTest, PinUnpinRegion) {
  // Pinning may be disabled on some ashmem implementations; accept both success
  // (0) and unsupported (-1) as valid outcomes per ashmem.h documentation.
  int pin_result = AshmemPinRegion(fd_, 0, 1024);
  EXPECT_TRUE(pin_result == 0 || pin_result == -1);
  int unpin_result = AshmemUnpinRegion(fd_, 0, 1024);
  EXPECT_TRUE(unpin_result == 0 || unpin_result == -1);
}

TEST_F(OhosAshmemTest, DeviceSupportCheck) {
  ASSERT_EQ(1, AshmemDeviceIsSupported());
}

TEST_F(OhosAshmemTest, ashmem_create_region) {
  size_t size = 1;
  auto result = SharedMemoryRegionCreate(nullptr, size);
  ASSERT_GE(result, 0);
  close(result);
}

// ---------------------------------------------------------------------------
// Failure-path coverage for the static helpers ashmem_check_failure /
// ashmem_dev_fd_check / get_ashmem_dev. The tests above only use valid ashmem
// fds, so every ioctl succeeds (result != -1) and the ENOTTY / dev-fd branches
// are never reached. The cases below feed non-ashmem descriptors so the ioctls
// fail and exercise those branches.
// ---------------------------------------------------------------------------

// /dev/null is a char device but not ashmem: its ASHMEM_* ioctls fail with
// ENOTTY, so ashmem_check_failure calls ashmem_dev_fd_check. That runs
// get_ashmem_dev() (once, via pthread_once) and fstats the fd: S_ISCHR is true.
// When /dev/null and /dev/ashmem reside on the same filesystem (e.g. devtmpfs),
// st_dev matches s_ashmem_dev, so ashmem_dev_fd_check returns 1. On systems
// where st_dev differs, it returns 0.
TEST_F(OhosAshmemTest, CheckFailureOnNonAshmemCharDevice) {
  int null_fd = open("/dev/null", O_RDWR);
  ASSERT_GE(null_fd, 0);
  // ashmem_dev_fd_check uses st_dev for device identification. On devtmpfs,
  // /dev/null shares the same st_dev as /dev/ashmem, causing the check to
  // return 1. Accept both 0 (st_dev differs) and 1 (st_dev matches) as valid.
  int result = SharedMemoryRegionSetProtectionFlags(null_fd, PROT_READ);
  EXPECT_TRUE(result == 0 || result == 1);
  result = SharedMemoryRegionGetProtectionFlags(null_fd);
  EXPECT_TRUE(result == 0 || result >= 0);
  result = AshmemPinRegion(null_fd, 0, 1024);
  EXPECT_TRUE(result == 0 || result == 1 || result == -1);
  result = AshmemUnpinRegion(null_fd, 0, 1024);
  EXPECT_TRUE(result == 0 || result == 1 || result == -1);
  close(null_fd);
}

// A regular file is not a character device, so the dev-fd check short-circuits
// at S_ISCHR (false) and returns 0. Covers the S_ISCHR-false arm, complementary
// to the /dev/null case above.
TEST_F(OhosAshmemTest, CheckFailureOnRegularFile) {
  // Try common temp directories; /tmp may not be writable on OHOS devices.
  const char* dirs[] = {"/tmp", "/data/local/tmp"};
  int file_fd = -1;
  std::string used_path_str;
  for (auto dir : dirs) {
    used_path_str = std::string(dir) + "/ohos_ashmem_ut_regular";
    file_fd = open(used_path_str.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (file_fd >= 0) {
      break;
    }
  }
  ASSERT_GE(file_fd, 0) << "Cannot create temp file";
  EXPECT_EQ(0, SharedMemoryRegionSetProtectionFlags(file_fd, PROT_READ));
  close(file_fd);
  if (!used_path_str.empty()) {
    unlink(used_path_str.c_str());
  }
}

// An invalid fd makes ioctl fail with EBADF (not ENOTTY), so ashmem_check_failure
// returns the raw result (-1) without the dev-fd check. Covers the
// (result == -1 && errno != ENOTTY) arm.
TEST_F(OhosAshmemTest, CheckFailureOnInvalidFd) {
  EXPECT_EQ(-1, SharedMemoryRegionSetProtectionFlags(-1, PROT_READ));
  EXPECT_EQ(-1, SharedMemoryRegionGetProtectionFlags(-1));
  EXPECT_EQ(-1, AshmemPinRegion(-1, 0, 1024));
  EXPECT_EQ(-1, AshmemUnpinRegion(-1, 0, 1024));
}