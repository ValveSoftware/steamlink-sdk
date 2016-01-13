// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_EVENT_DISPATCHER_H_
#define UI_AURA_WINDOW_EVENT_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_observer.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/capture_delegate.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_targeter.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace gfx {
class Size;
class Transform;
}

namespace ui {
class GestureEvent;
class GestureRecognizer;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class TouchEvent;
}

namespace aura {
class TestScreen;
class WindowTargeter;
class WindowTreeHost;

// WindowEventDispatcher orchestrates event dispatch within a window tree
// owned by WindowTreeHost. WTH also owns the WED.
// TODO(beng): In progress, remove functionality not directly related to
//             event dispatch.
class AURA_EXPORT WindowEventDispatcher : public ui::EventProcessor,
                                          public ui::GestureEventHelper,
                                          public client::CaptureDelegate,
                                          public WindowObserver,
                                          public EnvObserver {
 public:
  explicit WindowEventDispatcher(WindowTreeHost* host);
  virtual ~WindowEventDispatcher();

  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }
  Window* mouse_moved_handler() { return mouse_moved_handler_; }

  // Repost event for re-processing. Used when exiting context menus.
  // We only support the ET_MOUSE_PRESSED and ET_GESTURE_TAP_DOWN event
  // types (although the latter is currently a no-op).
  void RepostEvent(const ui::LocatedEvent& event);

  // Invoked when the mouse events get enabled or disabled.
  void OnMouseEventsEnableStateChanged(bool enabled);

  void DispatchCancelModeEvent();

  // Dispatches a ui::ET_MOUSE_EXITED event at |point|.
  // TODO(beng): needed only for WTH::OnCursorVisibilityChanged().
  void DispatchMouseExitAtPoint(const gfx::Point& point);

  // Gesture Recognition -------------------------------------------------------

  // When a touch event is dispatched to a Window, it may want to process the
  // touch event asynchronously. In such cases, the window should consume the
  // event during the event dispatch. Once the event is properly processed, the
  // window should let the WindowEventDispatcher know about the result of the
  // event processing, so that gesture events can be properly created and
  // dispatched.
  void ProcessedTouchEvent(ui::TouchEvent* event,
                           Window* window,
                           ui::EventResult result);

  // These methods are used to defer the processing of mouse/touch events
  // related to resize. A client (typically a RenderWidgetHostViewAura) can call
  // HoldPointerMoves when an resize is initiated and then ReleasePointerMoves
  // once the resize is completed.
  //
  // More than one hold can be invoked and each hold must be cancelled by a
  // release before we resume normal operation.
  void HoldPointerMoves();
  void ReleasePointerMoves();

  // Gets the last location seen in a mouse event in this root window's
  // coordinates. This may return a point outside the root window's bounds.
  gfx::Point GetLastMouseLocationInRoot() const;

  void OnHostLostMouseGrab();
  void OnCursorMovedToRootLocation(const gfx::Point& root_location);

  // TODO(beng): This is only needed because this cleanup needs to happen after
  //             all other observers are notified of OnWindowDestroying() but
  //             before OnWindowDestroyed() is sent (i.e. while the window
  //             hierarchy is still intact). This didn't seem worth adding a
  //             generic notification for as only this class needs to implement
  //             it. I would however like to find a way to do this via an
  //             observer.
  void OnPostNotifiedWindowDestroying(Window* window);

 private:
  FRIEND_TEST_ALL_PREFIXES(WindowEventDispatcherTest,
                           KeepTranslatedEventInRoot);

  friend class Window;
  friend class TestScreen;

  // The parameter for OnWindowHidden() to specify why window is hidden.
  enum WindowHiddenReason {
    WINDOW_DESTROYED,  // Window is destroyed.
    WINDOW_HIDDEN,     // Window is hidden.
    WINDOW_MOVING,     // Window is temporarily marked as hidden due to move
                       // across root windows.
  };

  Window* window();
  const Window* window() const;

  // Updates the event with the appropriate transform for the device scale
  // factor. The WindowEventDispatcher dispatches events in the physical pixel
  // coordinate. But the event processing from WindowEventDispatcher onwards
  // happen in device-independent pixel coordinate. So it is necessary to update
  // the event received from the host.
  void TransformEventForDeviceScaleFactor(ui::LocatedEvent* event);

  // Dispatches OnMouseExited to the |window| which is hiding if necessary.
  void DispatchMouseExitToHidingWindow(Window* window);

  // Dispatches the specified event type (intended for enter/exit) to the
  // |mouse_moved_handler_|.
  ui::EventDispatchDetails DispatchMouseEnterOrExit(
      const ui::MouseEvent& event,
      ui::EventType type) WARN_UNUSED_RESULT;
  ui::EventDispatchDetails ProcessGestures(
      ui::GestureRecognizer::Gestures* gestures) WARN_UNUSED_RESULT;

  // Called when a window becomes invisible, either by being removed
  // from root window hierarchy, via SetVisible(false) or being destroyed.
  // |reason| specifies what triggered the hiding. Note that becoming invisible
  // will cause a window to lose capture and some windows may destroy themselves
  // on capture (like DragDropTracker).
  void OnWindowHidden(Window* invisible, WindowHiddenReason reason);

  // Returns a target window for the given gesture event.
  Window* GetGestureTarget(ui::GestureEvent* event);

  // Overridden from aura::client::CaptureDelegate:
  virtual void UpdateCapture(Window* old_capture, Window* new_capture) OVERRIDE;
  virtual void OnOtherRootGotCapture() OVERRIDE;
  virtual void SetNativeCapture() OVERRIDE;
  virtual void ReleaseNativeCapture() OVERRIDE;

  // Overridden from ui::EventProcessor:
  virtual ui::EventTarget* GetRootTarget() OVERRIDE;
  virtual void PrepareEventForDispatch(ui::Event* event) OVERRIDE;

  // Overridden from ui::EventDispatcherDelegate.
  virtual bool CanDispatchToTarget(ui::EventTarget* target) OVERRIDE;
  virtual ui::EventDispatchDetails PreDispatchEvent(ui::EventTarget* target,
                                                    ui::Event* event) OVERRIDE;
  virtual ui::EventDispatchDetails PostDispatchEvent(
      ui::EventTarget* target, const ui::Event& event) OVERRIDE;

  // Overridden from ui::GestureEventHelper.
  virtual bool CanDispatchToConsumer(ui::GestureConsumer* consumer) OVERRIDE;
  virtual void DispatchGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void DispatchCancelTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowDestroying(Window* window) OVERRIDE;
  virtual void OnWindowDestroyed(Window* window) OVERRIDE;
  virtual void OnWindowAddedToRootWindow(Window* window) OVERRIDE;
  virtual void OnWindowRemovingFromRootWindow(Window* window,
                                              Window* new_root) OVERRIDE;
  virtual void OnWindowVisibilityChanging(Window* window,
                                          bool visible) OVERRIDE;
  virtual void OnWindowVisibilityChanged(Window* window, bool visible) OVERRIDE;
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowTransforming(Window* window) OVERRIDE;
  virtual void OnWindowTransformed(Window* window) OVERRIDE;

  // Overridden from EnvObserver:
  virtual void OnWindowInitialized(Window* window) OVERRIDE;

  // We hold and aggregate mouse drags and touch moves as a way of throttling
  // resizes when HoldMouseMoves() is called. The following methods are used to
  // dispatch held and newly incoming mouse and touch events, typically when an
  // event other than one of these needs dispatching or a matching
  // ReleaseMouseMoves()/ReleaseTouchMoves() is called.  NOTE: because these
  // methods dispatch events from WindowTreeHost the coordinates are in terms of
  // the root.
  ui::EventDispatchDetails DispatchHeldEvents() WARN_UNUSED_RESULT;

  // Posts a task to send synthesized mouse move event if there is no a pending
  // task.
  void PostSynthesizeMouseMove();

  // Creates and dispatches synthesized mouse move event using the current mouse
  // location.
  ui::EventDispatchDetails SynthesizeMouseMoveEvent() WARN_UNUSED_RESULT;

  // Calls SynthesizeMouseMove() if |window| is currently visible and contains
  // the mouse cursor.
  void SynthesizeMouseMoveAfterChangeToWindow(Window* window);

  void PreDispatchLocatedEvent(Window* target, ui::LocatedEvent* event);
  void PreDispatchMouseEvent(Window* target, ui::MouseEvent* event);
  void PreDispatchTouchEvent(Window* target, ui::TouchEvent* event);

  WindowTreeHost* host_;

  // Touch ids that are currently down.
  uint32 touch_ids_down_;

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  Window* event_dispatch_target_;
  Window* old_dispatch_target_;

  bool synthesize_mouse_move_;

  // How many move holds are outstanding. We try to defer dispatching
  // touch/mouse moves while the count is > 0.
  int move_hold_count_;
  // The location of |held_move_event_| is in |window_|'s coordinate.
  scoped_ptr<ui::LocatedEvent> held_move_event_;

  // Allowing for reposting of events. Used when exiting context menus.
  scoped_ptr<ui::LocatedEvent> held_repostable_event_;

  // Set when dispatching a held event.
  bool dispatching_held_event_;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  // Used to schedule reposting an event.
  base::WeakPtrFactory<WindowEventDispatcher> repost_event_factory_;

  // Used to schedule DispatchHeldEvents() when |move_hold_count_| goes to 0.
  base::WeakPtrFactory<WindowEventDispatcher> held_event_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowEventDispatcher);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_EVENT_DISPATCHER_H_
