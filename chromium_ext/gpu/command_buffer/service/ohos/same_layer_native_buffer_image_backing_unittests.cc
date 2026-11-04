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
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/MutableTextureState.h"
#include "base/task/single_thread_task_runner.h"
#define private public
#define protected public
#include "gpu/command_buffer/service/ohos/same_layer_native_buffer_image_backing.h"
#undef private
#undef protected
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/ohos/scoped_native_buffer_fence_sync.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/gl/test/gl_test_support.h"

using namespace gpu;
using testing::_;
using testing::Return;
using testing::NiceMock;

namespace gpu {
constexpr int SIZE = 256;

class MockStreamTextureSharedImageInterface
    : public StreamTextureSharedImageInterface {
public:
    MockStreamTextureSharedImageInterface() = default;
    ~MockStreamTextureSharedImageInterface() override = default;

    MOCK_METHOD(void, ReleaseResources, (), (override));
    MOCK_METHOD(bool, IsUsingGpuMemory, (), (const, override));
    MOCK_METHOD(void, UpdateAndBindTexImage, (GLuint service_id), (override));
    MOCK_METHOD(bool, HasTextureOwner, (), (const, override));
    MOCK_METHOD(TextureBase*, GetTextureBase, (), (const, override));
    MOCK_METHOD(void, NotifyOverlayPromotion, (bool promotion, const gfx::Rect& bounds), (override));
    MOCK_METHOD(bool, RenderToOverlay, (), (override));
    MOCK_METHOD(bool, TextureOwnerBindsTextureOnUpdate, (), ());
#if BUILDFLAG(ARKWEB_SAME_LAYER)
    MOCK_METHOD(std::unique_ptr<ScopedNativeBufferFenceSync>, GetNativeBuffer, (), (override));
#endif
};

class MockScopedNativeBufferFenceSync : public ScopedNativeBufferFenceSync {
public:
    MockScopedNativeBufferFenceSync() : ScopedNativeBufferFenceSync() {}
    MockScopedNativeBufferFenceSync(ScopedNativeBufferHandle handle, base::ScopedFD fence_fd)
        : ScopedNativeBufferFenceSync(std::move(handle), std::move(fence_fd)) {}
    ~MockScopedNativeBufferFenceSync() override = default;
    MOCK_METHOD(void, SetReadFence, (base::ScopedFD fence_fd), (override));
};

class SameLayerNativeBufferImageBackingTest : public ::testing::Test {
public:
    SameLayerNativeBufferImageBackingTest() : task_environment_() {}

    void SetUp() override {
        gl::init::InitializeGLNoExtensionsOneOff(
            true, gl::GpuPreference::kDefault);
        auto display = gl::GLTestSupport::InitializeGL(std::nullopt);
        ASSERT_NE(display, nullptr);
        surf_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());
        ASSERT_NE(surf_, nullptr);
        context_ = gl::init::CreateGLContext(nullptr, surf_.get(), gl::GLContextAttribs());
        ASSERT_NE(context_, nullptr);
        ASSERT_TRUE(context_->MakeCurrent(surf_.get()));
        tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
        mailbox_ = Mailbox::Generate();
        size_ = gfx::Size(SIZE, SIZE);
        color_space_ = gfx::ColorSpace::CreateSRGB();
        surface_origin_ = kTopLeft_GrSurfaceOrigin;
        alpha_type_ = kPremul_SkAlphaType;
        gr_context_type_ = GrContextType::kGL;
        debug_label_ = "test";
        stream_ = base::MakeRefCounted<NiceMock<MockStreamTextureSharedImageInterface>>();
        ON_CALL(*stream_, HasTextureOwner()).WillByDefault(Return(true));
        ON_CALL(*stream_, IsUsingGpuMemory()).WillByDefault(Return(true));
        state_ = base::MakeRefCounted<SharedContextState>(
            base::MakeRefCounted<gl::GLShareGroup>(), surf_, context_,
            false, base::DoNothing(), gr_context_type_);
        lock_ = base::MakeRefCounted<RefCountedLock>();
        backing_ = std::make_unique<SameLayerNativeBufferImageBacking>(
            mailbox_, size_, color_space_, surface_origin_, alpha_type_,
            debug_label_, stream_, state_, lock_);
    }

    void TearDown() override {
        if (context_ && surf_) {
            context_->ReleaseCurrent(surf_.get());
        }
        backing_.reset();
    }

protected:
    Mailbox mailbox_;
    gfx::Size size_;
    gfx::ColorSpace color_space_;
    GrSurfaceOrigin surface_origin_;
    SkAlphaType alpha_type_;
    GrContextType gr_context_type_;
    std::string debug_label_;
    scoped_refptr<NiceMock<MockStreamTextureSharedImageInterface>> stream_;
    scoped_refptr<SharedContextState> state_;
    scoped_refptr<RefCountedLock> lock_;
    std::unique_ptr<MemoryTypeTracker> tracker_;
    std::unique_ptr<SameLayerNativeBufferImageBacking> backing_;
    base::test::TaskEnvironment task_environment_{};
    scoped_refptr<gl::GLSurface> surf_;
    scoped_refptr<gl::GLContext> context_;
};

TEST_F(SameLayerNativeBufferImageBackingTest, ConstructorAndDestructor)
{
    EXPECT_NE(backing_, nullptr);
    EXPECT_EQ(backing_->mailbox(), mailbox_);
    EXPECT_EQ(backing_->size(), size_);
    EXPECT_EQ(backing_->color_space(), color_space_);
    EXPECT_EQ(backing_->surface_origin(), surface_origin_);
    EXPECT_EQ(backing_->alpha_type(), alpha_type_);
    EXPECT_NE(backing_->context_lost_helper_, nullptr);
    EXPECT_NE(backing_->stream_texture_sii_, nullptr);
    EXPECT_NE(backing_->gpu_main_task_runner_, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingMailboxMatchesInput)
{
    EXPECT_EQ(backing_->mailbox(), mailbox_);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingSizeMatchesInput)
{
    EXPECT_EQ(backing_->size(), size_);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingColorSpaceMatchesInput)
{
    EXPECT_EQ(backing_->color_space(), color_space_);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingSurfaceOriginMatchesInput)
{
    EXPECT_EQ(backing_->surface_origin(), surface_origin_);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingAlphaTypeMatchesInput)
{
    EXPECT_EQ(backing_->alpha_type(), alpha_type_);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ProduceGLTexture_Fail)
{
    EXPECT_CALL(*stream_, HasTextureOwner()).WillOnce(Return(false));
    SharedImageManager manager;
    auto result = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_EQ(result, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ProduceGLTexture_Success)
{
    SharedImageManager manager;
    auto result = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(result, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ProduceSkiaGanesh_NoTextureOwner)
{
    EXPECT_CALL(*stream_, HasTextureOwner()).WillOnce(Return(false));
    SharedImageManager manager;
    auto result = backing_->ProduceSkiaGanesh(&manager, tracker_.get(), state_);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, GetEstimatedSizeForMemoryDump)
{
    size_t estimated_size = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_GT(estimated_size, 0u);
}

TEST_F(SameLayerNativeBufferImageBackingTest, GetEstimatedSizeForMemoryDump_Consistent)
{
    size_t result1 = backing_->GetEstimatedSizeForMemoryDump();
    size_t result2 = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_EQ(result1, result2);
}

TEST_F(SameLayerNativeBufferImageBackingTest, GLTexture_BeginAccess_NullBuffer)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    EXPECT_CALL(*stream_, GetNativeBuffer()).WillOnce(Return(nullptr));
    bool result = representation->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    EXPECT_FALSE(result);
}

TEST_F(SameLayerNativeBufferImageBackingTest, GLTexture_BeginAccess_ReadWrite_NullBuffer)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    EXPECT_CALL(*stream_, GetNativeBuffer()).WillOnce(Return(nullptr));
    bool result = representation->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    EXPECT_FALSE(result);
}

TEST_F(SameLayerNativeBufferImageBackingTest, GLTexture_GetTexture)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    auto* texture = representation->GetTexture(0);
    EXPECT_NE(texture, nullptr);
    EXPECT_EQ(texture->target(), GL_TEXTURE_EXTERNAL_OES);
}

TEST_F(SameLayerNativeBufferImageBackingTest, GLTexture_DestroyWithoutAccess)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    representation.reset();
    EXPECT_NE(backing_, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, MultipleGLTextureRepresentations)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(rep1, nullptr);
    EXPECT_NE(rep2, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ProduceGLTexture_FailThenSucceed)
{
    EXPECT_CALL(*stream_, HasTextureOwner())
        .WillOnce(Return(false))
        .WillOnce(Return(true));
    SharedImageManager manager;
    auto fail_rep = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_EQ(fail_rep, nullptr);
    auto ok_rep = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(ok_rep, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ProduceGLTexture_MixedResponses)
{
    EXPECT_CALL(*stream_, HasTextureOwner())
        .WillOnce(Return(false))
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_EQ(rep1, nullptr);
    auto rep2 = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(rep2, nullptr);
    auto rep3 = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_EQ(rep3, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ProduceGLTexture_ThreeRepresentations)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep3 = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(rep1, nullptr);
    EXPECT_NE(rep2, nullptr);
    EXPECT_NE(rep3, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, GLTexture_BeginAccess_MixedNullBufferResults)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(rep1, nullptr);
    ASSERT_NE(rep2, nullptr);
    EXPECT_CALL(*stream_, GetNativeBuffer()).WillOnce(Return(nullptr));
    EXPECT_FALSE(rep1->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));
    EXPECT_CALL(*stream_, GetNativeBuffer()).WillOnce(Return(nullptr));
    EXPECT_FALSE(rep2->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));
}

TEST_F(SameLayerNativeBufferImageBackingTest, ContextLostHelper_OnContextLost)
{
    EXPECT_CALL(*stream_, ReleaseResources()).Times(2);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ContextLostHelper_OnContextLost_ThenDestroy)
{
    EXPECT_CALL(*stream_, ReleaseResources()).Times(1);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
    EXPECT_CALL(*stream_, ReleaseResources()).Times(1);
    backing_.reset();
}

TEST_F(SameLayerNativeBufferImageBackingTest, ContextLostHelper_OnContextLost_StreamTextureSiiRemains)
{
    EXPECT_CALL(*stream_, ReleaseResources()).Times(2);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_NE(backing_->context_lost_helper_->stream_texture_sii_, nullptr);
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ContextLostHelper_Destructor_SkipsRemoveWhenNull)
{
    EXPECT_CALL(*stream_, ReleaseResources()).Times(2);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
    backing_.reset();
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingUpdate)
{
    backing_->Update(nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingStreamTextureSii_NotNull)
{
    EXPECT_NE(backing_->stream_texture_sii_, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingGpuMainTaskRunner_NotNull)
{
    EXPECT_NE(backing_->gpu_main_task_runner_, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingContextLostHelper_NotNull)
{
    EXPECT_NE(backing_->context_lost_helper_, nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, DrDcLock_NotNull)
{
    EXPECT_NE(backing_->GetDrDcLock(), nullptr);
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingDebugLabel)
{
    EXPECT_FALSE(debug_label_.empty());
    EXPECT_EQ(debug_label_, "test");
}

TEST_F(SameLayerNativeBufferImageBackingTest, BackingContextStateIsGL)
{
    EXPECT_EQ(gr_context_type_, GrContextType::kGL);
}

TEST_F(SameLayerNativeBufferImageBackingTest, ProduceGLTexture_CreateDestroyMultiple)
{
    SharedImageManager manager;
    for (int i = 0; i < 3; ++i) {
        auto rep = backing_->ProduceGLTexture(&manager, tracker_.get());
        EXPECT_NE(rep, nullptr);
        rep.reset();
    }
}

TEST_F(SameLayerNativeBufferImageBackingTest, GetEstimatedSizeForMemoryDump_AlternatingStates)
{
    size_t size1 = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_GT(size1, 0u);
    size_t size2 = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_EQ(size1, size2);
}

}  // namespace gpu