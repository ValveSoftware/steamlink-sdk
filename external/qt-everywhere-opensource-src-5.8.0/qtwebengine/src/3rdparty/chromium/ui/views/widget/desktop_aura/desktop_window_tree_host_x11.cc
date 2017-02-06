// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

#include <X11/extensions/shape.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xregion.h>
#include <X11/Xutil.h>

#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/path.h"
#include "ui/gfx/path_x11.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_delegate.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_observer_x11.h"
#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"
#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"
#include "ui/views/widget/desktop_aura/x11_pointer_grab.h"
#include "ui/views/widget/desktop_aura/x11_window_event_filter.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/window_util.h"

DECLARE_WINDOW_PROPERTY_TYPE(views::DesktopWindowTreeHostX11*);

namespace views {

DesktopWindowTreeHostX11* DesktopWindowTreeHostX11::g_current_capture =
    NULL;
std::list<XID>* DesktopWindowTreeHostX11::open_windows_ = NULL;

DEFINE_WINDOW_PROPERTY_KEY(
    aura::Window*, kViewsWindowForRootWindow, NULL);

DEFINE_WINDOW_PROPERTY_KEY(
    DesktopWindowTreeHostX11*, kHostForRootWindow, NULL);

namespace {

// Constants that are part of EWMH.
const int k_NET_WM_STATE_ADD = 1;
const int k_NET_WM_STATE_REMOVE = 0;

// Special value of the _NET_WM_DESKTOP property which indicates that the window
// should appear on all desktops.
const int kAllDesktops = 0xFFFFFFFF;

const char* kAtomsToCache[] = {
  "UTF8_STRING",
  "WM_DELETE_WINDOW",
  "WM_PROTOCOLS",
  "_NET_FRAME_EXTENTS",
  "_NET_WM_CM_S0",
  "_NET_WM_DESKTOP",
  "_NET_WM_ICON",
  "_NET_WM_NAME",
  "_NET_WM_PID",
  "_NET_WM_PING",
  "_NET_WM_STATE",
  "_NET_WM_STATE_ABOVE",
  "_NET_WM_STATE_FULLSCREEN",
  "_NET_WM_STATE_HIDDEN",
  "_NET_WM_STATE_MAXIMIZED_HORZ",
  "_NET_WM_STATE_MAXIMIZED_VERT",
  "_NET_WM_STATE_SKIP_TASKBAR",
  "_NET_WM_STATE_STICKY",
  "_NET_WM_USER_TIME",
  "_NET_WM_WINDOW_OPACITY",
  "_NET_WM_WINDOW_TYPE",
  "_NET_WM_WINDOW_TYPE_DND",
  "_NET_WM_WINDOW_TYPE_MENU",
  "_NET_WM_WINDOW_TYPE_NORMAL",
  "_NET_WM_WINDOW_TYPE_NOTIFICATION",
  "_NET_WM_WINDOW_TYPE_TOOLTIP",
  "XdndActionAsk",
  "XdndActionCopy",
  "XdndActionLink",
  "XdndActionList",
  "XdndActionMove",
  "XdndActionPrivate",
  "XdndAware",
  "XdndDrop",
  "XdndEnter",
  "XdndFinished",
  "XdndLeave",
  "XdndPosition",
  "XdndProxy",  // Proxy windows?
  "XdndSelection",
  "XdndStatus",
  "XdndTypeList",
  NULL
};

const char kX11WindowRolePopup[] = "popup";
const char kX11WindowRoleBubble[] = "bubble";

// Returns the whole path from |window| to the root.
std::vector<::Window> GetParentsList(XDisplay* xdisplay, ::Window window) {
  ::Window parent_win, root_win;
  Window* child_windows;
  unsigned int num_child_windows;
  std::vector<::Window> result;

  while (window) {
    result.push_back(window);
    if (!XQueryTree(xdisplay, window,
                    &root_win, &parent_win, &child_windows, &num_child_windows))
      break;
    if (child_windows)
      XFree(child_windows);
    window = parent_win;
  }
  return result;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, public:

DesktopWindowTreeHostX11::DesktopWindowTreeHostX11(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : xdisplay_(gfx::GetXDisplay()),
      xwindow_(0),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      atom_cache_(xdisplay_, kAtomsToCache),
      window_mapped_(false),
      wait_for_unmap_(false),
      is_fullscreen_(false),
      is_always_on_top_(false),
      use_native_frame_(false),
      should_maximize_after_map_(false),
      use_argb_visual_(false),
      drag_drop_client_(NULL),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      content_window_(NULL),
      window_parent_(NULL),
      custom_window_shape_(false),
      urgency_hint_set_(false),
      activatable_(true),
      close_widget_factory_(this) {
}

DesktopWindowTreeHostX11::~DesktopWindowTreeHostX11() {
  window()->ClearProperty(kHostForRootWindow);
  aura::client::SetWindowMoveClient(window(), NULL);
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  DestroyDispatcher();
}

// static
aura::Window* DesktopWindowTreeHostX11::GetContentWindowForXID(XID xid) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(xid);
  return host ? host->window()->GetProperty(kViewsWindowForRootWindow) : NULL;
}

// static
DesktopWindowTreeHostX11* DesktopWindowTreeHostX11::GetHostForXID(XID xid) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(xid);
  return host ? host->window()->GetProperty(kHostForRootWindow) : NULL;
}

// static
std::vector<aura::Window*> DesktopWindowTreeHostX11::GetAllOpenWindows() {
  std::vector<aura::Window*> windows(open_windows().size());
  std::transform(open_windows().begin(),
                 open_windows().end(),
                 windows.begin(),
                 GetContentWindowForXID);
  return windows;
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowBounds() const {
  return bounds_in_pixels_;
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowOuterBounds() const {
  gfx::Rect outer_bounds(bounds_in_pixels_);
  outer_bounds.Inset(-native_window_frame_borders_in_pixels_);
  return outer_bounds;
}

::Region DesktopWindowTreeHostX11::GetWindowShape() const {
  return window_shape_.get();
}

void DesktopWindowTreeHostX11::HandleNativeWidgetActivationChanged(
    bool active) {
  if (active) {
    FlashFrame(false);
    OnHostActivated();
    open_windows().remove(xwindow_);
    open_windows().insert(open_windows().begin(), xwindow_);
  } else {
    ReleaseCapture();
  }

  desktop_native_widget_aura_->HandleActivationChanged(active);

  native_widget_delegate_->AsWidget()->GetRootView()->SchedulePaint();
}

void DesktopWindowTreeHostX11::AddObserver(
    views::DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopWindowTreeHostX11::RemoveObserver(
    views::DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.RemoveObserver(observer);
}

void DesktopWindowTreeHostX11::SwapNonClientEventHandler(
    std::unique_ptr<ui::EventHandler> handler) {
  wm::CompoundEventFilter* compound_event_filter =
      desktop_native_widget_aura_->root_window_event_filter();
  if (x11_non_client_event_filter_)
    compound_event_filter->RemoveHandler(x11_non_client_event_filter_.get());
  compound_event_filter->AddHandler(handler.get());
  x11_non_client_event_filter_ = std::move(handler);
}

void DesktopWindowTreeHostX11::CleanUpWindowList(
    void (*func)(aura::Window* window)) {
  if (!open_windows_)
    return;
  while (!open_windows_->empty()) {
    XID xid = open_windows_->front();
    func(GetContentWindowForXID(xid));
    if (!open_windows_->empty() && open_windows_->front() == xid)
      open_windows_->erase(open_windows_->begin());
  }

  delete open_windows_;
  open_windows_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, DesktopWindowTreeHost implementation:

void DesktopWindowTreeHostX11::Init(aura::Window* content_window,
                                    const Widget::InitParams& params) {
  content_window_ = content_window;
  activatable_ = (params.activatable == Widget::InitParams::ACTIVATABLE_YES);

  // TODO(erg): Check whether we *should* be building a WindowTreeHost here, or
  // whether we should be proxying requests to another DRWHL.

  // In some situations, views tries to make a zero sized window, and that
  // makes us crash. Make sure we have valid sizes.
  Widget::InitParams sanitized_params = params;
  if (sanitized_params.bounds.width() == 0)
    sanitized_params.bounds.set_width(100);
  if (sanitized_params.bounds.height() == 0)
    sanitized_params.bounds.set_height(100);

  InitX11Window(sanitized_params);
}

void DesktopWindowTreeHostX11::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kViewsWindowForRootWindow, content_window_);
  window()->SetProperty(kHostForRootWindow, this);

  // Ensure that the X11DesktopHandler exists so that it dispatches activation
  // messages to us.
  X11DesktopHandler::get();

  // TODO(erg): Unify this code once the other consumer goes away.
  SwapNonClientEventHandler(
      std::unique_ptr<ui::EventHandler>(new X11WindowEventFilter(this)));
  SetUseNativeFrame(params.type == Widget::InitParams::TYPE_WINDOW &&
                    !params.remove_standard_frame);

  x11_window_move_client_.reset(new X11DesktopWindowMoveClient);
  aura::client::SetWindowMoveClient(window(), x11_window_move_client_.get());

  SetWindowTransparency();

  native_widget_delegate_->OnNativeWidgetCreated(true);
}

std::unique_ptr<corewm::Tooltip> DesktopWindowTreeHostX11::CreateTooltip() {
  return base::WrapUnique(new corewm::TooltipAura);
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostX11::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  drag_drop_client_ = new DesktopDragDropClientAuraX11(
      window(), cursor_manager, xdisplay_, xwindow_);
  drag_drop_client_->Init();
  return base::WrapUnique(drag_drop_client_);
}

void DesktopWindowTreeHostX11::Close() {
  // TODO(erg): Might need to do additional hiding tasks here.
  delayed_resize_task_.Cancel();

  if (!close_widget_factory_.HasWeakPtrs()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&DesktopWindowTreeHostX11::CloseNow,
                              close_widget_factory_.GetWeakPtr()));
  }
}

void DesktopWindowTreeHostX11::CloseNow() {
  if (xwindow_ == None)
    return;

  ReleaseCapture();
  native_widget_delegate_->OnNativeWidgetDestroying();

  // If we have children, close them. Use a copy for iteration because they'll
  // remove themselves.
  std::set<DesktopWindowTreeHostX11*> window_children_copy = window_children_;
  for (std::set<DesktopWindowTreeHostX11*>::iterator it =
           window_children_copy.begin(); it != window_children_copy.end();
       ++it) {
    (*it)->CloseNow();
  }
  DCHECK(window_children_.empty());

  // If we have a parent, remove ourselves from its children list.
  if (window_parent_) {
    window_parent_->window_children_.erase(this);
    window_parent_ = NULL;
  }

  // Remove the event listeners we've installed. We need to remove these
  // because otherwise we get assert during ~WindowEventDispatcher().
  desktop_native_widget_aura_->root_window_event_filter()->RemoveHandler(
      x11_non_client_event_filter_.get());
  x11_non_client_event_filter_.reset();

  // Destroy the compositor before destroying the |xwindow_| since shutdown
  // may try to swap, and the swap without a window causes an X error, which
  // causes a crash with in-process renderer.
  DestroyCompositor();

  open_windows().remove(xwindow_);
  // Actually free our native resources.
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  XDestroyWindow(xdisplay_, xwindow_);
  xwindow_ = None;

  desktop_native_widget_aura_->OnHostClosed();
}

aura::WindowTreeHost* DesktopWindowTreeHostX11::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostX11::ShowWindowWithState(
    ui::WindowShowState show_state) {
  if (compositor())
    compositor()->SetVisible(true);
  if (!window_mapped_)
    MapWindow(show_state);

  switch (show_state) {
    case ui::SHOW_STATE_MAXIMIZED:
      Maximize();
      break;
    case ui::SHOW_STATE_MINIMIZED:
      Minimize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      SetFullscreen(true);
      break;
    default:
      break;
  }

  // Makes the window activated by default if the state is not INACTIVE or
  // MINIMIZED.
  if (show_state != ui::SHOW_STATE_INACTIVE &&
      show_state != ui::SHOW_STATE_MINIMIZED &&
      activatable_) {
    Activate();
  }

  native_widget_delegate_->AsWidget()->SetInitialFocus(show_state);
}

void DesktopWindowTreeHostX11::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  ShowWindowWithState(ui::SHOW_STATE_MAXIMIZED);
  // Enforce |restored_bounds_in_pixels_| since calling Maximize() could have
  // reset it.
  restored_bounds_in_pixels_ = ToPixelRect(restored_bounds);
}

bool DesktopWindowTreeHostX11::IsVisible() const {
  return window_mapped_;
}

void DesktopWindowTreeHostX11::SetSize(const gfx::Size& requested_size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(requested_size)).size();
  size_in_pixels = AdjustSize(size_in_pixels);
  bool size_changed = bounds_in_pixels_.size() != size_in_pixels;
  XResizeWindow(xdisplay_, xwindow_, size_in_pixels.width(),
                size_in_pixels.height());
  bounds_in_pixels_.set_size(size_in_pixels);
  if (size_changed) {
    OnHostResized(size_in_pixels);
    ResetWindowRegion();
  }
}

void DesktopWindowTreeHostX11::StackAbove(aura::Window* window) {
  if (window && window->GetRootWindow()) {
    ::Window window_below = window->GetHost()->GetAcceleratedWidget();
    // Find all parent windows up to the root.
    std::vector<::Window> window_below_parents =
        GetParentsList(xdisplay_, window_below);
    std::vector<::Window> window_above_parents =
        GetParentsList(xdisplay_, xwindow_);

    // Find their common ancestor.
    auto it_below_window = window_below_parents.rbegin();
    auto it_above_window = window_above_parents.rbegin();
    for (; it_below_window != window_below_parents.rend() &&
           it_above_window != window_above_parents.rend() &&
           *it_below_window == *it_above_window;
         ++it_below_window, ++it_above_window) {
    }

    if (it_below_window != window_below_parents.rend() &&
        it_above_window != window_above_parents.rend()) {
      // First stack |xwindow_| below so Z-order of |window| stays the same.
      ::Window windows[] = {*it_below_window, *it_above_window};
      if (XRestackWindows(xdisplay_, windows, 2) == 0) {
        // Now stack them properly.
        std::swap(windows[0], windows[1]);
        XRestackWindows(xdisplay_, windows, 2);
      }
    }
  }
}

void DesktopWindowTreeHostX11::StackAtTop() {
  XRaiseWindow(xdisplay_, xwindow_);
}

void DesktopWindowTreeHostX11::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(size)).size();
  gfx::Rect parent_bounds_in_pixels = GetWorkAreaBoundsInPixels();

  // If |window_|'s transient parent bounds are big enough to contain |size|,
  // use them instead.
  if (wm::GetTransientParent(content_window_)) {
    gfx::Rect transient_parent_rect =
        wm::GetTransientParent(content_window_)->GetBoundsInScreen();
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width()) {
      parent_bounds_in_pixels = ToPixelRect(transient_parent_rect);
    }
  }

  gfx::Rect window_bounds_in_pixels(
      parent_bounds_in_pixels.x() +
          (parent_bounds_in_pixels.width() - size_in_pixels.width()) / 2,
      parent_bounds_in_pixels.y() +
          (parent_bounds_in_pixels.height() - size_in_pixels.height()) / 2,
      size_in_pixels.width(), size_in_pixels.height());
  // Don't size the window bigger than the parent, otherwise the user may not be
  // able to close or move it.
  window_bounds_in_pixels.AdjustToFit(parent_bounds_in_pixels);

  SetBounds(window_bounds_in_pixels);
}

void DesktopWindowTreeHostX11::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();

  if (IsFullscreen()) {
    *show_state = ui::SHOW_STATE_FULLSCREEN;
  } else if (IsMinimized()) {
    *show_state = ui::SHOW_STATE_MINIMIZED;
  } else if (IsMaximized()) {
    *show_state = ui::SHOW_STATE_MAXIMIZED;
  } else if (!IsActive()) {
    *show_state = ui::SHOW_STATE_INACTIVE;
  } else {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
}

gfx::Rect DesktopWindowTreeHostX11::GetWindowBoundsInScreen() const {
  return ToDIPRect(bounds_in_pixels_);
}

gfx::Rect DesktopWindowTreeHostX11::GetClientAreaBoundsInScreen() const {
  // TODO(erg): The NativeWidgetAura version returns |bounds_in_pixels_|,
  // claiming it's needed for View::ConvertPointToScreen() to work correctly.
  // DesktopWindowTreeHostWin::GetClientAreaBoundsInScreen() just asks windows
  // what it thinks the client rect is.
  //
  // Attempts to calculate the rect by asking the NonClientFrameView what it
  // thought its GetBoundsForClientView() were broke combobox drop down
  // placement.
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostX11::GetRestoredBounds() const {
  // We can't reliably track the restored bounds of a window, but we can get
  // the 90% case down. When *chrome* is the process that requests maximizing
  // or restoring bounds, we can record the current bounds before we request
  // maximization, and clear it when we detect a state change.
  if (!restored_bounds_in_pixels_.IsEmpty())
    return ToDIPRect(restored_bounds_in_pixels_);

  return GetWindowBoundsInScreen();
}

std::string DesktopWindowTreeHostX11::GetWorkspace() const {
  int workspace_id;
  if (ui::GetIntProperty(xwindow_, "_NET_WM_DESKTOP", &workspace_id))
    return base::IntToString(workspace_id);
  return std::string();
}

gfx::Rect DesktopWindowTreeHostX11::GetWorkAreaBoundsInScreen() const {
  return ToDIPRect(GetWorkAreaBoundsInPixels());
}

void DesktopWindowTreeHostX11::SetShape(SkRegion* native_region) {
  custom_window_shape_ = false;
  window_shape_.reset();

  if (native_region) {
    gfx::Transform transform = GetRootTransform();
    if (!transform.IsIdentity() && !native_region->isEmpty()) {
      SkPath path_in_dip;
      if (native_region->getBoundaryPath(&path_in_dip)) {
        SkPath path_in_pixels;
        path_in_dip.transform(transform.matrix(), &path_in_pixels);
        window_shape_.reset(gfx::CreateRegionFromSkPath(path_in_pixels));
      } else {
        window_shape_.reset(XCreateRegion());
      }
    } else {
      window_shape_.reset(gfx::CreateRegionFromSkRegion(*native_region));
    }

    custom_window_shape_ = true;
    delete native_region;
  }
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::Activate() {
  if (!window_mapped_)
    return;

  X11DesktopHandler::get()->ActivateWindow(xwindow_);
}

void DesktopWindowTreeHostX11::Deactivate() {
  if (!IsActive())
    return;

  ReleaseCapture();
  X11DesktopHandler::get()->DeactivateWindow(xwindow_);
}

bool DesktopWindowTreeHostX11::IsActive() const {
  return X11DesktopHandler::get()->IsActiveWindow(xwindow_);
}

void DesktopWindowTreeHostX11::Maximize() {
  if (HasWMSpecProperty("_NET_WM_STATE_FULLSCREEN")) {
    // Unfullscreen the window if it is fullscreen.
    SetWMSpecState(false,
                   atom_cache_.GetAtom("_NET_WM_STATE_FULLSCREEN"),
                   None);

    // Resize the window so that it does not have the same size as a monitor.
    // (Otherwise, some window managers immediately put the window back in
    // fullscreen mode).
    gfx::Rect adjusted_bounds_in_pixels(bounds_in_pixels_.origin(),
                                        AdjustSize(bounds_in_pixels_.size()));
    if (adjusted_bounds_in_pixels != bounds_in_pixels_)
      SetBounds(adjusted_bounds_in_pixels);
  }

  // Some WMs do not respect maximization hints on unmapped windows, so we
  // save this one for later too.
  should_maximize_after_map_ = !window_mapped_;

  // When we are in the process of requesting to maximize a window, we can
  // accurately keep track of our restored bounds instead of relying on the
  // heuristics that are in the PropertyNotify and ConfigureNotify handlers.
  restored_bounds_in_pixels_ = bounds_in_pixels_;

  SetWMSpecState(true,
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  if (IsMinimized())
    ShowWindowWithState(ui::SHOW_STATE_NORMAL);
}

void DesktopWindowTreeHostX11::Minimize() {
  ReleaseCapture();
  XIconifyWindow(xdisplay_, xwindow_, 0);
}

void DesktopWindowTreeHostX11::Restore() {
  should_maximize_after_map_ = false;
  SetWMSpecState(false,
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 atom_cache_.GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  if (IsMinimized())
    ShowWindowWithState(ui::SHOW_STATE_NORMAL);
}

bool DesktopWindowTreeHostX11::IsMaximized() const {
  return (HasWMSpecProperty("_NET_WM_STATE_MAXIMIZED_VERT") &&
          HasWMSpecProperty("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

bool DesktopWindowTreeHostX11::IsMinimized() const {
  return HasWMSpecProperty("_NET_WM_STATE_HIDDEN");
}

bool DesktopWindowTreeHostX11::HasCapture() const {
  return g_current_capture == this;
}

void DesktopWindowTreeHostX11::SetAlwaysOnTop(bool always_on_top) {
  is_always_on_top_ = always_on_top;
  SetWMSpecState(always_on_top,
                 atom_cache_.GetAtom("_NET_WM_STATE_ABOVE"),
                 None);
}

bool DesktopWindowTreeHostX11::IsAlwaysOnTop() const {
  return is_always_on_top_;
}

void DesktopWindowTreeHostX11::SetVisibleOnAllWorkspaces(bool always_visible) {
  SetWMSpecState(always_visible,
                 atom_cache_.GetAtom("_NET_WM_STATE_STICKY"),
                 None);

  int new_desktop = 0;
  if (always_visible) {
    new_desktop = kAllDesktops;
  } else {
    if (!ui::GetCurrentDesktop(&new_desktop))
      return;
  }

  XEvent xevent;
  memset (&xevent, 0, sizeof (xevent));
  xevent.type = ClientMessage;
  xevent.xclient.window = xwindow_;
  xevent.xclient.message_type = atom_cache_.GetAtom("_NET_WM_DESKTOP");
  xevent.xclient.format = 32;
  xevent.xclient.data.l[0] = new_desktop;
  xevent.xclient.data.l[1] = 0;
  xevent.xclient.data.l[2] = 0;
  xevent.xclient.data.l[3] = 0;
  xevent.xclient.data.l[4] = 0;
  XSendEvent(xdisplay_, x_root_window_, False,
             SubstructureRedirectMask | SubstructureNotifyMask,
             &xevent);
}

bool DesktopWindowTreeHostX11::SetWindowTitle(const base::string16& title) {
  if (window_title_ == title)
    return false;
  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_NAME"),
                  atom_cache_.GetAtom("UTF8_STRING"),
                  8,
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(utf8str.c_str()),
                  utf8str.size());
  XTextProperty xtp;
  char *c_utf8_str = const_cast<char *>(utf8str.c_str());
  if (Xutf8TextListToTextProperty(xdisplay_, &c_utf8_str, 1,
                                  XUTF8StringStyle, &xtp) == Success) {
    XSetWMName(xdisplay_, xwindow_, &xtp);
    XFree(xtp.value);
  }
  return true;
}

void DesktopWindowTreeHostX11::ClearNativeFocus() {
  // This method is weird and misnamed. Instead of clearing the native focus,
  // it sets the focus to our |content_window_|, which will trigger a cascade
  // of focus changes into views.
  if (content_window_ && aura::client::GetFocusClient(content_window_) &&
      content_window_->Contains(
          aura::client::GetFocusClient(content_window_)->GetFocusedWindow())) {
    aura::client::GetFocusClient(content_window_)->FocusWindow(content_window_);
  }
}

Widget::MoveLoopResult DesktopWindowTreeHostX11::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  aura::client::WindowMoveSource window_move_source =
      source == Widget::MOVE_LOOP_SOURCE_MOUSE ?
      aura::client::WINDOW_MOVE_SOURCE_MOUSE :
      aura::client::WINDOW_MOVE_SOURCE_TOUCH;
  if (x11_window_move_client_->RunMoveLoop(content_window_, drag_offset,
      window_move_source) == aura::client::MOVE_SUCCESSFUL)
    return Widget::MOVE_LOOP_SUCCESSFUL;

  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostX11::EndMoveLoop() {
  x11_window_move_client_->EndMoveLoop();
}

void DesktopWindowTreeHostX11::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  // Much like the previous NativeWidgetGtk, we don't have anything to do here.
}

bool DesktopWindowTreeHostX11::ShouldUseNativeFrame() const {
  return use_native_frame_;
}

bool DesktopWindowTreeHostX11::ShouldWindowContentsBeTransparent() const {
  return IsTranslucentWindowOpacitySupported();
}

void DesktopWindowTreeHostX11::FrameTypeChanged() {
  Widget::FrameType new_type =
      native_widget_delegate_->AsWidget()->frame_type();
  if (new_type == Widget::FRAME_TYPE_DEFAULT) {
    // The default is determined by Widget::InitParams::remove_standard_frame
    // and does not change.
    return;
  }

  SetUseNativeFrame(new_type == Widget::FRAME_TYPE_FORCE_NATIVE);
  // Replace the frame and layout the contents. Even though we don't have a
  // swapable glass frame like on Windows, we still replace the frame because
  // the button assets don't update otherwise.
  native_widget_delegate_->AsWidget()->non_client_view()->UpdateFrame();
}

void DesktopWindowTreeHostX11::SetFullscreen(bool fullscreen) {
  if (is_fullscreen_ == fullscreen)
    return;
  is_fullscreen_ = fullscreen;
  if (is_fullscreen_)
    delayed_resize_task_.Cancel();

  // Work around a bug where if we try to unfullscreen, metacity immediately
  // fullscreens us again. This is a little flickery and not necessary if
  // there's a gnome-panel, but it's not easy to detect whether there's a
  // panel or not.
  bool unmaximize_and_remaximize = !fullscreen && IsMaximized() &&
                                   ui::GuessWindowManager() == ui::WM_METACITY;

  if (unmaximize_and_remaximize)
    Restore();
  SetWMSpecState(fullscreen,
                 atom_cache_.GetAtom("_NET_WM_STATE_FULLSCREEN"),
                 None);
  if (unmaximize_and_remaximize)
    Maximize();

  // Try to guess the size we will have after the switch to/from fullscreen:
  // - (may) avoid transient states
  // - works around Flash content which expects to have the size updated
  //   synchronously.
  // See https://crbug.com/361408
  if (fullscreen) {
    restored_bounds_in_pixels_ = bounds_in_pixels_;
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window());
    bounds_in_pixels_ = ToPixelRect(display.bounds());
  } else {
    bounds_in_pixels_ = restored_bounds_in_pixels_;
  }
  OnHostMoved(bounds_in_pixels_.origin());
  OnHostResized(bounds_in_pixels_.size());

  if (HasWMSpecProperty("_NET_WM_STATE_FULLSCREEN") == fullscreen) {
    Relayout();
    ResetWindowRegion();
  }
  // Else: the widget will be relaid out either when the window bounds change or
  // when |xwindow_|'s fullscreen state changes.
}

bool DesktopWindowTreeHostX11::IsFullscreen() const {
  return is_fullscreen_;
}

void DesktopWindowTreeHostX11::SetOpacity(float opacity) {
  // X server opacity is in terms of 32 bit unsigned int space, and counts from
  // the opposite direction.
  // XChangeProperty() expects "cardinality" to be long.
  unsigned long cardinality = static_cast<int>(opacity * 255) * 0x1010101;

  if (cardinality == 0xffffffff) {
    XDeleteProperty(xdisplay_, xwindow_,
                    atom_cache_.GetAtom("_NET_WM_WINDOW_OPACITY"));
  } else {
    XChangeProperty(xdisplay_, xwindow_,
                    atom_cache_.GetAtom("_NET_WM_WINDOW_OPACITY"),
                    XA_CARDINAL, 32,
                    PropModeReplace,
                    reinterpret_cast<unsigned char*>(&cardinality), 1);
  }
}

void DesktopWindowTreeHostX11::SetWindowIcons(
    const gfx::ImageSkia& window_icon, const gfx::ImageSkia& app_icon) {
  // TODO(erg): The way we handle icons across different versions of chrome
  // could be substantially improved. The Windows version does its own thing
  // and only sometimes comes down this code path. The icon stuff in
  // ChromeViewsDelegate is hard coded to use HICONs. Likewise, we're hard
  // coded to be given two images instead of an arbitrary collection of images
  // so that we can pass to the WM.
  //
  // All of this could be made much, much better.
  std::vector<unsigned long> data;

  if (window_icon.HasRepresentation(1.0f))
    SerializeImageRepresentation(window_icon.GetRepresentation(1.0f), &data);

  if (app_icon.HasRepresentation(1.0f))
    SerializeImageRepresentation(app_icon.GetRepresentation(1.0f), &data);

  if (!data.empty())
    ui::SetAtomArrayProperty(xwindow_, "_NET_WM_ICON", "CARDINAL", data);
}

void DesktopWindowTreeHostX11::InitModalType(ui::ModalType modal_type) {
  switch (modal_type) {
    case ui::MODAL_TYPE_NONE:
      break;
    default:
      // TODO(erg): Figure out under what situations |modal_type| isn't
      // none. The comment in desktop_native_widget_aura.cc suggests that this
      // is rare.
      NOTIMPLEMENTED();
  }
}

void DesktopWindowTreeHostX11::FlashFrame(bool flash_frame) {
  if (urgency_hint_set_ == flash_frame)
    return;

  gfx::XScopedPtr<XWMHints> hints(XGetWMHints(xdisplay_, xwindow_));
  if (!hints) {
    // The window hasn't had its hints set yet.
    hints.reset(XAllocWMHints());
  }

  if (flash_frame)
    hints->flags |= XUrgencyHint;
  else
    hints->flags &= ~XUrgencyHint;

  XSetWMHints(xdisplay_, xwindow_, hints.get());

  urgency_hint_set_ = flash_frame;
}

void DesktopWindowTreeHostX11::OnRootViewLayout() {
  UpdateMinAndMaxSize();
}

void DesktopWindowTreeHostX11::OnNativeWidgetFocus() {
}

void DesktopWindowTreeHostX11::OnNativeWidgetBlur() {
}

bool DesktopWindowTreeHostX11::IsAnimatingClosed() const {
  return false;
}

bool DesktopWindowTreeHostX11::IsTranslucentWindowOpacitySupported() const {
  return use_argb_visual_;
}

void DesktopWindowTreeHostX11::SizeConstraintsChanged() {
  UpdateMinAndMaxSize();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, aura::WindowTreeHost implementation:

gfx::Transform DesktopWindowTreeHostX11::GetRootTransform() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  if (window_mapped_) {
    aura::Window* win = const_cast<aura::Window*>(window());
    display = display::Screen::GetScreen()->GetDisplayNearestWindow(win);
  }

  float scale = display.device_scale_factor();
  gfx::Transform transform;
  transform.Scale(scale, scale);
  return transform;
}

ui::EventSource* DesktopWindowTreeHostX11::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget DesktopWindowTreeHostX11::GetAcceleratedWidget() {
  return xwindow_;
}

void DesktopWindowTreeHostX11::ShowImpl() {
  ShowWindowWithState(ui::SHOW_STATE_NORMAL);
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(true);
}

void DesktopWindowTreeHostX11::HideImpl() {
  if (window_mapped_) {
    XWithdrawWindow(xdisplay_, xwindow_, 0);
    window_mapped_ = false;
    wait_for_unmap_ = true;
  }
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(false);
}

gfx::Rect DesktopWindowTreeHostX11::GetBounds() const {
  return bounds_in_pixels_;
}

void DesktopWindowTreeHostX11::SetBounds(
    const gfx::Rect& requested_bounds_in_pixel) {
  gfx::Rect bounds_in_pixels(requested_bounds_in_pixel.origin(),
                             AdjustSize(requested_bounds_in_pixel.size()));
  bool origin_changed = bounds_in_pixels_.origin() != bounds_in_pixels.origin();
  bool size_changed = bounds_in_pixels_.size() != bounds_in_pixels.size();
  XWindowChanges changes = {0};
  unsigned value_mask = 0;

  if (size_changed) {
    // Update the minimum and maximum sizes in case they have changed.
    UpdateMinAndMaxSize();

    if (bounds_in_pixels.width() < min_size_in_pixels_.width() ||
        bounds_in_pixels.height() < min_size_in_pixels_.height() ||
        (!max_size_in_pixels_.IsEmpty() &&
         (bounds_in_pixels.width() > max_size_in_pixels_.width() ||
          bounds_in_pixels.height() > max_size_in_pixels_.height()))) {
      gfx::Size size_in_pixels = bounds_in_pixels.size();
      if (!max_size_in_pixels_.IsEmpty())
        size_in_pixels.SetToMin(max_size_in_pixels_);
      size_in_pixels.SetToMax(min_size_in_pixels_);
      bounds_in_pixels.set_size(size_in_pixels);
    }

    changes.width = bounds_in_pixels.width();
    changes.height = bounds_in_pixels.height();
    value_mask |= CWHeight | CWWidth;
  }

  if (origin_changed) {
    changes.x = bounds_in_pixels.x();
    changes.y = bounds_in_pixels.y();
    value_mask |= CWX | CWY;
  }
  if (value_mask)
    XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_in_pixels_| later.
  bounds_in_pixels_ = bounds_in_pixels;

  if (origin_changed)
    native_widget_delegate_->AsWidget()->OnNativeWidgetMove();
  if (size_changed) {
    OnHostResized(bounds_in_pixels.size());
    ResetWindowRegion();
  }
}

gfx::Point DesktopWindowTreeHostX11::GetLocationOnNativeScreen() const {
  return bounds_in_pixels_.origin();
}

void DesktopWindowTreeHostX11::SetCapture() {
  if (HasCapture())
    return;

  // Grabbing the mouse is asynchronous. However, we synchronously start
  // forwarding all mouse events received by Chrome to the
  // aura::WindowEventDispatcher which has capture. This makes capture
  // synchronous for all intents and purposes if either:
  // - |g_current_capture|'s X window has capture.
  // OR
  // - The topmost window underneath the mouse is managed by Chrome.
  DesktopWindowTreeHostX11* old_capturer = g_current_capture;

  // Update |g_current_capture| prior to calling OnHostLostWindowCapture() to
  // avoid releasing pointer grab.
  g_current_capture = this;
  if (old_capturer)
    old_capturer->OnHostLostWindowCapture();

  GrabPointer(xwindow_, true, None);
}

void DesktopWindowTreeHostX11::ReleaseCapture() {
  if (g_current_capture == this) {
    // Release mouse grab asynchronously. A window managed by Chrome is likely
    // the topmost window underneath the mouse so the capture release being
    // asynchronous is likely inconsequential.
    g_current_capture = NULL;
    UngrabPointer();

    OnHostLostWindowCapture();
  }
}

void DesktopWindowTreeHostX11::SetCursorNative(gfx::NativeCursor cursor) {
  XDefineCursor(xdisplay_, xwindow_, cursor.platform());
}

void DesktopWindowTreeHostX11::MoveCursorToNative(const gfx::Point& location) {
  XWarpPointer(xdisplay_, None, x_root_window_, 0, 0, 0, 0,
               bounds_in_pixels_.x() + location.x(),
               bounds_in_pixels_.y() + location.y());
}

void DesktopWindowTreeHostX11::OnCursorVisibilityChangedNative(bool show) {
  // TODO(erg): Conditional on us enabling touch on desktop linux builds, do
  // the same tap-to-click disabling here that chromeos does.
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, private:

void DesktopWindowTreeHostX11::InitX11Window(
    const Widget::InitParams& params) {
  unsigned long attribute_mask = CWBackPixmap | CWBitGravity;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = None;
  swa.bit_gravity = NorthWestGravity;

  ::Atom window_type;
  switch (params.type) {
    case Widget::InitParams::TYPE_MENU:
      swa.override_redirect = True;
      window_type = atom_cache_.GetAtom("_NET_WM_WINDOW_TYPE_MENU");
      break;
    case Widget::InitParams::TYPE_TOOLTIP:
      swa.override_redirect = True;
      window_type = atom_cache_.GetAtom("_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case Widget::InitParams::TYPE_POPUP:
      swa.override_redirect = True;
      window_type = atom_cache_.GetAtom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case Widget::InitParams::TYPE_DRAG:
      swa.override_redirect = True;
      window_type = atom_cache_.GetAtom("_NET_WM_WINDOW_TYPE_DND");
      break;
    default:
      window_type = atom_cache_.GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");
      break;
  }
  // An in-activatable window should not interact with the system wm.
  if (!activatable_)
    swa.override_redirect = True;

  if (swa.override_redirect)
    attribute_mask |= CWOverrideRedirect;

  Visual* visual;
  int depth;
  ui::ChooseVisualForWindow(&visual, &depth);
  if (depth == 32) {
    attribute_mask |= CWColormap;
    swa.colormap =
        XCreateColormap(xdisplay_, x_root_window_, visual, AllocNone);

    // x.org will BadMatch if we don't set a border when the depth isn't the
    // same as the parent depth.
    attribute_mask |= CWBorderPixel;
    swa.border_pixel = 0;

    // A compositing manager is required to support transparency.
    use_argb_visual_ =
        XGetSelectionOwner(xdisplay_, atom_cache_.GetAtom("_NET_WM_CM_S0")) !=
        None;
  }

  bounds_in_pixels_ = ToPixelRect(params.bounds);
  bounds_in_pixels_.set_size(AdjustSize(bounds_in_pixels_.size()));
  xwindow_ = XCreateWindow(xdisplay_, x_root_window_, bounds_in_pixels_.x(),
                           bounds_in_pixels_.y(), bounds_in_pixels_.width(),
                           bounds_in_pixels_.height(),
                           0,  // border width
                           depth, InputOutput, visual, attribute_mask, &swa);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  open_windows().push_back(xwindow_);

  // TODO(erg): Maybe need to set a ViewProp here like in RWHL::RWHL().

  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask |
                    EnterWindowMask | LeaveWindowMask |
                    ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);
  XFlush(xdisplay_);

  if (ui::IsXInput2Available())
    ui::TouchFactory::GetInstance()->SetupXI2ForXWindow(xwindow_);

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  ::Atom protocols[2];
  protocols[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
  protocols[1] = atom_cache_.GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(long) >= sizeof(pid_t),
                "pid_t should not be larger than long");
  long pid = getpid();
  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_PID"),
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&pid), 1);

  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_WINDOW_TYPE"),
                  XA_ATOM,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&window_type), 1);

  // List of window state properties (_NET_WM_STATE) to set, if any.
  std::vector< ::Atom> state_atom_list;

  // Remove popup windows from taskbar unless overridden.
  if ((params.type == Widget::InitParams::TYPE_POPUP ||
       params.type == Widget::InitParams::TYPE_BUBBLE) &&
      !params.force_show_in_taskbar) {
    state_atom_list.push_back(
        atom_cache_.GetAtom("_NET_WM_STATE_SKIP_TASKBAR"));
  }

  // If the window should stay on top of other windows, add the
  // _NET_WM_STATE_ABOVE property.
  is_always_on_top_ = params.keep_on_top;
  if (is_always_on_top_)
    state_atom_list.push_back(atom_cache_.GetAtom("_NET_WM_STATE_ABOVE"));

  if (params.visible_on_all_workspaces) {
    state_atom_list.push_back(atom_cache_.GetAtom("_NET_WM_STATE_STICKY"));
    ui::SetIntProperty(xwindow_, "_NET_WM_DESKTOP", "CARDINAL", kAllDesktops);
  } else if (!params.workspace.empty()) {
    int workspace;
    if (base::StringToInt(params.workspace, &workspace))
      ui::SetIntProperty(xwindow_, "_NET_WM_DESKTOP", "CARDINAL", workspace);
  }

  // Setting _NET_WM_STATE by sending a message to the root_window (with
  // SetWMSpecState) has no effect here since the window has not yet been
  // mapped. So we manually change the state.
  if (!state_atom_list.empty()) {
    ui::SetAtomArrayProperty(xwindow_,
                             "_NET_WM_STATE",
                             "ATOM",
                             state_atom_list);
  }

  if (!params.wm_class_name.empty() || !params.wm_class_class.empty()) {
    ui::SetWindowClassHint(
        xdisplay_, xwindow_, params.wm_class_name, params.wm_class_class);
  }

  const char* wm_role_name = NULL;
  // If the widget isn't overriding the role, provide a default value for popup
  // and bubble types.
  if (!params.wm_role_name.empty()) {
    wm_role_name = params.wm_role_name.c_str();
  } else {
    switch (params.type) {
      case Widget::InitParams::TYPE_POPUP:
        wm_role_name = kX11WindowRolePopup;
        break;
      case Widget::InitParams::TYPE_BUBBLE:
        wm_role_name = kX11WindowRoleBubble;
        break;
      default:
        break;
    }
  }
  if (wm_role_name)
    ui::SetWindowRole(xdisplay_, xwindow_, std::string(wm_role_name));

  if (params.remove_standard_frame) {
    // Setting _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED tells gnome-shell to not force
    // fullscreen on the window when it matches the desktop size.
    ui::SetHideTitlebarWhenMaximizedProperty(xwindow_,
                                             ui::HIDE_TITLEBAR_WHEN_MAXIMIZED);
  }

  // If we have a parent, record the parent/child relationship. We use this
  // data during destruction to make sure that when we try to close a parent
  // window, we also destroy all child windows.
  if (params.parent && params.parent->GetHost()) {
    XID parent_xid =
        params.parent->GetHost()->GetAcceleratedWidget();
    window_parent_ = GetHostForXID(parent_xid);
    DCHECK(window_parent_);
    window_parent_->window_children_.insert(this);
  }

  // If we have a delegate which is providing a default window icon, use that
  // icon.
  gfx::ImageSkia* window_icon =
      ViewsDelegate::GetInstance()
          ? ViewsDelegate::GetInstance()->GetDefaultWindowIcon()
          : NULL;
  if (window_icon) {
    SetWindowIcons(gfx::ImageSkia(), *window_icon);
  }
  CreateCompositor();
  OnAcceleratedWidgetAvailable();
}

gfx::Size DesktopWindowTreeHostX11::AdjustSize(
    const gfx::Size& requested_size_in_pixels) {
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  // Compare against all monitor sizes. The window manager can move the window
  // to whichever monitor it wants.
  for (size_t i = 0; i < displays.size(); ++i) {
    if (requested_size_in_pixels == displays[i].GetSizeInPixel()) {
      return gfx::Size(requested_size_in_pixels.width() - 1,
                       requested_size_in_pixels.height() - 1);
    }
  }

  // Do not request a 0x0 window size. It causes an XError.
  gfx::Size size_in_pixels = requested_size_in_pixels;
  size_in_pixels.SetToMax(gfx::Size(1, 1));
  return size_in_pixels;
}

void DesktopWindowTreeHostX11::OnWMStateUpdated() {
  std::vector< ::Atom> atom_list;
  // Ignore the return value of ui::GetAtomArrayProperty(). Fluxbox removes the
  // _NET_WM_STATE property when no _NET_WM_STATE atoms are set.
  ui::GetAtomArrayProperty(xwindow_, "_NET_WM_STATE", &atom_list);

  bool was_minimized = IsMinimized();

  window_properties_.clear();
  std::copy(atom_list.begin(), atom_list.end(),
            inserter(window_properties_, window_properties_.begin()));

  // Propagate the window minimization information to the content window, so
  // the render side can update its visibility properly. OnWMStateUpdated() is
  // called by PropertyNofify event from DispatchEvent() when the browser is
  // minimized or shown from minimized state. On Windows, this is realized by
  // calling OnHostResized() with an empty size. In particular,
  // HWNDMessageHandler::GetClientAreaBounds() returns an empty size when the
  // window is minimized. On Linux, returning empty size in GetBounds() or
  // SetBounds() does not work.
  // We also propagate the minimization to the compositor, to makes sure that we
  // don't draw any 'blank' frames that could be noticed in applications such as
  // window manager previews, which show content even when a window is
  // minimized.
  bool is_minimized = IsMinimized();
  if (is_minimized != was_minimized) {
    if (is_minimized) {
      compositor()->SetVisible(false);
      content_window_->Hide();
    } else {
      content_window_->Show();
      compositor()->SetVisible(true);
    }
  }

  if (restored_bounds_in_pixels_.IsEmpty()) {
    DCHECK(!IsFullscreen());
    if (IsMaximized()) {
      // The request that we become maximized originated from a different
      // process. |bounds_in_pixels_| already contains our maximized bounds. Do
      // a best effort attempt to get restored bounds by setting it to our
      // previously set bounds (and if we get this wrong, we aren't any worse
      // off since we'd otherwise be returning our maximized bounds).
      restored_bounds_in_pixels_ = previous_bounds_in_pixels_;
    }
  } else if (!IsMaximized() && !IsFullscreen()) {
    // If we have restored bounds, but WM_STATE no longer claims to be
    // maximized or fullscreen, we should clear our restored bounds.
    restored_bounds_in_pixels_ = gfx::Rect();
  }

  // Ignore requests by the window manager to enter or exit fullscreen (e.g. as
  // a result of pressing a window manager accelerator key). Chrome does not
  // handle window manager initiated fullscreen. In particular, Chrome needs to
  // do preprocessing before the x window's fullscreen state is toggled.

  is_always_on_top_ = HasWMSpecProperty("_NET_WM_STATE_ABOVE");

  // Now that we have different window properties, we may need to relayout the
  // window. (The windows code doesn't need this because their window change is
  // synchronous.)
  Relayout();
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::OnFrameExtentsUpdated() {
  std::vector<int> insets;
  if (ui::GetIntArrayProperty(xwindow_, "_NET_FRAME_EXTENTS", &insets) &&
      insets.size() == 4) {
    // |insets| are returned in the order: [left, right, top, bottom].
    native_window_frame_borders_in_pixels_ =
        gfx::Insets(insets[2], insets[0], insets[3], insets[1]);
  } else {
    native_window_frame_borders_in_pixels_ = gfx::Insets();
  }
}

void DesktopWindowTreeHostX11::UpdateMinAndMaxSize() {
  if (!window_mapped_)
    return;

  gfx::Size minimum_in_pixels =
      ToPixelRect(gfx::Rect(native_widget_delegate_->GetMinimumSize())).size();
  gfx::Size maximum_in_pixels =
      ToPixelRect(gfx::Rect(native_widget_delegate_->GetMaximumSize())).size();
  if (min_size_in_pixels_ == minimum_in_pixels &&
      max_size_in_pixels_ == maximum_in_pixels)
    return;

  min_size_in_pixels_ = minimum_in_pixels;
  max_size_in_pixels_ = maximum_in_pixels;

  XSizeHints hints;
  long supplied_return;
  XGetWMNormalHints(xdisplay_, xwindow_, &hints, &supplied_return);

  if (minimum_in_pixels.IsEmpty()) {
    hints.flags &= ~PMinSize;
  } else {
    hints.flags |= PMinSize;
    hints.min_width = min_size_in_pixels_.width();
    hints.min_height = min_size_in_pixels_.height();
  }

  if (maximum_in_pixels.IsEmpty()) {
    hints.flags &= ~PMaxSize;
  } else {
    hints.flags |= PMaxSize;
    hints.max_width = max_size_in_pixels_.width();
    hints.max_height = max_size_in_pixels_.height();
  }

  XSetWMNormalHints(xdisplay_, xwindow_, &hints);
}

void DesktopWindowTreeHostX11::UpdateWMUserTime(
    const ui::PlatformEvent& event) {
  if (!IsActive())
    return;

  ui::EventType type = ui::EventTypeFromNative(event);
  if (type == ui::ET_MOUSE_PRESSED ||
      type == ui::ET_KEY_PRESSED ||
      type == ui::ET_TOUCH_PRESSED) {
    unsigned long wm_user_time_ms = static_cast<unsigned long>(
        (ui::EventTimeFromNative(event) - base::TimeTicks()).InMilliseconds());
    XChangeProperty(xdisplay_,
                    xwindow_,
                    atom_cache_.GetAtom("_NET_WM_USER_TIME"),
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char *>(&wm_user_time_ms),
                    1);
    X11DesktopHandler::get()->set_wm_user_time_ms(wm_user_time_ms);
  }
}

void DesktopWindowTreeHostX11::SetWMSpecState(bool enabled,
                                              ::Atom state1,
                                              ::Atom state2) {
  XEvent xclient;
  memset(&xclient, 0, sizeof(xclient));
  xclient.type = ClientMessage;
  xclient.xclient.window = xwindow_;
  xclient.xclient.message_type = atom_cache_.GetAtom("_NET_WM_STATE");
  xclient.xclient.format = 32;
  xclient.xclient.data.l[0] =
      enabled ? k_NET_WM_STATE_ADD : k_NET_WM_STATE_REMOVE;
  xclient.xclient.data.l[1] = state1;
  xclient.xclient.data.l[2] = state2;
  xclient.xclient.data.l[3] = 1;
  xclient.xclient.data.l[4] = 0;

  XSendEvent(xdisplay_, x_root_window_, False,
             SubstructureRedirectMask | SubstructureNotifyMask,
             &xclient);
}

bool DesktopWindowTreeHostX11::HasWMSpecProperty(const char* property) const {
  return window_properties_.find(atom_cache_.GetAtom(property)) !=
      window_properties_.end();
}

void DesktopWindowTreeHostX11::SetUseNativeFrame(bool use_native_frame) {
  use_native_frame_ = use_native_frame;
  ui::SetUseOSWindowFrame(xwindow_, use_native_frame);
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::DispatchMouseEvent(ui::MouseEvent* event) {
  // In Windows, the native events sent to chrome are separated into client
  // and non-client versions of events, which we record on our LocatedEvent
  // structures. On X11, we emulate the concept of non-client. Before we pass
  // this event to the cross platform event handling framework, we need to
  // make sure it is appropriately marked as non-client if it's in the non
  // client area, or otherwise, we can get into a state where the a window is
  // set as the |mouse_pressed_handler_| in window_event_dispatcher.cc
  // despite the mouse button being released.
  //
  // We can't do this later in the dispatch process because we share that
  // with ash, and ash gets confused about event IS_NON_CLIENT-ness on
  // events, since ash doesn't expect this bit to be set, because it's never
  // been set before. (This works on ash on Windows because none of the mouse
  // events on the ash desktop are clicking in what Windows considers to be a
  // non client area.) Likewise, we won't want to do the following in any
  // WindowTreeHost that hosts ash.
  if (content_window_ && content_window_->delegate()) {
    int flags = event->flags();
    int hit_test_code =
        content_window_->delegate()->GetNonClientComponent(event->location());
    if (hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE)
      flags |= ui::EF_IS_NON_CLIENT;
    event->set_flags(flags);
  }

  // While we unset the urgency hint when we gain focus, we also must remove it
  // on mouse clicks because we can call FlashFrame() on an active window.
  if (event->IsAnyButton() || event->IsMouseWheelEvent())
    FlashFrame(false);

  if (!g_current_capture || g_current_capture == this) {
    SendEventToProcessor(event);
  } else {
    // Another DesktopWindowTreeHostX11 has installed itself as
    // capture. Translate the event's location and dispatch to the other.
    ConvertEventToDifferentHost(event, g_current_capture);
    g_current_capture->SendEventToProcessor(event);
  }
}

void DesktopWindowTreeHostX11::DispatchTouchEvent(ui::TouchEvent* event) {
  if (g_current_capture && g_current_capture != this &&
      event->type() == ui::ET_TOUCH_PRESSED) {
    ConvertEventToDifferentHost(event, g_current_capture);
    g_current_capture->SendEventToProcessor(event);
  } else {
    SendEventToProcessor(event);
  }
}

void DesktopWindowTreeHostX11::DispatchKeyEvent(ui::KeyEvent* event) {
  GetInputMethod()->DispatchKeyEvent(event);
}

void DesktopWindowTreeHostX11::ConvertEventToDifferentHost(
    ui::LocatedEvent* located_event,
    DesktopWindowTreeHostX11* host) {
  DCHECK_NE(this, host);
  const display::Display display_src =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  const display::Display display_dest =
      display::Screen::GetScreen()->GetDisplayNearestWindow(host->window());
  DCHECK_EQ(display_src.device_scale_factor(),
            display_dest.device_scale_factor());
  gfx::Vector2d offset = GetLocationOnNativeScreen() -
                         host->GetLocationOnNativeScreen();
  gfx::PointF location_in_pixel_in_host =
      located_event->location_f() + gfx::Vector2dF(offset);
  located_event->set_location_f(location_in_pixel_in_host);
}

void DesktopWindowTreeHostX11::ResetWindowRegion() {
  // If a custom window shape was supplied then apply it.
  if (custom_window_shape_) {
    XShapeCombineRegion(xdisplay_, xwindow_, ShapeBounding, 0, 0,
                        window_shape_.get(), false);
    return;
  }

  window_shape_.reset();

  if (!IsMaximized() && !IsFullscreen()) {
    gfx::Path window_mask;
    views::Widget* widget = native_widget_delegate_->AsWidget();
    if (widget->non_client_view()) {
      // Some frame views define a custom (non-rectangular) window mask. If
      // so, use it to define the window shape. If not, fall through.
      widget->non_client_view()->GetWindowMask(bounds_in_pixels_.size(),
                                               &window_mask);
      if (window_mask.countPoints() > 0) {
        window_shape_.reset(gfx::CreateRegionFromSkPath(window_mask));
        XShapeCombineRegion(xdisplay_, xwindow_, ShapeBounding, 0, 0,
                            window_shape_.get(), false);
        return;
      }
    }
  }

  // If we didn't set the shape for any reason, reset the shaping information.
  // How this is done depends on the border style, due to quirks and bugs in
  // various window managers.
  if (ShouldUseNativeFrame()) {
    // If the window has system borders, the mask must be set to null (not a
    // rectangle), because several window managers (eg, KDE, XFCE, XMonad) will
    // not put borders on a window with a custom shape.
    XShapeCombineMask(xdisplay_, xwindow_, ShapeBounding, 0, 0, None, ShapeSet);
  } else {
    // Conversely, if the window does not have system borders, the mask must be
    // manually set to a rectangle that covers the whole window (not null). This
    // is due to a bug in KWin <= 4.11.5 (KDE bug #330573) where setting a null
    // shape causes the hint to disable system borders to be ignored (resulting
    // in a double border).
    XRectangle r = {0,
                    0,
                    static_cast<unsigned short>(bounds_in_pixels_.width()),
                    static_cast<unsigned short>(bounds_in_pixels_.height())};
    XShapeCombineRectangles(
        xdisplay_, xwindow_, ShapeBounding, 0, 0, &r, 1, ShapeSet, YXBanded);
  }
}

void DesktopWindowTreeHostX11::SerializeImageRepresentation(
    const gfx::ImageSkiaRep& rep,
    std::vector<unsigned long>* data) {
  int width = rep.GetWidth();
  data->push_back(width);

  int height = rep.GetHeight();
  data->push_back(height);

  const SkBitmap& bitmap = rep.sk_bitmap();
  SkAutoLockPixels locker(bitmap);

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      data->push_back(bitmap.getColor(x, y));
}

std::list<XID>& DesktopWindowTreeHostX11::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<XID>();
  return *open_windows_;
}

void DesktopWindowTreeHostX11::MapWindow(ui::WindowShowState show_state) {
  if (show_state != ui::SHOW_STATE_DEFAULT &&
      show_state != ui::SHOW_STATE_NORMAL &&
      show_state != ui::SHOW_STATE_INACTIVE &&
      show_state != ui::SHOW_STATE_MAXIMIZED) {
    // It will behave like SHOW_STATE_NORMAL.
    NOTIMPLEMENTED();
  }

  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  XSizeHints size_hints;
  size_hints.flags = PPosition;
  size_hints.x = bounds_in_pixels_.x();
  size_hints.y = bounds_in_pixels_.y();
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

  // If SHOW_STATE_INACTIVE, tell the window manager not to focus the window
  // when mapping. This is done by setting the _NET_WM_USER_TIME to 0. See e.g.
  // http://standards.freedesktop.org/wm-spec/latest/ar01s05.html
  unsigned long wm_user_time_ms = (show_state == ui::SHOW_STATE_INACTIVE) ?
      0 : X11DesktopHandler::get()->wm_user_time_ms();
  if (show_state == ui::SHOW_STATE_INACTIVE || wm_user_time_ms != 0) {
    XChangeProperty(xdisplay_,
                    xwindow_,
                    atom_cache_.GetAtom("_NET_WM_USER_TIME"),
                    XA_CARDINAL,
                    32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char *>(&wm_user_time_ms),
                    1);
  }

  ui::X11EventSource* event_source = ui::X11EventSource::GetInstance();
  DCHECK(event_source);

  if (wait_for_unmap_) {
    // Block until our window is unmapped. This avoids a race condition when
    // remapping an unmapped window.
    event_source->BlockUntilWindowUnmapped(xwindow_);
    DCHECK(!wait_for_unmap_);
  }

  XMapWindow(xdisplay_, xwindow_);

  // We now block until our window is mapped. Some X11 APIs will crash and
  // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
  // asynchronous.
  event_source->BlockUntilWindowMapped(xwindow_);
}

void DesktopWindowTreeHostX11::SetWindowTransparency() {
  compositor()->SetHostHasTransparentBackground(use_argb_visual_);
  window()->SetTransparent(use_argb_visual_);
  content_window_->SetTransparent(use_argb_visual_);
}

void DesktopWindowTreeHostX11::Relayout() {
  Widget* widget = native_widget_delegate_->AsWidget();
  NonClientView* non_client_view = widget->non_client_view();
  // non_client_view may be NULL, especially during creation.
  if (non_client_view) {
    non_client_view->client_view()->InvalidateLayout();
    non_client_view->InvalidateLayout();
  }
  widget->GetRootView()->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, ui::PlatformEventDispatcher implementation:

bool DesktopWindowTreeHostX11::CanDispatchEvent(
    const ui::PlatformEvent& event) {
  return event->xany.window == xwindow_ ||
         (event->type == GenericEvent &&
          static_cast<XIDeviceEvent*>(event->xcookie.data)->event == xwindow_);
}

uint32_t DesktopWindowTreeHostX11::DispatchEvent(
    const ui::PlatformEvent& event) {
  XEvent* xev = event;

  TRACE_EVENT1("views", "DesktopWindowTreeHostX11::Dispatch",
               "event->type", event->type);

  UpdateWMUserTime(event);

  // May want to factor CheckXEventForConsistency(xev); into a common location
  // since it is called here.
  switch (xev->type) {
    case EnterNotify:
    case LeaveNotify: {
      // Ignore EventNotify and LeaveNotify events from children of |xwindow_|.
      // NativeViewGLSurfaceGLX adds a child to |xwindow_|.
      // TODO(pkotwicz|tdanderson): Figure out whether the suppression is
      // necessary. crbug.com/385716
      if (xev->xcrossing.detail == NotifyInferior)
        break;

      ui::MouseEvent mouse_event(xev);
      DispatchMouseEvent(&mouse_event);
      break;
    }
    case Expose: {
      gfx::Rect damage_rect_in_pixels(xev->xexpose.x, xev->xexpose.y,
                                      xev->xexpose.width, xev->xexpose.height);
      compositor()->ScheduleRedrawRect(damage_rect_in_pixels);
      break;
    }
    case KeyPress: {
      ui::KeyEvent keydown_event(xev);
      DispatchKeyEvent(&keydown_event);
      break;
    }
    case KeyRelease: {
      // There is no way to deactivate a window in X11 so ignore input if
      // window is supposed to be 'inactive'. See comments in
      // X11DesktopHandler::DeactivateWindow() for more details.
      if (!IsActive() && !HasCapture())
        break;

      ui::KeyEvent key_event(xev);
      DispatchKeyEvent(&key_event);
      break;
    }
    case ButtonPress:
    case ButtonRelease: {
      ui::EventType event_type = ui::EventTypeFromNative(xev);
      switch (event_type) {
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          DispatchMouseEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED: {
          ui::MouseEvent mouseev(xev);
          DispatchMouseEvent(&mouseev);
          break;
        }
        case ui::ET_UNKNOWN:
          // No event is created for X11-release events for mouse-wheel buttons.
          break;
        default:
          NOTREACHED() << event_type;
      }
      break;
    }
    case FocusOut:
      if (xev->xfocus.mode != NotifyGrab) {
        ReleaseCapture();
        OnHostLostWindowCapture();
        X11DesktopHandler::get()->ProcessXEvent(xev);
      } else {
        dispatcher()->OnHostLostMouseGrab();
      }
      break;
    case FocusIn:
      X11DesktopHandler::get()->ProcessXEvent(xev);
      break;
    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the root window size is maintained properly.
      int translated_x_in_pixels = xev->xconfigure.x;
      int translated_y_in_pixels = xev->xconfigure.y;
      if (!xev->xconfigure.send_event && !xev->xconfigure.override_redirect) {
        Window unused;
        XTranslateCoordinates(xdisplay_, xwindow_, x_root_window_, 0, 0,
                              &translated_x_in_pixels, &translated_y_in_pixels,
                              &unused);
      }
      gfx::Rect bounds_in_pixels(translated_x_in_pixels, translated_y_in_pixels,
                                 xev->xconfigure.width, xev->xconfigure.height);
      bool size_changed = bounds_in_pixels_.size() != bounds_in_pixels.size();
      bool origin_changed =
          bounds_in_pixels_.origin() != bounds_in_pixels.origin();
      previous_bounds_in_pixels_ = bounds_in_pixels_;
      bounds_in_pixels_ = bounds_in_pixels;

      if (origin_changed)
        OnHostMoved(bounds_in_pixels_.origin());

      if (size_changed) {
        delayed_resize_task_.Reset(base::Bind(
            &DesktopWindowTreeHostX11::DelayedResize,
            close_widget_factory_.GetWeakPtr(), bounds_in_pixels.size()));
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, delayed_resize_task_.callback());
      }
      break;
    }
    case GenericEvent: {
      ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(xev))
        break;

      ui::EventType type = ui::EventTypeFromNative(xev);
      XEvent last_event;
      int num_coalesced = 0;

      switch (type) {
        case ui::ET_TOUCH_MOVED:
          num_coalesced = ui::CoalescePendingMotionEvents(xev, &last_event);
          if (num_coalesced > 0)
            xev = &last_event;
          // fallthrough
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_TOUCH_RELEASED: {
          ui::TouchEvent touchev(xev);
          DispatchTouchEvent(&touchev);
          break;
        }
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED:
        case ui::ET_MOUSE_ENTERED:
        case ui::ET_MOUSE_EXITED: {
          if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED) {
            // If this is a motion event, we want to coalesce all pending motion
            // events that are at the top of the queue.
            num_coalesced = ui::CoalescePendingMotionEvents(xev, &last_event);
            if (num_coalesced > 0)
              xev = &last_event;
          }
          ui::MouseEvent mouseev(xev);
          DispatchMouseEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          DispatchMouseEvent(&mouseev);
          break;
        }
        case ui::ET_SCROLL_FLING_START:
        case ui::ET_SCROLL_FLING_CANCEL:
        case ui::ET_SCROLL: {
          ui::ScrollEvent scrollev(xev);
          SendEventToProcessor(&scrollev);
          break;
        }
        case ui::ET_KEY_PRESSED:
        case ui::ET_KEY_RELEASED: {
          ui::KeyEvent key_event(xev);
          DispatchKeyEvent(&key_event);
          break;
        }
        case ui::ET_UNKNOWN:
          break;
        default:
          NOTREACHED();
      }

      // If we coalesced an event we need to free its cookie.
      if (num_coalesced > 0)
        XFreeEventData(xev->xgeneric.display, &last_event.xcookie);
      break;
    }
    case MapNotify: {
      window_mapped_ = true;

      FOR_EACH_OBSERVER(DesktopWindowTreeHostObserverX11,
                        observer_list_,
                        OnWindowMapped(xwindow_));

      UpdateMinAndMaxSize();

      // Some WMs only respect maximize hints after the window has been mapped.
      // Check whether we need to re-do a maximization.
      if (should_maximize_after_map_) {
        Maximize();
        should_maximize_after_map_ = false;
      }

      break;
    }
    case UnmapNotify: {
      wait_for_unmap_ = false;
      FOR_EACH_OBSERVER(DesktopWindowTreeHostObserverX11,
                        observer_list_,
                        OnWindowUnmapped(xwindow_));
      break;
    }
    case ClientMessage: {
      Atom message_type = xev->xclient.message_type;
      if (message_type == atom_cache_.GetAtom("WM_PROTOCOLS")) {
        Atom protocol = static_cast<Atom>(xev->xclient.data.l[0]);
        if (protocol == atom_cache_.GetAtom("WM_DELETE_WINDOW")) {
          // We have received a close message from the window manager.
          OnHostCloseRequested();
        } else if (protocol == atom_cache_.GetAtom("_NET_WM_PING")) {
          XEvent reply_event = *xev;
          reply_event.xclient.window = x_root_window_;

          XSendEvent(xdisplay_,
                     reply_event.xclient.window,
                     False,
                     SubstructureRedirectMask | SubstructureNotifyMask,
                     &reply_event);
        }
      } else if (message_type == atom_cache_.GetAtom("XdndEnter")) {
        drag_drop_client_->OnXdndEnter(xev->xclient);
      } else if (message_type == atom_cache_.GetAtom("XdndLeave")) {
        drag_drop_client_->OnXdndLeave(xev->xclient);
      } else if (message_type == atom_cache_.GetAtom("XdndPosition")) {
        drag_drop_client_->OnXdndPosition(xev->xclient);
      } else if (message_type == atom_cache_.GetAtom("XdndStatus")) {
        drag_drop_client_->OnXdndStatus(xev->xclient);
      } else if (message_type == atom_cache_.GetAtom("XdndFinished")) {
        drag_drop_client_->OnXdndFinished(xev->xclient);
      } else if (message_type == atom_cache_.GetAtom("XdndDrop")) {
        drag_drop_client_->OnXdndDrop(xev->xclient);
      }
      break;
    }
    case MappingNotify: {
      switch (xev->xmapping.request) {
        case MappingModifier:
        case MappingKeyboard:
          XRefreshKeyboardMapping(&xev->xmapping);
          break;
        case MappingPointer:
          ui::DeviceDataManagerX11::GetInstance()->UpdateButtonMap();
          break;
        default:
          NOTIMPLEMENTED() << " Unknown request: " << xev->xmapping.request;
          break;
      }
      break;
    }
    case MotionNotify: {
      // Discard all but the most recent motion event that targets the same
      // window with unchanged state.
      XEvent last_event;
      while (XPending(xev->xany.display)) {
        XEvent next_event;
        XPeekEvent(xev->xany.display, &next_event);
        if (next_event.type == MotionNotify &&
            next_event.xmotion.window == xev->xmotion.window &&
            next_event.xmotion.subwindow == xev->xmotion.subwindow &&
            next_event.xmotion.state == xev->xmotion.state) {
          XNextEvent(xev->xany.display, &last_event);
          xev = &last_event;
        } else {
          break;
        }
      }

      ui::MouseEvent mouseev(xev);
      DispatchMouseEvent(&mouseev);
      break;
    }
    case PropertyNotify: {
      ::Atom changed_atom = xev->xproperty.atom;
      if (changed_atom == atom_cache_.GetAtom("_NET_WM_STATE"))
        OnWMStateUpdated();
      else if (changed_atom == atom_cache_.GetAtom("_NET_FRAME_EXTENTS"))
        OnFrameExtentsUpdated();
      else if (changed_atom == atom_cache_.GetAtom("_NET_WM_DESKTOP"))
        OnHostWorkspaceChanged();
      break;
    }
    case SelectionNotify: {
      drag_drop_client_->OnSelectionNotify(xev->xselection);
      break;
    }
  }
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

void DesktopWindowTreeHostX11::DelayedResize(const gfx::Size& size_in_pixels) {
  OnHostResized(size_in_pixels);
  ResetWindowRegion();
  delayed_resize_task_.Cancel();
}

gfx::Rect DesktopWindowTreeHostX11::GetWorkAreaBoundsInPixels() const {
  std::vector<int> value;
  if (ui::GetIntArrayProperty(x_root_window_, "_NET_WORKAREA", &value) &&
      value.size() >= 4) {
    return gfx::Rect(value[0], value[1], value[2], value[3]);
  }

  // Fetch the geometry of the root window.
  Window root;
  int x, y;
  unsigned int width, height;
  unsigned int border_width, depth;
  if (!XGetGeometry(xdisplay_, x_root_window_, &root, &x, &y, &width, &height,
                    &border_width, &depth)) {
    NOTIMPLEMENTED();
    return gfx::Rect(0, 0, 10, 10);
  }

  return gfx::Rect(x, y, width, height);
}

gfx::Rect DesktopWindowTreeHostX11::ToDIPRect(
    const gfx::Rect& rect_in_pixels) const {
  gfx::RectF rect_in_dip = gfx::RectF(rect_in_pixels);
  GetRootTransform().TransformRectReverse(&rect_in_dip);
  return gfx::ToEnclosingRect(rect_in_dip);
}

gfx::Rect DesktopWindowTreeHostX11::ToPixelRect(
    const gfx::Rect& rect_in_dip) const {
  gfx::RectF rect_in_pixels = gfx::RectF(rect_in_dip);
  GetRootTransform().TransformRect(&rect_in_pixels);
  return gfx::ToEnclosingRect(rect_in_pixels);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost, public:

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostX11(native_widget_delegate,
                                      desktop_native_widget_aura);
}

// static
ui::NativeTheme* DesktopWindowTreeHost::GetNativeTheme(aura::Window* window) {
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui) {
    ui::NativeTheme* native_theme = linux_ui->GetNativeTheme(window);
    if (native_theme)
      return native_theme;
  }

  return ui::NativeThemeAura::instance();
}

}  // namespace views
