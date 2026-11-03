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

#include "third_party/blink/renderer/core/html/media/media_remoting_interstitial.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/platform/media/media_player_client.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {

class MediaRemotingInterstitialTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    SetHtmlInnerHTML("<body><video id='testVideo'></video></body>");
    video_element_ = To<HTMLVideoElement>(GetDocument().getElementById(AtomicString("testVideo")));
    ShadowRoot& shadow_root = video_element_->EnsureUserAgentShadowRoot();
    interstitial_ = MakeGarbageCollected<MediaRemotingInterstitial>(*video_element_);
    shadow_root.appendChild(interstitial_);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  }

  void TearDown() override {
    video_element_ = nullptr;
    interstitial_ = nullptr;
    PageTestBase::TearDown();
  }

  // Helper method to find child elements by their shadow pseudo ID
  Element* FindChildByShadowPseudoId(const AtomicString& pseudo_id) {
    if (ShadowRoot* shadow_root = interstitial_->GetShadowRoot()) {
      for (Element* child = ElementTraversal::FirstChild(*shadow_root);
           child; child = ElementTraversal::NextSibling(*child)) {
        if (child->ShadowPseudoId() == pseudo_id) {
          return child;
        }
      }
    }
    for (Element* child = ElementTraversal::FirstChild(*interstitial_);
         child; child = ElementTraversal::NextSibling(*child)) {
      if (child->ShadowPseudoId() == pseudo_id) {
        return child;
      }
    }

    return nullptr;
  }

  HTMLVideoElement* video_element_;
  Persistent<MediaRemotingInterstitial> interstitial_;
};

TEST_F(MediaRemotingInterstitialTest, ConstructorInitializesElements) {
  ASSERT_NE(interstitial_, nullptr);
  EXPECT_EQ(interstitial_->tagName(), AtomicString("DIV"));
  EXPECT_TRUE(interstitial_->hasChildren());
  bool found_background = false;
  for (Element* child = ElementTraversal::FirstChild(*interstitial_);
       child; child = ElementTraversal::NextSibling(*child)) {
    if (child->tagName() == AtomicString("IMG")) {
      found_background = true;
      break;
    }
  }
  EXPECT_TRUE(found_background);
}

TEST_F(MediaRemotingInterstitialTest, ShowWithEmptyDeviceName) {
  EXPECT_FALSE(interstitial_->IsVisible());
  interstitial_->Show(WebString());
  EXPECT_TRUE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, ShowWithDeviceName) {
  const WebString device_name = WebString::FromUTF8("Living Room TV");
  interstitial_->Show(device_name);
  EXPECT_TRUE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, ShowWhenAlreadyVisible) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->Show(WebString::FromUTF8("Different Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, HideWithNoTextError) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->Hide(0);
  EXPECT_FALSE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, HideWithErrorCode) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  const int error_code = 1;
  interstitial_->Hide(error_code);
  EXPECT_FALSE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, HideWhenNotVisible) {
  EXPECT_FALSE(interstitial_->IsVisible());
  interstitial_->Hide(0);
  EXPECT_FALSE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, GetVideoElement) {
  EXPECT_EQ(&interstitial_->GetVideoElement(), video_element_);
}

TEST_F(MediaRemotingInterstitialTest, DocumentMovement) {
  GetDocument().body()->appendChild(interstitial_);
  interstitial_->Show(WebString::FromUTF8("Test Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->Hide(0);
  EXPECT_FALSE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, VisibilityStates) {
  EXPECT_FALSE(interstitial_->IsVisible());
  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->Hide(1);
  EXPECT_FALSE(interstitial_->IsVisible());
}

#if BUILDFLAG(ARKWEB_MEDIA_CAST)

TEST_F(MediaRemotingInterstitialTest, UpdatePlayStateWhenPlaying) {
  interstitial_->UpdateUiPlayState(true);
  EXPECT_TRUE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, UpdatePlayStateWhenPaused) {
  interstitial_->UpdateUiPlayState(false);
  EXPECT_FALSE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, UpdatePlayPosition) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  const int64_t position = 5000;
  interstitial_->UpdateUiPlayPosition(position);
}

TEST_F(MediaRemotingInterstitialTest, OnPlayPauseClickTogglesState) {
  bool initial_state = interstitial_->GetPlayingState();
  interstitial_->OnPlayPauseClicked();
  EXPECT_NE(interstitial_->GetPlayingState(), initial_state);
}

TEST_F(MediaRemotingInterstitialTest, OnStopCastingClicked) {
  interstitial_->Show(WebString::FromUTF8("Test Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->OnStopCastingClicked();
}

TEST_F(MediaRemotingInterstitialTest, OnSwitchDeviceClicked) {
  interstitial_->Show(WebString::FromUTF8("Test Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->OnSwitchDeviceClicked();
}

TEST_F(MediaRemotingInterstitialTest, ProgressBarClickedWithZeroDuration) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  interstitial_->OnProgressBarClicked(50.0);
}

TEST_F(MediaRemotingInterstitialTest, ProgressBarClickedWithValidDuration) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  interstitial_->OnProgressBarClicked(25.0);
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragStartWhenPlaying) {
  interstitial_->UpdateUiPlayState(true);
  EXPECT_TRUE(interstitial_->GetPlayingState());
  interstitial_->OnProgressDragStart();
  EXPECT_FALSE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragStartWhenPaused) {
  interstitial_->UpdateUiPlayState(false);
  EXPECT_FALSE(interstitial_->GetPlayingState());
  interstitial_->OnProgressDragStart();
  EXPECT_FALSE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, ProgressDraggingWhileDragging) {
  interstitial_->OnProgressDragStart();
  interstitial_->OnProgressDragging(30.0);
  interstitial_->OnProgressDragging(50.0);
  interstitial_->OnProgressDragging(75.0);
}

TEST_F(MediaRemotingInterstitialTest, ProgressDraggingWhenNotDragging) {
  interstitial_->OnProgressDragging(50.0);
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragEndAfterDragging) {
  interstitial_->UpdateUiPlayState(true);
  interstitial_->OnProgressDragStart();
  interstitial_->OnProgressDragging(40.0);
  interstitial_->OnProgressDragEnd(40.0);
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragEndWhenNotDragging) {
  interstitial_->OnProgressDragEnd(50.0);
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragCancelWhenDragging) {
  interstitial_->UpdateUiPlayState(true);
  interstitial_->OnProgressDragStart();
  interstitial_->OnProgressDragging(35.0);
  interstitial_->OnProgressDragCancel();
  EXPECT_TRUE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragCancelWhenNotDragging) {
  interstitial_->OnProgressDragCancel();
}

TEST_F(MediaRemotingInterstitialTest, NotifyRemoteInterstitialWithDifferentSizing) {
  interstitial_->NotifyRemoteInterstitial(MediaControlsSizingClass::kSmall);
  interstitial_->NotifyRemoteInterstitial(MediaControlsSizingClass::kMedium);
  interstitial_->NotifyRemoteInterstitial(MediaControlsSizingClass::kLarge);
}

TEST_F(MediaRemotingInterstitialTest, HideWithMediaRemotingStopNoText) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->Hide(MediaPlayerClient::kMediaRemotingStopNoText);
  EXPECT_FALSE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, HideWithSpecificErrorCode) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
  interstitial_->Hide(IDS_MEDIA_REMOTING_STOP_BY_ERROR_TEXT);
}

TEST_F(MediaRemotingInterstitialTest, MultipleShowHideWithDifferentStates) {
  EXPECT_FALSE(interstitial_->IsVisible());

  interstitial_->Show(WebString::FromUTF8("Device1"));
  EXPECT_TRUE(interstitial_->IsVisible());

  interstitial_->Hide(IDS_MEDIA_REMOTING_STOP_BY_ERROR_TEXT);

  interstitial_->Show(WebString::FromUTF8("Device2"));
  EXPECT_TRUE(interstitial_->IsVisible());

  interstitial_->Hide(MediaPlayerClient::kMediaRemotingStopNoText);
  EXPECT_FALSE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, ShowWhileTimerIsActive) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  interstitial_->Show(WebString::FromUTF8("Different Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, GetMediaRemotingProgressBar) {
  auto& progress_bar = interstitial_->GetMediaRemotingProgressBar();
}

TEST_F(MediaRemotingInterstitialTest, UpdateRemotePlayState) {
  interstitial_->UpdateRemotePlayState(true);
  interstitial_->UpdateRemotePlayState(false);
}

TEST_F(MediaRemotingInterstitialTest, UpdateRemotePlayPosition) {
  const int64_t position = 10000;
  interstitial_->UpdateRemotePlayPosition(position);
}

TEST_F(MediaRemotingInterstitialTest, OnPosterImageChanged) {
  video_element_->setAttribute(html_names::kPosterAttr,
                               AtomicString("http://example.com/poster.jpg"));
  interstitial_->OnPosterImageChanged();
}

TEST_F(MediaRemotingInterstitialTest, OnProgressDraggingWhenNotDragging) {
  EXPECT_NO_FATAL_FAILURE(interstitial_->OnProgressDragging(50.0));
}

TEST_F(MediaRemotingInterstitialTest, OnProgressDraggingAtBoundaries) {
  interstitial_->OnProgressDragStart();
  EXPECT_NO_FATAL_FAILURE(interstitial_->OnProgressDragging(0.0));
  EXPECT_NO_FATAL_FAILURE(interstitial_->OnProgressDragging(100.0));
  EXPECT_NO_FATAL_FAILURE(interstitial_->OnProgressDragging(-10.0));
  EXPECT_NO_FATAL_FAILURE(interstitial_->OnProgressDragging(110.0));
  interstitial_->OnProgressDragEnd(50.0);
}

TEST_F(MediaRemotingInterstitialTest, OnProgressDragEndWhenNotDragging) {
  EXPECT_NO_FATAL_FAILURE(interstitial_->OnProgressDragEnd(50.0));
}

TEST_F(MediaRemotingInterstitialTest, UpdateProgressUIWithNullElements) {
  interstitial_->UpdateUiPlayPosition(5000);
}

TEST_F(MediaRemotingInterstitialTest, UpdatePlayButtonUIWithNullButton) {
  interstitial_->OnPlayPauseClicked();
}

TEST_F(MediaRemotingInterstitialTest, NotifyRemoteInterstitialWithNullElements) {
  EXPECT_NO_FATAL_FAILURE(interstitial_->NotifyRemoteInterstitial(MediaControlsSizingClass::kSmall));
  EXPECT_NO_FATAL_FAILURE(interstitial_->NotifyRemoteInterstitial(MediaControlsSizingClass::kMedium));
  EXPECT_NO_FATAL_FAILURE(interstitial_->NotifyRemoteInterstitial(MediaControlsSizingClass::kLarge));
}

TEST_F(MediaRemotingInterstitialTest, OnProgressDragCancelWhenNotDragging2) {
  EXPECT_NO_FATAL_FAILURE(interstitial_->OnProgressDragCancel());
}

TEST_F(MediaRemotingInterstitialTest, RapidStateTransitions) {
  interstitial_->UpdateUiPlayState(true);
  EXPECT_TRUE(interstitial_->GetPlayingState());

  interstitial_->UpdateUiPlayState(false);
  EXPECT_FALSE(interstitial_->GetPlayingState());

  interstitial_->UpdateUiPlayState(true);
  EXPECT_TRUE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragWithResume) {
  interstitial_->UpdateUiPlayState(true);
  EXPECT_TRUE(interstitial_->GetPlayingState());

  interstitial_->OnProgressDragStart();
  EXPECT_FALSE(interstitial_->GetPlayingState());

  interstitial_->OnProgressDragging(50.0);
  interstitial_->OnProgressDragEnd(50.0);
  EXPECT_TRUE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, ProgressDragWithoutResume) {
  interstitial_->UpdateUiPlayState(false);
  EXPECT_FALSE(interstitial_->GetPlayingState());

  interstitial_->OnProgressDragStart();
  EXPECT_FALSE(interstitial_->GetPlayingState());

  interstitial_->OnProgressDragging(30.0);
  interstitial_->OnProgressDragEnd(30.0);
  EXPECT_FALSE(interstitial_->GetPlayingState());
}

TEST_F(MediaRemotingInterstitialTest, MultipleProgressOperations) {
  interstitial_->UpdateUiPlayPosition(10000);
  interstitial_->OnProgressBarClicked(25.0);
  interstitial_->UpdateUiPlayPosition(15000);
  interstitial_->OnProgressBarClicked(75.0);
}

TEST_F(MediaRemotingInterstitialTest, PlayPauseToggleSequence) {
  interstitial_->OnPlayPauseClicked();
  interstitial_->OnPlayPauseClicked();
  interstitial_->OnPlayPauseClicked();
}

TEST_F(MediaRemotingInterstitialTest, RemoteStateAndPositionSync) {
  interstitial_->UpdateRemotePlayState(true);
  interstitial_->UpdateRemotePlayPosition(10000);
  interstitial_->UpdateRemotePlayState(false);
  interstitial_->UpdateRemotePlayPosition(20000);
}

TEST_F(MediaRemotingInterstitialTest, EmptyAndNonEmptyDeviceNames) {
  interstitial_->Show(WebString());
  EXPECT_TRUE(interstitial_->IsVisible());

  interstitial_->Hide(0);
  EXPECT_FALSE(interstitial_->IsVisible());

  interstitial_->Show(WebString::FromUTF8("Test Device"));
  EXPECT_TRUE(interstitial_->IsVisible());
}

TEST_F(MediaRemotingInterstitialTest, HideWithVariousErrorCodes) {
  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());

  interstitial_->Hide(MediaPlayerClient::kMediaRemotingStopNoText);
  EXPECT_FALSE(interstitial_->IsVisible());

  interstitial_->Show(WebString::FromUTF8("Device"));
  EXPECT_TRUE(interstitial_->IsVisible());

  interstitial_->Hide(IDS_MEDIA_REMOTING_STOP_BY_ERROR_TEXT);
}

#endif  // BUILDFLAG(ARKWEB_MEDIA_CAST)

}  // namespace blink
