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
#include "arkweb/chromium_ext/content/browser/media/session/media_session_controller_ext.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/media/session/media_session_controllers_manager.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MediaSessionControllersManagerForIncludeTest
    : public RenderViewHostTestHarness {
 public:
  MediaSessionControllersManagerForIncludeTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    web_contents_impl_ = static_cast<WebContentsImpl*>(web_contents());

    // Enable media session features so IsMediaSessionEnabled() returns true.
    scoped_feature_list_.InitWithFeatures({media::kInternalMediaSession}, {});

    manager_ =
        std::make_unique<MediaSessionControllersManager>(web_contents_impl_);

    GlobalRenderFrameHostId frame_routing_id =
        web_contents()->GetPrimaryMainFrame()->GetGlobalId();
    media_player_id_ = MediaPlayerId(frame_routing_id, 1);
  }

  void TearDown() override {
    manager_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  WebContentsImpl* web_contents_impl_ = nullptr;
  std::unique_ptr<MediaSessionControllersManager> manager_;
  MediaPlayerId media_player_id_ = MediaPlayerId::CreateMediaPlayerIdForTests();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(ARKWEB_MEDIA_AVSESSION)
// OnEndAVSession: controller non-null → AsMediaSessionControllerExt non-null
// → OnEndAVSession called (L29-31).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       OnEndAVSession_with_controller) {
  // FindOrCreateController creates a MediaSessionControllerExt whose
  // AsMediaSessionControllerExt() returns non-null.
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(manager_->OnEndAVSession(media_player_id_, true));
  ASSERT_NO_FATAL_FAILURE(manager_->OnEndAVSession(media_player_id_, false));
}

// OnEndAVSession: IsMediaSessionEnabled() is false → early return (L25-27).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       OnEndAVSession_session_disabled) {
  base::test::ScopedFeatureList fl;
  fl.InitWithFeatures({}, {media::kInternalMediaSession,
                           media_session::features::kMediaSessionService});
  auto mgr =
      std::make_unique<MediaSessionControllersManager>(web_contents_impl_);
  ASSERT_NO_FATAL_FAILURE(mgr->OnEndAVSession(media_player_id_, true));
}
#endif  // ARKWEB_MEDIA_AVSESSION

#if BUILDFLAG(ARKWEB_MEDIA_CAST)
// OnNotifyMeidaCastUri: controller non-null + ext non-null (L38-39).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       OnNotifyMeidaCastUri_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(
      manager_->OnNotifyMeidaCastUri(media_player_id_, "http://example.com"));
}

// OnNotifyMeidaCastUri: no controller for this id → controller is null
// → early return (L38).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       OnNotifyMeidaCastUri_no_controller) {
  ASSERT_NO_FATAL_FAILURE(
      manager_->OnNotifyMeidaCastUri(media_player_id_, "http://example.com"));
}

// CreateAVCastAdapter: controller non-null + ext non-null (L46-47).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       CreateAVCastAdapter_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(manager_->CreateAVCastAdapter(media_player_id_));
}

// CreateAVCastAdapter: no controller → early return (L46).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       CreateAVCastAdapter_no_controller) {
  ASSERT_NO_FATAL_FAILURE(manager_->CreateAVCastAdapter(media_player_id_));
}

// HandleStopMediaCast: controller non-null + ext non-null (L54-55).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       HandleStopMediaCast_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(manager_->HandleStopMediaCast(media_player_id_));
}

// HandleStopMediaCast: no controller → early return (L54).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       HandleStopMediaCast_no_controller) {
  ASSERT_NO_FATAL_FAILURE(manager_->HandleStopMediaCast(media_player_id_));
}

// UpdateRemotePlayState: controller non-null + ext non-null (L62-63).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       UpdateRemotePlayState_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(
      manager_->UpdateRemotePlayState(media_player_id_, true));
  ASSERT_NO_FATAL_FAILURE(
      manager_->UpdateRemotePlayState(media_player_id_, false));
}

// UpdateRemotePlayState: no controller → early return (L62).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       UpdateRemotePlayState_no_controller) {
  ASSERT_NO_FATAL_FAILURE(
      manager_->UpdateRemotePlayState(media_player_id_, true));
}

// UpdateRemotePlayPosition: controller non-null + ext non-null (L70-71).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       UpdateRemotePlayPosition_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(
      manager_->UpdateRemotePlayPosition(media_player_id_, 12345));
}

// UpdateRemotePlayPosition: no controller → early return (L70).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       UpdateRemotePlayPosition_no_controller) {
  ASSERT_NO_FATAL_FAILURE(
      manager_->UpdateRemotePlayPosition(media_player_id_, 12345));
}

// MediaCastStopByNavigation: IsMediaSessionEnabled() true + controller in map
// (L76-83).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       MediaCastStopByNavigation_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(manager_->MediaCastStopByNavigation());
}

// MediaCastStopByNavigation: IsMediaSessionEnabled() false → early return
// (L76-77).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       MediaCastStopByNavigation_session_disabled) {
  base::test::ScopedFeatureList fl;
  fl.InitWithFeatures({}, {media::kInternalMediaSession,
                           media_session::features::kMediaSessionService});
  auto mgr =
      std::make_unique<MediaSessionControllersManager>(web_contents_impl_);
  ASSERT_NO_FATAL_FAILURE(mgr->MediaCastStopByNavigation());
}

// MediaCastStopByNavigation: no controllers in map → loop body not executed.
TEST_F(MediaSessionControllersManagerForIncludeTest,
       MediaCastStopByNavigation_empty_map) {
  ASSERT_NO_FATAL_FAILURE(manager_->MediaCastStopByNavigation());
}

// SetPauseByAvcast: controller non-null + ext non-null (L88-89).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       SetPauseByAvcast_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(manager_->SetPauseByAvcast(media_player_id_, true));
  ASSERT_NO_FATAL_FAILURE(manager_->SetPauseByAvcast(media_player_id_, false));
}

// SetPauseByAvcast: no controller → early return (L88).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       SetPauseByAvcast_no_controller) {
  ASSERT_NO_FATAL_FAILURE(manager_->SetPauseByAvcast(media_player_id_, true));
}

// NotifyRemoteExitFullScreen: IsMediaSessionEnabled() true + controller in map
// (L95-101).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       NotifyRemoteExitFullScreen_with_controller) {
  manager_->OnMetadata(media_player_id_, true, false,
                       media::MediaContentType::kTransient);
  ASSERT_NO_FATAL_FAILURE(manager_->NotifyRemoteExitFullScreen());
}

// NotifyRemoteExitFullScreen: IsMediaSessionEnabled() false → early return
// (L95-96).
TEST_F(MediaSessionControllersManagerForIncludeTest,
       NotifyRemoteExitFullScreen_session_disabled) {
  base::test::ScopedFeatureList fl;
  fl.InitWithFeatures({}, {media::kInternalMediaSession,
                           media_session::features::kMediaSessionService});
  auto mgr =
      std::make_unique<MediaSessionControllersManager>(web_contents_impl_);
  ASSERT_NO_FATAL_FAILURE(mgr->NotifyRemoteExitFullScreen());
}

// NotifyRemoteExitFullScreen: no controllers in map → loop body not executed.
TEST_F(MediaSessionControllersManagerForIncludeTest,
       NotifyRemoteExitFullScreen_empty_map) {
  ASSERT_NO_FATAL_FAILURE(manager_->NotifyRemoteExitFullScreen());
}
#endif  // BUILDFLAG(ARKWEB_MEDIA_CAST)

}  // namespace content
