// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"

#include "base/metrics/user_metrics_action.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/user_metrics.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/touch_selection/touch_selection_controller.h"

#if defined(OS_WIN)
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/common/context_menu_params.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#endif  // defined(OS_WIN)

namespace {

// In mouse lock mode, we need to prevent the (invisible) cursor from hitting
// the border of the view, in order to get valid movement information. However,
// forcing the cursor back to the center of the view after each mouse move
// doesn't work well. It reduces the frequency of useful mouse move messages
// significantly. Therefore, we move the cursor to the center of the view only
// if it approaches the border. |kMouseLockBorderPercentage| specifies the width
// of the border area, in percentage of the corresponding dimension.
const int kMouseLockBorderPercentage = 15;

#if defined(OS_WIN)
// A callback function for EnumThreadWindows to enumerate and dismiss
// any owned popup windows.
BOOL CALLBACK DismissOwnedPopups(HWND window, LPARAM arg) {
  const HWND toplevel_hwnd = reinterpret_cast<HWND>(arg);

  if (::IsWindowVisible(window)) {
    const HWND owner = ::GetWindow(window, GW_OWNER);
    if (toplevel_hwnd == owner) {
      ::PostMessage(window, WM_CANCELMODE, 0, 0);
    }
  }

  return TRUE;
}
#endif  // defined(OS_WIN)

gfx::Point GetScreenLocationFromEvent(const ui::LocatedEvent& event) {
  aura::Window* root =
      static_cast<aura::Window*>(event.target())->GetRootWindow();
  aura::client::ScreenPositionClient* spc =
      aura::client::GetScreenPositionClient(root);
  if (!spc)
    return event.root_location();

  gfx::Point screen_location(event.root_location());
  spc->ConvertPointToScreen(root, &screen_location);
  return screen_location;
}

bool IsFractionalScaleFactor(float scale_factor) {
  return (scale_factor - static_cast<int>(scale_factor)) > 0;
}

// We don't mark these as handled so that they're sent back to the
// DefWindowProc so it can generate WM_APPCOMMAND as necessary.
bool IsXButtonUpEvent(const ui::MouseEvent* event) {
#if defined(OS_WIN)
  switch (event->native_event().message) {
    case WM_XBUTTONUP:
    case WM_NCXBUTTONUP:
      return true;
  }
#endif
  return false;
}

// Reset unchanged touch points to StateStationary for touchmove and
// touchcancel.
void MarkUnchangedTouchPointsAsStationary(blink::WebTouchEvent* event,
                                          int changed_touch_id) {
  if (event->type == blink::WebInputEvent::TouchMove ||
      event->type == blink::WebInputEvent::TouchCancel) {
    for (size_t i = 0; i < event->touchesLength; ++i) {
      if (event->touches[i].id != changed_touch_id)
        event->touches[i].state = blink::WebTouchPoint::StateStationary;
    }
  }
}

bool NeedsInputGrab(content::RenderWidgetHostViewBase* view) {
  if (!view)
    return false;
  return view->GetPopupType() == blink::WebPopupTypePage;
}

}  // namespace

namespace content {

RenderWidgetHostViewEventHandler::Delegate::Delegate()
    : selection_controller_client_(nullptr),
      selection_controller_(nullptr),
      overscroll_controller_(nullptr) {}

RenderWidgetHostViewEventHandler::Delegate::~Delegate() {}

RenderWidgetHostViewEventHandler::RenderWidgetHostViewEventHandler(
    RenderWidgetHostImpl* host,
    RenderWidgetHostViewBase* host_view,
    Delegate* delegate)
    : accept_return_character_(false),
      disable_input_event_router_for_testing_(false),
      mouse_locked_(false),
      pinch_zoom_enabled_(content::IsPinchToZoomEnabled()),
      set_focus_on_mouse_down_or_key_event_(false),
      synthetic_move_sent_(false),
      host_(RenderWidgetHostImpl::From(host)),
      host_view_(host_view),
      popup_child_host_view_(nullptr),
      popup_child_event_handler_(nullptr),
      delegate_(delegate),
      window_(nullptr) {}

RenderWidgetHostViewEventHandler::~RenderWidgetHostViewEventHandler() {}

void RenderWidgetHostViewEventHandler::SetPopupChild(
    RenderWidgetHostViewBase* popup_child_host_view,
    ui::EventHandler* popup_child_event_handler) {
  popup_child_host_view_ = popup_child_host_view;
  popup_child_event_handler_ = popup_child_event_handler;
}

void RenderWidgetHostViewEventHandler::TrackHost(
    aura::Window* reference_window) {
  if (!reference_window)
    return;
  DCHECK(!host_tracker_);
  host_tracker_.reset(new aura::WindowTracker);
  host_tracker_->Add(reference_window);
}

#if defined(OS_WIN)
void RenderWidgetHostViewEventHandler::SetContextMenuParams(
    const ContextMenuParams& params) {
  last_context_menu_params_.reset();
  if (params.source_type == ui::MENU_SOURCE_LONG_PRESS) {
    last_context_menu_params_.reset(new ContextMenuParams);
    *last_context_menu_params_ = params;
  }
}

void RenderWidgetHostViewEventHandler::UpdateMouseLockRegion() {
  RECT window_rect =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(window_, window_->GetBoundsInScreen())
          .ToRECT();
  ::ClipCursor(&window_rect);
}
#endif

bool RenderWidgetHostViewEventHandler::LockMouse() {
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return false;

  if (mouse_locked_)
    return true;

  mouse_locked_ = true;
#if !defined(OS_WIN)
  window_->SetCapture();
#else
  UpdateMouseLockRegion();
#endif
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->HideCursor();
    cursor_client->LockCursor();
  }

  if (ShouldMoveToCenter()) {
    synthetic_move_sent_ = true;
    window_->MoveCursorTo(gfx::Rect(window_->bounds().size()).CenterPoint());
  }
  delegate_->SetTooltipsEnabled(false);
  return true;
}

void RenderWidgetHostViewEventHandler::UnlockMouse() {
  delegate_->SetTooltipsEnabled(true);

  aura::Window* root_window = window_->GetRootWindow();
  if (!mouse_locked_ || !root_window)
    return;

  mouse_locked_ = false;

  if (window_->HasCapture())
    window_->ReleaseCapture();

#if defined(OS_WIN)
  ::ClipCursor(NULL);
#endif

  // Ensure that the global mouse position is updated here to its original
  // value. If we don't do this then the synthesized mouse move which is posted
  // after the cursor is moved ends up getting a large movement delta which is
  // not what sites expect. The delta is computed in the
  // ModifyEventMovementAndCoords function.
  global_mouse_position_ = unlocked_global_mouse_position_;
  window_->MoveCursorTo(unlocked_mouse_position_);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->UnlockCursor();
    cursor_client->ShowCursor();
  }
  host_->LostMouseLock();
}

void RenderWidgetHostViewEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnKeyEvent");

  if (NeedsInputGrab(popup_child_host_view_)) {
    popup_child_event_handler_->OnKeyEvent(event);
    if (event->handled())
      return;
  }

  // We need to handle the Escape key for Pepper Flash.
  if (host_view_->is_fullscreen() && event->key_code() == ui::VKEY_ESCAPE) {
    // Focus the window we were created from.
    if (host_tracker_.get() && !host_tracker_->windows().empty()) {
      aura::Window* host = *(host_tracker_->windows().begin());
      aura::client::FocusClient* client = aura::client::GetFocusClient(host);
      if (client) {
        // Calling host->Focus() may delete |this|. We create a local observer
        // for that. In that case we exit without further access to any members.
        auto local_tracker = std::move(host_tracker_);
        local_tracker->Add(window_);
        host->Focus();
        if (!local_tracker->Contains(window_)) {
          event->SetHandled();
          return;
        }
      }
    }
    delegate_->Shutdown();
    host_tracker_.reset();
  } else {
    if (event->key_code() == ui::VKEY_RETURN) {
      // Do not forward return key release events if no press event was handled.
      if (event->type() == ui::ET_KEY_RELEASED && !accept_return_character_)
        return;
      // Accept return key character events between press and release events.
      accept_return_character_ = event->type() == ui::ET_KEY_PRESSED;
    }

    // Call SetKeyboardFocus() for not only ET_KEY_PRESSED but also
    // ET_KEY_RELEASED. If a user closed the hotdog menu with ESC key press,
    // we need to notify focus to Blink on ET_KEY_RELEASED for ESC key.
    SetKeyboardFocus();
    // We don't have to communicate with an input method here.
    NativeWebKeyboardEvent webkit_event(*event);
    delegate_->ForwardKeyboardEvent(webkit_event);
  }
  event->SetHandled();
}

void RenderWidgetHostViewEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnMouseEvent");
  ForwardMouseEventToParent(event);
  // TODO(mgiuca): Return if event->handled() returns true. This currently
  // breaks drop-down lists which means something is incorrectly setting
  // event->handled to true (http://crbug.com/577983).

  if (mouse_locked_) {
    HandleMouseEventWhileLocked(event);
    return;
  }

  // As the overscroll is handled during scroll events from the trackpad, the
  // RWHVA window is transformed by the overscroll controller. This transform
  // triggers a synthetic mouse-move event to be generated (by the aura
  // RootWindow). But this event interferes with the overscroll gesture. So,
  // ignore such synthetic mouse-move events if an overscroll gesture is in
  // progress.
  OverscrollController* overscroll_controller =
      delegate_->overscroll_controller();
  if (overscroll_controller &&
      overscroll_controller->overscroll_mode() != OVERSCROLL_NONE &&
      event->flags() & ui::EF_IS_SYNTHESIZED &&
      (event->type() == ui::ET_MOUSE_ENTERED ||
       event->type() == ui::ET_MOUSE_EXITED ||
       event->type() == ui::ET_MOUSE_MOVED)) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::ET_MOUSEWHEEL) {
#if defined(OS_WIN)
    // We get mouse wheel/scroll messages even if we are not in the foreground.
    // So here we check if we have any owned popup windows in the foreground and
    // dismiss them.
    aura::WindowTreeHost* host = window_->GetHost();
    if (host) {
      HWND parent = host->GetAcceleratedWidget();
      HWND toplevel_hwnd = ::GetAncestor(parent, GA_ROOT);
      EnumThreadWindows(GetCurrentThreadId(), DismissOwnedPopups,
                        reinterpret_cast<LPARAM>(toplevel_hwnd));
    }
#endif
    blink::WebMouseWheelEvent mouse_wheel_event =
        ui::MakeWebMouseWheelEvent(static_cast<ui::MouseWheelEvent&>(*event),
                                   base::Bind(&GetScreenLocationFromEvent));
    if (mouse_wheel_event.deltaX != 0 || mouse_wheel_event.deltaY != 0) {
      if (ShouldRouteEvent(event)) {
        host_->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
            host_view_, &mouse_wheel_event, *event->latency());
      } else {
        ProcessMouseWheelEvent(mouse_wheel_event, *event->latency());
      }
    }
  } else {
    bool is_selection_popup = NeedsInputGrab(popup_child_host_view_);
    if (CanRendererHandleEvent(event, mouse_locked_, is_selection_popup) &&
        !(event->flags() & ui::EF_FROM_TOUCH)) {
      // Confirm existing composition text on mouse press, to make sure
      // the input caret won't be moved with an ongoing composition text.
      if (event->type() == ui::ET_MOUSE_PRESSED)
        FinishImeCompositionSession();

      blink::WebMouseEvent mouse_event = ui::MakeWebMouseEvent(
          *event, base::Bind(&GetScreenLocationFromEvent));
      ModifyEventMovementAndCoords(*event, &mouse_event);
      if (ShouldRouteEvent(event)) {
        host_->delegate()->GetInputEventRouter()->RouteMouseEvent(
            host_view_, &mouse_event, *event->latency());
      } else {
        ProcessMouseEvent(mouse_event, *event->latency());
      }

      // Ensure that we get keyboard focus on mouse down as a plugin window may
      // have grabbed keyboard focus.
      if (event->type() == ui::ET_MOUSE_PRESSED)
        SetKeyboardFocus();
    }
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      window_->SetCapture();
      break;
    case ui::ET_MOUSE_RELEASED:
      if (!delegate_->NeedsMouseCapture())
        window_->ReleaseCapture();
      break;
    default:
      break;
  }

  if (!IsXButtonUpEvent(event))
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::OnScrollEvent(ui::ScrollEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnScrollEvent");

  if (event->type() == ui::ET_SCROLL) {
#if !defined(OS_WIN)
    // TODO(ananta)
    // Investigate if this is true for Windows 8 Metro ASH as well.
    if (event->finger_count() != 2)
      return;
#endif
    blink::WebGestureEvent gesture_event = ui::MakeWebGestureEventFlingCancel();
    // Coordinates need to be transferred to the fling cancel gesture only
    // for Surface-targeting to ensure that it is targeted to the correct
    // RenderWidgetHost.
    gesture_event.x = event->x();
    gesture_event.y = event->y();
    blink::WebMouseWheelEvent mouse_wheel_event = ui::MakeWebMouseWheelEvent(
        *event, base::Bind(&GetScreenLocationFromEvent));
    if (ShouldRouteEvent(event)) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &gesture_event,
          ui::LatencyInfo(ui::SourceEventType::WHEEL));
      host_->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
          host_view_, &mouse_wheel_event, *event->latency());
    } else {
      host_->ForwardGestureEvent(gesture_event);
      host_->ForwardWheelEventWithLatencyInfo(mouse_wheel_event,
                                              *event->latency());
    }
    RecordAction(base::UserMetricsAction("TrackpadScroll"));
  } else if (event->type() == ui::ET_SCROLL_FLING_START ||
             event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    blink::WebGestureEvent gesture_event = ui::MakeWebGestureEvent(
        *event, base::Bind(&GetScreenLocationFromEvent));
    if (ShouldRouteEvent(event)) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &gesture_event,
          ui::LatencyInfo(ui::SourceEventType::WHEEL));
    } else {
      host_->ForwardGestureEvent(gesture_event);
    }
    if (event->type() == ui::ET_SCROLL_FLING_START)
      RecordAction(base::UserMetricsAction("TrackpadScrollFling"));
  }

  event->SetHandled();
}

void RenderWidgetHostViewEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnTouchEvent");

  bool had_no_pointer = !pointer_state_.GetPointerCount();

  // Update the touch event first.
  if (!pointer_state_.OnTouch(*event)) {
    event->StopPropagation();
    return;
  }

  blink::WebTouchEvent touch_event;
  bool handled =
      delegate_->selection_controller()->WillHandleTouchEvent(pointer_state_);
  if (handled) {
    event->SetHandled();
  } else {
    touch_event = ui::CreateWebTouchEventFromMotionEvent(
        pointer_state_, event->may_cause_scrolling());
  }
  pointer_state_.CleanupRemovedTouchPoints(*event);

  if (handled)
    return;

  if (had_no_pointer)
    delegate_->selection_controller_client()->OnTouchDown();
  if (!pointer_state_.GetPointerCount())
    delegate_->selection_controller_client()->OnTouchUp();

  // It is important to always mark events as being handled asynchronously when
  // they are forwarded. This ensures that the current event does not get
  // processed by the gesture recognizer before events currently awaiting
  // dispatch in the touch queue.
  event->DisableSynchronousHandling();

  // Set unchanged touch point to StateStationary for touchmove and
  // touchcancel to make sure only send one ack per WebTouchEvent.
  MarkUnchangedTouchPointsAsStationary(&touch_event, event->touch_id());
  if (ShouldRouteEvent(event)) {
    host_->delegate()->GetInputEventRouter()->RouteTouchEvent(
        host_view_, &touch_event, *event->latency());
  } else {
    ProcessTouchEvent(touch_event, *event->latency());
  }
}

void RenderWidgetHostViewEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  TRACE_EVENT0("input", "RenderWidgetHostViewBase::OnGestureEvent");

  if ((event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
       event->type() == ui::ET_GESTURE_PINCH_UPDATE ||
       event->type() == ui::ET_GESTURE_PINCH_END) &&
      !pinch_zoom_enabled_) {
    event->SetHandled();
    return;
  }

  HandleGestureForTouchSelection(event);
  if (event->handled())
    return;

  // Confirm existing composition text on TAP gesture, to make sure the input
  // caret won't be moved with an ongoing composition text.
  if (event->type() == ui::ET_GESTURE_TAP)
    FinishImeCompositionSession();

  blink::WebGestureEvent gesture =
      ui::MakeWebGestureEvent(*event, base::Bind(&GetScreenLocationFromEvent));
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    // Webkit does not stop a fling-scroll on tap-down. So explicitly send an
    // event to stop any in-progress flings.
    blink::WebGestureEvent fling_cancel = gesture;
    fling_cancel.type = blink::WebInputEvent::GestureFlingCancel;
    fling_cancel.sourceDevice = blink::WebGestureDeviceTouchscreen;
    if (ShouldRouteEvent(event)) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &fling_cancel,
          ui::LatencyInfo(ui::SourceEventType::TOUCH));
    } else {
      host_->ForwardGestureEvent(fling_cancel);
    }
  }

  if (gesture.type != blink::WebInputEvent::Undefined) {
    if (ShouldRouteEvent(event)) {
      host_->delegate()->GetInputEventRouter()->RouteGestureEvent(
          host_view_, &gesture, *event->latency());
    } else {
      host_->ForwardGestureEventWithLatencyInfo(gesture, *event->latency());
    }

    if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
        event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
        event->type() == ui::ET_GESTURE_SCROLL_END) {
      RecordAction(base::UserMetricsAction("TouchscreenScroll"));
    } else if (event->type() == ui::ET_SCROLL_FLING_START) {
      RecordAction(base::UserMetricsAction("TouchscreenScrollFling"));
    }
  }

  // If a gesture is not processed by the webpage, then WebKit processes it
  // (e.g. generates synthetic mouse events).
  event->SetHandled();
}

bool RenderWidgetHostViewEventHandler::CanRendererHandleEvent(
    const ui::MouseEvent* event,
    bool mouse_locked,
    bool selection_popup) const {
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return false;

  if (event->type() == ui::ET_MOUSE_EXITED) {
    if (mouse_locked || selection_popup)
      return false;
#if defined(OS_WIN)
    // Don't forward the mouse leave message which is received when the context
    // menu is displayed by the page. This confuses the page and causes state
    // changes.
    if (host_view_->IsShowingContextMenu())
      return false;
#endif
    return true;
  }

#if defined(OS_WIN)
  // Renderer cannot handle WM_XBUTTON or NC events.
  switch (event->native_event().message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_NCMOUSELEAVE:
    case WM_NCMOUSEMOVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    case WM_NCXBUTTONDBLCLK:
      return false;
    default:
      break;
  }
#elif defined(USE_X11)
  // Renderer only supports standard mouse buttons, so ignore programmable
  // buttons.
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED: {
      const int kAllowedButtons = ui::EF_LEFT_MOUSE_BUTTON |
                                  ui::EF_MIDDLE_MOUSE_BUTTON |
                                  ui::EF_RIGHT_MOUSE_BUTTON;
      return (event->flags() & kAllowedButtons) != 0;
    }
    default:
      break;
  }
#endif
  return true;
}

void RenderWidgetHostViewEventHandler::FinishImeCompositionSession() {
  if (!host_view_->GetTextInputClient()->HasCompositionText())
    return;

  TextInputManager* text_input_manager = host_view_->GetTextInputManager();
  if (!!text_input_manager && !!text_input_manager->GetActiveWidget())
    text_input_manager->GetActiveWidget()->ImeFinishComposingText(false);
  host_view_->ImeCancelComposition();
}

void RenderWidgetHostViewEventHandler::ForwardMouseEventToParent(
    ui::MouseEvent* event) {
  // Needed to propagate mouse event to |window_->parent()->delegate()|, but
  // note that it might be something other than a WebContentsViewAura instance.
  // TODO(pkotwicz): Find a better way of doing this.
  // In fullscreen mode which is typically used by flash, don't forward
  // the mouse events to the parent. The renderer and the plugin process
  // handle these events.
  if (host_view_->is_fullscreen())
    return;

  if (event->flags() & ui::EF_FROM_TOUCH)
    return;

  if (!window_->parent() || !window_->parent()->delegate())
    return;

  // Take a copy of |event|, to avoid ConvertLocationToTarget mutating the
  // event.
  std::unique_ptr<ui::Event> event_copy = ui::Event::Clone(*event);
  ui::MouseEvent* mouse_event = static_cast<ui::MouseEvent*>(event_copy.get());
  mouse_event->ConvertLocationToTarget(window_, window_->parent());
  window_->parent()->delegate()->OnMouseEvent(mouse_event);
  if (mouse_event->handled())
    event->SetHandled();
}

void RenderWidgetHostViewEventHandler::HandleGestureForTouchSelection(
    ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      if (delegate_->selection_controller()->WillHandleLongPressEvent(
              event->time_stamp(), event->location_f())) {
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_TAP:
      if (delegate_->selection_controller()->WillHandleTapEvent(
              event->location_f(), event->details().tap_count())) {
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      delegate_->selection_controller_client()->OnScrollStarted();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      delegate_->selection_controller_client()->OnScrollCompleted();
      break;
#if defined(OS_WIN)
    case ui::ET_GESTURE_LONG_TAP: {
      if (!last_context_menu_params_)
        break;

      std::unique_ptr<ContextMenuParams> context_menu_params =
          std::move(last_context_menu_params_);

      // On Windows we want to display the context menu when the long press
      // gesture is released. To achieve that, we switch the saved context
      // menu params source type to MENU_SOURCE_TOUCH. This is to ensure that
      // the RenderWidgetHostViewBase::OnShowContextMenu function which is
      // called from the ShowContextMenu call below, does not treat it as
      // a context menu request coming in from the long press gesture.
      DCHECK(context_menu_params->source_type == ui::MENU_SOURCE_LONG_PRESS);
      context_menu_params->source_type = ui::MENU_SOURCE_TOUCH;

      delegate_->ShowContextMenu(*context_menu_params);
      event->SetHandled();
      // WARNING: we may have been deleted during the call to ShowContextMenu().
      break;
    }
#endif
    default:
      break;
  }
}

void RenderWidgetHostViewEventHandler::HandleMouseEventWhileLocked(
    ui::MouseEvent* event) {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());

  DCHECK(!cursor_client || !cursor_client->IsCursorVisible());

  if (event->type() == ui::ET_MOUSEWHEEL) {
    blink::WebMouseWheelEvent mouse_wheel_event =
        ui::MakeWebMouseWheelEvent(static_cast<ui::MouseWheelEvent&>(*event),
                                   base::Bind(&GetScreenLocationFromEvent));
    if (mouse_wheel_event.deltaX != 0 || mouse_wheel_event.deltaY != 0)
      host_->ForwardWheelEvent(mouse_wheel_event);
    return;
  }

  gfx::Point center(gfx::Rect(window_->bounds().size()).CenterPoint());

  // If we receive non client mouse messages while we are in the locked state
  // it probably means that the mouse left the borders of our window and
  // needs to be moved back to the center.
  if (event->flags() & ui::EF_IS_NON_CLIENT) {
    // TODO(jonross): ideally this would not be done for mus (crbug.com/621412)
    synthetic_move_sent_ = true;
    window_->MoveCursorTo(center);
    return;
  }

  blink::WebMouseEvent mouse_event =
      ui::MakeWebMouseEvent(*event, base::Bind(&GetScreenLocationFromEvent));

  bool is_move_to_center_event = (event->type() == ui::ET_MOUSE_MOVED ||
                                  event->type() == ui::ET_MOUSE_DRAGGED) &&
                                 mouse_event.x == center.x() &&
                                 mouse_event.y == center.y();

  // For fractional scale factors, the conversion from pixels to dip and
  // vice versa could result in off by 1 or 2 errors which hurts us because
  // we want to avoid sending the artificial move to center event to the
  // renderer. Sending the move to center to the renderer cause the cursor
  // to bounce around the center of the screen leading to the lock operation
  // not working correctly.
  // Workaround is to treat a mouse move or drag event off by at most 2 px
  // from the center as a move to center event.
  if (synthetic_move_sent_ &&
      IsFractionalScaleFactor(host_view_->current_device_scale_factor())) {
    if (event->type() == ui::ET_MOUSE_MOVED ||
        event->type() == ui::ET_MOUSE_DRAGGED) {
      if ((abs(mouse_event.x - center.x()) <= 2) &&
          (abs(mouse_event.y - center.y()) <= 2)) {
        is_move_to_center_event = true;
      }
    }
  }

  ModifyEventMovementAndCoords(*event, &mouse_event);

  bool should_not_forward = is_move_to_center_event && synthetic_move_sent_;
  if (should_not_forward) {
    synthetic_move_sent_ = false;
  } else {
    // Check if the mouse has reached the border and needs to be centered.
    if (ShouldMoveToCenter()) {
      synthetic_move_sent_ = true;
      window_->MoveCursorTo(center);
    }
    bool is_selection_popup = NeedsInputGrab(popup_child_host_view_);
    // Forward event to renderer.
    if (CanRendererHandleEvent(event, mouse_locked_, is_selection_popup) &&
        !(event->flags() & ui::EF_FROM_TOUCH)) {
      host_->ForwardMouseEvent(mouse_event);
      // Ensure that we get keyboard focus on mouse down as a plugin window
      // may have grabbed keyboard focus.
      if (event->type() == ui::ET_MOUSE_PRESSED)
        SetKeyboardFocus();
    }
  }
}

void RenderWidgetHostViewEventHandler::ModifyEventMovementAndCoords(
    const ui::MouseEvent& ui_mouse_event,
    blink::WebMouseEvent* event) {
  // If the mouse has just entered, we must report zero movementX/Y. Hence we
  // reset any global_mouse_position set previously.
  if (ui_mouse_event.type() == ui::ET_MOUSE_ENTERED ||
      ui_mouse_event.type() == ui::ET_MOUSE_EXITED) {
    global_mouse_position_.SetPoint(event->globalX, event->globalY);
  }

  // Movement is computed by taking the difference of the new cursor position
  // and the previous. Under mouse lock the cursor will be warped back to the
  // center so that we are not limited by clipping boundaries.
  // We do not measure movement as the delta from cursor to center because
  // we may receive more mouse movement events before our warp has taken
  // effect.
  event->movementX = event->globalX - global_mouse_position_.x();
  event->movementY = event->globalY - global_mouse_position_.y();

  global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Under mouse lock, coordinates of mouse are locked to what they were when
  // mouse lock was entered.
  if (mouse_locked_) {
    event->x = unlocked_mouse_position_.x();
    event->y = unlocked_mouse_position_.y();
    event->windowX = unlocked_mouse_position_.x();
    event->windowY = unlocked_mouse_position_.y();
    event->globalX = unlocked_global_mouse_position_.x();
    event->globalY = unlocked_global_mouse_position_.y();
  } else {
    unlocked_mouse_position_.SetPoint(event->x, event->y);
    unlocked_global_mouse_position_.SetPoint(event->globalX, event->globalY);
  }
}

void RenderWidgetHostViewEventHandler::SetKeyboardFocus() {
#if defined(OS_WIN)
  if (window_ && window_->delegate()->CanFocus()) {
    aura::WindowTreeHost* host = window_->GetHost();
    if (host) {
      gfx::AcceleratedWidget hwnd = host->GetAcceleratedWidget();
      if (!(::GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE))
        ::SetFocus(hwnd);
    }
  }
#endif
  // TODO(wjmaclean): can host_ ever be null?
  if (host_ && set_focus_on_mouse_down_or_key_event_) {
    set_focus_on_mouse_down_or_key_event_ = false;
    host_->Focus();
  }
}

bool RenderWidgetHostViewEventHandler::ShouldMoveToCenter() {
  gfx::Rect rect = window_->bounds();
  rect = delegate_->ConvertRectToScreen(rect);
  int border_x = rect.width() * kMouseLockBorderPercentage / 100;
  int border_y = rect.height() * kMouseLockBorderPercentage / 100;

  return global_mouse_position_.x() < rect.x() + border_x ||
         global_mouse_position_.x() > rect.right() - border_x ||
         global_mouse_position_.y() < rect.y() + border_y ||
         global_mouse_position_.y() > rect.bottom() - border_y;
}

bool RenderWidgetHostViewEventHandler::ShouldRouteEvent(
    const ui::Event* event) const {
  // We should route an event in two cases:
  // 1) Mouse events are routed only if cross-process frames are possible.
  // 2) Touch events are always routed. In the absence of a BrowserPlugin
  //    we expect the routing to always send the event to this view. If
  //    one or more BrowserPlugins are present, then the event may be targeted
  //    to one of them, or this view. This allows GuestViews to have access to
  //    them while still forcing pinch-zoom to be handled by the top-level
  //    frame. TODO(wjmaclean): At present, this doesn't work for OOPIF, but
  //    it should be a simple extension to modify RenderWidgetHostViewChildFrame
  //    in a similar manner to RenderWidgetHostViewGuest.
  bool result = host_->delegate() && host_->delegate()->GetInputEventRouter() &&
                !disable_input_event_router_for_testing_;
  // ScrollEvents get transformed into MouseWheel events, and so are treated
  // the same as mouse events for routing purposes.
  if (event->IsMouseEvent() || event->type() == ui::ET_SCROLL)
    result = result && SiteIsolationPolicy::AreCrossProcessFramesPossible();
  return result;
}

void RenderWidgetHostViewEventHandler::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewEventHandler::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewEventHandler::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardTouchEventWithLatencyInfo(event, latency);
}

}  // namespace content
