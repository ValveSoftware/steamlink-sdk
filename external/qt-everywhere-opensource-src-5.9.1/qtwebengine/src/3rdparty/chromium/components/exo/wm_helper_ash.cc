// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_ash.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/memory/singleton.h"
#include "ui/aura/client/focus_client.h"
#include "ui/display/manager/display_manager.h"
#include "ui/wm/public/activation_client.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// WMHelperAsh, public:

WMHelperAsh::WMHelperAsh() {
  ash::WmShell::Get()->AddShellObserver(this);
  ash::Shell::GetInstance()->activation_client()->AddObserver(this);
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
}

WMHelperAsh::~WMHelperAsh() {
  if (!ash::Shell::HasInstance())
    return;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->RemoveObserver(this);
  ash::Shell::GetInstance()->activation_client()->RemoveObserver(this);
  ash::WmShell::Get()->RemoveShellObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WMHelperAsh, private:

const display::ManagedDisplayInfo WMHelperAsh::GetDisplayInfo(
    int64_t display_id) const {
  return ash::Shell::GetInstance()->display_manager()->GetDisplayInfo(
      display_id);
}

aura::Window* WMHelperAsh::GetContainer(int container_id) {
  return ash::Shell::GetContainer(ash::Shell::GetTargetRootWindow(),
                                  container_id);
}

aura::Window* WMHelperAsh::GetActiveWindow() const {
  return ash::Shell::GetInstance()->activation_client()->GetActiveWindow();
}

aura::Window* WMHelperAsh::GetFocusedWindow() const {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  return focus_client->GetFocusedWindow();
}

ui::CursorSetType WMHelperAsh::GetCursorSet() const {
  return ash::Shell::GetInstance()->cursor_manager()->GetCursorSet();
}

void WMHelperAsh::AddPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->AddPreTargetHandler(handler);
}

void WMHelperAsh::PrependPreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->PrependPreTargetHandler(handler);
}

void WMHelperAsh::RemovePreTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->RemovePreTargetHandler(handler);
}

void WMHelperAsh::AddPostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->AddPostTargetHandler(handler);
}

void WMHelperAsh::RemovePostTargetHandler(ui::EventHandler* handler) {
  ash::Shell::GetInstance()->RemovePostTargetHandler(handler);
}

bool WMHelperAsh::IsMaximizeModeWindowManagerEnabled() const {
  return ash::WmShell::Get()
      ->maximize_mode_controller()
      ->IsMaximizeModeWindowManagerEnabled();
}

bool WMHelperAsh::IsSpokenFeedbackEnabled() const {
  return ash::WmShell::Get()
      ->accessibility_delegate()
      ->IsSpokenFeedbackEnabled();
}

void WMHelperAsh::PlayEarcon(int sound_key) const {
  return ash::WmShell::Get()->accessibility_delegate()->PlayEarcon(sound_key);
}

void WMHelperAsh::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  NotifyWindowActivated(gained_active, lost_active);
}

void WMHelperAsh::OnWindowFocused(aura::Window* gained_focus,
                                  aura::Window* lost_focus) {
  NotifyWindowFocused(gained_focus, lost_focus);
}

void WMHelperAsh::OnCursorVisibilityChanged(bool is_visible) {
  NotifyCursorVisibilityChanged(is_visible);
}

void WMHelperAsh::OnCursorSetChanged(ui::CursorSetType cursor_set) {
  NotifyCursorSetChanged(cursor_set);
}

void WMHelperAsh::OnAccessibilityModeChanged(
    ash::AccessibilityNotificationVisibility notify) {
  NotifyAccessibilityModeChanged();
}

void WMHelperAsh::OnMaximizeModeStarted() {
  NotifyMaximizeModeStarted();
}

void WMHelperAsh::OnMaximizeModeEnded() {
  NotifyMaximizeModeEnded();
}

}  // namespace exo
