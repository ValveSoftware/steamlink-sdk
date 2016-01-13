// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_WEBPLUGIN_DELEGATE_IMPL_H_
#define CONTENT_CHILD_NPAPI_WEBPLUGIN_DELEGATE_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/child/npapi/webplugin_delegate.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/npapi/bindings/npapi.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace base {
class FilePath;
}

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class CALayer;
@class CARenderer;
#else
class CALayer;
class CARenderer;
#endif
#endif

namespace content {
class PluginInstance;
class WebPlugin;

#if defined(OS_MACOSX)
class WebPluginAcceleratedSurface;
class ExternalDragTracker;
#endif  // OS_MACOSX

#if defined(OS_WIN)
class WebPluginIMEWin;
#endif  // OS_WIN

// An implementation of WebPluginDelegate that runs in the plugin process,
// proxied from the renderer by WebPluginDelegateProxy.
class WebPluginDelegateImpl : public WebPluginDelegate {
 public:
  enum PluginQuirks {
    PLUGIN_QUIRK_SETWINDOW_TWICE = 1,  // Win32
    PLUGIN_QUIRK_THROTTLE_WM_USER_PLUS_ONE = 2,  // Win32
    PLUGIN_QUIRK_DONT_CALL_WND_PROC_RECURSIVELY = 4,  // Win32
    PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY = 8,  // Win32
    PLUGIN_QUIRK_DONT_ALLOW_MULTIPLE_INSTANCES = 16,  // Win32
    PLUGIN_QUIRK_DIE_AFTER_UNLOAD = 32,  // Win32
    PLUGIN_QUIRK_PATCH_SETCURSOR = 64,  // Win32
    PLUGIN_QUIRK_BLOCK_NONSTANDARD_GETURL_REQUESTS = 128,  // Win32
    PLUGIN_QUIRK_WINDOWLESS_OFFSET_WINDOW_TO_DRAW = 256,  // Linux
    PLUGIN_QUIRK_WINDOWLESS_INVALIDATE_AFTER_SET_WINDOW = 512,  // Linux
    PLUGIN_QUIRK_NO_WINDOWLESS = 1024,  // Windows
    PLUGIN_QUIRK_PATCH_REGENUMKEYEXW = 2048,  // Windows
    PLUGIN_QUIRK_ALWAYS_NOTIFY_SUCCESS = 4096,  // Windows
    PLUGIN_QUIRK_HANDLE_MOUSE_CAPTURE = 16384,  // Windows
    PLUGIN_QUIRK_WINDOWLESS_NO_RIGHT_CLICK = 32768,  // Linux
    PLUGIN_QUIRK_IGNORE_FIRST_SETWINDOW_CALL = 65536,  // Windows.
    PLUGIN_QUIRK_EMULATE_IME = 131072,  // Windows.
    PLUGIN_QUIRK_FAKE_WINDOW_FROM_POINT = 262144,  // Windows.
    PLUGIN_QUIRK_COPY_STREAM_DATA = 524288,  // All platforms
  };

  static WebPluginDelegateImpl* Create(WebPlugin* plugin,
                                       const base::FilePath& filename,
                                       const std::string& mime_type);

  // WebPluginDelegate implementation
  virtual bool Initialize(const GURL& url,
                          const std::vector<std::string>& arg_names,
                          const std::vector<std::string>& arg_values,
                          bool load_manually) OVERRIDE;
  virtual void PluginDestroyed() OVERRIDE;
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect) OVERRIDE;
  virtual void Paint(SkCanvas* canvas, const gfx::Rect& rect) OVERRIDE;
  virtual void SetFocus(bool focused) OVERRIDE;
  virtual bool HandleInputEvent(const blink::WebInputEvent& event,
                                WebCursor::CursorInfo* cursor_info) OVERRIDE;
  virtual NPObject* GetPluginScriptableObject() OVERRIDE;
  virtual NPP GetPluginNPP() OVERRIDE;
  virtual bool GetFormValue(base::string16* value) OVERRIDE;
  virtual void DidFinishLoadWithReason(const GURL& url,
                                       NPReason reason,
                                       int notify_id) OVERRIDE;
  virtual int GetProcessId() OVERRIDE;
  virtual void SendJavaScriptStream(const GURL& url,
                                    const std::string& result,
                                    bool success,
                                    int notify_id) OVERRIDE;
  virtual void DidReceiveManualResponse(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified) OVERRIDE;
  virtual void DidReceiveManualData(const char* buffer, int length) OVERRIDE;
  virtual void DidFinishManualLoading() OVERRIDE;
  virtual void DidManualLoadFail() OVERRIDE;
  virtual WebPluginResourceClient* CreateResourceClient(
      unsigned long resource_id, const GURL& url, int notify_id) OVERRIDE;
  virtual WebPluginResourceClient* CreateSeekableResourceClient(
      unsigned long resource_id, int range_request_id) OVERRIDE;
  virtual void FetchURL(unsigned long resource_id,
                        int notify_id,
                        const GURL& url,
                        const GURL& first_party_for_cookies,
                        const std::string& method,
                        const char* buf,
                        unsigned int len,
                        const GURL& referrer,
                        bool notify_redirects,
                        bool is_plugin_src_load,
                        int origin_pid,
                        int render_frame_id,
                        int render_view_id) OVERRIDE;
  // End of WebPluginDelegate implementation.

  gfx::PluginWindowHandle windowed_handle() const { return windowed_handle_; }
  bool IsWindowless() const { return windowless_; }
  PluginInstance* instance() { return instance_.get(); }
  gfx::Rect GetRect() const { return window_rect_; }
  gfx::Rect GetClipRect() const { return clip_rect_; }

  // Returns the path for the library implementing this plugin.
  base::FilePath GetPluginPath();

  // Returns a combination of PluginQuirks.
  int GetQuirks() const { return quirks_; }

  // Informs the plugin that the view it is in has gained or lost focus.
  void SetContentAreaHasFocus(bool has_focus);

#if defined(OS_WIN)
  // Informs the plug-in that an IME has changed its status.
  void ImeCompositionUpdated(const base::string16& text,
                             const std::vector<int>& clauses,
                             const std::vector<int>& target,
                             int cursor_position);

  // Informs the plugin that IME composition has completed./ If |text| is empty,
  // IME was cancelled.
  void ImeCompositionCompleted(const base::string16& text);

  // Returns the IME status retrieved from a plug-in.
  bool GetIMEStatus(int* input_type, gfx::Rect* caret_rect);
#endif

#if defined(OS_MACOSX) && !defined(USE_AURA)
  // Informs the plugin that the geometry has changed, as with UpdateGeometry,
  // but also includes the new buffer context for that new geometry.
  void UpdateGeometryAndContext(const gfx::Rect& window_rect,
                                const gfx::Rect& clip_rect,
                                gfx::NativeDrawingContext context);
  // Informs the delegate that the plugin called NPN_Invalidate*. Used as a
  // trigger for Core Animation drawing.
  void PluginDidInvalidate();
  // Returns the delegate currently processing events.
  static WebPluginDelegateImpl* GetActiveDelegate();
  // Informs the plugin that the window it is in has gained or lost focus.
  void SetWindowHasFocus(bool has_focus);
  // Informs the plugin that its tab or window has been hidden or shown.
  void SetContainerVisibility(bool is_visible);
  // Informs the plugin that its containing window's frame has changed.
  // Frames are in screen coordinates.
  void WindowFrameChanged(const gfx::Rect& window_frame,
                          const gfx::Rect& view_frame);
  // Informs the plugin that IME composition has completed.
  // If |text| is empty, IME was cancelled.
  void ImeCompositionCompleted(const base::string16& text);
  // Informs the delegate that the plugin set a Cocoa NSCursor.
  void SetNSCursor(NSCursor* cursor);

  // Indicates that the windowless plugins will draw directly to the window
  // context instead of a buffer context.
  void SetNoBufferContext();

  // TODO(caryclark): This is a temporary workaround to allow the Darwin / Skia
  // port to share code with the Darwin / CG port. Later, this will be removed
  // and all callers will use the Paint defined above.
  void CGPaint(CGContextRef context, const gfx::Rect& rect);
#endif  // OS_MACOSX && !USE_AURA

 private:
  friend class base::DeleteHelper<WebPluginDelegateImpl>;
  friend class WebPluginDelegate;

  WebPluginDelegateImpl(WebPlugin* plugin, PluginInstance* instance);
  virtual ~WebPluginDelegateImpl();

  // Called by Initialize() for platform-specific initialization.
  // If this returns false, the plugin shouldn't be started--see Initialize().
  bool PlatformInitialize();

  // Called by DestroyInstance(), used for platform-specific destruction.
  void PlatformDestroyInstance();

  //--------------------------
  // used for windowed plugins
  void WindowedUpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  // Create the native window.
  // Returns true if the window is created (or already exists).
  // Returns false if unable to create the window.
  bool WindowedCreatePlugin();

  // Destroy the native window.
  void WindowedDestroyWindow();

  // Reposition the native window to be in sync with the given geometry.
  // Returns true if the native window has moved or been clipped differently.
  bool WindowedReposition(const gfx::Rect& window_rect,
                          const gfx::Rect& clip_rect);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowedSetWindow();

#if defined(OS_WIN)
  // Registers the window class for our window
  ATOM RegisterNativeWindowClass();

  // Our WndProc functions.
  static LRESULT CALLBACK WrapperWindowProc(
      HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK NativeWndProc(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK FlashWindowlessWndProc(
      HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK DummyWindowProc(
      HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  // Used for throttling Flash messages.
  static void ClearThrottleQueueForWindow(HWND window);
  static void OnThrottleMessage();
  static void ThrottleMessage(WNDPROC proc, HWND hwnd, UINT message,
      WPARAM wParam, LPARAM lParam);
#endif

  //----------------------------
  // used for windowless plugins
  void WindowlessUpdateGeometry(const gfx::Rect& window_rect,
                                const gfx::Rect& clip_rect);
  void WindowlessPaint(gfx::NativeDrawingContext hdc, const gfx::Rect& rect);

  // Tells the plugin about the current state of the window.
  // See NPAPI NPP_SetWindow for more information.
  void WindowlessSetWindow();

  // Informs the plugin that it has gained or lost keyboard focus (on the Mac,
  // this just means window first responder status).
  void SetPluginHasFocus(bool focused);

  // Handles the platform specific details of setting plugin focus. Returns
  // false if the platform cancelled the focus tranfer.
  bool PlatformSetPluginHasFocus(bool focused);

  //-----------------------------------------
  // used for windowed and windowless plugins

  // Does platform-specific event handling. Arguments and return are identical
  // to HandleInputEvent.
  bool PlatformHandleInputEvent(const blink::WebInputEvent& event,
                                WebCursor::CursorInfo* cursor_info);

  // Closes down and destroys our plugin instance.
  void DestroyInstance();


  // used for windowed plugins
  // Note: on Mac OS X, the only time the windowed handle is non-zero
  // is the case of accelerated rendering, which uses a fake window handle to
  // identify itself back to the browser. It still performs all of its
  // work offscreen.
  gfx::PluginWindowHandle windowed_handle_;
  gfx::Rect windowed_last_pos_;

  bool windowed_did_set_window_;

  // used by windowed and windowless plugins
  bool windowless_;

  WebPlugin* plugin_;
  scoped_refptr<PluginInstance> instance_;

#if defined(OS_WIN)
  // Original wndproc before we subclassed.
  WNDPROC plugin_wnd_proc_;

  // Used to throttle WM_USER+1 messages in Flash.
  uint32 last_message_;
  bool is_calling_wndproc;

  // An IME emulator used by a windowless plug-in to retrieve IME data through
  // IMM32 functions.
  scoped_ptr<WebPluginIMEWin> plugin_ime_;
#endif  // defined(OS_WIN)

  NPWindow window_;
  gfx::Rect window_rect_;
  gfx::Rect clip_rect_;
  int quirks_;

#if defined(OS_WIN)
  // Windowless plugins don't have keyboard focus causing issues with the
  // plugin not receiving keyboard events if the plugin enters a modal
  // loop like TrackPopupMenuEx or MessageBox, etc.
  // This is a basic issue with windows activation and focus arising due to
  // the fact that these windows are created by different threads. Activation
  // and focus are thread specific states, and if the browser has focus,
  // the plugin may not have focus.
  // To fix a majority of these activation issues we create a dummy visible
  // child window to which we set focus whenever the windowless plugin
  // receives a WM_LBUTTONDOWN/WM_RBUTTONDOWN message via NPP_HandleEvent.

  HWND dummy_window_for_activation_;
  HWND dummy_window_parent_;
  WNDPROC old_dummy_window_proc_;
  bool CreateDummyWindowForActivation();

  // Returns true if the event passed in needs to be tracked for a potential
  // modal loop.
  static bool ShouldTrackEventForModalLoops(NPEvent* event);

  // The message filter hook procedure, which tracks modal loops entered by
  // a plugin in the course of a NPP_HandleEvent call.
  static LRESULT CALLBACK HandleEventMessageFilterHook(int code, WPARAM wParam,
                                                       LPARAM lParam);

  // TrackPopupMenu interceptor. Parameters are the same as the Win32 function
  // TrackPopupMenu.
  static BOOL WINAPI TrackPopupMenuPatch(HMENU menu, unsigned int flags, int x,
                                         int y, int reserved, HWND window,
                                         const RECT* rect);

  // SetCursor interceptor for windowless plugins.
  static HCURSOR WINAPI SetCursorPatch(HCURSOR cursor);

  // RegEnumKeyExW interceptor.
  static LONG WINAPI RegEnumKeyExWPatch(
      HKEY key, DWORD index, LPWSTR name, LPDWORD name_size, LPDWORD reserved,
      LPWSTR class_name, LPDWORD class_size, PFILETIME last_write_time);

  // GetProcAddress intercepter for windowless plugins.
  static FARPROC WINAPI GetProcAddressPatch(HMODULE module, LPCSTR name);

  // WindowFromPoint patch for Flash windowless plugins. When flash receives
  // mouse move messages it calls the WindowFromPoint API to eventually convert
  // the mouse coordinates to screen. We need to return the dummy plugin parent
  // window for Aura to ensure that these conversions occur correctly.
  static HWND WINAPI WindowFromPointPatch(POINT point);

  // The mouse hook proc which handles mouse capture in windowed plugins.
  static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam,
                                        LPARAM lParam);

  // Calls SetCapture/ReleaseCapture based on the message type.
  static void HandleCaptureForMessage(HWND window, UINT message);

#elif defined(OS_MACOSX) && !defined(USE_AURA)
  // Sets window_rect_ to |rect|
  void SetPluginRect(const gfx::Rect& rect);
  // Sets content_area_origin to |origin|
  void SetContentAreaOrigin(const gfx::Point& origin);
  // Updates everything that depends on the plugin's absolute screen location.
  void PluginScreenLocationChanged();
  // Updates anything that depends on plugin visibility.
  void PluginVisibilityChanged();

  // Starts an IME session.
  void StartIme();

  // Informs the browser about the updated accelerated drawing surface.
  void UpdateAcceleratedSurface();

  // Uses a CARenderer to draw the plug-in's layer in our OpenGL surface.
  void DrawLayerInSurface();

  bool use_buffer_context_;
  CGContextRef buffer_context_;  // Weak ref.

  CALayer* layer_;  // Used for CA drawing mode. Weak, retained by plug-in.
  WebPluginAcceleratedSurface* surface_;  // Weak ref.
  CARenderer* renderer_;  // Renders layer_ to surface_.
  scoped_ptr<base::RepeatingTimer<WebPluginDelegateImpl> > redraw_timer_;

  // The upper-left corner of the web content area in screen coordinates,
  // relative to an upper-left (0,0).
  gfx::Point content_area_origin_;

  bool containing_window_has_focus_;
  bool initial_window_focus_;
  bool container_is_visible_;
  bool have_called_set_window_;

  gfx::Rect cached_clip_rect_;

  bool ime_enabled_;
  int keyup_ignore_count_;

  scoped_ptr<ExternalDragTracker> external_drag_tracker_;
#endif  // OS_MACOSX && !USE_AURA

  // Called by the message filter hook when the plugin enters a modal loop.
  void OnModalLoopEntered();

  // Returns true if the message passed in corresponds to a user gesture.
  static bool IsUserGesture(const blink::WebInputEvent& event);

  // The url with which the plugin was instantiated.
  std::string plugin_url_;

#if defined(OS_WIN)
  // Indicates the end of a user gesture period.
  void OnUserGestureEnd();

  // Handle to the message filter hook
  HHOOK handle_event_message_filter_hook_;

  // Event which is set when the plugin enters a modal loop in the course
  // of a NPP_HandleEvent call.
  HANDLE handle_event_pump_messages_event_;

  // This flag indicates whether we started tracking a user gesture message.
  bool user_gesture_message_posted_;

  // Runnable Method Factory used to invoke the OnUserGestureEnd method
  // asynchronously.
  base::WeakPtrFactory<WebPluginDelegateImpl> user_gesture_msg_factory_;

  // Handle to the mouse hook installed for certain windowed plugins like
  // flash.
  HHOOK mouse_hook_;
#endif

  // Holds the depth of the HandleEvent callstack.
  int handle_event_depth_;

  // Holds the current cursor set by the windowless plugin.
  WebCursor current_windowless_cursor_;

  // Set to true initially and indicates if this is the first npp_setwindow
  // call received by the plugin.
  bool first_set_window_call_;

  // True if the plugin thinks it has keyboard focus
  bool plugin_has_focus_;
  // True if the plugin element has focus within the web content, regardless of
  // whether its containing view currently has focus.
  bool has_webkit_focus_;
  // True if the containing view currently has focus.
  // Initially set to true so that plugin focus still works in environments
  // where SetContentAreaHasFocus is never called. See
  // https://bugs.webkit.org/show_bug.cgi?id=46013 for details.
  bool containing_view_has_focus_;

  // True if NPP_New did not return an error.
  bool creation_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(WebPluginDelegateImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_WEBPLUGIN_DELEGATE_IMPL_H_
