// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCOPED_WINDOW_TARGETER_H_
#define UI_AURA_SCOPED_WINDOW_TARGETER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"

namespace ui {
class EventTargeter;
}

namespace aura {

class Window;

// ScopedWindowTargeter is used to temporarily replace the event-targeter for a
// window. Upon construction, it installs a new targeter on the window, and upon
// destruction, it restores the previous event-targeter on the window.
class AURA_EXPORT ScopedWindowTargeter : public WindowObserver {
 public:
  ScopedWindowTargeter(Window* window,
                       scoped_ptr<ui::EventTargeter> new_targeter);

  virtual ~ScopedWindowTargeter();

  ui::EventTargeter* old_targeter() {
    return old_targeter_.get();
  }

 private:
  // WindowObserver:
  virtual void OnWindowDestroyed(Window* window) OVERRIDE;

  Window* window_;
  scoped_ptr<ui::EventTargeter> old_targeter_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowTargeter);
};

}  // namespace aura

#endif  // UI_AURA_SCOPED_WINDOW_TARGETER_H_
