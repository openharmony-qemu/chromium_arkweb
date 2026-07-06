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
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_damage_tracker.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/test/task_environment.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display/display_resource_provider_null.h"
#include "components/viz/service/display/null_renderer.h"

#define private public
#define protected public
#include "components/viz/service/display/display.h"
#include "arkweb/chromium_ext/components/viz/service/display/arkweb_display_utils.h"
#include "arkweb/chromium_ext/components/viz/service/display/arkweb_display_utils.cc"
#undef private
#undef protected

namespace viz {

class DisplaySchedulerMock : public DisplaySchedulerBase {
 public:
  DisplaySchedulerMock() = default;
  ~DisplaySchedulerMock() override = default;
  void OnDisplayDamaged(SurfaceId surface_id) override {}
  void OnRootFrameMissing(bool missing) override {}
  void OnPendingSurfacesChanged() override {}
  void SetVisible(bool visible) override {}
  void ForceImmediateSwapIfPossible() override {}
  void SetNeedsOneBeginFrame(const BeginFrameArgs& args, bool needs_draw) {}
  void DidSwapBuffers() override {}
  void DidReceiveSwapBuffersAck() override {}
  void OutputSurfaceLost() override {}
  void ReportFrameTime(
      base::TimeDelta frame_time,
      base::flat_set<base::PlatformThreadId> animation_thread_ids,
      base::flat_set<base::PlatformThreadId> renderer_main_thread_ids,
      base::TimeTicks draw_start,
      HintSession::BoostType boost_type) override {}
#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
  void SetShouldFrameSubmissionBeforeDraw(bool should) override {}
#endif
#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
  void ReenableSwapCheck(const SurfaceId& surface_id,
                         int width,
                         int height) override {}
#endif
#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
  void SetClientId(const uint32_t client_id) {}
#endif
};

class OutputSurfaceMock : public OutputSurface {
 public:
  OutputSurfaceMock() {
    capabilities_.skips_draw = true;
  }
  ~OutputSurfaceMock() override = default;
  void BindToClient(OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void CleanBufferAfterSwapBuffer(bool delay_clean) override {}
  void Reshape(const ReshapeParams& params) override {}
  void SwapBuffers(OutputSurfaceFrame frame) override {}
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  void SetBypassVsyncCondition(int32_t condition) override {}
  void SetNativeInnerWeb(bool isInnerWeb) override {}
  gfx::OverlayTransform GetDisplayTransform() override {
    return gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
  }
#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
  void SetIfNeedCleanBuffers(bool need_clean_buffers) override {
    need_clean_buffers_ = need_clean_buffers;
  }
  bool need_clean_buffers_ = false;
#endif
};

class DisplayClientMock : public DisplayClient {
 public:
  DisplayClientMock() = default;
  ~DisplayClientMock() override = default;
  void DisplayOutputSurfaceLost() override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      AggregatedRenderPassList* render_passes) override {}
  void DisplayDidDrawAndSwap() override {}
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}
  void DisplayAddChildWindowToBrowser(
      gpu::SurfaceHandle child_window) override {}
  void SetWideColorEnabled(bool enabled) override {}
  void SetPreferredFrameInterval(base::TimeDelta interval) {}
  void ModifyRenderFit(int32_t fitType, const FrameSinkId& frame_sink_id) {}
#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
  void RestoreRenderFit(const FrameSinkId& frame_sink_id) override {}
#endif
#if BUILDFLAG(ARKWEB_EVICT_UNLOCK_FRAMES)
  void DisplayDidRealSwapBuffer() override {}
#endif
};

class DumpFrameObserverTest : public testing::Test {
 public:
  DumpFrameObserverTest() = default;
  ~DumpFrameObserverTest() = default;

  void TestPropertiesUpdateAndShouldDump() {
    DumpFrameObserver observer;
    std::string test_string = "true";
    observer.PropertiesUpdate(test_string.c_str());
    EXPECT_TRUE(observer.ShouldDump());

    test_string = "false";
    observer.PropertiesUpdate(test_string.c_str());
    EXPECT_FALSE(observer.ShouldDump());

    observer.dump_param_list_.push_back("true");
    observer.PropertiesUpdate("true");
    EXPECT_TRUE(observer.ShouldDump());

    observer.PropertiesUpdate("a#b#c");
    EXPECT_EQ(observer.dump_param_list_.size(), 3);
    EXPECT_EQ(observer.dump_param_list_[0], "a");
    EXPECT_EQ(observer.dump_param_list_[1], "b");
    EXPECT_EQ(observer.dump_param_list_[2], "c");
  }

  void TestShouldDumpInFreqAndDumpPath(){
    DumpFrameObserver observer;
    observer.dump_param_list_.clear();
    EXPECT_EQ(observer.DumpPath(), "");
    observer.dump_param_list_.push_back("true");
    EXPECT_FALSE(observer.ShouldDumpInFreq());

    observer.dump_param_list_.push_back("1");
    EXPECT_TRUE(observer.ShouldDumpInFreq());

    observer.dump_param_list_.pop_back();
    observer.dump_param_list_.push_back("2");
    EXPECT_FALSE(observer.ShouldDumpInFreq());

    observer.dump_param_list_.push_back("3");
    EXPECT_EQ(observer.DumpPath(), "3");
  }
};

TEST_F(DumpFrameObserverTest, PropertiesUpdateAndShouldDump) {
  TestPropertiesUpdateAndShouldDump();
}

TEST_F(DumpFrameObserverTest, ShouldDumpInFreqAndDumpPath){
  TestShouldDumpInFreqAndDumpPath();
}

class ArkwebDisplayUtilsTest : public testing::Test {
 public:
  std::unique_ptr<Display> CreateTestDisplay(bool auto_resize = false) {
    RendererSettings settings;
    settings.auto_resize_output_surface = auto_resize;
    FrameSinkId frame_sink_id(1, 1);
    auto output_surface = std::make_unique<OutputSurfaceMock>();
    auto scheduler = std::make_unique<DisplaySchedulerMock>();
    auto overlay_processor = std::make_unique<viz::OverlayProcessorStub>();
    auto display = std::make_unique<Display>(
        nullptr, nullptr, settings,
        nullptr, frame_sink_id, nullptr,
        std::move(output_surface), std::move(overlay_processor),
        std::move(scheduler), nullptr);
    auto resource_provider = std::make_unique<DisplayResourceProviderNull>();
    display->renderer_ = std::make_unique<NullRenderer>(
        &display->settings_, nullptr,
        display->output_surface_.get(),
        resource_provider.get(),
        display->overlay_processor_.get());
    display->resource_provider_ = std::move(resource_provider);
    return display;
  }
};

TEST_F(ArkwebDisplayUtilsTest, ConstructorNullDisplay) {
  ArkwebDisplayUtils utils(nullptr);
  EXPECT_EQ(utils.display_, nullptr);
}

#if BUILDFLAG(ARKWEB_DFX_DUMP)
TEST_F(ArkwebDisplayUtilsTest, ConstructorDumpFrameObserverCreated) {
  ArkwebDisplayUtils utils(nullptr);
  EXPECT_NE(utils.dump_frame_observer_, nullptr);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwapDump_NullObserver) {
  ArkwebDisplayUtils utils(nullptr);
  utils.dump_frame_observer_ = nullptr;
  AggregatedFrame frame;
  utils.DrawAndSwapDump(frame);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwapDump_NotShouldDump) {
  ArkwebDisplayUtils utils(nullptr);
  ASSERT_NE(utils.dump_frame_observer_, nullptr);
  utils.dump_frame_observer_->should_dump_ = false;
  utils.dump_frame_observer_->dump_param_list_.clear();
  AggregatedFrame frame;
  utils.DrawAndSwapDump(frame);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwapDump_ShouldDumpNotInFreq) {
  ArkwebDisplayUtils utils(nullptr);
  ASSERT_NE(utils.dump_frame_observer_, nullptr);
  utils.dump_frame_observer_->should_dump_ = true;
  utils.dump_frame_observer_->dump_param_list_.clear();
  AggregatedFrame frame;
  utils.DrawAndSwapDump(frame);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwapDump_ShouldDumpInFreq_NoRenderPass) {
  ArkwebDisplayUtils utils(nullptr);
  ASSERT_NE(utils.dump_frame_observer_, nullptr);
  utils.dump_frame_observer_->should_dump_ = true;
  utils.dump_frame_observer_->dump_param_list_.push_back("true");
  utils.dump_frame_observer_->dump_param_list_.push_back("1");
  AggregatedFrame frame;
  frame.render_pass_list.clear();
  utils.DrawAndSwapDump(frame);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwapDump_ShouldDumpInFreq_WithRenderPass) {
  ArkwebDisplayUtils utils(nullptr);
  ASSERT_NE(utils.dump_frame_observer_, nullptr);
  utils.dump_frame_observer_->should_dump_ = true;
  utils.dump_frame_observer_->dump_param_list_.push_back("true");
  utils.dump_frame_observer_->dump_param_list_.push_back("1");
  AggregatedFrame frame;
  auto pass = std::make_unique<AggregatedRenderPass>();
  frame.render_pass_list.emplace_back(std::move(pass));
  utils.DrawAndSwapDump(frame);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwapDump_ShouldDumpInFreq_NullRootPass) {
  ArkwebDisplayUtils utils(nullptr);
  ASSERT_NE(utils.dump_frame_observer_, nullptr);
  utils.dump_frame_observer_->should_dump_ = true;
  utils.dump_frame_observer_->dump_param_list_.push_back("true");
  utils.dump_frame_observer_->dump_param_list_.push_back("1");
  AggregatedFrame frame;
  frame.render_pass_list.emplace_back(nullptr);
  utils.DrawAndSwapDump(frame);
}
#endif

#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
TEST_F(ArkwebDisplayUtilsTest, ConstructorTimersNotNull) {
  ArkwebDisplayUtils utils(nullptr);
  EXPECT_NE(utils.reset_init_timer_, nullptr);
  EXPECT_NE(utils.reenable_swap_timer_, nullptr);
}

TEST_F(ArkwebDisplayUtilsTest, ShouldDisableSwap_InitState) {
  ArkwebDisplayUtils utils(nullptr);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  EXPECT_TRUE(utils.ShouldDisableSwap(true));
  EXPECT_FALSE(utils.ShouldDisableSwap(false));
}

TEST_F(ArkwebDisplayUtilsTest, ShouldDisableSwap_DisableSwapState) {
  ArkwebDisplayUtils utils(nullptr);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  EXPECT_FALSE(utils.ShouldDisableSwap(true));
  EXPECT_FALSE(utils.ShouldDisableSwap(false));
}

TEST_F(ArkwebDisplayUtilsTest, ShouldDisableSwap_ReenableSwapState) {
  ArkwebDisplayUtils utils(nullptr);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP;
  EXPECT_TRUE(utils.ShouldDisableSwap(true));
  EXPECT_FALSE(utils.ShouldDisableSwap(false));
}

TEST_F(ArkwebDisplayUtilsTest, ShouldDisableSwap_RestoreRenderFitState) {
  ArkwebDisplayUtils utils(nullptr);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::RESTORE_RENDERFIT;
  EXPECT_TRUE(utils.ShouldDisableSwap(true));
  EXPECT_FALSE(utils.ShouldDisableSwap(false));
}

TEST_F(ArkwebDisplayUtilsTest, DisableSwapUntilMaximized) {
  ArkwebDisplayUtils utils(nullptr);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  utils.DisableSwapUntilMaximized();
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::INIT);
}

TEST_F(ArkwebDisplayUtilsTest, DisableSwapUntilMaximized_NullTimer) {
  ArkwebDisplayUtils utils(nullptr);
  utils.reset_init_timer_ = nullptr;
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  utils.DisableSwapUntilMaximized();
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::INIT);
}

TEST_F(ArkwebDisplayUtilsTest, ReenableSwapCheck_NotDisabledState) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  FrameSinkId fsid(1, 1);
  SurfaceId sid(fsid, LocalSurfaceId());
  utils.ReenableSwapCheck(sid, 100, 100);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::INIT);
}

TEST_F(ArkwebDisplayUtilsTest, ReenableSwapCheck_DifferentSurfaceId) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  FrameSinkId other_fsid(2, 2);
  base::UnguessableToken token = base::UnguessableToken::Create();
  LocalSurfaceId other_lsid(1, token);
  SurfaceId other_sid(other_fsid, other_lsid);
  utils.ReenableSwapCheck(other_sid, 100, 100);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, ReenableSwapCheck_SizeNotMet) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  SurfaceId sid = display->CurrentSurfaceId();
  utils.ReenableSwapCheck(sid, 50, 50);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, ReenableSwapCheck_SizeMet) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  SurfaceId sid = display->CurrentSurfaceId();
  utils.ReenableSwapCheck(sid, 100, 100);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, ReenableSwapCheck_SizeExceeds) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  SurfaceId sid = display->CurrentSurfaceId();
  utils.ReenableSwapCheck(sid, 200, 200);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, RestoreRenderFit_NotReenableState) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.RestoreRenderFit();
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::INIT);
}

TEST_F(ArkwebDisplayUtilsTest, RestoreRenderFit_ReenableState_NoClient) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP;
  utils.RestoreRenderFit();
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, RestoreRenderFit_ReenableState_WithClient) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  auto client = std::make_unique<DisplayClientMock>();
  display->client_ = client.get();
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP;
  utils.RestoreRenderFit();
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::RESTORE_RENDERFIT);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_SmallSize_InitState) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.Resize(gfx::Size(50, 50));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::INIT);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_LargeSize_InitState) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.Resize(gfx::Size(200, 200));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_LargeSize_RestoreRenderFitState) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::RESTORE_RENDERFIT;
  utils.Resize(gfx::Size(200, 200));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::RESTORE_RENDERFIT);
}
#endif

#if BUILDFLAG(ARKWEB_SYNC_RENDER)
TEST_F(ArkwebDisplayUtilsTest, SetDrawRect_ModeOne) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetDrawMode(1);
  gfx::Rect test_rect(10, 20, 30, 40);
  utils.SetDrawRect(test_rect);
  EXPECT_EQ(utils.draw_rect_, test_rect);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_BoundsCheck_MaxSurfaceSize) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(7000, 7000);
  utils.SetDrawMode(0);
  utils.Resize(gfx::Size(9000, 9000));
  EXPECT_EQ(display->current_surface_size_.width(), 8000);
  EXPECT_EQ(display->current_surface_size_.height(), 8000);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_DrawModeZero_SmallSize) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(7000, 7000);
  utils.SetDrawMode(0);
  utils.Resize(gfx::Size(100, 100));
  EXPECT_EQ(display->current_surface_size_.width(), 100);
  EXPECT_EQ(display->current_surface_size_.height(), 100);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_DrawModeOne_FitContentSizeRange) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(7000, 7000);
  utils.SetDrawMode(1);
  utils.Resize(gfx::Size(100, 100));
  EXPECT_EQ(display->current_surface_size_.width(), 100);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_DrawModeOne_OutOfFitContentRange) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.SetDrawMode(1);
  utils.Resize(gfx::Size(500, 500));
  EXPECT_EQ(display->current_surface_size_.width(), 500);
}
#endif

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_DrawModeZero) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(0);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(20, 20);
  last_render_pass.damage_rect = gfx::Rect(20, 20);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0u);
#endif
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_DrawModeOne) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(1);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(10, 20);
  last_render_pass.damage_rect = gfx::Rect(20, 20);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 1u);
#endif
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_AutoResizeTrue_DamageMatchesOutput) {
  auto display = CreateTestDisplay(true);
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(0);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(20, 20);
  last_render_pass.damage_rect = gfx::Rect(20, 20);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 1u);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_AutoResizeTrue_DamageNotMatch) {
  auto display = CreateTestDisplay(true);
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(0);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(20, 20);
  last_render_pass.damage_rect = gfx::Rect(5, 5);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0u);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_AutoResizeTrue_EmptyCurrentSize) {
  auto display = CreateTestDisplay(true);
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(0);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(20, 20);
  last_render_pass.damage_rect = gfx::Rect(20, 20);
  gfx::Size current_surface_size(0, 0);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0u);
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_AutoResizeFalse) {
  auto display = CreateTestDisplay(false);
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(0);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(20, 20);
  last_render_pass.damage_rect = gfx::Rect(20, 20);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0u);
#endif
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_EmptyDamageRect) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(0);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(0, 0);
  last_render_pass.damage_rect = gfx::Rect(0, 0);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0u);
#endif
}

TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_AutoResizeTrue_SizeSameAsCurrent) {
  auto display = CreateTestDisplay(true);
  ArkwebDisplayUtils utils(display.get());
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
  utils.SetDrawMode(0);
#endif
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(10, 10);
  last_render_pass.damage_rect = gfx::Rect(10, 10);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0u);
}

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
TEST_F(ArkwebDisplayUtilsTest, DiscardBackbuffer) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.DiscardBackbuffer();
}
#endif

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
TEST_F(ArkwebDisplayUtilsTest, SetIfNeedCleanBuffers_NullDisplay) {
  ArkwebDisplayUtils utils(nullptr);
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_FALSE(utils.need_clean_buffers_);
  utils.SetIfNeedCleanBuffers(false);
  EXPECT_FALSE(utils.need_clean_buffers_);
}

TEST_F(ArkwebDisplayUtilsTest, SetIfNeedCleanBuffers_WithDisplay) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
  utils.SetIfNeedCleanBuffers(false);
  EXPECT_FALSE(utils.need_clean_buffers_);
}

TEST_F(ArkwebDisplayUtilsTest, ReallocatedFrameBuffersIfNeed_NotNeeded) {
  ArkwebDisplayUtils utils(nullptr);
  utils.need_clean_buffers_ = false;
  utils.ReallocatedFrameBuffersIfNeed();
  EXPECT_FALSE(utils.need_clean_buffers_);
}

TEST_F(ArkwebDisplayUtilsTest, ReallocatedFrameBuffersIfNeed_Needed) {
  ArkwebDisplayUtils utils(nullptr);
  utils.need_clean_buffers_ = true;
  utils.ReallocatedFrameBuffersIfNeed();
  EXPECT_TRUE(utils.need_clean_buffers_);
}
#endif

#if BUILDFLAG(ARKWEB_OFFLINE_WEB_EVICT_BACK_BUFFERS)
TEST_F(ArkwebDisplayUtilsTest, CleanBufferAfterSwapBuffer) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.CleanBufferAfterSwapBuffer(true);
  utils.CleanBufferAfterSwapBuffer(false);
}
#endif

#if BUILDFLAG(ARKWEB_COMPOSITE_RENDER)
TEST_F(ArkwebDisplayUtilsTest, SetShouldFrameSubmissionBeforeDraw) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetShouldFrameSubmissionBeforeDraw(true);
  utils.SetShouldFrameSubmissionBeforeDraw(false);
}
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
TEST_F(ArkwebDisplayUtilsTest, SetClientId) {
  ArkwebDisplayUtils utils(nullptr);
  utils.SetClientId(42);
  EXPECT_EQ(utils.client_id_, 42u);
}

TEST_F(ArkwebDisplayUtilsTest, SetGpuServiceImpl_Null) {
  ArkwebDisplayUtils utils(nullptr);
  utils.SetGpuServiceImpl(nullptr);
  EXPECT_EQ(utils.gpu_service_impl_, nullptr);
}

TEST_F(ArkwebDisplayUtilsTest, DumpSnapshotForBlankLess_NullGpuServiceImpl) {
  ArkwebDisplayUtils utils(nullptr);
  AggregatedFrame frame;
  utils.DumpSnapshotForBlankLess(frame);
}

TEST_F(ArkwebDisplayUtilsTest, DumpSnapshotForBlankLess_WithDisplay_NullGpuServiceImpl) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  AggregatedFrame frame;
  utils.DumpSnapshotForBlankLess(frame);
}
#endif
#if BUILDFLAG(ARKWEB_SYNC_RENDER)
TEST_F(ArkwebDisplayUtilsTest, DrawAndSwap_DrawRectSet) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetDrawMode(1);
  gfx::Rect draw_rect(5, 10, 30, 40);
  utils.SetDrawRect(draw_rect);
  AggregatedRenderPass last_render_pass;
  last_render_pass.output_rect = gfx::Rect(30, 40);
  last_render_pass.damage_rect = gfx::Rect(30, 40);
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 1u);
  EXPECT_EQ(last_render_pass.output_rect.x(), 5);
  EXPECT_EQ(last_render_pass.output_rect.y(), 10);
}
#endif

#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
TEST_F(ArkwebDisplayUtilsTest, ReenableSwapCheck_InvalidSurfaceId) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  SurfaceId invalid_sid;
  utils.ReenableSwapCheck(invalid_sid, 100, 100);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, ReenableSwapCheck_TimerRunning) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  utils.reenable_swap_timer_->Reset();
  SurfaceId sid = display->CurrentSurfaceId();
  utils.ReenableSwapCheck(sid, 100, 100);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
  EXPECT_FALSE(utils.reenable_swap_timer_->IsRunning());
}

TEST_F(ArkwebDisplayUtilsTest, Resize_InitTimerRunning) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.reset_init_timer_->Reset();
  ASSERT_TRUE(utils.reset_init_timer_->IsRunning());
  utils.Resize(gfx::Size(200, 200));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
  EXPECT_FALSE(utils.reset_init_timer_->IsRunning());
}

TEST_F(ArkwebDisplayUtilsTest, Resize_WidthOnlyExceeds) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.Resize(gfx::Size(200, 50));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsTest, Resize_HeightOnlyExceeds) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->current_surface_size_ = gfx::Size(100, 100);
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.Resize(gfx::Size(50, 200));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}
#endif

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
TEST_F(ArkwebDisplayUtilsTest, DiscardBackbuffer_RendererNotNull) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  ASSERT_NE(display->renderer_, nullptr);
  utils.DiscardBackbuffer();
}

TEST_F(ArkwebDisplayUtilsTest, DiscardBackbuffer_OutputSurfaceNull) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->output_surface_.reset();
  utils.DiscardBackbuffer();
}
#endif

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
TEST_F(ArkwebDisplayUtilsTest, ReallocatedFrameBuffersIfNeed_WithDisplay_CleanBuffersTrue) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_TRUE(utils.need_clean_buffers_);
  utils.ReallocatedFrameBuffersIfNeed();
}
#endif

#if BUILDFLAG(ARKWEB_OFFLINE_WEB_EVICT_BACK_BUFFERS)
TEST_F(ArkwebDisplayUtilsTest, CleanBufferAfterSwapBuffer_OutputSurfaceNull) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  display->output_surface_.reset();
  utils.CleanBufferAfterSwapBuffer(true);
}
#endif

class ArkwebDisplayUtilsBranchTest : public testing::Test {
 protected:
  std::unique_ptr<Display> CreateTestDisplay(bool auto_resize = false) {
    RendererSettings settings;
    settings.auto_resize_output_surface = auto_resize;
    FrameSinkId frame_sink_id(1, 1);
    auto output_surface = std::make_unique<OutputSurfaceMock>();
    auto scheduler = std::make_unique<DisplaySchedulerMock>();
    auto overlay_processor = std::make_unique<viz::OverlayProcessorStub>();
    auto display = std::make_unique<Display>(
        nullptr, nullptr, settings,
        nullptr, frame_sink_id, nullptr,
        std::move(output_surface), std::move(overlay_processor),
        std::move(scheduler), nullptr);
    auto resource_provider = std::make_unique<DisplayResourceProviderNull>();
    display->renderer_ = std::make_unique<NullRenderer>(
        &display->settings_, nullptr,
        display->output_surface_.get(),
        resource_provider.get(),
        display->overlay_processor_.get());
    display->resource_provider_ = std::move(resource_provider);
    return display;
  }
};

#if BUILDFLAG(ARKWEB_MAXIMIZE_RESIZE)
TEST_F(ArkwebDisplayUtilsBranchTest, ReenableSwapCheck_ValidSurfaceMatchesCurrent) {
  auto display = CreateTestDisplay();
  FrameSinkId fsid(1, 1);
  base::UnguessableToken token = base::UnguessableToken::Create();
  LocalSurfaceId lsid(1, token);
  SurfaceId current_sid(fsid, lsid);
  display->current_surface_id_ = current_sid;
  display->current_surface_size_ = gfx::Size(10, 10);
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  utils.ReenableSwapCheck(current_sid, 11, 11);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, ReenableSwapCheck_PartialWidthHeight) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(10, 10);
  ArkwebDisplayUtils utils(display.get());
  SurfaceId sid;

  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  utils.ReenableSwapCheck(sid, 11, 5);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);

  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  utils.ReenableSwapCheck(sid, 5, 11);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, ReenableSwapCheck_TimerNull) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(10, 10);
  ArkwebDisplayUtils utils(display.get());
  SurfaceId sid;

  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  utils.reenable_swap_timer_ = nullptr;
  utils.ReenableSwapCheck(sid, 11, 11);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, ReenableSwapCheck_TimerNotRunning) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(10, 10);
  ArkwebDisplayUtils utils(display.get());
  SurfaceId sid;

  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP;
  ASSERT_NE(utils.reenable_swap_timer_, nullptr);
  ASSERT_FALSE(utils.reenable_swap_timer_->IsRunning());
  utils.ReenableSwapCheck(sid, 11, 11);
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::REENABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_InitWiderNotTaller) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(10, 20);
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.reset_init_timer_ = nullptr;
  utils.reenable_swap_timer_ = nullptr;
  utils.Resize(gfx::Size(20, 10));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_InitTallerNotWider) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(20, 10);
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.reset_init_timer_ = nullptr;
  utils.reenable_swap_timer_ = nullptr;
  utils.Resize(gfx::Size(10, 20));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_InitNeitherBigger) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(20, 20);
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.Resize(gfx::Size(10, 10));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::INIT);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_InitTimerNull) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(10, 10);
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.reset_init_timer_ = nullptr;
  utils.reenable_swap_timer_ = nullptr;
  utils.Resize(gfx::Size(20, 20));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_InitTimerNotRunning) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(10, 10);
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.reset_init_timer_ = std::make_unique<base::RetainingOneShotTimer>();
  ASSERT_FALSE(utils.reset_init_timer_->IsRunning());
  utils.reenable_swap_timer_ = nullptr;
  utils.Resize(gfx::Size(20, 20));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_ReenableSwapTimerNull) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(10, 10);
  ArkwebDisplayUtils utils(display.get());
  utils.temp_idle_state_ = ArkwebDisplayUtils::TempIdleState::INIT;
  utils.reset_init_timer_ = nullptr;
  utils.reenable_swap_timer_ = nullptr;
  utils.Resize(gfx::Size(20, 20));
  EXPECT_EQ(utils.temp_idle_state_, ArkwebDisplayUtils::TempIdleState::DISABLE_SWAP);
}
#endif

#if BUILDFLAG(ARKWEB_SYNC_RENDER)
TEST_F(ArkwebDisplayUtilsBranchTest, Resize_WidthClampOnly) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.Resize(gfx::Size(9000, 5000));
  EXPECT_EQ(display->current_surface_size_.width(), 8000);
  EXPECT_EQ(display->current_surface_size_.height(), 5000);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_HeightClampOnly) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.Resize(gfx::Size(5000, 9000));
  EXPECT_EQ(display->current_surface_size_.width(), 5000);
  EXPECT_EQ(display->current_surface_size_.height(), 8000);
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_DrawModeHeightBelowMin) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(0, 5000);
  ArkwebDisplayUtils utils(display.get());
  utils.SetDrawMode(1);
  utils.Resize(gfx::Size(100, 100));
  EXPECT_EQ(display->current_surface_size_, gfx::Size(100, 100));
}

TEST_F(ArkwebDisplayUtilsBranchTest, Resize_DrawModeHeightAboveMax) {
  auto display = CreateTestDisplay();
  display->current_surface_size_ = gfx::Size(0, 9000);
  ArkwebDisplayUtils utils(display.get());
  utils.SetDrawMode(1);
  utils.Resize(gfx::Size(100, 100));
  EXPECT_EQ(display->current_surface_size_, gfx::Size(100, 100));
}
#endif

TEST_F(ArkwebDisplayUtilsBranchTest, DrawAndSwap_AutoResizeFalse) {
  viz::AggregatedRenderPass last_render_pass;
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetDrawMode(0);
  last_render_pass.output_rect = gfx::Rect(20, 20);
  last_render_pass.damage_rect = gfx::Rect(20, 20);
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0U);
}

TEST_F(ArkwebDisplayUtilsBranchTest, DrawAndSwap_OutputRectMatchesCurrentSize) {
  viz::AggregatedRenderPass last_render_pass;
  gfx::Size current_surface_size(10, 10);
  AggregatedFrame frame;
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetDrawMode(0);
  last_render_pass.output_rect = gfx::Rect(10, 10);
  last_render_pass.damage_rect = gfx::Rect(10, 10);
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0U);
}

TEST_F(ArkwebDisplayUtilsBranchTest, DrawAndSwap_CurrentSurfaceSizeEmpty) {
  viz::AggregatedRenderPass last_render_pass;
  gfx::Size current_surface_size(0, 0);
  AggregatedFrame frame;
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.SetDrawMode(0);
  last_render_pass.output_rect = gfx::Rect(20, 20);
  last_render_pass.damage_rect = gfx::Rect(20, 20);
  utils.DrawAndSwap(last_render_pass, current_surface_size, frame);
  EXPECT_EQ(frame.surface_damage_rect_list_.size(), 0U);
}

#if BUILDFLAG(ARKWEB_DFX_DUMP)
TEST_F(ArkwebDisplayUtilsBranchTest, DrawAndSwapDump_ObserverExistsShouldDumpFalse) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.dump_frame_observer_ = std::make_unique<DumpFrameObserver>();
  utils.dump_frame_observer_->should_dump_ = false;
  utils.dump_frame_observer_->dump_param_list_.clear();
  AggregatedFrame frame;
  utils.DrawAndSwapDump(frame);
}

TEST_F(ArkwebDisplayUtilsBranchTest, DrawAndSwapDump_ShouldDumpTrueFreqFalse) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.dump_frame_observer_ = std::make_unique<DumpFrameObserver>();
  utils.dump_frame_observer_->should_dump_ = true;
  utils.dump_frame_observer_->dump_param_list_.push_back("true");
  utils.dump_frame_observer_->dump_param_list_.push_back("1");
  utils.dump_frame_observer_->dump_freq_count = 0;
  AggregatedFrame frame;
  utils.DrawAndSwapDump(frame);
}

TEST_F(ArkwebDisplayUtilsBranchTest, DrawAndSwapDump_NullRootRenderPass) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.dump_frame_observer_ = std::make_unique<DumpFrameObserver>();
  utils.dump_frame_observer_->should_dump_ = true;
  utils.dump_frame_observer_->dump_param_list_.push_back("true");
  utils.dump_frame_observer_->dump_param_list_.push_back("1");
  utils.dump_frame_observer_->dump_freq_count = 1;
  AggregatedFrame frame;
  frame.render_pass_list.emplace_back(nullptr);
  utils.DrawAndSwapDump(frame);
}
#endif

#if BUILDFLAG(ARKWEB_CLEAN_BUFFERS_WHEN_INVISIBLE)
TEST_F(ArkwebDisplayUtilsBranchTest, SetIfNeedCleanBuffers_DisplayNoOutputSurface) {
  auto display = CreateTestDisplay();
  OutputSurface* saved = display->output_surface_.release();
  ArkwebDisplayUtils utils(display.get());
  utils.SetIfNeedCleanBuffers(true);
  EXPECT_FALSE(utils.need_clean_buffers_);
  display->output_surface_.reset(saved);
}
#endif

#if BUILDFLAG(ARKWEB_OCCLUDED_OPT)
TEST_F(ArkwebDisplayUtilsBranchTest, DiscardBackbuffer_OutputSurfaceExists) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.DiscardBackbuffer();
}

TEST_F(ArkwebDisplayUtilsBranchTest, DiscardBackbuffer_OutputSurfaceNull) {
  auto display = CreateTestDisplay();
  OutputSurface* saved = display->output_surface_.release();
  ArkwebDisplayUtils utils(display.get());
  utils.DiscardBackbuffer();
  display->output_surface_.reset(saved);
}
#endif

#if BUILDFLAG(ARKWEB_OFFLINE_WEB_EVICT_BACK_BUFFERS)
TEST_F(ArkwebDisplayUtilsBranchTest, CleanBufferAfterSwapBuffer_OutputSurfaceExists) {
  auto display = CreateTestDisplay();
  ArkwebDisplayUtils utils(display.get());
  utils.CleanBufferAfterSwapBuffer(true);
  utils.CleanBufferAfterSwapBuffer(false);
}

TEST_F(ArkwebDisplayUtilsBranchTest, CleanBufferAfterSwapBuffer_OutputSurfaceNull) {
  auto display = CreateTestDisplay();
  OutputSurface* saved = display->output_surface_.release();
  ArkwebDisplayUtils utils(display.get());
  utils.CleanBufferAfterSwapBuffer(true);
  display->output_surface_.reset(saved);
}
#endif

#if BUILDFLAG(ARKWEB_BLANK_OPTIMIZE)
TEST_F(ArkwebDisplayUtilsBranchTest, DumpSnapshotForBlankLess_GpuServiceImplNull) {
  ArkwebDisplayUtils utils(nullptr);
  AggregatedFrame frame;
  utils.DumpSnapshotForBlankLess(frame);
}
#endif
}  // namespace viz