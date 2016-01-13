// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/views_export.h"
#include "ui/wm/core/native_cursor_manager.h"

namespace aura {
class WindowTreeHost;
}

namespace ui {
class CursorLoader;
}

namespace wm {
class NativeCursorManagerDelegate;
}

namespace views {
class DesktopCursorLoaderUpdater;

// A NativeCursorManager that performs the desktop-specific setting of cursor
// state. Similar to AshNativeCursorManager, it also communicates these changes
// to all root windows.
class VIEWS_EXPORT DesktopNativeCursorManager
    : public wm::NativeCursorManager {
 public:
  DesktopNativeCursorManager(
      scoped_ptr<DesktopCursorLoaderUpdater> cursor_loader_updater);
  virtual ~DesktopNativeCursorManager();

  // Builds a cursor and sets the internal platform representation.
  gfx::NativeCursor GetInitializedCursor(int type);

  // Adds |host| to the set |hosts_|.
  void AddHost(aura::WindowTreeHost* host);

  // Removes |host| from the set |hosts_|.
  void RemoveHost(aura::WindowTreeHost* host);

 private:
  // Overridden from wm::NativeCursorManager:
  virtual void SetDisplay(
      const gfx::Display& display,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetCursor(
      gfx::NativeCursor cursor,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetVisibility(
      bool visible,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetCursorSet(
      ui::CursorSetType cursor_set,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE;

  // The set of hosts to notify of changes in cursor state.
  typedef std::set<aura::WindowTreeHost*> Hosts;
  Hosts hosts_;

  scoped_ptr<DesktopCursorLoaderUpdater> cursor_loader_updater_;
  scoped_ptr<ui::CursorLoader> cursor_loader_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeCursorManager);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_

