// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_PORT_H_
#define UI_AURA_WINDOW_PORT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/aura/aura_export.h"

namespace gfx {
class Rect;
}

namespace aura {

class Window;
class WindowObserver;

// See comments in OnWillChangeProperty() for details.
struct AURA_EXPORT WindowPortPropertyData {
  virtual ~WindowPortPropertyData() {}
};

// WindowPort defines an interface to enable Window to be used with or without
// mus. WindowPort is owned by Window and called at key points in Windows
// lifetime that enable Window to be used in both environments.
//
// If a Window is created without an explicit WindowPort then
// Env::CreateWindowPort() is used to create the WindowPort.
class AURA_EXPORT WindowPort {
 public:
  virtual ~WindowPort() {}

  // Called from Window::Init().
  virtual void OnPreInit(Window* window) = 0;

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) = 0;

  // Called when a window is being added as a child. |child| may already have
  // a parent, but its parent is not the Window this WindowPort is associated
  // with.
  virtual void OnWillAddChild(Window* child) = 0;

  virtual void OnWillRemoveChild(Window* child) = 0;

  // Called to move the child at |current_index| to |dest_index|. |dest_index|
  // is calculated assuming the window at |current_index| has been removed, e.g.
  //   Window* child = children_[current_index];
  //   children_.erase(children_.begin() + current_index);
  //   children_.insert(children_.begin() + dest_index, child);
  virtual void OnWillMoveChild(size_t current_index, size_t dest_index) = 0;

  virtual void OnVisibilityChanged(bool visible) = 0;

  virtual void OnDidChangeBounds(const gfx::Rect& old_bounds,
                                 const gfx::Rect& new_bounds) = 0;

  // Called before a property is changed. The return value from this is supplied
  // into OnPropertyChanged() so that WindowPort may pass data between the two
  // calls.
  virtual std::unique_ptr<WindowPortPropertyData> OnWillChangeProperty(
      const void* key) = 0;

  // Called after a property changes, but before observers are notified. |data|
  // is the return value from OnWillChangeProperty().
  virtual void OnPropertyChanged(
      const void* key,
      std::unique_ptr<WindowPortPropertyData> data) = 0;

 protected:
  // Returns the WindowPort associated with a Window.
  static WindowPort* Get(Window* window);

  // Returns the ObserverList of a Window.
  static base::ObserverList<WindowObserver, true>* GetObservers(Window* window);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_PORT_H_
