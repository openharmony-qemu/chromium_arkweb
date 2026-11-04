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

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

#include "arkweb/build/features/features.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/node_set_view.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

#define private public
#include "background_task_policy.h"
#undef private

namespace {

template <typename T>
T NotReached()
{
    CHECK(false) << "Unexpected call to stub method";
#if defined(__GNUC__)
    __builtin_unreachable();
#endif  // defined(__GNUC__)
}

class MockPageNode : public performance_manager::PageNode {
public:
    MockPageNode() = default;

    void SetVisible(bool v) { visible_ = v; }
    void SetAudible(bool v) { audible_ = v; }
    void SetMediaPlaying(bool v) { media_playing_ = v; }

    bool IsVisible() const override { return visible_; }
    bool IsAudible() const override { return audible_; }
    bool IsMediaPlaying() const override { return media_playing_; }

    performance_manager::Graph *GetGraph() const override { return nullptr; }
    uintptr_t GetImplType() const override { return 0; }
    const void *GetImpl() const override { return nullptr; }
    const std::string &GetBrowserContextID() const override { return empty_string_; }
    const performance_manager::FrameNode *GetOpenerFrameNode() const override
    {
        return nullptr;
    }
    const performance_manager::FrameNode *GetEmbedderFrameNode() const override
    {
        return nullptr;
    }
    resource_attribution::PageContext GetResourceContext() const override
    {
        return NotReached<resource_attribution::PageContext>();
    }
    performance_manager::PageType GetType() const override
    {
        return performance_manager::PageType::kTab;
    }
    bool IsFocused() const override { return false; }
    base::TimeTicks GetLastVisibilityChangeTime() const override
    {
        return base::TimeTicks();
    }
    std::optional<base::TimeDelta> GetTimeSinceLastAudibleChange() const override
    {
        return std::nullopt;
    }
    bool HasPictureInPicture() const override { return false; }
    bool HasFreezingOriginTrialOptOut() const override { return false; }
    bool IsOffTheRecord() const override { return false; }
    LoadingState GetLoadingState() const override
    {
        return LoadingState::kLoadingNotStarted;
    }
    ukm::SourceId GetUkmSourceID() const override { return 0; }
    LifecycleState GetLifecycleState() const override
    {
        return LifecycleState::kRunning;
    }
    bool IsHoldingWebLock() const override { return false; }
    bool IsHoldingBlockingIndexedDBLock() const override { return false; }
    bool UsesWebRTC() const override { return false; }
    int64_t GetNavigationID() const override { return 0; }
    const std::string &GetContentsMimeType() const override { return empty_string_; }
    std::optional<blink::mojom::PermissionStatus> GetNotificationPermissionStatus()
        const override
    {
        return std::nullopt;
    }
    base::TimeDelta GetTimeSinceLastNavigation() const override
    {
        return base::TimeDelta();
    }
    const performance_manager::FrameNode *GetMainFrameNode() const override
    {
        return nullptr;
    }
    NodeSetView<const performance_manager::FrameNode *> GetMainFrameNodes() const override
    {
        static const performance_manager::PageNode::NodeSet empty_set;
        return NodeSetView<const performance_manager::FrameNode *>(empty_set);
    }
    const GURL &GetMainFrameUrl() const override { return empty_url_; }
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
    base::ByteCount EstimatePrivateFootprintSize() const override
    {
        return base::ByteCount();
    }
    base::WeakPtr<performance_manager::PageNode> GetWeakPtr() override
    {
        return base::WeakPtr<performance_manager::PageNode>();
    }
    base::WeakPtr<const performance_manager::PageNode> GetWeakPtr() const override
    {
        return base::WeakPtr<const performance_manager::PageNode>();
    }

private:
    bool visible_ = true;
    bool audible_ = false;
    bool media_playing_ = false;
    std::string empty_string_;
    GURL empty_url_;
};

template <typename NodeViewPtr>
performance_manager::Graph::NodeSetView<NodeViewPtr> EmptyNodeSetView()
{
    static const performance_manager::Graph::NodeSet empty_set;
    return performance_manager::Graph::NodeSetView<NodeViewPtr>(empty_set);
}

class MockGraph : public performance_manager::Graph {
public:
    void AddFrameNodeObserver(performance_manager::FrameNodeObserver *observer) override {}
    void AddPageNodeObserver(performance_manager::PageNodeObserver *observer) override {}
    void AddProcessNodeObserver(performance_manager::ProcessNodeObserver *observer) override
    {
    }
    void AddSystemNodeObserver(performance_manager::SystemNodeObserver *observer) override {}
    void AddWorkerNodeObserver(performance_manager::WorkerNodeObserver *observer) override {}
    void RemoveFrameNodeObserver(performance_manager::FrameNodeObserver *observer) override
    {
    }
    void RemovePageNodeObserver(performance_manager::PageNodeObserver *observer) override
    {
    }
    void RemoveProcessNodeObserver(
        performance_manager::ProcessNodeObserver *observer) override
    {
    }
    void RemoveSystemNodeObserver(
        performance_manager::SystemNodeObserver *observer) override
    {
    }
    void RemoveWorkerNodeObserver(
        performance_manager::WorkerNodeObserver *observer) override
    {
    }
    void PassToGraphImpl(
        std::unique_ptr<performance_manager::GraphOwned> graph_owned) override
    {
        owned_objects_.push_back(std::move(graph_owned));
    }
    std::unique_ptr<performance_manager::GraphOwned> TakeFromGraph(
        performance_manager::GraphOwned *graph_owned) override
    {
        return nullptr;
    }
    void RegisterObject(performance_manager::GraphRegistered *object) override {}
    void UnregisterObject(performance_manager::GraphRegistered *object) override {}
    performance_manager::GraphRegistered *GetRegisteredObject(uintptr_t type_id) override
    {
        return nullptr;
    }
    const performance_manager::SystemNode *GetSystemNode() const override
    {
        return nullptr;
    }
    NodeSetView<const performance_manager::ProcessNode *> GetAllProcessNodes() const override
    {
        return EmptyNodeSetView<const performance_manager::ProcessNode *>();
    }
    NodeSetView<const performance_manager::FrameNode *> GetAllFrameNodes() const override
    {
        return EmptyNodeSetView<const performance_manager::FrameNode *>();
    }
    NodeSetView<const performance_manager::PageNode *> GetAllPageNodes() const override
    {
        return EmptyNodeSetView<const performance_manager::PageNode *>();
    }
    NodeSetView<const performance_manager::WorkerNode *> GetAllWorkerNodes() const override
    {
        return EmptyNodeSetView<const performance_manager::WorkerNode *>();
    }
    bool HasOnlySystemNode() const override { return true; }
    ukm::UkmRecorder *GetUkmRecorder() const override { return nullptr; }
    performance_manager::NodeDataDescriberRegistry *GetNodeDataDescriberRegistry()
        const override
    {
        return nullptr;
    }
    uintptr_t GetImplType() const override { return 0; }
    const void *GetImpl() const override { return nullptr; }

    std::vector<std::unique_ptr<performance_manager::GraphOwned>> owned_objects_;
};

enum class FuzzAction : uint8_t {
    kOnPassedToGraph = 0,
    kOnTakenFromGraph = 1,
    kOnPageNodeAdded = 2,
    kOnBeforePageNodeRemoved = 3,
    kOnIsVisibleChanged = 4,
    kOnIsMediaPlayingChanged = 5,
    kOnIsAudibleChanged = 6,
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    kOnDecrementAudioNum = 7,
    kAudioContextStarted = 8,
    kAudioContextStopped = 9,
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
#if BUILDFLAG(ARKWEB_BGTASK)
    kSetBrowserForeground = 12,
#endif  // BUILDFLAG(ARKWEB_BGTASK)
    kMaxValue =
#if BUILDFLAG(ARKWEB_BGTASK)
        kSetBrowserForeground
#elif BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        kAudioContextStopped
#else
        kOnIsAudibleChanged
#endif
};

constexpr int MAX_AUDIO_CONTEXT_ID = 1000;
constexpr int MAX_RENDER_PROCESS_ID = 100;
constexpr int MAX_ROUTING_ID = 10000;
constexpr size_t kMaxIterations = 50;

using BackgroundTaskPolicy = performance_manager::policies::BackgroundTaskPolicy;

void ResetPolicyState(BackgroundTaskPolicy *policy)
{
    policy->is_request_background_task_ = false;
    policy->visible_page_num_ = 0;
    policy->media_playing_num_ = 0;
    policy->audio_state_num_ = 0;
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    policy->audio_context_players_num_.clear();
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
}

struct FuzzerState {
    MockGraph graph;
    MockPageNode page_node;
    std::unique_ptr<BackgroundTaskPolicy> policy;

    FuzzerState()
    {
        policy = std::make_unique<BackgroundTaskPolicy>();
        CHECK(policy);
    }
};

void FuzzNullPointers(FuzzerState *state)
{
    state->policy->OnPassedToGraph(nullptr);
    state->policy->OnTakenFromGraph(nullptr);
    state->policy->OnPageNodeAdded(nullptr);
    state->policy->OnBeforePageNodeRemoved(nullptr);
    state->policy->OnIsVisibleChanged(nullptr);
    state->policy->OnIsMediaPlayingChanged(nullptr);
    state->policy->OnIsAudibleChanged(nullptr);
    ResetPolicyState(state->policy.get());
}

void FuzzFullLifecycle(FuzzerState *state, FuzzedDataProvider *fdp)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);
    state->policy->OnPageNodeAdded(&state->page_node);

    size_t iterations = fdp->ConsumeIntegralInRange<size_t>(1, kMaxIterations);
    for (size_t i = 0; i < iterations; ++i) {
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        state->page_node.SetVisible(fdp->ConsumeBool());
        state->page_node.SetAudible(true);
#else
        state->page_node.SetVisible(fdp->ConsumeBool());
        state->page_node.SetAudible(fdp->ConsumeBool());
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        state->page_node.SetMediaPlaying(fdp->ConsumeBool());

        uint8_t action_raw = fdp->ConsumeIntegralInRange<uint8_t>(
            0, static_cast<uint8_t>(FuzzAction::kMaxValue));
        auto action = static_cast<FuzzAction>(action_raw);

        switch (action) {
        case FuzzAction::kOnPassedToGraph:
            state->policy->OnPassedToGraph(&state->graph);
            break;
        case FuzzAction::kOnTakenFromGraph:
            state->policy->OnTakenFromGraph(&state->graph);
            break;
        case FuzzAction::kOnPageNodeAdded:
            state->policy->OnPageNodeAdded(&state->page_node);
            break;
        case FuzzAction::kOnBeforePageNodeRemoved:
            state->policy->OnBeforePageNodeRemoved(&state->page_node);
            break;
        case FuzzAction::kOnIsVisibleChanged:
            state->policy->OnIsVisibleChanged(&state->page_node);
            break;
        case FuzzAction::kOnIsMediaPlayingChanged:
            state->policy->OnIsMediaPlayingChanged(&state->page_node);
            break;
        case FuzzAction::kOnIsAudibleChanged:
            state->policy->OnIsAudibleChanged(&state->page_node);
            break;
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        case FuzzAction::kOnDecrementAudioNum:
            state->policy->OnDecrementAudioNum(&state->page_node);
            break;
        case FuzzAction::kAudioContextStarted: {
            int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
            int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
            int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
            content::GlobalRenderFrameHostId rfh_id(rpid, rid);
            state->policy->OnAudioContextPlaybackStarted(rfh_id, acid);
            break;
        }
        case FuzzAction::kAudioContextStopped: {
            int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
            int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
            int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
            content::GlobalRenderFrameHostId rfh_id(rpid, rid);
            state->policy->OnAudioContextPlaybackStopped(rfh_id, acid);
            break;
        }
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
#if BUILDFLAG(ARKWEB_BGTASK)
        case FuzzAction::kSetBrowserForeground:
            state->policy->SetBrowserForeground(&state->page_node);
            break;
#endif  // BUILDFLAG(ARKWEB_BGTASK)
        default:
            break;
        }
    }

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}

void FuzzStateTransitions(FuzzerState *state, FuzzedDataProvider *fdp)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    constexpr size_t kMaxPages = 5;
    size_t num_pages = fdp->ConsumeIntegralInRange<size_t>(1, kMaxPages);
    for (size_t i = 0; i < num_pages; ++i) {
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        state->page_node.SetVisible(fdp->ConsumeBool());
        state->page_node.SetAudible(true);
#else
        state->page_node.SetVisible(fdp->ConsumeBool());
        state->page_node.SetAudible(fdp->ConsumeBool());
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        state->page_node.SetMediaPlaying(fdp->ConsumeBool());
        state->policy->OnPageNodeAdded(&state->page_node);
    }

    size_t changes = fdp->ConsumeIntegralInRange<size_t>(0, kMaxIterations);
    for (size_t i = 0; i < changes; ++i) {
        state->page_node.SetVisible(fdp->ConsumeBool());
        state->policy->OnIsVisibleChanged(&state->page_node);
    }

    for (size_t i = 0; i < num_pages; ++i) {
        state->policy->OnBeforePageNodeRemoved(&state->page_node);
    }

    state->policy->OnTakenFromGraph(&state->graph);
}

void FuzzRapidStateFlips(FuzzerState *state, FuzzedDataProvider *fdp)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);
    state->policy->OnPageNodeAdded(&state->page_node);

    size_t flips = fdp->ConsumeIntegralInRange<size_t>(1, kMaxIterations);
    for (size_t i = 0; i < flips; ++i) {
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        state->page_node.SetVisible(fdp->ConsumeBool());
        state->page_node.SetAudible(true);
#else
        state->page_node.SetVisible(fdp->ConsumeBool());
        state->page_node.SetAudible(fdp->ConsumeBool());
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        state->page_node.SetMediaPlaying(fdp->ConsumeBool());

        uint8_t change_type = fdp->ConsumeIntegralInRange<uint8_t>(0, 2);
        switch (change_type) {
        case 0:
            state->policy->OnIsVisibleChanged(&state->page_node);
            break;
        case 1:
            state->policy->OnIsMediaPlayingChanged(&state->page_node);
            break;
        case 2:
            state->policy->OnIsAudibleChanged(&state->page_node);
            break;
        default:
            break;
        }
    }

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
void FuzzAudioContextLifecycle(FuzzerState *state, FuzzedDataProvider *fdp)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);
    state->page_node.SetVisible(false);
    state->page_node.SetAudible(true);
    state->policy->OnPageNodeAdded(&state->page_node);
    state->policy->OnIsAudibleChanged(&state->page_node);

    int32_t ctx_count = fdp->ConsumeIntegralInRange<int32_t>(1, 10);
    for (int32_t i = 0; i < ctx_count; i++) {
        int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
        int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
        int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
        content::GlobalRenderFrameHostId rfh_id(rpid, rid);
        state->policy->OnAudioContextPlaybackStarted(rfh_id, acid);
    }

    int32_t stop_count = fdp->ConsumeIntegralInRange<int32_t>(0, ctx_count);
    for (int32_t i = 0; i < stop_count; i++) {
        int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
        int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
        int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
        content::GlobalRenderFrameHostId rfh_id(rpid, rid);
        state->policy->OnAudioContextPlaybackStopped(rfh_id, acid);
    }

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

void FuzzMaybeChangeBackgroundTaskBranches(FuzzerState *state, FuzzedDataProvider *fdp)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    state->page_node.SetVisible(true);
    state->page_node.SetAudible(true);
    state->page_node.SetMediaPlaying(true);
    state->policy->OnPageNodeAdded(&state->page_node);
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    state->policy->OnIsAudibleChanged(&state->page_node);
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    state->policy->OnIsMediaPlayingChanged(&state->page_node);

    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);

    state->page_node.SetVisible(true);
    state->policy->OnIsVisibleChanged(&state->page_node);

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);

    int32_t ctx_count = fdp->ConsumeIntegralInRange<int32_t>(1, 5);
    for (int32_t i = 0; i < ctx_count; i++) {
        int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
        int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
        int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
        content::GlobalRenderFrameHostId rfh_id(rpid, rid);
        state->policy->OnAudioContextPlaybackStarted(rfh_id, acid);
    }
    state->policy->OnIsVisibleChanged(&state->page_node);

    state->policy->OnDecrementAudioNum(&state->page_node);

    for (int32_t i = 0; i < ctx_count; i++) {
        int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
        int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
        int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
        content::GlobalRenderFrameHostId rfh_id(rpid, rid);
        state->policy->OnAudioContextPlaybackStopped(rfh_id, acid);
    }
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
void FuzzAudioContextWithVisibility(FuzzerState *state, FuzzedDataProvider *fdp)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    state->page_node.SetVisible(true);
    state->page_node.SetAudible(true);
    state->page_node.SetMediaPlaying(true);
    state->policy->OnPageNodeAdded(&state->page_node);
    state->policy->OnIsAudibleChanged(&state->page_node);

    int32_t ctx_count = fdp->ConsumeIntegralInRange<int32_t>(2, 8);
    for (int32_t i = 0; i < ctx_count; i++) {
        int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
        int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
        int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
        content::GlobalRenderFrameHostId rfh_id(rpid, rid);
        state->policy->OnAudioContextPlaybackStarted(rfh_id, acid);
    }

    size_t transitions = fdp->ConsumeIntegralInRange<size_t>(1, kMaxIterations);
    for (size_t i = 0; i < transitions; ++i) {
        state->page_node.SetVisible(fdp->ConsumeBool());
        uint8_t action = fdp->ConsumeIntegralInRange<uint8_t>(0, 3);
        switch (action) {
        case 0:
            state->policy->OnIsVisibleChanged(&state->page_node);
            break;
        case 1:
            state->policy->OnIsMediaPlayingChanged(&state->page_node);
            break;
        case 2:
            state->policy->OnDecrementAudioNum(&state->page_node);
            break;
        case 3: {
            int rpid = fdp->ConsumeIntegralInRange<int>(0, MAX_RENDER_PROCESS_ID);
            int rid = fdp->ConsumeIntegralInRange<int>(0, MAX_ROUTING_ID);
            int acid = fdp->ConsumeIntegralInRange<int>(0, MAX_AUDIO_CONTEXT_ID);
            content::GlobalRenderFrameHostId rfh_id(rpid, rid);
            state->policy->OnAudioContextPlaybackStopped(rfh_id, acid);
            break;
        }
        default:
            break;
        }
    }

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
void FuzzOnDecrementAudioNumAllPaths(FuzzerState *state)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    state->page_node.SetVisible(true);
    state->page_node.SetMediaPlaying(true);
    state->page_node.SetAudible(true);
    state->policy->OnPageNodeAdded(&state->page_node);

    // Path 1: decrement from zero -> negative clamp for both media and audio
    state->policy->OnDecrementAudioNum(&state->page_node);

    // add entries then decrement to trigger clear
    content::GlobalRenderFrameHostId id1(1, 100);
    state->policy->OnAudioContextPlaybackStarted(id1, 1);
    content::GlobalRenderFrameHostId id2(2, 200);
    state->policy->OnAudioContextPlaybackStarted(id2, 2);
    state->policy->audio_state_num_ = 2;
    state->policy->media_playing_num_ = 1;

    state->policy->OnDecrementAudioNum(&state->page_node);

    state->policy->OnDecrementAudioNum(&state->page_node);
    state->policy->OnDecrementAudioNum(&state->page_node);

    // Note: OnDecrementAudioNum does base::FastHash(page_node) before nullptr
    // check, so passing nullptr would crash. Use valid page_node instead.
    state->policy->OnDecrementAudioNum(&state->page_node);

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

void FuzzDeterministicBgTaskCycle(FuzzerState *state)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    // Setup: visible, audible, media playing
    state->page_node.SetVisible(true);
    state->page_node.SetAudible(true);
    state->page_node.SetMediaPlaying(true);
    state->policy->OnPageNodeAdded(&state->page_node);
    state->policy->OnIsAudibleChanged(&state->page_node);
    state->policy->OnIsMediaPlayingChanged(&state->page_node);
    // State: visible=1, audio=1, media=1, is_request=false

    // Step 1: Make invisible -> NEED_BG_TASK
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);
    // visible=0, media=1, audio=1, is_request=false -> NEED_BG_TASK

    // Step 2: Make visible -> NO_NEED_BG_TASK
    state->page_node.SetVisible(true);
    state->policy->OnIsVisibleChanged(&state->page_node);
    // visible=1, is_request=true -> NO_NEED_BG_TASK

    // Step 3: Make invisible again -> NEED_BG_TASK
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);
    // visible=0, is_request=false -> NEED_BG_TASK

    // Step 4: Stop media -> NO_CHANGE (audio still active)
    state->page_node.SetMediaPlaying(false);
    state->policy->OnIsMediaPlayingChanged(&state->page_node);
    // media=0, is_request=true, audio=1 -> NO_CHANGE

    // Step 5: Stop audio -> NO_NEED_BG_TASK (all media stopped)
    state->page_node.SetAudible(false);
    state->policy->OnIsAudibleChanged(&state->page_node);
    // audio=0, media=0, is_request=true -> NO_NEED_BG_TASK

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
void FuzzDecrementFromZero(FuzzerState *state)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    state->page_node.SetMediaPlaying(true);
    state->page_node.SetAudible(true);
    state->page_node.SetVisible(true);
    state->policy->OnPageNodeAdded(&state->page_node);

    // From zero: triggers clamp-to-zero (lines 150-157)
    state->policy->OnDecrementAudioNum(&state->page_node);
    // media_playing_num_ = 0 - 1 = -1 -> 0 (clamp)
    // audio_state_num_ = 0 - 1 = -1 -> 0 (clamp)
    // audio_state_num_ == 0 -> clear audio_context_players (line 158-160)

    // Setup with audio context entries, then decrement to trigger clear
    content::GlobalRenderFrameHostId id1(1, 100);
    state->policy->OnAudioContextPlaybackStarted(id1, 1);
    content::GlobalRenderFrameHostId id2(2, 200);
    state->policy->OnAudioContextPlaybackStarted(id2, 2);
    state->policy->audio_state_num_ = 1;

    // Decrement: audio goes 1 -> 0 -> clear
    state->policy->OnDecrementAudioNum(&state->page_node);

    // Decrement again from zero -> clamp
    state->policy->OnDecrementAudioNum(&state->page_node);

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}
#endif

void FuzzNegativeCountClamp(FuzzerState *state)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    state->page_node.SetVisible(false);
    state->page_node.SetMediaPlaying(false);
    state->page_node.SetAudible(false);

    state->policy->OnPageNodeAdded(&state->page_node);
    // visible_page_num_ = 1

    // Trigger negative clamp for visible: 1 + (-1) = 0
    state->policy->OnIsVisibleChanged(&state->page_node);

    // Trigger negative clamp for media: 0 + (-1) = -1 -> 0
    state->policy->OnIsMediaPlayingChanged(&state->page_node);

    // Trigger negative clamp for audio: 0 + (-1) = -1 -> 0
    state->policy->OnIsAudibleChanged(&state->page_node);

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}

void FuzzAllBranchesExhaustive(FuzzerState *state)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    // NO_CHANGE: visible, no media
    state->page_node.SetVisible(true);
    state->page_node.SetAudible(false);
    state->page_node.SetMediaPlaying(false);
    state->policy->OnPageNodeAdded(&state->page_node);

    // -> NEED_BG_TASK: invisible + has media
    state->page_node.SetAudible(true);
    state->policy->OnIsAudibleChanged(&state->page_node);
    state->page_node.SetMediaPlaying(true);
    state->policy->OnIsMediaPlayingChanged(&state->page_node);
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);

    // -> NO_NEED_BG_TASK: now visible
    state->page_node.SetVisible(true);
    state->policy->OnIsVisibleChanged(&state->page_node);

    // -> NEED_BG_TASK again: invisible
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);

    // -> NO_CHANGE: stop media, audio still active
    state->page_node.SetMediaPlaying(false);
    state->policy->OnIsMediaPlayingChanged(&state->page_node);

    // -> NO_NEED_BG_TASK: stop audio, all media stopped
    state->page_node.SetAudible(false);
    state->policy->OnIsAudibleChanged(&state->page_node);

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}

void FuzzNegativeClampAll(FuzzerState *state)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    state->page_node.SetVisible(false);
    state->page_node.SetMediaPlaying(false);
    state->page_node.SetAudible(false);

    state->policy->OnPageNodeAdded(&state->page_node);

    state->policy->OnIsVisibleChanged(&state->page_node);
    state->policy->OnIsVisibleChanged(&state->page_node);
    state->policy->OnIsVisibleChanged(&state->page_node);

    state->policy->OnIsMediaPlayingChanged(&state->page_node);
    state->policy->OnIsMediaPlayingChanged(&state->page_node);

    state->policy->OnIsAudibleChanged(&state->page_node);
    state->policy->OnIsAudibleChanged(&state->page_node);

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
void FuzzAudibleChangedAllPaths(FuzzerState *state)
{
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    state->page_node.SetVisible(true);
    state->page_node.SetMediaPlaying(false);
    state->page_node.SetAudible(true);
    state->policy->OnPageNodeAdded(&state->page_node);
    state->policy->OnIsAudibleChanged(&state->page_node);

    state->page_node.SetAudible(false);
    state->policy->OnIsAudibleChanged(&state->page_node);

    // negative clamp
    state->policy->OnIsAudibleChanged(&state->page_node);

    state->page_node.SetAudible(true);
    state->policy->OnIsAudibleChanged(&state->page_node);

    // clear with entries present
    content::GlobalRenderFrameHostId id1(1, 100);
    state->policy->OnAudioContextPlaybackStarted(id1, 1);
    content::GlobalRenderFrameHostId id2(2, 200);
    state->policy->OnAudioContextPlaybackStarted(id2, 2);
    state->page_node.SetAudible(false);
    state->policy->OnIsAudibleChanged(&state->page_node);

    state->policy->OnBeforePageNodeRemoved(&state->page_node);
    state->policy->OnTakenFromGraph(&state->graph);
}
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
void FuzzMegaAllPaths(FuzzerState *state)
{
    // Phase 0: Trigger destructor with populated audio_context_players
    // Covers: ~BackgroundTaskPolicy() → audio_context_players_num_.clear()
    {
        content::GlobalRenderFrameHostId id_ctor(99, 999);
        state->policy->OnAudioContextPlaybackStarted(id_ctor, 999);
        state->policy = std::make_unique<BackgroundTaskPolicy>();
    }

    // Phase 1: Initialize
    ResetPolicyState(state->policy.get());
    state->policy->OnPassedToGraph(&state->graph);

    // Phase 2: Add page visible, no media → NO_CHANGE
    state->page_node.SetVisible(true);
    state->page_node.SetAudible(false);
    state->page_node.SetMediaPlaying(false);
    state->policy->OnPageNodeAdded(&state->page_node);

    // Phase 3: Make audible → audio=1, NO_CHANGE
    state->page_node.SetAudible(true);
    state->policy->OnIsAudibleChanged(&state->page_node);

    // Phase 4: Make media playing → media=1, NO_CHANGE
    state->page_node.SetMediaPlaying(true);
    state->policy->OnIsMediaPlayingChanged(&state->page_node);

    // Phase 5: Go invisible → NEED_BG_TASK
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);
    // IsWebAudioRequestBackgroundRunning called → returns true

    // Phase 6: Go visible → NO_NEED_BG_TASK
    state->page_node.SetVisible(true);
    state->policy->OnIsVisibleChanged(&state->page_node);

    // Phase 7: Go invisible again → NEED_BG_TASK
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);

    // Phase 8: Add audio context entries
    content::GlobalRenderFrameHostId id1(1, 100);
    state->policy->OnAudioContextPlaybackStarted(id1, 1);
    content::GlobalRenderFrameHostId id2(2, 200);
    state->policy->OnAudioContextPlaybackStarted(id2, 2);

    // Phase 9: Make visible → NO_NEED_BG_TASK
    state->page_node.SetVisible(true);
    state->policy->OnIsVisibleChanged(&state->page_node);

    // Phase 10: Go invisible with audio contexts → NEED_BG_TASK
    // Triggers second WebAudio check in MaybeChangeBackgroundTask
    state->page_node.SetVisible(false);
    state->policy->OnIsVisibleChanged(&state->page_node);

    // Phase 11: Stop media → NO_CHANGE (audio still active)
    state->page_node.SetMediaPlaying(false);
    state->policy->OnIsMediaPlayingChanged(&state->page_node);

    // Phase 12: Stop audio → NO_NEED_BG_TASK (all media stopped)
    state->page_node.SetAudible(false);
    state->policy->OnIsAudibleChanged(&state->page_node);
    // audio_state_num_==0 → clear audio_context_players
    // ProcessAudioContextPlayers called → early return (no switch)

    // Phase 13: Setup for OnDecrementAudioNum
    content::GlobalRenderFrameHostId id3(3, 300);
    state->policy->OnAudioContextPlaybackStarted(id3, 3);
    content::GlobalRenderFrameHostId id4(4, 400);
    state->policy->OnAudioContextPlaybackStarted(id4, 4);
    state->policy->audio_state_num_ = 2;
    state->policy->media_playing_num_ = 2;

    // Phase 14: OnDecrementAudioNum - normal decrement (not to zero)
    state->policy->OnDecrementAudioNum(&state->page_node);

    // Phase 15: OnDecrementAudioNum - decrement to zero, trigger clear
    state->policy->OnDecrementAudioNum(&state->page_node);

    // Phase 16: OnDecrementAudioNum - from zero (negative clamp)
    state->policy->OnDecrementAudioNum(&state->page_node);

    // Phase 17: OnDecrementAudioNum with nullptr
    state->policy->OnDecrementAudioNum(nullptr);

    // Phase 18: OnIsAudibleChanged - negative clamp
    state->policy->OnIsAudibleChanged(&state->page_node);

    // Phase 19: OnIsMediaPlayingChanged - negative clamp
    state->policy->OnIsMediaPlayingChanged(&state->page_node);

    // Phase 20: OnIsVisibleChanged - negative clamp
    state->policy->OnIsVisibleChanged(&state->page_node);

    // Phase 21: Audio context stop existing entry
    content::GlobalRenderFrameHostId id5(5, 500);
    state->policy->OnAudioContextPlaybackStarted(id5, 5);
    state->policy->OnAudioContextPlaybackStopped(id5, 5);

    // Phase 22: Audio context stop non-existent entry
    content::GlobalRenderFrameHostId id_nonexist(6, 600);
    state->policy->OnAudioContextPlaybackStopped(id_nonexist, 600);

    // Phase 23: OnIsAudibleChanged audible=true then false with entries
    content::GlobalRenderFrameHostId id6(7, 700);
    state->policy->OnAudioContextPlaybackStarted(id6, 7);
    state->policy->audio_state_num_ = 1;
    state->page_node.SetAudible(true);
    state->policy->OnIsAudibleChanged(&state->page_node);

    state->page_node.SetAudible(false);
    state->policy->OnIsAudibleChanged(&state->page_node);
    // audio_state_num_==0 → clear audio_context_players
    // ProcessAudioContextPlayers called → early return

    // Phase 24: Second OnPageNodeAdded
    state->page_node.SetVisible(true);
    state->policy->OnPageNodeAdded(&state->page_node);

    // Phase 25: OnBeforePageNodeRemoved
    state->policy->OnBeforePageNodeRemoved(&state->page_node);

    // Phase 26: OnTakenFromGraph
    state->policy->OnTakenFromGraph(&state->graph);

    // Phase 27: Null pointer tests
    state->policy->OnPassedToGraph(nullptr);
    state->policy->OnTakenFromGraph(nullptr);
    state->policy->OnPageNodeAdded(nullptr);
    state->policy->OnBeforePageNodeRemoved(nullptr);
    state->policy->OnIsVisibleChanged(nullptr);
    state->policy->OnIsMediaPlayingChanged(nullptr);
    state->policy->OnIsAudibleChanged(nullptr);
}
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == nullptr || size < sizeof(uint32_t)) {
        return 0;
    }

    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        base::CommandLine::Init(0, nullptr);
    }

    static FuzzerState state;

    FuzzedDataProvider fdp(data, size);

    uint8_t scenario = fdp.ConsumeIntegralInRange<uint8_t>(0,
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
        15
#else
        9
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    );
    switch (scenario) {
    case 0:
        FuzzNullPointers(&state);
        break;
    case 1:
        FuzzFullLifecycle(&state, &fdp);
        break;
    case 2:
        FuzzStateTransitions(&state, &fdp);
        break;
    case 3:
        FuzzRapidStateFlips(&state, &fdp);
        break;
    case 4:
        FuzzMaybeChangeBackgroundTaskBranches(&state, &fdp);
        break;
    case 5:
        FuzzDeterministicBgTaskCycle(&state);
        break;
    case 6:
        FuzzNegativeCountClamp(&state);
        break;
    case 7:
        FuzzAllBranchesExhaustive(&state);
        break;
    case 8:
        FuzzNegativeClampAll(&state);
        break;
    case 9:
        FuzzNullPointers(&state);
        FuzzAllBranchesExhaustive(&state);
        break;
#if BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    case 10:
        FuzzAudioContextLifecycle(&state, &fdp);
        break;
    case 11:
        FuzzAudioContextWithVisibility(&state, &fdp);
        break;
    case 12:
        FuzzDecrementFromZero(&state);
        break;
    case 13:
        FuzzAudibleChangedAllPaths(&state);
        break;
    case 14:
        FuzzOnDecrementAudioNumAllPaths(&state);
        break;
    case 15:
        FuzzMegaAllPaths(&state);
        break;
#endif  // BUILDFLAG(ARKWEB_PERFORMANCE_PERSISTENT_TASK)
    default:
        break;
    }

    return 0;
}
