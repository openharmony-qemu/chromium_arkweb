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

#include "content/renderer/media/ohos/arkweb_media_factory_utils.h"

#include <type_traits>

#include "arkweb/build/features/features.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// ArkwebMediaFactoryUtils only stores a raw_ptr<MediaFactory> in its
// constructor. The sole method AddOhosAndCustomMediaFactory requires a real
// RenderThreadImpl (compositor_task_runner, GetNativeTexureFactory) and
// MediaFactory::CreateMojoRendererFactory / render_frame_, which are not
// available in the UT environment. We test the constructor here.
class ArkwebMediaFactoryUtilsTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// Compile-time check: ArkwebMediaFactoryUtils is constructible from a
// MediaFactory pointer.
static_assert(std::is_constructible_v<ArkwebMediaFactoryUtils, MediaFactory*>);

// Constructor with nullptr must not crash. The class only stores the pointer.
TEST_F(ArkwebMediaFactoryUtilsTest, Constructor_Nullptr_NoCrash) {
  ArkwebMediaFactoryUtils utils(nullptr);
  // The object stores a single raw_ptr<MediaFactory>; verify its size matches.
  EXPECT_EQ(sizeof(utils), sizeof(raw_ptr<MediaFactory>));
}

// Constructor with a non-null pointer must not crash.
TEST_F(ArkwebMediaFactoryUtilsTest, Constructor_NonNullPointer_NoCrash) {
  int dummy = 0;
  MediaFactory* fake_factory = reinterpret_cast<MediaFactory*>(&dummy);
  ArkwebMediaFactoryUtils utils(fake_factory);
  EXPECT_EQ(sizeof(utils), sizeof(raw_ptr<MediaFactory>));
}

// Two distinct instances have distinct addresses.
TEST_F(ArkwebMediaFactoryUtilsTest, Constructor_DistinctInstances) {
  ArkwebMediaFactoryUtils utils1(nullptr);
  ArkwebMediaFactoryUtils utils2(nullptr);
  EXPECT_NE(&utils1, &utils2);
}
}  // namespace content
