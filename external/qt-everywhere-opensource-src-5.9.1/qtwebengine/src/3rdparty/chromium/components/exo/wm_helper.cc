// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper.h"

#include "base/memory/ptr_util.h"

namespace exo {
namespace {
WMHelper* g_instance = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// WMHelper, public:

WMHelper::WMHelper() {}

WMHelper::~WMHelper() {}

// static
void WMHelper::SetInstance(WMHelper* helper) {
  DCHECK_NE(!!helper, !!g_instance);
  g_instance = helper;
}

// static
WMHelper* WMHelper::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

void WMHelper::AddActivationObserver(ActivationObserver* observer) {
  activation_observers_.AddObserver(observer);
}

void WMHelper::RemoveActivationObserver(ActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WMHelper::AddFocusObserver(FocusObserver* observer) {
  focus_observers_.AddObserver(observer);
}

void WMHelper::RemoveFocusObserver(FocusObserver* observer) {
  focus_observers_.RemoveObserver(observer);
}

void WMHelper::AddCursorObserver(CursorObserver* observer) {
  cursor_observers_.AddObserver(observer);
}

void WMHelper::RemoveCursorObserver(CursorObserver* observer) {
  cursor_observers_.RemoveObserver(observer);
}

void WMHelper::AddMaximizeModeObserver(MaximizeModeObserver* observer) {
  maximize_mode_observers_.AddObserver(observer);
}

void WMHelper::RemoveMaximizeModeObserver(MaximizeModeObserver* observer) {
  maximize_mode_observers_.RemoveObserver(observer);
}

void WMHelper::AddAccessibilityObserver(AccessibilityObserver* observer) {
  accessibility_observers_.AddObserver(observer);
}

void WMHelper::RemoveAccessibilityObserver(AccessibilityObserver* observer) {
  accessibility_observers_.RemoveObserver(observer);
}

void WMHelper::NotifyWindowActivated(aura::Window* gained_active,
                                     aura::Window* lost_active) {
  for (ActivationObserver& observer : activation_observers_)
    observer.OnWindowActivated(gained_active, lost_active);
}

void WMHelper::NotifyWindowFocused(aura::Window* gained_focus,
                                   aura::Window* lost_focus) {
  for (FocusObserver& observer : focus_observers_)
    observer.OnWindowFocused(gained_focus, lost_focus);
}

void WMHelper::NotifyCursorVisibilityChanged(bool is_visible) {
  for (CursorObserver& observer : cursor_observers_)
    observer.OnCursorVisibilityChanged(is_visible);
}

void WMHelper::NotifyCursorSetChanged(ui::CursorSetType cursor_set) {
  for (CursorObserver& observer : cursor_observers_)
    observer.OnCursorSetChanged(cursor_set);
}

void WMHelper::NotifyMaximizeModeStarted() {
  for (MaximizeModeObserver& observer : maximize_mode_observers_)
    observer.OnMaximizeModeStarted();
}

void WMHelper::NotifyMaximizeModeEnded() {
  for (MaximizeModeObserver& observer : maximize_mode_observers_)
    observer.OnMaximizeModeEnded();
}

void WMHelper::NotifyAccessibilityModeChanged() {
  for (AccessibilityObserver& observer : accessibility_observers_)
    observer.OnAccessibilityModeChanged();
}

}  // namespace exo
