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
#include "components/viz/service/display_embedder/skia_output_device_gl.h"
#define private public
#include "arkweb/chromium_ext/components/viz/service/display_embedder/skia_output_device_gl_utils.h"
#include "arkweb/chromium_ext/components/viz/service/display_embedder/skia_output_device_gl_utils.cc"
#undef private

#include "arkweb/build/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace viz {

class SkiaOutputDeviceGLUtilsTest : public testing::Test {
 public:
  SkiaOutputDeviceGLUtilsTest() = default;
  ~SkiaOutputDeviceGLUtilsTest() = default;
};

TEST_F(SkiaOutputDeviceGLUtilsTest, ConstructorDefaultValues) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_EQ(utils.skiaOutPutDeviceGl_, nullptr);
}

#if BUILDFLAG(ARKWEB_OFFLINE_WEB_EVICT_BACK_BUFFERS)
TEST_F(SkiaOutputDeviceGLUtilsTest, SetDelayClean_True) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.SetDelayClean(true);
  EXPECT_TRUE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, SetDelayClean_False) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.SetDelayClean(false);
  EXPECT_FALSE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DelayCleanDefaultValue) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_FALSE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBufferAfterSwapBuffer_SwapAckWithDelayClean) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = true;
  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_ACK);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBufferAfterSwapBuffer_SwapNak) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = true;
  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_FAILED);
  EXPECT_TRUE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBufferAfterSwapBuffer_SwapAckNoDelayClean) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = false;
  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_ACK);
  EXPECT_FALSE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBufferAfterSwapBuffer_SwapNakNoDelayClean) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = false;
  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_FAILED);
  EXPECT_FALSE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanOfflineBuffer_NoDelayClean) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = false;
  utils.CleanOfflineBuffer();
  EXPECT_FALSE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanOfflineBuffer_WithDelayClean) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = true;
  utils.CleanOfflineBuffer();
  EXPECT_FALSE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanOfflineBuffer_WithDelayCleanNullImpl) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.skiaOutPutDeviceGl_ = nullptr;
  utils.delay_clean_ = true;
  utils.CleanOfflineBuffer();
  EXPECT_FALSE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, SetDelayClean_MultipleCalls001) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.SetDelayClean(true);
  EXPECT_TRUE(utils.delay_clean_);
  utils.SetDelayClean(false);
  EXPECT_FALSE(utils.delay_clean_);
  utils.SetDelayClean(true);
  EXPECT_TRUE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBufferAfterSwapBuffer_AllSwapResults001) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_ACK);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_FAILED);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_SKIPPED);
}
#endif

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
TEST_F(SkiaOutputDeviceGLUtilsTest, SetIfNeedCleanBuffers_True) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, SetIfNeedCleanBuffers_False) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.SetIfNeedCleanBuffers(false);
  EXPECT_FALSE(utils.need_clean_buffers_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, NeedCleanBuffersDefaultValue) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_FALSE(utils.need_clean_buffers_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffersDefaultValue) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_FALSE(utils.do_clean_buffers_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, HasDelayCleanBufferTaskDefaultValue) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_FALSE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBuffersWhenInvisibleEnabledDefaultValue) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_TRUE(utils.clean_buffers_when_invisible_enabled_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, PrevCleanBuffersTimeDefaultValue) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_NE(utils.prev_clean_buffers_time_, base::TimeTicks());
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBuffersIfNeed_Disabled) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = false;
  utils.need_clean_buffers_ = true;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBuffersIfNeed_NotNeeded) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = false;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBuffersIfNeed_EnabledAndNeeded) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(200);
  utils.do_clean_buffers_ = true;
  utils.CleanBuffersIfNeed();
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBuffersIfNeed_ShortInterval) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(50);
  utils.has_delay_clean_buffer_task_ = false;
  utils.CleanBuffersIfNeed();
  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
  EXPECT_TRUE(utils.do_clean_buffers_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBuffersIfNeed_ShortIntervalHasTask) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(50);
  utils.has_delay_clean_buffer_task_ = true;
  utils.CleanBuffersIfNeed();
  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBuffersIfNeed_MultipleCalls) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.prev_clean_buffers_time_ = base::TimeTicks::Now() - base::Milliseconds(50);

  utils.CleanBuffersIfNeed();
  utils.CleanBuffersIfNeed();
  utils.CleanBuffersIfNeed();

  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffers_Disabled) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = false;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffers_NotNeeded) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = false;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffers_NotDoClean) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = false;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffers_AllConditionsMet) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.DoCleanBuffers();
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffers_WithCancelTask) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.has_delay_clean_buffer_task_ = true;
  utils.delay_clean_buffer_task_closure_ = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce([]() {}));
  utils.DoCleanBuffers();
  EXPECT_TRUE(utils.do_clean_buffers_);
  EXPECT_TRUE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffers_NoCancelTask) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.has_delay_clean_buffer_task_ = false;
  utils.DoCleanBuffers();
  EXPECT_TRUE(utils.do_clean_buffers_);
  EXPECT_FALSE(utils.has_delay_clean_buffer_task_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, DoCleanBuffers_NullptrCancelTask) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.clean_buffers_when_invisible_enabled_ = true;
  utils.need_clean_buffers_ = true;
  utils.do_clean_buffers_ = true;
  utils.has_delay_clean_buffer_task_ = true;
  utils.delay_clean_buffer_task_closure_ = nullptr;
  utils.DoCleanBuffers();
  EXPECT_TRUE(utils.do_clean_buffers_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, SetIfNeedCleanBuffers_MultipleCalls) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
  utils.SetIfNeedCleanBuffers(false);
  EXPECT_FALSE(utils.need_clean_buffers_);
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
}
#endif

TEST_F(SkiaOutputDeviceGLUtilsTest, SupportsDamageRegionDefaultValue) {
  SkiaOutputDeviceGLUtils utils(nullptr);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, WeakPtrFactoryValid) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  EXPECT_TRUE(utils.weak_ptr_factory_.GetWeakPtr());
}

TEST_F(SkiaOutputDeviceGLUtilsTest, WeakPtrFactoryGetWeakPtr) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  auto weak_ptr = utils.weak_ptr_factory_.GetWeakPtr();
  EXPECT_TRUE(weak_ptr);
}

#if BUILDFLAG(ARKWEB_OFFLINE_WEB_EVICT_BACK_BUFFERS)
TEST_F(SkiaOutputDeviceGLUtilsTest, SetDelayClean_MultipleCalls002) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.SetDelayClean(true);
  EXPECT_TRUE(utils.delay_clean_);
  utils.SetDelayClean(false);
  EXPECT_FALSE(utils.delay_clean_);
  utils.SetDelayClean(true);
  EXPECT_TRUE(utils.delay_clean_);
}

TEST_F(SkiaOutputDeviceGLUtilsTest, CleanBufferAfterSwapBuffer_AllSwapResults002) {
  SkiaOutputDeviceGLUtils utils(nullptr);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_ACK);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_FAILED);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS);
  utils.delay_clean_ = true;

  utils.CleanBufferAfterSwapBuffer(gfx::SwapResult::SWAP_SKIPPED);
}
#endif

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE) && BUILDFLAG(ARKWEB_OFFLINE_WEB_EVICT_BACK_BUFFERS)
TEST_F(SkiaOutputDeviceGLUtilsTest, CombinedFlagsTest) {
  SkiaOutputDeviceGLUtils utils(nullptr);

  utils.SetDelayClean(true);
  utils.SetIfNeedCleanBuffers(true);

  EXPECT_TRUE(utils.delay_clean_);
  EXPECT_TRUE(utils.need_clean_buffers_);
}
#endif

}  // namespace viz