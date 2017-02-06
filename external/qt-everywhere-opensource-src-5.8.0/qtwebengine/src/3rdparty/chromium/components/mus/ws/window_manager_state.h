// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/ws/event_dispatcher.h"
#include "components/mus/ws/event_dispatcher_delegate.h"
#include "components/mus/ws/user_id.h"
#include "components/mus/ws/window_server.h"

namespace mus {
namespace ws {

class DisplayManager;
class WindowTree;

namespace test {
class WindowManagerStateTestApi;
}

// Manages state specific to a WindowManager that is shared across displays.
// WindowManagerState is owned by the WindowTree the window manager is
// associated with.
class WindowManagerState : public EventDispatcherDelegate {
 public:
  explicit WindowManagerState(WindowTree* window_tree);
  ~WindowManagerState() override;

  const UserId& user_id() const;

  WindowTree* window_tree() { return window_tree_; }
  const WindowTree* window_tree() const { return window_tree_; }

  void OnWillDestroyTree(WindowTree* tree);

  void SetFrameDecorationValues(mojom::FrameDecorationValuesPtr values);
  const mojom::FrameDecorationValues& frame_decoration_values() const {
    return *frame_decoration_values_;
  }
  bool got_frame_decoration_values() const {
    return got_frame_decoration_values_;
  }

  bool SetCapture(ServerWindow* window, ClientSpecificId client_id);
  ServerWindow* capture_window() { return event_dispatcher_.capture_window(); }
  const ServerWindow* capture_window() const {
    return event_dispatcher_.capture_window();
  }

  void ReleaseCaptureBlockedByModalWindow(const ServerWindow* modal_window);
  void ReleaseCaptureBlockedByAnyModalWindow();

  void AddSystemModalWindow(ServerWindow* window);

  // TODO(sky): EventDispatcher is really an implementation detail and should
  // not be exposed.
  EventDispatcher* event_dispatcher() { return &event_dispatcher_; }

  // Returns true if this is the WindowManager of the active user.
  bool IsActive() const;

  void Activate(const gfx::Point& mouse_location_on_screen);
  void Deactivate();

  // Processes an event from PlatformDisplay.
  void ProcessEvent(const ui::Event& event);

  // Called when the ack from an event dispatched to WindowTree |tree| is
  // received.
  // TODO(sky): make this private and use a callback.
  void OnEventAck(mojom::WindowTree* tree, mojom::EventResult result);

 private:
  class ProcessedEventTarget;
  friend class Display;
  friend class test::WindowManagerStateTestApi;

  // There are two types of events that may be queued, both occur only when
  // waiting for an ack from a client.
  // . We get an event from the PlatformDisplay. This results in |event| being
  //   set, but |processed_target| is null.
  // . We get an event from the EventDispatcher. In this case both |event| and
  //   |processed_target| are valid.
  // The second case happens if EventDispatcher generates more than one event
  // at a time.
  struct QueuedEvent {
    QueuedEvent();
    ~QueuedEvent();

    std::unique_ptr<ui::Event> event;
    std::unique_ptr<ProcessedEventTarget> processed_target;
  };

  const WindowServer* window_server() const;
  WindowServer* window_server();

  DisplayManager* display_manager();
  const DisplayManager* display_manager() const;

  // Sets the visibility of all window manager roots windows to |value|.
  void SetAllRootWindowsVisible(bool value);

  // Returns the ServerWindow that is the root of the WindowManager for
  // |window|. |window| corresponds to the root of a Display.
  ServerWindow* GetWindowManagerRoot(ServerWindow* window);

  void OnEventAckTimeout(ClientSpecificId client_id);

  // Schedules an event to be processed later.
  void QueueEvent(const ui::Event& event,
                  std::unique_ptr<ProcessedEventTarget> processed_event_target);

  // Processes the next valid event in |event_queue_|. If the event has already
  // been processed it is dispatched, otherwise the event is passed to the
  // EventDispatcher for processing.
  void ProcessNextEventFromQueue();

  // Dispatches the event to the appropriate client and starts the ack timer.
  void DispatchInputEventToWindowImpl(ServerWindow* target,
                                      ClientSpecificId client_id,
                                      const ui::Event& event,
                                      base::WeakPtr<Accelerator> accelerator);

  // Registers accelerators used internally for debugging.
  void AddDebugAccelerators();

  // Returns true if the accelerator was handled.
  bool HandleDebugAccelerator(uint32_t accelerator_id);

  // EventDispatcherDelegate:
  void OnAccelerator(uint32_t accelerator_id, const ui::Event& event) override;
  void SetFocusedWindowFromEventDispatcher(ServerWindow* window) override;
  ServerWindow* GetFocusedWindowForEventDispatcher() override;
  void SetNativeCapture(ServerWindow* window) override;
  void ReleaseNativeCapture() override;
  void OnServerWindowCaptureLost(ServerWindow* window) override;
  void OnMouseCursorLocationChanged(const gfx::Point& point) override;
  void DispatchInputEventToWindow(ServerWindow* target,
                                  ClientSpecificId client_id,
                                  const ui::Event& event,
                                  Accelerator* accelerator) override;
  ClientSpecificId GetEventTargetClientId(const ServerWindow* window,
                                          bool in_nonclient_area) override;
  ServerWindow* GetRootWindowContaining(const gfx::Point& location) override;
  void OnEventTargetNotFound(const ui::Event& event) override;

  // The single WindowTree this WindowManagerState is associated with.
  // |window_tree_| owns this.
  WindowTree* window_tree_;

  // Set to true the first time SetFrameDecorationValues() is called.
  bool got_frame_decoration_values_ = false;
  mojom::FrameDecorationValuesPtr frame_decoration_values_;

  mojom::WindowTree* tree_awaiting_input_ack_ = nullptr;
  std::unique_ptr<ui::Event> event_awaiting_input_ack_;
  base::WeakPtr<Accelerator> post_target_accelerator_;
  std::queue<std::unique_ptr<QueuedEvent>> event_queue_;
  base::OneShotTimer event_ack_timer_;

  EventDispatcher event_dispatcher_;

  // PlatformDisplay that currently has capture.
  PlatformDisplay* platform_display_with_capture_ = nullptr;

  base::WeakPtrFactory<WindowManagerState> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerState);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_STATE_H_
