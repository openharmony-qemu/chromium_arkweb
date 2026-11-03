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

#include "arkweb/build/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"

#define private public
#define protected public
#include "arkweb/chromium_ext/components/viz/service/display_embedder/skia_output_device_vulkan_utils.h"
#include "arkweb/chromium_ext/components/viz/service/display_embedder/skia_output_device_vulkan_utils.cc"
#undef private
#undef protected

#include "base/cancelable_callback.h"

namespace viz {

class SkiaOutputDeviceVulkanUtilsTest : public testing::Test {
 public:
  SkiaOutputDeviceVulkanUtilsTest() = default;
  ~SkiaOutputDeviceVulkanUtilsTest() = default;
};

TEST_F(SkiaOutputDeviceVulkanUtilsTest, ConstructorDefaultValues) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  EXPECT_EQ(utils.deviceVulkan, nullptr);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, WeakPtrFactoryValid) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  EXPECT_TRUE(utils.weak_ptr_factory_.GetWeakPtr());
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, WeakPtrFactoryGetWeakPtr) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  auto weak_ptr = utils.weak_ptr_factory_.GetWeakPtr();
  EXPECT_TRUE(weak_ptr);
}

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
TEST_F(SkiaOutputDeviceVulkanUtilsTest, SetIfNeedCleanBuffers_True) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, SetIfNeedCleanBuffers_False) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.SetIfNeedCleanBuffers(false);
  EXPECT_FALSE(utils.need_clean_buffers_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, NeedCleanBuffersDefaultValue) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  EXPECT_FALSE(utils.need_clean_buffers_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffersDefaultValue) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  EXPECT_FALSE(utils.do_clean_buffers_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, HasDelayCleanBufferTaskDefaultValue) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  EXPECT_FALSE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersWhenInvisibleEnabledDefaultValue) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  EXPECT_TRUE(utils.clean_buffers_when_invisible_enabled_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, PrevCleanBuffersTimeDefaultValue) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  EXPECT_NE(utils.prev_clean_buffers_time_, base::TimeTicks());
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_Disabled) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = false;
  utils.need_clean_buffers_ = true;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_NotNeeded) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = false;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_EnabledAndNeededLongInterval) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(200);
  utils.do_clean_buffers_ = true;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_ShortInterval) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(50);
  utils.has_delay_clean_buffer_task_ = false;
  utils.CleanBuffersIfNeed();
  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
  EXPECT_TRUE(utils.do_clean_buffers_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_ShortIntervalHasTask) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(50);
  utils.has_delay_clean_buffer_task_ = true;
  utils.CleanBuffersIfNeed();
  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_MultipleCalls) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(50);

  utils.CleanBuffersIfNeed();
  utils.CleanBuffersIfNeed();
  utils.CleanBuffersIfNeed();

  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_Disabled) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = false;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_NotNeeded) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = false;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_NotDoClean) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = false;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_DeviceNull) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.deviceVulkan = nullptr;
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_WithCancelTask) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.has_delay_clean_buffer_task_ = true;
  utils.delay_clean_buffer_task_closure_ = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce([]() {}));
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_NullptrCancelTask) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.has_delay_clean_buffer_task_ = true;
  utils.delay_clean_buffer_task_closure_ = nullptr;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_NoCancelTask) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.has_delay_clean_buffer_task_ = false;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, SetIfNeedCleanBuffers_MultipleCalls) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
  utils.SetIfNeedCleanBuffers(false);
  EXPECT_FALSE(utils.need_clean_buffers_);
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_VeryShortInterval) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(10);
  utils.has_delay_clean_buffer_task_ = false;
  utils.CleanBuffersIfNeed();
  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_ExactInterval) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(100);
  utils.has_delay_clean_buffer_task_ = false;
  utils.do_clean_buffers_ = true;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, CleanBuffersIfNeed_JustAboveInterval) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(101);
  utils.has_delay_clean_buffer_task_ = false;
  utils.do_clean_buffers_ = true;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_AllConditionsFalse) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = false;
  utils.need_clean_buffers_ = false;
  utils.do_clean_buffers_ = false;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_PartialConditionsFalse) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = false;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_AnotherPartialFalse) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = false;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceVulkanUtilsTest, DoCleanBuffers_ThirdPartialFalse) {
  SkiaOutputDeviceVulkanUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = false;
  utils.DoCleanBuffers();
}
#endif

}  // namespace viz