/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "base/threading/thread_checker.h"
#include "arkweb/chromium_ext/gpu/command_buffer/service/ohos/scoped_native_buffer_fence_sync.h"
#define private public
#define protected public
#include "ui/gl/gl_context.h"
#include "gpu/command_buffer/service/ohos/native_image_texture_gl_owner.h"
#undef private
#undef protected

#include <memory>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "gmock/gmock.h"
#include "gpu/command_buffer/service/abstract_texture_ohos.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gtest/gtest.h"
#include "media/base/scoped_async_trace.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
const size_t MATRIX_SIZE = 16;
const int WIDTH = 5;
const int HEIGHT = 5;
namespace {
std::unique_ptr<AbstractTextureOHOS> CreateTexture(
    SharedContextState* context_state) {
  DCHECK(context_state);

  gles2::FeatureInfo* feature_info = context_state->feature_info();
  if (feature_info && feature_info->is_passthrough_cmd_decoder()) {
    return AbstractTextureOHOS::CreateForPassthrough(gfx::Size());
  }

  return AbstractTextureOHOS::CreateForValidating(gfx::Size());
}
}  // namespace

class NativeImageTextureGlOwnerTest : public GpuChannelTestCommon {
 public:
  NativeImageTextureGlOwnerTest()
      : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~NativeImageTextureGlOwnerTest() override = default;
  void SetUp() override {
    int32_t kClientId = 1;
    bool is_gpu_host = false;
    GpuChannel* channel = CreateChannel(kClientId, is_gpu_host);
    ContextResult result;
    auto context_state =
        channel->gpu_channel_manager()->GetSharedContextState(&result);
    auto texture = CreateTexture(context_state.get());
    texture_gl_owner = base::MakeRefCounted<gpu::NativeImageTextureGlOwner>(
        std::move(texture), std::move(context_state));
  }

  void TearDown() override {
    texture_gl_owner = nullptr;
    GpuChannelTestCommon::TearDown();
  }

  scoped_refptr<gpu::NativeImageTextureGlOwner> texture_gl_owner;
};

// ===== Original test cases (preserved) =====

TEST_F(NativeImageTextureGlOwnerTest, UpdateNativeImage_001) {
  texture_gl_owner->UpdateNativeImage();
  ASSERT_NE(texture_gl_owner->native_image_, nullptr);
  texture_gl_owner->native_image_ = nullptr;
  texture_gl_owner->UpdateNativeImage();
}

TEST_F(NativeImageTextureGlOwnerTest, GetSurfaceId_001) {
  uint64_t surface_id = 1;
  uint64_t* surface_id_ptr = &surface_id;
  texture_gl_owner->GetSurfaceId(surface_id_ptr);
  ASSERT_NE(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, GetSurfaceId_002) {
  uint64_t surface_id = 1;
  uint64_t* surface_id_ptr = &surface_id;
  texture_gl_owner->native_image_ = nullptr;
  texture_gl_owner->GetSurfaceId(surface_id_ptr);
  ASSERT_EQ(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, ReleaseNativeImage_001) {
  texture_gl_owner->ReleaseNativeImage();
  ASSERT_NE(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, ReleaseNativeImage_002) {
  texture_gl_owner->native_image_ = nullptr;
  texture_gl_owner->ReleaseNativeImage();
  ASSERT_EQ(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, AquireOhosNativeWindow_001) {
  auto result = texture_gl_owner->AquireOhosNativeWindow();
  ASSERT_NE(result, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, AquireOhosNativeWindow_002) {
  texture_gl_owner->native_image_ = nullptr;
  auto result = texture_gl_owner->AquireOhosNativeWindow();
  ASSERT_EQ(result, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, GetCodedSizeAndVisibleRect_001) {
  gfx::Size rotated_visible_size;
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  texture_gl_owner->native_image_ = nullptr;
  bool result = texture_gl_owner->GetCodedSizeAndVisibleRect(
      rotated_visible_size, coded_size, visible_rect);
  EXPECT_FALSE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, GetCodedSizeAndVisibleRect_002) {
  gfx::Size rotated_visible_size(1, 1);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->GetCodedSizeAndVisibleRect(
      rotated_visible_size, coded_size, visible_rect);
  EXPECT_TRUE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform001) {
  float mtx[16] = {0.0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_FALSE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform002) {
  float mtx[16] = {0.0};
  gfx::Size rotated_visible_size(1, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_TRUE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform003) {
  float mtx[16] = {0.0};
  gfx::Size rotated_visible_size(5, 1);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_TRUE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform004) {
  float mtx[16] = {0.0};
  gfx::Size rotated_visible_size(1, 1);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_TRUE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform005) {
  float mtx[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform006) {
  float mtx[16] = {1, 1, 3, 4, 0, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform007) {
  float mtx[16] = {1, 0, 3, 4, 1, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform008) {
  float mtx[16] = {1, 0, 3, 4, 0, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform009) {
  float mtx[16] = {1, 0, 3, 4, 1, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform010) {
  float mtx[16] = {1, 1, 3, 4, 0, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform011) {
  float mtx[16] = {1, 1, 3, 4, 1, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform012) {
  float mtx[16] = {1, 0, 3, 4, 0, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform013) {
  float mtx[16] = {1, 1, 3, 4, 1, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform014) {
  float mtx[16] = {-1, 1, 3, 4, 1, -1, 7, 8, 9, 10, 11, 2, 2, 2, 2, 2};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform015) {
  float mtx[16] = {0, 0, 3, 4, 0, 0, 7, 8, 9, 10, 11, 1, 0, 0, 1, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform016) {
  float mtx[16] = {0, 1, 3, 4, 0, 0, 7, 8, 9, 10, 11, 1, 0, 1, 1, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform017) {
  float mtx[16] = {0, 0, 3, 4, 1, 0, 7, 8, 9, 10, 11, 1, 1, 0, 1, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform018) {
  float mtx[16] = {0.5, 1, 3, 4, 1, 0.5, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform019) {
  float mtx[16] = {0.0};
  gfx::Size rotated_visible_size(1, 1);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_TRUE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform020) {
  float mtx[16] = {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform021) {
  float mtx[16] = {0, 1, 3, 4, 0, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform022) {
  float mtx[16] = {0, 0, 3, 4, 1, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform023) {
  float mtx[16] = {0, 0, 3, 4, 0, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform024) {
  float mtx[16] = {0, 0, 3, 4, 1, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform025) {
  float mtx[16] = {0, 1, 3, 4, 0, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform026) {
  float mtx[16] = {0, 1, 3, 4, 1, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform027) {
  float mtx[16] = {0, 0, 3, 4, 0, 0, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(false, texture_gl_owner->DecomposeTransform(
                       mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                       visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform028) {
  float mtx[16] = {0, 1, 3, 4, 1, 1, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  mtx[0] = true;
  texture_gl_owner->DecomposeTransform(mtx, MATRIX_SIZE, rotated_visible_size,
                                       coded_size, visible_rect);
  EXPECT_EQ(true, texture_gl_owner->DecomposeTransform(
                      mtx, MATRIX_SIZE, rotated_visible_size, coded_size,
                      visible_rect));
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform029) {
  float mtx[16] = {0.0};
  gfx::Size rotated_visible_size(1, 1);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, 0, rotated_visible_size, coded_size, visible_rect);
  EXPECT_FALSE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform030) {
  float mtx[16] = {0, 0.5, 3, 4, 0.5, 0, 7, 8, 9, 10, 11, 1, 0, 1, 1, 1};
  gfx::Size rotated_visible_size(WIDTH, HEIGHT);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_TRUE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform031) {
  float mtx[16] = {0, 0.5, 3, 4, 0, 2, 7, 8, 9, 10, 11, 1, 0, 1, 1, 1};
  gfx::Size rotated_visible_size(WIDTH, HEIGHT);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_FALSE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform032) {
  float mtx[16] = {0, 0, 3, 4, 2, 0, 7, 8, 9, 10, 11, 1, 0, 1, 1, 1};
  gfx::Size rotated_visible_size(WIDTH, HEIGHT);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_FALSE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform033) {
  float mtx[16] = {0, 1, 3, 4, 2, 1, 7, 8, 9, 10, 11, 1, 0, 1, 1, 1};
  gfx::Size rotated_visible_size(WIDTH, HEIGHT);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_TRUE(result);
  delete coded_size;
  delete visible_rect;
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform034) {
  float mtx[16] = {0, 1, 3, 4, 0, 0, 7, 8, 9, 10, 11, 1, 0, 1, 1, 1};
  gfx::Size rotated_visible_size(WIDTH, HEIGHT);
  gfx::Size* coded_size = new gfx::Size(1, 1);
  gfx::Rect* visible_rect = new gfx::Rect(1, 1);
  bool result = texture_gl_owner->DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, coded_size, visible_rect);
  EXPECT_FALSE(result);
  delete coded_size;
  delete visible_rect;
}

// ===== New test cases (added for 95%+ coverage) =====
// These cover methods previously excluded by LCOV_EXCL markers

TEST_F(NativeImageTextureGlOwnerTest, ConstructorSetsContextAndSurface) {
  EXPECT_NE(texture_gl_owner->context_, nullptr);
  EXPECT_NE(texture_gl_owner->surface_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, ConstructorCreatesNativeImage) {
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, ReleaseResources_ClearsNativeImage) {
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
  texture_gl_owner->ReleaseResources();
  EXPECT_EQ(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, ReleaseResources_DoubleCallNoCrash) {
  texture_gl_owner->ReleaseResources();
  EXPECT_EQ(texture_gl_owner->native_image_, nullptr);
  texture_gl_owner->ReleaseResources();
  EXPECT_EQ(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, SetFrameAvailableCallback_SetsFlag) {
  EXPECT_FALSE(texture_gl_owner->is_frame_available_callback_set_);
  base::RepeatingClosure callback = base::BindRepeating([]() {});
  texture_gl_owner->SetFrameAvailableCallback(callback);
  EXPECT_TRUE(texture_gl_owner->is_frame_available_callback_set_);
}

TEST_F(NativeImageTextureGlOwnerTest, RunWhenBufferIsAvailable_ImmediateRun) {
  bool callback_called = false;
  base::OnceClosure callback = base::BindOnce(
      [](bool* flag) { *flag = true; }, &callback_called);
  texture_gl_owner->RunWhenBufferIsAvailable(std::move(callback));
  EXPECT_TRUE(callback_called);
}

TEST_F(NativeImageTextureGlOwnerTest, UpdateNativeImage_AfterReleaseResources) {
  texture_gl_owner->ReleaseResources();
  EXPECT_EQ(texture_gl_owner->native_image_, nullptr);
  texture_gl_owner->UpdateNativeImage();
  EXPECT_EQ(texture_gl_owner->native_image_, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, EnsureNativeImageBound_ValidTextureId) {
  GLuint texture_id = texture_gl_owner->GetTextureId();
  EXPECT_GT(texture_id, 0u);
  texture_gl_owner->EnsureNativeImageBound(texture_id);
}

TEST_F(NativeImageTextureGlOwnerTest, EnsureNativeImageBound_MultipleCalls) {
  GLuint texture_id = texture_gl_owner->GetTextureId();
  texture_gl_owner->EnsureNativeImageBound(texture_id);
  texture_gl_owner->EnsureNativeImageBound(texture_id);
  texture_gl_owner->EnsureNativeImageBound(texture_id);
}

TEST_F(NativeImageTextureGlOwnerTest, GetContext_ReturnsNonNull) {
  gl::GLContext* context = texture_gl_owner->GetContext();
  EXPECT_NE(context, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, GetSurface_ReturnsNonNull) {
  gl::GLSurface* surface = texture_gl_owner->GetSurface();
  EXPECT_NE(surface, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, GetNativeBuffer_ReturnsNull) {
  auto buffer = texture_gl_owner->GetNativeBuffer();
  EXPECT_EQ(buffer, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, GetTextureId_ReturnsPositiveValue) {
  GLuint texture_id = texture_gl_owner->GetTextureId();
  EXPECT_GT(texture_id, 0u);
}

TEST_F(NativeImageTextureGlOwnerTest, GetTextureId_ConsistentAcrossCalls) {
  GLuint id1 = texture_gl_owner->GetTextureId();
  GLuint id2 = texture_gl_owner->GetTextureId();
  EXPECT_EQ(id1, id2);
}

TEST_F(NativeImageTextureGlOwnerTest, GetTextureBase_ReturnsNonNull) {
  TextureBase* base = texture_gl_owner->GetTextureBase();
  EXPECT_NE(base, nullptr);
}

TEST_F(NativeImageTextureGlOwnerTest, FrameAvailableCallbackNotSetInitially) {
  EXPECT_FALSE(texture_gl_owner->is_frame_available_callback_set_);
}

TEST_F(NativeImageTextureGlOwnerTest, GetCodedSizeAndVisibleRect_NativeImageNull_EmptySize) {
  gfx::Size rotated_visible_size(100, 200);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  texture_gl_owner->native_image_ = nullptr;
  bool result = texture_gl_owner->GetCodedSizeAndVisibleRect(
      rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_FALSE(result);
  EXPECT_TRUE(coded_size.IsEmpty());
  EXPECT_TRUE(visible_rect.IsEmpty());
}

TEST_F(NativeImageTextureGlOwnerTest, GetCodedSizeAndVisibleRect_NativeImageNotNull_LargeSize) {
  gfx::Size rotated_visible_size(100, 200);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = texture_gl_owner->GetCodedSizeAndVisibleRect(
      rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_MtxSizeNot16) {
  float mtx[16] = {0};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, 0, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_FALSE(result);
  result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, 8, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_FALSE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_Mtx0NonZero_InvalidMtx5Zero) {
  float mtx[16] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_FALSE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_Mtx0Zero_ValidRotation) {
  float mtx[16] = {0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
  EXPECT_EQ(visible_rect.width(), rotated_visible_size.height());
  EXPECT_EQ(visible_rect.height(), rotated_visible_size.width());
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_Mtx0Zero_InvalidNoMtx1) {
  float mtx[16] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_FALSE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_Mtx0Zero_InvalidNoMtx4) {
  float mtx[16] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_FALSE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_Mtx0Zero_InvalidMtx5NonZero) {
  float mtx[16] = {0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_NegativeSxTxCalc) {
  float mtx[16] = {-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_NegativeSyTyCalc) {
  float mtx[16] = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 2, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_SxLessThan1_WidthScaled) {
  float mtx[16] = {0.5, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(10, 10);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
  EXPECT_GT(coded_size.width(), rotated_visible_size.width());
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_SyLessThan1_HeightScaled) {
  float mtx[16] = {1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(10, 10);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
  EXPECT_GT(coded_size.height(), rotated_visible_size.height());
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_BothSxSyLessThan1) {
  float mtx[16] = {0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(10, 10);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
  EXPECT_GT(coded_size.width(), rotated_visible_size.width());
  EXPECT_GT(coded_size.height(), rotated_visible_size.height());
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_RotationScaledBoth) {
  float mtx[16] = {0, 0.5, 0, 0, 0.5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(10, 10);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_RotationNoMtx4) {
  float mtx[16] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(5, 5);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_FALSE(result);
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_LargeSizeIdentity) {
  float mtx[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(1920, 1080);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
  EXPECT_EQ(coded_size, rotated_visible_size);
  EXPECT_EQ(visible_rect, gfx::Rect(rotated_visible_size));
}

TEST_F(NativeImageTextureGlOwnerTest, DecomposeTransform_LargeSizeRotation) {
  float mtx[16] = {0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  gfx::Size rotated_visible_size(1920, 1080);
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  bool result = NativeImageTextureGlOwner::DecomposeTransform(
      mtx, MATRIX_SIZE, rotated_visible_size, &coded_size, &visible_rect);
  EXPECT_TRUE(result);
  EXPECT_EQ(visible_rect.width(), rotated_visible_size.height());
  EXPECT_EQ(visible_rect.height(), rotated_visible_size.width());
}

TEST_F(NativeImageTextureGlOwnerTest, MakeCurrentIfNeeded_ContextAlreadyCurrent)
{
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
  texture_gl_owner->UpdateNativeImage();
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
  gl::GLContext* context = texture_gl_owner->GetContext();
  EXPECT_TRUE(context->IsCurrent(nullptr));
}

TEST_F(NativeImageTextureGlOwnerTest, MakeCurrentIfNeeded_ContextNotCurrent_MakeCurrentSSuccess)
{
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
  gl::GLContext* context = texture_gl_owner->GetContext();
  gl::GLSurface* surface = texture_gl_owner->GetSurface();
  context->ReleaseCurrent(surface);
  EXPECT_FALSE(context->IsCurrent(nullptr));
  texture_gl_owner->UpdateNativeImage();
  EXPECT_TRUE(context->IsCurrent(nullptr));
}

TEST_F(NativeImageTextureGlOwnerTest, MakeCurrentIfNeeded_ContextNotCurrent_MakeCurrentFails)
{
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
  gl::GLContext* context = texture_gl_owner->GetContext();
  gl::GLSurface* surface = texture_gl_owner->GetSurface();
  context->ReleaseCurrent(surface);
  EXPECT_FALSE(context->IsCurrent(nullptr));
  context->context_lost_ = true;
  texture_gl_owner->UpdateNativeImage();
  context->context_lost_ = false;
  context->MakeCurrent(surface);
}

TEST_F(NativeImageTextureGlOwnerTest, UpdateNativeImage_ContextLost_ReturnsEarly)
{
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
  gl::GLContext* context = texture_gl_owner->GetContext();
  gl::GLSurface* surface = texture_gl_owner->GetSurface();
  context->ReleaseCurrent(surface);
  context->context_lost_ = true;
  texture_gl_owner->UpdateNativeImage();
  EXPECT_NE(texture_gl_owner->native_image_, nullptr);
  context->context_lost_ = false;
  context->MakeCurrent(surface);
}
}  // namespace gpu