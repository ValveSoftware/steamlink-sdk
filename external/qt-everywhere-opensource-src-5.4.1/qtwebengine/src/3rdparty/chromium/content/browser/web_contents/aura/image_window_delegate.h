// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_IMAGE_WINDOW_DELEGATE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_IMAGE_WINDOW_DELEGATE_H_

#include "content/common/content_export.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"

namespace content {

// An ImageWindowDelegate paints an image for a Window. The delegate destroys
// itself when the Window is destroyed. The delegate does not consume any event.
class CONTENT_EXPORT ImageWindowDelegate : public aura::WindowDelegate {
 public:
  ImageWindowDelegate();

  void SetImage(const gfx::Image& image);
  bool has_image() const { return !image_.IsEmpty(); }

 protected:
  virtual ~ImageWindowDelegate();

  // Overridden from aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual gfx::Size GetMaximumSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE;
  virtual bool CanFocus() OVERRIDE;
  virtual void OnCaptureLost() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE;
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE;

 protected:
  gfx::Image image_;
  gfx::Size window_size_;

  // Keeps track of whether the window size matches the image size or not. If
  // the image size is smaller than the window size, then the delegate paints a
  // white background for the missing regions.
  bool size_mismatch_;

  DISALLOW_COPY_AND_ASSIGN(ImageWindowDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_IMAGE_WINDOW_DELEGATE_H_
