// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_FOCUS_SYNCHRONIZER_H_
#define UI_AURA_MUS_FOCUS_SYNCHRONIZER_H_

#include "base/macros.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env_observer.h"

namespace ui {
namespace mojom {
class WindowTree;
}
}

namespace aura {

class FocusSynchronizerDelegate;
class WindowMus;

namespace client {
class FocusClient;
}

// FocusSynchronizer is resonsible for keeping focus in sync between aura
// and the mus server.
class FocusSynchronizer : public client::FocusChangeObserver,
                          public EnvObserver {
 public:
  FocusSynchronizer(FocusSynchronizerDelegate* delegate,
                    ui::mojom::WindowTree* window_tree);
  ~FocusSynchronizer() override;

  // Called when the server side wants to change focus to |window|.
  void SetFocusFromServer(WindowMus* window);

  // Called when the focused window is destroyed.
  void OnFocusedWindowDestroyed();

  WindowMus* focused_window() { return focused_window_; }

 private:
  void SetActiveFocusClient(client::FocusClient* focus_client);

  // Called internally to set |focused_window_| and update the server.
  void SetFocusedWindow(WindowMus* window);

  // Overriden from client::FocusChangeObserver:
  void OnWindowFocused(Window* gained_focus, Window* lost_focus) override;

  // Overrided from EnvObserver:
  void OnWindowInitialized(Window* window) override;
  void OnActiveFocusClientChanged(client::FocusClient* focus_client,
                                  Window* window) override;

  FocusSynchronizerDelegate* delegate_;
  ui::mojom::WindowTree* window_tree_;

  client::FocusClient* active_focus_client_ = nullptr;

  bool setting_focus_ = false;
  WindowMus* window_setting_focus_to_ = nullptr;
  WindowMus* focused_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FocusSynchronizer);
};

}  // namespace aura

#endif  // UI_AURA_MUS_FOCUS_SYNCHRONIZER_H_
