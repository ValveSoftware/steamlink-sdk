// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_ACTIVATION_CHANGE_OBSERVER_H_
#define UI_WM_PUBLIC_ACTIVATION_CHANGE_OBSERVER_H_

#include "ui/aura/aura_export.h"

namespace aura {
class Window;

namespace client {

class AURA_EXPORT ActivationChangeObserver {
 public:
  // Called when |active| gains focus, or there is no active window
  // (|active| is NULL in this case.) |old_active| refers to the
  // previous active window or NULL if there was no previously active
  // window.
  virtual void OnWindowActivated(Window* gained_active,
                                 Window* lost_active) = 0;

  // Called when during window activation the currently active window is
  // selected for activation. This can happen when a window requested for
  // activation cannot be activated because a system modal window is active.
  virtual void OnAttemptToReactivateWindow(aura::Window* request_active,
                                           aura::Window* actual_active) {}

 protected:
  virtual ~ActivationChangeObserver() {}
};

// Gets/Sets the ActivationChangeObserver for a specific window. This observer
// is notified after the ActivationClient notifies all registered observers.
AURA_EXPORT void SetActivationChangeObserver(
    Window* window,
    ActivationChangeObserver* observer);
AURA_EXPORT ActivationChangeObserver* GetActivationChangeObserver(
    Window* window);

}  // namespace client
}  // namespace aura

#endif  // UI_WM_PUBLIC_ACTIVATION_CHANGE_OBSERVER_H_
