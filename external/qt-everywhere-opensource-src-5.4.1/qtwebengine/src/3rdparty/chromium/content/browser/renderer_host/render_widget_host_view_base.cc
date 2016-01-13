// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/size_f.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/win/wrapped_window_proc.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/plugin_service_impl.h"
#include "content/common/plugin_constants_win.h"
#include "content/common/webplugin_geometry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/win/dpi.h"
#include "ui/gfx/win/hwnd_util.h"
#endif

namespace content {

#if defined(OS_WIN)

namespace {

// |window| is the plugin HWND, created and destroyed in the plugin process.
// |parent| is the parent HWND, created and destroyed on the browser UI thread.
void NotifyPluginProcessHostHelper(HWND window, HWND parent, int tries) {
  // How long to wait between each try.
  static const int kTryDelayMs = 200;

  DWORD plugin_process_id;
  bool found_starting_plugin_process = false;
  GetWindowThreadProcessId(window, &plugin_process_id);
  for (PluginProcessHostIterator iter; !iter.Done(); ++iter) {
    if (!iter.GetData().handle) {
      found_starting_plugin_process = true;
      continue;
    }
    if (base::GetProcId(iter.GetData().handle) == plugin_process_id) {
      iter->AddWindow(parent);
      return;
    }
  }

  if (found_starting_plugin_process) {
    // A plugin process has started but we don't have its handle yet.  Since
    // it's most likely the one for this plugin, try a few more times after a
    // delay.
    if (tries > 0) {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&NotifyPluginProcessHostHelper, window, parent, tries - 1),
          base::TimeDelta::FromMilliseconds(kTryDelayMs));
      return;
    }
  }

  // The plugin process might have died in the time to execute the task, don't
  // leak the HWND.
  PostMessage(parent, WM_CLOSE, 0, 0);
}

// The plugin wrapper window which lives in the browser process has this proc
// as its window procedure. We only handle the WM_PARENTNOTIFY message sent by
// windowed plugins for mouse input. This is forwarded off to the wrappers
// parent which is typically the RVH window which turns on user gesture.
LRESULT CALLBACK PluginWrapperWindowProc(HWND window, unsigned int message,
                                         WPARAM wparam, LPARAM lparam) {
  if (message == WM_PARENTNOTIFY) {
    switch (LOWORD(wparam)) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
        ::SendMessage(GetParent(window), message, wparam, lparam);
        return 0;
      default:
        break;
    }
  }
  return ::DefWindowProc(window, message, wparam, lparam);
}

bool IsPluginWrapperWindow(HWND window) {
  return gfx::GetClassNameW(window) ==
      base::string16(kWrapperNativeWindowClassName);
}

// Create an intermediate window between the given HWND and its parent.
HWND ReparentWindow(HWND window, HWND parent) {
  static ATOM atom = 0;
  static HMODULE instance = NULL;
  if (!atom) {
    WNDCLASSEX window_class;
    base::win::InitializeWindowClass(
        kWrapperNativeWindowClassName,
        &base::win::WrappedWindowProc<PluginWrapperWindowProc>,
        CS_DBLCLKS,
        0,
        0,
        NULL,
        // xxx reinterpret_cast<HBRUSH>(COLOR_WINDOW+1),
        reinterpret_cast<HBRUSH>(COLOR_GRAYTEXT+1),
        NULL,
        NULL,
        NULL,
        &window_class);
    instance = window_class.hInstance;
    atom = RegisterClassEx(&window_class);
  }
  DCHECK(atom);

  HWND new_parent = CreateWindowEx(
      WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
      MAKEINTATOM(atom), 0,
      WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      0, 0, 0, 0, parent, 0, instance, 0);
  gfx::CheckWindowCreated(new_parent);
  ::SetParent(window, new_parent);
  // How many times we try to find a PluginProcessHost whose process matches
  // the HWND.
  static const int kMaxTries = 5;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&NotifyPluginProcessHostHelper, window, new_parent,
                 kMaxTries));
  return new_parent;
}

BOOL CALLBACK PaintEnumChildProc(HWND hwnd, LPARAM lparam) {
  if (!PluginServiceImpl::GetInstance()->IsPluginWindow(hwnd))
    return TRUE;

  gfx::Rect* rect = reinterpret_cast<gfx::Rect*>(lparam);
  gfx::Rect rect_in_pixels = gfx::win::DIPToScreenRect(*rect);
  static UINT msg = RegisterWindowMessage(kPaintMessageName);
  WPARAM wparam = MAKEWPARAM(rect_in_pixels.x(), rect_in_pixels.y());
  lparam = MAKELPARAM(rect_in_pixels.width(), rect_in_pixels.height());

  // SendMessage gets the message across much quicker than PostMessage, since it
  // doesn't get queued.  When the plugin thread calls PeekMessage or other
  // Win32 APIs, sent messages are dispatched automatically.
  SendNotifyMessage(hwnd, msg, wparam, lparam);

  return TRUE;
}

// Windows callback for OnDestroy to detach the plugin windows.
BOOL CALLBACK DetachPluginWindowsCallbackInternal(HWND window, LPARAM param) {
  RenderWidgetHostViewBase::DetachPluginWindowsCallback(window);
  return TRUE;
}

}  // namespace

// static
void RenderWidgetHostViewBase::DetachPluginWindowsCallback(HWND window) {
  if (PluginServiceImpl::GetInstance()->IsPluginWindow(window) &&
      !IsHungAppWindow(window)) {
    ::ShowWindow(window, SW_HIDE);
    SetParent(window, NULL);
  }
}

// static
void RenderWidgetHostViewBase::MovePluginWindowsHelper(
    HWND parent,
    const std::vector<WebPluginGeometry>& moves) {
  if (moves.empty())
    return;

  bool oop_plugins =
    !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);

  HDWP defer_window_pos_info =
      ::BeginDeferWindowPos(static_cast<int>(moves.size()));

  if (!defer_window_pos_info) {
    NOTREACHED();
    return;
  }

#if defined(USE_AURA)
  std::vector<RECT> invalidate_rects;
#endif

  for (size_t i = 0; i < moves.size(); ++i) {
    unsigned long flags = 0;
    const WebPluginGeometry& move = moves[i];
    HWND window = move.window;

    // As the plugin parent window which lives on the browser UI thread is
    // destroyed asynchronously, it is possible that we have a stale window
    // sent in by the renderer for moving around.
    // Note: get the parent before checking if the window is valid, to avoid a
    // race condition where the window is destroyed after the check but before
    // the GetParent call.
    HWND cur_parent = ::GetParent(window);
    if (!::IsWindow(window))
      continue;

    if (!PluginServiceImpl::GetInstance()->IsPluginWindow(window)) {
      // The renderer should only be trying to move plugin windows. However,
      // this may happen as a result of a race condition (i.e. even after the
      // check right above), so we ignore it.
      continue;
    }

    if (oop_plugins) {
      if (cur_parent == GetDesktopWindow()) {
        // The plugin window hasn't been parented yet, add an intermediate
        // window that lives on this thread to speed up scrolling. Note this
        // only works with out of process plugins since we depend on
        // PluginProcessHost to destroy the intermediate HWNDs.
        cur_parent = ReparentWindow(window, parent);
        ::ShowWindow(window, SW_SHOW);  // Window was created hidden.
      } else if (!IsPluginWrapperWindow(cur_parent)) {
        continue;  // Race if plugin process is shutting down.
      }

      // We move the intermediate parent window which doesn't result in cross-
      // process synchronous Windows messages.
      window = cur_parent;
    } else {
      if (cur_parent == GetDesktopWindow())
        SetParent(window, parent);
    }

    if (move.visible)
      flags |= SWP_SHOWWINDOW;
    else
      flags |= SWP_HIDEWINDOW;

#if defined(USE_AURA)
    if (GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
      // Without this flag, Windows repaints the parent area uncovered by this
      // move. However when software compositing is used the clipping region is
      // ignored. Since in Aura the browser chrome could be under the plugin, if
      // if Windows tries to paint it synchronously inside EndDeferWindowsPos
      // then it won't have the data and it will flash white. So instead we
      // manually redraw the plugin.
      // Why not do this for native Windows? Not sure if there are any
      // performance issues with this.
      flags |= SWP_NOREDRAW;
    }
#endif

    if (move.rects_valid) {
      gfx::Rect clip_rect_in_pixel = gfx::win::DIPToScreenRect(move.clip_rect);
      HRGN hrgn = ::CreateRectRgn(clip_rect_in_pixel.x(),
                                  clip_rect_in_pixel.y(),
                                  clip_rect_in_pixel.right(),
                                  clip_rect_in_pixel.bottom());
      gfx::SubtractRectanglesFromRegion(hrgn, move.cutout_rects);

      // Note: System will own the hrgn after we call SetWindowRgn,
      // so we don't need to call DeleteObject(hrgn)
      ::SetWindowRgn(window, hrgn,
                     !move.clip_rect.IsEmpty() && (flags & SWP_NOREDRAW) == 0);

#if defined(USE_AURA)
      // When using the software compositor, if the clipping rectangle is empty
      // then DeferWindowPos won't redraw the newly uncovered area under the
      // plugin.
      if (clip_rect_in_pixel.IsEmpty() &&
          !GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
        RECT r;
        GetClientRect(window, &r);
        MapWindowPoints(window, parent, reinterpret_cast<POINT*>(&r), 2);
        invalidate_rects.push_back(r);
      }
#endif
    } else {
      flags |= SWP_NOMOVE;
      flags |= SWP_NOSIZE;
    }

    gfx::Rect window_rect_in_pixel =
        gfx::win::DIPToScreenRect(move.window_rect);
    defer_window_pos_info = ::DeferWindowPos(defer_window_pos_info,
                                             window, NULL,
                                             window_rect_in_pixel.x(),
                                             window_rect_in_pixel.y(),
                                             window_rect_in_pixel.width(),
                                             window_rect_in_pixel.height(),
                                             flags);

    if (!defer_window_pos_info) {
      DCHECK(false) << "DeferWindowPos failed, so all plugin moves ignored.";
      return;
    }
  }

  ::EndDeferWindowPos(defer_window_pos_info);

#if defined(USE_AURA)
  if (GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
    for (size_t i = 0; i < moves.size(); ++i) {
      const WebPluginGeometry& move = moves[i];
      RECT r;
      GetWindowRect(move.window, &r);
      gfx::Rect gr(r);
      PaintEnumChildProc(move.window, reinterpret_cast<LPARAM>(&gr));
    }
  } else {
      for (size_t i = 0; i < invalidate_rects.size(); ++i) {
      ::RedrawWindow(
          parent, &invalidate_rects[i], NULL,
          // These flags are from WebPluginDelegateImpl::NativeWndProc.
          RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
    }
  }
#endif
}

// static
void RenderWidgetHostViewBase::PaintPluginWindowsHelper(
    HWND parent, const gfx::Rect& damaged_screen_rect) {
  LPARAM lparam = reinterpret_cast<LPARAM>(&damaged_screen_rect);
  EnumChildWindows(parent, PaintEnumChildProc, lparam);
}

// static
void RenderWidgetHostViewBase::DetachPluginsHelper(HWND parent) {
  // When a tab is closed all its child plugin windows are destroyed
  // automatically. This happens before plugins get any notification that its
  // instances are tearing down.
  //
  // Plugins like Quicktime assume that their windows will remain valid as long
  // as they have plugin instances active. Quicktime crashes in this case
  // because its windowing code cleans up an internal data structure that the
  // handler for NPP_DestroyStream relies on.
  //
  // The fix is to detach plugin windows from web contents when it is going
  // away. This will prevent the plugin windows from getting destroyed
  // automatically. The detached plugin windows will get cleaned up in proper
  // sequence as part of the usual cleanup when the plugin instance goes away.
  EnumChildWindows(parent, DetachPluginWindowsCallbackInternal, NULL);
}

#endif  // OS_WIN

namespace {

// How many microseconds apart input events should be flushed.
const int kFlushInputRateInUs = 16666;

}

RenderWidgetHostViewBase::RenderWidgetHostViewBase()
    : popup_type_(blink::WebPopupTypeNone),
      background_opaque_(true),
      mouse_locked_(false),
      showing_context_menu_(false),
      selection_text_offset_(0),
      selection_range_(gfx::Range::InvalidRange()),
      current_device_scale_factor_(0),
      current_display_rotation_(gfx::Display::ROTATE_0),
      pinch_zoom_enabled_(content::IsPinchToZoomEnabled()),
      renderer_frame_number_(0) {
}

RenderWidgetHostViewBase::~RenderWidgetHostViewBase() {
  DCHECK(!mouse_locked_);
}

bool RenderWidgetHostViewBase::OnMessageReceived(const IPC::Message& msg){
  return false;
}

void RenderWidgetHostViewBase::SetBackgroundOpaque(bool opaque) {
  background_opaque_ = opaque;
}

bool RenderWidgetHostViewBase::GetBackgroundOpaque() {
  return background_opaque_;
}

gfx::Size RenderWidgetHostViewBase::GetPhysicalBackingSize() const {
  gfx::NativeView view = GetNativeView();
  gfx::Display display =
      gfx::Screen::GetScreenFor(view)->GetDisplayNearestWindow(view);
  return gfx::ToCeiledSize(gfx::ScaleSize(GetRequestedRendererSize(),
                                          display.device_scale_factor()));
}

float RenderWidgetHostViewBase::GetOverdrawBottomHeight() const {
  return 0.f;
}

void RenderWidgetHostViewBase::SelectionChanged(const base::string16& text,
                                                size_t offset,
                                                const gfx::Range& range) {
  selection_text_ = text;
  selection_text_offset_ = offset;
  selection_range_.set_start(range.start());
  selection_range_.set_end(range.end());
}

gfx::Size RenderWidgetHostViewBase::GetRequestedRendererSize() const {
  return GetViewBounds().size();
}

ui::TextInputClient* RenderWidgetHostViewBase::GetTextInputClient() {
  NOTREACHED();
  return NULL;
}

bool RenderWidgetHostViewBase::IsShowingContextMenu() const {
  return showing_context_menu_;
}

void RenderWidgetHostViewBase::SetShowingContextMenu(bool showing) {
  DCHECK_NE(showing_context_menu_, showing);
  showing_context_menu_ = showing;
}

base::string16 RenderWidgetHostViewBase::GetSelectedText() const {
  if (!selection_range_.IsValid())
    return base::string16();
  return selection_text_.substr(
      selection_range_.GetMin() - selection_text_offset_,
      selection_range_.length());
}

bool RenderWidgetHostViewBase::IsMouseLocked() {
  return mouse_locked_;
}

InputEventAckState RenderWidgetHostViewBase::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  // By default, input events are simply forwarded to the renderer.
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostViewBase::OnDidFlushInput() {
  // The notification can safely be ignored by most implementations.
}

void RenderWidgetHostViewBase::OnSetNeedsFlushInput() {
  if (flush_input_timer_.IsRunning())
    return;

  flush_input_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(kFlushInputRateInUs),
      this,
      &RenderWidgetHostViewBase::FlushInput);
}

void RenderWidgetHostViewBase::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    InputEventAckState ack_result) {
}

void RenderWidgetHostViewBase::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
}

void RenderWidgetHostViewBase::SetPopupType(blink::WebPopupType popup_type) {
  popup_type_ = popup_type;
}

blink::WebPopupType RenderWidgetHostViewBase::GetPopupType() {
  return popup_type_;
}

BrowserAccessibilityManager*
    RenderWidgetHostViewBase::GetBrowserAccessibilityManager() const {
  return browser_accessibility_manager_.get();
}

void RenderWidgetHostViewBase::CreateBrowserAccessibilityManagerIfNeeded() {
}

void RenderWidgetHostViewBase::SetBrowserAccessibilityManager(
    BrowserAccessibilityManager* manager) {
  browser_accessibility_manager_.reset(manager);
}

void RenderWidgetHostViewBase::OnAccessibilitySetFocus(int acc_obj_id) {
}

void RenderWidgetHostViewBase::AccessibilityShowMenu(int acc_obj_id) {
}

gfx::Point RenderWidgetHostViewBase::AccessibilityOriginInScreen(
    const gfx::Rect& bounds) {
  return bounds.origin();
}

void RenderWidgetHostViewBase::UpdateScreenInfo(gfx::NativeView view) {
  RenderWidgetHostImpl* impl = NULL;
  if (GetRenderWidgetHost())
    impl = RenderWidgetHostImpl::From(GetRenderWidgetHost());

  if (impl)
    impl->SendScreenRects();

  if (HasDisplayPropertyChanged(view) && impl)
    impl->NotifyScreenInfoChanged();
}

bool RenderWidgetHostViewBase::HasDisplayPropertyChanged(gfx::NativeView view) {
  gfx::Display display =
      gfx::Screen::GetScreenFor(view)->GetDisplayNearestWindow(view);
  if (current_display_area_ == display.work_area() &&
      current_device_scale_factor_ == display.device_scale_factor() &&
      current_display_rotation_ == display.rotation()) {
    return false;
  }

  current_display_area_ = display.work_area();
  current_device_scale_factor_ = display.device_scale_factor();
  current_display_rotation_ = display.rotation();
  return true;
}

scoped_ptr<SyntheticGestureTarget>
RenderWidgetHostViewBase::CreateSyntheticGestureTarget() {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  return scoped_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetBase(host));
}

// Platform implementation should override this method to allow frame
// subscription. Frame subscriber is set to RenderProcessHost, which is
// platform independent. It should be set to the specific presenter on each
// platform.
bool RenderWidgetHostViewBase::CanSubscribeFrame() const {
  NOTIMPLEMENTED();
  return false;
}

// Base implementation for this method sets the subscriber to RenderProcessHost,
// which is platform independent. Note: Implementation only support subscribing
// to accelerated composited frames.
void RenderWidgetHostViewBase::BeginFrameSubscription(
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  RenderWidgetHostImpl* impl = NULL;
  if (GetRenderWidgetHost())
    impl = RenderWidgetHostImpl::From(GetRenderWidgetHost());
  if (!impl)
    return;
  RenderProcessHostImpl* render_process_host =
      static_cast<RenderProcessHostImpl*>(impl->GetProcess());
  render_process_host->BeginFrameSubscription(impl->GetRoutingID(),
                                              subscriber.Pass());
}

void RenderWidgetHostViewBase::EndFrameSubscription() {
  RenderWidgetHostImpl* impl = NULL;
  if (GetRenderWidgetHost())
    impl = RenderWidgetHostImpl::From(GetRenderWidgetHost());
  if (!impl)
    return;
  RenderProcessHostImpl* render_process_host =
      static_cast<RenderProcessHostImpl*>(impl->GetProcess());
  render_process_host->EndFrameSubscription(impl->GetRoutingID());
}

uint32 RenderWidgetHostViewBase::RendererFrameNumber() {
  return renderer_frame_number_;
}

void RenderWidgetHostViewBase::DidReceiveRendererFrame() {
  ++renderer_frame_number_;
}

void RenderWidgetHostViewBase::FlushInput() {
  RenderWidgetHostImpl* impl = NULL;
  if (GetRenderWidgetHost())
    impl = RenderWidgetHostImpl::From(GetRenderWidgetHost());
  if (!impl)
    return;
  impl->FlushInput();
}

SkBitmap::Config RenderWidgetHostViewBase::PreferredReadbackFormat() {
  return SkBitmap::kARGB_8888_Config;
}

gfx::Size RenderWidgetHostViewBase::GetVisibleViewportSize() const {
  return GetViewBounds().size();
}

void RenderWidgetHostViewBase::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
}

}  // namespace content
