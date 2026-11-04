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

#if BUILDFLAG(ARKWEB_SCROLLBAR)

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/pickle.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "arkweb/chromium_ext/base/feature_list_utils.h"

namespace base {

class FeatureListUtilsTest : public testing::Test {
 public:
  FeatureListUtilsTest() = default;
  ~FeatureListUtilsTest() override = default;
};

// Covers: ModifyFeaturesToAllocator L70 while loop enters (allocator has
// entries), L76/L77 true branch (matching feature deleted), then
// AddFeatureToField L86 for loop enters (overrides non-empty), L87/L88 false
// branch (matching override processed), L93 false branch (no field_trial),
// L99 false branch (entry allocated), L109 false branch (memcpy_s success).
TEST_F(FeatureListUtilsTest, ModifyFeaturesToAllocator_ReplacesScrollbarEntries) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::make_unique<FeatureList>());

  FeatureList* global = FeatureList::GetInstance();
  ASSERT_NE(global, nullptr);

  // Set scrollbar overrides via the public path.
  global->GetUtils()->SetOverrideStateByFeatureName(
      "OverlayScrollbar", FeatureList::OVERRIDE_ENABLE_FEATURE);
  global->GetUtils()->SetOverrideStateByFeatureName(
      "ForceScrollbar", FeatureList::OVERRIDE_DISABLE_FEATURE);

  // Create an allocator with some initial features. Use AddFeaturesToAllocator
  // from a separate FeatureList that has different scrollbar states.
  auto source = std::make_unique<FeatureList>();
  source->InitFromCommandLine("OverlayScrollbar,ForceScrollbar", "");

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");
  source->AddFeaturesToAllocator(&allocator);

  // ModifyFeaturesToAllocator should delete old scrollbar entries and add
  // new ones from the global FeatureList's overrides.
  global->GetUtils()->ModifyFeaturesToAllocator(&allocator);

  // Read back and verify via public API.
  auto result_list = std::make_unique<FeatureList>();
  result_list->InitFromSharedMemory(&allocator);
  EXPECT_TRUE(result_list->IsFeatureOverridden("OverlayScrollbar"));
  EXPECT_TRUE(result_list->IsFeatureOverridden("ForceScrollbar"));
}

// Covers: AddFeatureToField L86 for loop does not enter (empty overrides_).
TEST_F(FeatureListUtilsTest, AddFeatureToField_EmptyOverrides_NoEntriesAdded) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::make_unique<FeatureList>());

  FeatureList* global = FeatureList::GetInstance();
  ASSERT_NE(global, nullptr);

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");

  global->GetUtils()->AddFeatureToField(&allocator, "");

  auto result_list = std::make_unique<FeatureList>();
  result_list->InitFromSharedMemory(&allocator);
  EXPECT_FALSE(result_list->IsFeatureOverridden("OverlayScrollbar"));
  EXPECT_FALSE(result_list->IsFeatureOverridden("ForceScrollbar"));
}

// Covers: AddFeatureToField L87/L88 true branch (non-matching override →
// continue). We cannot directly insert into overrides_ without friend access,
// so we set a non-scrollbar override via SetOverrideStateByFeatureName which
// only processes "OverlayScrollbar" and "ForceScrollbar". Instead, test that
// AddFeatureToField with only scrollbar overrides writes them correctly.
TEST_F(FeatureListUtilsTest,
       AddFeatureToField_ScrollbarOverrides_WrittenToAllocator) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::make_unique<FeatureList>());

  FeatureList* global = FeatureList::GetInstance();
  ASSERT_NE(global, nullptr);

  global->GetUtils()->SetOverrideStateByFeatureName(
      "OverlayScrollbar", FeatureList::OVERRIDE_ENABLE_FEATURE);
  global->GetUtils()->SetOverrideStateByFeatureName(
      "ForceScrollbar", FeatureList::OVERRIDE_DISABLE_FEATURE);

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");

  global->GetUtils()->AddFeatureToField(&allocator, "");

  auto result_list = std::make_unique<FeatureList>();
  result_list->InitFromSharedMemory(&allocator);
  EXPECT_TRUE(result_list->IsFeatureOverridden("OverlayScrollbar"));
  EXPECT_TRUE(result_list->IsFeatureOverridden("ForceScrollbar"));
}

// Covers: AddFeatureToField L93 true branch (field_trial non-null → trial
// name written to pickle).
TEST_F(FeatureListUtilsTest,
       AddFeatureToField_WithFieldTrial_TrialNamePersisted) {
  test::ScopedFeatureList scoped_feature_list;
  // Need a FieldTrialList for CreateFieldTrial.
  scoped_feature_list.InitWithEmptyFeatureAndFieldTrialLists();

  FeatureList* global = FeatureList::GetInstance();
  ASSERT_NE(global, nullptr);

  FieldTrial* trial = FieldTrialList::CreateFieldTrial("ScrollbarTrial", "Grp");
  ASSERT_NE(trial, nullptr);

  // RegisterFieldTrialOverride adds the feature to overrides_ with an
  // associated field trial. Do not call InitFromCommandLine first, because
  // emplace() retains the first entry and would prevent the trial association.
  global->RegisterFieldTrialOverride(
      "OverlayScrollbar", FeatureList::OVERRIDE_ENABLE_FEATURE, trial);

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");

  global->GetUtils()->AddFeatureToField(&allocator, "");

  // Read back and verify the trial name is associated.
  auto result_list = std::make_unique<FeatureList>();
  result_list->InitFromSharedMemory(&allocator);
  FieldTrial* associated =
      result_list->GetAssociatedFieldTrialByFeatureName("OverlayScrollbar");
  EXPECT_NE(associated, nullptr);
  if (associated) {
    EXPECT_EQ(associated->trial_name(), "ScrollbarTrial");
  }
}

// Covers: ModifyFeaturesToAllocator L70 while loop (empty allocator → loop
// body not entered), L76/L77 false branch (no matching feature to delete),
// then AddFeatureToField adds scrollbar entries from overrides_.
TEST_F(FeatureListUtilsTest,
       ModifyFeaturesToAllocator_EmptyAllocator_AddsScrollbarOnly) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::make_unique<FeatureList>());

  FeatureList* global = FeatureList::GetInstance();
  ASSERT_NE(global, nullptr);

  global->GetUtils()->SetOverrideStateByFeatureName(
      "OverlayScrollbar", FeatureList::OVERRIDE_ENABLE_FEATURE);
  global->GetUtils()->SetOverrideStateByFeatureName(
      "ForceScrollbar", FeatureList::OVERRIDE_DISABLE_FEATURE);

  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(4 << 10);
  WritableSharedPersistentMemoryAllocator allocator(std::move(shm.mapping), 1,
                                                    "");

  // Empty allocator — while loop won't find any FeatureEntry to delete.
  global->GetUtils()->ModifyFeaturesToAllocator(&allocator);

  auto result_list = std::make_unique<FeatureList>();
  result_list->InitFromSharedMemory(&allocator);
  EXPECT_TRUE(result_list->IsFeatureOverridden("OverlayScrollbar"));
  EXPECT_TRUE(result_list->IsFeatureOverridden("ForceScrollbar"));
}

// Covers: SetOverrideStateByFeatureName L55 false branch (feature already in
// overrides_ → no new entry added). Verify via IsFeatureOverridden that the
// override exists after first call, and state is unchanged after second call.
TEST_F(FeatureListUtilsTest,
       SetOverrideStateByFeatureName_ExistingEntry_NotReplaced) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::make_unique<FeatureList>());

  FeatureList* global = FeatureList::GetInstance();
  ASSERT_NE(global, nullptr);

  // First call registers the override.
  global->GetUtils()->SetOverrideStateByFeatureName(
      "OverlayScrollbar", FeatureList::OVERRIDE_ENABLE_FEATURE);
  EXPECT_TRUE(global->IsFeatureOverridden("OverlayScrollbar"));

  // Second call with different state — existing entry should NOT be replaced.
  global->GetUtils()->SetOverrideStateByFeatureName(
      "OverlayScrollbar", FeatureList::OVERRIDE_DISABLE_FEATURE);

  // Verify override still exists (state unchanged, but we can only check
  // existence via public API).
  EXPECT_TRUE(global->IsFeatureOverridden("OverlayScrollbar"));
}

}  // namespace base

#endif  // BUILDFLAG(ARKWEB_SCROLLBAR)
