// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_POINTER_WATCHER_EVENT_ROUTER_H_
#define UI_VIEWS_MUS_POINTER_WATCHER_EVENT_ROUTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"
#include "ui/views/mus/mus_export.h"

namespace ui {
class PointerEvent;
class WindowTreeClient;
}

namespace views {

class PointerWatcher;
class PointerWatcherEventRouterTest;

// PointerWatcherEventRouter is responsible for maintaining the list of
// PointerWatchers and notifying appropriately. It is expected the owner of
// PointerWatcherEventRouter is a WindowTreeClientDelegate and calls
// OnPointerEventObserved().
class VIEWS_MUS_EXPORT PointerWatcherEventRouter
    : public NON_EXPORTED_BASE(ui::WindowTreeClientObserver) {
 public:
  // Public solely for tests.
  enum EventTypes {
    // No PointerWatchers have been added.
    NONE,

    // Used when the only PointerWatchers added do not want moves.
    NON_MOVE_EVENTS,

    // Used when at least one PointerWatcher has been added that wants moves.
    MOVE_EVENTS,
  };

  explicit PointerWatcherEventRouter(ui::WindowTreeClient* client);
  ~PointerWatcherEventRouter() override;

  // Called by WindowTreeClientDelegate to notify PointerWatchers appropriately.
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target);

  void AddPointerWatcher(PointerWatcher* watcher, bool wants_moves);
  void RemovePointerWatcher(PointerWatcher* watcher);

 private:
  friend class PointerWatcherEventRouterTest;

  // Determines EventTypes based on the number and type of PointerWatchers.
  EventTypes DetermineEventTypes();

  // ui::WindowTreeClientObserver:
  void OnWindowTreeCaptureChanged(ui::Window* gained_capture,
                                  ui::Window* lost_capture) override;
  void OnDidDestroyClient(ui::WindowTreeClient* client) override;

  ui::WindowTreeClient* window_tree_client_;
  // The true parameter to ObserverList indicates the list must be empty on
  // destruction. Two sets of observers are maintained, one for observers not
  // needing moves |non_move_watchers_| and |move_watchers_| for those
  // observers wanting moves too.
  base::ObserverList<views::PointerWatcher, true> non_move_watchers_;
  base::ObserverList<views::PointerWatcher, true> move_watchers_;

  EventTypes event_types_ = EventTypes::NONE;

  DISALLOW_COPY_AND_ASSIGN(PointerWatcherEventRouter);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_POINTER_WATCHER_EVENT_ROUTER_H_
