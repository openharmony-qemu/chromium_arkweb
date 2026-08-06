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

#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/audio_system_manager_adapter.h"
#include "arkweb/chromium_ext/content/browser/media/session/audio_interrupt_adapter_impl.h"

namespace OHOS::NWeb {

// AudioInterruptAdapterImpl is a pure data holder: 5 members with default
// values exposed through 5 getter/setter pairs. These tests verify the
// constructor defaults and round-trip every setter through its getter.

TEST(AudioInterruptAdapterImplTest, DefaultValues) {
  AudioInterruptAdapterImpl impl;
  EXPECT_EQ(impl.GetStreamUsage(), AudioAdapterStreamUsage::STREAM_USAGE_MEDIA);
  EXPECT_EQ(impl.GetContentType(), AudioAdapterContentType::CONTENT_TYPE_MUSIC);
  EXPECT_EQ(impl.GetStreamType(), AudioAdapterStreamType::STREAM_MUSIC);
  EXPECT_EQ(impl.GetSessionID(), 0u);
  EXPECT_FALSE(impl.GetPauseWhenDucked());
}

TEST(AudioInterruptAdapterImplTest, SetAndGetStreamUsage) {
  AudioInterruptAdapterImpl impl;
  impl.SetStreamUsage(
      AudioAdapterStreamUsage::STREAM_USAGE_VOICE_COMMUNICATION);
  EXPECT_EQ(impl.GetStreamUsage(),
            AudioAdapterStreamUsage::STREAM_USAGE_VOICE_COMMUNICATION);
  impl.SetStreamUsage(
      AudioAdapterStreamUsage::STREAM_USAGE_NOTIFICATION_RINGTONE);
  EXPECT_EQ(impl.GetStreamUsage(),
            AudioAdapterStreamUsage::STREAM_USAGE_NOTIFICATION_RINGTONE);
  impl.SetStreamUsage(AudioAdapterStreamUsage::STREAM_USAGE_UNKNOWN);
  EXPECT_EQ(impl.GetStreamUsage(),
            AudioAdapterStreamUsage::STREAM_USAGE_UNKNOWN);
}

TEST(AudioInterruptAdapterImplTest, SetAndGetContentType) {
  AudioInterruptAdapterImpl impl;
  impl.SetContentType(AudioAdapterContentType::CONTENT_TYPE_SPEECH);
  EXPECT_EQ(impl.GetContentType(),
            AudioAdapterContentType::CONTENT_TYPE_SPEECH);
  impl.SetContentType(AudioAdapterContentType::CONTENT_TYPE_MOVIE);
  EXPECT_EQ(impl.GetContentType(), AudioAdapterContentType::CONTENT_TYPE_MOVIE);
  impl.SetContentType(AudioAdapterContentType::CONTENT_TYPE_RINGTONE);
  EXPECT_EQ(impl.GetContentType(),
            AudioAdapterContentType::CONTENT_TYPE_RINGTONE);
}

TEST(AudioInterruptAdapterImplTest, SetAndGetStreamType) {
  AudioInterruptAdapterImpl impl;
  impl.SetStreamType(AudioAdapterStreamType::STREAM_VOICE_CALL);
  EXPECT_EQ(impl.GetStreamType(), AudioAdapterStreamType::STREAM_VOICE_CALL);
  impl.SetStreamType(AudioAdapterStreamType::STREAM_ALL);
  EXPECT_EQ(impl.GetStreamType(), AudioAdapterStreamType::STREAM_ALL);
  impl.SetStreamType(AudioAdapterStreamType::STREAM_RING);
  EXPECT_EQ(impl.GetStreamType(), AudioAdapterStreamType::STREAM_RING);
}

TEST(AudioInterruptAdapterImplTest, SetAndGetSessionID) {
  AudioInterruptAdapterImpl impl;
  impl.SetSessionID(0);
  EXPECT_EQ(impl.GetSessionID(), 0u);
  impl.SetSessionID(1);
  EXPECT_EQ(impl.GetSessionID(), 1u);
  impl.SetSessionID(UINT32_MAX);
  EXPECT_EQ(impl.GetSessionID(), UINT32_MAX);
}

TEST(AudioInterruptAdapterImplTest, SetAndGetPauseWhenDucked) {
  AudioInterruptAdapterImpl impl;
  impl.SetPauseWhenDucked(true);
  EXPECT_TRUE(impl.GetPauseWhenDucked());
  impl.SetPauseWhenDucked(false);
  EXPECT_FALSE(impl.GetPauseWhenDucked());
}

}  // namespace OHOS::NWeb
