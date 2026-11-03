/* Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <memory>
#include <utility>

#include "arkweb/chromium_ext/content/browser/renderer_host/render_widget_host_view_child_frame_ext.h"

#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/mock_widget.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "content/test/test_render_widget_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {
namespace {

// CrossProcessFrameConnector mock whose GetRootRenderWidgetHostView() returns a
// settable view, so GetScrollable()'s root-view branches can be exercised.
// (GetRootRenderWidgetHostView is virtual on CrossProcessFrameConnector.)
class MockFrameConnectorExt : public CrossProcessFrameConnector {
 public:
  MockFrameConnectorExt() : CrossProcessFrameConnector(nullptr) {}
  ~MockFrameConnectorExt() override = default;

  RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override {
    return root_view_;
  }
  void SetRootView(RenderWidgetHostViewBase* view) { root_view_ = view; }

 private:
  raw_ptr<RenderWidgetHostViewBase> root_view_ = nullptr;
};

// Minimal RWHV with a controllable GetScrollable(), used as the root view.
// GetScrollable() is pure-virtual on RenderWidgetHostView; TestRenderWidgetHost
// View supplies the remaining interface.
class ScrollableMockView : public TestRenderWidgetHostView {
 public:
  explicit ScrollableMockView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh) {}
  ~ScrollableMockView() override = default;

  bool GetScrollable() override { return scrollable_; }
  void SetScrollable(bool scrollable) { scrollable_ = scrollable; }

 private:
  bool scrollable_ = true;
};

class RenderWidgetHostViewChildFrameExtTest
    : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    process_host_ =
        std::make_unique<MockRenderProcessHost>(browser_context());
    site_instance_group_ = base::WrapRefCounted(SiteInstanceGroup::CreateForTesting(
        browser_context(), process_host_.get()));
    int32_t routing_id = process_host_->GetNextRoutingID();

    widget_host_ = RenderWidgetHostFactory::Create(
        /*frame_tree=*/&contents()->GetPrimaryFrameTree(), &delegate_,
        RenderWidgetHostImpl::DefaultFrameSinkId(*site_instance_group_,
                                                 routing_id),
        site_instance_group_->GetSafeRef(), routing_id,
        /*hidden=*/false, /*renderer_initiated_creation=*/false);

    widget_host_->BindWidgetInterfaces(
        mojo::AssociatedRemote<blink::mojom::WidgetHost>()
            .BindNewEndpointAndPassDedicatedReceiver(),
        widget_.GetNewRemote());
    widget_host_->BindFrameWidgetInterfaces(
        mojo::AssociatedRemote<blink::mojom::FrameWidgetHost>()
            .BindNewEndpointAndPassDedicatedReceiver(),
        TestRenderWidgetHost::CreateStubFrameWidgetRemote());

    display::ScreenInfos screen_infos;
    view_ = new RenderWidgetHostViewChildFrameExt(widget_host_.get(),
                                                  screen_infos);
  }

  void TearDown() override {
    if (view_) {
      RenderWidgetHostViewChildFrameExt* local = view_;
      view_ = nullptr;
      local->Destroy();
    }
    root_view_.reset();
    frame_connector_.reset();
    widget_host_.reset();
    site_instance_group_.reset();
    process_host_->Cleanup();
    process_host_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  RenderWidgetHostViewChildFrameExt* view_ext() { return view_; }

 protected:
  std::unique_ptr<MockRenderProcessHost> process_host_;
  scoped_refptr<SiteInstanceGroup> site_instance_group_;
  MockRenderWidgetHostDelegate delegate_;
  MockWidget widget_;
  std::unique_ptr<RenderWidgetHostImpl> widget_host_;
  raw_ptr<RenderWidgetHostViewChildFrameExt> view_ = nullptr;
  std::unique_ptr<MockFrameConnectorExt> frame_connector_;
  std::unique_ptr<ScrollableMockView> root_view_;
};

// frame_connector_ is null right after construction -> GetScrollable() takes the
// `if (!frame_connector_)` true arm and returns true (line 27).
TEST_F(RenderWidgetHostViewChildFrameExtTest, GetScrollable_NullConnector) {
  EXPECT_TRUE(view_ext()->GetScrollable());
}

// Connector set, but root view null -> root_view && ... is false, so falls
// through to `return true` (line 33). Covers the !frame_connector_ false arm
// (line 26) and the root_view-null arm (line 30).
TEST_F(RenderWidgetHostViewChildFrameExtTest, GetScrollable_NullRootView) {
  frame_connector_ = std::make_unique<MockFrameConnectorExt>();
  view_ext()->SetFrameConnector(frame_connector_.get());
  EXPECT_TRUE(view_ext()->GetScrollable());
}

// Root view present and scrollable -> `root_view && !GetScrollable()` short
// circuits to false -> return true (line 33).
TEST_F(RenderWidgetHostViewChildFrameExtTest, GetScrollable_RootScrollable) {
  frame_connector_ = std::make_unique<MockFrameConnectorExt>();
  root_view_ = std::make_unique<ScrollableMockView>(widget_host_.get());
  root_view_->SetScrollable(true);
  frame_connector_->SetRootView(root_view_.get());
  view_ext()->SetFrameConnector(frame_connector_.get());
  EXPECT_TRUE(view_ext()->GetScrollable());
}

// Root view present but not scrollable -> `root_view && !GetScrollable()` is
// true -> return false (line 31).
TEST_F(RenderWidgetHostViewChildFrameExtTest, GetScrollable_RootNotScrollable) {
  frame_connector_ = std::make_unique<MockFrameConnectorExt>();
  root_view_ = std::make_unique<ScrollableMockView>(widget_host_.get());
  root_view_->SetScrollable(false);
  frame_connector_->SetRootView(root_view_.get());
  view_ext()->SetFrameConnector(frame_connector_.get());
  EXPECT_FALSE(view_ext()->GetScrollable());
}

// IsMarkedConsumed returns true only when !GetScrollable() && type ==
// kGestureScrollUpdate. Covers both arms of the compound condition (line 38/39).
TEST_F(RenderWidgetHostViewChildFrameExtTest, IsMarkedConsumed) {
  // Null connector -> scrollable -> never consumed, regardless of type.
  EXPECT_FALSE(view_ext()->IsMarkedConsumed(
      blink::WebInputEvent::Type::kGestureScrollUpdate));
  EXPECT_FALSE(view_ext()->IsMarkedConsumed(
      blink::WebInputEvent::Type::kGestureScrollEnd));

  // Root not scrollable + scroll-update -> consumed (line 40).
  frame_connector_ = std::make_unique<MockFrameConnectorExt>();
  root_view_ = std::make_unique<ScrollableMockView>(widget_host_.get());
  root_view_->SetScrollable(false);
  frame_connector_->SetRootView(root_view_.get());
  view_ext()->SetFrameConnector(frame_connector_.get());
  EXPECT_TRUE(view_ext()->IsMarkedConsumed(
      blink::WebInputEvent::Type::kGestureScrollUpdate));
  // Root not scrollable but a non-scroll-update type -> not consumed (the
  // type == kGestureScrollUpdate false arm of line 39).
  EXPECT_FALSE(view_ext()->IsMarkedConsumed(
      blink::WebInputEvent::Type::kGestureScrollEnd));
}

TEST_F(RenderWidgetHostViewChildFrameExtTest,
       IsTouchSequencePotentiallyActiveOnViz) {
  EXPECT_FALSE(view_ext()->IsTouchSequencePotentiallyActiveOnViz());
}

}  // namespace
}  // namespace content
