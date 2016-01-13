// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_PADDED_BUTTON_H_
#define UI_MESSAGE_CENTER_VIEWS_PADDED_BUTTON_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/image_button.h"

namespace message_center {

// PaddedButtons are ImageButtons whose image can be padded within the button.
// This allows the creation of buttons like the notification close and expand
// buttons whose clickable areas extends beyond their image areas
// (<http://crbug.com/168822>) without the need to create and maintain
// corresponding resource images with alpha padding. In the future, this class
// will also allow for buttons whose touch areas extend beyond their clickable
// area (<http://crbug.com/168856>).
class PaddedButton : public views::ImageButton {
 public:
  PaddedButton(views::ButtonListener* listener);
  virtual ~PaddedButton();

  // Overridden from views::ImageButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  // The SetPadding() method also sets the button's image alignment (positive
  // values yield left/top alignments, negative values yield right/bottom ones,
  // and zero values center/middle ones). ImageButton::SetImageAlignment() calls
  // will not affect PaddedButton image alignments.
  void SetPadding(int horizontal_padding, int vertical_padding);

  void SetNormalImage(int resource_id);
  void SetHoveredImage(int resource_id);
  void SetPressedImage(int resource_id);

 protected:
  gfx::Point ComputePaddedImagePaintPosition(const gfx::ImageSkia& image);

 private:
  gfx::Insets padding_;

  DISALLOW_COPY_AND_ASSIGN(PaddedButton);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_PADDED_BUTTON_H_
