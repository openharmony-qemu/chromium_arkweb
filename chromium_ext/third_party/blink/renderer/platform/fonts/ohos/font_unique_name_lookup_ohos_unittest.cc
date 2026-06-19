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
#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#endif // ARKWEB_CUSTOM_FONTS

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

#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
class MockArkwebCustomFontLookup : public mojom::blink::ArkwebCustomFontLookup {
 public:
  MOCK_METHOD(
      void,
      GetUniqueNameLookupTable,
      (mojom::blink::ArkwebCustomFontLookup::GetUniqueNameLookupTableCallback
           callback),
      (override));
  MOCK_METHOD(
      void,
      MatchLocalFontByUniqueName,
      (const ::blink::String& font_unique_name,
       mojom::blink::ArkwebCustomFontLookup::MatchLocalFontByUniqueNameCallback
           callback),
      (override));
  MOCK_METHOD(void,
              FetchAllFontFiles,
              (mojom::blink::ArkwebCustomFontLookup::FetchAllFontFilesCallback
                   callback),
              (override));

  void Bind(
      mojo::PendingReceiver<mojom::blink::ArkwebCustomFontLookup> receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<mojom::blink::ArkwebCustomFontLookup> receiver_{this};
};
#endif
}  // namespace

class FontUniqueNameLookupOhosTest : public testing::Test {
 protected:
  void SetUp() override {
    service_ = std::make_unique<MockFontUniqueNameLookup>();
    mojo::Remote<mojom::blink::FontUniqueNameLookup> remote;
    lookup_.ohos_font_lookup_service_ = std::move(remote);
#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
    SetUpCustomFontsService();
#endif
  }

  void TearDown() override {
#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
    TearDownCustomFontsService();
#endif
  }

#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
  void SetUpCustomFontsService() {
    custom_fonts_service_ = std::make_unique<MockArkwebCustomFontLookup>();
    mojo::PendingRemote<mojom::blink::ArkwebCustomFontLookup>
        custom_fonts_remote;
    custom_fonts_service_->Bind(
        custom_fonts_remote.InitWithNewPipeAndPassReceiver());
    Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
        mojom::blink::ArkwebCustomFontLookup::Name_,
        base::BindLambdaForTesting([this](
                                       mojo::ScopedMessagePipeHandle handle) {
          mojo::PendingReceiver<mojom::blink::ArkwebCustomFontLookup> receiver(
              std::move(handle));
          custom_fonts_service_->Bind(std::move(receiver));
        }));
  }

  void TearDownCustomFontsService() { custom_fonts_service_.reset(); }

  void EnsureCustomFontsServiceConnected() {
    if (!custom_fonts_service_) {
      return;
    }
    if (!lookup_.arkweb_custom_font_lookup_service_) {
      Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
          lookup_.arkweb_custom_font_lookup_service_
              .BindNewPipeAndPassReceiver());
    }
  }

  void GetTestRelativePath(const std::string& relative_path,
                           base::FilePath& result) {
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &result);
    result = result.Append(FILE_PATH_LITERAL(relative_path));
    if (!base::PathExists(result)) {
      base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &result);
      result = result.Append(FILE_PATH_LITERAL(relative_path));
    }
  }

  std::unique_ptr<MockArkwebCustomFontLookup> custom_fonts_service_;
#endif
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

TEST_F(FontUniqueNameLookupOhosTest, CustomFonts_Init) {
  ScopedFontSrcLocalMatchingForTest scope(false);
  EnsureCustomFontsServiceConnected();
  lookup_.Init();
  EXPECT_TRUE(lookup_.prefetched_font_map_.empty());
}

TEST_F(FontUniqueNameLookupOhosTest,
       CustomFonts_RequestedNameInQueryableFonts001) {
  std::string font_name = "testname";
  EnsureCustomFontsServiceConnected();
  EXPECT_CALL(*custom_fonts_service_, GetUniqueNameLookupTable)
      .WillOnce(
          testing::Invoke([](mojom::blink::ArkwebCustomFontLookup::
                                 GetUniqueNameLookupTableCallback callback) {
            ::blink::Vector<::blink::String> fake_fonts;
            std::move(callback).Run(std::move(fake_fonts));
          }));
  auto result = lookup_.RequestedNameInQueryableFonts(String(font_name));
  EXPECT_FALSE(result);
}

TEST_F(FontUniqueNameLookupOhosTest,
       CustomFonts_MatchUniqueNameFromArkwebCustomFonts001) {
  std::map<std::string, std::string> test_data = {
      {"name_test", "/path/to/test/font/file"}};
  base::FilePath test_data_path;
  std::string relative_path = "third_party/blink/web_tests/external/wpt/fonts";
  GetTestRelativePath(relative_path, test_data_path);
  std::string valid_font_name = "name_valid_font";
  test_data.emplace(valid_font_name, test_data_path.Append("Ahem.ttf").value());
  std::string invalid_font_name = "name_invalid_font";
  test_data.emplace(invalid_font_name,
                    test_data_path.Append("Ahem.ttf.headers").value());

  std::string valid_empty_name = "name_valid_empty";
  base::ScopedTempDir temp_dir;
  if (temp_dir.CreateUniqueTempDir()) {
    base::FilePath empty_font_path;
    if (base::CreateTemporaryFileInDir(temp_dir.GetPath(), &empty_font_path)) {
      test_data.emplace(valid_empty_name, empty_font_path.value());
    }
  }

  HashMap<String, base::File> font_files;
  for (auto it = test_data.begin(); it != test_data.end(); it++) {
    auto font_name_string = String(it->first);
    auto file_path = base::FilePath(it->second);
    auto font_file =
        base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    font_files.insert(std::move(font_name_string), std::move(font_file));
  }
  lookup_.prefetched_font_map_ = std::move(font_files);
  lookup_.queryable_fonts_.emplace(lookup_.prefetched_font_map_.Keys());
  std::string orphan_name = "name_orphan";
  lookup_.queryable_fonts_->push_back(String(orphan_name));

  EnsureCustomFontsServiceConnected();
  std::string invalid_name = "name_invalid";
  auto result =
      lookup_.MatchUniqueNameFromArkwebCustomFonts(String(invalid_name));
  EXPECT_EQ(result, nullptr);

  std::string test_name = "name_test";
  result = lookup_.MatchUniqueNameFromArkwebCustomFonts(String(test_name));
  EXPECT_EQ(result, nullptr);

  EXPECT_CALL(*custom_fonts_service_, MatchLocalFontByUniqueName)
      .WillOnce(
          testing::Invoke([](const blink::String& name,
                             mojom::blink::ArkwebCustomFontLookup::
                                 MatchLocalFontByUniqueNameCallback callback) {
            std::move(callback).Run(::base::File());
          }));
  result = lookup_.MatchUniqueNameFromArkwebCustomFonts(String(orphan_name));
  EXPECT_EQ(result, nullptr);

  EXPECT_CALL(*custom_fonts_service_, MatchLocalFontByUniqueName);
  result = lookup_.MatchUniqueNameFromArkwebCustomFonts(String(orphan_name));
  EXPECT_EQ(result, nullptr);
  result =
      lookup_.MatchUniqueNameFromArkwebCustomFonts(String(valid_empty_name));
  EXPECT_EQ(result, nullptr);
  result =
      lookup_.MatchUniqueNameFromArkwebCustomFonts(String(invalid_font_name));
  EXPECT_EQ(result, nullptr);

  auto combine_valid_case = String(valid_font_name);
  result = lookup_.MatchUniqueName(combine_valid_case);
  EXPECT_NE(result, nullptr);
}
#endif  // ARKWEB_CUSTOM_FONTS
}  // namespace blink