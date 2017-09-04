// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_SCOPED_DRAG_DROP_DISABLER_H_
#define UI_WM_PUBLIC_SCOPED_DRAG_DROP_DISABLER_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;

namespace client {
class DragDropClient;

// ScopedDragDropDisabler is used to temporarily replace the drag'n'drop client
// for a window with a "no-op" client. Upon construction, it installs a new
// client on the window, and upon destruction, it restores the previous one.
class AURA_EXPORT ScopedDragDropDisabler : public WindowObserver {
 public:
  explicit ScopedDragDropDisabler(Window* window);
  ~ScopedDragDropDisabler() override;

 private:
  // WindowObserver:
  void OnWindowDestroyed(Window* window) override;

  Window* window_;
  DragDropClient* old_client_;
  std::unique_ptr<DragDropClient> new_client_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDragDropDisabler);
};

}  // namespace client
}  // namespace aura

#endif  // UI_WM_PUBLIC_SCOPED_DRAG_DROP_DISABLER_H_
