// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/image_window_delegate.h"

#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace content {

ImageWindowDelegate::ImageWindowDelegate()
    : size_mismatch_(false) {
}

ImageWindowDelegate::~ImageWindowDelegate() {
}

void ImageWindowDelegate::SetImage(const gfx::Image& image) {
  image_ = image;
  if (!window_size_.IsEmpty() && !image_.IsEmpty())
    size_mismatch_ = window_size_ != image_.AsImageSkia().size();
}

gfx::Size ImageWindowDelegate::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size ImageWindowDelegate::GetMaximumSize() const {
  return gfx::Size();
}

void ImageWindowDelegate::OnBoundsChanged(const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  window_size_ = new_bounds.size();
  if (!image_.IsEmpty())
    size_mismatch_ = window_size_ != image_.AsImageSkia().size();
}

gfx::NativeCursor ImageWindowDelegate::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int ImageWindowDelegate::GetNonClientComponent(const gfx::Point& point) const {
  return HTNOWHERE;
}

bool ImageWindowDelegate::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return false;
}

bool ImageWindowDelegate::CanFocus() {
  return false;
}

void ImageWindowDelegate::OnCaptureLost() {
}

void ImageWindowDelegate::OnPaint(gfx::Canvas* canvas) {
  if (image_.IsEmpty()) {
    canvas->DrawColor(SK_ColorGRAY);
  } else {
    if (size_mismatch_)
      canvas->DrawColor(SK_ColorWHITE);
    canvas->DrawImageInt(image_.AsImageSkia(), 0, 0);
  }
}

void ImageWindowDelegate::OnDeviceScaleFactorChanged(float scale_factor) {
}

void ImageWindowDelegate::OnWindowDestroying(aura::Window* window) {
}

void ImageWindowDelegate::OnWindowDestroyed(aura::Window* window) {
  delete this;
}

void ImageWindowDelegate::OnWindowTargetVisibilityChanged(bool visible) {
}

bool ImageWindowDelegate::HasHitTestMask() const {
  return false;
}

void ImageWindowDelegate::GetHitTestMask(gfx::Path* mask) const {
}

}  // namespace content
