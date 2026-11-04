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

#include "ohos_nweb/browser/performance_manager/nweb_performance_manager.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

#include "arkweb/build/features/features.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/graph/node_set_view.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/system_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

// background_task_policy.h handles private/public via BUILDFLAG(ARKWEB_TEST).
// For fuzz builds where ARKWEB_TEST may not be set, expose private members.
#if !BUILDFLAG(ARKWEB_TEST)
#define private public
#endif  // !BUILDFLAG(ARKWEB_TEST)
#include "ohos_nweb/browser/performance_manager/policies/background_task_policy.h"
#if !BUILDFLAG(ARKWEB_TEST)
#undef private
#endif  // !BUILDFLAG(ARKWEB_TEST)

namespace {

template <typename T>
T NotReached() {
  CHECK(false) << "Unexpected call to stub method";
#if defined(__GNUC__)
  __builtin_unreachable();
#endif  // defined(__GNUC__)
}

class MockPageNode : public performance_manager::PageNode {
 public:
  MockPageNode() = default;

  void SetVisible(bool v) { visible_ = v; }

  bool IsVisible() const override { return visible_; }
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
  bool IsAudible() const override { return false; }
  bool IsMediaPlaying() const override { return false; }
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

  performance_manager::Graph* GetGraph() const override { return nullptr; }
  uintptr_t GetImplType() const override { return 0; }
  const void* GetImpl() const override { return nullptr; }
  const std::string& GetBrowserContextID() const override {
    return empty_string_;
  }
  const performance_manager::FrameNode* GetOpenerFrameNode() const override {
    return nullptr;
  }
  const performance_manager::FrameNode* GetEmbedderFrameNode() const override {
    return nullptr;
  }
  resource_attribution::PageContext GetResourceContext() const override {
    return NotReached<resource_attribution::PageContext>();
  }
  performance_manager::PageType GetType() const override {
    return performance_manager::PageType::kTab;
  }
  bool IsFocused() const override { return false; }
  base::TimeTicks GetLastVisibilityChangeTime() const override {
    return base::TimeTicks();
  }
  std::optional<base::TimeDelta> GetTimeSinceLastAudibleChange()
      const override {
    return std::nullopt;
  }
  bool HasPictureInPicture() const override { return false; }
  bool HasFreezingOriginTrialOptOut() const override { return false; }
  bool IsOffTheRecord() const override { return false; }
  LoadingState GetLoadingState() const override {
    return LoadingState::kLoadingNotStarted;
  }
  ukm::SourceId GetUkmSourceID() const override { return 0; }
  LifecycleState GetLifecycleState() const override {
    return LifecycleState::kRunning;
  }
  bool IsHoldingWebLock() const override { return false; }
  bool IsHoldingBlockingIndexedDBLock() const override { return false; }
  bool UsesWebRTC() const override { return false; }
  int64_t GetNavigationID() const override { return 0; }
  const std::string& GetContentsMimeType() const override {
    return empty_string_;
  }
  std::optional<blink::mojom::PermissionStatus>
  GetNotificationPermissionStatus() const override {
    return std::nullopt;
  }
  base::TimeDelta GetTimeSinceLastNavigation() const override {
    return base::TimeDelta();
  }
  const performance_manager::FrameNode* GetMainFrameNode() const override {
    return nullptr;
  }
  NodeSetView<const performance_manager::FrameNode*> GetMainFrameNodes()
      const override {
    static const performance_manager::PageNode::NodeSet empty_set;
    return NodeSetView<const performance_manager::FrameNode*>(empty_set);
  }
  const GURL& GetMainFrameUrl() const override { return empty_url_; }
  base::ByteCount EstimateMainFramePrivateFootprintSize() const override {
    return base::ByteCount();
  }
  bool HadFormInteraction() const override { return false; }
  bool HadUserEdits() const override { return false; }
  base::WeakPtr<content::WebContents> GetWebContents() const override {
    return base::WeakPtr<content::WebContents>();
  }
  base::ByteCount EstimateResidentSetSize() const override {
    return base::ByteCount();
  }
  base::ByteCount EstimatePrivateFootprintSize() const override {
    return base::ByteCount();
  }
  base::WeakPtr<performance_manager::PageNode> GetWeakPtr() override {
    return base::WeakPtr<performance_manager::PageNode>();
  }
  base::WeakPtr<const performance_manager::PageNode> GetWeakPtr()
      const override {
    return base::WeakPtr<const performance_manager::PageNode>();
  }

 private:
  bool visible_ = true;
  std::string empty_string_;
  GURL empty_url_;
};

template <typename NodeViewPtr>
performance_manager::Graph::NodeSetView<NodeViewPtr> EmptyNodeSetView() {
  static const performance_manager::Graph::NodeSet empty_set;
  return performance_manager::Graph::NodeSetView<NodeViewPtr>(empty_set);
}

class MockGraph : public performance_manager::Graph {
 public:
  void AddFrameNodeObserver(
      performance_manager::FrameNodeObserver* observer) override {}
  void AddPageNodeObserver(
      performance_manager::PageNodeObserver* observer) override {}
  void AddProcessNodeObserver(
      performance_manager::ProcessNodeObserver* observer) override {}
  void AddSystemNodeObserver(
      performance_manager::SystemNodeObserver* observer) override {}
  void AddWorkerNodeObserver(
      performance_manager::WorkerNodeObserver* observer) override {}
  void RemoveFrameNodeObserver(
      performance_manager::FrameNodeObserver* observer) override {}
  void RemovePageNodeObserver(
      performance_manager::PageNodeObserver* observer) override {}
  void RemoveProcessNodeObserver(
      performance_manager::ProcessNodeObserver* observer) override {}
  void RemoveSystemNodeObserver(
      performance_manager::SystemNodeObserver* observer) override {}
  void RemoveWorkerNodeObserver(
      performance_manager::WorkerNodeObserver* observer) override {}
  void PassToGraphImpl(
      std::unique_ptr<performance_manager::GraphOwned> graph_owned) override {
    owned_objects_.push_back(std::move(graph_owned));
  }
  std::unique_ptr<performance_manager::GraphOwned> TakeFromGraph(
      performance_manager::GraphOwned* graph_owned) override {
    return nullptr;
  }
  void RegisterObject(performance_manager::GraphRegistered* object) override {}
  void UnregisterObject(
      performance_manager::GraphRegistered* object) override {}
  performance_manager::GraphRegistered* GetRegisteredObject(
      uintptr_t type_id) override {
    return nullptr;
  }
  const performance_manager::SystemNode* GetSystemNode() const override {
    return nullptr;
  }
  NodeSetView<const performance_manager::ProcessNode*> GetAllProcessNodes()
      const override {
    return EmptyNodeSetView<const performance_manager::ProcessNode*>();
  }
  NodeSetView<const performance_manager::FrameNode*> GetAllFrameNodes()
      const override {
    return EmptyNodeSetView<const performance_manager::FrameNode*>();
  }
  NodeSetView<const performance_manager::PageNode*> GetAllPageNodes()
      const override {
    return EmptyNodeSetView<const performance_manager::PageNode*>();
  }
  NodeSetView<const performance_manager::WorkerNode*> GetAllWorkerNodes()
      const override {
    return EmptyNodeSetView<const performance_manager::WorkerNode*>();
  }
  bool HasOnlySystemNode() const override { return true; }
  ukm::UkmRecorder* GetUkmRecorder() const override { return nullptr; }
  performance_manager::NodeDataDescriberRegistry* GetNodeDataDescriberRegistry()
      const override {
    return nullptr;
  }
  uintptr_t GetImplType() const override { return 0; }
  const void* GetImpl() const override { return nullptr; }

  std::vector<std::unique_ptr<performance_manager::GraphOwned>> owned_objects_;
};

enum class FuzzAction : uint8_t {
  kOnPassedToGraph = 0,
  kOnTakenFromGraph = 1,
  kOnPageNodeAdded = 2,
  kOnBeforePageNodeRemoved = 3,
  kOnIsVisibleChanged = 4,
  kCreatePoliciesAndDecorators = 5,
  kMaxValue = kCreatePoliciesAndDecorators,
};

constexpr size_t kMaxIterations = 100;

void FuzzBackgroundTaskPolicy(FuzzedDataProvider* fdp) {
  MockGraph graph;
  MockPageNode page_node;
  auto policy =
      std::make_unique<performance_manager::policies::BackgroundTaskPolicy>();
  CHECK(policy);

  page_node.SetVisible(fdp->ConsumeBool());

  size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, kMaxIterations);
  for (size_t i = 0; i < iterations; ++i) {
    page_node.SetVisible(fdp->ConsumeBool());

    uint8_t action_raw = fdp->ConsumeIntegralInRange<uint8_t>(
        0, static_cast<uint8_t>(FuzzAction::kMaxValue));
    auto action = static_cast<FuzzAction>(action_raw);

    switch (action) {
      case FuzzAction::kOnPassedToGraph:
        policy->OnPassedToGraph(&graph);
        break;
      case FuzzAction::kOnTakenFromGraph:
        policy->OnTakenFromGraph(&graph);
        break;
      case FuzzAction::kOnPageNodeAdded:
        policy->OnPageNodeAdded(&page_node);
        break;
      case FuzzAction::kOnBeforePageNodeRemoved:
        policy->OnBeforePageNodeRemoved(&page_node);
        break;
      case FuzzAction::kOnIsVisibleChanged:
        policy->OnIsVisibleChanged(&page_node);
        break;
      case FuzzAction::kCreatePoliciesAndDecorators:
        OHOS::NWeb::NwebPerformanceManager::CreatePoliciesAndDecorators(&graph);
        break;
      default:
        break;
    }
  }

  policy->OnTakenFromGraph(&graph);
}

void FuzzNullPointers() {
  auto policy =
      std::make_unique<performance_manager::policies::BackgroundTaskPolicy>();
  CHECK(policy);

  policy->OnPassedToGraph(nullptr);
  policy->OnTakenFromGraph(nullptr);
  policy->OnPageNodeAdded(nullptr);
  policy->OnBeforePageNodeRemoved(nullptr);
  policy->OnIsVisibleChanged(nullptr);
}

void FuzzStateTransitions(FuzzedDataProvider* fdp) {
  MockGraph graph;
  MockPageNode page_node;
  auto policy =
      std::make_unique<performance_manager::policies::BackgroundTaskPolicy>();
  CHECK(policy);

  policy->OnPassedToGraph(&graph);

  constexpr size_t kMaxPages = 10;
  size_t num_pages = fdp->ConsumeIntegralInRange<size_t>(1, kMaxPages);

  for (size_t i = 0; i < num_pages; ++i) {
    page_node.SetVisible(fdp->ConsumeBool());
    policy->OnPageNodeAdded(&page_node);
  }

  size_t changes = fdp->ConsumeIntegralInRange<size_t>(0, kMaxIterations);
  for (size_t i = 0; i < changes; ++i) {
    page_node.SetVisible(fdp->ConsumeBool());
    policy->OnIsVisibleChanged(&page_node);
  }

  for (size_t i = 0; i < num_pages; ++i) {
    policy->OnBeforePageNodeRemoved(&page_node);
  }

  policy->OnTakenFromGraph(&graph);
}

void FuzzRapidStateFlips(FuzzedDataProvider* fdp) {
  MockGraph graph;
  MockPageNode page_node;
  auto policy =
      std::make_unique<performance_manager::policies::BackgroundTaskPolicy>();
  CHECK(policy);

  policy->OnPassedToGraph(&graph);
  policy->OnPageNodeAdded(&page_node);

  size_t flips = fdp->ConsumeIntegralInRange<size_t>(1, kMaxIterations);
  for (size_t i = 0; i < flips; ++i) {
    page_node.SetVisible(fdp->ConsumeBool());
    policy->OnIsVisibleChanged(&page_node);
  }

  policy->OnBeforePageNodeRemoved(&page_node);
  policy->OnTakenFromGraph(&graph);
}

void FuzzCreatePolicies(FuzzedDataProvider* fdp) {
  MockGraph graph;
  size_t calls = fdp->ConsumeIntegralInRange<size_t>(1, 20);
  for (size_t i = 0; i < calls; ++i) {
    OHOS::NWeb::NwebPerformanceManager::CreatePoliciesAndDecorators(&graph);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (data == nullptr || size < sizeof(uint32_t)) {
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  uint8_t scenario = fdp.ConsumeIntegralInRange<uint8_t>(0, 4);
  switch (scenario) {
    case 0:
      FuzzBackgroundTaskPolicy(&fdp);
      break;
    case 1:
      FuzzNullPointers();
      break;
    case 2:
      FuzzStateTransitions(&fdp);
      break;
    case 3:
      FuzzRapidStateFlips(&fdp);
      break;
    case 4:
      FuzzCreatePolicies(&fdp);
      break;
  }

  return 0;
}
