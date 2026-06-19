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

// Pre-include <ranges> before #define private public to prevent
// redeclaration errors in lazy_split_view internals.
#include <ranges>

#include "arkweb/build/features/features.h"

#define private public
#include "components/performance_manager/public/graph/graph.h"
#undef private

#include <cstdint>
#include <cstring>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_set_view.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace {

using performance_manager::FrameNode;
using performance_manager::Graph;
using performance_manager::GraphOwned;
using performance_manager::GraphRegistered;
using performance_manager::Node;
using performance_manager::NodeDataDescriberRegistry;
using performance_manager::NodeTypeEnum;
using performance_manager::PageNode;
using performance_manager::PriorityAndReason;
using performance_manager::ProcessNode;
using performance_manager::RenderFrameHostProxy;
using performance_manager::SystemNode;
using performance_manager::ViewportIntersection;
using performance_manager::WorkerNode;

template <typename T>
T NotReached()
{
    CHECK(false) << "Unexpected call to stub method";
#if defined(__GNUC__)
    __builtin_unreachable();
#endif  // defined(__GNUC__)
}

class MockFrameNode : public FrameNode {
public:
    bool is_current = true;

    static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kFrame; }
    bool IsCurrent() const override { return is_current; }
    const FrameNode *GetParentFrameNode() const override { return nullptr; }
    const FrameNode *GetParentOrOuterDocumentOrEmbedder() const override { return nullptr; }
    const PageNode *GetPageNode() const override { return nullptr; }
    const ProcessNode *GetProcessNode() const override { return nullptr; }
    const blink::LocalFrameToken &GetFrameToken() const override
    {
        static blink::LocalFrameToken token;
        return token;
    }
    content::BrowsingInstanceId GetBrowsingInstanceId() const override
    {
        return content::BrowsingInstanceId();
    }
    content::SiteInstanceGroupId GetSiteInstanceGroupId() const override
    {
        return content::SiteInstanceGroupId();
    }
    resource_attribution::FrameContext GetResourceContext() const override
    {
        return NotReached<resource_attribution::FrameContext>();
    }
    bool IsMainFrame() const override { return true; }
    FrameNode::NodeSetView<const FrameNode *> GetChildFrameNodes() const override
    {
        static const FrameNode::NodeSet empty_set;
        return FrameNode::NodeSetView<const FrameNode *>(empty_set);
    }
    FrameNode::NodeSetView<const PageNode *> GetOpenedPageNodes() const override
    {
        static const FrameNode::NodeSet empty_set;
        return FrameNode::NodeSetView<const PageNode *>(empty_set);
    }
    FrameNode::NodeSetView<const PageNode *> GetEmbeddedPageNodes() const override
    {
        static const FrameNode::NodeSet empty_set;
        return FrameNode::NodeSetView<const PageNode *>(empty_set);
    }
    LifecycleState GetLifecycleState() const override { return LifecycleState::kRunning; }
    bool HasNonemptyBeforeUnload() const override { return false; }
    const GURL &GetURL() const override
    {
        static GURL url;
        return url;
    }
    const std::optional<url::Origin> &GetOrigin() const override
    {
        static std::optional<url::Origin> origin;
        return origin;
    }
    bool IsActive() const override { return false; }
    const PriorityAndReason &GetPriorityAndReason() const override
    {
        static PriorityAndReason pr;
        return pr;
    }
    bool GetNetworkAlmostIdle() const override { return false; }
    bool IsAdFrame() const override { return false; }
    bool IsHoldingWebLock() const override { return false; }
    bool IsHoldingBlockingIndexedDBLock() const override { return false; }
    bool UsesWebRTC() const override { return false; }
    FrameNode::NodeSetView<const WorkerNode *> GetChildWorkerNodes() const override
    {
        static const FrameNode::NodeSet empty_set;
        return FrameNode::NodeSetView<const WorkerNode *>(empty_set);
    }
    bool HadUserActivation() const override { return false; }
    bool HadFormInteraction() const override { return false; }
    bool HadUserEdits() const override { return false; }
    bool IsAudible() const override { return false; }
    bool IsCapturingMediaStream() const override { return false; }
    bool HasFreezingOriginTrialOptOut() const override { return false; }
    ViewportIntersection GetViewportIntersection() const override { return ViewportIntersection::kIntersecting; }
    Visibility GetVisibility() const override { return Visibility::kVisible; }
    bool IsIntersectingLargeArea() const override { return false; }
    bool IsImportant() const override { return false; }
    bool IsRendered() const override { return false; }
    const RenderFrameHostProxy &GetRenderFrameHostProxy() const override
    {
        static RenderFrameHostProxy proxy;
        return proxy;
    }
    base::ByteCount GetResidentSetEstimate() const override { return base::ByteCount(); }
    base::ByteCount GetPrivateFootprintEstimate() const override { return base::ByteCount(); }
    void CrossProcessSubframeRenderProcessGone() override {}
    const void *GetImpl() const override { return nullptr; }
    uintptr_t GetImplType() const override { return 0; }
    Graph *GetGraph() const override { return nullptr; }
};

class MockPageNode : public PageNode {
public:
    PageNode::NodeSet main_frame_nodes_set;

    MockPageNode() = default;

    static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kPage; }
    const std::string &GetBrowserContextID() const override
    {
        static std::string id;
        return id;
    }
    const FrameNode *GetOpenerFrameNode() const override { return nullptr; }
    const FrameNode *GetEmbedderFrameNode() const override { return nullptr; }
    resource_attribution::PageContext GetResourceContext() const override
    {
        return NotReached<resource_attribution::PageContext>();
    }
    performance_manager::PageType GetType() const override
    {
        return performance_manager::PageType::kUnknown;
    }
    bool IsFocused() const override { return false; }
    bool IsVisible() const override { return false; }
    base::TimeTicks GetLastVisibilityChangeTime() const override { return base::TimeTicks(); }
    bool IsAudible() const override { return false; }
    std::optional<base::TimeDelta> GetTimeSinceLastAudibleChange() const override
    {
        return std::nullopt;
    }
    bool HasPictureInPicture() const override { return false; }
    bool HasFreezingOriginTrialOptOut() const override { return false; }
    bool IsOffTheRecord() const override { return false; }
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    bool IsMediaPlaying() const override { return false; }
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    LoadingState GetLoadingState() const override { return LoadingState::kLoadingNotStarted; }
    ukm::SourceId GetUkmSourceID() const override { return ukm::kInvalidSourceId; }
    LifecycleState GetLifecycleState() const override { return LifecycleState::kRunning; }
    bool IsHoldingWebLock() const override { return false; }
    bool IsHoldingBlockingIndexedDBLock() const override { return false; }
    bool UsesWebRTC() const override { return false; }
    int64_t GetNavigationID() const override { return 0; }
    const std::string &GetContentsMimeType() const override
    {
        static std::string mime;
        return mime;
    }
    std::optional<blink::mojom::PermissionStatus> GetNotificationPermissionStatus() const override
    {
        return std::nullopt;
    }
    base::TimeDelta GetTimeSinceLastNavigation() const override { return base::TimeDelta(); }
    const FrameNode *GetMainFrameNode() const override { return nullptr; }
    PageNode::NodeSetView<const FrameNode *> GetMainFrameNodes() const override
    {
        return PageNode::NodeSetView<const FrameNode *>(main_frame_nodes_set);
    }
    const GURL &GetMainFrameUrl() const override
    {
        static GURL url;
        return url;
    }
    base::ByteCount EstimateMainFramePrivateFootprintSize() const override
    {
        return base::ByteCount();
    }
    bool HadFormInteraction() const override { return false; }
    bool HadUserEdits() const override { return false; }
    base::WeakPtr<content::WebContents> GetWebContents() const override
    {
        return base::WeakPtr<content::WebContents>();
    }
    base::ByteCount EstimateResidentSetSize() const override { return base::ByteCount(); }
    base::ByteCount EstimatePrivateFootprintSize() const override { return base::ByteCount(); }
    base::WeakPtr<PageNode> GetWeakPtr() override
    {
        return base::WeakPtr<PageNode>();
    }
    base::WeakPtr<const PageNode> GetWeakPtr() const override
    {
        return base::WeakPtr<const PageNode>();
    }
    const void *GetImpl() const override { return nullptr; }
    uintptr_t GetImplType() const override { return 0; }
    Graph *GetGraph() const override { return nullptr; }
};

template <typename NodeViewPtr>
Graph::NodeSetView<NodeViewPtr> EmptyNodeSetView()
{
    static const Graph::NodeSet empty_set;
    return Graph::NodeSetView<NodeViewPtr>(empty_set);
}

class MockGraph : public Graph {
public:
    Graph::NodeSet page_nodes;

    void AddFrameNodeObserver(performance_manager::FrameNodeObserver *observer) override {}
    void AddPageNodeObserver(performance_manager::PageNodeObserver *observer) override {}
    void AddProcessNodeObserver(performance_manager::ProcessNodeObserver *observer) override {}
    void AddSystemNodeObserver(performance_manager::SystemNodeObserver *observer) override {}
    void AddWorkerNodeObserver(performance_manager::WorkerNodeObserver *observer) override {}
    void RemoveFrameNodeObserver(performance_manager::FrameNodeObserver *observer) override {}
    void RemovePageNodeObserver(performance_manager::PageNodeObserver *observer) override {}
    void RemoveProcessNodeObserver(performance_manager::ProcessNodeObserver *observer) override {}
    void RemoveSystemNodeObserver(performance_manager::SystemNodeObserver *observer) override {}
    void RemoveWorkerNodeObserver(performance_manager::WorkerNodeObserver *observer) override {}
    void PassToGraphImpl(
        std::unique_ptr<GraphOwned> graph_owned) override
    {
        owned_objects_.push_back(std::move(graph_owned));
    }
    std::unique_ptr<GraphOwned> TakeFromGraph(
        GraphOwned *graph_owned) override
    {
        return nullptr;
    }
    void RegisterObject(GraphRegistered *object) override {}
    void UnregisterObject(GraphRegistered *object) override {}
    GraphRegistered *GetRegisteredObject(uintptr_t type_id) override
    {
        return nullptr;
    }
    const SystemNode *GetSystemNode() const override { return nullptr; }
    Graph::NodeSetView<const ProcessNode *> GetAllProcessNodes() const override
    {
        return EmptyNodeSetView<const ProcessNode *>();
    }
    Graph::NodeSetView<const FrameNode *> GetAllFrameNodes() const override
    {
        return EmptyNodeSetView<const FrameNode *>();
    }
    Graph::NodeSetView<const PageNode *> GetAllPageNodes() const override
    {
        return Graph::NodeSetView<const PageNode *>(page_nodes);
    }
    Graph::NodeSetView<const WorkerNode *> GetAllWorkerNodes() const override
    {
        return EmptyNodeSetView<const WorkerNode *>();
    }
    bool HasOnlySystemNode() const override { return page_nodes.empty(); }
    ukm::UkmRecorder *GetUkmRecorder() const override { return nullptr; }
    NodeDataDescriberRegistry *GetNodeDataDescriberRegistry()
        const override
    {
        return nullptr;
    }
    uintptr_t GetImplType() const override { return 0; }
    const void *GetImpl() const override { return nullptr; }

    std::vector<std::unique_ptr<GraphOwned>> owned_objects_;
};

constexpr int MAX_MEMORY_PRESSURE_LEVEL = 2;
constexpr size_t kMaxPages = 5;
constexpr size_t kMaxFrames = 5;
constexpr size_t kMaxIterations = 30;

bool PageMightHaveFramesInBFCache(const PageNode *page_node)
{
    auto main_frame_nodes = page_node->GetMainFrameNodes();
    if (main_frame_nodes.size() == 1) {
        return false;
    }
    for (const FrameNode *main_frame_node : main_frame_nodes) {
        if (!main_frame_node->IsCurrent()) {
            return true;
        }
    }
    return false;
}

// Standalone test class that replicates OHOSBFCachePolicy's OnMemoryPressure
// logic without inheriting from it. This avoids the constructor's
// MemoryPressureListenerRegistration side effect which registers with the
// global MemoryPressureListenerRegistry singleton. In the fuzzer environment
// (no message loop), the async unregistration task never executes, leaving a
// dangling pointer in the global registry and causing use-after-free crashes.
class TestBFCachePolicy : public GraphOwned,
                          public base::MemoryPressureListener {
public:
    int flush_count = 0;
    const PageNode *last_page = nullptr;
    base::MemoryPressureLevel last_level = base::MEMORY_PRESSURE_LEVEL_NONE;

    void OnPassedToGraph(Graph *graph) override {}
    void OnTakenFromGraph(Graph *graph) override {}

    void OnMemoryPressure(base::MemoryPressureLevel new_level) override
    {
        if (new_level == base::MEMORY_PRESSURE_LEVEL_NONE) {
            return;
        }

        auto *owningGraph = GetOwningGraph();
        if (!owningGraph) {
            return;
        }

        for (const PageNode *page_node : GetOwningGraph()->GetAllPageNodes()) {
            if (PageMightHaveFramesInBFCache(page_node)) {
                MaybeFlushBFCache(page_node, new_level);
            }
        }
    }

protected:
    virtual void MaybeFlushBFCache(const PageNode *page_node,
                                   base::MemoryPressureLevel level)
    {
        flush_count++;
        last_page = page_node;
        last_level = level;
    }
};

void FuzzNullPointers()
{
    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(nullptr);
    policy->OnTakenFromGraph(nullptr);
    policy->OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_NONE);
    policy->OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);
    policy->OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

void FuzzNoGraph(FuzzedDataProvider *fdp)
{
    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    int pressure_level = fdp->ConsumeIntegralInRange<int>(0, MAX_MEMORY_PRESSURE_LEVEL);
    auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
    policy->OnMemoryPressure(level);
}

void FuzzEmptyGraph(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    int pressure_level = fdp->ConsumeIntegralInRange<int>(0, MAX_MEMORY_PRESSURE_LEVEL);
    auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
    policy->OnMemoryPressure(level);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);
}

void FuzzSinglePageNoFrames(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    MockPageNode page;
    graph.page_nodes.insert(&page);

    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    int pressure_level = fdp->ConsumeIntegralInRange<int>(1, MAX_MEMORY_PRESSURE_LEVEL);
    auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
    policy->OnMemoryPressure(level);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);
}

void FuzzSingleFrameCurrent(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    MockPageNode page;
    MockFrameNode frame;
    frame.is_current = true;
    page.main_frame_nodes_set.insert(&frame);
    graph.page_nodes.insert(&page);

    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    int pressure_level = fdp->ConsumeIntegralInRange<int>(1, MAX_MEMORY_PRESSURE_LEVEL);
    auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
    policy->OnMemoryPressure(level);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);
}

void FuzzSingleFrameNonCurrent(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    MockPageNode page;
    MockFrameNode frame;
    frame.is_current = false;
    page.main_frame_nodes_set.insert(&frame);
    graph.page_nodes.insert(&page);

    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    int pressure_level = fdp->ConsumeIntegralInRange<int>(1, MAX_MEMORY_PRESSURE_LEVEL);
    auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
    policy->OnMemoryPressure(level);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);
}

void FuzzMultipleFramesMixed(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    MockPageNode page;
    size_t num_frames = fdp->ConsumeIntegralInRange<size_t>(2, kMaxFrames);

    for (size_t i = 0; i < num_frames; ++i) {
        auto *frame = new MockFrameNode();
        frame->is_current = fdp->ConsumeBool();
        page.main_frame_nodes_set.insert(frame);
    }
    graph.page_nodes.insert(&page);

    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    int pressure_level = fdp->ConsumeIntegralInRange<int>(1, MAX_MEMORY_PRESSURE_LEVEL);
    auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
    policy->OnMemoryPressure(level);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);

    for (auto *node : page.main_frame_nodes_set) {
        delete static_cast<const MockFrameNode *>(node);
    }
}

void FuzzMultiplePages(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    size_t num_pages = fdp->ConsumeIntegralInRange<size_t>(1, kMaxPages);

    std::vector<MockPageNode *> pages;
    std::vector<std::vector<MockFrameNode *>> all_frames;

    for (size_t p = 0; p < num_pages; ++p) {
        auto *page = new MockPageNode();
        pages.push_back(page);

        size_t num_frames = fdp->ConsumeIntegralInRange<size_t>(0, kMaxFrames);
        std::vector<MockFrameNode *> page_frames;

        for (size_t f = 0; f < num_frames; ++f) {
            auto *frame = new MockFrameNode();
            frame->is_current = fdp->ConsumeBool();
            page->main_frame_nodes_set.insert(frame);
            page_frames.push_back(frame);
        }
        all_frames.push_back(page_frames);
        graph.page_nodes.insert(page);
    }

    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    int pressure_level = fdp->ConsumeIntegralInRange<int>(1, MAX_MEMORY_PRESSURE_LEVEL);
    auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
    policy->OnMemoryPressure(level);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);

    for (size_t p = 0; p < num_pages; ++p) {
        for (auto *frame : all_frames[p]) {
            delete frame;
        }
        delete pages[p];
    }
}

void FuzzFullLifecycle(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    std::vector<MockPageNode *> pages;
    std::vector<std::vector<MockFrameNode *>> all_frames;

    size_t num_pages = fdp->ConsumeIntegralInRange<size_t>(0, kMaxPages);
    for (size_t p = 0; p < num_pages; ++p) {
        auto *page = new MockPageNode();
        pages.push_back(page);

        size_t num_frames = fdp->ConsumeIntegralInRange<size_t>(0, kMaxFrames);
        std::vector<MockFrameNode *> page_frames;
        for (size_t f = 0; f < num_frames; ++f) {
            auto *frame = new MockFrameNode();
            frame->is_current = fdp->ConsumeBool();
            page->main_frame_nodes_set.insert(frame);
            page_frames.push_back(frame);
        }
        all_frames.push_back(page_frames);
        graph.page_nodes.insert(page);
    }

    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, kMaxIterations);
    for (size_t i = 0; i < iterations; ++i) {
        int pressure_level = fdp->ConsumeIntegralInRange<int>(0, MAX_MEMORY_PRESSURE_LEVEL);
        auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
        policy->OnMemoryPressure(level);
    }

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);

    for (size_t p = 0; p < num_pages; ++p) {
        for (auto *frame : all_frames[p]) {
            delete frame;
        }
        delete pages[p];
    }
}

void FuzzRepeatedPressureChanges(FuzzedDataProvider *fdp)
{
    MockGraph graph;
    MockPageNode page;
    MockFrameNode frame1;
    MockFrameNode frame2;
    frame1.is_current = true;
    frame2.is_current = false;
    page.main_frame_nodes_set.insert(&frame1);
    page.main_frame_nodes_set.insert(&frame2);
    graph.page_nodes.insert(&page);

    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(&graph);
    policy->graph_ = &graph;

    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, kMaxIterations);
    for (size_t i = 0; i < iterations; ++i) {
        int pressure_level = fdp->ConsumeIntegralInRange<int>(0, MAX_MEMORY_PRESSURE_LEVEL);
        auto level = static_cast<base::MemoryPressureLevel>(pressure_level);
        policy->OnMemoryPressure(level);

        if (fdp->ConsumeBool()) {
            frame2.is_current = fdp->ConsumeBool();
        }
    }

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph);
}

void FuzzGraphOwnershipLifecycle(FuzzedDataProvider *fdp)
{
    auto policy = std::make_unique<TestBFCachePolicy>();
    CHECK(policy);

    policy->OnPassedToGraph(nullptr);

    MockGraph graph1;
    policy->graph_ = &graph1;
    policy->OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_MODERATE);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(nullptr);

    MockGraph graph2;
    MockPageNode page;
    MockFrameNode frame;
    frame.is_current = false;
    page.main_frame_nodes_set.insert(&frame);
    graph2.page_nodes.insert(&page);

    policy->graph_ = &graph2;
    policy->OnPassedToGraph(&graph2);
    policy->OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL);

    policy->graph_ = nullptr;
    policy->OnTakenFromGraph(&graph2);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == nullptr || size < sizeof(uint32_t)) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    uint8_t scenario = fdp.ConsumeIntegralInRange<uint8_t>(0, 9);
    switch (scenario) {
    case 0:
        FuzzNullPointers();
        break;
    case 1:
        FuzzNoGraph(&fdp);
        break;
    case 2:
        FuzzEmptyGraph(&fdp);
        break;
    case 3:
        FuzzSinglePageNoFrames(&fdp);
        break;
    case 4:
        FuzzSingleFrameCurrent(&fdp);
        break;
    case 5:
        FuzzSingleFrameNonCurrent(&fdp);
        break;
    case 6:
        FuzzMultipleFramesMixed(&fdp);
        break;
    case 7:
        FuzzMultiplePages(&fdp);
        break;
    case 8:
        FuzzFullLifecycle(&fdp);
        break;
    case 9:
        FuzzRepeatedPressureChanges(&fdp);
        break;
    default:
        break;
    }

    return 0;
}
