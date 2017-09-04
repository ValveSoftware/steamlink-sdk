// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/compositor_resize_lock_aura.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/common/content_switches_internal.h"
#include "content/common/input_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/scoped_tooltip_disabler.h"
#include "ui/wm/public/tooltip_client.h"
#include "ui/wm/public/window_types.h"

#if defined(OS_WIN)
#include "base/time/time.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "ui/base/win/hidden_window.h"
#include "ui/base/win/osk_display_manager.h"
#include "ui/base/win/osk_display_observer.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/gdi_util.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/base/ime/linux/text_edit_command_auralinux.h"
#include "ui/base/ime/linux/text_edit_key_bindings_delegate_auralinux.h"
#endif

using gfx::RectToSkIRect;
using gfx::SkIRectToRect;

using blink::WebInputEvent;
using blink::WebGestureEvent;
using blink::WebTouchEvent;

namespace content {

namespace {

// When accelerated compositing is enabled and a widget resize is pending,
// we delay further resizes of the UI. The following constant is the maximum
// length of time that we should delay further UI resizes while waiting for a
// resized frame from a renderer.
const int kResizeLockTimeoutMs = 67;

#if defined(OS_WIN)

// This class implements the ui::OnScreenKeyboardObserver interface
// which provides notifications about the on screen keyboard on Windows getting
// displayed or hidden in response to taps on editable fields.
// It provides functionality to request blink to scroll the input field if it
// is obscured by the on screen keyboard.
class WinScreenKeyboardObserver : public ui::OnScreenKeyboardObserver {
 public:
  WinScreenKeyboardObserver(RenderWidgetHostImpl* host,
                            const gfx::Point& location_in_screen,
                            float scale_factor,
                            aura::Window* window)
      : host_(host),
        location_in_screen_(location_in_screen),
        device_scale_factor_(scale_factor),
        window_(window) {
    host_->GetView()->SetInsets(gfx::Insets());
  }

  // base::win::OnScreenKeyboardObserver overrides.
  void OnKeyboardVisible(const gfx::Rect& keyboard_rect_pixels) override {
    gfx::Point location_in_pixels =
        gfx::ConvertPointToPixel(device_scale_factor_, location_in_screen_);

    // Restore the viewport.
    host_->GetView()->SetInsets(gfx::Insets());

    if (keyboard_rect_pixels.Contains(location_in_pixels)) {
      aura::client::ScreenPositionClient* screen_position_client =
          aura::client::GetScreenPositionClient(window_->GetRootWindow());
      if (!screen_position_client)
        return;

      DVLOG(1) << "OSK covering focus point.";
      gfx::Rect keyboard_rect =
          gfx::ConvertRectToDIP(device_scale_factor_, keyboard_rect_pixels);
      gfx::Rect bounds_in_screen = window_->GetBoundsInScreen();

      DCHECK(bounds_in_screen.bottom() > keyboard_rect.y());

      // Set the viewport of the window to be just above the on screen
      // keyboard.
      int viewport_bottom = bounds_in_screen.bottom() - keyboard_rect.y();

      // If the viewport is bigger than the view, then we cannot handle it
      // with the current approach. Moving the window above the OSK is one way.
      // That for a later patchset.
      if (viewport_bottom > bounds_in_screen.height())
        return;

      host_->GetView()->SetInsets(gfx::Insets(0, 0, viewport_bottom, 0));

      gfx::Point origin(location_in_screen_);
      screen_position_client->ConvertPointFromScreen(window_, &origin);

      // TODO(ekaramad): We should support the case where the focused node is
      // inside an OOPIF (https://crbug.com/676037).
      // We want to scroll the node into a rectangle which originates from
      // the touch point and a small offset (10) in either direction.
      gfx::Rect node_rect(origin.x(), origin.y(), 10, 10);
      host_->ScrollFocusedEditableNodeIntoRect(node_rect);
    }
  }

  void OnKeyboardHidden(const gfx::Rect& keyboard_rect_pixels) override {
    // Restore the viewport.
    host_->GetView()->SetInsets(gfx::Insets());
  }

 private:
  RenderWidgetHostImpl* host_;
  // The location in DIPs where the touch occurred.
  gfx::Point location_in_screen_;
  // The current device scale factor.
  float device_scale_factor_;

  // The content Window.
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WinScreenKeyboardObserver);
};
#endif  // defined(OS_WIN)

}  // namespace

// We need to watch for mouse events outside a Web Popup or its parent
// and dismiss the popup for certain events.
class RenderWidgetHostViewAura::EventFilterForPopupExit
    : public ui::EventHandler {
 public:
  explicit EventFilterForPopupExit(RenderWidgetHostViewAura* rwhva)
      : rwhva_(rwhva) {
    DCHECK(rwhva_);
    aura::Env::GetInstance()->AddPreTargetHandler(this);
  }

  ~EventFilterForPopupExit() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  // Overridden from ui::EventHandler
  void OnMouseEvent(ui::MouseEvent* event) override {
    rwhva_->ApplyEventFilterForPopupExit(event);
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    rwhva_->ApplyEventFilterForPopupExit(event);
  }

 private:
  RenderWidgetHostViewAura* rwhva_;

  DISALLOW_COPY_AND_ASSIGN(EventFilterForPopupExit);
};

void RenderWidgetHostViewAura::ApplyEventFilterForPopupExit(
    ui::LocatedEvent* event) {
  if (in_shutdown_ || is_fullscreen_ || !event->target())
    return;

  if (event->type() != ui::ET_MOUSE_PRESSED &&
      event->type() != ui::ET_TOUCH_PRESSED) {
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (target != window_ &&
      (!popup_parent_host_view_ ||
       target != popup_parent_host_view_->window_)) {
    // If we enter this code path it means that we did not receive any focus
    // lost notifications for the popup window. Ensure that blink is aware
    // of the fact that focus was lost for the host window by sending a Blur
    // notification. We also set a flag in the view indicating that we need
    // to force a Focus notification on the next mouse down.
    if (popup_parent_host_view_ && popup_parent_host_view_->host_) {
      popup_parent_host_view_->event_handler()
          ->set_focus_on_mouse_down_or_key_event(true);
      popup_parent_host_view_->host_->Blur();
    }
    // Note: popup_parent_host_view_ may be NULL when there are multiple
    // popup children per view. See: RenderWidgetHostViewAura::InitAsPopup().
    Shutdown();
  }
}

// We have to implement the WindowObserver interface on a separate object
// because clang doesn't like implementing multiple interfaces that have
// methods with the same name. This object is owned by the
// RenderWidgetHostViewAura.
class RenderWidgetHostViewAura::WindowObserver : public aura::WindowObserver {
 public:
  explicit WindowObserver(RenderWidgetHostViewAura* view)
      : view_(view) {
    view_->window_->AddObserver(this);
  }

  ~WindowObserver() override { view_->window_->RemoveObserver(this); }

  // Overridden from aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override {
    if (window == view_->window_)
      view_->AddedToRootWindow();
  }

  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    if (window == view_->window_)
      view_->RemovingFromRootWindow();
  }

  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    view_->ParentHierarchyChanged();
  }

 private:
  RenderWidgetHostViewAura* view_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};

// This class provides functionality to observe the ancestors of the RWHVA for
// bounds changes. This is done to snap the RWHVA window to a pixel boundary,
// which could change when the bounds relative to the root changes.
// An example where this happens is below:-
// The fast resize code path for bookmarks where in the parent of RWHVA which
// is WCV has its bounds changed before the bookmark is hidden. This results in
// the traditional bounds change notification for the WCV reporting the old
// bounds as the bookmark is still around. Observing all the ancestors of the
// RWHVA window enables us to know when the bounds of the window relative to
// root changes and allows us to snap accordingly.
class RenderWidgetHostViewAura::WindowAncestorObserver
    : public aura::WindowObserver {
 public:
  explicit WindowAncestorObserver(RenderWidgetHostViewAura* view)
      : view_(view) {
    aura::Window* parent = view_->window_->parent();
    while (parent) {
      parent->AddObserver(this);
      ancestors_.insert(parent);
      parent = parent->parent();
    }
  }

  ~WindowAncestorObserver() override {
    RemoveAncestorObservers();
  }

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override {
    DCHECK(ancestors_.find(window) != ancestors_.end());
    if (new_bounds.origin() != old_bounds.origin())
      view_->HandleParentBoundsChanged();
  }

 private:
  void RemoveAncestorObservers() {
    for (auto* ancestor : ancestors_)
      ancestor->RemoveObserver(this);
    ancestors_.clear();
  }

  RenderWidgetHostViewAura* view_;
  std::set<aura::Window*> ancestors_;

  DISALLOW_COPY_AND_ASSIGN(WindowAncestorObserver);
};

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host,
                                                   bool is_guest_view_hack)
    : host_(RenderWidgetHostImpl::From(host)),
      window_(nullptr),
      in_shutdown_(false),
      in_bounds_changed_(false),
      popup_parent_host_view_(nullptr),
      popup_child_host_view_(nullptr),
      is_loading_(false),
      has_composition_text_(false),
      begin_frame_source_(nullptr),
      needs_begin_frames_(false),
      needs_flush_input_(false),
      added_frame_observer_(false),
      cursor_visibility_state_in_renderer_(UNKNOWN),
#if defined(OS_WIN)
      legacy_render_widget_host_HWND_(nullptr),
      legacy_window_destroyed_(false),
      virtual_keyboard_requested_(false),
#endif
      has_snapped_to_boundary_(false),
      is_guest_view_hack_(is_guest_view_hack),
      device_scale_factor_(0.0f),
      last_active_widget_process_id_(ChildProcessHost::kInvalidUniqueID),
      last_active_widget_routing_id_(MSG_ROUTING_NONE),
      event_handler_(new RenderWidgetHostViewEventHandler(host_, this, this)),
      weak_ptr_factory_(this) {
  // GuestViews have two RenderWidgetHostViews and so we need to make sure
  // we don't have FrameSinkId collisions.
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  cc::FrameSinkId frame_sink_id =
      is_guest_view_hack_
          ? factory->GetContextFactory()->AllocateFrameSinkId()
          : cc::FrameSinkId(
                base::checked_cast<uint32_t>(host_->GetProcess()->GetID()),
                base::checked_cast<uint32_t>(host_->GetRoutingID()));
  delegated_frame_host_ =
      base::MakeUnique<DelegatedFrameHost>(frame_sink_id, this);

  if (!is_guest_view_hack_)
    host_->SetView(this);

  // Let the page-level input event router know about our surface ID
  // namespace for surface-based hit testing.
  if (host_->delegate() && host_->delegate()->GetInputEventRouter()) {
    host_->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }

  // We should start observing the TextInputManager for IME-related events as
  // well as monitoring its lifetime.
  if (GetTextInputManager())
    GetTextInputManager()->AddObserver(this);

  bool overscroll_enabled = base::CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kOverscrollHistoryNavigation) != "0";
  SetOverscrollControllerEnabled(overscroll_enabled);

  selection_controller_client_.reset(
      new TouchSelectionControllerClientAura(this));
  CreateSelectionController();

  RenderViewHost* rvh = RenderViewHost::From(host_);
  if (rvh) {
    // TODO(mostynb): actually use prefs.  Landing this as a separate CL
    // first to rebaseline some unreliable layout tests.
    ignore_result(rvh->GetWebkitPreferences());
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsChild(
    gfx::NativeView parent_view) {
  CreateAuraWindow();
  window_->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  window_->Init(ui::LAYER_SOLID_COLOR);
  window_->SetName("RenderWidgetHostViewAura");
  window_->layer()->SetColor(background_color_);

  if (parent_view)
    parent_view->AddChild(GetNativeView());

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  device_scale_factor_ = display.device_scale_factor();
}

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds_in_screen) {
  CreateAuraWindow();
  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewAura*>(parent_host_view);

  // TransientWindowClient may be NULL during tests.
  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  RenderWidgetHostViewAura* old_child =
      popup_parent_host_view_->popup_child_host_view_;
  if (old_child) {
    // TODO(jhorwich): Allow multiple popup_child_host_view_ per view, or
    // similar mechanism to ensure a second popup doesn't cause the first one
    // to never get a chance to filter events. See crbug.com/160589.
    DCHECK(old_child->popup_parent_host_view_ == popup_parent_host_view_);
    if (transient_window_client) {
      transient_window_client->RemoveTransientChild(
        popup_parent_host_view_->window_, old_child->window_);
    }
    old_child->popup_parent_host_view_ = NULL;
  }
  popup_parent_host_view_->SetPopupChild(this);
  window_->SetType(ui::wm::WINDOW_TYPE_MENU);
  window_->Init(ui::LAYER_SOLID_COLOR);
  window_->SetName("RenderWidgetHostViewAura");
  window_->layer()->SetColor(background_color_);

  // Setting the transient child allows for the popup to get mouse events when
  // in a system modal dialog. Do this before calling ParentWindowWithContext
  // below so that the transient parent is visible to WindowTreeClient.
  // This fixes crbug.com/328593.
  if (transient_window_client) {
    transient_window_client->AddTransientChild(
        popup_parent_host_view_->window_, window_);
  }

  aura::Window* root = popup_parent_host_view_->window_->GetRootWindow();
  aura::client::ParentWindowWithContext(window_, root, bounds_in_screen);

  SetBounds(bounds_in_screen);
  Show();
  if (NeedsMouseCapture())
    window_->SetCapture();

  event_filter_for_popup_exit_.reset(new EventFilterForPopupExit(this));

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  device_scale_factor_ = display.device_scale_factor();
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  CreateAuraWindow();
  is_fullscreen_ = true;
  window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window_->Init(ui::LAYER_SOLID_COLOR);
  window_->SetName("RenderWidgetHostViewAura");
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window_->layer()->SetColor(background_color_);

  aura::Window* parent = NULL;
  gfx::Rect bounds;
  if (reference_host_view) {
    aura::Window* reference_window =
        static_cast<RenderWidgetHostViewAura*>(reference_host_view)->window_;
    event_handler_->TrackHost(reference_window);
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(reference_window);
    parent = reference_window->GetRootWindow();
    bounds = display.bounds();
  }
  aura::client::ParentWindowWithContext(window_, parent, bounds);
  Show();
  Focus();

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  device_scale_factor_ = display.device_scale_factor();
}

RenderWidgetHost* RenderWidgetHostViewAura::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAura::Show() {
  window_->Show();

  if (!host_->is_hidden())
    return;

  bool has_saved_frame = delegated_frame_host_->HasSavedFrame();
  ui::LatencyInfo renderer_latency_info, browser_latency_info;
  if (has_saved_frame) {
    browser_latency_info.AddLatencyNumber(
        ui::TAB_SHOW_COMPONENT, host_->GetLatencyComponentId(), 0);
  } else {
    renderer_latency_info.AddLatencyNumber(
        ui::TAB_SHOW_COMPONENT, host_->GetLatencyComponentId(), 0);
  }
  host_->WasShown(renderer_latency_info);

  aura::Window* root = window_->GetRootWindow();
  if (root) {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root);
    if (cursor_client)
      NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
  }

  delegated_frame_host_->WasShown(browser_latency_info);

#if defined(OS_WIN)
  UpdateLegacyWin();
#endif
}

void RenderWidgetHostViewAura::Hide() {
  window_->Hide();

  // TODO(wjmaclean): can host_ ever be null?
  if (host_ && !host_->is_hidden()) {
    host_->WasHidden();
    delegated_frame_host_->WasHidden();

#if defined(OS_WIN)
    aura::WindowTreeHost* host = window_->GetHost();
    if (host) {
      // We reparent the legacy Chrome_RenderWidgetHostHWND window to the global
      // hidden window on the same lines as Windowed plugin windows.
      if (legacy_render_widget_host_HWND_)
        legacy_render_widget_host_HWND_->UpdateParent(ui::GetHiddenWindow());
    }
#endif
  }

#if defined(OS_WIN)
  if (legacy_render_widget_host_HWND_)
    legacy_render_widget_host_HWND_->Hide();
#endif
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  // For a SetSize operation, we don't care what coordinate system the origin
  // of the window is in, it's only important to make sure that the origin
  // remains constant after the operation.
  InternalSetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  gfx::Point relative_origin(rect.origin());

  // RenderWidgetHostViewAura::SetBounds() takes screen coordinates, but
  // Window::SetBounds() takes parent coordinates, so do the conversion here.
  aura::Window* root = window_->GetRootWindow();
  if (root) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(
          window_->parent(), &relative_origin);
    }
  }

  InternalSetBounds(gfx::Rect(relative_origin, rect.size()));
}

gfx::Vector2dF RenderWidgetHostViewAura::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

#if defined(OS_WIN)
HWND RenderWidgetHostViewAura::GetHostWindowHWND() const {
  aura::WindowTreeHost* host = window_->GetHost();
  return host ? host->GetAcceleratedWidget() : nullptr;
}
#endif

gfx::NativeViewAccessible RenderWidgetHostViewAura::GetNativeViewAccessible() {
#if defined(OS_WIN)
  aura::WindowTreeHost* host = window_->GetHost();
  if (!host)
    return static_cast<gfx::NativeViewAccessible>(NULL);
  BrowserAccessibilityManager* manager =
      host_->GetOrCreateRootBrowserAccessibilityManager();
  if (manager)
    return ToBrowserAccessibilityWin(manager->GetRoot());
#endif

  NOTIMPLEMENTED();
  return static_cast<gfx::NativeViewAccessible>(NULL);
}

ui::TextInputClient* RenderWidgetHostViewAura::GetTextInputClient() {
  return this;
}

void RenderWidgetHostViewAura::SetNeedsBeginFrames(bool needs_begin_frames) {
  needs_begin_frames_ = needs_begin_frames;
  UpdateNeedsBeginFramesInternal();
}

void RenderWidgetHostViewAura::OnSetNeedsFlushInput() {
  needs_flush_input_ = true;
  UpdateNeedsBeginFramesInternal();
}

void RenderWidgetHostViewAura::UpdateNeedsBeginFramesInternal() {
  if (!begin_frame_source_)
    return;

  bool needs_frame = needs_begin_frames_ || needs_flush_input_;
  if (needs_frame == added_frame_observer_)
    return;

  added_frame_observer_ = needs_frame;
  if (needs_frame)
    begin_frame_source_->AddObserver(this);
  else
    begin_frame_source_->RemoveObserver(this);
}

void RenderWidgetHostViewAura::OnBeginFrame(
    const cc::BeginFrameArgs& args) {
  needs_flush_input_ = false;
  host_->FlushInput();
  UpdateNeedsBeginFramesInternal();
  host_->Send(new ViewMsg_BeginFrame(host_->GetRoutingID(), args));
  last_begin_frame_args_ = args;
}

const cc::BeginFrameArgs& RenderWidgetHostViewAura::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void RenderWidgetHostViewAura::OnBeginFrameSourcePausedChanged(bool paused) {
  // Only used on Android WebView.
}

RenderFrameHostImpl* RenderWidgetHostViewAura::GetFocusedFrame() {
  RenderViewHost* rvh = RenderViewHost::From(host_);
  if (!rvh)
    return nullptr;
  FrameTreeNode* focused_frame =
      rvh->GetDelegate()->GetFrameTree()->GetFocusedFrame();
  if (!focused_frame)
    return nullptr;

  return focused_frame->current_frame_host();
}

void RenderWidgetHostViewAura::HandleParentBoundsChanged() {
  SnapToPhysicalPixelBoundary();
#if defined(OS_WIN)
  if (legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_->SetBounds(
        window_->GetBoundsInRootWindow());
  }
#endif
  if (!in_shutdown_) {
    // Send screen rects through the delegate if there is one. Not every
    // RenderWidgetHost has a delegate (for example, drop-down widgets).
    if (host_->delegate())
      host_->delegate()->SendScreenRects();
    else
      host_->SendScreenRects();
  }
}

void RenderWidgetHostViewAura::ParentHierarchyChanged() {
  ancestor_window_observer_.reset(new WindowAncestorObserver(this));
  // Snap when we receive a hierarchy changed. http://crbug.com/388908.
  HandleParentBoundsChanged();
}

void RenderWidgetHostViewAura::Focus() {
  // Make sure we have a FocusClient before attempting to Focus(). In some
  // situations we may not yet be in a valid Window hierarchy (such as reloading
  // after out of memory discarded the tab).
  aura::client::FocusClient* client = aura::client::GetFocusClient(window_);
  if (client)
    window_->Focus();
}

bool RenderWidgetHostViewAura::HasFocus() const {
  return window_->HasFocus();
}

bool RenderWidgetHostViewAura::IsSurfaceAvailableForCopy() const {
  return delegated_frame_host_->CanCopyToBitmap();
}

bool RenderWidgetHostViewAura::IsShowing() {
  return window_->IsVisible();
}

gfx::Rect RenderWidgetHostViewAura::GetViewBounds() const {
  return window_->GetBoundsInScreen();
}

void RenderWidgetHostViewAura::SetBackgroundColor(SkColor color) {
  if (color == background_color())
    return;
  RenderWidgetHostViewBase::SetBackgroundColor(color);
  bool opaque = GetBackgroundOpaque();
  host_->SetBackgroundOpaque(opaque);
  window_->layer()->SetFillsBoundsOpaquely(opaque);
  window_->layer()->SetColor(color);
}

bool RenderWidgetHostViewAura::IsMouseLocked() {
  return event_handler_->mouse_locked();
}

gfx::Size RenderWidgetHostViewAura::GetVisibleViewportSize() const {
  gfx::Rect requested_rect(GetRequestedRendererSize());
  requested_rect.Inset(insets_);
  return requested_rect.size();
}

void RenderWidgetHostViewAura::SetInsets(const gfx::Insets& insets) {
  if (insets != insets_) {
    insets_ = insets;
    host_->WasResized();
  }
}

void RenderWidgetHostViewAura::FocusedNodeTouched(
    const gfx::Point& location_dips_screen,
    bool editable) {
#if defined(OS_WIN)
  RenderViewHost* rvh = RenderViewHost::From(host_);
  if (rvh && rvh->GetDelegate())
    rvh->GetDelegate()->SetIsVirtualKeyboardRequested(editable);

  ui::OnScreenKeyboardDisplayManager* osk_display_manager =
      ui::OnScreenKeyboardDisplayManager::GetInstance();
  DCHECK(osk_display_manager);
  if (editable && host_ && host_->GetView() && host_->delegate()) {
    keyboard_observer_.reset(new WinScreenKeyboardObserver(
        host_, location_dips_screen, device_scale_factor_, window_));
    virtual_keyboard_requested_ =
        osk_display_manager->DisplayVirtualKeyboard(keyboard_observer_.get());
  } else {
    virtual_keyboard_requested_ = false;
    osk_display_manager->DismissVirtualKeyboard();
  }
#endif
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  current_cursor_.SetDisplayInfo(display);
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::RenderProcessGone(base::TerminationStatus status,
                                                 int error_code) {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewAura::Destroy() {
  // Beware, this function is not called on all destruction paths. If |window_|
  // has been created, then it will implicitly end up calling
  // ~RenderWidgetHostViewAura when |window_| is destroyed. Otherwise, The
  // destructor is invoked directly from here. So all destruction/cleanup code
  // should happen there, not here.
  in_shutdown_ = true;
  if (window_)
    delete window_;
  else
    delete this;
}

void RenderWidgetHostViewAura::SetTooltipText(
    const base::string16& tooltip_text) {
  tooltip_ = tooltip_text;
  aura::Window* root_window = window_->GetRootWindow();
  aura::client::TooltipClient* tooltip_client =
      aura::client::GetTooltipClient(root_window);
  if (tooltip_client) {
    tooltip_client->UpdateTooltip(window_);
    // Content tooltips should be visible indefinitely.
    tooltip_client->SetTooltipShownTimeout(window_, 0);
  }
}

gfx::Size RenderWidgetHostViewAura::GetRequestedRendererSize() const {
  return delegated_frame_host_->GetRequestedRendererSize();
}

void RenderWidgetHostViewAura::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  delegated_frame_host_->CopyFromCompositingSurface(
      src_subrect, dst_size, callback, preferred_color_type);
}

void RenderWidgetHostViewAura::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  delegated_frame_host_->CopyFromCompositingSurfaceToVideoFrame(
      src_subrect, target, callback);
}

bool RenderWidgetHostViewAura::CanCopyToVideoFrame() const {
  return delegated_frame_host_->CanCopyToVideoFrame();
}

void RenderWidgetHostViewAura::BeginFrameSubscription(
    std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  delegated_frame_host_->BeginFrameSubscription(std::move(subscriber));
}

void RenderWidgetHostViewAura::EndFrameSubscription() {
  delegated_frame_host_->EndFrameSubscription();
}

#if defined(OS_WIN)
bool RenderWidgetHostViewAura::UsesNativeWindowFrame() const {
  return (legacy_render_widget_host_HWND_ != NULL);
}

void RenderWidgetHostViewAura::UpdateMouseLockRegion() {
  RECT window_rect =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(window_, window_->GetBoundsInScreen())
          .ToRECT();
  ::ClipCursor(&window_rect);
}

void RenderWidgetHostViewAura::OnLegacyWindowDestroyed() {
  legacy_render_widget_host_HWND_ = NULL;
  legacy_window_destroyed_ = true;
}
#endif

void RenderWidgetHostViewAura::OnSwapCompositorFrame(
    uint32_t compositor_frame_sink_id,
    cc::CompositorFrame frame) {
  TRACE_EVENT0("content", "RenderWidgetHostViewAura::OnSwapCompositorFrame");

  // Override the background color to the current compositor background.
  // This allows us to, when navigating to a new page, transfer this color to
  // that page. This allows us to pass this background color to new views on
  // navigation.
  SetBackgroundColor(frame.metadata.root_background_color);

  last_scroll_offset_ = frame.metadata.root_scroll_offset;
  if (!frame.delegated_frame_data)
    return;

  cc::Selection<gfx::SelectionBound> selection = frame.metadata.selection;
  if (IsUseZoomForDSFEnabled()) {
    float viewportToDIPScale = 1.0f / current_device_scale_factor_;
    gfx::PointF start_edge_top = selection.start.edge_top();
    gfx::PointF start_edge_bottom = selection.start.edge_bottom();
    gfx::PointF end_edge_top = selection.end.edge_top();
    gfx::PointF end_edge_bottom = selection.end.edge_bottom();

    start_edge_top.Scale(viewportToDIPScale);
    start_edge_bottom.Scale(viewportToDIPScale);
    end_edge_top.Scale(viewportToDIPScale);
    end_edge_bottom.Scale(viewportToDIPScale);

    selection.start.SetEdge(start_edge_top, start_edge_bottom);
    selection.end.SetEdge(end_edge_top, end_edge_bottom);
  }

  delegated_frame_host_->SwapDelegatedFrame(compositor_frame_sink_id,
                                            std::move(frame));
  SelectionUpdated(selection.is_editable, selection.is_empty_text_form_control,
                   selection.start, selection.end);
}

void RenderWidgetHostViewAura::ClearCompositorFrame() {
  delegated_frame_host_->ClearDelegatedFrame();
}

void RenderWidgetHostViewAura::DidStopFlinging() {
  selection_controller_client_->OnScrollCompleted();
}

bool RenderWidgetHostViewAura::HasAcceleratedSurface(
    const gfx::Size& desired_size) {
  // Aura doesn't use GetBackingStore for accelerated pages, so it doesn't
  // matter what is returned here as GetBackingStore is the only caller of this
  // method. TODO(jbates) implement this if other Aura code needs it.
  return false;
}

gfx::Rect RenderWidgetHostViewAura::GetBoundsInRootWindow() {
  aura::Window* top_level = window_->GetToplevelWindow();
  gfx::Rect bounds(top_level->GetBoundsInScreen());

#if defined(OS_WIN)
  // TODO(zturner,iyengar): This will break when we remove support for NPAPI and
  // remove the legacy hwnd, so a better fix will need to be decided when that
  // happens.
  if (UsesNativeWindowFrame()) {
    // aura::Window doesn't take into account non-client area of native windows
    // (e.g. HWNDs), so for that case ask Windows directly what the bounds are.
    aura::WindowTreeHost* host = top_level->GetHost();
    if (!host)
      return top_level->GetBoundsInScreen();
    RECT window_rect = {0};
    HWND hwnd = host->GetAcceleratedWidget();
    ::GetWindowRect(hwnd, &window_rect);
    bounds = gfx::Rect(window_rect);

    // Maximized windows are outdented from the work area by the frame thickness
    // even though this "frame" is not painted.  This confuses code (and people)
    // that think of a maximized window as corresponding exactly to the work
    // area.  Correct for this by subtracting the frame thickness back off.
    if (::IsZoomed(hwnd)) {
      bounds.Inset(GetSystemMetrics(SM_CXSIZEFRAME),
                   GetSystemMetrics(SM_CYSIZEFRAME));

      bounds.Inset(GetSystemMetrics(SM_CXPADDEDBORDER),
                   GetSystemMetrics(SM_CXPADDEDBORDER));
    }
  }

  bounds =
      display::Screen::GetScreen()->ScreenToDIPRectInWindow(top_level, bounds);
#endif

  return bounds;
}

void RenderWidgetHostViewAura::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    InputEventAckState ack_result) {
  if (overscroll_controller_) {
    overscroll_controller_->ReceivedEventACK(
        event, (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result));
  }
}

void RenderWidgetHostViewAura::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (overscroll_controller_) {
    overscroll_controller_->ReceivedEventACK(
        event, (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result));
  }
}

void RenderWidgetHostViewAura::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
  ScopedVector<ui::TouchEvent> events;
  aura::WindowTreeHost* host = window_->GetHost();
  // |host| is NULL during tests.
  if (!host)
    return;

  // The TouchScrollStarted event is generated & consumed downstream from the
  // TouchEventQueue. So we don't expect an ACK up here.
  DCHECK(touch.event.type != blink::WebInputEvent::TouchScrollStarted);

  ui::EventResult result = (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
                               ? ui::ER_HANDLED
                               : ui::ER_UNHANDLED;

  blink::WebTouchPoint::State required_state;
  switch (touch.event.type) {
    case blink::WebInputEvent::TouchStart:
      required_state = blink::WebTouchPoint::StatePressed;
      break;
    case blink::WebInputEvent::TouchEnd:
      required_state = blink::WebTouchPoint::StateReleased;
      break;
    case blink::WebInputEvent::TouchMove:
      required_state = blink::WebTouchPoint::StateMoved;
      break;
    case blink::WebInputEvent::TouchCancel:
      required_state = blink::WebTouchPoint::StateCancelled;
      break;
    default:
      required_state = blink::WebTouchPoint::StateUndefined;
      NOTREACHED();
      break;
  }

  // Only send acks for one changed touch point.
  bool sent_ack = false;
  for (size_t i = 0; i < touch.event.touchesLength; ++i) {
    if (touch.event.touches[i].state == required_state) {
      DCHECK(!sent_ack);
      host->dispatcher()->ProcessedTouchEvent(touch.event.uniqueTouchEventId,
                                              window_, result);
      sent_ack = true;
    }
  }
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewAura::CreateSyntheticGestureTarget() {
  return std::unique_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetAura(host_));
}

InputEventAckState RenderWidgetHostViewAura::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  bool consumed = false;
  if (input_event.type == WebInputEvent::GestureFlingStart) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    // Zero-velocity touchpad flings are an Aura-specific signal that the
    // touchpad scroll has ended, and should not be forwarded to the renderer.
    if (gesture_event.sourceDevice == blink::WebGestureDeviceTouchpad &&
        !gesture_event.data.flingStart.velocityX &&
        !gesture_event.data.flingStart.velocityY) {
      consumed = true;
    }
  }

  if (overscroll_controller_)
    consumed |= overscroll_controller_->WillHandleEvent(input_event);

  // Touch events should always propagate to the renderer.
  if (WebTouchEvent::isTouchEventType(input_event.type))
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  if (consumed && input_event.type == blink::WebInputEvent::GestureFlingStart) {
    // Here we indicate that there was no consumer for this event, as
    // otherwise the fling animation system will try to run an animation
    // and will also expect a notification when the fling ends. Since
    // CrOS just uses the GestureFlingStart with zero-velocity as a means
    // of indicating that touchpad scroll has ended, we don't actually want
    // a fling animation. Note: Similar code exists in
    // RenderWidgetHostViewChildFrame::FilterInputEvent()
    return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }

  return consumed ? INPUT_EVENT_ACK_STATE_CONSUMED
                  : INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

BrowserAccessibilityManager*
RenderWidgetHostViewAura::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate, bool for_root_frame) {
  BrowserAccessibilityManager* manager = NULL;
#if defined(OS_WIN)
  manager = new BrowserAccessibilityManagerWin(
      BrowserAccessibilityManagerWin::GetEmptyDocument(), delegate);
#else
  manager = BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
#endif
  return manager;
}

gfx::AcceleratedWidget
RenderWidgetHostViewAura::AccessibilityGetAcceleratedWidget() {
#if defined(OS_WIN)
  if (legacy_render_widget_host_HWND_)
    return legacy_render_widget_host_HWND_->hwnd();
#endif
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
RenderWidgetHostViewAura::AccessibilityGetNativeViewAccessible() {
#if defined(OS_WIN)
  if (legacy_render_widget_host_HWND_)
    return legacy_render_widget_host_HWND_->window_accessible();
#endif
  return NULL;
}

bool RenderWidgetHostViewAura::LockMouse() {
  return event_handler_->LockMouse();
}

void RenderWidgetHostViewAura::UnlockMouse() {
  event_handler_->UnlockMouse();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::TextInputClient implementation:
void RenderWidgetHostViewAura::SetCompositionText(
    const ui::CompositionText& composition) {
  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return;

  // TODO(suzhe): convert both renderer_host and renderer to use
  // ui::CompositionText.
  std::vector<blink::WebCompositionUnderline> underlines;
  underlines.reserve(composition.underlines.size());
  for (std::vector<ui::CompositionUnderline>::const_iterator it =
           composition.underlines.begin();
       it != composition.underlines.end(); ++it) {
    underlines.push_back(
        blink::WebCompositionUnderline(static_cast<unsigned>(it->start_offset),
                                       static_cast<unsigned>(it->end_offset),
                                       it->color,
                                       it->thick,
                                       it->background_color));
  }

  // TODO(suzhe): due to a bug of webkit, we can't use selection range with
  // composition string. See: https://bugs.webkit.org/show_bug.cgi?id=37788
  text_input_manager_->GetActiveWidget()->ImeSetComposition(
      composition.text, underlines, gfx::Range::InvalidRange(),
      composition.selection.end(), composition.selection.end());

  has_composition_text_ = !composition.text.empty();
}

void RenderWidgetHostViewAura::ConfirmCompositionText() {
  if (text_input_manager_ && text_input_manager_->GetActiveWidget() &&
      has_composition_text_) {
    text_input_manager_->GetActiveWidget()->ImeFinishComposingText(false);
  }
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::ClearCompositionText() {
  if (text_input_manager_ && text_input_manager_->GetActiveWidget() &&
      has_composition_text_)
    text_input_manager_->GetActiveWidget()->ImeCancelComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertText(const base::string16& text) {
  DCHECK_NE(GetTextInputType(), ui::TEXT_INPUT_TYPE_NONE);

  if (text_input_manager_ && text_input_manager_->GetActiveWidget()) {
    if (text.length())
      text_input_manager_->GetActiveWidget()->ImeCommitText(
          text, gfx::Range::InvalidRange(), 0);
    else if (has_composition_text_)
      text_input_manager_->GetActiveWidget()->ImeFinishComposingText(false);
  }
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertChar(const ui::KeyEvent& event) {
  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab()) {
    popup_child_host_view_->InsertChar(event);
    return;
  }

  // Ignore character messages for VKEY_RETURN sent on CTRL+M. crbug.com/315547
  // TODO(wjmaclean): can host_ ever be null?
  if (host_ && (event_handler_->accept_return_character() ||
                event.GetCharacter() != ui::VKEY_RETURN)) {
    // Send a blink::WebInputEvent::Char event to |host_|.
    ForwardKeyboardEvent(NativeWebKeyboardEvent(event, event.GetCharacter()));
  }
}

ui::TextInputType RenderWidgetHostViewAura::GetTextInputType() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->type;
  return ui::TEXT_INPUT_TYPE_NONE;
}

ui::TextInputMode RenderWidgetHostViewAura::GetTextInputMode() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->mode;
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection RenderWidgetHostViewAura::GetTextDirection() const {
  NOTIMPLEMENTED();
  return base::i18n::UNKNOWN_DIRECTION;
}

int RenderWidgetHostViewAura::GetTextInputFlags() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->flags;
  return 0;
}

bool RenderWidgetHostViewAura::CanComposeInline() const {
  if (text_input_manager_ && text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->can_compose_inline;
  return true;
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectToScreen(
    const gfx::Rect& rect) const {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return rect;
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (!screen_position_client)
    return rect;
  screen_position_client->ConvertPointToScreen(window_, &origin);
  screen_position_client->ConvertPointToScreen(window_, &end);
  return gfx::Rect(origin.x(),
                   origin.y(),
                   end.x() - origin.x(),
                   end.y() - origin.y());
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectFromScreen(
    const gfx::Rect& rect) const {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::Window* root_window = window_->GetRootWindow();
  if (root_window) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    screen_position_client->ConvertPointFromScreen(window_, &origin);
    screen_position_client->ConvertPointFromScreen(window_, &end);
    return gfx::Rect(origin.x(),
                     origin.y(),
                     end.x() - origin.x(),
                     end.y() - origin.y());
  }

  return rect;
}

gfx::Rect RenderWidgetHostViewAura::GetCaretBounds() const {
  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return gfx::Rect();

  const TextInputManager::SelectionRegion* region =
      text_input_manager_->GetSelectionRegion();
  return ConvertRectToScreen(
      gfx::RectBetweenSelectionBounds(region->anchor, region->focus));
}

bool RenderWidgetHostViewAura::GetCompositionCharacterBounds(
    uint32_t index,
    gfx::Rect* rect) const {
  DCHECK(rect);

  if (!text_input_manager_ || !text_input_manager_->GetActiveWidget())
    return false;

  const TextInputManager::CompositionRangeInfo* composition_range_info =
      text_input_manager_->GetCompositionRangeInfo();

  if (index >= composition_range_info->character_bounds.size())
    return false;
  *rect = ConvertRectToScreen(composition_range_info->character_bounds[index]);
  return true;
}

bool RenderWidgetHostViewAura::HasCompositionText() const {
  return has_composition_text_;
}

bool RenderWidgetHostViewAura::GetTextRange(gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const TextInputManager::TextSelection* selection =
      text_input_manager_->GetTextSelection(GetFocusedWidget()->GetView());
  if (!selection)
    return false;

  range->set_start(selection->offset);
  range->set_end(selection->offset + selection->text.length());
  return true;
}

bool RenderWidgetHostViewAura::GetCompositionTextRange(
    gfx::Range* range) const {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetSelectionRange(gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const TextInputManager::TextSelection* selection =
      text_input_manager_->GetTextSelection(GetFocusedWidget()->GetView());
  if (!selection)
    return false;

  range->set_start(selection->range.start());
  range->set_end(selection->range.end());
  return true;
}

bool RenderWidgetHostViewAura::SetSelectionRange(const gfx::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::DeleteRange(const gfx::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetTextFromRange(
    const gfx::Range& range,
    base::string16* text) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const TextInputManager::TextSelection* selection =
      text_input_manager_->GetTextSelection(GetFocusedWidget()->GetView());
  if (!selection)
    return false;

  gfx::Range selection_text_range(selection->offset,
                                  selection->offset + selection->text.length());

  if (!selection_text_range.Contains(range)) {
    text->clear();
    return false;
  }
  if (selection_text_range.EqualsIgnoringDirection(range)) {
    // Avoid calling substr whose performance is low.
    *text = selection->text;
  } else {
    *text = selection->text.substr(range.GetMin() - selection->offset,
                                   range.length());
  }
  return true;
}

void RenderWidgetHostViewAura::OnInputMethodChanged() {
  // TODO(wjmaclean): can host_ ever be null?
  if (!host_)
    return;

  // TODO(suzhe): implement the newly added “locale” property of HTML DOM
  // TextEvent.
}

bool RenderWidgetHostViewAura::ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) {
  if (!GetTextInputManager() && !GetTextInputManager()->GetActiveWidget())
    return false;

  GetTextInputManager()->GetActiveWidget()->UpdateTextDirection(
      direction == base::i18n::RIGHT_TO_LEFT
          ? blink::WebTextDirectionRightToLeft
          : blink::WebTextDirectionLeftToRight);
  GetTextInputManager()->GetActiveWidget()->NotifyTextDirection();
  return true;
}

void RenderWidgetHostViewAura::ExtendSelectionAndDelete(
    size_t before, size_t after) {
  RenderFrameHostImpl* rfh = GetFocusedFrame();
  if (rfh)
    rfh->ExtendSelectionAndDelete(before, after);
}

void RenderWidgetHostViewAura::EnsureCaretInRect(const gfx::Rect& rect) {
  gfx::Rect intersected_rect(
      gfx::IntersectRects(rect, window_->GetBoundsInScreen()));

  if (intersected_rect.IsEmpty())
    return;

  host_->ScrollFocusedEditableNodeIntoRect(
      ConvertRectFromScreen(intersected_rect));
}

bool RenderWidgetHostViewAura::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  return false;
}

void RenderWidgetHostViewAura::SetTextEditCommandForNextKeyEvent(
    ui::TextEditCommand command) {}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, display::DisplayObserver implementation:

void RenderWidgetHostViewAura::OnDisplayAdded(
    const display::Display& new_display) {}

void RenderWidgetHostViewAura::OnDisplayRemoved(
    const display::Display& old_display) {}

void RenderWidgetHostViewAura::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  // The screen info should be updated regardless of the metric change.
  display::Screen* screen = display::Screen::GetScreen();
  if (display.id() == screen->GetDisplayNearestWindow(window_).id()) {
    UpdateScreenInfo(window_);
    current_cursor_.SetDisplayInfo(display);
    UpdateCursorIfOverSelf();
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

gfx::Size RenderWidgetHostViewAura::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size RenderWidgetHostViewAura::GetMaximumSize() const {
  return gfx::Size();
}

void RenderWidgetHostViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  base::AutoReset<bool> in_bounds_changed(&in_bounds_changed_, true);
  // We care about this whenever RenderWidgetHostViewAura is not owned by a
  // WebContentsViewAura since changes to the Window's bounds need to be
  // messaged to the renderer.  WebContentsViewAura invokes SetSize() or
  // SetBounds() itself.  No matter how we got here, any redundant calls are
  // harmless.
  SetSize(new_bounds.size());

  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  if (mouse_locked_)
    return ui::kCursorNone;
  return current_cursor_.GetNativeCursor();
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool RenderWidgetHostViewAura::CanFocus() {
  return popup_type_ == blink::WebPopupTypeNone;
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  host_->LostCapture();
}

void RenderWidgetHostViewAura::OnPaint(const ui::PaintContext& context) {
  NOTREACHED();
}

void RenderWidgetHostViewAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  if (!window_->GetRootWindow())
    return;

  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  if (host && host->delegate())
    host->delegate()->UpdateDeviceScaleFactor(device_scale_factor);

  device_scale_factor_ = device_scale_factor;
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  DCHECK_EQ(device_scale_factor, display.device_scale_factor());
  current_cursor_.SetDisplayInfo(display);
  SnapToPhysicalPixelBoundary();
}

void RenderWidgetHostViewAura::OnWindowDestroying(aura::Window* window) {
#if defined(OS_WIN)
  // The LegacyRenderWidgetHostHWND instance is destroyed when its window is
  // destroyed. Normally we control when that happens via the Destroy call
  // in the dtor. However there may be cases where the window is destroyed
  // by Windows, i.e. the parent window is destroyed before the
  // RenderWidgetHostViewAura instance goes away etc. To avoid that we
  // destroy the LegacyRenderWidgetHostHWND instance here.
  if (legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_->set_host(NULL);
    legacy_render_widget_host_HWND_->Destroy();
    // The Destroy call above will delete the LegacyRenderWidgetHostHWND
    // instance.
    legacy_render_widget_host_HWND_ = NULL;
  }
#endif

  // Make sure that the input method no longer references to this object before
  // this object is removed from the root window (i.e. this object loses access
  // to the input method).
  DetachFromInputMethod();

  if (overscroll_controller_)
    overscroll_controller_->Reset();
}

void RenderWidgetHostViewAura::OnWindowDestroyed(aura::Window* window) {
  // This is not called on all destruction paths (e.g. if this view was never
  // inialized properly to create the window). So the destruction/cleanup code
  // that do not depend on |window_| should happen in the destructor, not here.
  delete this;
}

void RenderWidgetHostViewAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool RenderWidgetHostViewAura::HasHitTestMask() const {
  return false;
}

void RenderWidgetHostViewAura::GetHitTestMask(gfx::Path* mask) const {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::EventHandler implementation:

void RenderWidgetHostViewAura::OnKeyEvent(ui::KeyEvent* event) {
  event_handler_->OnKeyEvent(event);
}

void RenderWidgetHostViewAura::OnMouseEvent(ui::MouseEvent* event) {
  event_handler_->OnMouseEvent(event);
}

cc::FrameSinkId RenderWidgetHostViewAura::FrameSinkIdAtPoint(
    cc::SurfaceHittestDelegate* delegate,
    const gfx::Point& point,
    gfx::Point* transformed_point) {
  DCHECK(device_scale_factor_ != 0.0f);

  // The surface hittest happens in device pixels, so we need to convert the
  // |point| from DIPs to pixels before hittesting.
  gfx::Point point_in_pixels =
      gfx::ConvertPointToPixel(device_scale_factor_, point);
  cc::SurfaceId id = delegated_frame_host_->SurfaceIdAtPoint(
      delegate, point_in_pixels, transformed_point);
  *transformed_point =
      gfx::ConvertPointToDIP(device_scale_factor_, *transformed_point);

  // It is possible that the renderer has not yet produced a surface, in which
  // case we return our current namespace.
  if (!id.is_valid())
    return GetFrameSinkId();
  return id.frame_sink_id();
}

void RenderWidgetHostViewAura::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewAura::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewAura::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardTouchEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewAura::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency) {
  host_->ForwardGestureEventWithLatencyInfo(event, latency);
}

bool RenderWidgetHostViewAura::TransformPointToLocalCoordSpace(
    const gfx::Point& point,
    const cc::SurfaceId& original_surface,
    gfx::Point* transformed_point) {
  // Transformations use physical pixels rather than DIP, so conversion
  // is necessary.
  gfx::Point point_in_pixels =
      gfx::ConvertPointToPixel(device_scale_factor_, point);
  if (!delegated_frame_host_->TransformPointToLocalCoordSpace(
          point_in_pixels, original_surface, transformed_point))
    return false;
  *transformed_point =
      gfx::ConvertPointToDIP(device_scale_factor_, *transformed_point);
  return true;
}

bool RenderWidgetHostViewAura::TransformPointToCoordSpaceForView(
    const gfx::Point& point,
    RenderWidgetHostViewBase* target_view,
    gfx::Point* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  // In TransformPointToLocalCoordSpace() there is a Point-to-Pixel conversion,
  // but it is not necessary here because the final target view is responsible
  // for converting before computing the final transform.
  return delegated_frame_host_->TransformPointToCoordSpaceForView(
      point, target_view, transformed_point);
}

void RenderWidgetHostViewAura::FocusedNodeChanged(
    bool editable,
    const gfx::Rect& node_bounds_in_screen) {
#if defined(OS_WIN)
  if (!editable && virtual_keyboard_requested_) {
    virtual_keyboard_requested_ = false;

    RenderViewHost* rvh = RenderViewHost::From(host_);
    if (rvh && rvh->GetDelegate())
      rvh->GetDelegate()->SetIsVirtualKeyboardRequested(false);

    DCHECK(ui::OnScreenKeyboardDisplayManager::GetInstance());
    ui::OnScreenKeyboardDisplayManager::GetInstance()->DismissVirtualKeyboard();
  }
#endif
}

void RenderWidgetHostViewAura::OnScrollEvent(ui::ScrollEvent* event) {
  event_handler_->OnScrollEvent(event);
}

void RenderWidgetHostViewAura::OnTouchEvent(ui::TouchEvent* event) {
  event_handler_->OnTouchEvent(event);
}

void RenderWidgetHostViewAura::OnGestureEvent(ui::GestureEvent* event) {
  event_handler_->OnGestureEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::ActivationDelegate implementation:

bool RenderWidgetHostViewAura::ShouldActivate() const {
  aura::WindowTreeHost* host = window_->GetHost();
  if (!host)
    return true;
  const ui::Event* event = host->dispatcher()->current_event();
  if (!event)
    return true;
  return is_fullscreen_;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::CursorClientObserver implementation:

void RenderWidgetHostViewAura::OnCursorVisibilityChanged(bool is_visible) {
  NotifyRendererOfCursorVisibilityState(is_visible);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::FocusChangeObserver implementation:

void RenderWidgetHostViewAura::OnWindowFocused(aura::Window* gained_focus,
                                               aura::Window* lost_focus) {
  DCHECK(window_ == gained_focus || window_ == lost_focus);
  if (window_ == gained_focus) {
    // We need to honor input bypass if the associated tab is does not want
    // input. This gives the current focused window a chance to be the text
    // input client and handle events.
    if (host_->ignore_input_events())
      return;

    host_->GotFocus();
    host_->SetActive(true);

    ui::InputMethod* input_method = GetInputMethod();
    if (input_method) {
      // Ask the system-wide IME to send all TextInputClient messages to |this|
      // object.
      input_method->SetFocusedTextInputClient(this);

      // Often the application can set focus to the view in response to a key
      // down. However, the following events shouldn't be sent to the web page.
      host_->SuppressEventsUntilKeyDown();
    }

    BrowserAccessibilityManager* manager =
        host_->GetRootBrowserAccessibilityManager();
    if (manager)
      manager->OnWindowFocused();
  } else if (window_ == lost_focus) {
    host_->SetActive(false);
    host_->Blur();

    DetachFromInputMethod();

    selection_controller_->HideAndDisallowShowingAutomatically();

    if (overscroll_controller_)
      overscroll_controller_->Cancel();

    BrowserAccessibilityManager* manager =
        host_->GetRootBrowserAccessibilityManager();
    if (manager)
      manager->OnWindowBlurred();

    // If we lose the focus while fullscreen, close the window; Pepper Flash
    // won't do it for us (unlike NPAPI Flash). However, we do not close the
    // window if we lose the focus to a window on another display.
    display::Screen* screen = display::Screen::GetScreen();
    bool focusing_other_display =
        gained_focus && screen->GetNumDisplays() > 1 &&
        (screen->GetDisplayNearestWindow(window_).id() !=
         screen->GetDisplayNearestWindow(gained_focus).id());
    if (is_fullscreen_ && !in_shutdown_ && !focusing_other_display) {
#if defined(OS_WIN)
      // On Windows, if we are switching to a non Aura Window on a different
      // screen we should not close the fullscreen window.
      if (!gained_focus) {
        POINT point = {0};
        ::GetCursorPos(&point);
        if (screen->GetDisplayNearestWindow(window_).id() !=
            screen->GetDisplayNearestPoint(gfx::Point(point)).id())
          return;
      }
#endif
      Shutdown();
      return;
    }

    // Close the child popup window if we lose focus (e.g. due to a JS alert or
    // system modal dialog). This is particularly important if
    // |popup_child_host_view_| has mouse capture.
    if (popup_child_host_view_)
      popup_child_host_view_->Shutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowTreeHostObserver implementation:

void RenderWidgetHostViewAura::OnHostMoved(const aura::WindowTreeHost* host,
                                           const gfx::Point& new_origin) {
  TRACE_EVENT1("ui", "RenderWidgetHostViewAura::OnHostMoved",
               "new_origin", new_origin.ToString());

  UpdateScreenInfo(window_);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
  // Ask the RWH to drop reference to us.
  if (!is_guest_view_hack_)
    host_->ViewDestroyed();

  selection_controller_.reset();
  selection_controller_client_.reset();

  delegated_frame_host_.reset();
  window_observer_.reset();
  if (window_) {
    if (window_->GetHost())
      window_->GetHost()->RemoveObserver(this);
    UnlockMouse();
    aura::client::SetTooltipText(window_, NULL);
    display::Screen::GetScreen()->RemoveObserver(this);

    // This call is usually no-op since |this| object is already removed from
    // the Aura root window and we don't have a way to get an input method
    // object associated with the window, but just in case.
    DetachFromInputMethod();
  }
  if (popup_parent_host_view_) {
    DCHECK(popup_parent_host_view_->popup_child_host_view_ == NULL ||
           popup_parent_host_view_->popup_child_host_view_ == this);
    popup_parent_host_view_->SetPopupChild(nullptr);
  }
  if (popup_child_host_view_) {
    DCHECK(popup_child_host_view_->popup_parent_host_view_ == NULL ||
           popup_child_host_view_->popup_parent_host_view_ == this);
    popup_child_host_view_->popup_parent_host_view_ = NULL;
  }
  event_filter_for_popup_exit_.reset();

#if defined(OS_WIN)
  // The LegacyRenderWidgetHostHWND window should have been destroyed in
  // RenderWidgetHostViewAura::OnWindowDestroying and the pointer should
  // be set to NULL.
  DCHECK(!legacy_render_widget_host_HWND_);
  if (virtual_keyboard_requested_) {
    DCHECK(keyboard_observer_.get());
    ui::OnScreenKeyboardDisplayManager* osk_display_manager =
        ui::OnScreenKeyboardDisplayManager::GetInstance();
    DCHECK(osk_display_manager);
    osk_display_manager->RemoveObserver(keyboard_observer_.get());
  }

#endif

  if (text_input_manager_)
    text_input_manager_->RemoveObserver(this);
}

void RenderWidgetHostViewAura::CreateAuraWindow() {
  DCHECK(!window_);
  window_ = new aura::Window(this);
  event_handler_->set_window(window_);
  window_observer_.reset(new WindowObserver(this));

  aura::client::SetTooltipText(window_, &tooltip_);
  aura::client::SetActivationDelegate(window_, this);
  aura::client::SetFocusChangeObserver(window_, this);
  display::Screen::GetScreen()->AddObserver(this);
}

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  if (host_->GetProcess()->FastShutdownStarted())
    return;

  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return;

  display::Screen* screen = display::Screen::GetScreen();
  DCHECK(screen);

  gfx::Point cursor_screen_point = screen->GetCursorScreenPoint();

#if !defined(OS_CHROMEOS)
  // Ignore cursor update messages if the window under the cursor is not us.
  aura::Window* window_at_screen_point = screen->GetWindowAtScreenPoint(
      cursor_screen_point);
#if defined(OS_WIN)
  // On Windows we may fail to retrieve the aura Window at the current cursor
  // position. This is because the WindowFromPoint API may return the legacy
  // window which is not associated with an aura Window. In this case we need
  // to get the aura window for the parent of the legacy window.
  if (!window_at_screen_point && legacy_render_widget_host_HWND_) {
    HWND hwnd_at_point = ::WindowFromPoint(cursor_screen_point.ToPOINT());

    if (hwnd_at_point == legacy_render_widget_host_HWND_->hwnd())
      hwnd_at_point = legacy_render_widget_host_HWND_->GetParent();

    display::win::ScreenWin* screen_win =
        static_cast<display::win::ScreenWin*>(screen);
    window_at_screen_point = screen_win->GetNativeWindowFromHWND(
        hwnd_at_point);
  }
#endif  // defined(OS_WIN)
  if (!window_at_screen_point ||
      (window_at_screen_point->GetRootWindow() != root_window)) {
    return;
  }
#endif  // !defined(OS_CHROMEOS)

  gfx::Point root_window_point = cursor_screen_point;
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(
        root_window, &root_window_point);
  }

  if (root_window->GetEventHandlerForPoint(root_window_point) != window_)
    return;

  gfx::NativeCursor cursor = current_cursor_.GetNativeCursor();
  // Do not show loading cursor when the cursor is currently hidden.
  if (is_loading_ && cursor != ui::kCursorNone)
    cursor = ui::kCursorPointer;

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    cursor_client->SetCursor(cursor);
  }
}

ui::InputMethod* RenderWidgetHostViewAura::GetInputMethod() const {
  if (!window_)
    return nullptr;
  aura::Window* root_window = window_->GetRootWindow();
  if (!root_window)
    return nullptr;
  return root_window->GetHost()->GetInputMethod();
}

void RenderWidgetHostViewAura::Shutdown() {
  if (!in_shutdown_) {
    in_shutdown_ = true;
    host_->ShutdownAndDestroyWidget(true);
  }
}

bool RenderWidgetHostViewAura::NeedsInputGrab() {
  return popup_type_ == blink::WebPopupTypePage;
}

bool RenderWidgetHostViewAura::NeedsMouseCapture() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return NeedsInputGrab();
#endif
  return false;
}

void RenderWidgetHostViewAura::SetTooltipsEnabled(bool enable) {
  if (enable) {
    tooltip_disabler_.reset();
  } else {
    tooltip_disabler_.reset(
        new aura::client::ScopedTooltipDisabler(window_->GetRootWindow()));
  }
}

void RenderWidgetHostViewAura::ShowContextMenu(
    const ContextMenuParams& params) {
  // Use RenderViewHostDelegate to get to the WebContentsViewAura, which will
  // actually show the disambiguation popup.
  RenderViewHost* rvh = RenderViewHost::From(host_);
  if (!rvh)
    return;

  RenderViewHostDelegate* delegate = rvh->GetDelegate();
  if (!delegate)
    return;

  RenderViewHostDelegateView* delegate_view = delegate->GetDelegateView();
  if (!delegate_view)
    return;
  delegate_view->ShowContextMenu(GetFocusedFrame(), params);
}

void RenderWidgetHostViewAura::NotifyRendererOfCursorVisibilityState(
    bool is_visible) {
  if (host_->is_hidden() ||
      (cursor_visibility_state_in_renderer_ == VISIBLE && is_visible) ||
      (cursor_visibility_state_in_renderer_ == NOT_VISIBLE && !is_visible))
    return;

  cursor_visibility_state_in_renderer_ = is_visible ? VISIBLE : NOT_VISIBLE;
  host_->SendCursorVisibilityState(is_visible);
}

void RenderWidgetHostViewAura::SetOverscrollControllerEnabled(bool enabled) {
  if (!enabled)
    overscroll_controller_.reset();
  else if (!overscroll_controller_)
    overscroll_controller_.reset(new OverscrollController());
}

void RenderWidgetHostViewAura::SnapToPhysicalPixelBoundary() {
  // The top left corner of our view in window coordinates might not land on a
  // device pixel boundary if we have a non-integer device scale. In that case,
  // to avoid the web contents area looking blurry we translate the web contents
  // in the +x, +y direction to land on the nearest pixel boundary. This may
  // cause the bottom and right edges to be clipped slightly, but that's ok.
#if defined(OS_CHROMEOS)
  aura::Window* snapped = window_->GetToplevelWindow();
#else
  aura::Window* snapped = window_->GetRootWindow();
#endif

  if (snapped && snapped != window_)
    ui::SnapLayerToPhysicalPixelBoundary(snapped->layer(), window_->layer());

  has_snapped_to_boundary_ = true;
}

bool RenderWidgetHostViewAura::OnShowContextMenu(
    const ContextMenuParams& params) {
#if defined(OS_WIN)
  event_handler_->SetContextMenuParams(params);
  return params.source_type != ui::MENU_SOURCE_LONG_PRESS;
#else
  return true;
#endif  // defined(OS_WIN)
}

void RenderWidgetHostViewAura::SetSelectionControllerClientForTest(
    std::unique_ptr<TouchSelectionControllerClientAura> client) {
  selection_controller_client_.swap(client);
  CreateSelectionController();
}

void RenderWidgetHostViewAura::InternalSetBounds(const gfx::Rect& rect) {
  SnapToPhysicalPixelBoundary();
  // Don't recursively call SetBounds if this bounds update is the result of
  // a Window::SetBoundsInternal call.
  if (!in_bounds_changed_)
    window_->SetBounds(rect);
  host_->WasResized();
  delegated_frame_host_->WasResized();
#if defined(OS_WIN)
  UpdateLegacyWin();

  if (mouse_locked_)
    UpdateMouseLockRegion();
#endif
}

#if defined(OS_WIN)
void RenderWidgetHostViewAura::UpdateLegacyWin() {
  if (legacy_window_destroyed_ || !GetHostWindowHWND())
    return;

  if (!legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_ =
        LegacyRenderWidgetHostHWND::Create(GetHostWindowHWND());
  }

  if (legacy_render_widget_host_HWND_) {
    legacy_render_widget_host_HWND_->set_host(this);
    legacy_render_widget_host_HWND_->UpdateParent(GetHostWindowHWND());
    legacy_render_widget_host_HWND_->SetBounds(
        window_->GetBoundsInRootWindow());
    // There are cases where the parent window is created, made visible and
    // the associated RenderWidget is also visible before the
    // LegacyRenderWidgetHostHWND instace is created. Ensure that it is shown
    // here.
    if (!host_->is_hidden())
      legacy_render_widget_host_HWND_->Show();
  }
}
#endif

void RenderWidgetHostViewAura::SchedulePaintIfNotInClip(
    const gfx::Rect& rect,
    const gfx::Rect& clip) {
  if (!clip.IsEmpty()) {
    gfx::Rect to_paint = gfx::SubtractRects(rect, clip);
    if (!to_paint.IsEmpty())
      window_->SchedulePaintInRect(to_paint);
  } else {
    window_->SchedulePaintInRect(rect);
  }
}

void RenderWidgetHostViewAura::AddedToRootWindow() {
  window_->GetHost()->AddObserver(this);
  UpdateScreenInfo(window_);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client) {
    cursor_client->AddObserver(this);
    NotifyRendererOfCursorVisibilityState(cursor_client->IsCursorVisible());
  }
  if (HasFocus()) {
    ui::InputMethod* input_method = GetInputMethod();
    if (input_method)
      input_method->SetFocusedTextInputClient(this);
  }

#if defined(OS_WIN)
  UpdateLegacyWin();
#endif

  delegated_frame_host_->SetCompositor(window_->GetHost()->compositor());
}

void RenderWidgetHostViewAura::RemovingFromRootWindow() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window_->GetRootWindow());
  if (cursor_client)
    cursor_client->RemoveObserver(this);

  DetachFromInputMethod();

  window_->GetHost()->RemoveObserver(this);
  delegated_frame_host_->ResetCompositor();

#if defined(OS_WIN)
  // Update the legacy window's parent temporarily to the hidden window. It
  // will eventually get reparented to the right root.
  if (legacy_render_widget_host_HWND_)
    legacy_render_widget_host_HWND_->UpdateParent(ui::GetHiddenWindow());
#endif
}

void RenderWidgetHostViewAura::DetachFromInputMethod() {
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);
}

void RenderWidgetHostViewAura::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  RenderWidgetHostImpl* target_host = host_;

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (host_->delegate())
    target_host = host_->delegate()->GetFocusedRenderWidgetHost(host_);
  if (!target_host)
    return;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  ui::TextEditKeyBindingsDelegateAuraLinux* keybinding_delegate =
      ui::GetTextEditKeyBindingsDelegate();
  std::vector<ui::TextEditCommandAuraLinux> commands;
  if (!event.skip_in_browser &&
      keybinding_delegate &&
      event.os_event &&
      keybinding_delegate->MatchEvent(*event.os_event, &commands)) {
    // Transform from ui/ types to content/ types.
    EditCommands edit_commands;
    for (std::vector<ui::TextEditCommandAuraLinux>::const_iterator it =
             commands.begin(); it != commands.end(); ++it) {
      edit_commands.push_back(EditCommand(it->GetCommandString(),
                                          it->argument()));
    }

    target_host->Send(new InputMsg_SetEditCommandsForNextKeyEvent(
        target_host->GetRoutingID(), edit_commands));
    target_host->ForwardKeyboardEvent(event);
    return;
  }
#endif

  target_host->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewAura::SelectionUpdated(
    bool is_editable,
    bool is_empty_text_form_control,
    const gfx::SelectionBound& start,
    const gfx::SelectionBound& end) {
  selection_controller_->OnSelectionEditable(is_editable);
  selection_controller_->OnSelectionEmpty(is_empty_text_form_control);
  selection_controller_->OnSelectionBoundsChanged(start, end);
}

void RenderWidgetHostViewAura::CreateSelectionController() {
  ui::TouchSelectionController::Config tsc_config;
  tsc_config.max_tap_duration = base::TimeDelta::FromMilliseconds(
      ui::GestureConfiguration::GetInstance()->long_press_time_in_ms());
  tsc_config.tap_slop = ui::GestureConfiguration::GetInstance()
                            ->max_touch_move_in_pixels_for_click();
  tsc_config.enable_longpress_drag_selection = false;
  selection_controller_.reset(new ui::TouchSelectionController(
      selection_controller_client_.get(), tsc_config));
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, public:

ui::Layer* RenderWidgetHostViewAura::DelegatedFrameHostGetLayer() const {
  return window_->layer();
}

bool RenderWidgetHostViewAura::DelegatedFrameHostIsVisible() const {
  return !host_->is_hidden();
}

SkColor RenderWidgetHostViewAura::DelegatedFrameHostGetGutterColor(
    SkColor color) const {
  // When making an element on the page fullscreen the element's background
  // may not match the page's, so use black as the gutter color to avoid
  // flashes of brighter colors during the transition.
  if (host_->delegate() && host_->delegate()->IsFullscreenForCurrentTab())
    return SK_ColorBLACK;
  return color;
}

gfx::Size RenderWidgetHostViewAura::DelegatedFrameHostDesiredSizeInDIP() const {
  return window_->bounds().size();
}

bool RenderWidgetHostViewAura::DelegatedFrameCanCreateResizeLock() const {
#if !defined(OS_CHROMEOS)
  // On Windows and Linux, holding pointer moves will not help throttling
  // resizes.
  // TODO(piman): on Windows we need to block (nested message loop?) the
  // WM_SIZE event. On Linux we need to throttle at the WM level using
  // _NET_WM_SYNC_REQUEST.
  return false;
#else
  if (host_->auto_resize_enabled())
    return false;
  return true;
#endif
}

std::unique_ptr<ResizeLock>
RenderWidgetHostViewAura::DelegatedFrameHostCreateResizeLock(
    bool defer_compositor_lock) {
  gfx::Size desired_size = window_->bounds().size();
  return std::unique_ptr<ResizeLock>(new CompositorResizeLock(
      window_->GetHost(), desired_size, defer_compositor_lock,
      base::TimeDelta::FromMilliseconds(kResizeLockTimeoutMs)));
}

void RenderWidgetHostViewAura::DelegatedFrameHostResizeLockWasReleased() {
  host_->WasResized();
}

void RenderWidgetHostViewAura::DelegatedFrameHostSendReclaimCompositorResources(
    int compositor_frame_sink_id,
    bool is_swap_ack,
    const cc::ReturnedResourceArray& resources) {
  host_->Send(new ViewMsg_ReclaimCompositorResources(
      host_->GetRoutingID(), compositor_frame_sink_id, is_swap_ack, resources));
}

void RenderWidgetHostViewAura::SetBeginFrameSource(
    cc::BeginFrameSource* source) {
  if (begin_frame_source_ && added_frame_observer_) {
    begin_frame_source_->RemoveObserver(this);
    added_frame_observer_ = false;
  }
  begin_frame_source_ = source;
  UpdateNeedsBeginFramesInternal();
}

bool RenderWidgetHostViewAura::IsAutoResizeEnabled() const {
  return host_->auto_resize_enabled();
}

void RenderWidgetHostViewAura::OnDidNavigateMainFrameToNewPage() {
  ui::GestureRecognizer::Get()->CancelActiveTouches(window_);
}

void RenderWidgetHostViewAura::LockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::UnlockCompositingSurface() {
  NOTIMPLEMENTED();
}

cc::FrameSinkId RenderWidgetHostViewAura::GetFrameSinkId() {
  return delegated_frame_host_->GetFrameSinkId();
}

cc::SurfaceId RenderWidgetHostViewAura::SurfaceIdForTesting() const {
  return delegated_frame_host_->SurfaceIdForTesting();
}

void RenderWidgetHostViewAura::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  DCHECK_EQ(text_input_manager_, text_input_manager);

  if (!GetInputMethod())
    return;

  if (did_update_state)
    GetInputMethod()->OnTextInputTypeChanged(this);

  const TextInputState* state = text_input_manager_->GetTextInputState();
  if (state && state->show_ime_if_needed)
    GetInputMethod()->ShowImeIfNeeded();


  if (state && state->type != ui::TEXT_INPUT_TYPE_NONE) {
    // Start monitoring the composition information if the focused node is
    // editable.
    RenderWidgetHostImpl* last_active_widget =
        text_input_manager_->GetActiveWidget();
    last_active_widget_routing_id_ = last_active_widget->GetRoutingID();
    last_active_widget_process_id_ = last_active_widget->GetProcess()->GetID();
    last_active_widget->Send(new InputMsg_RequestCompositionUpdate(
        last_active_widget->GetRoutingID(), false /* immediate request */,
        true /* monitor request */));
  } else {
    // Stop monitoring the composition information if the focused node is not
    // editable.
    RenderWidgetHostImpl* last_active_widget = RenderWidgetHostImpl::FromID(
        last_active_widget_process_id_, last_active_widget_routing_id_);
    if (last_active_widget) {
      last_active_widget->Send(new InputMsg_RequestCompositionUpdate(
          last_active_widget->GetRoutingID(), false /* immediate request */,
          false /* monitor request */));
    }
    last_active_widget_routing_id_ = MSG_ROUTING_NONE;
    last_active_widget_process_id_ = ChildProcessHost::kInvalidUniqueID;
  }
}

void RenderWidgetHostViewAura::OnImeCancelComposition(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* view) {
  // |view| is not necessarily the one corresponding to
  // TextInputManager::GetActiveWidget() as RenderWidgetHostViewAura can call
  // this method to finish any ongoing composition in response to a mouse down
  // event.
  if (GetInputMethod())
    GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::OnSelectionBoundsChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);
}

void RenderWidgetHostViewAura::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
#if defined(USE_X11) && !defined(OS_CHROMEOS)
  if (!GetTextInputManager())
    return;

  // We obtain the TextSelection from focused RWH which is obtained from the
  // frame tree. BrowserPlugin-based guests' RWH is not part of the frame tree
  // and the focused RWH will be that of the embedder which is incorrect. In
  // this case we should use TextSelection for |this| since RWHV for guest
  // forwards text selection information to its platform view.
  RenderWidgetHostViewBase* focused_view =
      is_guest_view_hack_ ? this : GetFocusedWidget()
                                       ? GetFocusedWidget()->GetView()
                                       : nullptr;

  if (!focused_view)
    return;

  base::string16 selected_text;
  if (GetTextInputManager()
          ->GetTextSelection(focused_view)
          ->GetSelectedText(&selected_text)) {
    // Set the CLIPBOARD_TYPE_SELECTION to the ui::Clipboard.
    ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_SELECTION);
    clipboard_writer.WriteText(selected_text);
  }
#endif  // defined(USE_X11) && !defined(OS_CHROMEOS)
}

void RenderWidgetHostViewAura::SetPopupChild(
    RenderWidgetHostViewAura* popup_child_host_view) {
  popup_child_host_view_ = popup_child_host_view;
  event_handler_->SetPopupChild(
      popup_child_host_view,
      popup_child_host_view ? popup_child_host_view->event_handler() : nullptr);
}

}  // namespace content
