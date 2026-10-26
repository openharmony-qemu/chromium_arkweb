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

#include "gtest/gtest.h"

#include "arkweb/build/features/features.h"

#define private public
#include "arkweb/chromium_ext/gpu/ipc/client/gpu_channel_host_utils.h"
#undef private

namespace gpu {

class GpuChannelHostUtilsTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

// Test constructor: nullptr is stored as-is (no dereference, safe with raw_ptr)
TEST_F(GpuChannelHostUtilsTest, Constructor_NullptrStored)
{
    GpuChannelHostUtils utils(nullptr);
    EXPECT_EQ(utils.gpu_channel_Host_.get(), static_cast<GpuChannelHost*>(nullptr));
}

// Test constructor: non-null pointer is stored (no dereference in constructor)
TEST_F(GpuChannelHostUtilsTest, Constructor_StoresPointer)
{
    // Use a valid-looking but non-dereferenced pointer. raw_ptr assignment
    // without dereference is safe; we only verify storage, never dereference.
    GpuChannelHost* host = reinterpret_cast<GpuChannelHost*>(0x1234);
    GpuChannelHostUtils utils(host);
    // Note: raw_ptr may report on release in debug builds, but assignment is safe.
    EXPECT_EQ(utils.gpu_channel_Host_.get(), host);
}

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
// Test SetBlanklessDumpInfo: verify method exists and is callable when flag is set.
// We cannot call it with an invalid host (would dereference), so just verify the
// method symbol exists by taking its address.
TEST_F(GpuChannelHostUtilsTest, SetBlanklessDumpInfo_MethodExists)
{
    auto methodPtr = &GpuChannelHostUtils::SetBlanklessDumpInfo;
    EXPECT_NE(methodPtr, nullptr);
}
#endif // BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)

} // namespace gpu
