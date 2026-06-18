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

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/android_font_lookup/android_font_lookup.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#define private public
#include "third_party/blink/renderer/platform/fonts/ohos/font_unique_name_lookup_ohos.h"
#undef private

using testing::_;
using testing::Invoke;

namespace blink {

namespace {
class MockFontUniqueNameLookup : public mojom::blink::FontUniqueNameLookup {
 public:
  MOCK_METHOD(bool,
              GetUniqueNameLookupTableIfAvailable,
              (bool* sync_available, base::ReadOnlySharedMemoryRegion* region),
              (override));
  MOCK_METHOD(void,
              GetUniqueNameLookupTableIfAvailable,
              (GetUniqueNameLookupTableIfAvailableCallback callback),
              (override));
  MOCK_METHOD(void,
              GetUniqueNameLookupTable,
              (GetUniqueNameLookupTableCallback callback),
              (override));
};
}  // namespace

class FontUniqueNameLookupOhosTest : public testing::Test {
 protected:
  void SetUp() override {
    service_ = std::make_unique<MockFontUniqueNameLookup>();
    mojo::Remote<mojom::blink::FontUniqueNameLookup> remote;
    lookup_.ohos_font_lookup_service_ = std::move(remote);
  }

  base::test::TaskEnvironment task_environment_;
  FontUniqueNameLookupOhos lookup_;
  std::unique_ptr<MockFontUniqueNameLookup> service_;
  mojom::blink::FontUniqueNameLookup::GetUniqueNameLookupTableCallback saved_callback_;
};

TEST_F(FontUniqueNameLookupOhosTest, Init) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  lookup_.Init();
}

TEST_F(FontUniqueNameLookupOhosTest, EnsureServiceConnected) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  lookup_.EnsureServiceConnected();
}

TEST_F(FontUniqueNameLookupOhosTest, ReceiveReadOnlySharedMemoryRegion) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  base::ReadOnlySharedMemoryRegion shared_memory_region;
  lookup_.ReceiveReadOnlySharedMemoryRegion(std::move(shared_memory_region));
}

TEST_F(FontUniqueNameLookupOhosTest, MatchUniqueName_NotReady_ReturnsNull) {
  ScopedFontSrcLocalMatchingForTest scope(true);
  EXPECT_EQ(lookup_.MatchUniqueName("test_font"), nullptr);
}

TEST_F(FontUniqueNameLookupOhosTest, IsReady_WhenFeatureDisabled_ReturnsTrue) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  lookup_.ohos_font_lookup_service_.reset();
  EXPECT_TRUE(lookup_.IsFontUniqueNameLookupReadyForSyncLookup());
}

TEST_F(FontUniqueNameLookupOhosTest, PrepareLookup_QueuesCallbacks) {
  ScopedFontSrcLocalMatchingForTest scope(true);

  bool callback1_called = false;
  bool callback2_called = false;

  lookup_.PrepareFontUniqueNameLookup(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback1_called));

  lookup_.PrepareFontUniqueNameLookup(
      base::BindOnce([](bool* flag) { *flag = true; }, &callback2_called));
}

TEST_F(FontUniqueNameLookupOhosTest, MatchUniqueName001) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  base::MappedReadOnlyRegion mapped_region = base::ReadOnlySharedMemoryRegion::Create(1024);
  base::ReadOnlySharedMemoryRegion region = std::move(mapped_region.region);
  lookup_.ReceiveReadOnlySharedMemoryRegion(std::move(region));
  EXPECT_EQ(lookup_.MatchUniqueName("test_font"), nullptr);
}

TEST_F(FontUniqueNameLookupOhosTest, MatchUniqueName002) {
  ScopedFontSrcLocalMatchingForTest scope(true);
  base::MappedReadOnlyRegion mapped_region = base::ReadOnlySharedMemoryRegion::Create(1024);
  base::ReadOnlySharedMemoryRegion region = std::move(mapped_region.region);
  lookup_.ReceiveReadOnlySharedMemoryRegion(std::move(region));
  EXPECT_EQ(lookup_.MatchUniqueName("Arial"), nullptr);
}

TEST_F(FontUniqueNameLookupOhosTest, IsFontUniqueNameLookupReadyForSyncLookup001) {
  ScopedFontSrcLocalMatchingForTest scope(true);
  lookup_.ohos_font_lookup_service_.reset();
  base::MappedReadOnlyRegion mapped_region = base::ReadOnlySharedMemoryRegion::Create(1024);
  base::ReadOnlySharedMemoryRegion region = std::move(mapped_region.region);
  lookup_.ReceiveReadOnlySharedMemoryRegion(std::move(region));
  bool flag = lookup_.IsFontUniqueNameLookupReadyForSyncLookup();
  EXPECT_TRUE(flag);
}

TEST_F(FontUniqueNameLookupOhosTest, IsFontUniqueNameLookupReadyForSyncLookup002) {
  ScopedFontSrcLocalMatchingForTest scope(true);
  lookup_.ohos_font_lookup_service_.reset();
  bool flag = lookup_.IsFontUniqueNameLookupReadyForSyncLookup();
  EXPECT_FALSE(flag);
}

TEST_F(FontUniqueNameLookupOhosTest, IsFontUniqueNameLookupReadyForSyncLookup003) {
  ScopedFontSrcLocalMatchingForTest scope(true);
  lookup_.ohos_font_lookup_service_.reset();
  bool flag = lookup_.IsFontUniqueNameLookupReadyForSyncLookup();
  flag = lookup_.IsFontUniqueNameLookupReadyForSyncLookup();
  EXPECT_FALSE(flag);
}

#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
TEST_F(FontUniqueNameLookupOhosTest, CustomFonts_CustomFontsPrefetched) {
  HashMap<String, base::File> font_files;
  auto font_name = String("testname");
  auto file_path = ::base::FilePath("/path/to/font/file");
  auto font_file = ::base::File(file_path, ::base::File::FLAG_OPEN |
                                               ::base::File::FLAG_READ);
  font_files.insert(std::move(font_name), std::move(font_file));
  lookup_.CustomFontsPrefetched(std::move(font_files));
  EXPECT_EQ(lookup_.prefetched_font_map_.size(), 1u);
  EXPECT_TRUE(lookup_.queryable_fonts_.has_value());
  EXPECT_EQ(lookup_.queryable_fonts_->size(), 1u);
}

TEST_F(FontUniqueNameLookupOhosTest,
       CustomFonts_RequestedNameInQueryableFonts) {
  std::string font_name = "testname";
  Vector<String> fake_fonts;
  fake_fonts.push_back(String(font_name));
  lookup_.queryable_fonts_ = std::move(fake_fonts);
  bool result = lookup_.RequestedNameInQueryableFonts(String(font_name));
  EXPECT_TRUE(result);
}

TEST_F(FontUniqueNameLookupOhosTest,
       CustomFonts_MatchUniqueNameFromArkwebCustomFonts) {
  std::string font_name = "testname";
  auto result = lookup_.MatchUniqueNameFromArkwebCustomFonts(String(font_name));
  EXPECT_EQ(result, nullptr);
}
#endif // ARKWEB_CUSTOM_FONTS
}  // namespace blink