// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on bfcache_policy.cc originally written by
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pre-include <ranges> before #define private public to prevent
// redeclaration errors in lazy_split_view internals.
#include <ranges>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

// Access private/protected members for testing.
// Placed after standard library includes (to avoid polluting them) but
// before graph_impl.h and ohos_bfcache_policy.h whose private members
// we need to access.
#define protected public
#define private public
#include "components/performance_manager/graph/graph_impl.h"
#include "ohos_bfcache_policy.h"
#undef private
#undef protected

using namespace performance_manager::policies;
using namespace performance_manager;
using namespace content;
using MemoryPressureLevel = base::MemoryPressureLevel;

class GraphMock : public performance_manager::GraphImpl {
 public:
  GraphMock() {}
  ~GraphMock() {}

  void RemovePageNodeObserver(PageNodeObserver* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  void RemoveSystemNodeObserver(SystemNodeObserver* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  void AddSystemNodeObserver(SystemNodeObserver* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  void AddPageNodeObserver(PageNodeObserver* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  void SetLifecycleState() {
    lifecycle_state_ = LifecycleState::kTearDownCalled;
  }
};

class PageNodeMock : public PageNode {
 public:
  base::flat_set<const Node*> main_frame_nodes_set;

  PageNodeMock() {}

  static const char* ToString(performance_manager::PageType type) {
    return "";
  }
  static const char* ToString(PageNode::LoadingState loading_state) {
    return "";
  }
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kPage; }
  const std::string& GetBrowserContextID() const {
    static std::string id;
    return id;
  }
  const FrameNode* GetOpenerFrameNode() const { return nullptr; }
  const FrameNode* GetEmbedderFrameNode() const { return nullptr; }
  resource_attribution::PageContext GetResourceContext() const {
    return resource_attribution::PageContext::FromPageNode(nullptr);
  }
  performance_manager::PageType GetType() const {
    return performance_manager::PageType::kUnknown;
  }
  bool IsFocused() const { return false; }
  void SetIsVisible(bool visibility) {}
  bool IsVisible() const { return false; }
  base::TimeTicks GetLastVisibilityChangeTime() const {
    return base::TimeTicks();
  }
  base::TimeDelta GetTimeSinceLastVisibilityChange() const {
    return base::TimeDelta();
  }
  void SetIsAudible(bool audible) {}
  bool IsAudible() const { return false; }
  std::optional<base::TimeDelta> GetTimeSinceLastAudibleChange() const {
    return std::nullopt;
  }
  bool HasPictureInPicture() const { return false; }
  bool HasFreezingOriginTrialOptOut() const { return false; }
  bool IsOffTheRecord() const { return false; }

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
  void SetIsMediaPlaying(bool media_playing) {}
  bool IsMediaPlaying() const { return false; }
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

  LoadingState GetLoadingState() const {
    return LoadingState::kLoadingNotStarted;
  }
  ukm::SourceId GetUkmSourceID() const { return ukm::kInvalidSourceId; }
  LifecycleState GetLifecycleState() const {
    return LifecycleState::kRunning;
  }
  bool IsHoldingWebLock() const { return false; }
  bool IsHoldingBlockingIndexedDBLock() const { return false; }
  bool IsHoldingIndexedDBLock() const { return false; }
  bool UsesWebRTC() const { return false; }
  int64_t GetNavigationID() const { return 0; }
  const std::string& GetContentsMimeType() const {
    static std::string mime;
    return mime;
  }
  std::optional<blink::mojom::PermissionStatus>
  GetNotificationPermissionStatus() const {
    return std::nullopt;
  }
  base::TimeDelta GetTimeSinceLastNavigation() const {
    return base::TimeDelta();
  }
  const FrameNode* GetMainFrameNode() const { return nullptr; }
  NodeSetView<const FrameNode*> GetMainFrameNodes() const {
    return NodeSetView<const FrameNode*>(main_frame_nodes_set);
  }
  const GURL& GetMainFrameUrl() const {
    static GURL url;
    return url;
  }
  base::ByteCount EstimateMainFramePrivateFootprintSize() const {
    return base::ByteCount();
  }
  bool HadFormInteraction() const { return false; }
  bool HadUserEdits() const { return false; }
  base::WeakPtr<content::WebContents> GetWebContents() const {
    return base::WeakPtr<content::WebContents>();
  }
  base::ByteCount EstimateResidentSetSize() const {
    return base::ByteCount();
  }
  base::ByteCount EstimatePrivateFootprintSize() const {
    return base::ByteCount();
  }
  base::WeakPtr<PageNode> GetWeakPtr() {
    return base::WeakPtr<PageNode>();
  }
  base::WeakPtr<const PageNode> GetWeakPtr() const {
    return base::WeakPtr<const PageNode>();
  }
  const void* GetImpl() const { return nullptr; }
  uintptr_t GetImplType() const { return 0; }
  NodeState GetNodeState() const { return NodeState::kActiveInGraph; }
  Graph* GetGraph() const { return nullptr; }
};

class FrameNodeMock : public FrameNode {
 public:
  bool is_current = true;

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kFrame; }
  bool IsCurrent() const override { return is_current; }
  const FrameNode* GetParentFrameNode() const { return nullptr; }
  const FrameNode* GetParentOrOuterDocumentOrEmbedder() const {
    return nullptr;
  }
  const PageNode* GetPageNode() const { return nullptr; }
  const ProcessNode* GetProcessNode() const { return nullptr; }
  const blink::LocalFrameToken& GetFrameToken() const {
    static blink::LocalFrameToken token;
    return token;
  }
  content::BrowsingInstanceId GetBrowsingInstanceId() const {
    return content::BrowsingInstanceId();
  }
  content::SiteInstanceGroupId GetSiteInstanceGroupId() const {
    return content::SiteInstanceGroupId();
  }
  resource_attribution::FrameContext GetResourceContext() const {
    return resource_attribution::FrameContext::FromFrameNode(nullptr);
  }
  bool IsMainFrame() const { return true; }
  NodeSetView<const FrameNode*> GetChildFrameNodes() const {
    static base::flat_set<const Node*> empty;
    return NodeSetView<const FrameNode*>(empty);
  }
  NodeSetView<const PageNode*> GetOpenedPageNodes() const {
    static base::flat_set<const Node*> empty;
    return NodeSetView<const PageNode*>(empty);
  }
  NodeSetView<const PageNode*> GetEmbeddedPageNodes() const {
    static base::flat_set<const Node*> empty;
    return NodeSetView<const PageNode*>(empty);
  }
  LifecycleState GetLifecycleState() const {
    return LifecycleState::kRunning;
  }
  bool HasNonemptyBeforeUnload() const { return false; }
  const GURL& GetURL() const {
    static GURL url;
    return url;
  }
  const std::optional<url::Origin>& GetOrigin() const {
    static std::optional<url::Origin> origin;
    return origin;
  }
  bool IsActive() const { return false; }
  const PriorityAndReason& GetPriorityAndReason() const {
    static PriorityAndReason pr;
    return pr;
  }
  bool GetNetworkAlmostIdle() const { return false; }
  bool IsAdFrame() const { return false; }
  bool IsHoldingWebLock() const { return false; }
  bool IsHoldingBlockingIndexedDBLock() const { return false; }
  bool UsesWebRTC() const { return false; }
  NodeSetView<const WorkerNode*> GetChildWorkerNodes() const {
    static base::flat_set<const Node*> empty;
    return NodeSetView<const WorkerNode*>(empty);
  }
  bool HadUserActivation() const { return false; }
  bool HadFormInteraction() const { return false; }
  bool HadUserEdits() const { return false; }
  bool IsAudible() const { return false; }
  bool IsCapturingMediaStream() const { return false; }
  bool HasFreezingOriginTrialOptOut() const { return false; }
  ViewportIntersection GetViewportIntersection() const { return {}; }
  Visibility GetVisibility() const { return Visibility::kVisible; }
  bool IsIntersectingLargeArea() const { return false; }
  bool IsImportant() const { return false; }
  bool IsRendered() const { return false; }
  const RenderFrameHostProxy& GetRenderFrameHostProxy() const {
    static RenderFrameHostProxy proxy;
    return proxy;
  }
  base::ByteCount GetResidentSetEstimate() const {
    return base::ByteCount();
  }
  base::ByteCount GetPrivateFootprintEstimate() const {
    return base::ByteCount();
  }
  void CrossProcessSubframeRenderProcessGone() {}
  const void* GetImpl() const { return nullptr; }
  uintptr_t GetImplType() const { return 0; }
  NodeState GetNodeState() const { return NodeState::kActiveInGraph; }
  Graph* GetGraph() const { return nullptr; }
};

class TestOHOSBFCachePolicy : public OHOSBFCachePolicy {
 public:
  bool flush_called = false;
  const PageNode* last_page = nullptr;
  MemoryPressureLevel last_level =
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE;

 protected:
  void MaybeFlushBFCache(const PageNode* page_node,
                         MemoryPressureLevel level) override {
    flush_called = true;
    last_page = page_node;
    last_level = level;
  }
};

TEST(OHOS_BFCACHE_POLICY_TEST, OnPassedToGraph) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  auto graph_mock = std::make_shared<GraphMock>();
  graph_mock->SetLifecycleState();
  ohos_bfcache_policy->OnPassedToGraph(graph_mock.get());
}

TEST(OHOS_BFCACHE_POLICY_TEST, OnPassedToGraph001) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();
  ohos_bfcache_policy->OnPassedToGraph(graph.get());
  graph->TearDown();
}

TEST(OHOS_BFCACHE_POLICY_TEST, OnTakenFromGraph) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  auto graph_mock = std::make_shared<GraphMock>();
  graph_mock->SetLifecycleState();
  ohos_bfcache_policy->OnTakenFromGraph(graph_mock.get());
}

// NONE pressure level should early return
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  ohos_bfcache_policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE);
}

// owningGraph=null, MODERATE should return early without crash
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure001) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();

  PageNodeMock page_node_mock;
  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&page_node_mock);

  ohos_bfcache_policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  graph->TearDown();
}

// owningGraph=null with single page, MODERATE
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure002) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();

  PageNodeMock page_node_mock;
  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&page_node_mock);

  ohos_bfcache_policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  graph->TearDown();
}

// owningGraph=null with multiple pages declared, MODERATE
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure003) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();

  PageNodeMock pageA;
  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&pageA);

  ohos_bfcache_policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  graph->TearDown();
}

// owningGraph=null with three pages declared, MODERATE
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure004) {
  auto ohos_bfcache_policy = std::make_shared<OHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();

  PageNodeMock pageA;
  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&pageA);

  ohos_bfcache_policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  graph->TearDown();
}

// owningGraph=null, MODERATE should not flush
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure005) {
  auto policy = std::make_shared<TestOHOSBFCachePolicy>();
  policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(policy->flush_called);
}

// owningGraph set, no pages, MODERATE should not flush
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure006) {
  auto policy = std::make_shared<TestOHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();
  policy->graph_ = graph.get();

  policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(policy->flush_called);

  policy->graph_ = nullptr;
  graph->TearDown();
}

// page with no frames should not flush
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure007) {
  auto policy = std::make_shared<TestOHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();
  policy->graph_ = graph.get();

  PageNodeMock page;
  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&page);

  policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(policy->flush_called);

  policy->graph_ = nullptr;
  graph->TearDown();
}

// page with single current frame should not flush
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure008) {
  auto policy = std::make_shared<TestOHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();
  policy->graph_ = graph.get();

  PageNodeMock page;
  FrameNodeMock frame;
  page.main_frame_nodes_set.insert(&frame);

  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&page);

  policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(policy->flush_called);

  policy->graph_ = nullptr;
  graph->TearDown();
}

// page with two current frames should not flush
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure009) {
  auto policy = std::make_shared<TestOHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();
  policy->graph_ = graph.get();

  PageNodeMock page;
  FrameNodeMock frame1;
  FrameNodeMock frame2;
  page.main_frame_nodes_set.insert(&frame1);
  page.main_frame_nodes_set.insert(&frame2);

  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&page);

  policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_FALSE(policy->flush_called);

  policy->graph_ = nullptr;
  graph->TearDown();
}

// page with one non-current frame should flush
TEST(OHOS_BFCACHE_POLICY_TEST, OnMemoryPressure010) {
  auto policy = std::make_shared<TestOHOSBFCachePolicy>();
  std::unique_ptr<GraphImpl> graph = std::make_unique<GraphImpl>();
  graph->SetUp();
  policy->graph_ = graph.get();

  PageNodeMock page;
  FrameNodeMock frame1;
  FrameNodeMock frame2;
  frame2.is_current = false;
  page.main_frame_nodes_set.insert(&frame1);
  page.main_frame_nodes_set.insert(&frame2);

  const size_t page_index = static_cast<size_t>(PageNodeMock::Type());
  CHECK(page_index < graph->nodes_.size());
  graph->nodes_[page_index].insert(&page);

  policy->OnMemoryPressure(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  EXPECT_TRUE(policy->flush_called);
  EXPECT_EQ(policy->last_page, &page);
  EXPECT_EQ(policy->last_level,
            MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);

  policy->graph_ = nullptr;
  graph->TearDown();
}
