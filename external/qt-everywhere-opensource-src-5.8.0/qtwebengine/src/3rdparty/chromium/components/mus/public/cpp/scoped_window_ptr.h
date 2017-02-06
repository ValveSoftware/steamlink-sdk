// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_SCOPED_WINDOW_PTR_H_
#define COMPONENTS_MUS_PUBLIC_CPP_SCOPED_WINDOW_PTR_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"

namespace mus {

// Wraps a Window, taking overship of the Window. Also deals with Window being
// destroyed while ScopedWindowPtr still exists.
class ScopedWindowPtr : public WindowObserver {
 public:
  explicit ScopedWindowPtr(Window* window);
  ~ScopedWindowPtr() override;

  // Destroys |window|. If |window| is a root of the WindowManager than the
  // WindowManager is destroyed (which in turn destroys |window|).
  //
  // NOTE: this function (and class) are only useful for clients that only
  // ever have a single root.
  static void DeleteWindowOrWindowManager(Window* window);

  Window* window() { return window_; }
  const Window* window() const { return window_; }

 private:
  void DetachFromWindow();

  void OnWindowDestroying(Window* window) override;

  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowPtr);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_SCOPED_WINDOW_PTR_H_
