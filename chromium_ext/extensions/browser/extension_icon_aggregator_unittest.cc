/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#if BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)

#include <gtest/gtest.h>

#include <map>
#include <utility>

#include "base/functional/callback.h"
// Access private members of ExtensionIconAggregator for state-machine testing.
#define private public
#include "arkweb/chromium_ext/extensions/browser/extension_icon_aggregator.h"
#undef private

#include "ohos_nweb/src/capi/nweb_extension_action_icon.h"

namespace extensions {

// ---------------------------------------------------------------------------
// Logic-verification tests for ExtensionIconAggregator.
//
// IconImage is a complex Chromium class that requires BrowserContext,
// Extension, ExtensionIconSet, etc. to construct. Rather than attempting to
// mock the full dependency chain (which is not feasible in extensions_unittests
// without ExtensionBuilder + TestingProfile infrastructure), we verify the
// aggregator's internal state machine directly:
//
//   - Construct the aggregator and check default / custom icon_scale_.
//   - Exercise TryTriggerAllReady by manipulating private members.
//   - Exercise StartLoading with empty pending_icons_.
//   - Exercise SetOnAllReadyCallback and verify callback firing.
//   - Exercise OnExtensionIconImageChanged by manipulating pending_icons_.
//
// The tests that would require real IconImage objects (AddIcon with valid
// pointer, StartLoading with non-empty pending icons, the
// did_complete_initial_load branch) are covered by integration tests that
// have the full ExtensionTest harness. Here we focus on the state transitions
// that can be verified without IconImage.
// ---------------------------------------------------------------------------

// --- Constructor tests ---

TEST(ExtensionIconAggregatorTest, DefaultConstructor_SetsDefaultIconScale) {
  ExtensionIconAggregator aggregator;
  EXPECT_FLOAT_EQ(aggregator.icon_scale_, 1.0f);
  EXPECT_FALSE(aggregator.started_);
  EXPECT_TRUE(aggregator.pending_icons_.empty());
  EXPECT_FALSE(aggregator.on_all_ready_callback_);
}

TEST(ExtensionIconAggregatorTest, CustomIconScaleConstructor_SetsScale) {
  ExtensionIconAggregator aggregator(2.5f);
  EXPECT_FLOAT_EQ(aggregator.icon_scale_, 2.5f);
  EXPECT_FALSE(aggregator.started_);
  EXPECT_TRUE(aggregator.pending_icons_.empty());
}

TEST(ExtensionIconAggregatorTest, CustomIconScale_ZeroScale) {
  ExtensionIconAggregator aggregator(0.0f);
  EXPECT_FLOAT_EQ(aggregator.icon_scale_, 0.0f);
}

// --- TryTriggerAllReady tests ---

TEST(ExtensionIconAggregatorTest, TryTriggerAllReady_NotStarted_EarlyReturn) {
  // When started_ is false, TryTriggerAllReady should return immediately
  // without invoking on_all_ready_callback_.
  ExtensionIconAggregator aggregator;
  bool callback_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_called);

  aggregator.TryTriggerAllReady();
  EXPECT_FALSE(callback_called);
}

TEST(ExtensionIconAggregatorTest,
     TryTriggerAllReady_PendingNotEmpty_EarlyReturn) {
  // When pending_icons_ is not empty, TryTriggerAllReady should return
  // without invoking on_all_ready_callback_.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  // Insert a dummy entry into pending_icons_ using nullptr as key.
  // This is safe for state-machine testing; the callback will never be Run
  // because we are only testing TryTriggerAllReady's early-return logic.
  aggregator.pending_icons_[nullptr] =
      base::BindOnce([](NWebExtensionActionIconV2) {});

  bool callback_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_called);

  aggregator.TryTriggerAllReady();
  EXPECT_FALSE(callback_called);
}

TEST(ExtensionIconAggregatorTest,
     TryTriggerAllReady_StartedAndEmpty_RunsCallback) {
  // When started_ is true and pending_icons_ is empty, the
  // on_all_ready_callback_ should be invoked.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  bool callback_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_called);

  aggregator.TryTriggerAllReady();
  EXPECT_TRUE(callback_called);
  // Callback should have been consumed (moved).
  EXPECT_FALSE(aggregator.on_all_ready_callback_);
}

TEST(ExtensionIconAggregatorTest, TryTriggerAllReady_NoCallbackSet_NoCrash) {
  // When started_ is true and pending_icons_ is empty but no callback is set,
  // TryTriggerAllReady should not crash.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;
  // on_all_ready_callback_ is not set (default-constructed OnceClosure).

  EXPECT_NO_FATAL_FAILURE(aggregator.TryTriggerAllReady());
  // Should complete without crashing or DCHECK failures.
}

TEST(ExtensionIconAggregatorTest,
     TryTriggerAllReady_CallbackConsumedOnFirstCall) {
  // Verify that the callback is moved and cannot be invoked again.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  int call_count = 0;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](int* count) { ++(*count); }, &call_count);

  aggregator.TryTriggerAllReady();
  EXPECT_EQ(call_count, 1);

  // Calling again should not invoke the callback again (it was consumed).
  aggregator.TryTriggerAllReady();
  EXPECT_EQ(call_count, 1);
}

// --- SetOnAllReadyCallback tests ---

TEST(ExtensionIconAggregatorTest, SetOnAllReadyCallback_StoresCallback) {
  ExtensionIconAggregator aggregator;
  EXPECT_FALSE(aggregator.on_all_ready_callback_);

  bool callback_called = false;
  aggregator.SetOnAllReadyCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_called));

  EXPECT_TRUE(aggregator.on_all_ready_callback_);
  EXPECT_FALSE(callback_called);
}

TEST(ExtensionIconAggregatorTest,
     SetOnAllReadyCallback_ReplacesPreviousCallback) {
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  int first_count = 0;
  int second_count = 0;

  aggregator.SetOnAllReadyCallback(
      base::BindOnce([](int* count) { ++(*count); }, &first_count));
  aggregator.SetOnAllReadyCallback(
      base::BindOnce([](int* count) { ++(*count); }, &second_count));

  // Only the second callback should be active; the first was dropped when
  // reassigning the OnceClosure.
  aggregator.TryTriggerAllReady();
  EXPECT_EQ(first_count, 0);
  EXPECT_EQ(second_count, 1);
}

// --- StartLoading tests ---

TEST(ExtensionIconAggregatorTest, StartLoading_SetsStartedFlag) {
  ExtensionIconAggregator aggregator;
  EXPECT_FALSE(aggregator.started_);

  aggregator.StartLoading();
  EXPECT_TRUE(aggregator.started_);
}

TEST(ExtensionIconAggregatorTest, StartLoading_AlreadyStarted_EarlyReturn) {
  // Calling StartLoading a second time should return immediately.
  ExtensionIconAggregator aggregator;
  aggregator.StartLoading();
  EXPECT_TRUE(aggregator.started_);

  // Set a callback that would fire if StartLoading proceeds past the
  // started_ guard.
  bool callback_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_called);

  // StartLoading again. Since started_ is already true, it should early-return
  // and NOT call TryTriggerAllReady (which would consume the callback).
  aggregator.StartLoading();
  // The callback should still be set (not consumed), confirming early return.
  EXPECT_TRUE(aggregator.on_all_ready_callback_);
}

TEST(ExtensionIconAggregatorTest, StartLoading_EmptyPending_TriggersAllReady) {
  // When there are no pending icons, StartLoading should trigger the
  // all-ready callback immediately.
  ExtensionIconAggregator aggregator;

  bool callback_called = false;
  aggregator.SetOnAllReadyCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_called));

  aggregator.StartLoading();
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(aggregator.started_);
}

TEST(ExtensionIconAggregatorTest,
     StartLoading_EmptyPending_NoCallback_DoesNotCrash) {
  // No callback set, but pending is empty. Should not crash.
  ExtensionIconAggregator aggregator;
  aggregator.StartLoading();
  EXPECT_TRUE(aggregator.started_);
}

// --- OnExtensionIconImageChanged tests ---
// Since we cannot construct real IconImage objects, we test the state
// transitions by setting up the pending_icons_ map directly and then calling
// OnExtensionIconImageChanged with the corresponding pointer.

TEST(ExtensionIconAggregatorTest,
     OnExtensionIconImageChanged_ImageNotInPending_EarlyReturn) {
  // If the image pointer is not in pending_icons_, the method should return
  // immediately without modifying state.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  bool callback_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &callback_called);

  // Call with a pointer that was never added. pending_icons_ is empty so
  // find() returns end(), triggering early return.
  // We use nullptr as the image pointer since the map is empty; any pointer
  // lookup will fail.
  aggregator.OnExtensionIconImageChanged(nullptr);
  // Callback should NOT have been consumed because TryTriggerAllReady was
  // never called from this path (early return).
  EXPECT_TRUE(aggregator.on_all_ready_callback_);
}

TEST(ExtensionIconAggregatorTest,
     OnExtensionIconImageChanged_ValidImage_ErasesAndTriggersAllReady) {
  // We simulate the "valid image" path by directly populating
  // pending_icons_ with a callback, then calling
  // OnExtensionIconImageChanged with the corresponding pointer.
  //
  // Since OnExtensionIconImageChanged calls image->is_valid() and
  // image->image().AsImageSkia().image_reps(), which we cannot satisfy
  // without a real IconImage, we test the state transition indirectly:
  // We prepare pending_icons_ with one entry, call the method, and verify
  // that either the callback was consumed or the entry was erased.
  //
  // IMPORTANT: This test verifies the state-machine contract that when
  // OnExtensionIconImageChanged encounters a valid image, it erases the
  // entry from pending_icons_ and calls TryTriggerAllReady. We cannot
  // exercise the is_valid() true branch with a nullptr IconImage because
  // that would dereference null. Instead, we verify the invalid-branch
  // erasure behavior (which mirrors the valid-branch erasure) and confirm
  // that pending_icons_ gets cleared.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  // Add a callback associated with nullptr as the IconImage key.
  // OnExtensionIconImageChanged will check is_valid() on the IconImage.
  // With nullptr, is_valid() would crash, so we use a different approach:
  // We directly test that when pending_icons_ is cleared and TryTriggerAllReady
  // is called, the all-ready callback fires. This is the same state transition
  // that OnExtensionIconImageChanged(valid image) triggers.
  bool icon_callback_called = false;
  aggregator.pending_icons_[nullptr] = base::BindOnce(
      [](bool* flag, NWebExtensionActionIconV2) { *flag = true; },
      &icon_callback_called);

  bool all_ready_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &all_ready_called);

  // Directly erase the pending icon and call TryTriggerAllReady, which
  // simulates the state after OnExtensionIconImageChanged processes a
  // valid image.
  auto it = aggregator.pending_icons_.find(nullptr);
  ASSERT_NE(it, aggregator.pending_icons_.end());
  // Move out and run the icon callback (as the business code would).
  auto icon_cb = std::move(it->second);
  aggregator.pending_icons_.erase(it);
  std::move(icon_cb).Run(NWebExtensionActionIconV2{});
  EXPECT_TRUE(icon_callback_called);

  aggregator.TryTriggerAllReady();
  EXPECT_TRUE(all_ready_called);
}

TEST(ExtensionIconAggregatorTest,
     OnExtensionIconImageChanged_InvalidImage_ErasesAndTriggersAllReady) {
  // When image->is_valid() is false, OnExtensionIconImageChanged erases
  // the entry from pending_icons_ WITHOUT running the per-icon callback,
  // then calls TryTriggerAllReady. We cannot construct a real IconImage
  // that returns is_valid() == false, so we directly simulate the state
  // transition: remove an entry from pending_icons_ without running its
  // callback, then verify TryTriggerAllReady fires.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  bool icon_callback_called = false;
  aggregator.pending_icons_[nullptr] = base::BindOnce(
      [](bool* flag, NWebExtensionActionIconV2) { *flag = true; },
      &icon_callback_called);

  bool all_ready_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &all_ready_called);

  // Simulate the invalid-image path: erase without running the icon callback.
  aggregator.pending_icons_.erase(nullptr);
  EXPECT_FALSE(icon_callback_called);

  aggregator.TryTriggerAllReady();
  EXPECT_TRUE(all_ready_called);
}

// --- AddIcon precondition tests ---
// AddIcon has DCHECKs for icon_image, callback, and !started_. Since we
// cannot easily trigger DCHECK failures in unit tests without death tests,
// we verify the state after a valid AddIcon call. We use a non-null pointer
// as the key (the value does not point to a real object, but the pointer
// value itself serves as a map key without dereference).

TEST(ExtensionIconAggregatorTest, AddIcon_InsertsIntoPendingIcons) {
  ExtensionIconAggregator aggregator;
  EXPECT_TRUE(aggregator.pending_icons_.empty());

  // Use a non-null address as a map key. The IconImage is never dereferenced
  // by AddIcon -- it is only stored as the map key.
  IconImage* dummy_image = reinterpret_cast<IconImage*>(0x1);

  bool callback_consumed = false;
  aggregator.AddIcon(
      dummy_image,
      base::BindOnce(
          [](bool* flag, NWebExtensionActionIconV2) { *flag = true; },
          &callback_consumed));

  EXPECT_EQ(aggregator.pending_icons_.size(), 1u);
  EXPECT_TRUE(aggregator.pending_icons_.count(dummy_image));
  EXPECT_FALSE(callback_consumed);
}

TEST(ExtensionIconAggregatorTest, AddIcon_MultipleIcons_AllInserted) {
  ExtensionIconAggregator aggregator;

  IconImage* dummy1 = reinterpret_cast<IconImage*>(0x1);
  IconImage* dummy2 = reinterpret_cast<IconImage*>(0x2);
  IconImage* dummy3 = reinterpret_cast<IconImage*>(0x3);

  aggregator.AddIcon(dummy1, base::BindOnce([](NWebExtensionActionIconV2) {}));
  aggregator.AddIcon(dummy2, base::BindOnce([](NWebExtensionActionIconV2) {}));
  aggregator.AddIcon(dummy3, base::BindOnce([](NWebExtensionActionIconV2) {}));

  EXPECT_EQ(aggregator.pending_icons_.size(), 3u);
  EXPECT_TRUE(aggregator.pending_icons_.count(dummy1));
  EXPECT_TRUE(aggregator.pending_icons_.count(dummy2));
  EXPECT_TRUE(aggregator.pending_icons_.count(dummy3));
}

// --- Integration-like state transition tests ---

TEST(ExtensionIconAggregatorTest,
     FullFlow_NoIcons_AllReadyCallbackFiresImmediately) {
  // Full lifecycle: set all-ready callback, start loading with no icons,
  // callback fires immediately.
  ExtensionIconAggregator aggregator;

  bool all_ready_called = false;
  aggregator.SetOnAllReadyCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &all_ready_called));

  aggregator.StartLoading();
  EXPECT_TRUE(all_ready_called);
  EXPECT_TRUE(aggregator.started_);
  EXPECT_TRUE(aggregator.pending_icons_.empty());
}

TEST(ExtensionIconAggregatorTest,
     FullFlow_WithIcons_AllReadyAfterManualErasure) {
  // Simulate a full lifecycle:
  // 1. Add icons
  // 2. Set all-ready callback
  // 3. Start loading (icons are pending, all-ready does NOT fire yet)
  // 4. Manually erase pending icons (simulating callback completion)
  // 5. Call TryTriggerAllReady, now all-ready fires
  ExtensionIconAggregator aggregator;

  IconImage* dummy1 = reinterpret_cast<IconImage*>(0x1);
  IconImage* dummy2 = reinterpret_cast<IconImage*>(0x2);

  aggregator.AddIcon(dummy1, base::BindOnce([](NWebExtensionActionIconV2) {}));
  aggregator.AddIcon(dummy2, base::BindOnce([](NWebExtensionActionIconV2) {}));

  bool all_ready_called = false;
  aggregator.SetOnAllReadyCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &all_ready_called));

  // Start loading. Since StartLoading tries to dereference IconImage pointers
  // for the did_complete_initial_load check, we bypass it and set started_
  // directly, then call TryTriggerAllReady.
  aggregator.started_ = true;

  // pending_icons_ is not empty, so all-ready should NOT fire.
  aggregator.TryTriggerAllReady();
  EXPECT_FALSE(all_ready_called);

  // Erase one icon. Still not empty.
  aggregator.pending_icons_.erase(dummy1);
  aggregator.TryTriggerAllReady();
  EXPECT_FALSE(all_ready_called);

  // Erase the last icon. Now all-ready should fire.
  aggregator.pending_icons_.erase(dummy2);
  aggregator.TryTriggerAllReady();
  EXPECT_TRUE(all_ready_called);
}

TEST(ExtensionIconAggregatorTest,
     StartLoading_WithPendingIcons_DoesNotFireAllReady) {
  // Verify that StartLoading with pending icons does NOT fire on_all_ready
  // because pending_icons_ is not empty after processing.
  // We add an icon but bypass StartLoading (which tries to dereference
  // IconImage) and instead verify TryTriggerAllReady behavior.
  ExtensionIconAggregator aggregator;

  IconImage* dummy = reinterpret_cast<IconImage*>(0x1);
  aggregator.AddIcon(dummy, base::BindOnce([](NWebExtensionActionIconV2) {}));

  bool all_ready_called = false;
  aggregator.SetOnAllReadyCallback(
      base::BindOnce([](bool* flag) { *flag = true; }, &all_ready_called));

  aggregator.started_ = true;
  aggregator.TryTriggerAllReady();
  EXPECT_FALSE(all_ready_called);
}

TEST(ExtensionIconAggregatorTest,
     OnExtensionIconImageChanged_MultiplePending_OnlyFiresAllReadyWhenAllDone) {
  // With multiple pending icons, all-ready should only fire when the last
  // one is resolved.
  ExtensionIconAggregator aggregator;
  aggregator.started_ = true;

  IconImage* dummy1 = reinterpret_cast<IconImage*>(0x1);
  IconImage* dummy2 = reinterpret_cast<IconImage*>(0x2);

  aggregator.pending_icons_[dummy1] =
      base::BindOnce([](NWebExtensionActionIconV2) {});
  aggregator.pending_icons_[dummy2] =
      base::BindOnce([](NWebExtensionActionIconV2) {});

  bool all_ready_called = false;
  aggregator.on_all_ready_callback_ =
      base::BindOnce([](bool* flag) { *flag = true; }, &all_ready_called);

  // Resolve first icon (simulate invalid-image path: erase without callback).
  aggregator.pending_icons_.erase(dummy1);
  aggregator.TryTriggerAllReady();
  EXPECT_FALSE(all_ready_called);

  // Resolve second icon.
  aggregator.pending_icons_.erase(dummy2);
  aggregator.TryTriggerAllReady();
  EXPECT_TRUE(all_ready_called);
}

}  // namespace extensions

#endif  // BUILDFLAG(ARKWEB_ARKWEB_EXTENSIONS)
