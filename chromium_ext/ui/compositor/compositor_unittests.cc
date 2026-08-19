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
#include "base/containers/flat_map.h"
#define private public
#define protected public
#include "ui/compositor/compositor.h"

#include <gtest/gtest.h>

#include <memory>

#include "arkweb/build/features/features.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "content/browser/browser_thread_impl.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "ui/compositor/test/test_context_factories.h"
#include "arkweb/chromium_ext/ui/compositor/compositor_utils.h"
#include "arkweb/chromium_ext/components/viz/host/host_frame_sink_manager_utils.h"
#include "components/viz/host/host_frame_sink_manager.h"
#undef private
#undef protected

namespace ui {
namespace {
std::shared_ptr<Compositor> g_compositor = nullptr;
}

class CompositorTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

private:
    base::test::TaskEnvironment task_environment_{base::test::TaskEnvironment::MainThreadType::IO};
    std::unique_ptr<TestContextFactories> context_factories_;
};

void CompositorTest::SetUpTestCase(void)
{}

void CompositorTest::TearDownTestCase(void)
{}

void CompositorTest::SetUp(void)
{
    context_factories_ = std::make_unique<TestContextFactories>(false);
    bool enable_pixel_canvas = false;
    bool use_external_begin_frame_control = false;
    bool force_software_compositor = false;
    bool enable_compositing_based_throttling = false;
    size_t memory_limit_when_visible_mb = 0;
    g_compositor = std::make_shared<Compositor>(context_factories_->GetContextFactory()->AllocateFrameSinkId(),
        context_factories_->GetContextFactory(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        enable_pixel_canvas,
        use_external_begin_frame_control,
        force_software_compositor,
        enable_compositing_based_throttling,
        memory_limit_when_visible_mb);
}

void CompositorTest::TearDown(void)
{
    g_compositor = nullptr;
    context_factories_.reset();
}

#if BUILDFLAG(ARKWEB_SYNC_RENDER)
TEST_F(CompositorTest, SetDrawRect)
{
    ASSERT_NE(g_compositor, nullptr);
    gfx::Rect rect(10, 20, 30, 40);
    g_compositor->Utils()->SetDrawRect(rect);
}

TEST_F(CompositorTest, SetDrawRect_DisplayPrivateBound)
{
    ASSERT_NE(g_compositor, nullptr);
    auto receiver = g_compositor->display_private_.BindNewEndpointAndPassDedicatedReceiver();
    ASSERT_TRUE(g_compositor->display_private_.is_bound());

    gfx::Rect rect(10, 20, 30, 40);
    g_compositor->Utils()->SetDrawRect(rect);

    g_compositor->display_private_.reset();
}
#endif

TEST_F(CompositorTest, SetDrawMode)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetDrawMode(0);
    g_compositor->Utils()->SetDrawMode(1);
}

TEST_F(CompositorTest, SetDrawMode_DisplayPrivateBound)
{
    ASSERT_NE(g_compositor, nullptr);
    auto receiver = g_compositor->display_private_.BindNewEndpointAndPassDedicatedReceiver();
    ASSERT_TRUE(g_compositor->display_private_.is_bound());

    g_compositor->Utils()->SetDrawMode(0);
    g_compositor->Utils()->SetDrawMode(1);

    g_compositor->display_private_.reset();
}

TEST_F(CompositorTest, SetNativeInnerWeb)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetNativeInnerWeb(true);
    g_compositor->Utils()->SetNativeInnerWeb(false);
}

TEST_F(CompositorTest, SetNativeInnerWeb_DisplayPrivateBound)
{
    ASSERT_NE(g_compositor, nullptr);
    auto receiver = g_compositor->display_private_.BindNewEndpointAndPassDedicatedReceiver();
    ASSERT_TRUE(g_compositor->display_private_.is_bound());

    g_compositor->Utils()->SetNativeInnerWeb(true);
    g_compositor->Utils()->SetNativeInnerWeb(false);

    g_compositor->display_private_.reset();
}

TEST_F(CompositorTest, SetBypassVsyncCondition)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetBypassVsyncCondition(0);
    g_compositor->Utils()->SetBypassVsyncCondition(1);
}

TEST_F(CompositorTest, SetBypassVsyncCondition_DisplayPrivateBound)
{
    ASSERT_NE(g_compositor, nullptr);
    auto receiver = g_compositor->display_private_.BindNewEndpointAndPassDedicatedReceiver();
    ASSERT_TRUE(g_compositor->display_private_.is_bound());

    g_compositor->Utils()->SetBypassVsyncCondition(0);
    g_compositor->Utils()->SetBypassVsyncCondition(1);

    g_compositor->display_private_.reset();
}

TEST_F(CompositorTest, SetShouldFrameSubmissionBeforeDraw)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetShouldFrameSubmissionBeforeDraw(false);
    g_compositor->Utils()->SetShouldFrameSubmissionBeforeDraw(true);
}

TEST_F(CompositorTest, SendInternalBeginFrame)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SendInternalBeginFrame();
}

TEST_F(CompositorTest, SetEnableLowerFrameRate)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetEnableLowerFrameRate(false);
    g_compositor->Utils()->SetEnableLowerFrameRate(true);
}

TEST_F(CompositorTest, SetEnableHalfFrameRate)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetEnableHalfFrameRate(false);
    g_compositor->Utils()->SetEnableHalfFrameRate(true);
}

TEST_F(CompositorTest, SetIsOfflineWebComponentInactive)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetIsOfflineWebComponentInactive(true);
    g_compositor->Utils()->SetIsOfflineWebComponentInactive(false);
}

TEST_F(CompositorTest, EvictFrameBackBuffers)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->EvictFrameBackBuffers();
}

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
TEST_F(CompositorTest, SetIfNeedCleanBuffers)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->SetIfNeedCleanBuffers(false));
    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->SetIfNeedCleanBuffers(true));
}

TEST_F(CompositorTest, SetIfNeedCleanBuffers_CompositorNull)
{
    CompositorUtils utils(nullptr);
    ASSERT_NO_FATAL_FAILURE(utils.SetIfNeedCleanBuffers(true));
    ASSERT_NO_FATAL_FAILURE(utils.SetIfNeedCleanBuffers(false));
}

TEST_F(CompositorTest, SetIfNeedCleanBuffers_ContextFactoryNull)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    raw_ptr<ui::ContextFactory> saved_factory = g_compositor->context_factory_;
    g_compositor->context_factory_ = nullptr;

    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->SetIfNeedCleanBuffers(true));

    g_compositor->context_factory_ = saved_factory;
}

TEST_F(CompositorTest, SetIfNeedCleanBuffers_ManagerUtilsNull)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);

    std::unique_ptr<viz::HostFrameSinkManagerUtils> saved_manager_utils = std::move(host_manager->managerUtils);

    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->SetIfNeedCleanBuffers(true));

    host_manager->managerUtils = std::move(saved_manager_utils);
}
#endif

TEST_F(CompositorTest, UpdateVSyncFrequency)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->UpdateVSyncFrequency();
}

TEST_F(CompositorTest, ResetVSyncFrequency)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->ResetVSyncFrequency();
}

#if BUILDFLAG(IS_ARKWEB) && BUILDFLAG(ARKWEB_PERFORMANCE_JITTER)
TEST_F(CompositorTest, SetCurrentFrameSinkId_DisplayPrivateUnbound)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_FALSE(g_compositor->display_private_.is_bound());

    const viz::FrameSinkId id(0, 0);
    g_compositor->Utils()->SetCurrentFrameSinkId(id);
}

TEST_F(CompositorTest, SetCurrentFrameSinkId_DisplayPrivateBound)
{
    ASSERT_NE(g_compositor, nullptr);
    auto receiver = g_compositor->display_private_.BindNewEndpointAndPassDedicatedReceiver();
    ASSERT_TRUE(g_compositor->display_private_.is_bound());

    const viz::FrameSinkId id(1, 1);
    g_compositor->Utils()->SetCurrentFrameSinkId(id);

    g_compositor->display_private_.reset();
}
#endif

TEST_F(CompositorTest, DisableSwapUntilMaximized)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->DisableSwapUntilMaximized();
}

TEST_F(CompositorTest, DisableSwapUntilMaximized_DisplayPrivateBound)
{
    ASSERT_NE(g_compositor, nullptr);
    auto receiver = g_compositor->display_private_.BindNewEndpointAndPassDedicatedReceiver();
    ASSERT_TRUE(g_compositor->display_private_.is_bound());

    g_compositor->Utils()->DisableSwapUntilMaximized();

    g_compositor->display_private_.reset();
}

TEST_F(CompositorTest, SetPipActive)
{
    ASSERT_NE(g_compositor, nullptr);
    g_compositor->Utils()->SetPipActive(false);
    g_compositor->Utils()->SetPipActive(true);
}

namespace {
class NullHostManagerContextFactory : public ContextFactory {
public:
    void CreateLayerTreeFrameSink(base::WeakPtr<Compositor> compositor) override
    {}
    scoped_refptr<viz::RasterContextProvider> SharedMainThreadRasterContextProvider() override
    {
        return nullptr;
    }
    void RemoveCompositor(Compositor *compositor) override
    {}
    cc::TaskGraphRunner *GetTaskGraphRunner() override
    {
        return nullptr;
    }
    viz::FrameSinkId AllocateFrameSinkId() override
    {
        return viz::FrameSinkId();
    }
    viz::SubtreeCaptureId AllocateSubtreeCaptureId() override
    {
        return viz::SubtreeCaptureId();
    }
    viz::HostFrameSinkManager *GetHostFrameSinkManager() override
    {
        return nullptr;
    }
};
}  // namespace

TEST_F(CompositorTest, SetIfNeedCleanBuffers_HostFrameSinkManagerNull)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);

    NullHostManagerContextFactory mock_factory;
    raw_ptr<ui::ContextFactory> saved_factory = g_compositor->context_factory_;
    g_compositor->context_factory_ = &mock_factory;

    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->SetIfNeedCleanBuffers(true));
    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->SetIfNeedCleanBuffers(false));

    g_compositor->context_factory_ = saved_factory;
}

namespace {
class MockHostFrameSinkClient : public viz::HostFrameSinkClient {
public:
    void OnFirstSurfaceActivation(const viz::SurfaceInfo &surface_info) override
    {}
    void OnFrameTokenChanged(uint32_t frame_token, base::TimeTicks activation_time) override
    {}
};
}  // namespace

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
TEST_F(CompositorTest, UtilsSetIfNeedCleanBuffers_NullHostManager)
{
    viz::HostFrameSinkManagerUtils utils(nullptr);
    ASSERT_NO_FATAL_FAILURE(utils.SetIfNeedCleanBuffers(viz::FrameSinkId(1, 1), true));
    ASSERT_NO_FATAL_FAILURE(utils.SetIfNeedCleanBuffers(viz::FrameSinkId(1, 1), false));
}

TEST_F(CompositorTest, UtilsSetIfNeedCleanBuffers_NullFrameSinkManager)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    auto saved_fsm = host_manager->frame_sink_manager_;
    host_manager->frame_sink_manager_ = nullptr;

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->SetIfNeedCleanBuffers(g_compositor->frame_sink_id(), true));

    host_manager->frame_sink_manager_ = saved_fsm;
}
#endif

#if BUILDFLAG(ARKWEB_VIDEO_LTPO)
TEST_F(CompositorTest, UtilsUpdateVSyncFrequency_UnregisteredId)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->UpdateVSyncFrequency(viz::FrameSinkId(999, 999)));
}

TEST_F(CompositorTest, UtilsUpdateVSyncFrequency_WithChildren)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    auto &data = host_manager->frame_sink_data_map_[g_compositor->frame_sink_id()];
    data.children.push_back(viz::FrameSinkId(888, 888));

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->UpdateVSyncFrequency(g_compositor->frame_sink_id()));

    data.children.clear();
}
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
TEST_F(CompositorTest, UtilsClearBlanklessSnapshotInfo_Valid)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->ClearBlanklessSnapshotInfo(12345));
}

TEST_F(CompositorTest, UtilsClearBlanklessSnapshotInfo_NullFrameSinkManager)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    auto saved_fsm = host_manager->frame_sink_manager_;
    host_manager->frame_sink_manager_ = nullptr;

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->ClearBlanklessSnapshotInfo(12345));

    host_manager->frame_sink_manager_ = saved_fsm;
}
#endif

TEST_F(CompositorTest, UtilsRestoreRenderFit_UnregisteredId)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->UtilsRestoreRenderFit(999, 999));
}

TEST_F(CompositorTest, UtilsRestoreRenderFit_RegisteredIdNullClient)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    const auto &fsid = g_compositor->frame_sink_id();
    auto &data = host_manager->frame_sink_data_map_[fsid];
    auto saved_client = data.client;
    data.client = nullptr;

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->UtilsRestoreRenderFit(fsid.client_id(), fsid.sink_id()));

    data.client = saved_client;
}

TEST_F(CompositorTest, UtilsRestoreRenderFit_RegisteredIdWithClient)
{
    ASSERT_NE(g_compositor, nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    ASSERT_NE(host_manager->managerUtils, nullptr);

    const auto &fsid = g_compositor->frame_sink_id();
    auto &data = host_manager->frame_sink_data_map_[fsid];
    auto saved_client = data.client;

    MockHostFrameSinkClient mock_client;
    data.client = &mock_client;

    ASSERT_NO_FATAL_FAILURE(host_manager->managerUtils->UtilsRestoreRenderFit(fsid.client_id(), fsid.sink_id()));

    data.client = saved_client;
}

TEST_F(CompositorTest, IsValidManagerUtils_IsValid)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    EXPECT_TRUE(g_compositor->Utils()->IsValidManagerUtils());
}

TEST_F(CompositorTest, IsValidManagerUtils_CompositorNull)
{
    CompositorUtils utils(nullptr);
    EXPECT_FALSE(utils.IsValidManagerUtils());
}

TEST_F(CompositorTest, IsValidManagerUtils_ContextFactoryNull)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    raw_ptr<ui::ContextFactory> saved_factory = g_compositor->context_factory_;
    g_compositor->context_factory_ = nullptr;
    EXPECT_FALSE(g_compositor->Utils()->IsValidManagerUtils());
    g_compositor->context_factory_ = saved_factory;
}

TEST_F(CompositorTest, IsValidManagerUtils_HostFrameSinkManagerNull)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    NullHostManagerContextFactory mock_factory;
    raw_ptr<ui::ContextFactory> saved_factory = g_compositor->context_factory_;
    g_compositor->context_factory_ = &mock_factory;
    EXPECT_FALSE(g_compositor->Utils()->IsValidManagerUtils());
    g_compositor->context_factory_ = saved_factory;
}

TEST_F(CompositorTest, IsValidManagerUtils_ManagerUtilsNull)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    auto *host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
    ASSERT_NE(host_manager, nullptr);
    std::unique_ptr<viz::HostFrameSinkManagerUtils> saved_manager_utils = std::move(host_manager->managerUtils);
    EXPECT_FALSE(g_compositor->Utils()->IsValidManagerUtils());
    host_manager->managerUtils = std::move(saved_manager_utils);
}

TEST_F(CompositorTest, DiscardCompositingResources_Valid)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->DiscardCompositingResources());
}

TEST_F(CompositorTest, DiscardCompositingResources_Invalid)
{
    CompositorUtils utils(nullptr);
    ASSERT_NO_FATAL_FAILURE(utils.DiscardCompositingResources());
}

TEST_F(CompositorTest, UpdateFocusAndScreenStatus_Valid)
{
    ASSERT_NE(g_compositor, nullptr);
    ASSERT_NE(g_compositor->Utils(), nullptr);
    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->UpdateFocusAndScreenStatus(true, true));
    ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->UpdateFocusAndScreenStatus(false, false));
}

TEST_F(CompositorTest, UpdateFocusAndScreenStatus_Invalid)
{
    CompositorUtils utils(nullptr);
    ASSERT_NO_FATAL_FAILURE(utils.UpdateFocusAndScreenStatus(true, true));
    ASSERT_NO_FATAL_FAILURE(utils.UpdateFocusAndScreenStatus(false, false));
}

TEST_F(CompositorTest, IsValidManagerUtils_AllNull) {
  CompositorUtils utils(nullptr);
  EXPECT_FALSE(utils.IsValidManagerUtils());
}

TEST_F(CompositorTest, IsValidManagerUtils_CompositorNull02) {
  CompositorUtils utils(nullptr);
  EXPECT_FALSE(utils.IsValidManagerUtils());
}

TEST_F(CompositorTest, IsValidManagerUtils_ContextFactoryNull02) {
  ASSERT_NE(g_compositor, nullptr);
  raw_ptr<ui::ContextFactory> saved_factory = g_compositor->context_factory_;
  g_compositor->context_factory_ = nullptr;
  EXPECT_FALSE(g_compositor->Utils()->IsValidManagerUtils());
  g_compositor->context_factory_ = saved_factory;
}

TEST_F(CompositorTest, IsValidManagerUtils_ManagerUtilsNull02) {
  ASSERT_NE(g_compositor, nullptr);
  auto* host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
  ASSERT_NE(host_manager, nullptr);
  std::unique_ptr<viz::HostFrameSinkManagerUtils> saved_utils =
      std::move(host_manager->managerUtils);
  EXPECT_FALSE(g_compositor->Utils()->IsValidManagerUtils());
  host_manager->managerUtils = std::move(saved_utils);
}

TEST_F(CompositorTest, IsValidManagerUtils_AllValid) {
  ASSERT_NE(g_compositor, nullptr);
  EXPECT_NE(g_compositor->context_factory_, nullptr);
  auto* host_manager = g_compositor->context_factory_->GetHostFrameSinkManager();
  ASSERT_NE(host_manager, nullptr);
  ASSERT_NE(host_manager->managerUtils, nullptr);
  EXPECT_TRUE(g_compositor->Utils()->IsValidManagerUtils());
}

TEST_F(CompositorTest, DiscardCompositingResources_InvalidManager) {
  CompositorUtils utils(nullptr);
  ASSERT_NO_FATAL_FAILURE(utils.DiscardCompositingResources());
}

TEST_F(CompositorTest, DiscardCompositingResources_ValidManager) {
  ASSERT_NE(g_compositor, nullptr);
  ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->DiscardCompositingResources());
}

TEST_F(CompositorTest, UpdateFocusAndScreenStatus_InvalidManager) {
  CompositorUtils utils(nullptr);
  ASSERT_NO_FATAL_FAILURE(utils.UpdateFocusAndScreenStatus(true, true));
}

TEST_F(CompositorTest, UpdateFocusAndScreenStatus_ValidManager) {
  ASSERT_NE(g_compositor, nullptr);
  ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->UpdateFocusAndScreenStatus(true, true));
  ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->UpdateFocusAndScreenStatus(true, false));
  ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->UpdateFocusAndScreenStatus(false, true));
  ASSERT_NO_FATAL_FAILURE(g_compositor->Utils()->UpdateFocusAndScreenStatus(false, false));
}

}  // namespace ui
