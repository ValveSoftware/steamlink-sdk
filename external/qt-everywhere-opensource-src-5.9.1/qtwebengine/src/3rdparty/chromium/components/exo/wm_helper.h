// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WM_HELPER_H_
#define COMPONENTS_EXO_WM_HELPER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/cursor/cursor.h"

namespace aura {
class Window;
}

namespace display {
class ManagedDisplayInfo;
}

namespace ui {
class EventHandler;
}

namespace exo {

// A helper class for accessing WindowManager related features.
class WMHelper {
 public:
  class ActivationObserver {
   public:
    virtual void OnWindowActivated(aura::Window* gained_active,
                                   aura::Window* lost_active) = 0;

   protected:
    virtual ~ActivationObserver() {}
  };

  class FocusObserver {
   public:
    virtual void OnWindowFocused(aura::Window* gained_focus,
                                 aura::Window* lost_focus) = 0;

   protected:
    virtual ~FocusObserver() {}
  };

  class CursorObserver {
   public:
    virtual void OnCursorVisibilityChanged(bool is_visible) {}
    virtual void OnCursorSetChanged(ui::CursorSetType cursor_set) {}

   protected:
    virtual ~CursorObserver() {}
  };

  class MaximizeModeObserver {
   public:
    virtual void OnMaximizeModeStarted() = 0;
    virtual void OnMaximizeModeEnded() = 0;

   protected:
    virtual ~MaximizeModeObserver() {}
  };

  class AccessibilityObserver {
   public:
    virtual void OnAccessibilityModeChanged() = 0;

   protected:
    virtual ~AccessibilityObserver() {}
  };

  virtual ~WMHelper();

  static void SetInstance(WMHelper* helper);
  static WMHelper* GetInstance();

  void AddActivationObserver(ActivationObserver* observer);
  void RemoveActivationObserver(ActivationObserver* observer);
  void AddFocusObserver(FocusObserver* observer);
  void RemoveFocusObserver(FocusObserver* observer);
  void AddCursorObserver(CursorObserver* observer);
  void RemoveCursorObserver(CursorObserver* observer);
  void AddMaximizeModeObserver(MaximizeModeObserver* observer);
  void RemoveMaximizeModeObserver(MaximizeModeObserver* observer);
  void AddAccessibilityObserver(AccessibilityObserver* observer);
  void RemoveAccessibilityObserver(AccessibilityObserver* observer);

  virtual const display::ManagedDisplayInfo GetDisplayInfo(
      int64_t display_id) const = 0;
  virtual aura::Window* GetContainer(int container_id) = 0;
  virtual aura::Window* GetActiveWindow() const = 0;
  virtual aura::Window* GetFocusedWindow() const = 0;
  virtual ui::CursorSetType GetCursorSet() const = 0;
  virtual void AddPreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void PrependPreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void RemovePreTargetHandler(ui::EventHandler* handler) = 0;
  virtual void AddPostTargetHandler(ui::EventHandler* handler) = 0;
  virtual void RemovePostTargetHandler(ui::EventHandler* handler) = 0;
  virtual bool IsMaximizeModeWindowManagerEnabled() const = 0;
  virtual bool IsSpokenFeedbackEnabled() const = 0;
  virtual void PlayEarcon(int sound_key) const = 0;

 protected:
  WMHelper();

  void NotifyWindowActivated(aura::Window* gained_active,
                             aura::Window* lost_active);
  void NotifyWindowFocused(aura::Window* gained_focus,
                           aura::Window* lost_focus);
  void NotifyCursorVisibilityChanged(bool is_visible);
  void NotifyCursorSetChanged(ui::CursorSetType cursor_set);
  void NotifyMaximizeModeStarted();
  void NotifyMaximizeModeEnded();
  void NotifyAccessibilityModeChanged();

 private:
  base::ObserverList<ActivationObserver> activation_observers_;
  base::ObserverList<FocusObserver> focus_observers_;
  base::ObserverList<CursorObserver> cursor_observers_;
  base::ObserverList<MaximizeModeObserver> maximize_mode_observers_;
  base::ObserverList<AccessibilityObserver> accessibility_observers_;

  DISALLOW_COPY_AND_ASSIGN(WMHelper);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WM_HELPER_H_
