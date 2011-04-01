// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/take_photo_view.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class SkBitmap;

namespace views {
class NativeButton;
}  // namespace views

namespace chromeos {

// View used for selecting user image on OOBE screen.
class UserImageView : public views::View,
                      public views::ButtonListener,
                      public TakePhotoView::Delegate {
 public:
  // Delegate class to get notifications from the view.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // Called if user accepts the selected image. The image is passed as a
    // parameter.
    virtual void OnOK(const SkBitmap& image) = 0;

    // Called if user decides to skip image selection screen.
    virtual void OnSkip() = 0;
  };

  explicit UserImageView(Delegate* delegate);
  virtual ~UserImageView();

  // Initializes this view, its children and layout.
  void Init();

  // Updates image from camera.
  void UpdateVideoFrame(const SkBitmap& frame);

  // If in capturing mode, shows that camera is initializing by running
  // throbber above the picture. Disables snapshot button until frame is
  // received.
  void ShowCameraInitializing();

  // If in capturing mode, shows that camera is broken instead of video
  // frame and disables snapshot button until new frame is received.
  void ShowCameraError();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from TakePhotoView::Delegate.
  virtual void OnCapturingStarted();
  virtual void OnCapturingStopped();

 private:
  // Initializes layout manager for this view.
  void InitLayout();

  TakePhotoView* take_photo_view_;
  views::NativeButton* ok_button_;
  views::NativeButton* skip_button_;

  // Notifications receiver.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(UserImageView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_VIEW_H_
