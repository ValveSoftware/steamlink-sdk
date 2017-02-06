// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/padded_button.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/painter.h"

namespace message_center {

PaddedButton::PaddedButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetFocusForPlatform();
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor,
      gfx::Insets(1, 2, 2, 2)));
}

PaddedButton::~PaddedButton() {
}

void PaddedButton::SetPadding(int horizontal_padding, int vertical_padding) {
  padding_.Set(std::max(vertical_padding, 0),
               std::max(horizontal_padding, 0),
               std::max(-vertical_padding, 0),
               std::max(-horizontal_padding, 0));
}

void PaddedButton::SetNormalImage(int resource_id) {
  SetImage(views::CustomButton::STATE_NORMAL,
           ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
               resource_id));
}

void PaddedButton::SetHoveredImage(int resource_id) {
  SetImage(views::CustomButton::STATE_HOVERED,
           ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
               resource_id));
}

void PaddedButton::SetPressedImage(int resource_id) {
  SetImage(views::CustomButton::STATE_PRESSED,
           ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
               resource_id));
}

gfx::Size PaddedButton::GetPreferredSize() const {
  return gfx::Size(message_center::kControlButtonSize,
                   message_center::kControlButtonSize);
}

void PaddedButton::OnPaint(gfx::Canvas* canvas) {
  // This is the same implementation as ImageButton::OnPaint except
  // that it calls ComputePaddedImagePaintPosition() instead of
  // ComputeImagePaintPosition(), in effect overriding that private method.
  View::OnPaint(canvas);
  gfx::ImageSkia image = GetImageToPaint();
  if (!image.isNull()) {
    gfx::Point position = ComputePaddedImagePaintPosition(image);
    if (!background_image_.isNull())
      canvas->DrawImageInt(background_image_, position.x(), position.y());
    canvas->DrawImageInt(image, position.x(), position.y());
  }
  views::Painter::PaintFocusPainter(this, canvas, focus_painter());
}

void PaddedButton::OnFocus() {
  views::ImageButton::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

gfx::Point PaddedButton::ComputePaddedImagePaintPosition(
    const gfx::ImageSkia& image) {
  gfx::Vector2d offset;
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(padding_);

  if (padding_.left() == 0 && padding_.right() == 0)
    offset.set_x((bounds.width() - image.width()) / 2);  // Center align.
  else if (padding_.right() > 0)
    offset.set_x(bounds.width() - image.width());  // Right align.

  if (padding_.top() == 0 && padding_.bottom() == 0)
    offset.set_y((bounds.height() - image.height()) / 2);  // Middle align.
  else if (padding_.bottom() > 0)
    offset.set_y(bounds.height() - image.height());  // Bottom align.

  return bounds.origin() + offset;
}

}  // namespace message_center
