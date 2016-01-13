// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_VISIBILITY_CONTROLLER_H_
#define UI_WM_CORE_VISIBILITY_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/aura/client/visibility_client.h"
#include "ui/wm/wm_export.h"

namespace wm {

class WM_EXPORT VisibilityController
    : public aura::client::VisibilityClient {
 public:
  VisibilityController();
  virtual ~VisibilityController();

 protected:
  // Subclasses override if they want to call a different implementation of
  // this function.
  // TODO(beng): potentially replace by an actual window animator class in
  //             window_animations.h.
  virtual bool CallAnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                         bool visible);

 private:
  // Overridden from aura::client::VisibilityClient:
  virtual void UpdateLayerVisibility(aura::Window* window,
                                     bool visible) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(VisibilityController);
};

// Suspends the animations for visibility changes during the lifetime of an
// instance of this class.
//
// Example:
//
// void ViewName::UnanimatedAction() {
//   SuspendChildWindowVisibilityAnimations suspend(parent);
//   // Perform unanimated action here.
//   // ...
//   // When the method finishes, visibility animations will return to their
//   // previous state.
// }
//
class WM_EXPORT SuspendChildWindowVisibilityAnimations {
 public:
  // Suspend visibility animations of child windows.
  explicit SuspendChildWindowVisibilityAnimations(aura::Window* window);

  // Restore visibility animations to their original state.
  ~SuspendChildWindowVisibilityAnimations();

 private:
  // The window to manage.
  aura::Window* window_;

  // Whether the visibility animations on child windows were originally enabled.
  const bool original_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SuspendChildWindowVisibilityAnimations);
};

// Tells |window| to animate visibility changes to its children.
void WM_EXPORT SetChildWindowVisibilityChangesAnimated(
    aura::Window* window);

}  // namespace wm

#endif  // UI_WM_CORE_VISIBILITY_CONTROLLER_H_
