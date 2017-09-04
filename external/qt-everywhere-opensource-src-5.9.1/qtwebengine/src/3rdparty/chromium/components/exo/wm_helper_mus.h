// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_MUS_H_
#define COMPONENTS_EXO_WM_HELPER_MUS_H_

#include "base/macros.h"
#include "components/exo/wm_helper.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelperMus : public WMHelper,
                    public ui::WindowTreeClientObserver,
                    public aura::client::FocusChangeObserver {
 public:
  WMHelperMus();
  ~WMHelperMus() override;

  // Overriden from WMHelper:
  const display::ManagedDisplayInfo GetDisplayInfo(
      int64_t display_id) const override;
  aura::Window* GetContainer(int container_id) override;
  aura::Window* GetActiveWindow() const override;
  aura::Window* GetFocusedWindow() const override;
  ui::CursorSetType GetCursorSet() const override;
  void AddPreTargetHandler(ui::EventHandler* handler) override;
  void PrependPreTargetHandler(ui::EventHandler* handler) override;
  void RemovePreTargetHandler(ui::EventHandler* handler) override;
  void AddPostTargetHandler(ui::EventHandler* handler) override;
  void RemovePostTargetHandler(ui::EventHandler* handler) override;
  bool IsMaximizeModeWindowManagerEnabled() const override;
  bool IsSpokenFeedbackEnabled() const override;
  void PlayEarcon(int sound_key) const override;

  // Overriden from ui::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(ui::Window* gained_focus,
                                ui::Window* lost_focus) override;

  // Overriden from ui::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

 private:
  aura::Window* active_window_;
  aura::Window* focused_window_;

  DISALLOW_COPY_AND_ASSIGN(WMHelperMus);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
