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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>

#include "base/test/bind.h"
#include "base/test/task_environment.h"

#include "gpu/command_buffer/service/ohos/mock_native_image_texture_owner.h"
#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"

#define private public
#include "media/gpu/ohos/codec_output_buffer_renderer.h"
#include "media/gpu/ohos/codec_buffer_wait_coordinator.h"
#include "media/gpu/ohos/codec_wrapper.h"
#undef private

#include "media/gpu/ohos/frame_info_helper.h"

namespace media {

namespace {

constexpr gfx::Size kTestVisibleSize(100, 100);
constexpr gfx::Size kTestVisibleSize2(110, 110);
constexpr gfx::Size kTestCodedSize(128, 128);
constexpr gfx::Rect kTestVisibleRect(0, 0, 100, 50);

}  // namespace

using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

class FrameInfoHelperTest : public testing::Test {
 public:
  FrameInfoHelperTest()
      : drdc_lock_(base::MakeRefCounted<gpu::RefCountedLockForTest>()),
        helper_(CreateHelper()) {}

  std::unique_ptr<FrameInfoHelper> CreateHelper() {
    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    auto get_stub_cb =
        base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
    return FrameInfoHelper::Create(
        std::move(task_runner), std::move(get_stub_cb), drdc_lock_);
  }

  // Synchronous helper to call GetFrameInfo and capture the result.
  void GetFrameInfo(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
      FrameInfoHelper::FrameInfo* out_info = nullptr) {
    bool called = false;
    FrameInfoHelper::FrameInfo local_info;
    auto callback = base::BindLambdaForTesting(
        [&](std::unique_ptr<CodecOutputBufferRenderer> br,
            FrameInfoHelper::FrameInfo info) {
          called = true;
          local_info = info;
        });
    helper_->GetFrameInfo(std::move(buffer_renderer), callback);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(called);
    last_frame_info_ = local_info;
    if (out_info) {
      *out_info = local_info;
    }
  }

  // Create a CodecOutputBufferRenderer with the given size and optional
  // texture_owner. If texture_owner is null, no CodecBufferWaitCoordinator
  // is created (overlay case).
  std::unique_ptr<CodecOutputBufferRenderer> CreateBufferRenderer(
      gfx::Size size,
      scoped_refptr<gpu::NativeImageTextureOwner> texture_owner) {
    auto codec_buffer_wait_coordinator =
        texture_owner
            ? base::MakeRefCounted<CodecBufferWaitCoordinator>(
                  texture_owner, drdc_lock_)
            : nullptr;
#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
    auto buffer = CodecOutputBuffer::CreateForTesting(
        size, false /* render_video_view */, gfx::ColorSpace::CreateSRGB());
#else
    auto buffer = CodecOutputBuffer::CreateForTesting(
        size, gfx::ColorSpace::CreateSRGB());
#endif
    auto buffer_renderer = std::make_unique<CodecOutputBufferRenderer>(
        std::move(buffer), std::move(codec_buffer_wait_coordinator), drdc_lock_);
    return buffer_renderer;
  }

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
  // Create a CodecOutputBufferRenderer with RenderVideoView() returning true.
  std::unique_ptr<CodecOutputBufferRenderer> CreateVideoViewBufferRenderer(
      gfx::Size size,
      scoped_refptr<gpu::NativeImageTextureOwner> texture_owner) {
    auto codec_buffer_wait_coordinator =
        texture_owner
            ? base::MakeRefCounted<CodecBufferWaitCoordinator>(
                  texture_owner, drdc_lock_)
            : nullptr;
    auto buffer = CodecOutputBuffer::CreateForTesting(
        size, true /* render_video_view */, gfx::ColorSpace::CreateSRGB());
    auto buffer_renderer = std::make_unique<CodecOutputBufferRenderer>(
        std::move(buffer), std::move(codec_buffer_wait_coordinator), drdc_lock_);
    return buffer_renderer;
  }
#endif // ARKWEB_VIDEO_ASSISTANT

  // Invalidate the buffer renderer so that RenderToTextureOwnerFrontBuffer
  // will fail. This simulates a render failure.
  void FailNextRender(CodecOutputBufferRenderer* buffer_renderer) {
    buffer_renderer->set_phase_for_testing(
        CodecOutputBufferRenderer::Phase::kInvalidated);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  scoped_refptr<gpu::RefCountedLock> drdc_lock_;
  std::unique_ptr<FrameInfoHelper> helper_;
  FrameInfoHelper::FrameInfo last_frame_info_;
};

// ============================================================================
// Branch 1: No buffer_renderer → empty FrameInfo
// ============================================================================

TEST_F(FrameInfoHelperTest, NoBufferRenderer) {
  // If there is no buffer renderer we should get an empty FrameInfo.
  FrameInfoHelper::FrameInfo info;
  GetFrameInfo(nullptr, &info);
  EXPECT_TRUE(info.coded_size.IsEmpty());
  EXPECT_TRUE(info.visible_rect.IsEmpty());
  EXPECT_FALSE(info.ycbcr_info.has_value());
}

// ============================================================================
// Branch 2: No texture_owner (Overlay) → visible size
// ============================================================================

TEST_F(FrameInfoHelperTest, Overlay) {
  // In overlay case (no texture_owner), we always use visible size.
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  EXPECT_EQ(last_frame_info_.visible_rect, gfx::Rect(kTestVisibleSize));
}

#if BUILDFLAG(ARKWEB_VIDEO_ASSISTANT)
// ============================================================================
// Branch 2 (OH-specific): RenderVideoView() returns true → visible size
// ============================================================================

TEST_F(FrameInfoHelperTest, RenderVideoView) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  // When RenderVideoView() is true, the helper should use visible size
  // instead of going through the texture owner rendering path.
  GetFrameInfo(CreateVideoViewBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  EXPECT_EQ(last_frame_info_.visible_rect, gfx::Rect(kTestVisibleSize));
}
#endif // ARKWEB_VIDEO_ASSISTANT

// ============================================================================
// Branch 4 → OnFrameInfoReady: TextureOwner render success
// ============================================================================

TEST_F(FrameInfoHelperTest, TextureOwnerRenderSuccess) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  // Return CodedSize when GetCodedSizeAndVisibleRect is called.
  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize),
                           SetArgPointee<2>(kTestVisibleRect), Return(true)));

  // Make RunWhenBufferIsAvailable immediately run the callback.
  ON_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });

  // Render should succeed and we should get coded_size from
  // GetCodedSizeAndVisibleRect.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  EXPECT_CALL(*texture_owner, RunWhenBufferIsAvailable(_)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  EXPECT_EQ(last_frame_info_.visible_rect, kTestVisibleRect);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

// ============================================================================
// Branch 4 → OnFrameInfoReady: Render failure → fallback to visible size
// ============================================================================

TEST_F(FrameInfoHelperTest, TextureOwnerRenderFailure) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize),
                           SetArgPointee<2>(kTestVisibleRect), Return(true)));

  ON_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });

  // Fail rendering by invalidating the buffer.
  auto buffer = CreateBufferRenderer(kTestVisibleSize, texture_owner);
  FailNextRender(buffer.get());

  // GetCodedSizeAndVisibleRect should NOT be called since render failed.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  EXPECT_CALL(*texture_owner, RunWhenBufferIsAvailable(_)).Times(1);
  GetFrameInfo(std::move(buffer));
  // Fallback to visible size.
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

// ============================================================================
// Branch 3: Cache hit (same visible_size) → return cached frame_info
// ============================================================================

TEST_F(FrameInfoHelperTest, CacheHitSameSize) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize),
                           SetArgPointee<2>(kTestVisibleRect), Return(true)));

  ON_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });

  // First request: render and get coded size.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Second request with same size: should use cached value.
  // GetCodedSizeAndVisibleRect should NOT be called again.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

// ============================================================================
// Branch 4: Cache miss (different visible_size) → re-render
// ============================================================================

TEST_F(FrameInfoHelperTest, CacheMissDifferentSize) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize),
                           SetArgPointee<2>(kTestVisibleRect), Return(true)));

  ON_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });

  // First request with size1.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Second request with different size: should re-render.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize2, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

// ============================================================================
// Switch between Overlay and TextureOwner
// ============================================================================

TEST_F(FrameInfoHelperTest, SwitchBetweenOverlayAndTextureOwner) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize),
                           SetArgPointee<2>(kTestVisibleRect), Return(true)));

  ON_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });

  // Overlay case: visible size.
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);

  // Switch to TextureOwner: should render and get coded size.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Switch back to overlay: should fallback to visible size.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Switch to TextureOwner with same size: should use cached value.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Switch to TextureOwner with different size: should re-render.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize2, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

// ============================================================================
// GetCodedSizeAndVisibleRect fails → fallback to visible size
// ============================================================================

TEST_F(FrameInfoHelperTest, FailedGetCodedSize) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  ON_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillByDefault([](base::OnceClosure cb) { std::move(cb).Run(); });

  // Fail GetCodedSizeAndVisibleRect. GetFrameInfo should fallback to
  // visible size.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(gfx::Size()), Return(false)));
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // This time GetCodedSizeAndVisibleRect will succeed.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kTestCodedSize),
                      SetArgPointee<2>(kTestVisibleRect), Return(true)));
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Subsequent same-size request should use cache.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

// ============================================================================
// Buffer not available → RunWhenBufferIsAvailable deferred callback
// ============================================================================

TEST_F(FrameInfoHelperTest, TextureOwnerBufferNotAvailable) {
  auto texture_owner =
      base::MakeRefCounted<NiceMock<gpu::MockNativeImageTextureOwner>>();

  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize),
                           SetArgPointee<2>(kTestVisibleRect), Return(true)));

  // Save buffer available callback; we will run it manually.
  base::OnceClosure buffer_available_cb;
  EXPECT_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillOnce([&buffer_available_cb](base::OnceClosure cb) {
        buffer_available_cb = std::move(cb);
      });

  // Verify that no GetCodedSizeAndVisibleRect will be called until buffer
  // is available.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);

  auto buffer_renderer = CreateBufferRenderer(kTestVisibleSize, texture_owner);
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<CodecOutputBufferRenderer> br,
          FrameInfoHelper::FrameInfo info) {
        called = true;
        last_frame_info_ = info;
      });
  helper_->GetFrameInfo(std::move(buffer_renderer), callback);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(buffer_available_cb);
  ASSERT_FALSE(called);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // When buffer is available, GetCodedSizeAndVisibleRect should be called.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  std::move(buffer_available_cb).Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  ASSERT_TRUE(called);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

// ============================================================================
// Create with valid parameters
// ============================================================================

TEST_F(FrameInfoHelperTest, CreateHelperWithValidParameters) {
  auto helper = CreateHelper();
  EXPECT_NE(helper, nullptr);
}

// ============================================================================
// FrameInfo struct: default construction
// ============================================================================

TEST_F(FrameInfoHelperTest, FrameInfoDefaultConstruction) {
  FrameInfoHelper::FrameInfo info;
  EXPECT_TRUE(info.coded_size.IsEmpty());
  EXPECT_TRUE(info.visible_rect.IsEmpty());
  EXPECT_FALSE(info.ycbcr_info.has_value());
}

// ============================================================================
// FrameInfo struct: copy construction
// ============================================================================

TEST_F(FrameInfoHelperTest, FrameInfoCopyConstruction) {
  FrameInfoHelper::FrameInfo source;
  source.coded_size = kTestCodedSize;
  source.visible_rect = kTestVisibleRect;

  FrameInfoHelper::FrameInfo copy(source);
  EXPECT_EQ(copy.coded_size, kTestCodedSize);
  EXPECT_EQ(copy.visible_rect, kTestVisibleRect);
  EXPECT_EQ(copy.ycbcr_info.has_value(), source.ycbcr_info.has_value());
}

// ============================================================================
// FrameInfo struct: move construction
// ============================================================================

TEST_F(FrameInfoHelperTest, FrameInfoMoveConstruction) {
  FrameInfoHelper::FrameInfo source;
  source.coded_size = kTestCodedSize;
  source.visible_rect = kTestVisibleRect;

  FrameInfoHelper::FrameInfo moved(std::move(source));
  EXPECT_EQ(moved.coded_size, kTestCodedSize);
  EXPECT_EQ(moved.visible_rect, kTestVisibleRect);
}

// ============================================================================
// FrameInfo struct: assignment operator
// ============================================================================

TEST_F(FrameInfoHelperTest, FrameInfoAssignmentOperator) {
  FrameInfoHelper::FrameInfo info1;
  FrameInfoHelper::FrameInfo info2;

  info1.coded_size = kTestCodedSize;
  info1.visible_rect = kTestVisibleRect;

  info2 = info1;

  EXPECT_EQ(info2.coded_size, kTestCodedSize);
  EXPECT_EQ(info2.visible_rect, kTestVisibleRect);
}

// ============================================================================
// FrameInfo struct: YCbCr info can be set
// ============================================================================

TEST_F(FrameInfoHelperTest, FrameInfoWithYCbCrInfo) {
  FrameInfoHelper::FrameInfo info;
  EXPECT_FALSE(info.ycbcr_info.has_value());

  info.ycbcr_info = gpu::VulkanYCbCrInfo();
  EXPECT_TRUE(info.ycbcr_info.has_value());
}

}  // namespace media
