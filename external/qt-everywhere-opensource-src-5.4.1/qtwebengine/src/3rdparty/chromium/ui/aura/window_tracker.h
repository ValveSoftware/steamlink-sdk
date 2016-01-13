// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TRACKER_H_
#define UI_AURA_WINDOW_TRACKER_H_

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_observer.h"

namespace aura {

// This class keeps track of a set of windows. A Window is removed either
// explicitly by Remove(), or implicitly when the window is destroyed.
class AURA_EXPORT WindowTracker : public WindowObserver {
 public:
  typedef std::set<Window*> Windows;

  WindowTracker();
  virtual ~WindowTracker();

  // Returns the set of windows being observed.
  const std::set<Window*>& windows() const { return windows_; }

  // Adds |window| to the set of Windows being tracked.
  void Add(Window* window);

  // Removes |window| from the set of windows being tracked.
  void Remove(Window* window);

  // Returns true if |window| was previously added and has not been removed or
  // deleted.
  bool Contains(Window* window);

  // WindowObserver overrides:
  virtual void OnWindowDestroying(Window* window) OVERRIDE;

 private:
  Windows windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowTracker);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TRACKER_H_
