// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arkweb/chromium_ext/content/browser/media/fullscreen_video_overlay/fullscreen_overlay_media_player_controller.h"

#include "base/logging.h"
#include "content/public/browser/fullscreen_overlay_media_player_controller.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "gpu/ipc/common/nweb_native_window_tracker.h"
#include "arkweb/chromium_ext/content/browser/web_contents/web_contents_impl_ext.h"

namespace content {

FullscreenOverlayMediaPlayerController::FullscreenOverlayMediaPlayerController(
    base::WeakPtr<WebContents> web_contents,
    const MediaPlayerId& media_player_id)
    : web_contents_(std::move(web_contents)),
      media_player_id_(media_player_id) {}

FullscreenOverlayMediaPlayerController::~FullscreenOverlayMediaPlayerController() = default;

void FullscreenOverlayMediaPlayerController::Play() {
  if (!CheckAvailable()) {
    return;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)->RequestPlay();
}

void FullscreenOverlayMediaPlayerController::Pause() {
  if (!CheckAvailable()) {
    return;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)->RequestPause(true);
}

void FullscreenOverlayMediaPlayerController::Seek(double time) {
  if (!CheckAvailable()) {
    return;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)
      ->RequestSeekTo(base::Seconds(time));
}

void FullscreenOverlayMediaPlayerController::SetMuted(bool muted) {
  if (!CheckAvailable()) {
    return;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)->RequestMute(muted);
}

void FullscreenOverlayMediaPlayerController::SetPlaybackRate(
    double playback_rate) {
  if (!CheckAvailable()) {
    return;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)
      ->SetPlaybackRate(playback_rate);
}

void FullscreenOverlayMediaPlayerController::ExitFullscreen() {
  if (!CheckAvailable()) {
    return;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)->RequestExitFullscreen();
}

bool FullscreenOverlayMediaPlayerController::SetVideoSurface(
    void* native_window) {
  if (!CheckAvailable()) {
    return false;
  }
  if (!native_window) {
    return false;
  }
  int32_t surface_widget =
      NWebNativeWindowTracker::Get()->AddNativeWindow(native_window);
  auto* web_contents_ext = GetWebContentsImpl()->AsWebContentsImplExt();
  if (!web_contents_ext) {
    NWebNativeWindowTracker::Get()->DestroyNativeWindow(surface_widget);
    return false;
  }
  web_contents_ext->SetVideoSurface(media_player_id_, surface_widget);
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)
      ->SetVideoSurface(surface_widget);
  return true;
}

void FullscreenOverlayMediaPlayerController::Download() {
  LOG(WARNING) << "FullscreenOverlayMediaPlayerController::Download not supported";
}

void FullscreenOverlayMediaPlayerController::SetVolume(double volume) {
  if (!CheckAvailable()) {
    return;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)->SetVolume(volume);
}

double FullscreenOverlayMediaPlayerController::GetVolume() {
  double ret = -1.0;
  if (!CheckAvailable()) {
    return ret;
  }
  GetWebContentsImpl()->media_web_contents_observer()
      ->GetMediaPlayerRemote(media_player_id_)->GetVolume(&ret);
  return ret;
}

bool FullscreenOverlayMediaPlayerController::CheckAvailable() {
  auto* wci = GetWebContentsImpl();
  if (!wci) {
    return false;
  }
  if (!wci->media_web_contents_observer()) {
    return false;
  }
  return wci->media_web_contents_observer()
      ->IsMediaPlayerRemoteAvailable(media_player_id_);
}

WebContentsImpl* FullscreenOverlayMediaPlayerController::GetWebContentsImpl() {
  auto* wc = web_contents_.get();
  if (!wc) {
    return nullptr;
  }
  return static_cast<WebContentsImpl*>(wc);
}

std::unique_ptr<MediaPlayerController>
CreateFullscreenOverlayMediaPlayerController(
    WebContents* web_contents,
    const MediaPlayerId& media_player_id) {
  if (!web_contents) {
    return nullptr;
  }
  return std::make_unique<FullscreenOverlayMediaPlayerController>(
      web_contents->GetWeakPtr(), media_player_id);
}

}  // namespace content
