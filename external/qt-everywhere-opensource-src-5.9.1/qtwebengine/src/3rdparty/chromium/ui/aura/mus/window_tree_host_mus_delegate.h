// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_

#include "ui/aura/aura_export.h"

namespace gfx {
class Rect;
}

namespace aura {

class WindowPortMus;
class WindowTreeHostMus;

class AURA_EXPORT WindowTreeHostMusDelegate {
 public:
  // Called when the bounds of a WindowTreeHostMus is about to change.
  // |bounds| is the bounds supplied to WindowTreeHostMus::SetBounds() and is
  // in screen pixel coordinates.
  virtual void OnWindowTreeHostBoundsWillChange(
      WindowTreeHostMus* window_tree_host,
      const gfx::Rect& bounds) = 0;

  // Called when a WindowTreeHostMus is created without a WindowPort.
  virtual std::unique_ptr<WindowPortMus> CreateWindowPortForTopLevel() = 0;

  // Called from WindowTreeHostMus's constructor once the Window has been
  // created.
  virtual void OnWindowTreeHostCreated(WindowTreeHostMus* window_tree_host) = 0;

 protected:
  virtual ~WindowTreeHostMusDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_
