// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FULLSCREEN_OVERLAY_MEDIA_PLAYER_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_FULLSCREEN_OVERLAY_MEDIA_PLAYER_CONTROLLER_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"

namespace content {

class MediaPlayerController;
class WebContents;

// Creates a MediaPlayerController for the fullscreen video overlay path.
// Returns nullptr if |web_contents| is null.
// This controller uses Mojo IPC to control media playback and holds a weak
// reference to |web_contents| for safe lifetime management.
CONTENT_EXPORT std::unique_ptr<MediaPlayerController>
CreateFullscreenOverlayMediaPlayerController(
    WebContents* web_contents,
    const MediaPlayerId& media_player_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FULLSCREEN_OVERLAY_MEDIA_PLAYER_CONTROLLER_H_
