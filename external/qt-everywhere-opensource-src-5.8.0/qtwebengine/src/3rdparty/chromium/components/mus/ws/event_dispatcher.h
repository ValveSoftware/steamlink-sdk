// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
#define COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/event_matcher.mojom.h"
#include "components/mus/ws/modal_window_controller.h"
#include "components/mus/ws/server_window_observer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {
class Event;
class KeyEvent;
class LocatedEvent;
}

namespace mus {
namespace ws {

class Accelerator;
class EventDispatcherDelegate;
class ServerWindow;

namespace test {
class EventDispatcherTestApi;
}

// Handles dispatching events to the right location as well as updating focus.
class EventDispatcher : public ServerWindowObserver {
 public:
  explicit EventDispatcher(EventDispatcherDelegate* delegate);
  ~EventDispatcher() override;

  // Cancels capture and stops tracking any pointer events. This does not send
  // any events to the delegate.
  void Reset();

  void SetMousePointerScreenLocation(const gfx::Point& screen_location);
  const gfx::Point& mouse_pointer_last_location() const {
    return mouse_pointer_last_location_;
  }

  // If we still have the window of the last mouse move, returns true and sets
  // the current cursor to use to |cursor_out|.
  bool GetCurrentMouseCursor(int32_t* cursor_out);

  // |capture_window_| will receive all input. See window_tree.mojom for
  // details.
  ServerWindow* capture_window() { return capture_window_; }
  const ServerWindow* capture_window() const { return capture_window_; }
  // Setting capture can fail if the window is blocked by a modal window
  // (indicated by returning |false|).
  bool SetCaptureWindow(ServerWindow* capture_window,
                        ClientSpecificId client_id);

  // Id of the client that capture events are sent to.
  ClientSpecificId capture_window_client_id() const {
    return capture_window_client_id_;
  }

  // Adds a system modal window. The window remains modal to system until it is
  // destroyed. There can exist multiple system modal windows, in which case the
  // one that is visible and added most recently or shown most recently would be
  // the active one.
  void AddSystemModalWindow(ServerWindow* window);

  // Checks if |modal_window| is a visible modal window that blocks current
  // capture window and if that's the case, releases the capture.
  void ReleaseCaptureBlockedByModalWindow(const ServerWindow* modal_window);

  // Checks if the current capture window is blocked by any visible modal window
  // and if that's the case, releases the capture.
  void ReleaseCaptureBlockedByAnyModalWindow();

  // Retrieves the ServerWindow of the last mouse move.
  ServerWindow* mouse_cursor_source_window() const {
    return mouse_cursor_source_window_;
  }

  // If the mouse cursor is still over |mouse_cursor_source_window_|, updates
  // whether we are in the non-client area. Used when
  // |mouse_cursor_source_window_| has changed its properties.
  void UpdateNonClientAreaForCurrentWindow();

  // Possibly updates the cursor. If we aren't in an implicit capture, we take
  // the last known location of the mouse pointer, and look for the
  // ServerWindow* under it.
  void UpdateCursorProviderByLastKnownLocation();

  // Adds an accelerator with the given id and event-matcher. If an accelerator
  // already exists with the same id or the same matcher, then the accelerator
  // is not added. Returns whether adding the accelerator was successful or not.
  bool AddAccelerator(uint32_t id, mojom::EventMatcherPtr event_matcher);
  void RemoveAccelerator(uint32_t id);

  // Processes the supplied event, informing the delegate as approriate. This
  // may result in generating any number of events.
  void ProcessEvent(const ui::Event& event);

 private:
  friend class test::EventDispatcherTestApi;

  // Keeps track of state associated with an active pointer.
  struct PointerTarget {
    PointerTarget()
        : window(nullptr),
          is_mouse_event(false),
          in_nonclient_area(false),
          is_pointer_down(false) {}

    // NOTE: this is set to null if the window is destroyed before a
    // corresponding release/cancel.
    ServerWindow* window;

    bool is_mouse_event;

    // Did the pointer event start in the non-client area.
    bool in_nonclient_area;

    bool is_pointer_down;
  };

  void ProcessKeyEvent(const ui::KeyEvent& event);

  bool IsTrackingPointer(int32_t pointer_id) const {
    return pointer_targets_.count(pointer_id) > 0;
  }

  // EventDispatcher provides the following logic for pointer events:
  // . wheel events go to the current target of the associated pointer. If
  //   there is no target, they go to the deepest window.
  // . move (not drag) events go to the deepest window.
  // . when a pointer goes down all events until the corresponding up or
  //   cancel go to the deepest target. For mouse events the up only occurs
  //   when no buttons on the mouse are down.
  // This also generates exit events as appropriate. For example, if the mouse
  // moves between one window to another an exit is generated on the first.
  void ProcessLocatedEvent(const ui::LocatedEvent& event);

  // Adds |pointer_target| to |pointer_targets_|.
  void StartTrackingPointer(int32_t pointer_id,
                            const PointerTarget& pointer_target);

  // Removes a PointerTarget from |pointer_targets_|.
  void StopTrackingPointer(int32_t pointer_id);

  // Starts tracking the pointer for |event|, or if already tracking the
  // pointer sends the appropriate event to the delegate and updates the
  // currently tracked PointerTarget appropriately.
  void UpdateTargetForPointer(int32_t pointer_id,
                              const ui::LocatedEvent& event);

  // Returns a PointerTarget from the supplied event.
  PointerTarget PointerTargetForEvent(const ui::LocatedEvent& event);

  // Returns true if any pointers are in the pressed/down state.
  bool AreAnyPointersDown() const;

  // If |target->window| is valid, then passes the event to the delegate.
  void DispatchToPointerTarget(const PointerTarget& target,
                               const ui::LocatedEvent& event);

  // Dispatch |event| to the delegate.
  void DispatchToClient(ServerWindow* window,
                        ClientSpecificId client_id,
                        const ui::LocatedEvent& event);

  // Stops sending pointer events to |window|. This does not remove the entry
  // for |window| from |pointer_targets_|, rather it nulls out the window. This
  // way we continue to eat events until the up/cancel is received.
  void CancelPointerEventsToTarget(ServerWindow* window);

  // Used to observe a window. Can be called multiple times on a window. To
  // unobserve a window, UnobserveWindow() should be called the same number of
  // times.
  void ObserveWindow(ServerWindow* winodw);
  void UnobserveWindow(ServerWindow* winodw);

  // Returns an Accelerator bound to the specified code/flags, and of the
  // matching |phase|. Otherwise returns null.
  Accelerator* FindAccelerator(const ui::KeyEvent& event,
                               const ui::mojom::AcceleratorPhase phase);

  ServerWindow* FindDeepestVisibleWindowForEvents(gfx::Point* location);

  // ServerWindowObserver:
  void OnWillChangeWindowHierarchy(ServerWindow* window,
                                   ServerWindow* new_parent,
                                   ServerWindow* old_parent) override;
  void OnWindowVisibilityChanged(ServerWindow* window) override;
  void OnWindowDestroyed(ServerWindow* window) override;

  EventDispatcherDelegate* delegate_;

  ServerWindow* capture_window_;
  ClientSpecificId capture_window_client_id_;

  ModalWindowController modal_window_controller_;

  bool mouse_button_down_;
  ServerWindow* mouse_cursor_source_window_;
  bool mouse_cursor_in_non_client_area_;

  // The on screen location of the mouse pointer. This can be outside the
  // bounds of |mouse_cursor_source_window_|, which can capture the cursor.
  gfx::Point mouse_pointer_last_location_;

  std::map<uint32_t, std::unique_ptr<Accelerator>> accelerators_;

  using PointerIdToTargetMap = std::map<int32_t, PointerTarget>;
  // |pointer_targets_| contains the active pointers. For a mouse based pointer
  // a PointerTarget is always active (and present in |pointer_targets_|). For
  // touch based pointers the pointer is active while down and removed on
  // cancel or up.
  PointerIdToTargetMap pointer_targets_;

  // Keeps track of number of observe requests for each observed window.
  std::map<const ServerWindow*, uint8_t> observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_EVENT_DISPATCHER_H_
