// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_FOCUS_RULES_H_
#define UI_VIEWS_WIDGET_DESKTOP_FOCUS_RULES_H_

#include "ui/wm/core/base_focus_rules.h"

namespace views {

class DesktopFocusRules : public wm::BaseFocusRules {
 public:
  explicit DesktopFocusRules(aura::Window* content_window);
  virtual ~DesktopFocusRules();

 private:
  // Overridden from wm::BaseFocusRules:
  virtual bool CanActivateWindow(aura::Window* window) const OVERRIDE;
  virtual bool SupportsChildActivation(aura::Window* window) const OVERRIDE;
  virtual bool IsWindowConsideredVisibleForActivation(
      aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetToplevelWindow(aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetNextActivatableWindow(
      aura::Window* window) const OVERRIDE;

  // The content window. This is an activatable window even though it is a
  // child.
  aura::Window* content_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopFocusRules);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_FOCUS_RULES_H_
