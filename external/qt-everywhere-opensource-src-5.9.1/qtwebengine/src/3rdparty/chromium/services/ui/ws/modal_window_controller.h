// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_MODAL_WINDOW_CONTROLLER_H_
#define SERVICES_UI_WS_MODAL_WINDOW_CONTROLLER_H_

#include <list>
#include <map>

#include "services/ui/ws/server_window_drawn_tracker_observer.h"
#include "services/ui/ws/server_window_observer.h"

namespace ui {
namespace ws {

class EventDispatcher;
class ServerWindow;
class ServerWindowDrawnTracker;

namespace test {
class ModalWindowControllerTestApi;
}

// Used to keeps track of system modal windows and check whether windows are
// blocked by modal windows or not to do appropriate retargetting of events.
class ModalWindowController : public ServerWindowObserver,
                              public ServerWindowDrawnTrackerObserver {
 public:
  explicit ModalWindowController(EventDispatcher* event_dispatcher);
  ~ModalWindowController() override;

  // Adds a system modal window. The window remains modal to system until it is
  // destroyed. There can exist multiple system modal windows, in which case the
  // one that is visible and added most recently or shown most recently would be
  // the active one.
  void AddSystemModalWindow(ServerWindow* window);

  // Checks whether |modal_window| is a visible modal window that blocks
  // |window|.
  bool IsWindowBlockedBy(const ServerWindow* window,
                         const ServerWindow* modal_window) const;

  // Checks whether |window| is blocked by any visible modal window.
  bool IsWindowBlocked(const ServerWindow* window) const;

  // Returns the window that events targeted to |window| should be retargeted
  // to; according to modal windows. If any modal window is blocking |window|
  // (either system modal or window modal), returns the topmost one; otherwise,
  // returns |window| itself.
  const ServerWindow* GetTargetForWindow(const ServerWindow* window) const;
  ServerWindow* GetTargetForWindow(ServerWindow* window) const {
    return const_cast<ServerWindow*>(
        GetTargetForWindow(static_cast<const ServerWindow*>(window)));
  }

 private:
  friend class test::ModalWindowControllerTestApi;

  // Returns the system modal window that is visible and added/shown most
  // recently, if any.
  ServerWindow* GetActiveSystemModalWindow() const;

  // ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override;

  // ServerWindowDrawnTrackerObserver:
  void OnDrawnStateChanged(ServerWindow* ancestor,
                           ServerWindow* window,
                           bool is_drawn) override;

  EventDispatcher* event_dispatcher_;

  // List of system modal windows in order they are added/shown.
  std::list<ServerWindow*> system_modal_windows_;

  // Drawn trackers for system modal windows.
  std::map<ServerWindow*, std::unique_ptr<ServerWindowDrawnTracker>>
      window_drawn_trackers_;

  DISALLOW_COPY_AND_ASSIGN(ModalWindowController);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_MODAL_WINDOW_CONTROLLER_H_
