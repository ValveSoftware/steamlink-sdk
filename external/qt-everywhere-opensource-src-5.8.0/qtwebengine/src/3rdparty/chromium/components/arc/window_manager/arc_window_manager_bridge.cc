// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/window_manager/arc_window_manager_bridge.h"

#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/logging.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

ArcWindowManagerBridge::ArcWindowManagerBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      current_mode_(mojom::WindowManagerMode::MODE_NORMAL) {
  arc_bridge_service()->window_manager()->AddObserver(this);
  if (!ash::WmShell::HasInstance()) {
    // The shell gets always loaded before ARC. If there is no shell it can only
    // mean that a unit test is running.
    return;
  }
  // Monitor any mode changes from now on.
  ash::WmShell::Get()->AddShellObserver(this);
}

void ArcWindowManagerBridge::OnInstanceReady() {
  if (!ash::Shell::HasInstance()) {
    // The shell gets always loaded before ARC. If there is no shell it can only
    // mean that a unit test is running.
    return;
  }
  ash::MaximizeModeController* controller =
      ash::Shell::GetInstance()->maximize_mode_controller();
  if (!controller)
    return;

  // Set the initial mode configuration.
  SendWindowManagerModeChange(controller->IsMaximizeModeWindowManagerEnabled());
}

ArcWindowManagerBridge::~ArcWindowManagerBridge() {
  if (ash::WmShell::HasInstance())
    ash::WmShell::Get()->RemoveShellObserver(this);
  arc_bridge_service()->window_manager()->RemoveObserver(this);
}

void ArcWindowManagerBridge::OnMaximizeModeStarted() {
  SendWindowManagerModeChange(true);
}

void ArcWindowManagerBridge::OnMaximizeModeEnded() {
  SendWindowManagerModeChange(false);
}

void ArcWindowManagerBridge::SendWindowManagerModeChange(
    bool touch_view_enabled) {
  // We let the ArcBridgeService check that we are calling on the right thread.
  DCHECK(ArcBridgeService::Get() != nullptr);
  mojom::WindowManagerMode wm_mode =
      touch_view_enabled ? mojom::WindowManagerMode::MODE_TOUCH_VIEW
                         : mojom::WindowManagerMode::MODE_NORMAL;

  mojom::WindowManagerInstance* wm_instance =
      arc_bridge_service()->window_manager()->instance();
  if (!wm_instance || wm_mode == current_mode_) {
    return;
  }
  VLOG(1) << "Sending window manager mode change to " << wm_mode;
  wm_instance->OnWindowManagerModeChange(wm_mode);
  current_mode_ = wm_mode;
}

}  // namespace arc
