// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_message_handler.h"

#include <dwmapi.h>
#include <oleacc.h>
#include <shellapi.h>
#include <tchar.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/alias.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_version.h"
#include "ui/base/touch/touch_enabled.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/internal_constants.h"
#include "ui/base/win/lock_state.h"
#include "ui/base/win/mouse_wheel_util.h"
#include "ui/base/win/shell.h"
#include "ui/base/win/touch_input.h"
#include "ui/display/win/dpi.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/win/system_event_state_lookup.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/path.h"
#include "ui/gfx/path_win.h"
#include "ui/gfx/win/direct_manipulation.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/gfx/win/rendering_window_manager.h"
#include "ui/native_theme/native_theme_win.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/monitor_win.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/win/fullscreen_handler.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#include "ui/views/win/windows_session_change_observer.h"

namespace views {
namespace {

// MoveLoopMouseWatcher is used to determine if the user canceled or completed a
// move. win32 doesn't appear to offer a way to determine the result of a move,
// so we install hooks to determine if we got a mouse up and assume the move
// completed.
class MoveLoopMouseWatcher {
 public:
  MoveLoopMouseWatcher(HWNDMessageHandler* host, bool hide_on_escape);
  ~MoveLoopMouseWatcher();

  // Returns true if the mouse is up, or if we couldn't install the hook.
  bool got_mouse_up() const { return got_mouse_up_; }

 private:
  // Instance that owns the hook. We only allow one instance to hook the mouse
  // at a time.
  static MoveLoopMouseWatcher* instance_;

  // Key and mouse callbacks from the hook.
  static LRESULT CALLBACK MouseHook(int n_code, WPARAM w_param, LPARAM l_param);
  static LRESULT CALLBACK KeyHook(int n_code, WPARAM w_param, LPARAM l_param);

  void Unhook();

  // HWNDMessageHandler that created us.
  HWNDMessageHandler* host_;

  // Should the window be hidden when escape is pressed?
  const bool hide_on_escape_;

  // Did we get a mouse up?
  bool got_mouse_up_;

  // Hook identifiers.
  HHOOK mouse_hook_;
  HHOOK key_hook_;

  DISALLOW_COPY_AND_ASSIGN(MoveLoopMouseWatcher);
};

// static
MoveLoopMouseWatcher* MoveLoopMouseWatcher::instance_ = NULL;

MoveLoopMouseWatcher::MoveLoopMouseWatcher(HWNDMessageHandler* host,
                                           bool hide_on_escape)
    : host_(host),
      hide_on_escape_(hide_on_escape),
      got_mouse_up_(false),
      mouse_hook_(NULL),
      key_hook_(NULL) {
  // Only one instance can be active at a time.
  if (instance_)
    instance_->Unhook();

  mouse_hook_ = SetWindowsHookEx(
      WH_MOUSE, &MouseHook, NULL, GetCurrentThreadId());
  if (mouse_hook_) {
    instance_ = this;
    // We don't care if setting the key hook succeeded.
    key_hook_ = SetWindowsHookEx(
        WH_KEYBOARD, &KeyHook, NULL, GetCurrentThreadId());
  }
  if (instance_ != this) {
    // Failed installation. Assume we got a mouse up in this case, otherwise
    // we'll think all drags were canceled.
    got_mouse_up_ = true;
  }
}

MoveLoopMouseWatcher::~MoveLoopMouseWatcher() {
  Unhook();
}

void MoveLoopMouseWatcher::Unhook() {
  if (instance_ != this)
    return;

  DCHECK(mouse_hook_);
  UnhookWindowsHookEx(mouse_hook_);
  if (key_hook_)
    UnhookWindowsHookEx(key_hook_);
  key_hook_ = NULL;
  mouse_hook_ = NULL;
  instance_ = NULL;
}

// static
LRESULT CALLBACK MoveLoopMouseWatcher::MouseHook(int n_code,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  DCHECK(instance_);
  if (n_code == HC_ACTION && w_param == WM_LBUTTONUP)
    instance_->got_mouse_up_ = true;
  return CallNextHookEx(instance_->mouse_hook_, n_code, w_param, l_param);
}

// static
LRESULT CALLBACK MoveLoopMouseWatcher::KeyHook(int n_code,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  if (n_code == HC_ACTION && w_param == VK_ESCAPE) {
    int value = TRUE;
    DwmSetWindowAttribute(instance_->host_->hwnd(),
                          DWMWA_TRANSITIONS_FORCEDISABLED, &value,
                          sizeof(value));
    if (instance_->hide_on_escape_)
      instance_->host_->Hide();
  }
  return CallNextHookEx(instance_->key_hook_, n_code, w_param, l_param);
}

// Called from OnNCActivate.
BOOL CALLBACK EnumChildWindowsForRedraw(HWND hwnd, LPARAM lparam) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  int flags = RDW_INVALIDATE | RDW_NOCHILDREN | RDW_FRAME;
  if (process_id == GetCurrentProcessId())
    flags |= RDW_UPDATENOW;
  RedrawWindow(hwnd, NULL, NULL, flags);
  return TRUE;
}

bool GetMonitorAndRects(const RECT& rect,
                        HMONITOR* monitor,
                        gfx::Rect* monitor_rect,
                        gfx::Rect* work_area) {
  DCHECK(monitor);
  DCHECK(monitor_rect);
  DCHECK(work_area);
  *monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
  if (!*monitor)
    return false;
  MONITORINFO monitor_info = { 0 };
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(*monitor, &monitor_info);
  *monitor_rect = gfx::Rect(monitor_info.rcMonitor);
  *work_area = gfx::Rect(monitor_info.rcWork);
  return true;
}

struct FindOwnedWindowsData {
  HWND window;
  std::vector<Widget*> owned_widgets;
};

// Enables or disables the menu item for the specified command and menu.
void EnableMenuItemByCommand(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

// Callback used to notify child windows that the top level window received a
// DWMCompositionChanged message.
BOOL CALLBACK SendDwmCompositionChanged(HWND window, LPARAM param) {
  SendMessage(window, WM_DWMCOMPOSITIONCHANGED, 0, 0);
  return TRUE;
}

// The thickness of an auto-hide taskbar in pixels.
const int kAutoHideTaskbarThicknessPx = 2;

bool IsTopLevelWindow(HWND window) {
  long style = ::GetWindowLong(window, GWL_STYLE);
  if (!(style & WS_CHILD))
    return true;
  HWND parent = ::GetParent(window);
  return !parent || (parent == ::GetDesktopWindow());
}

void AddScrollStylesToWindow(HWND window) {
  if (::IsWindow(window)) {
    long current_style = ::GetWindowLong(window, GWL_STYLE);
    ::SetWindowLong(window, GWL_STYLE,
                    current_style | WS_VSCROLL | WS_HSCROLL);
  }
}

const int kTouchDownContextResetTimeout = 500;

// Windows does not flag synthesized mouse messages from touch in all cases.
// This causes us grief as we don't want to process touch and mouse messages
// concurrently. Hack as per msdn is to check if the time difference between
// the touch message and the mouse move is within 500 ms and at the same
// location as the cursor.
const int kSynthesizedMouseTouchMessagesTimeDifference = 500;

}  // namespace

// A scoping class that prevents a window from being able to redraw in response
// to invalidations that may occur within it for the lifetime of the object.
//
// Why would we want such a thing? Well, it turns out Windows has some
// "unorthodox" behavior when it comes to painting its non-client areas.
// Occasionally, Windows will paint portions of the default non-client area
// right over the top of the custom frame. This is not simply fixed by handling
// WM_NCPAINT/WM_PAINT, with some investigation it turns out that this
// rendering is being done *inside* the default implementation of some message
// handlers and functions:
//  . WM_SETTEXT
//  . WM_SETICON
//  . WM_NCLBUTTONDOWN
//  . EnableMenuItem, called from our WM_INITMENU handler
// The solution is to handle these messages and call DefWindowProc ourselves,
// but prevent the window from being able to update itself for the duration of
// the call. We do this with this class, which automatically calls its
// associated Window's lock and unlock functions as it is created and destroyed.
// See documentation in those methods for the technique used.
//
// The lock only has an effect if the window was visible upon lock creation, as
// it doesn't guard against direct visiblility changes, and multiple locks may
// exist simultaneously to handle certain nested Windows messages.
//
// IMPORTANT: Do not use this scoping object for large scopes or periods of
//            time! IT WILL PREVENT THE WINDOW FROM BEING REDRAWN! (duh).
//
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
class HWNDMessageHandler::ScopedRedrawLock {
 public:
  explicit ScopedRedrawLock(HWNDMessageHandler* owner)
    : owner_(owner),
      hwnd_(owner_->hwnd()),
      was_visible_(owner_->IsVisible()),
      cancel_unlock_(false),
      force_(!(GetWindowLong(hwnd_, GWL_STYLE) & WS_CAPTION)) {
    if (was_visible_ && ::IsWindow(hwnd_))
      owner_->LockUpdates(force_);
  }

  ~ScopedRedrawLock() {
    if (!cancel_unlock_ && was_visible_ && ::IsWindow(hwnd_))
      owner_->UnlockUpdates(force_);
  }

  // Cancel the unlock operation, call this if the Widget is being destroyed.
  void CancelUnlockOperation() { cancel_unlock_ = true; }

 private:
  // The owner having its style changed.
  HWNDMessageHandler* owner_;
  // The owner's HWND, cached to avoid action after window destruction.
  HWND hwnd_;
  // Records the HWND visibility at the time of creation.
  bool was_visible_;
  // A flag indicating that the unlock operation was canceled.
  bool cancel_unlock_;
  // If true, perform the redraw lock regardless of Aero state.
  bool force_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRedrawLock);
};

// static HWNDMessageHandler member initialization.
base::LazyInstance<HWNDMessageHandler::FullscreenWindowMonitorMap>
    HWNDMessageHandler::fullscreen_monitor_map_ = LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, public:

long HWNDMessageHandler::last_touch_message_time_ = 0;

HWNDMessageHandler::HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate)
    : msg_handled_(FALSE),
      delegate_(delegate),
      fullscreen_handler_(new FullscreenHandler),
      waiting_for_close_now_(false),
      use_system_default_icon_(false),
      restored_enabled_(false),
      current_cursor_(NULL),
      previous_cursor_(NULL),
      active_mouse_tracking_flags_(0),
      is_right_mouse_pressed_on_caption_(false),
      lock_updates_count_(0),
      ignore_window_pos_changes_(false),
      last_monitor_(NULL),
      is_first_nccalc_(true),
      menu_depth_(0),
      id_generator_(0),
      needs_scroll_styles_(false),
      in_size_loop_(false),
      touch_down_contexts_(0),
      last_mouse_hwheel_time_(0),
      dwm_transition_desired_(false),
      sent_window_size_changing_(false),
      left_button_down_on_caption_(false),
      background_fullscreen_hack_(false),
      autohide_factory_(this),
      weak_factory_(this) {}

HWNDMessageHandler::~HWNDMessageHandler() {
  delegate_ = NULL;
  // Prevent calls back into this class via WNDPROC now that we've been
  // destroyed.
  ClearUserData();
}

void HWNDMessageHandler::Init(HWND parent, const gfx::Rect& bounds) {
  TRACE_EVENT0("views", "HWNDMessageHandler::Init");
  GetMonitorAndRects(bounds.ToRECT(), &last_monitor_, &last_monitor_rect_,
                     &last_work_area_);

  // Create the window.
  WindowImpl::Init(parent, bounds);
  // TODO(ananta)
  // Remove the scrolling hack code once we have scrolling working well.
#if defined(ENABLE_SCROLL_HACK)
  // Certain trackpad drivers on Windows have bugs where in they don't generate
  // WM_MOUSEWHEEL messages for the trackpoint and trackpad scrolling gestures
  // unless there is an entry for Chrome with the class name of the Window.
  // These drivers check if the window under the trackpoint has the WS_VSCROLL/
  // WS_HSCROLL style and if yes they generate the legacy WM_VSCROLL/WM_HSCROLL
  // messages. We add these styles to ensure that trackpad/trackpoint scrolling
  // work.
  // TODO(ananta)
  // Look into moving the WS_VSCROLL and WS_HSCROLL style setting logic to the
  // CalculateWindowStylesFromInitParams function. Doing it there seems to
  // cause some interactive tests to fail. Investigation needed.
  if (IsTopLevelWindow(hwnd())) {
    long current_style = ::GetWindowLong(hwnd(), GWL_STYLE);
    if (!(current_style & WS_POPUP)) {
      AddScrollStylesToWindow(hwnd());
      needs_scroll_styles_ = true;
    }
  }
#endif

  prop_window_target_.reset(new ui::ViewProp(hwnd(),
                            ui::WindowEventTarget::kWin32InputEventTarget,
                            static_cast<ui::WindowEventTarget*>(this)));

  // Direct Manipulation is enabled on Windows 10+. The CreateInstance function
  // returns NULL if Direct Manipulation is not available.
  direct_manipulation_helper_ =
      gfx::win::DirectManipulationHelper::CreateInstance();
  if (direct_manipulation_helper_)
    direct_manipulation_helper_->Initialize(hwnd());

  // Disable pen flicks (http://crbug.com/506977)
  base::win::DisableFlicks(hwnd());
}

void HWNDMessageHandler::InitModalType(ui::ModalType modal_type) {
  if (modal_type == ui::MODAL_TYPE_NONE)
    return;
  // We implement modality by crawling up the hierarchy of windows starting
  // at the owner, disabling all of them so that they don't receive input
  // messages.
  HWND start = ::GetWindow(hwnd(), GW_OWNER);
  while (start) {
    ::EnableWindow(start, FALSE);
    start = ::GetParent(start);
  }
}

void HWNDMessageHandler::Close() {
  if (!IsWindow(hwnd()))
    return;  // No need to do anything.

  // Let's hide ourselves right away.
  Hide();

  // Modal dialog windows disable their owner windows; re-enable them now so
  // they can activate as foreground windows upon this window's destruction.
  RestoreEnabledIfNecessary();

  // Re-enable flicks which removes the window property.
  base::win::EnableFlicks(hwnd());

  if (!waiting_for_close_now_) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    waiting_for_close_now_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&HWNDMessageHandler::CloseNow, weak_factory_.GetWeakPtr()));
  }
}

void HWNDMessageHandler::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  waiting_for_close_now_ = false;
  if (IsWindow(hwnd()))
    DestroyWindow(hwnd());
}

gfx::Rect HWNDMessageHandler::GetWindowBoundsInScreen() const {
  RECT r;
  GetWindowRect(hwnd(), &r);
  return gfx::Rect(r);
}

gfx::Rect HWNDMessageHandler::GetClientAreaBoundsInScreen() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  POINT point = { r.left, r.top };
  ClientToScreen(hwnd(), &point);
  return gfx::Rect(point.x, point.y, r.right - r.left, r.bottom - r.top);
}

gfx::Rect HWNDMessageHandler::GetRestoredBounds() const {
  // If we're in fullscreen mode, we've changed the normal bounds to the monitor
  // rect, so return the saved bounds instead.
  if (IsFullscreen())
    return fullscreen_handler_->GetRestoreBounds();

  gfx::Rect bounds;
  GetWindowPlacement(&bounds, NULL);
  return bounds;
}

gfx::Rect HWNDMessageHandler::GetClientAreaBounds() const {
  if (IsMinimized())
    return gfx::Rect();
  if (delegate_->WidgetSizeIsClientSize())
    return GetClientAreaBoundsInScreen();
  return GetWindowBoundsInScreen();
}

void HWNDMessageHandler::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool succeeded = !!::GetWindowPlacement(hwnd(), &wp);
  DCHECK(succeeded);

  if (bounds != NULL) {
    if (wp.showCmd == SW_SHOWNORMAL) {
      // GetWindowPlacement can return misleading position if a normalized
      // window was resized using Aero Snap feature (see comment 9 in bug
      // 36421). As a workaround, using GetWindowRect for normalized windows.
      const bool succeeded = GetWindowRect(hwnd(), &wp.rcNormalPosition) != 0;
      DCHECK(succeeded);

      *bounds = gfx::Rect(wp.rcNormalPosition);
    } else {
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      const bool succeeded = GetMonitorInfo(
          MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST), &mi) != 0;
      DCHECK(succeeded);

      *bounds = gfx::Rect(wp.rcNormalPosition);
      // Convert normal position from workarea coordinates to screen
      // coordinates.
      bounds->Offset(mi.rcWork.left - mi.rcMonitor.left,
                     mi.rcWork.top - mi.rcMonitor.top);
    }
  }

  if (show_state) {
    if (wp.showCmd == SW_SHOWMAXIMIZED)
      *show_state = ui::SHOW_STATE_MAXIMIZED;
    else if (wp.showCmd == SW_SHOWMINIMIZED)
      *show_state = ui::SHOW_STATE_MINIMIZED;
    else
      *show_state = ui::SHOW_STATE_NORMAL;
  }
}

void HWNDMessageHandler::SetBounds(const gfx::Rect& bounds_in_pixels,
                                   bool force_size_changed) {
  background_fullscreen_hack_ = false;
  SetBoundsInternal(bounds_in_pixels, force_size_changed);
}

void HWNDMessageHandler::SetDwmFrameExtension(DwmFrameState state) {
  if (!delegate_->HasFrame() && ui::win::IsAeroGlassEnabled() &&
      (window_ex_style() & WS_EX_COMPOSITED) == 0) {
    MARGINS m = {0, 0, 0, 0};
    if (state == DwmFrameState::ON)
      m = {0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(hwnd(), &m);
  }
}

void HWNDMessageHandler::SetSize(const gfx::Size& size) {
  SetWindowPos(hwnd(), NULL, 0, 0, size.width(), size.height(),
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
}

void HWNDMessageHandler::CenterWindow(const gfx::Size& size) {
  HWND parent = GetParent(hwnd());
  if (!IsWindow(hwnd()))
    parent = ::GetWindow(hwnd(), GW_OWNER);
  gfx::CenterAndSizeWindow(parent, hwnd(), size);
}

void HWNDMessageHandler::SetRegion(HRGN region) {
  custom_window_region_.reset(region);
  ResetWindowRegion(true, true);
}

void HWNDMessageHandler::StackAbove(HWND other_hwnd) {
  // Windows API allows to stack behind another windows only.
  DCHECK(other_hwnd);
  HWND next_window = GetNextWindow(other_hwnd, GW_HWNDPREV);
  SetWindowPos(hwnd(), next_window ? next_window : HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void HWNDMessageHandler::StackAtTop() {
  SetWindowPos(hwnd(), HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void HWNDMessageHandler::Show() {
  if (IsWindow(hwnd())) {
    if (!(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT) &&
        !(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
      ShowWindowWithState(ui::SHOW_STATE_NORMAL);
    } else {
      ShowWindowWithState(ui::SHOW_STATE_INACTIVE);
    }
  }
  if (direct_manipulation_helper_)
    direct_manipulation_helper_->Activate(hwnd());
}

void HWNDMessageHandler::ShowWindowWithState(ui::WindowShowState show_state) {
  TRACE_EVENT0("views", "HWNDMessageHandler::ShowWindowWithState");
  DWORD native_show_state;
  switch (show_state) {
    case ui::SHOW_STATE_INACTIVE:
      native_show_state = SW_SHOWNOACTIVATE;
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      native_show_state = SW_SHOWMAXIMIZED;
      break;
    case ui::SHOW_STATE_MINIMIZED:
      native_show_state = SW_SHOWMINIMIZED;
      break;
    case ui::SHOW_STATE_NORMAL:
      native_show_state = SW_SHOWNORMAL;
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      native_show_state = SW_SHOWNORMAL;
      SetFullscreen(true);
      break;
    default:
      native_show_state = delegate_->GetInitialShowState();
      break;
  }

  ShowWindow(hwnd(), native_show_state);
  // When launched from certain programs like bash and Windows Live Messenger,
  // show_state is set to SW_HIDE, so we need to correct that condition. We
  // don't just change show_state to SW_SHOWNORMAL because MSDN says we must
  // always first call ShowWindow with the specified value from STARTUPINFO,
  // otherwise all future ShowWindow calls will be ignored (!!#@@#!). Instead,
  // we call ShowWindow again in this case.
  if (native_show_state == SW_HIDE) {
    native_show_state = SW_SHOWNORMAL;
    ShowWindow(hwnd(), native_show_state);
  }

  // We need to explicitly activate the window if we've been shown with a state
  // that should activate, because if we're opened from a desktop shortcut while
  // an existing window is already running it doesn't seem to be enough to use
  // one of these flags to activate the window.
  if (native_show_state == SW_SHOWNORMAL ||
      native_show_state == SW_SHOWMAXIMIZED)
    Activate();

  if (!delegate_->HandleInitialFocus(show_state))
    SetInitialFocus();
}

void HWNDMessageHandler::ShowMaximizedWithBounds(const gfx::Rect& bounds) {
  WINDOWPLACEMENT placement = { 0 };
  placement.length = sizeof(WINDOWPLACEMENT);
  placement.showCmd = SW_SHOWMAXIMIZED;
  placement.rcNormalPosition = bounds.ToRECT();
  SetWindowPlacement(hwnd(), &placement);

  // We need to explicitly activate the window, because if we're opened from a
  // desktop shortcut while an existing window is already running it doesn't
  // seem to be enough to use SW_SHOWMAXIMIZED to activate the window.
  Activate();
}

void HWNDMessageHandler::Hide() {
  if (IsWindow(hwnd())) {
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
    SetWindowPos(hwnd(), NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

void HWNDMessageHandler::Maximize() {
  ExecuteSystemMenuCommand(SC_MAXIMIZE);
}

void HWNDMessageHandler::Minimize() {
  ExecuteSystemMenuCommand(SC_MINIMIZE);
  delegate_->HandleNativeBlur(NULL);
}

void HWNDMessageHandler::Restore() {
  ExecuteSystemMenuCommand(SC_RESTORE);
}

void HWNDMessageHandler::Activate() {
  if (IsMinimized())
    ::ShowWindow(hwnd(), SW_RESTORE);
  ::SetWindowPos(hwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(hwnd());
}

void HWNDMessageHandler::Deactivate() {
  HWND next_hwnd = ::GetNextWindow(hwnd(), GW_HWNDNEXT);
  while (next_hwnd) {
    if (::IsWindowVisible(next_hwnd)) {
      ::SetForegroundWindow(next_hwnd);
      return;
    }
    next_hwnd = ::GetNextWindow(next_hwnd, GW_HWNDNEXT);
  }
}

void HWNDMessageHandler::SetAlwaysOnTop(bool on_top) {
  ::SetWindowPos(hwnd(), on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool HWNDMessageHandler::IsVisible() const {
  return !!::IsWindowVisible(hwnd());
}

bool HWNDMessageHandler::IsActive() const {
  return GetActiveWindow() == hwnd();
}

bool HWNDMessageHandler::IsMinimized() const {
  return !!::IsIconic(hwnd());
}

bool HWNDMessageHandler::IsMaximized() const {
  return !!::IsZoomed(hwnd()) && !IsFullscreen();
}

bool HWNDMessageHandler::IsFullscreen() const {
  return fullscreen_handler_->fullscreen();
}

bool HWNDMessageHandler::IsAlwaysOnTop() const {
  return (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

bool HWNDMessageHandler::RunMoveLoop(const gfx::Vector2d& drag_offset,
                                     bool hide_on_escape) {
  ReleaseCapture();
  MoveLoopMouseWatcher watcher(this, hide_on_escape);
  // In Aura, we handle touch events asynchronously. So we need to allow nested
  // tasks while in windows move loop.
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(
      base::MessageLoop::current());

  SendMessage(hwnd(), WM_SYSCOMMAND, SC_MOVE | 0x0002, GetMessagePos());
  // Windows doesn't appear to offer a way to determine whether the user
  // canceled the move or not. We assume if the user released the mouse it was
  // successful.
  return watcher.got_mouse_up();
}

void HWNDMessageHandler::EndMoveLoop() {
  SendMessage(hwnd(), WM_CANCELMODE, 0, 0);
}

void HWNDMessageHandler::SendFrameChanged() {
  SetWindowPos(hwnd(), NULL, 0, 0, 0, 0,
      SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS |
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION |
      SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_NOZORDER);
}

void HWNDMessageHandler::FlashFrame(bool flash) {
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = hwnd();
  if (flash) {
    fwi.dwFlags = custom_window_region_.is_valid() ? FLASHW_TRAY : FLASHW_ALL;
    fwi.uCount = 4;
    fwi.dwTimeout = 0;
  } else {
    fwi.dwFlags = FLASHW_STOP;
  }
  FlashWindowEx(&fwi);
}

void HWNDMessageHandler::ClearNativeFocus() {
  ::SetFocus(hwnd());
}

void HWNDMessageHandler::SetCapture() {
  DCHECK(!HasCapture());
  ::SetCapture(hwnd());
}

void HWNDMessageHandler::ReleaseCapture() {
  if (HasCapture())
    ::ReleaseCapture();
}

bool HWNDMessageHandler::HasCapture() const {
  return ::GetCapture() == hwnd();
}

void HWNDMessageHandler::SetVisibilityChangedAnimationsEnabled(bool enabled) {
  int dwm_value = enabled ? FALSE : TRUE;
  DwmSetWindowAttribute(hwnd(), DWMWA_TRANSITIONS_FORCEDISABLED, &dwm_value,
                        sizeof(dwm_value));
}

bool HWNDMessageHandler::SetTitle(const base::string16& title) {
  base::string16 current_title;
  size_t len_with_null = GetWindowTextLength(hwnd()) + 1;
  if (len_with_null == 1 && title.length() == 0)
    return false;
  if (len_with_null - 1 == title.length() &&
      GetWindowText(hwnd(),
                    base::WriteInto(&current_title, len_with_null),
                    len_with_null) &&
      current_title == title)
    return false;
  SetWindowText(hwnd(), title.c_str());
  return true;
}

void HWNDMessageHandler::SetCursor(HCURSOR cursor) {
  if (cursor) {
    previous_cursor_ = ::SetCursor(cursor);
    current_cursor_ = cursor;
  } else if (previous_cursor_) {
    ::SetCursor(previous_cursor_);
    previous_cursor_ = NULL;
  }
}

void HWNDMessageHandler::FrameTypeChanged() {
  if (!custom_window_region_.is_valid() &&
      delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN)
    dwm_transition_desired_ = true;
  if (!dwm_transition_desired_ || !IsFullscreen())
    PerformDwmTransition();
}

void HWNDMessageHandler::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                        const gfx::ImageSkia& app_icon) {
  if (!window_icon.isNull()) {
    base::win::ScopedHICON previous_icon = std::move(window_icon_);
    window_icon_ = IconUtil::CreateHICONFromSkBitmap(*window_icon.bitmap());
    SendMessage(hwnd(), WM_SETICON, ICON_SMALL,
                reinterpret_cast<LPARAM>(window_icon_.get()));
  }
  if (!app_icon.isNull()) {
    base::win::ScopedHICON previous_icon = std::move(app_icon_);
    app_icon_ = IconUtil::CreateHICONFromSkBitmap(*app_icon.bitmap());
    SendMessage(hwnd(), WM_SETICON, ICON_BIG,
                reinterpret_cast<LPARAM>(app_icon_.get()));
  }
}

void HWNDMessageHandler::SetFullscreen(bool fullscreen) {
  background_fullscreen_hack_ = false;
  fullscreen_handler()->SetFullscreen(fullscreen);
  // If we are out of fullscreen and there was a pending DWM transition for the
  // window, then go ahead and do it now.
  if (!fullscreen && dwm_transition_desired_)
    PerformDwmTransition();

  // Add the fullscreen window to the fullscreen window map which is used to
  // handle window activations.
  HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);
  if (fullscreen) {
    (fullscreen_monitor_map_.Get())[monitor] = this;
  } else {
    FullscreenWindowMonitorMap::iterator iter =
        fullscreen_monitor_map_.Get().find(monitor);
    if (iter != fullscreen_monitor_map_.Get().end())
      fullscreen_monitor_map_.Get().erase(iter);
  }
}

void HWNDMessageHandler::SizeConstraintsChanged() {
  LONG style = GetWindowLong(hwnd(), GWL_STYLE);
  // Ignore if this is not a standard window.
  if (style & (WS_POPUP | WS_CHILD))
    return;

  LONG exstyle = GetWindowLong(hwnd(), GWL_EXSTYLE);
  // Windows cannot have WS_THICKFRAME set if WS_EX_COMPOSITED is set.
  // See CalculateWindowStylesFromInitParams().
  if (delegate_->CanResize() && (exstyle & WS_EX_COMPOSITED) == 0) {
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    if (!delegate_->CanMaximize())
      style &= ~WS_MAXIMIZEBOX;
  } else {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  if (delegate_->CanMinimize()) {
    style |= WS_MINIMIZEBOX;
  } else {
    style &= ~WS_MINIMIZEBOX;
  }
  SetWindowLong(hwnd(), GWL_STYLE, style);
}

bool HWNDMessageHandler::HasChildRenderingWindow() {
  // This can change dynamically if the system switches between GPU and
  // software rendering.
  return gfx::RenderingWindowManager::GetInstance()->HasValidChildWindow(
      hwnd());
}

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, gfx::WindowImpl overrides:

HICON HWNDMessageHandler::GetDefaultWindowIcon() const {
  if (use_system_default_icon_)
    return nullptr;
  return ViewsDelegate::GetInstance()
             ? ViewsDelegate::GetInstance()->GetDefaultWindowIcon()
             : nullptr;
}

HICON HWNDMessageHandler::GetSmallWindowIcon() const {
  if (use_system_default_icon_)
    return nullptr;
  return ViewsDelegate::GetInstance()
             ? ViewsDelegate::GetInstance()->GetSmallWindowIcon()
             : nullptr;
}

LRESULT HWNDMessageHandler::OnWndProc(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  HWND window = hwnd();
  LRESULT result = 0;

  if (delegate_ && delegate_->PreHandleMSG(message, w_param, l_param, &result))
    return result;

  // Otherwise we handle everything else.
  // NOTE: We inline ProcessWindowMessage() as 'this' may be destroyed during
  // dispatch and ProcessWindowMessage() doesn't deal with that well.
  const BOOL old_msg_handled = msg_handled_;
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  const BOOL processed =
      _ProcessWindowMessage(window, message, w_param, l_param, result, 0);
  if (!ref)
    return 0;
  msg_handled_ = old_msg_handled;

  if (!processed) {
    result = DefWindowProc(window, message, w_param, l_param);
    // DefWindowProc() may have destroyed the window and/or us in a nested
    // message loop.
    if (!ref || !::IsWindow(window))
      return result;
  }

  if (delegate_) {
    delegate_->PostHandleMSG(message, w_param, l_param);
    if (message == WM_NCDESTROY) {
      RestoreEnabledIfNecessary();
      delegate_->HandleDestroyed();
    }
  }

  if (message == WM_ACTIVATE && IsTopLevelWindow(window))
    PostProcessActivateMessage(LOWORD(w_param), !!HIWORD(w_param),
                               reinterpret_cast<HWND>(l_param));
  return result;
}

LRESULT HWNDMessageHandler::HandleMouseMessage(unsigned int message,
                                               WPARAM w_param,
                                               LPARAM l_param,
                                               bool* handled) {
  // Don't track forwarded mouse messages. We expect the caller to track the
  // mouse.
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  LRESULT ret = HandleMouseEventInternal(message, w_param, l_param, false);
  *handled = IsMsgHandled();
  return ret;
}

LRESULT HWNDMessageHandler::HandleKeyboardMessage(unsigned int message,
                                                  WPARAM w_param,
                                                  LPARAM l_param,
                                                  bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  LRESULT ret = 0;
  if ((message == WM_CHAR) || (message == WM_SYSCHAR))
    ret = OnImeMessages(message, w_param, l_param);
  else
    ret = OnKeyEvent(message, w_param, l_param);
  *handled = IsMsgHandled();
  return ret;
}

LRESULT HWNDMessageHandler::HandleTouchMessage(unsigned int message,
                                               WPARAM w_param,
                                               LPARAM l_param,
                                               bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  LRESULT ret = OnTouchEvent(message, w_param, l_param);
  *handled = IsMsgHandled();
  return ret;
}

LRESULT HWNDMessageHandler::HandleScrollMessage(unsigned int message,
                                                WPARAM w_param,
                                                LPARAM l_param,
                                                bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  LRESULT ret = OnScrollMessage(message, w_param, l_param);
  *handled = IsMsgHandled();
  return ret;
}

LRESULT HWNDMessageHandler::HandleNcHitTestMessage(unsigned int message,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  LRESULT ret = OnNCHitTest(
      gfx::Point(CR_GET_X_LPARAM(l_param), CR_GET_Y_LPARAM(l_param)));
  *handled = IsMsgHandled();
  return ret;
}

void HWNDMessageHandler::HandleParentChanged() {
  // If the forwarder window's parent is changed then we need to reset our
  // context as we will not receive touch releases if the touch was initiated
  // in the forwarder window.
  touch_ids_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, private:

int HWNDMessageHandler::GetAppbarAutohideEdges(HMONITOR monitor) {
  autohide_factory_.InvalidateWeakPtrs();
  return ViewsDelegate::GetInstance()
             ? ViewsDelegate::GetInstance()->GetAppbarAutohideEdges(
                   monitor,
                   base::Bind(&HWNDMessageHandler::OnAppbarAutohideEdgesChanged,
                              autohide_factory_.GetWeakPtr()))
             : ViewsDelegate::EDGE_BOTTOM;
}

void HWNDMessageHandler::OnAppbarAutohideEdgesChanged() {
  // This triggers querying WM_NCCALCSIZE again.
  RECT client;
  GetWindowRect(hwnd(), &client);
  SetWindowPos(hwnd(), NULL, client.left, client.top,
               client.right - client.left, client.bottom - client.top,
               SWP_FRAMECHANGED);
}

void HWNDMessageHandler::SetInitialFocus() {
  if (!(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT) &&
      !(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
    // The window does not get keyboard messages unless we focus it.
    SetFocus(hwnd());
  }
}

void HWNDMessageHandler::PostProcessActivateMessage(
    int activation_state,
    bool minimized,
    HWND window_gaining_or_losing_activation) {
  DCHECK(IsTopLevelWindow(hwnd()));
  const bool active = activation_state != WA_INACTIVE && !minimized;
  if (delegate_->CanActivate())
    delegate_->HandleActivationChanged(active);

  if (!::IsWindow(window_gaining_or_losing_activation))
    window_gaining_or_losing_activation = ::GetForegroundWindow();

  // If the window losing activation is a fullscreen window, we reduce the size
  // of the window by 1px. i.e. Not fullscreen. This is to work around an
  // apparent bug in the Windows taskbar where in it tracks fullscreen state on
  // a per thread basis. This causes it not be a topmost window when any
  // maximized window on a thread which has a fullscreen window is active. This
  // affects the way these windows interact with the taskbar, they obscure it
  // when maximized, autohide does not work correctly, etc.
  // By reducing the size of the fullscreen window by 1px, we ensure that the
  // taskbar no longer treats the window and in turn the thread as a fullscreen
  // thread. This in turn ensures that maximized windows on the same thread
  // don't obscure the taskbar, etc.
  // Please note that this taskbar behavior only occurs if the window becoming
  // active is on the same monitor as the fullscreen window.
  if (!active) {
    if (IsFullscreen() && ::IsWindow(window_gaining_or_losing_activation)) {
      HMONITOR active_window_monitor = MonitorFromWindow(
          window_gaining_or_losing_activation, MONITOR_DEFAULTTOPRIMARY);
      HMONITOR fullscreen_window_monitor =
          MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);

      if (active_window_monitor == fullscreen_window_monitor)
        OnBackgroundFullscreen();
    }
  } else if (background_fullscreen_hack_) {
    // Restore the bounds of the window to fullscreen.
    DCHECK(IsFullscreen());
    MONITORINFO monitor_info = {sizeof(monitor_info)};
    GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY),
                   &monitor_info);
    SetBoundsInternal(gfx::Rect(monitor_info.rcMonitor), false);
    background_fullscreen_hack_ = false;
  } else {
    // If the window becoming active has a fullscreen window on the same
    // monitor then we need to reduce the size of the fullscreen window by
    // 1 px. Please refer to the comments above for the reasoning behind
    // this.
    CheckAndHandleBackgroundFullscreenOnMonitor(
        window_gaining_or_losing_activation);
  }
}

void HWNDMessageHandler::RestoreEnabledIfNecessary() {
  if (delegate_->IsModal() && !restored_enabled_) {
    restored_enabled_ = true;
    // If we were run modally, we need to undo the disabled-ness we inflicted on
    // the owner's parent hierarchy.
    HWND start = ::GetWindow(hwnd(), GW_OWNER);
    while (start) {
      ::EnableWindow(start, TRUE);
      start = ::GetParent(start);
    }
  }
}

void HWNDMessageHandler::ExecuteSystemMenuCommand(int command) {
  if (command)
    SendMessage(hwnd(), WM_SYSCOMMAND, command, 0);
}

void HWNDMessageHandler::TrackMouseEvents(DWORD mouse_tracking_flags) {
  // Begin tracking mouse events for this HWND so that we get WM_MOUSELEAVE
  // when the user moves the mouse outside this HWND's bounds.
  if (active_mouse_tracking_flags_ == 0 || mouse_tracking_flags & TME_CANCEL) {
    if (mouse_tracking_flags & TME_CANCEL) {
      // We're about to cancel active mouse tracking, so empty out the stored
      // state.
      active_mouse_tracking_flags_ = 0;
    } else {
      active_mouse_tracking_flags_ = mouse_tracking_flags;
    }

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = mouse_tracking_flags;
    tme.hwndTrack = hwnd();
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  } else if (mouse_tracking_flags != active_mouse_tracking_flags_) {
    TrackMouseEvents(active_mouse_tracking_flags_ | TME_CANCEL);
    TrackMouseEvents(mouse_tracking_flags);
  }
}

void HWNDMessageHandler::ClientAreaSizeChanged() {
  // Ignore size changes due to fullscreen windows losing activation.
  if (background_fullscreen_hack_ && !sent_window_size_changing_)
    return;
  gfx::Size s = GetClientAreaBounds().size();
  delegate_->HandleClientSizeChanged(s);
}

bool HWNDMessageHandler::GetClientAreaInsets(gfx::Insets* insets) const {
  if (delegate_->GetClientAreaInsets(insets))
    return true;
  DCHECK(insets->IsEmpty());

  // Returning false causes the default handling in OnNCCalcSize() to
  // be invoked.
  if (!delegate_->HasNonClientView() || HasSystemFrame())
    return false;

  if (IsMaximized()) {
    // Windows automatically adds a standard width border to all sides when a
    // window is maximized.
    int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
    if (!delegate_->HasFrame())
      border_thickness -= 1;
    *insets = gfx::Insets(
        border_thickness, border_thickness, border_thickness, border_thickness);
    return true;
  }

  *insets = gfx::Insets();
  return true;
}

void HWNDMessageHandler::ResetWindowRegion(bool force, bool redraw) {
  // A native frame uses the native window region, and we don't want to mess
  // with it.
  // WS_EX_COMPOSITED is used instead of WS_EX_LAYERED under aura. WS_EX_LAYERED
  // automatically makes clicks on transparent pixels fall through, that isn't
  // the case with WS_EX_COMPOSITED. So, we route WS_EX_COMPOSITED through to
  // the delegate to allow for a custom hit mask.
  if ((window_ex_style() & WS_EX_COMPOSITED) == 0 &&
      !custom_window_region_.is_valid() &&
      (delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN ||
       !delegate_->HasNonClientView())) {
    if (force)
      SetWindowRgn(hwnd(), NULL, redraw);
    return;
  }

  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  base::win::ScopedRegion current_rgn(CreateRectRgn(0, 0, 0, 0));
  GetWindowRgn(hwnd(), current_rgn.get());

  RECT window_rect;
  GetWindowRect(hwnd(), &window_rect);
  base::win::ScopedRegion new_region;
  if (custom_window_region_.is_valid()) {
    new_region.reset(CreateRectRgn(0, 0, 0, 0));
    CombineRgn(new_region.get(), custom_window_region_.get(), NULL, RGN_COPY);
  } else if (IsMaximized()) {
    HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    GetMonitorInfo(monitor, &mi);
    RECT work_rect = mi.rcWork;
    OffsetRect(&work_rect, -window_rect.left, -window_rect.top);
    new_region.reset(CreateRectRgnIndirect(&work_rect));
  } else {
    gfx::Path window_mask;
    delegate_->GetWindowMask(gfx::Size(window_rect.right - window_rect.left,
                                       window_rect.bottom - window_rect.top),
                             &window_mask);
    if (!window_mask.isEmpty())
      new_region.reset(gfx::CreateHRGNFromSkPath(window_mask));
  }

  const bool has_current_region = current_rgn != 0;
  const bool has_new_region = new_region != 0;
  if (has_current_region != has_new_region ||
      (has_current_region && !EqualRgn(current_rgn.get(), new_region.get()))) {
    // SetWindowRgn takes ownership of the HRGN.
    SetWindowRgn(hwnd(), new_region.release(), redraw);
  }
}

void HWNDMessageHandler::UpdateDwmNcRenderingPolicy() {
  if (IsFullscreen())
    return;

  DWMNCRENDERINGPOLICY policy =
      custom_window_region_.is_valid() ||
              delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN
          ? DWMNCRP_DISABLED
          : DWMNCRP_ENABLED;

  DwmSetWindowAttribute(hwnd(), DWMWA_NCRENDERING_POLICY,
                        &policy, sizeof(DWMNCRENDERINGPOLICY));
}

LRESULT HWNDMessageHandler::DefWindowProcWithRedrawLock(UINT message,
                                                        WPARAM w_param,
                                                        LPARAM l_param) {
  ScopedRedrawLock lock(this);
  // The Widget and HWND can be destroyed in the call to DefWindowProc, so use
  // the WeakPtrFactory to avoid unlocking (and crashing) after destruction.
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  LRESULT result = DefWindowProc(hwnd(), message, w_param, l_param);
  if (!ref)
    lock.CancelUnlockOperation();
  return result;
}

void HWNDMessageHandler::LockUpdates(bool force) {
  // We skip locked updates when Aero is on for two reasons:
  // 1. Because it isn't necessary
  // 2. Because toggling the WS_VISIBLE flag may occur while the GPU process is
  //    attempting to present a child window's backbuffer onscreen. When these
  //    two actions race with one another, the child window will either flicker
  //    or will simply stop updating entirely.
  if ((force || !ui::win::IsAeroGlassEnabled()) && ++lock_updates_count_ == 1) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) & ~WS_VISIBLE);
  }
}

void HWNDMessageHandler::UnlockUpdates(bool force) {
  if ((force || !ui::win::IsAeroGlassEnabled()) && --lock_updates_count_ <= 0) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) | WS_VISIBLE);
    lock_updates_count_ = 0;
  }
}

void HWNDMessageHandler::ForceRedrawWindow(int attempts) {
  if (ui::IsWorkstationLocked()) {
    // Presents will continue to fail as long as the input desktop is
    // unavailable.
    if (--attempts <= 0)
      return;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&HWNDMessageHandler::ForceRedrawWindow,
                              weak_factory_.GetWeakPtr(), attempts),
        base::TimeDelta::FromMilliseconds(500));
    return;
  }
  InvalidateRect(hwnd(), NULL, FALSE);
}

bool HWNDMessageHandler::HasSystemFrame() const {
  return delegate_->HasFrame() &&
         delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN;
}

// Message handlers ------------------------------------------------------------

void HWNDMessageHandler::OnActivateApp(BOOL active, DWORD thread_id) {
  if (delegate_->HasNonClientView() && !active &&
      thread_id != GetCurrentThreadId()) {
    delegate_->HandleAppDeactivated();
    // Also update the native frame if it is rendering the non-client area.
    if (HasSystemFrame())
      DefWindowProcWithRedrawLock(WM_NCACTIVATE, FALSE, 0);
  }
}

BOOL HWNDMessageHandler::OnAppCommand(HWND window,
                                      short command,
                                      WORD device,
                                      int keystate) {
  BOOL handled = !!delegate_->HandleAppCommand(command);
  SetMsgHandled(handled);
  // Make sure to return TRUE if the event was handled or in some cases the
  // system will execute the default handler which can cause bugs like going
  // forward or back two pages instead of one.
  return handled;
}

void HWNDMessageHandler::OnCancelMode() {
  delegate_->HandleCancelMode();
  // Need default handling, otherwise capture and other things aren't canceled.
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnCaptureChanged(HWND window) {
  delegate_->HandleCaptureLost();
}

void HWNDMessageHandler::OnClose() {
  delegate_->HandleClose();
}

void HWNDMessageHandler::OnCommand(UINT notification_code,
                                   int command,
                                   HWND window) {
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1 || delegate_->HandleAppCommand(command))
    SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnCreate(CREATESTRUCT* create_struct) {
  if (window_ex_style() &  WS_EX_COMPOSITED) {
    // This is part of the magic to emulate layered windows with Aura
    // see the explanation elsewere when we set WS_EX_COMPOSITED style.
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd(), &margins);
  }

  fullscreen_handler_->set_hwnd(hwnd());

  // This message initializes the window so that focus border are shown for
  // windows.
  SendMessage(hwnd(),
              WM_CHANGEUISTATE,
              MAKELPARAM(UIS_CLEAR, UISF_HIDEFOCUS),
              0);

  if (!delegate_->HasFrame()) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) & ~WS_CAPTION);
    SendFrameChanged();
  }

  // Get access to a modifiable copy of the system menu.
  GetSystemMenu(hwnd(), false);

  if (ui::AreTouchEventsEnabled())
    RegisterTouchWindow(hwnd(), TWF_WANTPALM);

  // We need to allow the delegate to size its contents since the window may not
  // receive a size notification when its initial bounds are specified at window
  // creation time.
  ClientAreaSizeChanged();

  delegate_->HandleCreate();

  windows_session_change_observer_.reset(new WindowsSessionChangeObserver(
      base::Bind(&HWNDMessageHandler::OnSessionChange,
                 base::Unretained(this))));

  // TODO(beng): move more of NWW::OnCreate here.
  return 0;
}

void HWNDMessageHandler::OnDestroy() {
  windows_session_change_observer_.reset(nullptr);
  delegate_->HandleDestroying();
  // If the window going away is a fullscreen window then remove its references
  // from the full screen window map.
  for (auto iter = fullscreen_monitor_map_.Get().begin();
       iter != fullscreen_monitor_map_.Get().end();
       iter++) {
    if (iter->second == this) {
      fullscreen_monitor_map_.Get().erase(iter);
      break;
    }
  }
}

void HWNDMessageHandler::OnDisplayChange(UINT bits_per_pixel,
                                         const gfx::Size& screen_size) {
  delegate_->HandleDisplayChange();
  // Force a WM_NCCALCSIZE to occur to ensure that we handle auto hide
  // taskbars correctly.
  SendFrameChanged();
}

LRESULT HWNDMessageHandler::OnDwmCompositionChanged(UINT msg,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  if (!delegate_->HasNonClientView()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  FrameTypeChanged();
  return 0;
}

LRESULT HWNDMessageHandler::OnDpiChanged(UINT msg,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  if (LOWORD(w_param) != HIWORD(w_param))
    NOTIMPLEMENTED() << "Received non-square scaling factors";

  SetBoundsInternal(gfx::Rect(*reinterpret_cast<RECT*>(l_param)), false);
  delegate_->HandleWindowScaleFactorChanged(
      display::win::GetScalingFactorFromDPI(LOWORD(w_param)));
  return 0;
}

void HWNDMessageHandler::OnEnterMenuLoop(BOOL from_track_popup_menu) {
  if (menu_depth_++ == 0)
    delegate_->HandleMenuLoop(true);
}

void HWNDMessageHandler::OnEnterSizeMove() {
  // Please refer to the comments in the OnSize function about the scrollbar
  // hack.
  // Hide the Windows scrollbar if the scroll styles are present to ensure
  // that a paint flicker does not occur while sizing.
  if (in_size_loop_ && needs_scroll_styles_)
    ShowScrollBar(hwnd(), SB_BOTH, FALSE);

  delegate_->HandleBeginWMSizeMove();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnEraseBkgnd(HDC dc) {
  // Needed to prevent resize flicker.
  return 1;
}

void HWNDMessageHandler::OnExitMenuLoop(BOOL is_shortcut_menu) {
  if (--menu_depth_ == 0)
    delegate_->HandleMenuLoop(false);
  DCHECK_GE(0, menu_depth_);
}

void HWNDMessageHandler::OnExitSizeMove() {
  delegate_->HandleEndWMSizeMove();
  SetMsgHandled(FALSE);
  // Please refer to the notes in the OnSize function for information about
  // the scrolling hack.
  // We hide the Windows scrollbar in the OnEnterSizeMove function. We need
  // to add the scroll styles back to ensure that scrolling works in legacy
  // trackpoint drivers.
  if (in_size_loop_ && needs_scroll_styles_)
    AddScrollStylesToWindow(hwnd());
  // If the window was moved to a monitor which has a fullscreen window active,
  // we need to reduce the size of the fullscreen window by 1px.
  CheckAndHandleBackgroundFullscreenOnMonitor(hwnd());
}

void HWNDMessageHandler::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  gfx::Size min_window_size;
  gfx::Size max_window_size;
  delegate_->GetMinMaxSize(&min_window_size, &max_window_size);
  min_window_size = delegate_->DIPToScreenSize(min_window_size);
  max_window_size = delegate_->DIPToScreenSize(max_window_size);


  // Add the native frame border size to the minimum and maximum size if the
  // view reports its size as the client size.
  if (delegate_->WidgetSizeIsClientSize()) {
    RECT client_rect, window_rect;
    GetClientRect(hwnd(), &client_rect);
    GetWindowRect(hwnd(), &window_rect);
    CR_DEFLATE_RECT(&window_rect, &client_rect);
    min_window_size.Enlarge(window_rect.right - window_rect.left,
                            window_rect.bottom - window_rect.top);
    // Either axis may be zero, so enlarge them independently.
    if (max_window_size.width())
      max_window_size.Enlarge(window_rect.right - window_rect.left, 0);
    if (max_window_size.height())
      max_window_size.Enlarge(0, window_rect.bottom - window_rect.top);
  }
  minmax_info->ptMinTrackSize.x = min_window_size.width();
  minmax_info->ptMinTrackSize.y = min_window_size.height();
  if (max_window_size.width() || max_window_size.height()) {
    if (!max_window_size.width())
      max_window_size.set_width(GetSystemMetrics(SM_CXMAXTRACK));
    if (!max_window_size.height())
      max_window_size.set_height(GetSystemMetrics(SM_CYMAXTRACK));
    minmax_info->ptMaxTrackSize.x = max_window_size.width();
    minmax_info->ptMaxTrackSize.y = max_window_size.height();
  }
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnGetObject(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Only the lower 32 bits of l_param are valid when checking the object id
  // because it sometimes gets sign-extended incorrectly (but not always).
  DWORD obj_id = static_cast<DWORD>(static_cast<DWORD_PTR>(l_param));

  // Accessibility readers will send an OBJID_CLIENT message
  if (static_cast<DWORD>(OBJID_CLIENT) == obj_id) {
    // Retrieve MSAA dispatch object for the root view.
    base::win::ScopedComPtr<IAccessible> root(
        delegate_->GetNativeViewAccessible());

    // Create a reference that MSAA will marshall to the client.
    reference_result = LresultFromObject(IID_IAccessible, w_param,
        static_cast<IAccessible*>(root.Detach()));
  }

  return reference_result;
}

LRESULT HWNDMessageHandler::OnImeMessages(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  LRESULT result = 0;
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  const bool msg_handled =
      delegate_->HandleIMEMessage(message, w_param, l_param, &result);
  if (ref.get())
    SetMsgHandled(msg_handled);
  return result;
}

void HWNDMessageHandler::OnInitMenu(HMENU menu) {
  bool is_fullscreen = IsFullscreen();
  bool is_minimized = IsMinimized();
  bool is_maximized = IsMaximized();
  bool is_restored = !is_fullscreen && !is_minimized && !is_maximized;

  ScopedRedrawLock lock(this);
  EnableMenuItemByCommand(menu, SC_RESTORE, delegate_->CanResize() &&
                          (is_minimized || is_maximized));
  EnableMenuItemByCommand(menu, SC_MOVE, is_restored);
  EnableMenuItemByCommand(menu, SC_SIZE, delegate_->CanResize() && is_restored);
  EnableMenuItemByCommand(menu, SC_MAXIMIZE, delegate_->CanMaximize() &&
                          !is_fullscreen && !is_maximized);
  EnableMenuItemByCommand(menu, SC_MINIMIZE, delegate_->CanMinimize() &&
                          !is_minimized);

  if (is_maximized && delegate_->CanResize())
    ::SetMenuDefaultItem(menu, SC_RESTORE, FALSE);
  else if (!is_maximized && delegate_->CanMaximize())
    ::SetMenuDefaultItem(menu, SC_MAXIMIZE, FALSE);
}

void HWNDMessageHandler::OnInputLangChange(DWORD character_set,
                                           HKL input_language_id) {
  delegate_->HandleInputLanguageChange(character_set, input_language_id);
}

LRESULT HWNDMessageHandler::OnKeyEvent(UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  MSG msg = {
      hwnd(), message, w_param, l_param, static_cast<DWORD>(GetMessageTime())};
  ui::KeyEvent key(msg);
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  delegate_->HandleKeyEvent(&key);
  if (!ref)
    return 0;
  if (!key.handled())
    SetMsgHandled(FALSE);
  return 0;
}

void HWNDMessageHandler::OnKillFocus(HWND focused_window) {
  delegate_->HandleNativeBlur(focused_window);
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnMouseActivate(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  // Please refer to the comments in the header for the touch_down_contexts_
  // member for the if statement below.
  if (touch_down_contexts_)
    return MA_NOACTIVATE;

  // On Windows, if we select the menu item by touch and if the window at the
  // location is another window on the same thread, that window gets a
  // WM_MOUSEACTIVATE message and ends up activating itself, which is not
  // correct. We workaround this by setting a property on the window at the
  // current cursor location. We check for this property in our
  // WM_MOUSEACTIVATE handler and don't activate the window if the property is
  // set.
  if (::GetProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow)) {
    ::RemoveProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow);
    return MA_NOACTIVATE;
  }

  // TODO(beng): resolve this with the GetWindowLong() check on the subsequent
  //             line.
  if (delegate_->HasNonClientView()) {
    if (delegate_->CanActivate())
      return MA_ACTIVATE;
    if (delegate_->WantsMouseEventsWhenInactive())
      return MA_NOACTIVATE;
    return MA_NOACTIVATEANDEAT;
  }
  if (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)
    return MA_NOACTIVATE;
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

LRESULT HWNDMessageHandler::OnMouseRange(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  return HandleMouseEventInternal(message, w_param, l_param, true);
}

// On some systems with a high-resolution track pad and running Windows 10,
// using the scrolling gesture (two-finger scroll) on the track pad
// causes it to also generate a WM_POINTERDOWN message if the window
// isn't focused. This leads to a WM_POINTERACTIVATE message and the window
// gaining focus and coming to the front. This code detects a
// WM_POINTERACTIVATE coming from the track pad and kills the activation
// of the window. NOTE: most other trackpad messages come in as mouse
// messages, including WM_MOUSEWHEEL instead of WM_POINTERWHEEL.
LRESULT HWNDMessageHandler::OnPointerActivate(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  using GetPointerTypeFn = BOOL(WINAPI*)(UINT32, POINTER_INPUT_TYPE*);
  UINT32 pointer_id = GET_POINTERID_WPARAM(w_param);
  POINTER_INPUT_TYPE pointer_type;
  static GetPointerTypeFn get_pointer_type = reinterpret_cast<GetPointerTypeFn>(
      GetProcAddress(GetModuleHandleA("user32.dll"), "GetPointerType"));
  if (get_pointer_type && get_pointer_type(pointer_id, &pointer_type) &&
      pointer_type == PT_TOUCHPAD)
    return PA_NOACTIVATE;
  SetMsgHandled(FALSE);
  return -1;
}

void HWNDMessageHandler::OnMove(const gfx::Point& point) {
  delegate_->HandleMove();
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnMoving(UINT param, const RECT* new_bounds) {
  delegate_->HandleMove();
}

LRESULT HWNDMessageHandler::OnNCActivate(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  // Per MSDN, w_param is either TRUE or FALSE. However, MSDN also hints that:
  // "If the window is minimized when this message is received, the application
  // should pass the message to the DefWindowProc function."
  // It is found out that the high word of w_param might be set when the window
  // is minimized or restored. To handle this, w_param's high word should be
  // cleared before it is converted to BOOL.
  BOOL active = static_cast<BOOL>(LOWORD(w_param));

  bool render_as_active = delegate_->IsAlwaysRenderAsActive();

  if (!delegate_->HasNonClientView()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  if (!delegate_->CanActivate())
    return TRUE;

  // On activation, lift any prior restriction against rendering as inactive.
  if (active && render_as_active)
    delegate_->SetAlwaysRenderAsActive(false);

  if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    // TODO(beng, et al): Hack to redraw this window and child windows
    //     synchronously upon activation. Not all child windows are redrawing
    //     themselves leading to issues like http://crbug.com/74604
    //     We redraw out-of-process HWNDs asynchronously to avoid hanging the
    //     whole app if a child HWND belonging to a hung plugin is encountered.
    RedrawWindow(hwnd(), NULL, NULL,
                 RDW_NOCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW);
    EnumChildWindows(hwnd(), EnumChildWindowsForRedraw, NULL);
  }

  // The frame may need to redraw as a result of the activation change.
  // We can get WM_NCACTIVATE before we're actually visible. If we're not
  // visible, no need to paint.
  if (IsVisible())
    delegate_->SchedulePaint();

  if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    SetMsgHandled(TRUE);
    return TRUE;
  }

  return DefWindowProcWithRedrawLock(
      WM_NCACTIVATE, render_as_active || active, 0);
}

LRESULT HWNDMessageHandler::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We only override the default handling if we need to specify a custom
  // non-client edge width. Note that in most cases "no insets" means no
  // custom width, but in fullscreen mode or when the NonClientFrameView
  // requests it, we want a custom width of 0.

  // Let User32 handle the first nccalcsize for captioned windows
  // so it updates its internal structures (specifically caption-present)
  // Without this Tile & Cascade windows won't work.
  // See http://code.google.com/p/chromium/issues/detail?id=900
  if (is_first_nccalc_) {
    is_first_nccalc_ = false;
    if (GetWindowLong(hwnd(), GWL_STYLE) & WS_CAPTION) {
      SetMsgHandled(FALSE);
      return 0;
    }
  }

  gfx::Insets insets;
  bool got_insets = GetClientAreaInsets(&insets);
  if (!got_insets && !IsFullscreen() && !(mode && !delegate_->HasFrame())) {
    SetMsgHandled(FALSE);
    return 0;
  }

  RECT* client_rect = mode ?
      &(reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0]) :
      reinterpret_cast<RECT*>(l_param);
  client_rect->left += insets.left();
  client_rect->top += insets.top();
  client_rect->bottom -= insets.bottom();
  client_rect->right -= insets.right();
  if (IsMaximized()) {
    // Find all auto-hide taskbars along the screen edges and adjust in by the
    // thickness of the auto-hide taskbar on each such edge, so the window isn't
    // treated as a "fullscreen app", which would cause the taskbars to
    // disappear.
    HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONULL);
    if (!monitor) {
      // We might end up here if the window was previously minimized and the
      // user clicks on the taskbar button to restore it in the previously
      // maximized position. In that case WM_NCCALCSIZE is sent before the
      // window coordinates are restored to their previous values, so our
      // (left,top) would probably be (-32000,-32000) like all minimized
      // windows. So the above MonitorFromWindow call fails, but if we check
      // the window rect given with WM_NCCALCSIZE (which is our previous
      // restored window position) we will get the correct monitor handle.
      monitor = MonitorFromRect(client_rect, MONITOR_DEFAULTTONULL);
      if (!monitor) {
        // This is probably an extreme case that we won't hit, but if we don't
        // intersect any monitor, let us not adjust the client rect since our
        // window will not be visible anyway.
        return 0;
      }
    }
    const int autohide_edges = GetAppbarAutohideEdges(monitor);
    if (autohide_edges & ViewsDelegate::EDGE_LEFT)
      client_rect->left += kAutoHideTaskbarThicknessPx;
    if (autohide_edges & ViewsDelegate::EDGE_TOP) {
      if (delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN) {
        // Tricky bit.  Due to a bug in DwmDefWindowProc()'s handling of
        // WM_NCHITTEST, having any nonclient area atop the window causes the
        // caption buttons to draw onscreen but not respond to mouse
        // hover/clicks.
        // So for a taskbar at the screen top, we can't push the
        // client_rect->top down; instead, we move the bottom up by one pixel,
        // which is the smallest change we can make and still get a client area
        // less than the screen size. This is visibly ugly, but there seems to
        // be no better solution.
        --client_rect->bottom;
      } else {
        client_rect->top += kAutoHideTaskbarThicknessPx;
      }
    }
    if (autohide_edges & ViewsDelegate::EDGE_RIGHT)
      client_rect->right -= kAutoHideTaskbarThicknessPx;
    if (autohide_edges & ViewsDelegate::EDGE_BOTTOM)
      client_rect->bottom -= kAutoHideTaskbarThicknessPx;

    // We cannot return WVR_REDRAW when there is nonclient area, or Windows
    // exhibits bugs where client pixels and child HWNDs are mispositioned by
    // the width/height of the upper-left nonclient area.
    return 0;
  }

  // If the window bounds change, we're going to relayout and repaint anyway.
  // Returning WVR_REDRAW avoids an extra paint before that of the old client
  // pixels in the (now wrong) location, and thus makes actions like resizing a
  // window from the left edge look slightly less broken.
  // We special case when left or top insets are 0, since these conditions
  // actually require another repaint to correct the layout after glass gets
  // turned on and off.
  if (insets.left() == 0 || insets.top() == 0)
    return 0;
  return mode ? WVR_REDRAW : 0;
}

LRESULT HWNDMessageHandler::OnNCHitTest(const gfx::Point& point) {
  if (!delegate_->HasNonClientView()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  // Some views may overlap the non client area of the window.
  // This means that we should look for these views before handing the
  // hittest message off to DWM or DefWindowProc.
  // If the hittest returned from the search for a view returns HTCLIENT
  // then it means that we have a view overlapping the non client area.
  // In all other cases we can fallback to the system default handling.

  // Allow the NonClientView to handle the hittest to see if we have a view
  // overlapping the non client area of the window.
  POINT temp = { point.x(), point.y() };
  MapWindowPoints(HWND_DESKTOP, hwnd(), &temp, 1);
  int component = delegate_->GetNonClientComponent(gfx::Point(temp));
  if (component == HTCLIENT)
    return component;

  // If the DWM is rendering the window controls, we need to give the DWM's
  // default window procedure first chance to handle hit testing.
  if (HasSystemFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(hwnd(), WM_NCHITTEST, 0,
                         MAKELPARAM(point.x(), point.y()), &result)) {
      return result;
    }
  }

  // If the point is specified as custom or system nonclient item, return it.
  if (component != HTNOWHERE)
    return component;

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  LRESULT hit_test_code = DefWindowProc(hwnd(), WM_NCHITTEST, 0,
                                        MAKELPARAM(point.x(), point.y()));
  if (needs_scroll_styles_) {
    switch (hit_test_code) {
      // If we faked the WS_VSCROLL and WS_HSCROLL styles for this window, then
      // Windows returns the HTVSCROLL or HTHSCROLL hit test codes if we hover
      // or click on the non client portions of the window where the OS
      // scrollbars would be drawn. These hittest codes are returned even when
      // the scrollbars are hidden, which is the case in Aura. We fake the
      // hittest code as HTCLIENT in this case to ensure that we receive client
      // mouse messages as opposed to non client mouse messages.
      case HTVSCROLL:
      case HTHSCROLL:
        hit_test_code = HTCLIENT;
        break;

      case HTBOTTOMRIGHT: {
        // Normally the HTBOTTOMRIGHT hittest code is received when we hover
        // near the bottom right of the window. However due to our fake scroll
        // styles, we get this code even when we hover around the area where
        // the vertical scrollar down arrow would be drawn.
        // We check if the hittest coordinates lie in this region and if yes
        // we return HTCLIENT.
        int border_width = ::GetSystemMetrics(SM_CXSIZEFRAME);
        int border_height = ::GetSystemMetrics(SM_CYSIZEFRAME);
        int scroll_width = ::GetSystemMetrics(SM_CXVSCROLL);
        int scroll_height = ::GetSystemMetrics(SM_CYVSCROLL);
        RECT window_rect;
        ::GetWindowRect(hwnd(), &window_rect);
        window_rect.bottom -= border_height;
        window_rect.right -= border_width;
        window_rect.left = window_rect.right - scroll_width;
        window_rect.top = window_rect.bottom - scroll_height;
        POINT pt;
        pt.x = point.x();
        pt.y = point.y();
        if (::PtInRect(&window_rect, pt))
          hit_test_code = HTCLIENT;
        break;
      }

      default:
        break;
    }
  }
  return hit_test_code;
}

void HWNDMessageHandler::OnNCPaint(HRGN rgn) {
  RECT window_rect;
  GetWindowRect(hwnd(), &window_rect);
  RECT dirty_region;
  // A value of 1 indicates paint all.
  if (!rgn || rgn == reinterpret_cast<HRGN>(1)) {
    dirty_region.left = 0;
    dirty_region.top = 0;
    dirty_region.right = window_rect.right - window_rect.left;
    dirty_region.bottom = window_rect.bottom - window_rect.top;
  } else {
    RECT rgn_bounding_box;
    GetRgnBox(rgn, &rgn_bounding_box);
    if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect)) {
      SetMsgHandled(FALSE);
      return;  // Dirty region doesn't intersect window bounds, bail.
    }

    // rgn_bounding_box is in screen coordinates. Map it to window coordinates.
    OffsetRect(&dirty_region, -window_rect.left, -window_rect.top);
  }

  // We only do non-client painting if we're not using the system frame.
  // It's required to avoid some native painting artifacts from appearing when
  // the window is resized.
  if (!delegate_->HasNonClientView() ||
      delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN) {
    if (ui::win::IsAeroGlassEnabled()) {
      // The default WM_NCPAINT handler under Aero Glass doesn't clear the
      // nonclient area, so it'll remain the default white color. That area is
      // invisible initially (covered by the window border) but can become
      // temporarily visible on maximizing or fullscreening, so clear it here.
      HDC dc = GetWindowDC(hwnd());
      RECT client_rect;
      ::GetClientRect(hwnd(), &client_rect);
      ::MapWindowPoints(hwnd(), nullptr, reinterpret_cast<POINT*>(&client_rect),
                        2);
      ::OffsetRect(&client_rect, -window_rect.left, -window_rect.top);
      // client_rect now is in window space.

      base::win::ScopedRegion base(::CreateRectRgnIndirect(&dirty_region));
      base::win::ScopedRegion client(::CreateRectRgnIndirect(&client_rect));
      base::win::ScopedRegion nonclient(::CreateRectRgn(0, 0, 0, 0));
      ::CombineRgn(nonclient.get(), base.get(), client.get(), RGN_DIFF);

      ::SelectClipRgn(dc, nonclient.get());
      HBRUSH brush = CreateSolidBrush(0);
      ::FillRect(dc, &dirty_region, brush);
      ::DeleteObject(brush);
      ::ReleaseDC(hwnd(), dc);
    }
    SetMsgHandled(FALSE);
    return;
  }

  gfx::Size root_view_size = delegate_->GetRootViewSize();
  if (gfx::Size(window_rect.right - window_rect.left,
                window_rect.bottom - window_rect.top) != root_view_size) {
    // If the size of the window differs from the size of the root view it
    // means we're being asked to paint before we've gotten a WM_SIZE. This can
    // happen when the user is interactively resizing the window. To avoid
    // mass flickering we don't do anything here. Once we get the WM_SIZE we'll
    // reset the region of the window which triggers another WM_NCPAINT and
    // all is well.
    return;
  }
  delegate_->HandlePaintAccelerated(gfx::Rect(dirty_region));

  // When using a custom frame, we want to avoid calling DefWindowProc() since
  // that may render artifacts.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
}

LRESULT HWNDMessageHandler::OnNCUAHDrawCaption(UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
  return 0;
}

LRESULT HWNDMessageHandler::OnNCUAHDrawFrame(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
  return 0;
}

LRESULT HWNDMessageHandler::OnNotify(int w_param, NMHDR* l_param) {
  LRESULT l_result = 0;
  SetMsgHandled(delegate_->HandleTooltipNotify(w_param, l_param, &l_result));
  return l_result;
}

void HWNDMessageHandler::OnPaint(HDC dc) {
  // Call BeginPaint()/EndPaint() around the paint handling, as that seems
  // to do more to actually validate the window's drawing region. This only
  // appears to matter for Windows that have the WS_EX_COMPOSITED style set
  // but will be valid in general too.
  PAINTSTRUCT ps;
  HDC display_dc = BeginPaint(hwnd(), &ps);

  if (!display_dc) {
    // Collect some information as to why this may have happened and preserve
    // it on the stack so it shows up in a dump.
    // This is temporary data collection code in service of
    // http://crbug.com/512945
    DWORD last_error = GetLastError();
    size_t current_gdi_objects =
        GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    size_t peak_gdi_objects = GetGuiResources(
        GetCurrentProcess(), GR_GDIOBJECTS_PEAK);
    base::debug::Alias(&last_error);
    base::debug::Alias(&current_gdi_objects);
    base::debug::Alias(&peak_gdi_objects);

    LOG(FATAL) << "Failed to create DC in BeginPaint(). GLE = " << last_error
               << ", GDI object count: " << current_gdi_objects
               << ", GDI peak count: " << peak_gdi_objects;
  }

  if (!IsRectEmpty(&ps.rcPaint)) {
    if (HasChildRenderingWindow()) {
      // If there's a child window that's being rendered to then clear the
      // area outside it (as WS_CLIPCHILDREN is set) with transparent black.
      // Otherwise, other portions of the backing store for the window can
      // flicker opaque black. http://crbug.com/586454

      FillRect(ps.hdc, &ps.rcPaint,
               reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    }
    delegate_->HandlePaintAccelerated(gfx::Rect(ps.rcPaint));
  }

  EndPaint(hwnd(), &ps);
}

LRESULT HWNDMessageHandler::OnReflectedMessage(UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT HWNDMessageHandler::OnScrollMessage(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  MSG msg = {
      hwnd(), message, w_param, l_param, static_cast<DWORD>(GetMessageTime())};
  ui::ScrollEvent event(msg);
  delegate_->HandleScrollEvent(event);
  return 0;
}

LRESULT HWNDMessageHandler::OnSetCursor(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  // Reimplement the necessary default behavior here. Calling DefWindowProc can
  // trigger weird non-client painting for non-glass windows with custom frames.
  // Using a ScopedRedrawLock to prevent caption rendering artifacts may allow
  // content behind this window to incorrectly paint in front of this window.
  // Invalidating the window to paint over either set of artifacts is not ideal.
  wchar_t* cursor = IDC_ARROW;
  switch (LOWORD(l_param)) {
    case HTSIZE:
      cursor = IDC_SIZENWSE;
      break;
    case HTLEFT:
    case HTRIGHT:
      cursor = IDC_SIZEWE;
      break;
    case HTTOP:
    case HTBOTTOM:
      cursor = IDC_SIZENS;
      break;
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
      cursor = IDC_SIZENWSE;
      break;
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
      cursor = IDC_SIZENESW;
      break;
    case HTCLIENT:
      SetCursor(current_cursor_);
      return 1;
    case LOWORD(HTERROR):  // Use HTERROR's LOWORD value for valid comparison.
      SetMsgHandled(FALSE);
      break;
    default:
      // Use the default value, IDC_ARROW.
      break;
  }
  ::SetCursor(LoadCursor(NULL, cursor));
  return 1;
}

void HWNDMessageHandler::OnSetFocus(HWND last_focused_window) {
  delegate_->HandleNativeFocus(last_focused_window);
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnSetIcon(UINT size_type, HICON new_icon) {
  // Use a ScopedRedrawLock to avoid weird non-client painting.
  return DefWindowProcWithRedrawLock(WM_SETICON, size_type,
                                     reinterpret_cast<LPARAM>(new_icon));
}

LRESULT HWNDMessageHandler::OnSetText(const wchar_t* text) {
  // Use a ScopedRedrawLock to avoid weird non-client painting.
  return DefWindowProcWithRedrawLock(WM_SETTEXT, NULL,
                                     reinterpret_cast<LPARAM>(text));
}

void HWNDMessageHandler::OnSettingChange(UINT flags, const wchar_t* section) {
  if (!GetParent(hwnd()) && (flags == SPI_SETWORKAREA) &&
      !delegate_->WillProcessWorkAreaChange()) {
    // Fire a dummy SetWindowPos() call, so we'll trip the code in
    // OnWindowPosChanging() below that notices work area changes.
    ::SetWindowPos(hwnd(), 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
        SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    SetMsgHandled(TRUE);
  } else {
    if (flags == SPI_SETWORKAREA)
      delegate_->HandleWorkAreaChanged();
    SetMsgHandled(FALSE);
  }

  // If the work area is changing, then it could be as a result of the taskbar
  // broadcasting the WM_SETTINGCHANGE message due to changes in auto hide
  // settings, etc. Force a WM_NCCALCSIZE to occur to ensure that we handle
  // this correctly.
  if (flags == SPI_SETWORKAREA)
    SendFrameChanged();
}

void HWNDMessageHandler::OnSize(UINT param, const gfx::Size& size) {
  RedrawWindow(hwnd(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been laid out.
  ResetWindowRegion(false, true);

  // We add the WS_VSCROLL and WS_HSCROLL styles to top level windows to ensure
  // that legacy trackpad/trackpoint drivers generate the WM_VSCROLL and
  // WM_HSCROLL messages and scrolling works.
  // We want the scroll styles to be present on the window. However we don't
  // want Windows to draw the scrollbars. To achieve this we hide the scroll
  // bars and readd them to the window style in a posted task to ensure that we
  // don't get nested WM_SIZE messages.
  if (needs_scroll_styles_ && !in_size_loop_) {
    ShowScrollBar(hwnd(), SB_BOTH, FALSE);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&AddScrollStylesToWindow, hwnd()));
  }
}

void HWNDMessageHandler::OnSysCommand(UINT notification_code,
                                      const gfx::Point& point) {
  if (!delegate_->ShouldHandleSystemCommands())
    return;

  // Windows uses the 4 lower order bits of |notification_code| for type-
  // specific information so we must exclude this when comparing.
  static const int sc_mask = 0xFFF0;
  // Ignore size/move/maximize in fullscreen mode.
  if (IsFullscreen() && (((notification_code & sc_mask) == SC_SIZE) ||
                         ((notification_code & sc_mask) == SC_MOVE) ||
                         ((notification_code & sc_mask) == SC_MAXIMIZE)))
    return;
  if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    if ((notification_code & sc_mask) == SC_MINIMIZE ||
        (notification_code & sc_mask) == SC_MAXIMIZE ||
        (notification_code & sc_mask) == SC_RESTORE) {
      delegate_->ResetWindowControls();
    } else if ((notification_code & sc_mask) == SC_MOVE ||
               (notification_code & sc_mask) == SC_SIZE) {
      if (!IsVisible()) {
        // Circumvent ScopedRedrawLocks and force visibility before entering a
        // resize or move modal loop to get continuous sizing/moving feedback.
        SetWindowLong(hwnd(), GWL_STYLE,
                      GetWindowLong(hwnd(), GWL_STYLE) | WS_VISIBLE);
      }
    }
  }

  // Handle SC_KEYMENU, which means that the user has pressed the ALT
  // key and released it, so we should focus the menu bar.
  if ((notification_code & sc_mask) == SC_KEYMENU && point.x() == 0) {
    int modifiers = ui::EF_NONE;
    if (ui::win::IsShiftPressed())
      modifiers |= ui::EF_SHIFT_DOWN;
    if (ui::win::IsCtrlPressed())
      modifiers |= ui::EF_CONTROL_DOWN;
    // Retrieve the status of shift and control keys to prevent consuming
    // shift+alt keys, which are used by Windows to change input languages.
    ui::Accelerator accelerator(ui::KeyboardCodeForWindowsKeyCode(VK_MENU),
                                modifiers);
    delegate_->HandleAccelerator(accelerator);
    return;
  }

  // If the delegate can't handle it, the system implementation will be called.
  if (!delegate_->HandleCommand(notification_code)) {
    // If the window is being resized by dragging the borders of the window
    // with the mouse/touch/keyboard, we flag as being in a size loop.
    if ((notification_code & sc_mask) == SC_SIZE)
      in_size_loop_ = true;
    base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());

    DefWindowProc(hwnd(), WM_SYSCOMMAND, notification_code,
                  MAKELPARAM(point.x(), point.y()));

    if (!ref.get())
      return;
    in_size_loop_ = false;
  }
}

void HWNDMessageHandler::OnThemeChanged() {
  ui::NativeThemeWin::instance()->CloseHandles();
}

LRESULT HWNDMessageHandler::OnTouchEvent(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  // Handle touch events only on Aura for now.
  int num_points = LOWORD(w_param);
  std::unique_ptr<TOUCHINPUT[]> input(new TOUCHINPUT[num_points]);
  if (ui::GetTouchInputInfoWrapper(reinterpret_cast<HTOUCHINPUT>(l_param),
                                   num_points, input.get(),
                                   sizeof(TOUCHINPUT))) {
    // input[i].dwTime doesn't necessarily relate to the system time at all,
    // so use base::TimeTicks::Now().
    const base::TimeTicks event_time = base::TimeTicks::Now();
    TouchEvents touch_events;
    for (int i = 0; i < num_points; ++i) {
      POINT point;
      point.x = TOUCH_COORD_TO_PIXEL(input[i].x);
      point.y = TOUCH_COORD_TO_PIXEL(input[i].y);

      if (base::win::GetVersion() == base::win::VERSION_WIN7) {
        // Windows 7 sends touch events for touches in the non-client area,
        // whereas Windows 8 does not. In order to unify the behaviour, always
        // ignore touch events in the non-client area.
        LPARAM l_param_ht = MAKELPARAM(point.x, point.y);
        LRESULT hittest = SendMessage(hwnd(), WM_NCHITTEST, 0, l_param_ht);

        if (hittest != HTCLIENT)
          return 0;
      }

      ScreenToClient(hwnd(), &point);

      last_touch_message_time_ = ::GetMessageTime();

      gfx::Point touch_point(point.x, point.y);
      unsigned int touch_id = id_generator_.GetGeneratedID(input[i].dwID);

      if (input[i].dwFlags & TOUCHEVENTF_DOWN) {
        touch_ids_.insert(input[i].dwID);
        GenerateTouchEvent(ui::ET_TOUCH_PRESSED, touch_point, touch_id,
                           event_time, &touch_events);
        touch_down_contexts_++;
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE, base::Bind(&HWNDMessageHandler::ResetTouchDownContext,
                                  weak_factory_.GetWeakPtr()),
            base::TimeDelta::FromMilliseconds(kTouchDownContextResetTimeout));
      } else {
        if (input[i].dwFlags & TOUCHEVENTF_MOVE) {
          GenerateTouchEvent(ui::ET_TOUCH_MOVED, touch_point, touch_id,
                             event_time, &touch_events);
        }

        if (input[i].dwFlags & TOUCHEVENTF_UP) {
          touch_ids_.erase(input[i].dwID);
          GenerateTouchEvent(ui::ET_TOUCH_RELEASED, touch_point, touch_id,
                             event_time, &touch_events);
          id_generator_.ReleaseNumber(input[i].dwID);
        }
      }
    }
    // Handle the touch events asynchronously. We need this because touch
    // events on windows don't fire if we enter a modal loop in the context of
    // a touch event.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&HWNDMessageHandler::HandleTouchEvents,
                              weak_factory_.GetWeakPtr(), touch_events));
  }
  CloseTouchInputHandle(reinterpret_cast<HTOUCHINPUT>(l_param));
  SetMsgHandled(FALSE);
  return 0;
}

void HWNDMessageHandler::OnWindowPosChanging(WINDOWPOS* window_pos) {
  if (ignore_window_pos_changes_) {
    // If somebody's trying to toggle our visibility, change the nonclient area,
    // change our Z-order, or activate us, we should probably let it go through.
    if (!(window_pos->flags & ((IsVisible() ? SWP_HIDEWINDOW : SWP_SHOWWINDOW) |
        SWP_FRAMECHANGED)) &&
        (window_pos->flags & (SWP_NOZORDER | SWP_NOACTIVATE))) {
      // Just sizing/moving the window; ignore.
      window_pos->flags |= SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW;
      window_pos->flags &= ~(SWP_SHOWWINDOW | SWP_HIDEWINDOW);
    }
  } else if (!GetParent(hwnd())) {
    RECT window_rect;
    HMONITOR monitor;
    gfx::Rect monitor_rect, work_area;
    if (GetWindowRect(hwnd(), &window_rect) &&
        GetMonitorAndRects(window_rect, &monitor, &monitor_rect, &work_area)) {
      bool work_area_changed = (monitor_rect == last_monitor_rect_) &&
                               (work_area != last_work_area_);
      // If the size of a background fullscreen window changes again, then we
      // should reset the |background_fullscreen_hack_| flag.
      if (background_fullscreen_hack_ &&
          (!(window_pos->flags & SWP_NOSIZE) &&
            (monitor_rect.height() - window_pos->cy != 1))) {
          background_fullscreen_hack_ = false;
      }
      if (monitor && (monitor == last_monitor_) &&
          ((IsFullscreen() && !background_fullscreen_hack_) ||
           work_area_changed)) {
        // A rect for the monitor we're on changed.  Normally Windows notifies
        // us about this (and thus we're reaching here due to the SetWindowPos()
        // call in OnSettingChange() above), but with some software (e.g.
        // nVidia's nView desktop manager) the work area can change asynchronous
        // to any notification, and we're just sent a SetWindowPos() call with a
        // new (frequently incorrect) position/size.  In either case, the best
        // response is to throw away the existing position/size information in
        // |window_pos| and recalculate it based on the new work rect.
        gfx::Rect new_window_rect;
        if (IsFullscreen()) {
          new_window_rect = monitor_rect;
        } else if (IsMaximized()) {
          new_window_rect = work_area;
          int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
          new_window_rect.Inset(-border_thickness, -border_thickness);
        } else {
          new_window_rect = gfx::Rect(window_rect);
          new_window_rect.AdjustToFit(work_area);
        }
        window_pos->x = new_window_rect.x();
        window_pos->y = new_window_rect.y();
        window_pos->cx = new_window_rect.width();
        window_pos->cy = new_window_rect.height();
        // WARNING!  Don't set SWP_FRAMECHANGED here, it breaks moving the child
        // HWNDs for some reason.
        window_pos->flags &= ~(SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW);
        window_pos->flags |= SWP_NOCOPYBITS;

        // Now ignore all immediately-following SetWindowPos() changes.  Windows
        // likes to (incorrectly) recalculate what our position/size should be
        // and send us further updates.
        ignore_window_pos_changes_ = true;
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::Bind(&HWNDMessageHandler::StopIgnoringPosChanges,
                                  weak_factory_.GetWeakPtr()));
      }
      last_monitor_ = monitor;
      last_monitor_rect_ = monitor_rect;
      last_work_area_ = work_area;
    }
  }

  RECT window_rect;
  gfx::Size old_size;
  if (GetWindowRect(hwnd(), &window_rect))
    old_size = gfx::Rect(window_rect).size();
  gfx::Size new_size = gfx::Size(window_pos->cx, window_pos->cy);
  if ((old_size != new_size && !(window_pos->flags & SWP_NOSIZE)) ||
      window_pos->flags & SWP_FRAMECHANGED) {
    delegate_->HandleWindowSizeChanging();
    sent_window_size_changing_ = true;
  }

  if (ScopedFullscreenVisibility::IsHiddenForFullscreen(hwnd())) {
    // Prevent the window from being made visible if we've been asked to do so.
    // See comment in header as to why we might want this.
    window_pos->flags &= ~SWP_SHOWWINDOW;
  }

  if (window_pos->flags & SWP_SHOWWINDOW)
    delegate_->HandleVisibilityChanging(true);
  else if (window_pos->flags & SWP_HIDEWINDOW) {
    SetDwmFrameExtension(DwmFrameState::OFF);
    delegate_->HandleVisibilityChanging(false);
  }

  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (DidClientAreaSizeChange(window_pos))
    ClientAreaSizeChanged();
  if (window_pos->flags & SWP_FRAMECHANGED)
    SetDwmFrameExtension(DwmFrameState::ON);
  if (window_pos->flags & SWP_SHOWWINDOW) {
    delegate_->HandleVisibilityChanged(true);
    if (direct_manipulation_helper_)
      direct_manipulation_helper_->Activate(hwnd());
    SetDwmFrameExtension(DwmFrameState::ON);
  } else if (window_pos->flags & SWP_HIDEWINDOW) {
    delegate_->HandleVisibilityChanged(false);
    if (direct_manipulation_helper_)
      direct_manipulation_helper_->Deactivate(hwnd());
  }
  if (sent_window_size_changing_) {
    sent_window_size_changing_ = false;
    delegate_->HandleWindowSizeChanged();
  }
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnSessionChange(WPARAM status_code) {
  // Direct3D presents are ignored while the screen is locked, so force the
  // window to be redrawn on unlock.
  if (status_code == WTS_SESSION_UNLOCK)
    ForceRedrawWindow(10);
}

void HWNDMessageHandler::HandleTouchEvents(const TouchEvents& touch_events) {
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  for (size_t i = 0; i < touch_events.size() && ref; ++i)
    delegate_->HandleTouchEvent(touch_events[i]);
}

void HWNDMessageHandler::ResetTouchDownContext() {
  touch_down_contexts_--;
}

LRESULT HWNDMessageHandler::HandleMouseEventInternal(UINT message,
                                                     WPARAM w_param,
                                                     LPARAM l_param,
                                                     bool track_mouse) {
  // We handle touch events on Windows Aura. Windows generates synthesized
  // mouse messages in response to touch which we should ignore. However touch
  // messages are only received for the client area. We need to ignore the
  // synthesized mouse messages for all points in the client area and places
  // which return HTNOWHERE.
  // TODO(ananta)
  // Windows does not reliably set the touch flag on mouse messages. Look into
  // a better way of identifying mouse messages originating from touch.
  if (ui::IsMouseEventFromTouch(message)) {
    LPARAM l_param_ht = l_param;
    // For mouse events (except wheel events), location is in window coordinates
    // and should be converted to screen coordinates for WM_NCHITTEST.
    if (message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL) {
      POINT screen_point = CR_POINT_INITIALIZER_FROM_LPARAM(l_param_ht);
      MapWindowPoints(hwnd(), HWND_DESKTOP, &screen_point, 1);
      l_param_ht = MAKELPARAM(screen_point.x, screen_point.y);
    }
    LRESULT hittest = SendMessage(hwnd(), WM_NCHITTEST, 0, l_param_ht);
    if (hittest == HTCLIENT || hittest == HTNOWHERE)
      return 0;
  }

  // Certain logitech drivers send the WM_MOUSEHWHEEL message to the parent
  // followed by WM_MOUSEWHEEL messages to the child window causing a vertical
  // scroll. We treat these WM_MOUSEWHEEL messages as WM_MOUSEHWHEEL
  // messages.
  if (message == WM_MOUSEHWHEEL)
    last_mouse_hwheel_time_ = ::GetMessageTime();

  if (message == WM_MOUSEWHEEL &&
      ::GetMessageTime() == last_mouse_hwheel_time_) {
    message = WM_MOUSEHWHEEL;
  }

  if (message == WM_RBUTTONUP && is_right_mouse_pressed_on_caption_) {
    is_right_mouse_pressed_on_caption_ = false;
    ReleaseCapture();
    // |point| is in window coordinates, but WM_NCHITTEST and TrackPopupMenu()
    // expect screen coordinates.
    POINT screen_point = CR_POINT_INITIALIZER_FROM_LPARAM(l_param);
    MapWindowPoints(hwnd(), HWND_DESKTOP, &screen_point, 1);
    w_param = SendMessage(hwnd(), WM_NCHITTEST, 0,
                          MAKELPARAM(screen_point.x, screen_point.y));
    if (w_param == HTCAPTION || w_param == HTSYSMENU) {
      gfx::ShowSystemMenuAtPoint(hwnd(), gfx::Point(screen_point));
      return 0;
    }
  } else if (message == WM_NCLBUTTONDOWN &&
             delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    switch (w_param) {
      case HTCLOSE:
      case HTMINBUTTON:
      case HTMAXBUTTON: {
        // When the mouse is pressed down in these specific non-client areas,
        // we need to tell the RootView to send the mouse pressed event (which
        // sets capture, allowing subsequent WM_LBUTTONUP (note, _not_
        // WM_NCLBUTTONUP) to fire so that the appropriate WM_SYSCOMMAND can be
        // sent by the applicable button's ButtonListener. We _have_ to do this
        // way rather than letting Windows just send the syscommand itself (as
        // would happen if we never did this dance) because for some insane
        // reason DefWindowProc for WM_NCLBUTTONDOWN also renders the pressed
        // window control button appearance, in the Windows classic style, over
        // our view! Ick! By handling this message we prevent Windows from
        // doing this undesirable thing, but that means we need to roll the
        // sys-command handling ourselves.
        // Combine |w_param| with common key state message flags.
        w_param |= ui::win::IsCtrlPressed() ? MK_CONTROL : 0;
        w_param |= ui::win::IsShiftPressed() ? MK_SHIFT : 0;
      }
    }
  } else if (message == WM_NCRBUTTONDOWN &&
      (w_param == HTCAPTION || w_param == HTSYSMENU)) {
    is_right_mouse_pressed_on_caption_ = true;
    // We SetCapture() to ensure we only show the menu when the button
    // down and up are both on the caption. Note: this causes the button up to
    // be WM_RBUTTONUP instead of WM_NCRBUTTONUP.
    SetCapture();
  }

  long message_time = GetMessageTime();
  MSG msg = { hwnd(), message, w_param, l_param,
              static_cast<DWORD>(message_time),
              { CR_GET_X_LPARAM(l_param), CR_GET_Y_LPARAM(l_param) } };
  ui::MouseEvent event(msg);
  if (IsSynthesizedMouseMessage(message, message_time, l_param))
    event.set_flags(event.flags() | ui::EF_FROM_TOUCH);

  if (event.type() == ui::ET_MOUSE_MOVED && !HasCapture() && track_mouse) {
    // Windows only fires WM_MOUSELEAVE events if the application begins
    // "tracking" mouse events for a given HWND during WM_MOUSEMOVE events.
    // We need to call |TrackMouseEvents| to listen for WM_MOUSELEAVE.
    TrackMouseEvents((message == WM_NCMOUSEMOVE) ?
        TME_NONCLIENT | TME_LEAVE : TME_LEAVE);
  } else if (event.type() == ui::ET_MOUSE_EXITED) {
    // Reset our tracking flags so future mouse movement over this
    // NativeWidget results in a new tracking session. Fall through for
    // OnMouseEvent.
    active_mouse_tracking_flags_ = 0;
  } else if (event.type() == ui::ET_MOUSEWHEEL) {
    // Reroute the mouse wheel to the window under the pointer if applicable.
    return (ui::RerouteMouseWheel(hwnd(), w_param, l_param) ||
            delegate_->HandleMouseEvent(ui::MouseWheelEvent(msg))) ? 0 : 1;
  }

  // There are cases where the code handling the message destroys the window,
  // so use the weak ptr to check if destruction occured or not.
  base::WeakPtr<HWNDMessageHandler> ref(weak_factory_.GetWeakPtr());
  bool handled = delegate_->HandleMouseEvent(event);

  if (!ref.get())
    return 0;

  if (direct_manipulation_helper_ && track_mouse &&
      (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)) {
    direct_manipulation_helper_->HandleMouseWheel(hwnd(), message, w_param,
        l_param);
  }

  if (!handled && message == WM_NCLBUTTONDOWN && w_param != HTSYSMENU &&
      w_param != HTCAPTION &&
      delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    // TODO(msw): Eliminate undesired painting, or re-evaluate this workaround.
    // DefWindowProc for WM_NCLBUTTONDOWN does weird non-client painting, so we
    // need to call it inside a ScopedRedrawLock. This may cause other negative
    // side-effects (ex/ stifling non-client mouse releases).
    DefWindowProcWithRedrawLock(message, w_param, l_param);
    handled = true;
  }

  // We need special processing for mouse input on the caption.
  // Please refer to the HandleMouseInputForCaption() function for more
  // information.
  if (!handled)
    handled = HandleMouseInputForCaption(message, w_param, l_param);

  if (ref.get())
    SetMsgHandled(handled);
  return 0;
}

bool HWNDMessageHandler::IsSynthesizedMouseMessage(unsigned int message,
                                                   int message_time,
                                                   LPARAM l_param) {
  if (ui::IsMouseEventFromTouch(message))
    return true;
  // Ignore mouse messages which occur at the same location as the current
  // cursor position and within a time difference of 500 ms from the last
  // touch message.
  if (last_touch_message_time_ && message_time >= last_touch_message_time_ &&
      ((message_time - last_touch_message_time_) <=
          kSynthesizedMouseTouchMessagesTimeDifference)) {
    POINT mouse_location = CR_POINT_INITIALIZER_FROM_LPARAM(l_param);
    ::ClientToScreen(hwnd(), &mouse_location);
    POINT cursor_pos = {0};
    ::GetCursorPos(&cursor_pos);
    if (memcmp(&cursor_pos, &mouse_location, sizeof(POINT)))
      return false;
    return true;
  }
  return false;
}

void HWNDMessageHandler::PerformDwmTransition() {
  dwm_transition_desired_ = false;

  UpdateDwmNcRenderingPolicy();
  // Don't redraw the window here, because we need to hide and show the window
  // which will also trigger a redraw.
  ResetWindowRegion(true, false);
  // The non-client view needs to update too.
  delegate_->HandleFrameChanged();

  if (IsVisible() && delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN) {
    // For some reason, we need to hide the window after we change from a custom
    // frame to a native frame.  If we don't, the client area will be filled
    // with black.  This seems to be related to an interaction between DWM and
    // SetWindowRgn, but the details aren't clear. Additionally, we need to
    // specify SWP_NOZORDER here, otherwise if you have multiple chrome windows
    // open they will re-appear with a non-deterministic Z-order.
    UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER;
    SetWindowPos(hwnd(), NULL, 0, 0, 0, 0, flags | SWP_HIDEWINDOW);
    SetWindowPos(hwnd(), NULL, 0, 0, 0, 0, flags | SWP_SHOWWINDOW);
  }
  // WM_DWMCOMPOSITIONCHANGED is only sent to top level windows, however we want
  // to notify our children too, since we can have MDI child windows who need to
  // update their appearance.
  EnumChildWindows(hwnd(), &SendDwmCompositionChanged, NULL);
}

void HWNDMessageHandler::GenerateTouchEvent(ui::EventType event_type,
                                            const gfx::Point& point,
                                            unsigned int id,
                                            base::TimeTicks time_stamp,
                                            TouchEvents* touch_events) {
  ui::TouchEvent event(event_type, point, id, time_stamp);

  event.set_flags(ui::GetModifiersFromKeyState());

  event.latency()->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
      0,
      0,
      time_stamp,
      1);

  touch_events->push_back(event);
}

bool HWNDMessageHandler::HandleMouseInputForCaption(unsigned int message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  // If we are receive a WM_NCLBUTTONDOWN messsage for the caption, the
  // following things happen.
  // 1. This message is defproced which will post a WM_SYSCOMMAND message with
  //    SC_MOVE and a constant 0x0002 which is documented on msdn as
  //    SC_DRAGMOVE.
  // 2. The WM_SYSCOMMAND message is defproced in our handler which works
  //    correctly in all cases except the case when we click on the caption
  //    and hold. The defproc appears to try and detect whether a mouse move
  //    is going to happen presumably via the DragEnter or a similar
  //    implementation which in its modal loop appears to only peek for
  //    mouse input briefly.
  // 3. Our workhorse message pump relies on the tickler posted message to get
  //    control during modal loops which does not happen in the above case for
  //    a while leading to the problem where activity on the page pauses
  //    briefly or at times stops for a while.
  // To fix this we don't defproc the WM_NCLBUTTONDOWN message and instead wait
  // for the subsequent WM_NCMOUSEMOVE/WM_MOUSEMOVE message. Once we receive
  // these messages and the mouse actually moved, we defproc the
  // WM_NCLBUTTONDOWN message.
  bool handled = false;
  switch (message) {
    case WM_NCLBUTTONDOWN: {
      if (w_param == HTCAPTION) {
        left_button_down_on_caption_ = true;
        // Cache the location where the click occurred. We use this in the
        // WM_NCMOUSEMOVE message to determine if the mouse actually moved.F
        caption_left_button_click_pos_.set_x(CR_GET_X_LPARAM(l_param));
        caption_left_button_click_pos_.set_y(CR_GET_Y_LPARAM(l_param));
        handled = true;
      }
      break;
    }

    // WM_NCMOUSEMOVE is received for normal drags which originate on the
    // caption and stay there.
    // WM_MOUSEMOVE can be received for drags which originate on the caption
    // and move towards the client area.
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE: {
      if (!left_button_down_on_caption_)
        break;

      bool should_handle_pending_ncl_button_down = true;
      // Check if the mouse actually moved.
      if (message == WM_NCMOUSEMOVE) {
        if (caption_left_button_click_pos_.x() == CR_GET_X_LPARAM(l_param) &&
            caption_left_button_click_pos_.y() == CR_GET_Y_LPARAM(l_param)) {
          should_handle_pending_ncl_button_down = false;
        }
      }
      if (should_handle_pending_ncl_button_down) {
        l_param = MAKELPARAM(caption_left_button_click_pos_.x(),
                             caption_left_button_click_pos_.y());
        // TODO(msw): Eliminate undesired painting, or re-evaluate this
        // workaround.
        // DefWindowProc for WM_NCLBUTTONDOWN does weird non-client painting,
        // so we need to call it inside a ScopedRedrawLock. This may cause
        // other negative side-effects
        // (ex/ stifling non-client mouse releases).
        // We may be deleted in the context of DefWindowProc. Don't refer to
        // any member variables after the DefWindowProc call.
        left_button_down_on_caption_ = false;

        if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
          DefWindowProcWithRedrawLock(WM_NCLBUTTONDOWN, HTCAPTION, l_param);
        } else {
          DefWindowProc(hwnd(), WM_NCLBUTTONDOWN, HTCAPTION, l_param);
        }
      }
      break;
    }

    case WM_NCMOUSELEAVE:
      break;

    default:
      left_button_down_on_caption_ = false;
      break;
  }
  return handled;
}

void HWNDMessageHandler::SetBoundsInternal(const gfx::Rect& bounds_in_pixels,
                                           bool force_size_changed) {
  LONG style = GetWindowLong(hwnd(), GWL_STYLE);
  if (style & WS_MAXIMIZE)
    SetWindowLong(hwnd(), GWL_STYLE, style & ~WS_MAXIMIZE);

  gfx::Size old_size = GetClientAreaBounds().size();
  SetWindowPos(hwnd(), NULL, bounds_in_pixels.x(), bounds_in_pixels.y(),
               bounds_in_pixels.width(), bounds_in_pixels.height(),
               SWP_NOACTIVATE | SWP_NOZORDER);

  // If HWND size is not changed, we will not receive standard size change
  // notifications. If |force_size_changed| is |true|, we should pretend size is
  // changed.
  if (old_size == bounds_in_pixels.size() && force_size_changed &&
      !background_fullscreen_hack_) {
    delegate_->HandleClientSizeChanged(GetClientAreaBounds().size());
    ResetWindowRegion(false, true);
  }

  if (direct_manipulation_helper_)
    direct_manipulation_helper_->SetBounds(bounds_in_pixels);
}

void HWNDMessageHandler::CheckAndHandleBackgroundFullscreenOnMonitor(
    HWND window) {
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

  FullscreenWindowMonitorMap::iterator iter =
      fullscreen_monitor_map_.Get().find(monitor);
  if (iter != fullscreen_monitor_map_.Get().end()) {
    DCHECK(iter->second);
    if (window != iter->second->hwnd())
      iter->second->OnBackgroundFullscreen();
  }
}

void HWNDMessageHandler::OnBackgroundFullscreen() {
  // Reduce the bounds of the window by 1px to ensure that Windows does
  // not treat this like a fullscreen window.
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY),
                 &monitor_info);
  gfx::Rect shrunk_rect(monitor_info.rcMonitor);
  shrunk_rect.set_height(shrunk_rect.height() - 1);
  background_fullscreen_hack_ = true;
  SetBoundsInternal(shrunk_rect, false);
}

}  // namespace views
