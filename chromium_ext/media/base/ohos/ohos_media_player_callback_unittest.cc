// Copyright (c) 2025 Huawei Device Co., Ltd.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "ohos_media_player_callback.h"

#include "arkweb/build/features/features.h"
#include "arkweb/ohos_adapter_ndk/interfaces/ohos_adapter_helper.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "ohos_media_player_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace media;
using namespace testing;
using namespace OHOS::NWeb;
using namespace base;

namespace media {
using namespace OHOS::NWeb;

namespace {

// Manual mock to avoid gmock InternalExpectedAt OOM crash under LTO.
class FakeClient final : public OHOSMediaPlayerBridge::Client {
 public:
  ~FakeClient() = default;

  void OnFrameAvailable(int fd,
                        uint32_t size,
                        int32_t coded_width,
                        int32_t coded_height,
                        int32_t visible_width,
                        int32_t visible_height,
                        int32_t format) override {
    frame_available_count_++;
  }

  void OnMediaDurationChanged(base::TimeDelta duration) override {
    duration_changed_count_++;
    last_duration_ = duration;
  }

  void OnPlaybackComplete() override { playback_complete_count_++; }

  void OnError(int error) override {
    error_count_++;
    last_error_ = error;
  }

  void OnVideoSizeChanged(int width, int height) override {
    video_size_changed_count_++;
    last_width_ = width;
    last_height_ = height;
  }

  void OnPlayerInterruptEvent(int32_t value) override {
    interrupt_event_count_++;
    last_interrupt_value_ = value;
  }

  void OnAudioStateChanged(bool isAudible) override {
    audio_state_changed_count_++;
  }

  void OnPlayerSeekBack(base::TimeDelta back_time) override {
    seek_back_count_++;
    last_back_time_ = back_time;
  }

  OHOSMediaResourceGetter* GetMediaResourceGetter() override { return nullptr; }

  // Counters for verifying callback invocations.
  int frame_available_count_ = 0;
  int duration_changed_count_ = 0;
  int playback_complete_count_ = 0;
  int error_count_ = 0;
  int video_size_changed_count_ = 0;
  int interrupt_event_count_ = 0;
  int audio_state_changed_count_ = 0;
  int seek_back_count_ = 0;
  int last_error_ = 0;
  int last_width_ = 0;
  int last_height_ = 0;
  int32_t last_interrupt_value_ = 0;
  base::TimeDelta last_duration_;
  base::TimeDelta last_back_time_;
};

}  // namespace

class OHOSMediaPlayerCallbackTest : public ::testing::Test {
 public:
  OHOSMediaPlayerCallbackTest() = default;
  ~OHOSMediaPlayerCallbackTest() override = default;

  void SetUp() override {
    gurl_ = std::make_unique<GURL>("http://example.com");
    fake_client_ = std::make_unique<FakeClient>();

    bridge_ = new OHOSMediaPlayerBridge(
        *gurl_.get(), site_for_cookies_, top_frame_origin_, user_agent_,
        storage_access_api_status_, hide_url_log_, fake_client_.get(),
        allow_credentials_, is_hls_, headers_, grantMediaFileAccessDirs_);

    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    media_player_callback_ = std::make_unique<OHOSMediaPlayerCallback>(
        task_runner_, bridge_->weak_factory_.GetWeakPtr());
  }

  void TearDown() override {
    media_player_callback_.reset();
    if (bridge_) {
      delete bridge_;
      bridge_ = nullptr;
    }
    gurl_.reset();
    fake_client_.reset();
  }

  // Helper methods to access private members via friend declarations.
  base::WeakPtr<OHOSMediaPlayerBridge>& GetCallbackMediaPlayer() {
    return media_player_callback_->media_player_;
  }

  base::WeakPtrFactory<OHOSMediaPlayerBridge>& GetBridgeWeakFactory() {
    return bridge_->weak_factory_;
  }

  std::unique_ptr<OHOS::NWeb::PlayerAdapter>& GetBridgePlayer() {
    return bridge_->player_;
  }

  OHOS::NWeb::PlayerAdapter::PlayerStates& GetBridgePlayerState() {
    return bridge_->player_state_;
  }

  base::TimeDelta& GetBridgeExtraTime() { return bridge_->extra_time_; }

  bool& GetBridgeSeekComplete() { return bridge_->seek_complete_; }

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

  std::unique_ptr<OHOSMediaPlayerCallback> media_player_callback_;
  OHOSMediaPlayerBridge* bridge_;
  net::SiteForCookies site_for_cookies_;
  url::Origin top_frame_origin_;
  std::string user_agent_{"test"};
  net::StorageAccessApiStatus storage_access_api_status_ =
      net::StorageAccessApiStatus::kAccessViaAPI;
  bool hide_url_log_ = false;
  bool allow_credentials_ = true;
  bool is_hls_ = false;
  base::flat_map<std::string, std::string> headers_;
  std::vector<std::string> grantMediaFileAccessDirs_;
  std::unique_ptr<GURL> gurl_;
  std::unique_ptr<FakeClient> fake_client_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

// --- OnError tests ---

TEST_F(OHOSMediaPlayerCallbackTest, OnError_InvalidCode) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();

  media_player_callback_->OnError(
      OHOS::NWeb::PlayerAdapterErrorType::INVALID_CODE);
  RunPendingTasks();

  EXPECT_EQ(fake_client_->error_count_, 1);
  EXPECT_EQ(fake_client_->last_error_,
            OHOSMediaPlayerBridge::MediaErrorType::MEDIA_ERROR_INVALID_CODE);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnError_UnsupportType) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();

  media_player_callback_->OnError(
      OHOS::NWeb::PlayerAdapterErrorType::UNSUPPORT_TYPE);
  RunPendingTasks();

  EXPECT_EQ(fake_client_->error_count_, 1);
  EXPECT_EQ(fake_client_->last_error_,
            OHOSMediaPlayerBridge::MediaErrorType::MEDIA_ERROR_FORMAT);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnError_FatalError) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();

  media_player_callback_->OnError(
      OHOS::NWeb::PlayerAdapterErrorType::FATAL_ERROR);
  RunPendingTasks();

  EXPECT_EQ(fake_client_->error_count_, 1);
  EXPECT_EQ(fake_client_->last_error_,
            OHOSMediaPlayerBridge::MediaErrorType::MEDIA_ERROR_DECODE);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnError_NullMediaPlayer) {
  // Invalidate the weak pointer by deleting the bridge.
  delete bridge_;
  bridge_ = nullptr;
  // OnError should not crash when media_player_ is null.
  media_player_callback_->OnError(
      OHOS::NWeb::PlayerAdapterErrorType::INVALID_CODE);
  RunPendingTasks();

  EXPECT_EQ(fake_client_->error_count_, 0);
}

// --- OnInfo tests ---

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_UnsetType) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_UNSET,
                                 TEST_EXTRA, TEST_VALUE);
  RunPendingTasks();
  // Unset type does not trigger any client callback.
  EXPECT_EQ(fake_client_->playback_complete_count_, 0);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_MessageType) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(
      OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_MESSAGE, TEST_EXTRA, TEST_VALUE);
  RunPendingTasks();
  // Message type does not trigger any client callback.
  EXPECT_EQ(fake_client_->playback_complete_count_, 0);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_EosType) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_EOS,
                                 TEST_EXTRA, TEST_VALUE);
  RunPendingTasks();

  EXPECT_EQ(fake_client_->playback_complete_count_, 1);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_StateChange) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  GetBridgePlayer() =
      OHOS::NWeb::OhosAdapterHelper::GetInstance().CreatePlayerAdapter();
  media_player_callback_->OnInfo(
      OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_STATE_CHANGE, TEST_EXTRA,
      TEST_VALUE);
  RunPendingTasks();
  ASSERT_EQ(GetBridgePlayerState(), TEST_EXTRA);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_PositionUpdate) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(
      OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_POSITION_UPDATE, TEST_EXTRA,
      TEST_VALUE);
  RunPendingTasks();
  ASSERT_EQ(GetBridgeExtraTime(), base::Milliseconds(TEST_EXTRA));
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_InterruptEvent) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(
      OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_INTERRUPT_EVENT, TEST_EXTRA,
      TEST_VALUE);
  RunPendingTasks();

  EXPECT_EQ(fake_client_->interrupt_event_count_, 1);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_SeekDone) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(
      OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_SEEKDONE, TEST_EXTRA, TEST_VALUE);
  RunPendingTasks();
  ASSERT_EQ(GetBridgeSeekComplete(), true);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_ResolutionChange) {
  GetCallbackMediaPlayer() = GetBridgeWeakFactory().GetWeakPtr();
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(
      OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_RESOLUTION_CHANGE, TEST_EXTRA,
      TEST_VALUE);
  RunPendingTasks();

  EXPECT_EQ(fake_client_->video_size_changed_count_, 1);
  EXPECT_EQ(fake_client_->last_width_, TEST_EXTRA);
  EXPECT_EQ(fake_client_->last_height_, TEST_VALUE);
}

TEST_F(OHOSMediaPlayerCallbackTest, OnInfo_NullMediaPlayer) {
  // Invalidate the weak pointer by deleting the bridge.
  delete bridge_;
  bridge_ = nullptr;
  // OnInfo should not crash when media_player_ is null.
  constexpr int32_t TEST_EXTRA = 5;
  constexpr int32_t TEST_VALUE = 5;

  media_player_callback_->OnInfo(OHOS::NWeb::PlayerOnInfoType::INFO_TYPE_EOS,
                                 TEST_EXTRA, TEST_VALUE);
  RunPendingTasks();
  // No callback should be invoked since media_player_ is null.
  EXPECT_EQ(fake_client_->playback_complete_count_, 0);
}

}  // namespace media
