// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/stdafx.h"
#include "win8/metro_driver/chrome_app_view.h"

#include <corewindow.h>
#include <windows.applicationModel.datatransfer.h>
#include <windows.foundation.h>

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/win/metro.h"
// This include allows to send WM_SYSCOMMANDs to chrome.
#include "chrome/app/chrome_command_ids.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "win8/metro_driver/metro_driver.h"
#include "win8/metro_driver/winrt_utils.h"

typedef winfoundtn::ITypedEventHandler<
    winapp::Core::CoreApplicationView*,
    winapp::Activation::IActivatedEventArgs*> ActivatedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::WindowSizeChangedEventArgs*> SizeChangedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Input::EdgeGesture*,
    winui::Input::EdgeGestureEventArgs*> EdgeEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winapp::DataTransfer::DataTransferManager*,
    winapp::DataTransfer::DataRequestedEventArgs*> ShareDataRequestedHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::ViewManagement::InputPane*,
    winui::ViewManagement::InputPaneVisibilityEventArgs*>
    InputPaneEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::PointerEventArgs*> PointerEventHandler;

typedef winfoundtn::ITypedEventHandler<
    winui::Core::CoreWindow*,
    winui::Core::KeyEventArgs*> KeyEventHandler;

struct Globals globals;

// TODO(ananta)
// Remove this once we consolidate metro driver with chrome.
const wchar_t kMetroGetCurrentTabInfoMessage[] =
    L"CHROME_METRO_GET_CURRENT_TAB_INFO";

static const int kFlipWindowsHotKeyId = 0x0000baba;

static const int kAnimateWindowTimeoutMs = 200;

static const int kCheckOSKDelayMs = 300;

const wchar_t kOSKClassName[] = L"IPTip_Main_Window";

static const int kOSKAdjustmentOffset = 20;

const wchar_t kChromeSubclassWindowProp[] = L"MetroChromeWindowProc";

namespace {

enum Modifier {
  NONE,
  SHIFT = 1,
  CONTROL = 2,
  ALT = 4
};

// Helper function to send keystrokes via the SendInput function.
// Params:
// mnemonic_char: The keystroke to be sent.
// modifiers: Combination with Alt, Ctrl, Shift, etc.
// extended: whether this is an extended key.
// unicode: whether this is a unicode key.
void SendMnemonic(WORD mnemonic_char, Modifier modifiers, bool extended,
                  bool unicode) {
  INPUT keys[4] = {0};  // Keyboard events
  int key_count = 0;  // Number of generated events

  if (modifiers & SHIFT) {
    keys[key_count].type = INPUT_KEYBOARD;
    keys[key_count].ki.wVk = VK_SHIFT;
    keys[key_count].ki.wScan = MapVirtualKey(VK_SHIFT, 0);
    key_count++;
  }

  if (modifiers & CONTROL) {
    keys[key_count].type = INPUT_KEYBOARD;
    keys[key_count].ki.wVk = VK_CONTROL;
    keys[key_count].ki.wScan = MapVirtualKey(VK_CONTROL, 0);
    key_count++;
  }

  if (modifiers & ALT) {
    keys[key_count].type = INPUT_KEYBOARD;
    keys[key_count].ki.wVk = VK_MENU;
    keys[key_count].ki.wScan = MapVirtualKey(VK_MENU, 0);
    key_count++;
  }

  keys[key_count].type = INPUT_KEYBOARD;
  keys[key_count].ki.wVk = mnemonic_char;
  keys[key_count].ki.wScan = MapVirtualKey(mnemonic_char, 0);

  if (extended)
    keys[key_count].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  if (unicode)
    keys[key_count].ki.dwFlags |= KEYEVENTF_UNICODE;
  key_count++;

  bool should_sleep = key_count > 1;

  // Send key downs
  for (int i = 0; i < key_count; i++) {
    SendInput(1, &keys[ i ], sizeof(keys[0]));
    keys[i].ki.dwFlags |= KEYEVENTF_KEYUP;
    if (should_sleep) {
      Sleep(10);
    }
  }

  // Now send key ups in reverse order
  for (int i = key_count; i; i--) {
    SendInput(1, &keys[ i - 1 ], sizeof(keys[0]));
    if (should_sleep) {
      Sleep(10);
    }
  }
}

// Helper function to Exit metro chrome cleanly. If we are in the foreground
// then we try and exit by sending an Alt+F4 key combination to the core
// window which ensures that the chrome application tile does not show up in
// the running metro apps list on the top left corner. We have seen cases
// where this does work. To workaround that we invoke the
// ICoreApplicationExit::Exit function in a background delayed task which
// ensures that chrome exits.
void MetroExit(bool send_alt_f4_mnemonic) {
  if (send_alt_f4_mnemonic && globals.view &&
      globals.view->core_window_hwnd() == ::GetForegroundWindow()) {
    DVLOG(1) << "We are in the foreground. Exiting via Alt F4";
    SendMnemonic(VK_F4, ALT, false, false);
    DWORD core_window_process_id = 0;
    DWORD core_window_thread_id = GetWindowThreadProcessId(
        globals.view->core_window_hwnd(), &core_window_process_id);
    if (core_window_thread_id != ::GetCurrentThreadId()) {
      globals.appview_msg_loop->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MetroExit, false),
        base::TimeDelta::FromMilliseconds(100));
    }
  } else {
    globals.app_exit->Exit();
  }
}

void AdjustToFitWindow(HWND hwnd, int flags) {
  RECT rect = {0};
  ::GetWindowRect(globals.view->core_window_hwnd() , &rect);
  int cx = rect.right - rect.left;
  int cy = rect.bottom - rect.top;

  ::SetWindowPos(hwnd, HWND_TOP,
                 rect.left, rect.top, cx, cy,
                 SWP_NOZORDER | flags);
}

LRESULT CALLBACK ChromeWindowProc(HWND window,
                                  UINT message,
                                  WPARAM wp,
                                  LPARAM lp) {
  if (message == WM_SETCURSOR) {
    // Override the WM_SETCURSOR message to avoid showing the resize cursor
    // as the mouse moves to the edge of the screen.
    switch (LOWORD(lp)) {
      case HTBOTTOM:
      case HTBOTTOMLEFT:
      case HTBOTTOMRIGHT:
      case HTLEFT:
      case HTRIGHT:
      case HTTOP:
      case HTTOPLEFT:
      case HTTOPRIGHT:
        lp = MAKELPARAM(HTCLIENT, HIWORD(lp));
        break;
      default:
        break;
    }
  }

  WNDPROC old_proc = reinterpret_cast<WNDPROC>(
      ::GetProp(window, kChromeSubclassWindowProp));
  DCHECK(old_proc);
  return CallWindowProc(old_proc, window, message, wp, lp);
}

void AdjustFrameWindowStyleForMetro(HWND hwnd) {
  DVLOG(1) << __FUNCTION__;
  // Ajust the frame so the live preview works and the frame buttons dissapear.
  ::SetWindowLong(hwnd, GWL_STYLE,
                  WS_POPUP | (::GetWindowLong(hwnd, GWL_STYLE) &
                  ~(WS_MAXIMIZE | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU)));
  ::SetWindowLong(hwnd, GWL_EXSTYLE,
                  ::GetWindowLong(hwnd, GWL_EXSTYLE) & ~(WS_EX_DLGMODALFRAME |
                  WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

  // Subclass the wndproc of the frame window, if it's not already there.
  if (::GetProp(hwnd, kChromeSubclassWindowProp) == NULL) {
    WNDPROC old_chrome_proc =
        reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
            hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ChromeWindowProc)));
    ::SetProp(hwnd, kChromeSubclassWindowProp, old_chrome_proc);
  }
  AdjustToFitWindow(hwnd, SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

void SetFrameWindowInternal(HWND hwnd) {
  DVLOG(1) << __FUNCTION__ << ", hwnd=" << LONG_PTR(hwnd);

  HWND current_top_frame =
      !globals.host_windows.empty() ? globals.host_windows.front().first : NULL;
  if (hwnd != current_top_frame && IsWindow(current_top_frame)) {
    DVLOG(1) << "Hiding current top window, hwnd="
               << LONG_PTR(current_top_frame);
    ::ShowWindow(current_top_frame, SW_HIDE);
  }

  // Visible frame windows always need to be at the head of the list.
  // Check if the window being shown already exists in our global list.
  // If no then add it at the head of the list.
  // If yes, retrieve the osk window scrolled state, remove the window from the
  // list and readd it at the head.
  std::list<std::pair<HWND, bool> >::iterator index =
      std::find_if(globals.host_windows.begin(), globals.host_windows.end(),
              [hwnd](std::pair<HWND, bool>& item) {
    return (item.first == hwnd);
  });

  bool window_scrolled_state = false;
  bool new_window = (index == globals.host_windows.end());
  if (!new_window) {
    window_scrolled_state = index->second;
    globals.host_windows.erase(index);
  }

  globals.host_windows.push_front(std::make_pair(hwnd, window_scrolled_state));

  if (new_window) {
    AdjustFrameWindowStyleForMetro(hwnd);
  } else {
    DVLOG(1) << "Adjusting new top window to core window size";
    AdjustToFitWindow(hwnd, 0);
  }
  if (globals.view->GetViewState() ==
      winui::ViewManagement::ApplicationViewState_Snapped) {
    DVLOG(1) << "Enabling Metro snap state on new window: " << hwnd;
    ::PostMessageW(hwnd, WM_SYSCOMMAND, IDC_METRO_SNAP_ENABLE, 0);
  }
}

void CloseFrameWindowInternal(HWND hwnd) {
  DVLOG(1) << __FUNCTION__ << ", hwnd=" << LONG_PTR(hwnd);

  globals.host_windows.remove_if([hwnd](std::pair<HWND, bool>& item) {
    return (item.first == hwnd);
  });

  if (globals.host_windows.size() > 0) {
    DVLOG(1) << "Making new top frame window visible:"
            << reinterpret_cast<int>(globals.host_windows.front().first);
    AdjustToFitWindow(globals.host_windows.front().first, SWP_SHOWWINDOW);
  } else {
    // time to quit
    DVLOG(1) << "Last host window closed. Calling Exit().";
    MetroExit(true);
  }
}

void FlipFrameWindowsInternal() {
  DVLOG(1) << __FUNCTION__;
  // Get the first window in the frame windows queue and push it to the end.
  // Metroize the next window in the queue.
  if (globals.host_windows.size() > 1) {
    std::pair<HWND, bool> current_top_window = globals.host_windows.front();
    globals.host_windows.pop_front();

    DVLOG(1) << "Making new top frame window visible:"
            <<  reinterpret_cast<int>(globals.host_windows.front().first);

    AdjustToFitWindow(globals.host_windows.front().first, SWP_SHOWWINDOW);

    DVLOG(1) << "Hiding current top window:"
            << reinterpret_cast<int>(current_top_window.first);
    AnimateWindow(current_top_window.first, kAnimateWindowTimeoutMs,
                  AW_HIDE | AW_HOR_POSITIVE | AW_SLIDE);

    globals.host_windows.push_back(current_top_window);
  }
}

}  // namespace

void ChromeAppView::DisplayNotification(
    const ToastNotificationHandler::DesktopNotification& notification) {
  DVLOG(1) << __FUNCTION__;

  if (IsValidNotification(notification.id)) {
    NOTREACHED() << "Duplicate notification id passed in.";
    return;
  }

  base::AutoLock lock(notification_lock_);

  ToastNotificationHandler* notification_handler =
      new ToastNotificationHandler;

  notification_map_[notification.id].reset(notification_handler);
  notification_handler->DisplayNotification(notification);
}

void ChromeAppView::CancelNotification(const std::string& notification) {
  DVLOG(1) << __FUNCTION__;

  base::AutoLock lock(notification_lock_);

  NotificationMap::iterator index = notification_map_.find(notification);
  if (index == notification_map_.end()) {
    NOTREACHED() << "Invalid notification:" << notification.c_str();
    return;
  }

  scoped_ptr<ToastNotificationHandler> notification_handler(
      index->second.release());

  notification_map_.erase(index);

  notification_handler->CancelNotification();
}

// Returns true if the notification passed in is valid.
bool ChromeAppView::IsValidNotification(const std::string& notification) {
  DVLOG(1) << __FUNCTION__;

  base::AutoLock lock(notification_lock_);
  return notification_map_.find(notification) != notification_map_.end();
}

void ChromeAppView::ShowDialogBox(
    const MetroDialogBox::DialogBoxInfo& dialog_box_info) {
  VLOG(1) << __FUNCTION__;
  dialog_box_.Show(dialog_box_info);
}

void ChromeAppView::DismissDialogBox() {
  VLOG(1) << __FUNCTION__;
  dialog_box_.Dismiss();
}

// static
HRESULT ChromeAppView::Unsnap() {
  mswr::ComPtr<winui::ViewManagement::IApplicationViewStatics> view_statics;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_ViewManagement_ApplicationView,
      view_statics.GetAddressOf());
  CheckHR(hr);

  winui::ViewManagement::ApplicationViewState state =
      winui::ViewManagement::ApplicationViewState_FullScreenLandscape;
  hr = view_statics->get_Value(&state);
  CheckHR(hr);

  if (state == winui::ViewManagement::ApplicationViewState_Snapped) {
    boolean success = FALSE;
    hr = view_statics->TryUnsnap(&success);

    if (FAILED(hr) || !success) {
      LOG(ERROR) << "Failed to unsnap. Error 0x" << hr;
      if (SUCCEEDED(hr))
        hr = E_UNEXPECTED;
    }
  }
  return hr;
}

void ChromeAppView::SetFullscreen(bool fullscreen) {
  VLOG(1) << __FUNCTION__;

  if (osk_offset_adjustment_) {
    VLOG(1) << "Scrolling the window down by: "
            << osk_offset_adjustment_;

    ::ScrollWindowEx(globals.host_windows.front().first,
                     0,
                     osk_offset_adjustment_,
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     SW_INVALIDATE | SW_SCROLLCHILDREN);
    osk_offset_adjustment_ = 0;
  }
}

winui::ViewManagement::ApplicationViewState ChromeAppView::GetViewState() {
  winui::ViewManagement::ApplicationViewState view_state =
      winui::ViewManagement::ApplicationViewState_FullScreenLandscape;
  app_view_->get_Value(&view_state);
  return view_state;
}

void UnsnapHelper() {
  ChromeAppView::Unsnap();
}

extern "C" __declspec(dllexport)
void MetroUnsnap() {
  DVLOG(1) << __FUNCTION__;
  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&UnsnapHelper));
}

extern "C" __declspec(dllexport)
HWND GetRootWindow() {
  DVLOG(1) << __FUNCTION__;
  return globals.view->core_window_hwnd();
}

extern "C" __declspec(dllexport)
void SetFrameWindow(HWND hwnd) {
  DVLOG(1) << __FUNCTION__ << ", hwnd=" << LONG_PTR(hwnd);
  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&SetFrameWindowInternal, hwnd));
}

// TODO(ananta)
// Handle frame window close by deleting it from the window list and making the
// next guy visible.
extern "C" __declspec(dllexport)
  void CloseFrameWindow(HWND hwnd) {
  DVLOG(1) << __FUNCTION__ << ", hwnd=" << LONG_PTR(hwnd);

  // This is a hack to ensure that the BrowserViewLayout code layout happens
  // just at the right time to hide the switcher button if it is visible.
  globals.appview_msg_loop->PostDelayedTask(
    FROM_HERE, base::Bind(&CloseFrameWindowInternal, hwnd),
        base::TimeDelta::FromMilliseconds(50));
}

// Returns the initial url. This returns a valid url only if we were launched
// into metro via a url navigation.
extern "C" __declspec(dllexport)
const wchar_t* GetInitialUrl() {
  DVLOG(1) << __FUNCTION__;
  bool was_initial_activation = globals.is_initial_activation;
  globals.is_initial_activation = false;
  if (!was_initial_activation || globals.navigation_url.empty())
    return L"";

  const wchar_t* initial_url = globals.navigation_url.c_str();
  DVLOG(1) << initial_url;
  return initial_url;
}

// Returns the initial search string. This returns a valid url only if we were
// launched into metro via the search charm
extern "C" __declspec(dllexport)
const wchar_t* GetInitialSearchString() {
  DVLOG(1) << __FUNCTION__;
  bool was_initial_activation = globals.is_initial_activation;
  globals.is_initial_activation = false;
  if (!was_initial_activation || globals.search_string.empty())
    return L"";

  const wchar_t* initial_search_string = globals.search_string.c_str();
  DVLOG(1) << initial_search_string;
  return initial_search_string;
}

// Returns the launch type.
extern "C" __declspec(dllexport)
base::win::MetroLaunchType GetLaunchType(
    base::win::MetroPreviousExecutionState* previous_state) {
  if (previous_state) {
    *previous_state = static_cast<base::win::MetroPreviousExecutionState>(
        globals.previous_state);
  }
  return static_cast<base::win::MetroLaunchType>(
      globals.initial_activation_kind);
}

extern "C" __declspec(dllexport)
void FlipFrameWindows() {
  DVLOG(1) << __FUNCTION__;
  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&FlipFrameWindowsInternal));
}

extern "C" __declspec(dllexport)
void DisplayNotification(const char* origin_url, const char* icon_url,
                         const wchar_t* title, const wchar_t* body,
                         const wchar_t* display_source,
                         const char* notification_id,
                         base::win::MetroNotificationClickedHandler handler,
                         const wchar_t* handler_context) {
  // TODO(ananta)
  // Needs implementation.
  DVLOG(1) << __FUNCTION__;

  ToastNotificationHandler::DesktopNotification notification(origin_url,
                                                             icon_url,
                                                             title,
                                                             body,
                                                             display_source,
                                                             notification_id,
                                                             handler,
                                                             handler_context);
  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&ChromeAppView::DisplayNotification,
                            globals.view, notification));
}

extern "C" __declspec(dllexport)
bool CancelNotification(const char* notification_id) {
  // TODO(ananta)
  // Needs implementation.
  DVLOG(1) << __FUNCTION__;

  if (!globals.view->IsValidNotification(notification_id)) {
    NOTREACHED() << "Invalid notification id :" << notification_id;
    return false;
  }

  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&ChromeAppView::CancelNotification,
                            globals.view, std::string(notification_id)));
  return true;
}

// Returns command line switches if any to be used by metro chrome.
extern "C" __declspec(dllexport)
const wchar_t* GetMetroCommandLineSwitches() {
  DVLOG(1) << __FUNCTION__;
  // The metro_command_line_switches field should be filled up once.
  // ideally in ChromeAppView::Activate.
  return globals.metro_command_line_switches.c_str();
}

// Provides functionality to display a metro style dialog box with two buttons.
// Only one dialog box can be displayed at any given time.
extern "C" __declspec(dllexport)
void ShowDialogBox(
    const wchar_t* title,
    const wchar_t* content,
    const wchar_t* button1_label,
    const wchar_t* button2_label,
    base::win::MetroDialogButtonPressedHandler button1_handler,
    base::win::MetroDialogButtonPressedHandler button2_handler) {
  VLOG(1) << __FUNCTION__;

  DCHECK(title);
  DCHECK(content);
  DCHECK(button1_label);
  DCHECK(button2_label);
  DCHECK(button1_handler);
  DCHECK(button2_handler);

  MetroDialogBox::DialogBoxInfo dialog_box_info;
  dialog_box_info.title = title;
  dialog_box_info.content = content;
  dialog_box_info.button1_label = button1_label;
  dialog_box_info.button2_label = button2_label;
  dialog_box_info.button1_handler = button1_handler;
  dialog_box_info.button2_handler = button2_handler;

  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(
          &ChromeAppView::ShowDialogBox, globals.view, dialog_box_info));
}

// Provides functionality to dismiss the previously displayed metro style
// dialog box.
extern "C" __declspec(dllexport)
void DismissDialogBox() {
  VLOG(1) << __FUNCTION__;

  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(
          &ChromeAppView::DismissDialogBox,
          globals.view));
}

extern "C" __declspec(dllexport)
void SetFullscreen(bool fullscreen) {
  VLOG(1) << __FUNCTION__;

  globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(
          &ChromeAppView::SetFullscreen,
          globals.view, fullscreen));
}

template <typename ContainerT>
void CloseSecondaryWindows(ContainerT& windows) {
  DVLOG(1) << "Closing secondary windows", windows.size();
  std::for_each(windows.begin(), windows.end(), [](HWND hwnd) {
    ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
  });
  windows.clear();
}

void EndChromeSession() {
  DVLOG(1) << "Sending chrome WM_ENDSESSION window message.";
  ::SendMessage(globals.host_windows.front().first, WM_ENDSESSION, FALSE,
                ENDSESSION_CLOSEAPP);
}

DWORD WINAPI HostMainThreadProc(void*) {
  // Test feature - devs have requested the ability to easily add metro-chrome
  // command line arguments. This is hard since shortcut arguments are ignored,
  // by Metro, so we instead read them directly from the pinned taskbar
  // shortcut. This may call Coinitialize and there is tell of badness
  // occurring if CoInitialize is called on a metro thread.
  globals.metro_command_line_switches =
      winrt_utils::ReadArgumentsFromPinnedTaskbarShortcut();

  globals.g_core_proc =
      reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
          globals.view->core_window_hwnd(), GWLP_WNDPROC,
          reinterpret_cast<LONG_PTR>(ChromeAppView::CoreWindowProc)));
  DWORD exit_code = globals.host_main(globals.host_context);

  DVLOG(1) << "host thread done, exit_code=" << exit_code;
  MetroExit(true);
  return exit_code;
}

ChromeAppView::ChromeAppView()
    : osk_visible_notification_received_(false),
      osk_offset_adjustment_(0),
      core_window_hwnd_(NULL) {
  globals.previous_state =
      winapp::Activation::ApplicationExecutionState_NotRunning;
}

ChromeAppView::~ChromeAppView() {
  DVLOG(1) << __FUNCTION__;
}

IFACEMETHODIMP
ChromeAppView::Initialize(winapp::Core::ICoreApplicationView* view) {
  view_ = view;
  DVLOG(1) << __FUNCTION__;

  HRESULT hr = view_->add_Activated(mswr::Callback<ActivatedHandler>(
      this, &ChromeAppView::OnActivate).Get(),
      &activated_token_);
  CheckHR(hr);
  return hr;
}

IFACEMETHODIMP
ChromeAppView::SetWindow(winui::Core::ICoreWindow* window) {
  window_ = window;
  DVLOG(1) << __FUNCTION__;

  // Retrieve the native window handle via the interop layer.
  mswr::ComPtr<ICoreWindowInterop> interop;
  HRESULT hr = window->QueryInterface(interop.GetAddressOf());
  CheckHR(hr);
  hr = interop->get_WindowHandle(&core_window_hwnd_);
  CheckHR(hr);

  hr = url_launch_handler_.Initialize();
  CheckHR(hr, "Failed to initialize url launch handler.");
  // Register for size notifications.
  hr = window_->add_SizeChanged(mswr::Callback<SizeChangedHandler>(
      this, &ChromeAppView::OnSizeChanged).Get(),
      &sizechange_token_);
  CheckHR(hr);

  // Register for edge gesture notifications.
  mswr::ComPtr<winui::Input::IEdgeGestureStatics> edge_gesture_statics;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_Input_EdgeGesture,
      edge_gesture_statics.GetAddressOf());
  CheckHR(hr, "Failed to activate IEdgeGestureStatics.");

  mswr::ComPtr<winui::Input::IEdgeGesture> edge_gesture;
  hr = edge_gesture_statics->GetForCurrentView(&edge_gesture);
  CheckHR(hr);

  hr = edge_gesture->add_Completed(mswr::Callback<EdgeEventHandler>(
      this, &ChromeAppView::OnEdgeGestureCompleted).Get(),
      &edgeevent_token_);
  CheckHR(hr);

  // Register for share notifications.
  mswr::ComPtr<winapp::DataTransfer::IDataTransferManagerStatics>
      data_mgr_statics;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager,
      data_mgr_statics.GetAddressOf());
  CheckHR(hr, "Failed to activate IDataTransferManagerStatics.");

  mswr::ComPtr<winapp::DataTransfer::IDataTransferManager> data_transfer_mgr;
  hr = data_mgr_statics->GetForCurrentView(&data_transfer_mgr);
  CheckHR(hr, "Failed to get IDataTransferManager for current view.");

  hr = data_transfer_mgr->add_DataRequested(
      mswr::Callback<ShareDataRequestedHandler>(
          this, &ChromeAppView::OnShareDataRequested).Get(),
      &share_data_requested_token_);
  CheckHR(hr);

  // TODO(ananta)
  // The documented InputPane notifications don't fire on Windows 8 in metro
  // chrome. Uncomment this once we figure out why they don't fire.
  // RegisterInputPaneNotifications();
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_ViewManagement_ApplicationView,
      app_view_.GetAddressOf());
  CheckHR(hr);

  DVLOG(1) << "Created appview instance.";

  hr = devices_handler_.Initialize(window);
  // Don't check or return the failure here, we need to let the app
  // initialization succeed. Even if we won't be able to access devices
  // we still want to allow the app to start.
  LOG_IF(ERROR, FAILED(hr)) << "Failed to initialize devices handler.";
  return S_OK;
}

IFACEMETHODIMP
ChromeAppView::Load(HSTRING entryPoint) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

void RunMessageLoop(winui::Core::ICoreDispatcher* dispatcher) {
  // We're entering a nested message loop, let's allow dispatching
  // tasks while we're in there.
  base::MessageLoop::current()->SetNestableTasksAllowed(true);

  // Enter main core message loop. There are several ways to exit it
  // Nicely:
  // 1 - User action like ALT-F4.
  // 2 - Calling ICoreApplicationExit::Exit().
  // 3-  Posting WM_CLOSE to the core window.
  HRESULT hr = dispatcher->ProcessEvents(
      winui::Core::CoreProcessEventsOption
          ::CoreProcessEventsOption_ProcessUntilQuit);

  // Wind down the thread's chrome message loop.
  base::MessageLoop::current()->Quit();
}

void ChromeAppView::CheckForOSKActivation() {
  // Hack for checking if the OSK was displayed while we are in the foreground.
  // The input pane notifications which are supposed to fire when the OSK is
  // shown and hidden don't seem to be firing in Windows 8 metro for us.
  // The current hack is supposed to workaround that issue till we figure it
  // out. Logic is to find the OSK window and see if we are the foreground
  // process. If yes then fire the notification once for when the OSK is shown
  // and once for when it is hidden.
  // TODO(ananta)
  // Take this out when the documented input pane notification issues are
  // addressed.
  HWND osk = ::FindWindow(kOSKClassName, NULL);
  if (::IsWindow(osk)) {
    HWND foreground_window = ::GetForegroundWindow();
    if (globals.host_windows.size() > 0 &&
        foreground_window == globals.host_windows.front().first) {
      RECT osk_rect = {0};
      ::GetWindowRect(osk, &osk_rect);

      if (::IsWindowVisible(osk) && ::IsWindowEnabled(osk)) {
        if (!globals.view->osk_visible_notification_received()) {
          DVLOG(1) << "Found KB window while we are in the forground.";
          HandleInputPaneVisible(osk_rect);
        }
      } else if (osk_visible_notification_received()) {
        DVLOG(1) << "KB window hidden while we are in the foreground.";
        HandleInputPaneHidden(osk_rect);
      }
    }
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChromeAppView::CheckForOSKActivation, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kCheckOSKDelayMs));
}

IFACEMETHODIMP
ChromeAppView::Run() {
  DVLOG(1) << __FUNCTION__;
  mswr::ComPtr<winui::Core::ICoreDispatcher> dispatcher;
  HRESULT hr = window_->get_Dispatcher(&dispatcher);
  CheckHR(hr, "Dispatcher failed.");

  hr = window_->Activate();
  if (SUCCEEDED(hr)) {
    // TODO(cpu): Draw something here.
  } else {
    DVLOG(1) << "Activate failed, hr=" << hr;
  }

  // Create a message loop to allow message passing into this thread.
  base::MessageLoopForUI msg_loop;

  // Announce our message loop to the world.
  globals.appview_msg_loop = msg_loop.message_loop_proxy();

  // And post the task that'll do the inner Metro message pumping to it.
  msg_loop.PostTask(FROM_HERE, base::Bind(&RunMessageLoop, dispatcher.Get()));

  // Post the recurring task which checks for OSK activation in metro chrome.
  // Please refer to the comments in the CheckForOSKActivation function for why
  // this is needed.
  // TODO(ananta)
  // Take this out when the documented OSK notifications start working.
  msg_loop.PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChromeAppView::CheckForOSKActivation,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kCheckOSKDelayMs));

  msg_loop.Run();

  globals.appview_msg_loop = NULL;

  DVLOG(0) << "ProcessEvents done, hr=" << hr;

  // We join here with chrome's main thread so that the chrome is not killed
  // while a critical operation is still in progress. Now, if there are host
  // windows active it is possible we end up stuck on the wait below therefore
  // we tell chrome to close its windows.
  if (!globals.host_windows.empty()) {
    DVLOG(1) << "Chrome still has windows open!";
    EndChromeSession();
  }
  DWORD wr = ::WaitForSingleObject(globals.host_thread, INFINITE);
  if (wr != WAIT_OBJECT_0) {
    DVLOG(1) << "Waiting for host thread failed : " << wr;
  }

  DVLOG(1) << "Host thread exited";

  ::CloseHandle(globals.host_thread);
  globals.host_thread = NULL;
  return hr;
}

IFACEMETHODIMP
ChromeAppView::Uninitialize() {
  DVLOG(1) << __FUNCTION__;
  window_ = nullptr;
  view_ = nullptr;
  base::AutoLock lock(notification_lock_);
  notification_map_.clear();
  return S_OK;
}

HRESULT ChromeAppView::RegisterInputPaneNotifications() {
  DVLOG(1) << __FUNCTION__;

  mswr::ComPtr<winui::ViewManagement::IInputPaneStatics>
      input_pane_statics;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_UI_ViewManagement_InputPane,
      input_pane_statics.GetAddressOf());
  CheckHR(hr);

  hr = input_pane_statics->GetForCurrentView(&input_pane_);
  CheckHR(hr);
  DVLOG(1) << "Got input pane.";

  hr = input_pane_->add_Showing(
      mswr::Callback<InputPaneEventHandler>(
          this, &ChromeAppView::OnInputPaneVisible).Get(),
      &input_pane_visible_token_);
  CheckHR(hr);

  DVLOG(1) << "Added showing event handler for input pane",
      input_pane_visible_token_.value;

  hr = input_pane_->add_Hiding(
      mswr::Callback<InputPaneEventHandler>(
          this, &ChromeAppView::OnInputPaneHiding).Get(),
      &input_pane_hiding_token_);
  CheckHR(hr);

  DVLOG(1) << "Added hiding event handler for input pane, value="
          << input_pane_hiding_token_.value;
  return hr;
}

HRESULT ChromeAppView::OnActivate(winapp::Core::ICoreApplicationView*,
    winapp::Activation::IActivatedEventArgs* args) {
  DVLOG(1) << __FUNCTION__;

  args->get_PreviousExecutionState(&globals.previous_state);
  DVLOG(1) << "Previous Execution State: " << globals.previous_state;

  window_->Activate();
  url_launch_handler_.Activate(args);

  if (globals.previous_state ==
      winapp::Activation::ApplicationExecutionState_Running &&
      globals.host_thread) {
    DVLOG(1) << "Already running. Skipping rest of OnActivate.";
    return S_OK;
  }

  if (!globals.host_thread) {
    DWORD chrome_ui_thread_id = 0;
    globals.host_thread =
        ::CreateThread(NULL, 0, HostMainThreadProc, NULL, 0,
                      &chrome_ui_thread_id);

    if (!globals.host_thread) {
      NOTREACHED() << "thread creation failed.";
      return E_UNEXPECTED;
    }
  }

  if (RegisterHotKey(core_window_hwnd_, kFlipWindowsHotKeyId,
                     MOD_CONTROL, VK_F12)) {
    DVLOG(1) << "Registered flip window hotkey.";
  } else {
    VPLOG(1) << "Failed to register flip window hotkey.";
  }
  HRESULT hr = settings_handler_.Initialize();
  CheckHR(hr,"Failed to initialize settings handler.");
  return hr;
}

// We subclass the core window for moving the associated chrome window when the
// core window is moved around, typically in the snap view operation. The
// size changes are handled in the documented size changed event.
LRESULT CALLBACK ChromeAppView::CoreWindowProc(
    HWND window, UINT message, WPARAM wp, LPARAM lp) {

  static const UINT kBrowserClosingMessage =
      ::RegisterWindowMessage(L"DefaultBrowserClosing");

  if (message == WM_WINDOWPOSCHANGED) {
    WINDOWPOS* pos = reinterpret_cast<WINDOWPOS*>(lp);
    if (!(pos->flags & SWP_NOMOVE)) {
      DVLOG(1) << "WM_WINDOWPOSCHANGED. Moving the chrome window.";
      globals.view->OnPositionChanged(pos->x, pos->y);
    }
  } else if (message == WM_HOTKEY && wp == kFlipWindowsHotKeyId) {
    FlipFrameWindows();
  } else if (message == kBrowserClosingMessage) {
    DVLOG(1) << "Received DefaultBrowserClosing window message.";
    // Ensure that the view is uninitialized. The kBrowserClosingMessage
    // means that the app is going to be terminated, i.e. the proper
    // uninitialization sequence does not occur.
    globals.view->Uninitialize();
    if (!globals.host_windows.empty()) {
      EndChromeSession();
    }
  }
  return CallWindowProc(globals.g_core_proc, window, message, wp, lp);
}

HRESULT ChromeAppView::OnSizeChanged(winui::Core::ICoreWindow* sender,
    winui::Core::IWindowSizeChangedEventArgs* args) {
  if (!globals.host_windows.size()) {
    return S_OK;
  }

  winfoundtn::Size size;
  args->get_Size(&size);

  int cx = static_cast<int>(size.Width);
  int cy = static_cast<int>(size.Height);

  if (!::SetWindowPos(globals.host_windows.front().first, NULL, 0, 0, cx, cy,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED)) {
    DVLOG(1) << "SetWindowPos failed.";
  }
  DVLOG(1) << "size changed cx=" << cx;
  DVLOG(1) << "size changed cy=" << cy;

  winui::ViewManagement::ApplicationViewState view_state =
      winui::ViewManagement::ApplicationViewState_FullScreenLandscape;
  app_view_->get_Value(&view_state);

  HWND top_level_frame = globals.host_windows.front().first;
  if (view_state == winui::ViewManagement::ApplicationViewState_Snapped) {
    DVLOG(1) << "Enabling metro snap mode.";
    ::PostMessageW(top_level_frame, WM_SYSCOMMAND, IDC_METRO_SNAP_ENABLE, 0);
  } else {
    ::PostMessageW(top_level_frame, WM_SYSCOMMAND, IDC_METRO_SNAP_DISABLE, 0);
  }
  return S_OK;
}

HRESULT ChromeAppView::OnPositionChanged(int x, int y) {
  DVLOG(1) << __FUNCTION__;

  ::SetWindowPos(globals.host_windows.front().first, NULL, x, y, 0, 0,
                 SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOSIZE);
  return S_OK;
}

HRESULT ChromeAppView::OnEdgeGestureCompleted(
    winui::Input::IEdgeGesture* gesture,
    winui::Input::IEdgeGestureEventArgs* args) {
  DVLOG(1) << "edge gesture completed.";

  winui::ViewManagement::ApplicationViewState view_state =
      winui::ViewManagement::ApplicationViewState_FullScreenLandscape;
  app_view_->get_Value(&view_state);
  // We don't want fullscreen chrome unless we are fullscreen metro.
  if ((view_state == winui::ViewManagement::ApplicationViewState_Filled) ||
      (view_state == winui::ViewManagement::ApplicationViewState_Snapped)) {
    DVLOG(1) << "No full screen in snapped view state:" << view_state;
    return S_OK;
  }

  // Deactivate anything pending, e.g., the wrench or a context menu.
  BOOL success = ::ReleaseCapture();
  DCHECK(success) << "Couldn't ReleaseCapture() before going full screen";

  DVLOG(1) << "Going full screen.";
  ::PostMessageW(globals.host_windows.front().first, WM_SYSCOMMAND,
                 IDC_FULLSCREEN, 0);
  return S_OK;
}

HRESULT ChromeAppView::OnShareDataRequested(
  winapp::DataTransfer::IDataTransferManager* data_transfer_mgr,
  winapp::DataTransfer::IDataRequestedEventArgs* event_args) {

  DVLOG(1) << "Share data requested.";

  // The current tab info is retrieved from Chrome via a registered window
  // message.

  static const UINT get_current_tab_info =
      RegisterWindowMessage(kMetroGetCurrentTabInfoMessage);

  static const int kGetTabInfoTimeoutMs = 1000;

  mswr::ComPtr<winapp::DataTransfer::IDataRequest> data_request;
  HRESULT hr = event_args->get_Request(&data_request);
  CheckHR(hr);

  mswr::ComPtr<winapp::DataTransfer::IDataPackage> data_package;
  hr = data_request->get_Data(&data_package);
  CheckHR(hr);

  base::win::CurrentTabInfo current_tab_info;
  current_tab_info.title = NULL;
  current_tab_info.url = NULL;

  DWORD_PTR result = 0;

  if (!SendMessageTimeout(globals.host_windows.front().first,
                          get_current_tab_info,
                          reinterpret_cast<WPARAM>(&current_tab_info),
                          0,
                          SMTO_ABORTIFHUNG,
                          kGetTabInfoTimeoutMs,
                          &result)) {
    VPLOG(1) << "Failed to retrieve tab info from chrome.";
    return E_FAIL;
  }

  if (!current_tab_info.title || !current_tab_info.url) {
    DVLOG(1) << "Failed to retrieve tab info from chrome.";
    return E_FAIL;
  }

  base::string16 current_title(current_tab_info.title);
  base::string16 current_url(current_tab_info.url);

  LocalFree(current_tab_info.title);
  LocalFree(current_tab_info.url);

  mswr::ComPtr<winapp::DataTransfer::IDataPackagePropertySet> data_properties;
  hr = data_package->get_Properties(&data_properties);

  mswrw::HString title;
  title.Attach(MakeHString(current_title));
  data_properties->put_Title(title.Get());

  mswr::ComPtr<winfoundtn::IUriRuntimeClassFactory> uri_factory;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Foundation_Uri,
      uri_factory.GetAddressOf());
  CheckHR(hr);

  mswrw::HString url;
  url.Attach(MakeHString(current_url));
  mswr::ComPtr<winfoundtn::IUriRuntimeClass> uri;
  hr = uri_factory->CreateUri(url.Get(), &uri);
  CheckHR(hr);

  hr = data_package->SetUri(uri.Get());
  CheckHR(hr);

  return S_OK;
}

void ChromeAppView::HandleInputPaneVisible(const RECT& osk_rect) {
  DCHECK(!osk_visible_notification_received_);

  DVLOG(1) << __FUNCTION__;
  DVLOG(1) << "OSK width:" << osk_rect.right - osk_rect.left;
  DVLOG(1) << "OSK height:" << osk_rect.bottom - osk_rect.top;

  globals.host_windows.front().second = false;

  POINT cursor_pos = {0};
  GetCursorPos(&cursor_pos);

  osk_offset_adjustment_ = 0;

  if (::PtInRect(&osk_rect, cursor_pos)) {
    DVLOG(1) << "OSK covering focus point.";
    int osk_height = osk_rect.bottom - osk_rect.top;

    osk_offset_adjustment_ = osk_height + kOSKAdjustmentOffset;

    DVLOG(1) << "Scrolling window by offset: " << osk_offset_adjustment_;
    ::ScrollWindowEx(globals.host_windows.front().first,
                     0,
                     -osk_offset_adjustment_,
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     SW_INVALIDATE | SW_SCROLLCHILDREN);

    globals.host_windows.front().second  = true;
  }
  osk_visible_notification_received_ = true;
}

void ChromeAppView::HandleInputPaneHidden(const RECT& osk_rect) {
  DCHECK(osk_visible_notification_received_);
  DVLOG(1) << __FUNCTION__;
  DVLOG(1) << "OSK width:" << osk_rect.right - osk_rect.left;
  DVLOG(1) << "OSK height:" << osk_rect.bottom - osk_rect.top;
  osk_visible_notification_received_ = false;
  if (globals.host_windows.front().second == true) {

    if (osk_offset_adjustment_) {
      DVLOG(1) << "Restoring scrolled window offset: "
               << osk_offset_adjustment_;

      ::ScrollWindowEx(globals.host_windows.front().first,
                       0,
                       osk_offset_adjustment_,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       SW_INVALIDATE | SW_SCROLLCHILDREN);
    }

    globals.host_windows.front().second = false;
  }
}

HRESULT ChromeAppView::OnInputPaneVisible(
    winui::ViewManagement::IInputPane* input_pane,
    winui::ViewManagement::IInputPaneVisibilityEventArgs* event_args) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

HRESULT ChromeAppView::OnInputPaneHiding(
    winui::ViewManagement::IInputPane* input_pane,
    winui::ViewManagement::IInputPaneVisibilityEventArgs* event_args) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

ChromeAppViewFactory::ChromeAppViewFactory(
    winapp::Core::ICoreApplication* icore_app,
    LPTHREAD_START_ROUTINE host_main,
    void* host_context) {
  globals.host_main = host_main;
  globals.host_context = host_context;
  mswr::ComPtr<winapp::Core::ICoreApplication> core_app(icore_app);
  mswr::ComPtr<winapp::Core::ICoreApplicationExit> app_exit;
  CheckHR(core_app.As(&app_exit));
  globals.app_exit = app_exit.Detach();
}

IFACEMETHODIMP
ChromeAppViewFactory::CreateView(winapp::Core::IFrameworkView** view) {
  globals.view = mswr::Make<ChromeAppView>().Detach();
  *view = globals.view;
  return (*view) ? S_OK :  E_OUTOFMEMORY;
}
