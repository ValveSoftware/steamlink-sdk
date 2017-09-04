// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_OBSERVER_H_
#define UI_AURA_ENV_OBSERVER_H_

#include "ui/aura/aura_export.h"

namespace aura {

class Window;
class WindowTreeHost;

namespace client {
class FocusClient;
}

class AURA_EXPORT EnvObserver {
 public:
  // Called when |window| has been initialized.
  virtual void OnWindowInitialized(Window* window) = 0;

 // Called when a WindowTreeHost is initialized.
  virtual void OnHostInitialized(WindowTreeHost* host) {}

  // Called when a WindowTreeHost is activated.
  virtual void OnHostActivated(WindowTreeHost* host) {}

  // Called right before Env is destroyed.
  virtual void OnWillDestroyEnv() {}

  // Called from Env::SetActiveFocusClient(), see it for details.
  virtual void OnActiveFocusClientChanged(client::FocusClient* focus_client,
                                          Window* window) {}

 protected:
  virtual ~EnvObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_ENV_OBSERVER_H_
