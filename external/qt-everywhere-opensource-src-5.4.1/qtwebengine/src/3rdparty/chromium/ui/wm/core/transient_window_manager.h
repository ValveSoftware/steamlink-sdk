// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_TRANSIENT_WINDOW_MANAGER_H_
#define UI_WM_CORE_TRANSIENT_WINDOW_MANAGER_H_

#include <vector>

#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/wm_export.h"

namespace wm {

class TransientWindowObserver;

// TransientWindowManager manages the set of transient children for a window
// along with the transient parent. Transient children get the following
// behavior:
// . The transient parent destroys any transient children when it is
//   destroyed. This means a transient child is destroyed if either its parent
//   or transient parent is destroyed.
// . If a transient child and its transient parent share the same parent, then
//   transient children are always ordered above the transient parent.
// Transient windows are typically used for popups and menus.
// TODO(sky): when we nuke TransientWindowClient rename this to
// TransientWindowController.
class WM_EXPORT TransientWindowManager : public aura::WindowObserver {
 public:
  typedef std::vector<aura::Window*> Windows;

  virtual ~TransientWindowManager();

  // Returns the TransientWindowManager for |window|. This never returns NULL.
  static TransientWindowManager* Get(aura::Window* window);

  // Returns the TransientWindowManager for |window| only if it already exists.
  // WARNING: this may return NULL.
  static const TransientWindowManager* Get(const aura::Window* window);

  void AddObserver(TransientWindowObserver* observer);
  void RemoveObserver(TransientWindowObserver* observer);

  // Adds or removes a transient child.
  void AddTransientChild(aura::Window* child);
  void RemoveTransientChild(aura::Window* child);

  const Windows& transient_children() const { return transient_children_; }

  aura::Window* transient_parent() { return transient_parent_; }
  const aura::Window* transient_parent() const { return transient_parent_; }

  // Returns true if in the process of stacking |window_| on top of |target|.
  // That is, when the stacking order of a window changes
  // (OnWindowStackingChanged()) the transients may get restacked as well. This
  // function can be used to detect if TransientWindowManager is in the process
  // of stacking a transient as the result of window stacking changing.
  bool IsStackingTransient(const aura::Window* target) const;

 private:
  explicit TransientWindowManager(aura::Window* window);

  // Stacks transient descendants of this window that are its siblings just
  // above it.
  void RestackTransientDescendants();

  // WindowObserver:
  virtual void OnWindowParentChanged(aura::Window* window,
                                     aura::Window* parent) OVERRIDE;
  virtual void OnWindowVisibilityChanging(aura::Window* window,
                                          bool visible) OVERRIDE;
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  aura::Window* window_;
  aura::Window* transient_parent_;
  Windows transient_children_;

  // If non-null we're actively restacking transient as the result of a
  // transient ancestor changing.
  aura::Window* stacking_target_;

  ObserverList<TransientWindowObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowManager);
};

}  // namespace wm

#endif  // UI_WM_CORE_TRANSIENT_WINDOW_MANAGER_H_
