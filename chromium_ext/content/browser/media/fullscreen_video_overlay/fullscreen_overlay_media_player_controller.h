// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_FULLSCREEN_OVERLAY_MEDIA_PLAYER_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_FULLSCREEN_OVERLAY_MEDIA_PLAYER_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/media_player_controller.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents.h"

namespace content {

class WebContentsImpl;

class FullscreenOverlayMediaPlayerController
    : public MediaPlayerController {
 public:
  FullscreenOverlayMediaPlayerController(
      base::WeakPtr<WebContents> web_contents,
      const MediaPlayerId& media_player_id);
  ~FullscreenOverlayMediaPlayerController() override;

  void Play() override;
  void Pause() override;
  void Seek(double time) override;
  void SetMuted(bool muted) override;
  void SetPlaybackRate(double playback_rate) override;
  void ExitFullscreen() override;
  bool SetVideoSurface(void* native_window) override;
  void Download() override;
  void SetVolume(double volume) override;
  double GetVolume() override;

 private:
  bool CheckAvailable();
  WebContentsImpl* GetWebContentsImpl();

  base::WeakPtr<WebContents> web_contents_;
  const MediaPlayerId media_player_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_FULLSCREEN_OVERLAY_MEDIA_PLAYER_CONTROLLER_H_
