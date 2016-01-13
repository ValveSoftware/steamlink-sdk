// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/webplugin_delegate_impl.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <unistd.h>

#include <set>
#include <string>

#include "base/mac/mac_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/stats_counters.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_lib.h"
#include "content/child/npapi/plugin_web_event_converter_mac.h"
#include "content/child/npapi/webplugin.h"
#include "content/child/npapi/webplugin_accelerated_surface_mac.h"
#include "content/common/cursors/webcursor.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

// Important implementation notes: The Mac definition of NPAPI, particularly
// the distinction between windowed and windowless modes, differs from the
// Windows and Linux definitions.  Most of those differences are
// accomodated by the WebPluginDelegate class.

namespace content {

namespace {

const int kCoreAnimationRedrawPeriodMs = 10;  // 100 Hz

WebPluginDelegateImpl* g_active_delegate;

// Helper to simplify correct usage of g_active_delegate.  Instantiating will
// set the active delegate to |delegate| for the lifetime of the object, then
// NULL when it goes out of scope.
class ScopedActiveDelegate {
 public:
  explicit ScopedActiveDelegate(WebPluginDelegateImpl* delegate) {
    g_active_delegate = delegate;
  }
  ~ScopedActiveDelegate() {
    g_active_delegate = NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedActiveDelegate);
};

}  // namespace

// Helper to build and maintain a model of a drag entering the plugin but not
// starting there. See explanation in PlatformHandleInputEvent.
class ExternalDragTracker {
 public:
  ExternalDragTracker() : pressed_buttons_(0) {}

  // Returns true if an external drag is in progress.
  bool IsDragInProgress() { return pressed_buttons_ != 0; };

  // Returns true if the given event appears to be related to an external drag.
  bool EventIsRelatedToDrag(const WebInputEvent& event);

  // Updates the tracking of whether an external drag is in progress--and if
  // so what buttons it involves--based on the given event.
  void UpdateDragStateFromEvent(const WebInputEvent& event);

 private:
  // Returns the mask for just the button state in a WebInputEvent's modifiers.
  static int WebEventButtonModifierMask();

  // The WebInputEvent modifier flags for any buttons that were down when an
  // external drag entered the plugin, and which and are still down now.
  int pressed_buttons_;

  DISALLOW_COPY_AND_ASSIGN(ExternalDragTracker);
};

void ExternalDragTracker::UpdateDragStateFromEvent(const WebInputEvent& event) {
  switch (event.type) {
    case WebInputEvent::MouseEnter:
      pressed_buttons_ = event.modifiers & WebEventButtonModifierMask();
      break;
    case WebInputEvent::MouseUp: {
      const WebMouseEvent* mouse_event =
          static_cast<const WebMouseEvent*>(&event);
      if (mouse_event->button == WebMouseEvent::ButtonLeft)
        pressed_buttons_ &= ~WebInputEvent::LeftButtonDown;
      if (mouse_event->button == WebMouseEvent::ButtonMiddle)
        pressed_buttons_ &= ~WebInputEvent::MiddleButtonDown;
      if (mouse_event->button == WebMouseEvent::ButtonRight)
        pressed_buttons_ &= ~WebInputEvent::RightButtonDown;
      break;
    }
    default:
      break;
  }
}

bool ExternalDragTracker::EventIsRelatedToDrag(const WebInputEvent& event) {
  const WebMouseEvent* mouse_event = static_cast<const WebMouseEvent*>(&event);
  switch (event.type) {
    case WebInputEvent::MouseUp:
      // We only care about release of buttons that were part of the drag.
      return ((mouse_event->button == WebMouseEvent::ButtonLeft &&
               (pressed_buttons_ & WebInputEvent::LeftButtonDown)) ||
              (mouse_event->button == WebMouseEvent::ButtonMiddle &&
               (pressed_buttons_ & WebInputEvent::MiddleButtonDown)) ||
              (mouse_event->button == WebMouseEvent::ButtonRight &&
               (pressed_buttons_ & WebInputEvent::RightButtonDown)));
    case WebInputEvent::MouseEnter:
      return (event.modifiers & WebEventButtonModifierMask()) != 0;
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseMove: {
      int event_buttons = (event.modifiers & WebEventButtonModifierMask());
      return (pressed_buttons_ &&
              pressed_buttons_ == event_buttons);
    }
    default:
      return false;
  }
  return false;
}

int ExternalDragTracker::WebEventButtonModifierMask() {
  return WebInputEvent::LeftButtonDown |
         WebInputEvent::RightButtonDown |
         WebInputEvent::MiddleButtonDown;
}

#pragma mark -
#pragma mark Core WebPluginDelegate implementation

WebPluginDelegateImpl::WebPluginDelegateImpl(
    WebPlugin* plugin,
    PluginInstance* instance)
    : windowed_handle_(gfx::kNullPluginWindow),
      // all Mac plugins are "windowless" in the Windows/X11 sense
      windowless_(true),
      plugin_(plugin),
      instance_(instance),
      quirks_(0),
      use_buffer_context_(true),
      buffer_context_(NULL),
      layer_(nil),
      surface_(NULL),
      renderer_(nil),
      containing_window_has_focus_(false),
      initial_window_focus_(false),
      container_is_visible_(false),
      have_called_set_window_(false),
      ime_enabled_(false),
      keyup_ignore_count_(0),
      external_drag_tracker_(new ExternalDragTracker()),
      handle_event_depth_(0),
      first_set_window_call_(true),
      plugin_has_focus_(false),
      has_webkit_focus_(false),
      containing_view_has_focus_(true),
      creation_succeeded_(false) {
  memset(&window_, 0, sizeof(window_));
  instance->set_windowless(true);
}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  DestroyInstance();
}

bool WebPluginDelegateImpl::PlatformInitialize() {
  // Don't set a NULL window handle on destroy for Mac plugins.  This matches
  // Safari and other Mac browsers (see PluginView::stop() in PluginView.cpp,
  // where code to do so is surrounded by an #ifdef that excludes Mac OS X, or
  // destroyPlugin in WebNetscapePluginView.mm, for examples).
  quirks_ |= PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY;

  // Mac plugins don't expect to be unloaded, and they don't always do so
  // cleanly, so don't unload them at shutdown.
  instance()->plugin_lib()->PreventLibraryUnload();

#ifndef NP_NO_CARBON
  if (instance()->event_model() == NPEventModelCarbon)
    return false;
#endif

  window_.type = NPWindowTypeDrawable;
  NPDrawingModel drawing_model = instance()->drawing_model();
  switch (drawing_model) {
#ifndef NP_NO_QUICKDRAW
    case NPDrawingModelQuickDraw:
      return false;
#endif
    case NPDrawingModelCoreGraphics:
      break;
    case NPDrawingModelCoreAnimation:
    case NPDrawingModelInvalidatingCoreAnimation: {
      // Ask the plug-in for the CALayer it created for rendering content.
      // Create a surface to host it, and request a "window" handle to identify
      // the surface.
      CALayer* layer = nil;
      NPError err = instance()->NPP_GetValue(NPPVpluginCoreAnimationLayer,
                                             reinterpret_cast<void*>(&layer));
      if (!err) {
        if (drawing_model == NPDrawingModelCoreAnimation) {
          // Create the timer; it will be started when we get a window handle.
          redraw_timer_.reset(new base::RepeatingTimer<WebPluginDelegateImpl>);
        }
        layer_ = layer;

        gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;
        // On dual GPU systems, force the use of the discrete GPU for
        // the CARenderer underlying our Core Animation backend for
        // all plugins except Flash. For some reason Unity3D's output
        // doesn't show up if the integrated GPU is used. Safari keeps
        // even Flash 11 with Stage3D on the integrated GPU, so mirror
        // that behavior here.
        const WebPluginInfo& plugin_info =
            instance_->plugin_lib()->plugin_info();
        if (plugin_info.name.find(base::ASCIIToUTF16("Flash")) !=
            base::string16::npos)
          gpu_preference = gfx::PreferIntegratedGpu;
        surface_ = plugin_->GetAcceleratedSurface(gpu_preference);

        // If surface initialization fails for some reason, just continue
        // without any drawing; returning false would be a more confusing user
        // experience (since it triggers a missing plugin placeholder).
        if (surface_ && surface_->context()) {
          renderer_ = [[CARenderer rendererWithCGLContext:surface_->context()
                                                  options:NULL] retain];
          [renderer_ setLayer:layer_];
          plugin_->AcceleratedPluginEnabledRendering();
        }
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  // Let the WebPlugin know that we are windowless, unless this is a Core
  // Animation plugin, in which case AcceleratedPluginEnabledRendering
  // calls SetWindow. Rendering breaks if SetWindow is called before
  // accelerated rendering is enabled.
  if (!layer_)
    plugin_->SetWindow(gfx::kNullPluginWindow);

  return true;
}

void WebPluginDelegateImpl::PlatformDestroyInstance() {
  if (redraw_timer_)
    redraw_timer_->Stop();
  [renderer_ release];
  renderer_ = nil;
  layer_ = nil;
}

void WebPluginDelegateImpl::UpdateGeometryAndContext(
    const gfx::Rect& window_rect, const gfx::Rect& clip_rect,
    CGContextRef context) {
  buffer_context_ = context;
  UpdateGeometry(window_rect, clip_rect);
}

void WebPluginDelegateImpl::Paint(SkCanvas* canvas, const gfx::Rect& rect) {
  gfx::SkiaBitLocker bit_locker(canvas);
  CGContextRef context = bit_locker.cgContext();
  CGPaint(context, rect);
}

void WebPluginDelegateImpl::CGPaint(CGContextRef context,
                                    const gfx::Rect& rect) {
  WindowlessPaint(context, rect);
}

bool WebPluginDelegateImpl::PlatformHandleInputEvent(
    const WebInputEvent& event, WebCursor::CursorInfo* cursor_info) {
  DCHECK(cursor_info != NULL);

  // If an event comes in before the plugin has been set up, bail.
  if (!have_called_set_window_)
    return false;

  // WebKit sometimes sends spurious mouse move events when the window doesn't
  // have focus; Cocoa event model plugins don't expect to receive mouse move
  // events when they are in a background window, so drop those events.
  if (!containing_window_has_focus_ &&
      (event.type == WebInputEvent::MouseMove ||
       event.type == WebInputEvent::MouseEnter ||
       event.type == WebInputEvent::MouseLeave)) {
    return false;
  }

  if (WebInputEvent::isMouseEventType(event.type) ||
      event.type == WebInputEvent::MouseWheel) {
    // Check our plugin location before we send the event to the plugin, just
    // in case we somehow missed a plugin frame change.
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(&event);
    gfx::Point content_origin(
        mouse_event->globalX - mouse_event->x - window_rect_.x(),
        mouse_event->globalY - mouse_event->y - window_rect_.y());
    if (content_origin.x() != content_area_origin_.x() ||
        content_origin.y() != content_area_origin_.y()) {
      DLOG(WARNING) << "Stale plugin content area location: "
                    << content_area_origin_.ToString() << " instead of "
                    << content_origin.ToString();
      SetContentAreaOrigin(content_origin);
    }

    current_windowless_cursor_.GetCursorInfo(cursor_info);
  }

  // Per the Cocoa Plugin IME spec, plugins shoudn't receive keydown or keyup
  // events while composition is in progress. Treat them as handled, however,
  // since IME is consuming them on behalf of the plugin.
  if ((event.type == WebInputEvent::KeyDown && ime_enabled_) ||
      (event.type == WebInputEvent::KeyUp && keyup_ignore_count_)) {
    // Composition ends on a keydown, so ime_enabled_ will be false at keyup;
    // because the keydown wasn't sent to the plugin, the keyup shouldn't be
    // either (per the spec).
    if (event.type == WebInputEvent::KeyDown)
      ++keyup_ignore_count_;
    else
      --keyup_ignore_count_;
    return true;
  }

  ScopedActiveDelegate active_delegate(this);

  // Create the plugin event structure.
  scoped_ptr<PluginWebEventConverter> event_converter(
      new PluginWebEventConverter);
  if (!event_converter->InitWithEvent(event)) {
    // Silently consume any keyboard event types that aren't handled, so that
    // they don't fall through to the page.
    if (WebInputEvent::isKeyboardEventType(event.type))
      return true;
    return false;
  }
  NPCocoaEvent* plugin_event = event_converter->plugin_event();

  // The plugin host receives events related to drags starting outside the
  // plugin, but the NPAPI Cocoa event model spec says plugins shouldn't receive
  // them, so filter them out.
  // If WebKit adds a page capture mode (like the plugin capture mode that
  // handles drags starting inside) this can be removed.
  bool drag_related = external_drag_tracker_->EventIsRelatedToDrag(event);
  external_drag_tracker_->UpdateDragStateFromEvent(event);
  if (drag_related) {
    if (event.type == WebInputEvent::MouseUp &&
        !external_drag_tracker_->IsDragInProgress()) {
      // When an external drag ends, we need to synthesize a MouseEntered.
      NPCocoaEvent enter_event = *plugin_event;
      enter_event.type = NPCocoaEventMouseEntered;
      ScopedCurrentPluginEvent event_scope(instance(), &enter_event);
      instance()->NPP_HandleEvent(&enter_event);
    }
    return false;
  }

  // Send the plugin the event.
  scoped_ptr<ScopedCurrentPluginEvent> event_scope(
      new ScopedCurrentPluginEvent(instance(), plugin_event));
  int16_t handle_response = instance()->NPP_HandleEvent(plugin_event);
  bool handled = handle_response != kNPEventNotHandled;

  // Start IME if requested by the plugin.
  if (handled && handle_response == kNPEventStartIME &&
      event.type == WebInputEvent::KeyDown) {
    StartIme();
    ++keyup_ignore_count_;
  }

  // Plugins don't give accurate information about whether or not they handled
  // events, so browsers on the Mac ignore the return value.
  // Scroll events are the exception, since the Cocoa spec defines a meaning
  // for the return value.
  if (WebInputEvent::isMouseEventType(event.type)) {
    handled = true;
  } else if (WebInputEvent::isKeyboardEventType(event.type)) {
    // For Command-key events, trust the return value since eating all menu
    // shortcuts is not ideal.
    // TODO(stuartmorgan): Implement the advanced key handling spec, and trust
    // trust the key event return value from plugins that implement it.
    if (!(event.modifiers & WebInputEvent::MetaKey))
      handled = true;
  }

  return handled;
}

#pragma mark -

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  gfx::Rect old_clip_rect = clip_rect_;
  cached_clip_rect_ = clip_rect;
  if (container_is_visible_)  // Remove check when cached_clip_rect_ is removed.
    clip_rect_ = clip_rect;
  bool clip_rect_changed = (clip_rect_ != old_clip_rect);
  bool window_size_changed = (window_rect.size() != window_rect_.size());

  if (window_rect == window_rect_ && !clip_rect_changed)
    return;

  if (old_clip_rect.IsEmpty() != clip_rect_.IsEmpty()) {
    PluginVisibilityChanged();
  }

  SetPluginRect(window_rect);

  if (window_size_changed || clip_rect_changed)
    WindowlessSetWindow();
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext context,
                                            const gfx::Rect& damage_rect) {
  // If we get a paint event before we are completely set up (e.g., a nested
  // call while the plugin is still in NPP_SetWindow), bail.
  if (!have_called_set_window_ || (use_buffer_context_ && !buffer_context_))
    return;
  DCHECK(!use_buffer_context_ || buffer_context_ == context);

  base::StatsRate plugin_paint("Plugin.Paint");
  base::StatsScope<base::StatsRate> scope(plugin_paint);

  gfx::Rect paint_rect = damage_rect;
  if (use_buffer_context_) {
    // Plugin invalidates trigger asynchronous paints with the original
    // invalidation rect; the plugin may be resized before the paint is handled,
    // so we need to ensure that the damage rect is still sane.
    paint_rect.Intersect(
        gfx::Rect(0, 0, window_rect_.width(), window_rect_.height()));
  } else {
    // Use the actual window region when drawing directly to the window context.
    paint_rect.Intersect(window_rect_);
  }

  ScopedActiveDelegate active_delegate(this);

  CGContextSaveGState(context);

  if (!use_buffer_context_) {
    // Reposition the context origin so that plugins will draw at the correct
    // location in the window.
    CGContextClipToRect(context, paint_rect.ToCGRect());
    CGContextTranslateCTM(context, window_rect_.x(), window_rect_.y());
  }

  NPCocoaEvent paint_event;
  memset(&paint_event, 0, sizeof(NPCocoaEvent));
  paint_event.type = NPCocoaEventDrawRect;
  paint_event.data.draw.context = context;
  paint_event.data.draw.x = paint_rect.x();
  paint_event.data.draw.y = paint_rect.y();
  paint_event.data.draw.width = paint_rect.width();
  paint_event.data.draw.height = paint_rect.height();
  instance()->NPP_HandleEvent(&paint_event);

  if (use_buffer_context_) {
    // The backing buffer can change during the call to NPP_HandleEvent, in
    // which case the old context is (or is about to be) invalid.
    if (context == buffer_context_)
      CGContextRestoreGState(context);
  } else {
    // Always restore the context to the saved state.
    CGContextRestoreGState(context);
  }
}

void WebPluginDelegateImpl::WindowlessSetWindow() {
  if (!instance())
    return;

  window_.x = 0;
  window_.y = 0;
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();

  NPError err = instance()->NPP_SetWindow(&window_);

  // Send an appropriate window focus event after the first SetWindow.
  if (!have_called_set_window_) {
    have_called_set_window_ = true;
    SetWindowHasFocus(initial_window_focus_);
  }

  DCHECK(err == NPERR_NO_ERROR);
}

#pragma mark -

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  NOTREACHED();
  return false;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  NOTREACHED();
}

bool WebPluginDelegateImpl::WindowedReposition(const gfx::Rect& window_rect,
                                               const gfx::Rect& clip_rect) {
  NOTREACHED();
  return false;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  NOTREACHED();
}

#pragma mark -
#pragma mark Mac Extensions

void WebPluginDelegateImpl::PluginDidInvalidate() {
  if (instance()->drawing_model() == NPDrawingModelInvalidatingCoreAnimation)
    DrawLayerInSurface();
}

WebPluginDelegateImpl* WebPluginDelegateImpl::GetActiveDelegate() {
  return g_active_delegate;
}

void WebPluginDelegateImpl::SetWindowHasFocus(bool has_focus) {
  // If we get a window focus event before calling SetWindow, just remember the
  // states (WindowlessSetWindow will then send it on the first call).
  if (!have_called_set_window_) {
    initial_window_focus_ = has_focus;
    return;
  }

  if (has_focus == containing_window_has_focus_)
    return;
  containing_window_has_focus_ = has_focus;

  ScopedActiveDelegate active_delegate(this);
  NPCocoaEvent focus_event;
  memset(&focus_event, 0, sizeof(focus_event));
  focus_event.type = NPCocoaEventWindowFocusChanged;
  focus_event.data.focus.hasFocus = has_focus;
  instance()->NPP_HandleEvent(&focus_event);
}

bool WebPluginDelegateImpl::PlatformSetPluginHasFocus(bool focused) {
  if (!have_called_set_window_)
    return false;

  plugin_->FocusChanged(focused);

  ScopedActiveDelegate active_delegate(this);

  NPCocoaEvent focus_event;
  memset(&focus_event, 0, sizeof(focus_event));
  focus_event.type = NPCocoaEventFocusChanged;
  focus_event.data.focus.hasFocus = focused;
  instance()->NPP_HandleEvent(&focus_event);

  return true;
}

void WebPluginDelegateImpl::SetContainerVisibility(bool is_visible) {
  if (is_visible == container_is_visible_)
    return;
  container_is_visible_ = is_visible;

  // TODO(stuartmorgan): This is a temporary workarond for
  // <http://crbug.com/34266>. When that is fixed, the cached_clip_rect_ code
  // should all be removed.
  if (is_visible) {
    clip_rect_ = cached_clip_rect_;
  } else {
    clip_rect_.set_width(0);
    clip_rect_.set_height(0);
  }

  // If the plugin is changing visibility, let the plugin know. If it's scrolled
  // off screen (i.e., cached_clip_rect_ is empty), then container visibility
  // doesn't change anything.
  if (!cached_clip_rect_.IsEmpty()) {
    PluginVisibilityChanged();
    WindowlessSetWindow();
  }

  // When the plugin become visible, send an empty invalidate. If there were any
  // pending invalidations this will trigger a paint event for the damaged
  // region, and if not it's a no-op. This is necessary since higher levels
  // that would normally do this weren't responsible for the clip_rect_ change).
  if (!clip_rect_.IsEmpty())
    instance()->webplugin()->InvalidateRect(gfx::Rect());
}

void WebPluginDelegateImpl::WindowFrameChanged(const gfx::Rect& window_frame,
                                               const gfx::Rect& view_frame) {
  instance()->set_window_frame(window_frame);
  SetContentAreaOrigin(gfx::Point(view_frame.x(), view_frame.y()));
}

void WebPluginDelegateImpl::ImeCompositionCompleted(
    const base::string16& text) {
  ime_enabled_ = false;

  // If |text| is empty this was just called to tell us composition was
  // cancelled externally (e.g., the user pressed esc).
  if (!text.empty()) {
    NPCocoaEvent text_event;
    memset(&text_event, 0, sizeof(NPCocoaEvent));
    text_event.type = NPCocoaEventTextInput;
    text_event.data.text.text =
        reinterpret_cast<NPNSString*>(base::SysUTF16ToNSString(text));
    instance()->NPP_HandleEvent(&text_event);
  }
}

void WebPluginDelegateImpl::SetNSCursor(NSCursor* cursor) {
  current_windowless_cursor_.InitFromNSCursor(cursor);
}

void WebPluginDelegateImpl::SetNoBufferContext() {
  use_buffer_context_ = false;
}

#pragma mark -
#pragma mark Internal Tracking

void WebPluginDelegateImpl::SetPluginRect(const gfx::Rect& rect) {
  bool plugin_size_changed = rect.width() != window_rect_.width() ||
                             rect.height() != window_rect_.height();
  window_rect_ = rect;
  PluginScreenLocationChanged();
  if (plugin_size_changed)
    UpdateAcceleratedSurface();
}

void WebPluginDelegateImpl::SetContentAreaOrigin(const gfx::Point& origin) {
  content_area_origin_ = origin;
  PluginScreenLocationChanged();
}

void WebPluginDelegateImpl::PluginScreenLocationChanged() {
  gfx::Point plugin_origin(content_area_origin_.x() + window_rect_.x(),
                           content_area_origin_.y() + window_rect_.y());
  instance()->set_plugin_origin(plugin_origin);
}

void WebPluginDelegateImpl::PluginVisibilityChanged() {
  if (instance()->drawing_model() == NPDrawingModelCoreAnimation) {
    bool plugin_visible = container_is_visible_ && !clip_rect_.IsEmpty();
    if (plugin_visible && !redraw_timer_->IsRunning()) {
      redraw_timer_->Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kCoreAnimationRedrawPeriodMs),
          this, &WebPluginDelegateImpl::DrawLayerInSurface);
    } else if (!plugin_visible) {
      redraw_timer_->Stop();
    }
  }
}

void WebPluginDelegateImpl::StartIme() {
  if (ime_enabled_)
    return;
  ime_enabled_ = true;
  plugin_->StartIme();
}

#pragma mark -
#pragma mark Core Animation Support

void WebPluginDelegateImpl::DrawLayerInSurface() {
  // If we haven't plumbed up the surface yet, don't try to draw.
  if (!renderer_)
    return;

  [renderer_ beginFrameAtTime:CACurrentMediaTime() timeStamp:NULL];
  if (CGRectIsEmpty([renderer_ updateBounds])) {
    // If nothing has changed, we are done.
    [renderer_ endFrame];
    return;
  }

  surface_->StartDrawing();

  CGRect layerRect = [layer_ bounds];
  [renderer_ addUpdateRect:layerRect];
  [renderer_ render];
  [renderer_ endFrame];

  surface_->EndDrawing();
}

// Update the size of the surface to match the current size of the plug-in.
void WebPluginDelegateImpl::UpdateAcceleratedSurface() {
  if (!surface_ || !layer_)
    return;

  [CATransaction begin];
  [CATransaction setValue:[NSNumber numberWithInt:0]
                   forKey:kCATransactionAnimationDuration];
  [layer_ setFrame:CGRectMake(0, 0,
                              window_rect_.width(), window_rect_.height())];
  [CATransaction commit];

  [renderer_ setBounds:[layer_ bounds]];
  surface_->SetSize(window_rect_.size());
  // Kick off the drawing timer, if necessary.
  PluginVisibilityChanged();
}

}  // namespace content
