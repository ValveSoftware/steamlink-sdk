// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/proportional_image_view.h"

#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center_style.h"

namespace message_center {

ProportionalImageView::ProportionalImageView(const gfx::ImageSkia& image,
                                             const gfx::Size& max_size)
    : image_(image), max_size_(max_size) {}

ProportionalImageView::~ProportionalImageView() {}

gfx::Size ProportionalImageView::GetPreferredSize() const { return max_size_; }

int ProportionalImageView::GetHeightForWidth(int width) const {
  return max_size_.height();
}

void ProportionalImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size draw_size = GetImageDrawingSize();

  if (draw_size.IsEmpty())
    return;

  gfx::Rect draw_bounds = GetContentsBounds();
  draw_bounds.ClampToCenteredSize(draw_size);

  gfx::Size image_size(image_.size());

  if (image_size == draw_size) {
    canvas->DrawImageInt(image_, draw_bounds.x(), draw_bounds.y());
  } else {
    SkPaint paint;
    paint.setFilterLevel(SkPaint::kLow_FilterLevel);

    // This call resizes the image while drawing into the canvas.
    canvas->DrawImageInt(
        image_,
        0, 0, image_size.width(), image_size.height(),
        draw_bounds.x(), draw_bounds.y(), draw_size.width(), draw_size.height(),
        true,
        paint);
  }
}

gfx::Size ProportionalImageView::GetImageDrawingSize() {
  if (!visible())
    return gfx::Size();
  return message_center::GetImageSizeForContainerSize(
      GetContentsBounds().size(), image_.size());
}

}  // namespace message_center
