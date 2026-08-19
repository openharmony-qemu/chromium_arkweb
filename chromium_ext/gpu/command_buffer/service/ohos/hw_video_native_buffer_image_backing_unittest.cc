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
#include "base/task/single_thread_task_runner.h"
#define private public
#define protected public
#include "gpu/command_buffer/service/ohos/hw_video_native_buffer_image_backing.h"
#undef private
#undef protected

#include "base/ohos/scoped_native_buffer_handle.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ohos/scoped_native_buffer_fence_sync.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/command_buffer/service/abstract_texture_ohos.h"
#include "gpu/command_buffer/service/ohos/ohos_image_backing.h"
#include "gpu/command_buffer/service/ohos/ohos_video_image_backing.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
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

class HwVideoNativeBufferImageBackingTest : public ::testing::Test {
public:
    HwVideoNativeBufferImageBackingTest() : task_environment_() {}

    void SetUp() override {
        gl::init::InitializeGLNoExtensionsOneOff(
            true, gl::GpuPreference::kDefault);
        auto display = gl::GLTestSupport::InitializeGL(std::nullopt);
        ASSERT_NE(display, nullptr);
        surf_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());
        ASSERT_NE(surf_, nullptr);
        gl_context_ = gl::init::CreateGLContext(nullptr, surf_.get(), gl::GLContextAttribs());
        ASSERT_NE(gl_context_, nullptr);
        bool make_current_result = gl_context_->MakeCurrent(surf_.get());
        ASSERT_TRUE(make_current_result);
        tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
        mailbox_ = Mailbox::Generate();
        size_ = gfx::Size(SIZE, SIZE);
        color_space_ = gfx::ColorSpace::CreateSRGB();
        surface_origin_ = kTopLeft_GrSurfaceOrigin;
        alpha_type_ = kPremul_SkAlphaType;
        gr_context_type_ = GrContextType::kGL;
        debug_label_ = "test";
        stream_texture_sii_ = base::MakeRefCounted<NiceMock<MockStreamTextureSharedImageInterface>>();
        ON_CALL(*stream_texture_sii_, HasTextureOwner()).WillByDefault(Return(true));
        ON_CALL(*stream_texture_sii_, IsUsingGpuMemory()).WillByDefault(Return(true));
        context_state_ = base::MakeRefCounted<SharedContextState>(
            base::MakeRefCounted<gl::GLShareGroup>(),
            surf_,
            gl_context_,
            false,
            base::DoNothing(),
            gr_context_type_);
        drdc_lock_ = base::MakeRefCounted<RefCountedLock>();
        backing_ = std::make_unique<HwVideoNativeBufferImageBacking>(mailbox_,
            size_, color_space_, surface_origin_, alpha_type_, debug_label_,
            stream_texture_sii_, context_state_, drdc_lock_);
    }

    void TearDown() override {
        if (gl_context_ && surf_) {
            gl_context_->ReleaseCurrent(surf_.get());
        }
        backing_.reset();
    }

protected:
    gpu::Mailbox mailbox_;
    gfx::Size size_;
    gfx::ColorSpace color_space_;
    GrSurfaceOrigin surface_origin_;
    SkAlphaType alpha_type_;
    GrContextType gr_context_type_;
    std::string debug_label_;
    scoped_refptr<NiceMock<MockStreamTextureSharedImageInterface>> stream_texture_sii_;
    scoped_refptr<SharedContextState> context_state_;
    scoped_refptr<RefCountedLock> drdc_lock_;
    std::unique_ptr<MemoryTypeTracker> tracker_;
    std::unique_ptr<HwVideoNativeBufferImageBacking> backing_;
    base::test::TaskEnvironment task_environment_{};
    scoped_refptr<gl::GLSurface> surf_;
    scoped_refptr<gl::GLContext> gl_context_;
};

TEST_F(HwVideoNativeBufferImageBackingTest, ConstructorAndDestructor)
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

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexture_Fail)
{
    EXPECT_CALL(*stream_texture_sii_, HasTextureOwner())
        .WillOnce(Return(false));
    SharedImageManager manager;
    std::unique_ptr<GLTextureImageRepresentation> result =
        backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_EQ(result, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexture_Success)
{
    SharedImageManager manager;
    std::unique_ptr<GLTextureImageRepresentation> result =
        backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(result, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexturePassthrough_Fail)
{
    EXPECT_CALL(*stream_texture_sii_, HasTextureOwner())
        .WillOnce(Return(false));
    SharedImageManager manager;
    std::unique_ptr<GLTexturePassthroughImageRepresentation> result =
        backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_EQ(result, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexturePassthrough_Success)
{
    SharedImageManager manager;
    std::unique_ptr<GLTexturePassthroughImageRepresentation> result =
        backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_NE(result, nullptr);
    EXPECT_NE(result->GetTexturePassthrough(0), nullptr);
    EXPECT_EQ(result->GetTexturePassthrough(0)->target(), GL_TEXTURE_EXTERNAL_OES);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceSkiaGanesh_NoTextureOwner)
{
    EXPECT_CALL(*stream_texture_sii_, HasTextureOwner())
        .WillOnce(Return(false));
    SharedImageManager manager;
    std::unique_ptr<SkiaGaneshImageRepresentation> result =
        backing_->ProduceSkiaGanesh(&manager, tracker_.get(), context_state_);
    EXPECT_EQ(result, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GetEstimatedSizeForMemoryDump_UsingGpuMemory)
{
    EXPECT_CALL(*stream_texture_sii_, IsUsingGpuMemory())
        .WillOnce(Return(true));
    size_t estimated_size = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_GT(estimated_size, 0u);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GetEstimatedSizeForMemoryDump_NotUsingGpuMemory)
{
    EXPECT_CALL(*stream_texture_sii_, IsUsingGpuMemory())
        .WillOnce(Return(false));
    size_t estimated_size = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_EQ(estimated_size, 0u);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GetEstimatedSizeForMemoryDump_ConsistentResult)
{
    EXPECT_CALL(*stream_texture_sii_, IsUsingGpuMemory())
        .WillOnce(Return(true));
    size_t result1 = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_CALL(*stream_texture_sii_, IsUsingGpuMemory())
        .WillOnce(Return(true));
    size_t result2 = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_EQ(result1, result2);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexture_BeginAccess_NullBuffer)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    bool result = representation->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    EXPECT_FALSE(result);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexture_GetTexture)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    auto* texture = representation->GetTexture(0);
    EXPECT_NE(texture, nullptr);
    EXPECT_EQ(texture->target(), GL_TEXTURE_EXTERNAL_OES);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexture_DestroyWithoutAccess)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    representation.reset();
    EXPECT_NE(backing_, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexturePassthrough_BeginAccess_NullBuffer)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    bool result = representation->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    EXPECT_FALSE(result);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexturePassthrough_GetTexturePassthrough)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    const auto& texture = representation->GetTexturePassthrough(0);
    EXPECT_NE(texture, nullptr);
    EXPECT_EQ(texture->target(), GL_TEXTURE_EXTERNAL_OES);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexturePassthrough_DestroyWithoutAccess)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    representation.reset();
    EXPECT_NE(backing_, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ContextLostHelper_OnContextLost)
{
    EXPECT_CALL(*stream_texture_sii_, ReleaseResources()).Times(1);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ContextLostHelper_OnContextLost_ThenDestroy)
{
    EXPECT_CALL(*stream_texture_sii_, ReleaseResources()).Times(1);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
    EXPECT_CALL(*stream_texture_sii_, ReleaseResources()).Times(1);
    backing_.reset();
}

TEST_F(HwVideoNativeBufferImageBackingTest, ContextLostHelper_OnContextLost_StreamTextureSiiRemains)
{
    EXPECT_CALL(*stream_texture_sii_, ReleaseResources()).Times(1);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_NE(backing_->context_lost_helper_->stream_texture_sii_, nullptr);
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GetNativeBufferHandle_NullNativeBufferSync)
{
    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    auto result = backing_->GetNativeBufferHandle();
    EXPECT_FALSE(result.is_valid());
}

TEST_F(HwVideoNativeBufferImageBackingTest, GetNativeBufferHandle_NullBuffer)
{
    auto mock_native_buffer = std::make_unique<MockScopedNativeBufferFenceSync>();
    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(testing::ByMove(std::move(mock_native_buffer))));
    auto result = backing_->GetNativeBufferHandle();
    EXPECT_FALSE(result.is_valid());
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingUpdate)
{
    backing_->Update(nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingMailboxMatchesInput)
{
    EXPECT_EQ(backing_->mailbox(), mailbox_);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingSizeMatchesInput)
{
    EXPECT_EQ(backing_->size(), size_);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingColorSpaceMatchesInput)
{
    EXPECT_EQ(backing_->color_space(), color_space_);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingSurfaceOriginMatchesInput)
{
    EXPECT_EQ(backing_->surface_origin(), surface_origin_);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingAlphaTypeMatchesInput)
{
    EXPECT_EQ(backing_->alpha_type(), alpha_type_);
}

TEST_F(HwVideoNativeBufferImageBackingTest, MultipleGLTextureRepresentations)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(rep1, nullptr);
    EXPECT_NE(rep2, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, MultiplePassthroughRepresentations)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_NE(rep1, nullptr);
    EXPECT_NE(rep2, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexture_FailThenSucceed)
{
    EXPECT_CALL(*stream_texture_sii_, HasTextureOwner())
        .WillOnce(Return(false))
        .WillOnce(Return(true));
    SharedImageManager manager;
    auto fail_rep = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_EQ(fail_rep, nullptr);
    auto ok_rep = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(ok_rep, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexturePassthrough_FailThenSucceed)
{
    EXPECT_CALL(*stream_texture_sii_, HasTextureOwner())
        .WillOnce(Return(false))
        .WillOnce(Return(true));
    SharedImageManager manager;
    auto fail_rep = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_EQ(fail_rep, nullptr);
    auto ok_rep = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_NE(ok_rep, nullptr);
}



TEST_F(HwVideoNativeBufferImageBackingTest, GLTexture_BeginAccess_ReadWriteMode_NullBuffer)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    bool result = representation->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    EXPECT_FALSE(result);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexturePassthrough_BeginAccess_ReadWriteMode_NullBuffer)
{
    SharedImageManager manager;
    auto representation = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    ASSERT_NE(representation, nullptr);
    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    bool result = representation->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    EXPECT_FALSE(result);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexture_MixedResponses)
{
    EXPECT_CALL(*stream_texture_sii_, HasTextureOwner())
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

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexturePassthrough_MixedResponses)
{
    EXPECT_CALL(*stream_texture_sii_, HasTextureOwner())
        .WillOnce(Return(true))
        .WillOnce(Return(false))
        .WillOnce(Return(true));
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_NE(rep1, nullptr);
    auto rep2 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_EQ(rep2, nullptr);
    auto rep3 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_NE(rep3, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingStreamTextureSii_NotNull)
{
    EXPECT_NE(backing_->stream_texture_sii_, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingGpuMainTaskRunner_NotNull)
{
    EXPECT_NE(backing_->gpu_main_task_runner_, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingContextLostHelper_NotNull)
{
    EXPECT_NE(backing_->context_lost_helper_, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexturePassthrough_MultipleRepsOwnTextures)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    ASSERT_NE(rep1, nullptr);
    ASSERT_NE(rep2, nullptr);
    EXPECT_NE(rep1->GetTexturePassthrough(0), nullptr);
    EXPECT_NE(rep2->GetTexturePassthrough(0), nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexture_ThreeRepresentations)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep3 = backing_->ProduceGLTexture(&manager, tracker_.get());
    EXPECT_NE(rep1, nullptr);
    EXPECT_NE(rep2, nullptr);
    EXPECT_NE(rep3, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceGLTexture_ThenPassthrough)
{
    SharedImageManager manager;
    auto gl_rep = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto pt_rep = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    EXPECT_NE(gl_rep, nullptr);
    EXPECT_NE(pt_rep, nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GetEstimatedSizeForMemoryDump_AlternatingStates)
{
    EXPECT_CALL(*stream_texture_sii_, IsUsingGpuMemory())
        .WillOnce(Return(true));
    size_t size_with_gpu = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_GT(size_with_gpu, 0u);

    EXPECT_CALL(*stream_texture_sii_, IsUsingGpuMemory())
        .WillOnce(Return(false));
    size_t size_without_gpu = backing_->GetEstimatedSizeForMemoryDump();
    EXPECT_EQ(size_without_gpu, 0u);

    EXPECT_GT(size_with_gpu, size_without_gpu);
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexture_BeginAccess_MixedNullBufferResults)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexture(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexture(&manager, tracker_.get());
    ASSERT_NE(rep1, nullptr);
    ASSERT_NE(rep2, nullptr);

    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    EXPECT_FALSE(rep1->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));

    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    EXPECT_FALSE(rep2->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));
}

TEST_F(HwVideoNativeBufferImageBackingTest, GLTexturePassthrough_BeginAccess_MixedNullBufferResults)
{
    SharedImageManager manager;
    auto rep1 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    auto rep2 = backing_->ProduceGLTexturePassthrough(&manager, tracker_.get());
    ASSERT_NE(rep1, nullptr);
    ASSERT_NE(rep2, nullptr);

    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    EXPECT_FALSE(rep1->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));

    EXPECT_CALL(*stream_texture_sii_, GetNativeBuffer())
        .WillOnce(Return(nullptr));
    EXPECT_FALSE(rep2->BeginAccess(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingDebugLabel)
{
    EXPECT_FALSE(debug_label_.empty());
    EXPECT_EQ(debug_label_, "test");
}

TEST_F(HwVideoNativeBufferImageBackingTest, BackingContextStateIsGL)
{
    EXPECT_EQ(gr_context_type_, GrContextType::kGL);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ContextLostHelper_Destructor_SkipsRemoveWhenNull)
{
    EXPECT_CALL(*stream_texture_sii_, ReleaseResources()).Times(1);
    backing_->context_lost_helper_->OnContextLost();
    EXPECT_EQ(backing_->context_lost_helper_->context_state_, nullptr);
    backing_.reset();
}

TEST_F(HwVideoNativeBufferImageBackingTest, DrDcLock_NotNull)
{
    EXPECT_NE(backing_->GetDrDcLock(), nullptr);
}

TEST_F(HwVideoNativeBufferImageBackingTest, ProduceOverlay) {
    SharedImageManager manager;
    auto representation = backing_->ProduceOverlay(&manager, tracker_.get());
    EXPECT_NE(representation, nullptr);
}

}  // namespace gpu