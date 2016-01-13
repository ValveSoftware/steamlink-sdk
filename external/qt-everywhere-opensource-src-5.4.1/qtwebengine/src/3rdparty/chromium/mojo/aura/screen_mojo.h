// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_AURA_SCREEN_MOJO_H_
#define MOJO_AURA_SCREEN_MOJO_H_

#include "base/compiler_specific.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace gfx {
class Rect;
class Transform;
}

namespace mojo {

// A minimal implementation of gfx::Screen for mojo.
class ScreenMojo : public gfx::Screen {
 public:
  static ScreenMojo* Create();
  virtual ~ScreenMojo();

 protected:
  // gfx::Screen overrides:
  virtual bool IsDIPEnabled() OVERRIDE;
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      OVERRIDE;
  virtual int GetNumDisplays() const OVERRIDE;
  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const OVERRIDE;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE;
  virtual void AddObserver(gfx::DisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(gfx::DisplayObserver* observer) OVERRIDE;

 private:
  explicit ScreenMojo(const gfx::Rect& screen_bounds);

  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMojo);
};

}  // namespace mojo

#endif  // MOJO_AURA_SCREEN_MOJO_H_
