// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_DISPLAY_MANAGER_H_
#define SERVICES_UI_WS_DISPLAY_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/macros.h"
#include "services/ui/display/platform_screen_delegate.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/user_id.h"
#include "services/ui/ws/user_id_tracker_observer.h"

namespace ui {
namespace ws {

class Display;
class ServerWindow;
class UserDisplayManager;
class UserIdTracker;
class WindowManagerDisplayRoot;
class WindowServer;

// DisplayManager manages the set of Displays. DisplayManager distinguishes
// between displays that do yet have an accelerated widget (pending), vs
// those that do.
class DisplayManager : public UserIdTrackerObserver,
                       public display::PlatformScreenDelegate {
 public:
  DisplayManager(WindowServer* window_server, UserIdTracker* user_id_tracker);
  ~DisplayManager() override;

  // Returns the UserDisplayManager for |user_id|. DisplayManager owns the
  // return value.
  UserDisplayManager* GetUserDisplayManager(const UserId& user_id);

  // Adds/removes a Display. DisplayManager owns the Displays.
  // TODO(sky): make add take a scoped_ptr.
  void AddDisplay(Display* display);
  void DestroyDisplay(Display* display);
  void DestroyAllDisplays();
  const std::set<Display*>& displays() { return displays_; }
  std::set<const Display*> displays() const;

  // Notifies when something about the Display changes.
  void OnDisplayUpdate(Display* display);

  // Returns the Display that contains |window|, or null if |window| is not
  // attached to a display.
  Display* GetDisplayContaining(const ServerWindow* window);
  const Display* GetDisplayContaining(const ServerWindow* window) const;

  // Returns the display with the specified display id, or null if there is no
  // display with that id.
  Display* GetDisplayById(int64_t display_id);

  const WindowManagerDisplayRoot* GetWindowManagerDisplayRoot(
      const ServerWindow* window) const;
  // TODO(sky): constness here is wrong! fix!
  WindowManagerDisplayRoot* GetWindowManagerDisplayRoot(
      const ServerWindow* window);

  bool has_displays() const { return !displays_.empty(); }
  bool has_active_or_pending_displays() const {
    return !displays_.empty() || !pending_displays_.empty();
  }

  // Returns the id for the next root window (both for the root of a Display
  // as well as the root of WindowManagers).
  WindowId GetAndAdvanceNextRootId();

  // Called when the AcceleratedWidget is available for |display|.
  void OnDisplayAcceleratedWidgetAvailable(Display* display);

 private:
  // UserIdTrackerObserver:
  void OnActiveUserIdChanged(const UserId& previously_active_id,
                             const UserId& active_id) override;

  // display::PlatformScreenDelegate:
  void OnDisplayAdded(int64_t id,
                      const display::ViewportMetrics& metrics) override;
  void OnDisplayRemoved(int64_t id) override;
  void OnDisplayModified(int64_t id,
                         const display::ViewportMetrics& metrics) override;
  void OnPrimaryDisplayChanged(int64_t primary_display_id) override;

  WindowServer* window_server_;
  UserIdTracker* user_id_tracker_;

  // Displays are initially added to |pending_displays_|. When the display is
  // initialized it is moved to |displays_|. WindowServer owns the Displays.
  std::set<Display*> pending_displays_;
  std::set<Display*> displays_;

  std::map<UserId, std::unique_ptr<UserDisplayManager>> user_display_managers_;

  // ID to use for next root node.
  ClientSpecificId next_root_id_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_DISPLAY_MANAGER_H_
