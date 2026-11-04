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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "arkweb/build/features/features.h"
#include "arkweb/chromium_ext/content/browser/font_unique_name_lookup/arkweb_custom_font_manager.h"
#include "base/test/task_environment.h"

namespace content {

class ArkwebCustomFontManagerTest : public ::testing::Test {
public:
  ArkwebCustomFontManagerTest() = default;
  ~ArkwebCustomFontManagerTest() = default;
  void SetUp() override {}
  void TearDown() override {}

  base::test::TaskEnvironment task_environment_;
};

#if BUILDFLAG(ARKWEB_CUSTOM_FONTS)
TEST_F(ArkwebCustomFontManagerTest, SetCustomFonts) {
  NWebCustomFontInfoVector testFonts;
  EXPECT_TRUE(
      ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts));
  testFonts.emplace_back("TestInvalid", "/path/to/invalid/file.txt");
  EXPECT_FALSE(
      ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts));
  for (int i = 0; i < 16; i += 3) {
    testFonts.emplace_back("TestName" + std::to_string(i + 1),
                           "/path/to/font/file.ttf");
    testFonts.emplace_back("TestName" + std::to_string(i + 2),
                           "/path/to/font/file.ttc");
    testFonts.emplace_back("TestName" + std::to_string(i + 3),
                           "/path/to/font/file.otf");
  }
  EXPECT_TRUE(
      ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts));
}

TEST_F(ArkwebCustomFontManagerTest, GetFontByUniqueName) {
  NWebCustomFontInfoVector testFonts;
  ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts);
  std::string testName = "TestName";
  auto testFile1 =
      ArkwebCustomFontManager::GetInstance()->GetFontByUniqueName(testName);
  EXPECT_FALSE(testFile1.IsValid());

  testFonts.emplace_back(testName, "/path/to/font/file.ttf");
  ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts);
  auto testFile2 =
      ArkwebCustomFontManager::GetInstance()->GetFontByUniqueName(testName);
  EXPECT_FALSE(testFile2.IsValid());
}

TEST_F(ArkwebCustomFontManagerTest, GetAllCustomFontsUniqueNames) {
  NWebCustomFontInfoVector testFonts;
  ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts);
  testFonts.emplace_back("TestName", "/path/to/font/file.ttf");
  ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts);
  auto results =
      ArkwebCustomFontManager::GetInstance()->GetAllCustomFontsUniqueNames();
  EXPECT_TRUE(!results.empty());
}

TEST_F(ArkwebCustomFontManagerTest, GetAllCustomFonts) {
  NWebCustomFontInfoVector testFonts;
  ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts);
  auto resultsEmpty =
      ArkwebCustomFontManager::GetInstance()->GetAllCustomFonts();
  EXPECT_TRUE(resultsEmpty.empty());

  testFonts.emplace_back("TestName1", "/path/to/font/file1.ttf");
  testFonts.emplace_back("TestName2", "/path/to/font/file2.ttf");
  ArkwebCustomFontManager::GetInstance()->SetCustomFonts(testFonts);
  auto results = ArkwebCustomFontManager::GetInstance()->GetAllCustomFonts();
  EXPECT_TRUE(results.empty());
}
#endif // ARKWEB_CUSTOM_FONTS
}  // namespace content
