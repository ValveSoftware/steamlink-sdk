// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/webplugin_delegate_impl.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/version.h"
#include "base/win/iat_patch_function.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/child/npapi/plugin_stream_url.h"
#include "content/child/npapi/webplugin.h"
#include "content/child/npapi/webplugin_ime_win.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/plugin_constants_win.h"
#include "content/public/common/content_constants.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/win/dpi.h"
#include "ui/gfx/win/hwnd_util.h"

using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;

namespace content {

namespace {

const wchar_t kWebPluginDelegateProperty[] = L"WebPluginDelegateProperty";
const wchar_t kPluginFlashThrottle[] = L"FlashThrottle";

// The fastest we are willing to process WM_USER+1 events for Flash.
// Flash can easily exceed the limits of our CPU if we don't throttle it.
// The throttle has been chosen by testing various delays and compromising
// on acceptable Flash performance and reasonable CPU consumption.
//
// I'd like to make the throttle delay variable, based on the amount of
// time currently required to paint Flash plugins.  There isn't a good
// way to count the time spent in aggregate plugin painting, however, so
// this seems to work well enough.
const int kFlashWMUSERMessageThrottleDelayMs = 5;

// Flash displays popups in response to user clicks by posting a WM_USER
// message to the plugin window. The handler for this message displays
// the popup. To ensure that the popups allowed state is sent correctly
// to the renderer we reset the popups allowed state in a timer.
const int kWindowedPluginPopupTimerMs = 50;

// The current instance of the plugin which entered the modal loop.
WebPluginDelegateImpl* g_current_plugin_instance = NULL;

typedef std::deque<MSG> ThrottleQueue;
base::LazyInstance<ThrottleQueue> g_throttle_queue = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<std::map<HWND, WNDPROC> > g_window_handle_proc_map =
    LAZY_INSTANCE_INITIALIZER;

// Helper object for patching the TrackPopupMenu API.
base::LazyInstance<base::win::IATPatchFunction> g_iat_patch_track_popup_menu =
    LAZY_INSTANCE_INITIALIZER;

// Helper object for patching the SetCursor API.
base::LazyInstance<base::win::IATPatchFunction> g_iat_patch_set_cursor =
    LAZY_INSTANCE_INITIALIZER;

// Helper object for patching the RegEnumKeyExW API.
base::LazyInstance<base::win::IATPatchFunction> g_iat_patch_reg_enum_key_ex_w =
    LAZY_INSTANCE_INITIALIZER;

// Helper object for patching the GetProcAddress API.
base::LazyInstance<base::win::IATPatchFunction> g_iat_patch_get_proc_address =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::win::IATPatchFunction> g_iat_patch_window_from_point =
    LAZY_INSTANCE_INITIALIZER;

// http://crbug.com/16114
// Enforces providing a valid device context in NPWindow, so that NPP_SetWindow
// is never called with NPNWindoTypeDrawable and NPWindow set to NULL.
// Doing so allows removing NPP_SetWindow call during painting a windowless
// plugin, which otherwise could trigger layout change while painting by
// invoking NPN_Evaluate. Which would cause bad, bad crashes. Bad crashes.
// TODO(dglazkov): If this approach doesn't produce regressions, move class to
// webplugin_delegate_impl.h and implement for other platforms.
class DrawableContextEnforcer {
 public:
  explicit DrawableContextEnforcer(NPWindow* window)
      : window_(window),
        disposable_dc_(window && !window->window) {
    // If NPWindow is NULL, create a device context with monochrome 1x1 surface
    // and stuff it to NPWindow.
    if (disposable_dc_)
      window_->window = CreateCompatibleDC(NULL);
  }

  ~DrawableContextEnforcer() {
    if (!disposable_dc_)
      return;

    DeleteDC(static_cast<HDC>(window_->window));
    window_->window = NULL;
  }

 private:
  NPWindow* window_;
  bool disposable_dc_;
};

// These are from ntddk.h
typedef LONG NTSTATUS;

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#ifndef STATUS_BUFFER_TOO_SMALL
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#endif

typedef enum _KEY_INFORMATION_CLASS {
  KeyBasicInformation,
  KeyNodeInformation,
  KeyFullInformation,
  KeyNameInformation,
  KeyCachedInformation,
  KeyVirtualizationInformation
} KEY_INFORMATION_CLASS;

typedef struct _KEY_NAME_INFORMATION {
  ULONG NameLength;
  WCHAR Name[1];
} KEY_NAME_INFORMATION, *PKEY_NAME_INFORMATION;

typedef DWORD (__stdcall *ZwQueryKeyType)(
    HANDLE  key_handle,
    int key_information_class,
    PVOID  key_information,
    ULONG  length,
    PULONG  result_length);

// Returns a key's full path.
std::wstring GetKeyPath(HKEY key) {
  if (key == NULL)
    return L"";

  HMODULE dll = GetModuleHandle(L"ntdll.dll");
  if (dll == NULL)
    return L"";

  ZwQueryKeyType func = reinterpret_cast<ZwQueryKeyType>(
      ::GetProcAddress(dll, "ZwQueryKey"));
  if (func == NULL)
    return L"";

  DWORD size = 0;
  DWORD result = 0;
  result = func(key, KeyNameInformation, 0, 0, &size);
  if (result != STATUS_BUFFER_TOO_SMALL)
    return L"";

  scoped_ptr<char[]> buffer(new char[size]);
  if (buffer.get() == NULL)
    return L"";

  result = func(key, KeyNameInformation, buffer.get(), size, &size);
  if (result != STATUS_SUCCESS)
    return L"";

  KEY_NAME_INFORMATION* info =
      reinterpret_cast<KEY_NAME_INFORMATION*>(buffer.get());
  return std::wstring(info->Name, info->NameLength / sizeof(wchar_t));
}

int GetPluginMajorVersion(const WebPluginInfo& plugin_info) {
  Version plugin_version;
  WebPluginInfo::CreateVersionFromString(plugin_info.version, &plugin_version);

  int major_version = 0;
  if (plugin_version.IsValid())
    major_version = plugin_version.components()[0];

  return major_version;
}

}  // namespace

LRESULT CALLBACK WebPluginDelegateImpl::HandleEventMessageFilterHook(
    int code, WPARAM wParam, LPARAM lParam) {
  if (g_current_plugin_instance) {
    g_current_plugin_instance->OnModalLoopEntered();
  } else {
    NOTREACHED();
  }
  return CallNextHookEx(NULL, code, wParam, lParam);
}

LRESULT CALLBACK WebPluginDelegateImpl::MouseHookProc(
    int code, WPARAM wParam, LPARAM lParam) {
  if (code == HC_ACTION) {
    MOUSEHOOKSTRUCT* hook_struct = reinterpret_cast<MOUSEHOOKSTRUCT*>(lParam);
    if (hook_struct)
      HandleCaptureForMessage(hook_struct->hwnd, wParam);
  }

  return CallNextHookEx(NULL, code, wParam, lParam);
}

WebPluginDelegateImpl::WebPluginDelegateImpl(
    WebPlugin* plugin,
    PluginInstance* instance)
    : instance_(instance),
      quirks_(0),
      plugin_(plugin),
      windowless_(false),
      windowed_handle_(NULL),
      windowed_did_set_window_(false),
      plugin_wnd_proc_(NULL),
      last_message_(0),
      is_calling_wndproc(false),
      dummy_window_for_activation_(NULL),
      dummy_window_parent_(NULL),
      old_dummy_window_proc_(NULL),
      handle_event_message_filter_hook_(NULL),
      handle_event_pump_messages_event_(NULL),
      user_gesture_message_posted_(false),
      user_gesture_msg_factory_(this),
      handle_event_depth_(0),
      mouse_hook_(NULL),
      first_set_window_call_(true),
      plugin_has_focus_(false),
      has_webkit_focus_(false),
      containing_view_has_focus_(true),
      creation_succeeded_(false) {
  memset(&window_, 0, sizeof(window_));

  const WebPluginInfo& plugin_info = instance_->plugin_lib()->plugin_info();
  std::wstring filename =
      StringToLowerASCII(plugin_info.path.BaseName().value());

  if (instance_->mime_type() == kFlashPluginSwfMimeType ||
      filename == kFlashPlugin) {
    // Flash only requests windowless plugins if we return a Mozilla user
    // agent.
    instance_->set_use_mozilla_user_agent();
    quirks_ |= PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE;
    quirks_ |= PLUGIN_QUIRK_PATCH_SETCURSOR;
    quirks_ |= PLUGIN_QUIRK_ALWAYS_NOTIFY_SUCCESS;
    quirks_ |= PLUGIN_QUIRK_HANDLE_MOUSE_CAPTURE;
    quirks_ |= PLUGIN_QUIRK_EMULATE_IME;
    quirks_ |= PLUGIN_QUIRK_FAKE_WINDOW_FROM_POINT;
  } else if (filename == kAcrobatReaderPlugin) {
    // Check for the version number above or equal 9.
    int major_version = GetPluginMajorVersion(plugin_info);
    if (major_version >= 9) {
      quirks_ |= PLUGIN_QUIRK_DIE_AFTER_UNLOAD;
      // 9.2 needs this.
      quirks_ |= PLUGIN_QUIRK_SETWINDOW_TWICE;
    }
    quirks_ |= PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS;
  } else if (plugin_info.name.find(L"Windows Media Player") !=
             std::wstring::npos) {
    // Windows Media Player needs two NPP_SetWindow calls.
    quirks_ |= PLUGIN_QUIRK_SETWINDOW_TWICE;

    // Windowless mode doesn't work in the WMP NPAPI plugin.
    quirks_ |= PLUGIN_QUIRK_NO_WINDOWLESS;

    // The media player plugin sets its size on the first NPP_SetWindow call
    // and never updates its size. We should call the underlying NPP_SetWindow
    // only when we have the correct size.
    quirks_ |= PLUGIN_QUIRK_IGNORE_FIRST_SETWINDOW_CALL;

    if (filename == kOldWMPPlugin) {
      // Non-admin users on XP couldn't modify the key to force the new UI.
      quirks_ |= PLUGIN_QUIRK_PATCH_REGENUMKEYEXW;
    }
  } else if (instance_->mime_type() == "audio/x-pn-realaudio-plugin" ||
             filename == kRealPlayerPlugin) {
    quirks_ |= PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY;
  } else if (plugin_info.name.find(L"VLC Multimedia Plugin") !=
             std::wstring::npos ||
             plugin_info.name.find(L"VLC Multimedia Plug-in") !=
             std::wstring::npos) {
    // VLC hangs on NPP_Destroy if we call NPP_SetWindow with a null window
    // handle
    quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;
    int major_version = GetPluginMajorVersion(plugin_info);
    if (major_version == 0) {
      // VLC 0.8.6d and 0.8.6e crash if multiple instances are created.
      quirks_ |= PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES;
    }
  } else if (filename == kSilverlightPlugin) {
    // Explanation for this quirk can be found in
    // WebPluginDelegateImpl::Initialize.
    quirks_ |= PLUGIN_QUIRK_PATCH_SETCURSOR;
  } else if (plugin_info.name.find(L"DivX Web Player") !=
             std::wstring::npos) {
    // The divx plugin sets its size on the first NPP_SetWindow call and never
    // updates its size. We should call the underlying NPP_SetWindow only when
    // we have the correct size.
    quirks_ |= PLUGIN_QUIRK_IGNORE_FIRST_SETWINDOW_CALL;
  }
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  if (::IsWindow(dummy_window_for_activation_)) {
    WNDPROC current_wnd_proc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtr(dummy_window_for_activation_, GWLP_WNDPROC));
    if (current_wnd_proc == DummyWindowProc) {
      SetWindowLongPtr(dummy_window_for_activation_,
                       GWLP_WNDPROC,
                       reinterpret_cast<LONG_PTR>(old_dummy_window_proc_));
    }
    ::DestroyWindow(dummy_window_for_activation_);
  }

  DestroyInstance();

  if (!windowless_)
    WindowedDestroyWindow();

  if (handle_event_pump_messages_event_) {
    CloseHandle(handle_event_pump_messages_event_);
  }
}

bool WebPluginDelegateImpl::PlatformInitialize() {
  plugin_->SetWindow(windowed_handle_);

  if (windowless_) {
    CreateDummyWindowForActivation();
    handle_event_pump_messages_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
    plugin_->SetWindowlessData(
        handle_event_pump_messages_event_,
        reinterpret_cast<gfx::NativeViewId>(dummy_window_for_activation_));
  }

  // Windowless plugins call the WindowFromPoint API and passes the result of
  // that to the TrackPopupMenu API call as the owner window. This causes the
  // API to fail as the API expects the window handle to live on the same
  // thread as the caller. It works in the other browsers as the plugin lives
  // on the browser thread. Our workaround is to intercept the TrackPopupMenu
  // API and replace the window handle with the dummy activation window.
  if (windowless_ && !g_iat_patch_track_popup_menu.Pointer()->is_patched()) {
    g_iat_patch_track_popup_menu.Pointer()->Patch(
        GetPluginPath().value().c_str(), "user32.dll", "TrackPopupMenu",
        WebPluginDelegateImpl::TrackPopupMenuPatch);
  }

  // Windowless plugins can set cursors by calling the SetCursor API. This
  // works because the thread inputs of the browser UI thread and the plugin
  // thread are attached. We intercept the SetCursor API for windowless
  // plugins and remember the cursor being set. This is shipped over to the
  // browser in the HandleEvent call, which ensures that the cursor does not
  // change when a windowless plugin instance changes the cursor
  // in a background tab.
  if (windowless_ && !g_iat_patch_set_cursor.Pointer()->is_patched() &&
      (quirks_ & PLUGIN_QUIRK_PATCH_SETCURSOR)) {
    g_iat_patch_set_cursor.Pointer()->Patch(
        GetPluginPath().value().c_str(), "user32.dll", "SetCursor",
        WebPluginDelegateImpl::SetCursorPatch);
  }

  // The windowed flash plugin has a bug which occurs when the plugin enters
  // fullscreen mode. It basically captures the mouse on WM_LBUTTONDOWN and
  // does not release capture correctly causing it to stop receiving
  // subsequent mouse events. This problem is also seen in Safari where there
  // is code to handle this in the wndproc. However the plugin subclasses the
  // window again in WM_LBUTTONDOWN before entering full screen. As a result
  // Safari does not receive the WM_LBUTTONUP message. To workaround this
  // issue we use a per thread mouse hook. This bug does not occur in Firefox
  // and opera. Firefox has code similar to Safari. It could well be a bug in
  // the flash plugin, which only occurs in webkit based browsers.
  if (quirks_ & PLUGIN_QUIRK_HANDLE_MOUSE_CAPTURE) {
    mouse_hook_ = SetWindowsHookEx(WH_MOUSE, MouseHookProc, NULL,
                                    GetCurrentThreadId());
  }

  // On XP, WMP will use its old UI unless a registry key under HKLM has the
  // name of the current process.  We do it in the installer for admin users,
  // for the rest patch this function.
  if ((quirks_ & PLUGIN_QUIRK_PATCH_REGENUMKEYEXW) &&
      base::win::GetVersion() == base::win::VERSION_XP &&
      (base::win::RegKey().Open(HKEY_LOCAL_MACHINE,
          L"SOFTWARE\\Microsoft\\MediaPlayer\\ShimInclusionList\\chrome.exe",
          KEY_READ) != ERROR_SUCCESS) &&
      !g_iat_patch_reg_enum_key_ex_w.Pointer()->is_patched()) {
    g_iat_patch_reg_enum_key_ex_w.Pointer()->Patch(
        L"wmpdxm.dll", "advapi32.dll", "RegEnumKeyExW",
        WebPluginDelegateImpl::RegEnumKeyExWPatch);
  }

  // Flash retrieves the pointers to IMM32 functions with GetProcAddress() calls
  // and use them to retrieve IME data. We add a patch to this function so we
  // can dispatch these IMM32 calls to the WebPluginIMEWin class, which emulates
  // IMM32 functions for Flash.
  if (!g_iat_patch_get_proc_address.Pointer()->is_patched() &&
      (quirks_ & PLUGIN_QUIRK_EMULATE_IME)) {
    g_iat_patch_get_proc_address.Pointer()->Patch(
        GetPluginPath().value().c_str(), "kernel32.dll", "GetProcAddress",
        GetProcAddressPatch);
  }

  if (windowless_ && !g_iat_patch_window_from_point.Pointer()->is_patched() &&
      (quirks_ & PLUGIN_QUIRK_FAKE_WINDOW_FROM_POINT)) {
    g_iat_patch_window_from_point.Pointer()->Patch(
        GetPluginPath().value().c_str(), "user32.dll", "WindowFromPoint",
        WebPluginDelegateImpl::WindowFromPointPatch);
  }

  return true;
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
  if (!instance_->plugin_lib())
    return;

  // Unpatch if this is the last plugin instance.
  if (instance_->plugin_lib()->instance_count() != 1)
    return;

  if (g_iat_patch_set_cursor.Pointer()->is_patched())
    g_iat_patch_set_cursor.Pointer()->Unpatch();

  if (g_iat_patch_track_popup_menu.Pointer()->is_patched())
    g_iat_patch_track_popup_menu.Pointer()->Unpatch();

  if (g_iat_patch_reg_enum_key_ex_w.Pointer()->is_patched())
    g_iat_patch_reg_enum_key_ex_w.Pointer()->Unpatch();

  if (g_iat_patch_window_from_point.Pointer()->is_patched())
    g_iat_patch_window_from_point.Pointer()->Unpatch();

  if (mouse_hook_) {
    UnhookWindowsHookEx(mouse_hook_);
    mouse_hook_ = NULL;
  }
}

void WebPluginDelegateImpl::Paint(SkCanvas* canvas, const gfx::Rect& rect) {
  if (windowless_ && skia::SupportsPlatformPaint(canvas)) {
    skia::ScopedPlatformPaint scoped_platform_paint(canvas);
    HDC hdc = scoped_platform_paint.GetPlatformSurface();
    WindowlessPaint(hdc, rect);
  }
}

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  DCHECK(!windowed_handle_);

  RegisterNativeWindowClass();

  // The window will be sized and shown later.
  windowed_handle_ = CreateWindowEx(
      WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
      kNativeWindowClassName,
      0,
      WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      0,
      0,
      0,
      0,
      GetDesktopWindow(),
      0,
      GetModuleHandle(NULL),
      0);
  if (windowed_handle_ == 0)
    return false;

    // This is a tricky workaround for Issue 2673 in chromium "Flash: IME not
    // available". To use IMEs in this window, we have to make Windows attach
  // IMEs to this window (i.e. load IME DLLs, attach them to this process, and
  // add their message hooks to this window). Windows attaches IMEs while this
  // process creates a top-level window. On the other hand, to layout this
  // window correctly in the given parent window (RenderWidgetHostViewWin or
  // RenderWidgetHostViewAura), this window should be a child window of the
  // parent window. To satisfy both of the above conditions, this code once
  // creates a top-level window and change it to a child window of the parent
  // window (in the browser process).
    SetWindowLongPtr(windowed_handle_, GWL_STYLE,
                     WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

  BOOL result = SetProp(windowed_handle_, kWebPluginDelegateProperty, this);
  DCHECK(result == TRUE) << "SetProp failed, last error = " << GetLastError();
  // Get the name and version of the plugin, create atoms and set them in a
  // window property. Use atoms so that other processes can access the name and
  // version of the plugin that this window is hosting.
  if (instance_ != NULL) {
    PluginLib* plugin_lib = instance()->plugin_lib();
    if (plugin_lib != NULL) {
      std::wstring plugin_name = plugin_lib->plugin_info().name;
      if (!plugin_name.empty()) {
        ATOM plugin_name_atom = GlobalAddAtomW(plugin_name.c_str());
        DCHECK_NE(0, plugin_name_atom);
        result = SetProp(windowed_handle_,
            kPluginNameAtomProperty,
            reinterpret_cast<HANDLE>(plugin_name_atom));
        DCHECK(result == TRUE) << "SetProp failed, last error = " <<
            GetLastError();
      }
      base::string16 plugin_version = plugin_lib->plugin_info().version;
      if (!plugin_version.empty()) {
        ATOM plugin_version_atom = GlobalAddAtomW(plugin_version.c_str());
        DCHECK_NE(0, plugin_version_atom);
        result = SetProp(windowed_handle_,
            kPluginVersionAtomProperty,
            reinterpret_cast<HANDLE>(plugin_version_atom));
        DCHECK(result == TRUE) << "SetProp failed, last error = " <<
            GetLastError();
      }
    }
  }

  // Calling SetWindowLongPtrA here makes the window proc ASCII, which is
  // required by at least the Shockwave Director plug-in.
  SetWindowLongPtrA(windowed_handle_,
                    GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(DefWindowProcA));

  return true;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  if (windowed_handle_ != NULL) {
    // Unsubclass the window.
    WNDPROC current_wnd_proc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtr(windowed_handle_, GWLP_WNDPROC));
    if (current_wnd_proc == NativeWndProc) {
      SetWindowLongPtr(windowed_handle_,
                       GWLP_WNDPROC,
                       reinterpret_cast<LONG_PTR>(plugin_wnd_proc_));
    }

    plugin_->WillDestroyWindow(windowed_handle_);

    DestroyWindow(windowed_handle_);
    windowed_handle_ = 0;
  }
}

// Erase all messages in the queue destined for a particular window.
// When windows are closing, callers should use this function to clear
// the queue.
// static
void WebPluginDelegateImpl::ClearThrottleQueueForWindow(HWND window) {
  ThrottleQueue* throttle_queue = g_throttle_queue.Pointer();

  ThrottleQueue::iterator it;
  for (it = throttle_queue->begin(); it != throttle_queue->end(); ) {
    if (it->hwnd == window) {
      it = throttle_queue->erase(it);
    } else {
      it++;
    }
  }
}

// Delayed callback for processing throttled messages.
// Throttled messages are aggregated globally across all plugins.
// static
void WebPluginDelegateImpl::OnThrottleMessage() {
  // The current algorithm walks the list and processes the first
  // message it finds for each plugin.  It is important to service
  // all active plugins with each pass through the throttle, otherwise
  // we see video jankiness.  Copy the set to notify before notifying
  // since we may re-enter OnThrottleMessage from CallWindowProc!
  ThrottleQueue* throttle_queue = g_throttle_queue.Pointer();
  ThrottleQueue notify_queue;
  std::set<HWND> processed;

  ThrottleQueue::iterator it = throttle_queue->begin();
  while (it != throttle_queue->end()) {
    const MSG& msg = *it;
    if (processed.find(msg.hwnd) == processed.end()) {
      processed.insert(msg.hwnd);
      notify_queue.push_back(msg);
      it = throttle_queue->erase(it);
    } else {
      it++;
    }
  }

  // Due to re-entrancy, we must save our queue state now.  Otherwise, we may
  // self-post below, and *also* start up another delayed task when the first
  // entry is pushed onto the queue in ThrottleMessage().
  bool throttle_queue_was_empty = throttle_queue->empty();

  for (it = notify_queue.begin(); it != notify_queue.end(); ++it) {
    const MSG& msg = *it;
    WNDPROC proc = reinterpret_cast<WNDPROC>(msg.time);
    // It is possible that the window was closed after we queued
    // this message.  This is a rare event; just verify the window
    // is alive.  (see also bug 1259488)
    if (IsWindow(msg.hwnd))
      CallWindowProc(proc, msg.hwnd, msg.message, msg.wParam, msg.lParam);
  }

  if (!throttle_queue_was_empty) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WebPluginDelegateImpl::OnThrottleMessage),
        base::TimeDelta::FromMilliseconds(kFlashWMUSERMessageThrottleDelayMs));
  }
}

// Schedule a windows message for delivery later.
// static
void WebPluginDelegateImpl::ThrottleMessage(WNDPROC proc, HWND hwnd,
                                            UINT message, WPARAM wParam,
                                            LPARAM lParam) {
  MSG msg;
  msg.time = reinterpret_cast<DWORD>(proc);
  msg.hwnd = hwnd;
  msg.message = message;
  msg.wParam = wParam;
  msg.lParam = lParam;

  ThrottleQueue* throttle_queue = g_throttle_queue.Pointer();

  throttle_queue->push_back(msg);

  if (throttle_queue->size() == 1) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WebPluginDelegateImpl::OnThrottleMessage),
        base::TimeDelta::FromMilliseconds(kFlashWMUSERMessageThrottleDelayMs));
  }
}

// We go out of our way to find the hidden windows created by Flash for
// windowless plugins.  We throttle the rate at which they deliver messages
// so that they will not consume outrageous amounts of CPU.
// static
LRESULT CALLBACK WebPluginDelegateImpl::FlashWindowlessWndProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  std::map<HWND, WNDPROC>::iterator index =
      g_window_handle_proc_map.Get().find(hwnd);

  WNDPROC old_proc = (*index).second;
  DCHECK(old_proc);

  switch (message) {
    case WM_NCDESTROY: {
      WebPluginDelegateImpl::ClearThrottleQueueForWindow(hwnd);
      g_window_handle_proc_map.Get().erase(index);
      break;
    }
    // Flash may flood the message queue with WM_USER+1 message causing 100% CPU
    // usage.  See https://bugzilla.mozilla.org/show_bug.cgi?id=132759.  We
    // prevent this by throttling the messages.
    case WM_USER + 1: {
      WebPluginDelegateImpl::ThrottleMessage(old_proc, hwnd, message, wparam,
                                             lparam);
      return TRUE;
    }

    default: {
      break;
    }
  }
  return CallWindowProc(old_proc, hwnd, message, wparam, lparam);
}

LRESULT CALLBACK WebPluginDelegateImpl::DummyWindowProc(
    HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
  WebPluginDelegateImpl* delegate = reinterpret_cast<WebPluginDelegateImpl*>(
      GetProp(hwnd, kWebPluginDelegateProperty));
  CHECK(delegate);
  if (message == WM_WINDOWPOSCHANGING) {
    // We need to know when the dummy window is parented because windowless
    // plugins need the parent window for things like menus. There's no message
    // for a parent being changed, but a WM_WINDOWPOSCHANGING is sent so we
    // check every time we get it.
    // For non-aura builds, this never changes since RenderWidgetHostViewWin's
    // window is constant. For aura builds, this changes every time the tab gets
    // dragged to a new window.
    HWND parent = GetParent(hwnd);
    if (parent != delegate->dummy_window_parent_) {
      delegate->dummy_window_parent_ = parent;

      // Set the containing window handle as the instance window handle. This is
      // what Safari does. Not having a valid window handle causes subtle bugs
      // with plugins which retrieve the window handle and use it for things
      // like context menus. The window handle can be retrieved via
      // NPN_GetValue of NPNVnetscapeWindow.
      delegate->instance_->set_window_handle(parent);

      // The plugin caches the result of NPNVnetscapeWindow when we originally
      // called NPP_SetWindow, so force it to get the new value.
      delegate->WindowlessSetWindow();
    }
  } else if (message == WM_NCDESTROY) {
    RemoveProp(hwnd, kWebPluginDelegateProperty);
  }
  return CallWindowProc(
      delegate->old_dummy_window_proc_, hwnd, message, w_param, l_param);
}

// Callback for enumerating the Flash windows.
BOOL CALLBACK EnumFlashWindows(HWND window, LPARAM arg) {
  WNDPROC wnd_proc = reinterpret_cast<WNDPROC>(arg);
  TCHAR class_name[1024];
  if (!RealGetWindowClass(window, class_name,
                          sizeof(class_name)/sizeof(TCHAR))) {
    LOG(ERROR) << "RealGetWindowClass failure: " << GetLastError();
    return FALSE;
  }

  if (wcscmp(class_name, L"SWFlash_PlaceholderX"))
    return TRUE;

  WNDPROC current_wnd_proc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtr(window, GWLP_WNDPROC));
  if (current_wnd_proc != wnd_proc) {
    WNDPROC old_flash_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
        window, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(wnd_proc)));
    DCHECK(old_flash_proc);
    g_window_handle_proc_map.Get()[window] = old_flash_proc;
  }

  return TRUE;
}

bool WebPluginDelegateImpl::CreateDummyWindowForActivation() {
  DCHECK(!dummy_window_for_activation_);

  dummy_window_for_activation_ = CreateWindowEx(
    0,
    L"Static",
    kDummyActivationWindowName,
    WS_CHILD,
    0,
    0,
    0,
    0,
    // We don't know the parent of the dummy window yet, so just set it to the
    // desktop and it'll get parented by the browser.
    GetDesktopWindow(),
    0,
    GetModuleHandle(NULL),
    0);

  if (dummy_window_for_activation_ == 0)
    return false;

  BOOL result = SetProp(dummy_window_for_activation_,
                        kWebPluginDelegateProperty, this);
  DCHECK(result == TRUE) << "SetProp failed, last error = " << GetLastError();
  old_dummy_window_proc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
      dummy_window_for_activation_, GWLP_WNDPROC,
      reinterpret_cast<LONG_PTR>(DummyWindowProc)));

  // Flash creates background windows which use excessive CPU in our
  // environment; we wrap these windows and throttle them so that they don't
  // get out of hand.
  if (!EnumThreadWindows(::GetCurrentThreadId(), EnumFlashWindows,
      reinterpret_cast<LPARAM>(
      &WebPluginDelegateImpl::FlashWindowlessWndProc))) {
    // Log that this happened.  Flash will still work; it just means the
    // throttle isn't installed (and Flash will use more CPU).
    NOTREACHED();
    LOG(ERROR) << "Failed to wrap all windowless Flash windows";
  }
  return true;
}

bool WebPluginDelegateImpl::WindowedReposition(
    const gfx::Rect& window_rect_in_dip,
    const gfx::Rect& clip_rect_in_dip) {
  if (!windowed_handle_) {
    NOTREACHED();
    return false;
  }

  gfx::Rect window_rect = gfx::win::DIPToScreenRect(window_rect_in_dip);
  gfx::Rect clip_rect = gfx::win::DIPToScreenRect(clip_rect_in_dip);
  if (window_rect_ == window_rect && clip_rect_ == clip_rect)
    return false;

  // We only set the plugin's size here.  Its position is moved elsewhere, which
  // allows the window moves/scrolling/clipping to be synchronized with the page
  // and other windows.
  // If the plugin window has no parent, then don't focus it because it isn't
  // being displayed anywhere. See:
  // http://code.google.com/p/chromium/issues/detail?id=32658
  if (window_rect.size() != window_rect_.size()) {
    UINT flags = SWP_NOMOVE | SWP_NOZORDER;
    if (!GetParent(windowed_handle_))
      flags |= SWP_NOACTIVATE;
    ::SetWindowPos(windowed_handle_,
                   NULL,
                   0,
                   0,
                   window_rect.width(),
                   window_rect.height(),
                   flags);
  }

  window_rect_ = window_rect;
  clip_rect_ = clip_rect;

  // Ensure that the entire window gets repainted.
  ::InvalidateRect(windowed_handle_, NULL, FALSE);

  return true;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  if (!instance_)
    return;

  if (!windowed_handle_) {
    NOTREACHED();
    return;
  }

  instance()->set_window_handle(windowed_handle_);

  DCHECK(!instance()->windowless());

  window_.clipRect.top = std::max(0, clip_rect_.y());
  window_.clipRect.left = std::max(0, clip_rect_.x());
  window_.clipRect.bottom = std::max(0, clip_rect_.y() + clip_rect_.height());
  window_.clipRect.right = std::max(0, clip_rect_.x() + clip_rect_.width());
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = 0;
  window_.y = 0;

  window_.window = windowed_handle_;
  window_.type = NPWindowTypeWindow;

  // Reset this flag before entering the instance in case of side-effects.
  windowed_did_set_window_ = true;

  NPError err = instance()->NPP_SetWindow(&window_);
  if (quirks_ & PLUGIN_QUIRK_SETWINDOW_TWICE)
    instance()->NPP_SetWindow(&window_);

  WNDPROC current_wnd_proc = reinterpret_cast<WNDPROC>(
        GetWindowLongPtr(windowed_handle_, GWLP_WNDPROC));
  if (current_wnd_proc != NativeWndProc) {
    plugin_wnd_proc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(windowed_handle_,
                         GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(NativeWndProc)));
  }
}

ATOM WebPluginDelegateImpl::RegisterNativeWindowClass() {
  static bool have_registered_window_class = false;
  if (have_registered_window_class == true)
    return true;

  have_registered_window_class = true;

  WNDCLASSEX wcex;
  wcex.cbSize         = sizeof(WNDCLASSEX);
  wcex.style          = CS_DBLCLKS;
  wcex.lpfnWndProc    = WrapperWindowProc;
  wcex.cbClsExtra     = 0;
  wcex.cbWndExtra     = 0;
  wcex.hInstance      = GetModuleHandle(NULL);
  wcex.hIcon          = 0;
  wcex.hCursor        = 0;
  // Some plugins like windows media player 11 create child windows parented
  // by our plugin window, where the media content is rendered. These plugins
  // dont implement WM_ERASEBKGND, which causes painting issues, when the
  // window where the media is rendered is moved around. DefWindowProc does
  // implement WM_ERASEBKGND correctly if we have a valid background brush.
  wcex.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
  wcex.lpszMenuName   = 0;
  wcex.lpszClassName  = kNativeWindowClassName;
  wcex.hIconSm        = 0;

  return RegisterClassEx(&wcex);
}

LRESULT CALLBACK WebPluginDelegateImpl::WrapperWindowProc(
    HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  // This is another workaround for Issue 2673 in chromium "Flash: IME not
  // available". Somehow, the CallWindowProc() function does not dispatch
  // window messages when its first parameter is a handle representing the
  // DefWindowProc() function. To avoid this problem, this code creates a
  // wrapper function which just encapsulates the DefWindowProc() function
  // and set it as the window procedure of a windowed plug-in.
  return DefWindowProc(hWnd, message, wParam, lParam);
}

// Returns true if the message passed in corresponds to a user gesture.
static bool IsUserGestureMessage(unsigned int message) {
  switch (message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
      return true;

    default:
      break;
  }

  return false;
}

LRESULT CALLBACK WebPluginDelegateImpl::NativeWndProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  WebPluginDelegateImpl* delegate = reinterpret_cast<WebPluginDelegateImpl*>(
      GetProp(hwnd, kWebPluginDelegateProperty));
  if (!delegate) {
    NOTREACHED();
    return 0;
  }

  if (message == delegate->last_message_ &&
      delegate->GetQuirks() & PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY &&
      delegate->is_calling_wndproc) {
    // Real may go into a state where it recursively dispatches the same event
    // when subclassed.  See https://bugzilla.mozilla.org/show_bug.cgi?id=192914
    // We only do the recursive check for Real because it's possible and valid
    // for a plugin to synchronously dispatch a message to itself such that it
    // looks like it's in recursion.
    return TRUE;
  }

  // Flash may flood the message queue with WM_USER+1 message causing 100% CPU
  // usage.  See https://bugzilla.mozilla.org/show_bug.cgi?id=132759.  We
  // prevent this by throttling the messages.
  if (message == WM_USER + 1 &&
      delegate->GetQuirks() & PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE) {
    WebPluginDelegateImpl::ThrottleMessage(delegate->plugin_wnd_proc_, hwnd,
                                           message, wparam, lparam);
    return FALSE;
  }

  LRESULT result;
  uint32 old_message = delegate->last_message_;
  delegate->last_message_ = message;

  static UINT custom_msg = RegisterWindowMessage(kPaintMessageName);
  if (message == custom_msg) {
    // Get the invalid rect which is in screen coordinates and convert to
    // window coordinates.
    gfx::Rect invalid_rect;
    invalid_rect.set_x(static_cast<short>(LOWORD(wparam)));
    invalid_rect.set_y(static_cast<short>(HIWORD(wparam)));
    invalid_rect.set_width(static_cast<short>(LOWORD(lparam)));
    invalid_rect.set_height(static_cast<short>(HIWORD(lparam)));

    RECT window_rect;
    GetWindowRect(hwnd, &window_rect);
    invalid_rect.Offset(-window_rect.left, -window_rect.top);

    // The plugin window might have non-client area.   If we don't pass in
    // RDW_FRAME then the children don't receive WM_NCPAINT messages while
    // scrolling, which causes painting problems (http://b/issue?id=923945).
    uint32 flags = RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME;

    // If a plugin (like Google Earth or Java) has child windows that are hosted
    // in a different process, then RedrawWindow with UPDATENOW will
    // synchronously wait for this call to complete.  Some messages are pumped
    // but not others, which could lead to a deadlock.  So avoid reentrancy by
    // only synchronously calling RedrawWindow once at a time.
    if (old_message != custom_msg)
      flags |= RDW_UPDATENOW;
    RECT rect = invalid_rect.ToRECT();
    RedrawWindow(hwnd, &rect, NULL, flags);
    result = FALSE;
  } else {
    delegate->is_calling_wndproc = true;

    if (!delegate->user_gesture_message_posted_ &&
        IsUserGestureMessage(message)) {
      delegate->user_gesture_message_posted_ = true;

      delegate->instance()->PushPopupsEnabledState(true);

      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&WebPluginDelegateImpl::OnUserGestureEnd,
                     delegate->user_gesture_msg_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kWindowedPluginPopupTimerMs));
    }

    HandleCaptureForMessage(hwnd, message);

    // Maintain a local/global stack for the g_current_plugin_instance variable
    // as this may be a nested invocation.
    WebPluginDelegateImpl* last_plugin_instance = g_current_plugin_instance;

    g_current_plugin_instance = delegate;

    result = CallWindowProc(
        delegate->plugin_wnd_proc_, hwnd, message, wparam, lparam);

    // The plugin instance may have been destroyed in the CallWindowProc call
    // above. This will also destroy the plugin window. Before attempting to
    // access the WebPluginDelegateImpl instance we validate if the window is
    // still valid.
    if (::IsWindow(hwnd))
      delegate->is_calling_wndproc = false;

    g_current_plugin_instance = last_plugin_instance;

    if (message == WM_NCDESTROY) {
      RemoveProp(hwnd, kWebPluginDelegateProperty);
      ATOM plugin_name_atom = reinterpret_cast<ATOM>(
          RemoveProp(hwnd, kPluginNameAtomProperty));
      if (plugin_name_atom != 0)
        GlobalDeleteAtom(plugin_name_atom);
      ATOM plugin_version_atom = reinterpret_cast<ATOM>(
          RemoveProp(hwnd, kPluginVersionAtomProperty));
      if (plugin_version_atom != 0)
        GlobalDeleteAtom(plugin_version_atom);
      ClearThrottleQueueForWindow(hwnd);
    }
  }
  if (::IsWindow(hwnd))
    delegate->last_message_ = old_message;
  return result;
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  bool window_rect_changed = (window_rect_ != window_rect);
  // Only resend to the instance if the geometry has changed.
  if (!window_rect_changed && clip_rect == clip_rect_)
    return;

  clip_rect_ = clip_rect;
  window_rect_ = window_rect;

  WindowlessSetWindow();

  if (window_rect_changed) {
    WINDOWPOS win_pos = {0};
    win_pos.x = window_rect_.x();
    win_pos.y = window_rect_.y();
    win_pos.cx = window_rect_.width();
    win_pos.cy = window_rect_.height();

    NPEvent pos_changed_event;
    pos_changed_event.event = WM_WINDOWPOSCHANGED;
    pos_changed_event.wParam = 0;
    pos_changed_event.lParam = PtrToUlong(&win_pos);

    instance()->NPP_HandleEvent(&pos_changed_event);
  }
}

void WebPluginDelegateImpl::WindowlessPaint(HDC hdc,
                                            const gfx::Rect& damage_rect) {
  DCHECK(hdc);

  RECT damage_rect_win;
  damage_rect_win.left   = damage_rect.x();  // + window_rect_.x();
  damage_rect_win.top    = damage_rect.y();  // + window_rect_.y();
  damage_rect_win.right  = damage_rect_win.left + damage_rect.width();
  damage_rect_win.bottom = damage_rect_win.top + damage_rect.height();

  // Save away the old HDC as this could be a nested invocation.
  void* old_dc = window_.window;
  window_.window = hdc;

  NPEvent paint_event;
  paint_event.event = WM_PAINT;
  // NOTE: NPAPI is not 64bit safe.  It puts pointers into 32bit values.
  paint_event.wParam = PtrToUlong(hdc);
  paint_event.lParam = PtrToUlong(&damage_rect_win);
  base::StatsRate plugin_paint("Plugin.Paint");
  base::StatsScope<base::StatsRate> scope(plugin_paint);
  instance()->NPP_HandleEvent(&paint_event);
  window_.window = old_dc;
}

void WebPluginDelegateImpl::WindowlessSetWindow() {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  DCHECK(instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;
  DrawableContextEnforcer enforcer(&window_);

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

bool WebPluginDelegateImpl::PlatformSetPluginHasFocus(bool focused) {
  DCHECK(instance()->windowless());

  NPEvent focus_event;
  focus_event.event = focused ? WM_SETFOCUS : WM_KILLFOCUS;
  focus_event.wParam = 0;
  focus_event.lParam = 0;

  instance()->NPP_HandleEvent(&focus_event);
  return true;
}

static bool NPEventFromWebMouseEvent(const WebMouseEvent& event,
                                     NPEvent* np_event) {
  np_event->lParam = static_cast<uint32>(MAKELPARAM(event.windowX,
                                                   event.windowY));
  np_event->wParam = 0;

  if (event.modifiers & WebInputEvent::ControlKey)
    np_event->wParam |= MK_CONTROL;
  if (event.modifiers & WebInputEvent::ShiftKey)
    np_event->wParam |= MK_SHIFT;
  if (event.modifiers & WebInputEvent::LeftButtonDown)
    np_event->wParam |= MK_LBUTTON;
  if (event.modifiers & WebInputEvent::MiddleButtonDown)
    np_event->wParam |= MK_MBUTTON;
  if (event.modifiers & WebInputEvent::RightButtonDown)
    np_event->wParam |= MK_RBUTTON;

  switch (event.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
      np_event->event = WM_MOUSEMOVE;
      return true;
    case WebInputEvent::MouseDown:
      switch (event.button) {
        case WebMouseEvent::ButtonLeft:
          np_event->event = WM_LBUTTONDOWN;
          break;
        case WebMouseEvent::ButtonMiddle:
          np_event->event = WM_MBUTTONDOWN;
          break;
        case WebMouseEvent::ButtonRight:
          np_event->event = WM_RBUTTONDOWN;
          break;
      }
      return true;
    case WebInputEvent::MouseUp:
      switch (event.button) {
        case WebMouseEvent::ButtonLeft:
          np_event->event = WM_LBUTTONUP;
          break;
        case WebMouseEvent::ButtonMiddle:
          np_event->event = WM_MBUTTONUP;
          break;
        case WebMouseEvent::ButtonRight:
          np_event->event = WM_RBUTTONUP;
          break;
      }
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebKeyboardEvent(const WebKeyboardEvent& event,
                                        NPEvent* np_event) {
  np_event->wParam = event.windowsKeyCode;

  switch (event.type) {
    case WebInputEvent::KeyDown:
      np_event->event = WM_KEYDOWN;
      np_event->lParam = 0;
      return true;
    case WebInputEvent::Char:
      np_event->event = WM_CHAR;
      np_event->lParam = 0;
      return true;
    case WebInputEvent::KeyUp:
      np_event->event = WM_KEYUP;
      np_event->lParam = 0x8000;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool NPEventFromWebInputEvent(const WebInputEvent& event,
                                     NPEvent* np_event) {
  switch (event.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::MouseDown:
    case WebInputEvent::MouseUp:
      if (event.size < sizeof(WebMouseEvent)) {
        NOTREACHED();
        return false;
      }
      return NPEventFromWebMouseEvent(
          *static_cast<const WebMouseEvent*>(&event), np_event);
    case WebInputEvent::KeyDown:
    case WebInputEvent::Char:
    case WebInputEvent::KeyUp:
      if (event.size < sizeof(WebKeyboardEvent)) {
        NOTREACHED();
        return false;
      }
      return NPEventFromWebKeyboardEvent(
          *static_cast<const WebKeyboardEvent*>(&event), np_event);
    default:
      return false;
  }
}

bool WebPluginDelegateImpl::PlatformHandleInputEvent(
    const WebInputEvent& event, WebCursor::CursorInfo* cursor_info) {
  DCHECK(cursor_info != NULL);

  NPEvent np_event;
  if (!NPEventFromWebInputEvent(event, &np_event)) {
    return false;
  }

  // Allow this plug-in to access this IME emulator through IMM32 API while the
  // plug-in is processing this event.
  if (GetQuirks() & PLUGIN_QUIRK_EMULATE_IME) {
    if (!plugin_ime_)
      plugin_ime_.reset(new WebPluginIMEWin);
  }
  WebPluginIMEWin::ScopedLock lock(
      event.isKeyboardEventType(event.type) ? plugin_ime_.get() : NULL);

  HWND last_focus_window = NULL;

  if (ShouldTrackEventForModalLoops(&np_event)) {
    // A windowless plugin can enter a modal loop in a NPP_HandleEvent call.
    // For e.g. Flash puts up a context menu when we right click on the
    // windowless plugin area. We detect this by setting up a message filter
    // hook pror to calling NPP_HandleEvent on the plugin and unhook on
    // return from NPP_HandleEvent. If the plugin does enter a modal loop
    // in that context we unhook on receiving the first notification in
    // the message filter hook.
    handle_event_message_filter_hook_ =
        SetWindowsHookEx(WH_MSGFILTER, HandleEventMessageFilterHook, NULL,
                         GetCurrentThreadId());
    // To ensure that the plugin receives keyboard events we set focus to the
    // dummy window.
    // TODO(iyengar) We need a framework in the renderer to identify which
    // windowless plugin is under the mouse and to handle this. This would
    // also require some changes in RenderWidgetHost to detect this in the
    // WM_MOUSEACTIVATE handler and inform the renderer accordingly.
    bool valid = GetParent(dummy_window_for_activation_) != GetDesktopWindow();
    if (valid) {
      last_focus_window = ::SetFocus(dummy_window_for_activation_);
    } else {
      NOTREACHED() << "Dummy window not parented";
    }
  }

  bool old_task_reentrancy_state =
      base::MessageLoop::current()->NestableTasksAllowed();

  // Maintain a local/global stack for the g_current_plugin_instance variable
  // as this may be a nested invocation.
  WebPluginDelegateImpl* last_plugin_instance = g_current_plugin_instance;

  g_current_plugin_instance = this;

  handle_event_depth_++;

  bool popups_enabled = false;

  if (IsUserGestureMessage(np_event.event)) {
    instance()->PushPopupsEnabledState(true);
    popups_enabled = true;
  }

  bool ret = instance()->NPP_HandleEvent(&np_event) != 0;

  if (popups_enabled) {
    instance()->PopPopupsEnabledState();
  }

  // Flash and SilverLight always return false, even when they swallow the
  // event.  Flash does this because it passes the event to its window proc,
  // which is supposed to return 0 if an event was handled.  There are few
  // exceptions, such as IME, where it sometimes returns true.
  ret = true;

  if (np_event.event == WM_MOUSEMOVE) {
    current_windowless_cursor_.InitFromExternalCursor(GetCursor());
    // Snag a reference to the current cursor ASAP in case the plugin modified
    // it. There is a nasty race condition here with the multiprocess browser
    // as someone might be setting the cursor in the main process as well.
    current_windowless_cursor_.GetCursorInfo(cursor_info);
  }

  handle_event_depth_--;

  g_current_plugin_instance = last_plugin_instance;

  // We could have multiple NPP_HandleEvent calls nested together in case
  // the plugin enters a modal loop. Reset the pump messages event when
  // the outermost NPP_HandleEvent call unwinds.
  if (handle_event_depth_ == 0) {
    ResetEvent(handle_event_pump_messages_event_);
  }

  // If we didn't enter a modal loop, need to unhook the filter.
  if (handle_event_message_filter_hook_) {
    UnhookWindowsHookEx(handle_event_message_filter_hook_);
    handle_event_message_filter_hook_ = NULL;
  }

  if (::IsWindow(last_focus_window)) {
    // Restore the nestable tasks allowed state in the message loop and reset
    // the os modal loop state as the plugin returned from the TrackPopupMenu
    // API call.
    base::MessageLoop::current()->SetNestableTasksAllowed(
        old_task_reentrancy_state);
    base::MessageLoop::current()->set_os_modal_loop(false);
    // The Flash plugin at times sets focus to its hidden top level window
    // with class name SWFlash_PlaceholderX. This causes the chrome browser
    // window to receive a WM_ACTIVATEAPP message as a top level window from
    // another thread is now active. We end up in a state where the chrome
    // browser window is not active even though the user clicked on it.
    // Our workaround for this is to send over a raw
    // WM_LBUTTONDOWN/WM_LBUTTONUP combination to the last focus window, which
    // does the trick.
    if (dummy_window_for_activation_ != ::GetFocus()) {
      INPUT input_info = {0};
      input_info.type = INPUT_MOUSE;
      input_info.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
      ::SendInput(1, &input_info, sizeof(INPUT));

      input_info.type = INPUT_MOUSE;
      input_info.mi.dwFlags = MOUSEEVENTF_LEFTUP;
      ::SendInput(1, &input_info, sizeof(INPUT));
    } else {
      ::SetFocus(last_focus_window);
    }
  }
  return ret;
}


void WebPluginDelegateImpl::OnModalLoopEntered() {
  DCHECK(handle_event_pump_messages_event_ != NULL);
  SetEvent(handle_event_pump_messages_event_);

  base::MessageLoop::current()->SetNestableTasksAllowed(true);
  base::MessageLoop::current()->set_os_modal_loop(true);

  UnhookWindowsHookEx(handle_event_message_filter_hook_);
  handle_event_message_filter_hook_ = NULL;
}

bool WebPluginDelegateImpl::ShouldTrackEventForModalLoops(NPEvent* event) {
  if (event->event == WM_RBUTTONDOWN)
    return true;
  return false;
}

void WebPluginDelegateImpl::OnUserGestureEnd() {
  user_gesture_message_posted_ = false;
  instance()->PopPopupsEnabledState();
}

BOOL WINAPI WebPluginDelegateImpl::TrackPopupMenuPatch(
    HMENU menu, unsigned int flags, int x, int y, int reserved,
    HWND window, const RECT* rect) {

  if (g_current_plugin_instance) {
    unsigned long window_process_id = 0;
    unsigned long window_thread_id =
        GetWindowThreadProcessId(window, &window_process_id);
    // TrackPopupMenu fails if the window passed in belongs to a different
    // thread.
    if (::GetCurrentThreadId() != window_thread_id) {
      bool valid =
          GetParent(g_current_plugin_instance->dummy_window_for_activation_) !=
              GetDesktopWindow();
      if (valid) {
        window = g_current_plugin_instance->dummy_window_for_activation_;
      } else {
        NOTREACHED() << "Dummy window not parented";
      }
    }
  }

  BOOL result = TrackPopupMenu(menu, flags, x, y, reserved, window, rect);
  return result;
}

HCURSOR WINAPI WebPluginDelegateImpl::SetCursorPatch(HCURSOR cursor) {
  // The windowless flash plugin periodically calls SetCursor in a wndproc
  // instantiated on the plugin thread. This causes annoying cursor flicker
  // when the mouse is moved on a foreground tab, with a windowless plugin
  // instance in a background tab. We just ignore the call here.
  if (!g_current_plugin_instance) {
    HCURSOR current_cursor = GetCursor();
    if (current_cursor != cursor) {
      ::SetCursor(cursor);
    }
    return current_cursor;
  }
  return ::SetCursor(cursor);
}

LONG WINAPI WebPluginDelegateImpl::RegEnumKeyExWPatch(
    HKEY key, DWORD index, LPWSTR name, LPDWORD name_size, LPDWORD reserved,
    LPWSTR class_name, LPDWORD class_size, PFILETIME last_write_time) {
  DWORD orig_size = *name_size;
  LONG rv = RegEnumKeyExW(key, index, name, name_size, reserved, class_name,
                          class_size, last_write_time);
  if (rv == ERROR_SUCCESS &&
      GetKeyPath(key).find(L"Microsoft\\MediaPlayer\\ShimInclusionList") !=
          std::wstring::npos) {
    static const wchar_t kChromeExeName[] = L"chrome.exe";
    wcsncpy_s(name, orig_size, kChromeExeName, arraysize(kChromeExeName));
    *name_size =
        std::min(orig_size, static_cast<DWORD>(arraysize(kChromeExeName)));
  }

  return rv;
}

void WebPluginDelegateImpl::ImeCompositionUpdated(
    const base::string16& text,
    const std::vector<int>& clauses,
    const std::vector<int>& target,
    int cursor_position) {
  if (!plugin_ime_)
    plugin_ime_.reset(new WebPluginIMEWin);

  plugin_ime_->CompositionUpdated(text, clauses, target, cursor_position);
  plugin_ime_->SendEvents(instance());
}

void WebPluginDelegateImpl::ImeCompositionCompleted(
    const base::string16& text) {
  if (!plugin_ime_)
    plugin_ime_.reset(new WebPluginIMEWin);
  plugin_ime_->CompositionCompleted(text);
  plugin_ime_->SendEvents(instance());
}

bool WebPluginDelegateImpl::GetIMEStatus(int* input_type,
                                         gfx::Rect* caret_rect) {
  if (!plugin_ime_)
    return false;
  return plugin_ime_->GetStatus(input_type, caret_rect);
}

// static
FARPROC WINAPI WebPluginDelegateImpl::GetProcAddressPatch(HMODULE module,
                                                          LPCSTR name) {
  FARPROC imm_function = WebPluginIMEWin::GetProcAddress(name);
  if (imm_function)
    return imm_function;
  return ::GetProcAddress(module, name);
}

HWND WINAPI WebPluginDelegateImpl::WindowFromPointPatch(POINT point) {
  HWND window = WindowFromPoint(point);
  if (::ScreenToClient(window, &point)) {
    HWND child = ChildWindowFromPoint(window, point);
    if (::IsWindow(child) &&
        ::GetProp(child, content::kPluginDummyParentProperty))
      return child;
  }
  return window;
}

void WebPluginDelegateImpl::HandleCaptureForMessage(HWND window,
                                                    UINT message) {
  if (gfx::GetClassName(window) != base::string16(kNativeWindowClassName))
    return;

  switch (message) {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
      ::SetCapture(window);
      // As per documentation the WM_PARENTNOTIFY message is sent to the parent
      // window chain if mouse input is received by the child window. However
      // the parent receives the WM_PARENTNOTIFY message only if we doubleclick
      // on the window. We send the WM_PARENTNOTIFY message for mouse input
      // messages to the parent to indicate that user action is expected.
      ::SendMessage(::GetParent(window), WM_PARENTNOTIFY, message, 0);
      break;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
      ::ReleaseCapture();
      break;

    default:
      break;
  }
}

}  // namespace content
