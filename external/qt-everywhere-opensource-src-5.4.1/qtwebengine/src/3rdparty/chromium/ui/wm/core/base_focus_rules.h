// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_BASE_FOCUS_RULES_H_
#define UI_WM_CORE_BASE_FOCUS_RULES_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/wm/core/focus_rules.h"

namespace wm {

// A set of basic focus and activation rules. Specializations should most likely
// subclass this and call up to these methods rather than reimplementing them.
class WM_EXPORT BaseFocusRules : public FocusRules {
 protected:
  BaseFocusRules();
  virtual ~BaseFocusRules();

  // Returns true if the children of |window| can be activated.
  virtual bool SupportsChildActivation(aura::Window* window) const = 0;

  // Returns true if |window| is considered visible for activation purposes.
  virtual bool IsWindowConsideredVisibleForActivation(
      aura::Window* window) const;

  // Overridden from FocusRules:
  virtual bool IsToplevelWindow(aura::Window* window) const OVERRIDE;
  virtual bool CanActivateWindow(aura::Window* window) const OVERRIDE;
  virtual bool CanFocusWindow(aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetToplevelWindow(aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetActivatableWindow(
      aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetFocusableWindow(aura::Window* window) const OVERRIDE;
  virtual aura::Window* GetNextActivatableWindow(
      aura::Window* ignore) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseFocusRules);
};

}  // namespace wm

#endif  // UI_WM_CORE_BASE_FOCUS_RULES_H_
