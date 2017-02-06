// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_fullscreen_pepper.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCanvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gpu_preference.h"

using blink::WebCanvas;
using blink::WebCompositionUnderline;
using blink::WebCursorInfo;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using blink::WebTextDirection;
using blink::WebTextInputType;
using blink::WebVector;
using blink::WebWidget;

namespace content {

namespace {

class FullscreenMouseLockDispatcher : public MouseLockDispatcher {
 public:
  explicit FullscreenMouseLockDispatcher(RenderWidgetFullscreenPepper* widget);
  ~FullscreenMouseLockDispatcher() override;

 private:
  // MouseLockDispatcher implementation.
  void SendLockMouseRequest(bool unlocked_by_target) override;
  void SendUnlockMouseRequest() override;

  RenderWidgetFullscreenPepper* widget_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenMouseLockDispatcher);
};

WebMouseEvent WebMouseEventFromGestureEvent(const WebGestureEvent& gesture) {
  WebMouseEvent mouse;

  // Only convert touch screen gesture events, do not convert
  // touchpad/mouse wheel gesture events. (crbug.com/620974)
  if (gesture.sourceDevice != blink::WebGestureDeviceTouchscreen)
    return mouse;

  switch (gesture.type) {
    case WebInputEvent::GestureScrollBegin:
      mouse.type = WebInputEvent::MouseDown;
      break;
    case WebInputEvent::GestureScrollUpdate:
      mouse.type = WebInputEvent::MouseMove;
      break;
    case WebInputEvent::GestureFlingStart:
      // A scroll gesture on the touchscreen may end with a GestureScrollEnd
      // when there is no velocity, or a GestureFlingStart when it has a
      // velocity. In both cases, it should end the drag that was initiated by
      // the GestureScrollBegin (and subsequent GestureScrollUpdate) events.
      mouse.type = WebInputEvent::MouseUp;
    case WebInputEvent::GestureScrollEnd:
      mouse.type = WebInputEvent::MouseUp;
      break;
    default:
      return mouse;
  }

  mouse.timeStampSeconds = gesture.timeStampSeconds;
  mouse.modifiers = gesture.modifiers | WebInputEvent::LeftButtonDown;
  mouse.button = WebMouseEvent::ButtonLeft;
  mouse.clickCount = (mouse.type == WebInputEvent::MouseDown ||
                      mouse.type == WebInputEvent::MouseUp);

  mouse.x = gesture.x;
  mouse.y = gesture.y;
  mouse.windowX = gesture.x;
  mouse.windowY = gesture.y;
  mouse.globalX = gesture.globalX;
  mouse.globalY = gesture.globalY;

  return mouse;
}

FullscreenMouseLockDispatcher::FullscreenMouseLockDispatcher(
    RenderWidgetFullscreenPepper* widget) : widget_(widget) {
}

FullscreenMouseLockDispatcher::~FullscreenMouseLockDispatcher() {
}

void FullscreenMouseLockDispatcher::SendLockMouseRequest(
    bool unlocked_by_target) {
  widget_->Send(new ViewHostMsg_LockMouse(widget_->routing_id(), false,
                                          unlocked_by_target, true));
}

void FullscreenMouseLockDispatcher::SendUnlockMouseRequest() {
  widget_->Send(new ViewHostMsg_UnlockMouse(widget_->routing_id()));
}

// WebWidget that simply wraps the pepper plugin.
// TODO(piman): figure out IME and implement setComposition and friends if
// necessary.
class PepperWidget : public WebWidget {
 public:
  explicit PepperWidget(RenderWidgetFullscreenPepper* widget)
      : widget_(widget) {
  }

  virtual ~PepperWidget() {}

  // WebWidget API
  void close() override { delete this; }

  WebSize size() override { return size_; }

  void resize(const WebSize& size) override {
    if (!widget_->plugin() || size_ == size)
      return;

    size_ = size;
    WebRect plugin_rect(0, 0, size_.width, size_.height);
    widget_->plugin()->ViewChanged(plugin_rect, plugin_rect, plugin_rect);
    widget_->Invalidate();
  }

  void themeChanged() override { NOTIMPLEMENTED(); }

  WebInputEventResult handleInputEvent(const WebInputEvent& event) override {
    if (!widget_->plugin())
      return WebInputEventResult::NotHandled;

    // This cursor info is ignored, we always set the cursor directly from
    // RenderWidgetFullscreenPepper::DidChangeCursor.
    WebCursorInfo cursor;

    // Pepper plugins do not accept gesture events. So do not send the gesture
    // events directly to the plugin. Instead, try to convert them to equivalent
    // mouse events, and then send to the plugin.
    if (WebInputEvent::isGestureEventType(event.type)) {
      bool result = false;
      const WebGestureEvent* gesture_event =
          static_cast<const WebGestureEvent*>(&event);
      switch (event.type) {
        case WebInputEvent::GestureTap: {
          WebMouseEvent mouse;

          mouse.timeStampSeconds = gesture_event->timeStampSeconds;
          mouse.type = WebInputEvent::MouseMove;
          mouse.modifiers = gesture_event->modifiers;

          mouse.x = gesture_event->x;
          mouse.y = gesture_event->y;
          mouse.windowX = gesture_event->x;
          mouse.windowY = gesture_event->y;
          mouse.globalX = gesture_event->globalX;
          mouse.globalY = gesture_event->globalY;
          mouse.movementX = 0;
          mouse.movementY = 0;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);

          mouse.type = WebInputEvent::MouseDown;
          mouse.button = WebMouseEvent::ButtonLeft;
          mouse.clickCount = gesture_event->data.tap.tapCount;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);

          mouse.type = WebInputEvent::MouseUp;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);
          break;
        }

        default: {
          WebMouseEvent mouse = WebMouseEventFromGestureEvent(*gesture_event);
          if (mouse.type != WebInputEvent::Undefined)
            result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);
          break;
        }
      }
      return result ? WebInputEventResult::HandledApplication
                    : WebInputEventResult::NotHandled;
    }

    bool result = widget_->plugin()->HandleInputEvent(event, &cursor);

    // For normal web pages, WebViewImpl does input event translations and
    // generates context menu events. Since we don't have a WebView, we need to
    // do the necessary translation ourselves.
    if (WebInputEvent::isMouseEventType(event.type)) {
      const WebMouseEvent& mouse_event =
          reinterpret_cast<const WebMouseEvent&>(event);
      bool send_context_menu_event = false;
      // On Mac/Linux, we handle it on mouse down.
      // On Windows, we handle it on mouse up.
#if defined(OS_WIN)
      send_context_menu_event =
          mouse_event.type == WebInputEvent::MouseUp &&
          mouse_event.button == WebMouseEvent::ButtonRight;
#elif defined(OS_MACOSX)
      send_context_menu_event =
          mouse_event.type == WebInputEvent::MouseDown &&
          (mouse_event.button == WebMouseEvent::ButtonRight ||
           (mouse_event.button == WebMouseEvent::ButtonLeft &&
            mouse_event.modifiers & WebMouseEvent::ControlKey));
#else
      send_context_menu_event =
          mouse_event.type == WebInputEvent::MouseDown &&
          mouse_event.button == WebMouseEvent::ButtonRight;
#endif
      if (send_context_menu_event) {
        WebMouseEvent context_menu_event(mouse_event);
        context_menu_event.type = WebInputEvent::ContextMenu;
        widget_->plugin()->HandleInputEvent(context_menu_event, &cursor);
      }
    }
    return result ? WebInputEventResult::HandledApplication
                  : WebInputEventResult::NotHandled;
  }

 private:
  RenderWidgetFullscreenPepper* widget_;
  WebSize size_;

  DISALLOW_COPY_AND_ASSIGN(PepperWidget);
};

}  // anonymous namespace

// static
RenderWidgetFullscreenPepper* RenderWidgetFullscreenPepper::Create(
    int32_t opener_id,
    CompositorDependencies* compositor_deps,
    PepperPluginInstanceImpl* plugin,
    const GURL& active_url,
    const blink::WebScreenInfo& screen_info) {
  DCHECK_NE(MSG_ROUTING_NONE, opener_id);
  scoped_refptr<RenderWidgetFullscreenPepper> widget(
      new RenderWidgetFullscreenPepper(compositor_deps, plugin, active_url,
                                       screen_info));
  widget->Init(opener_id);
  widget->AddRef();
  return widget.get();
}

RenderWidgetFullscreenPepper::RenderWidgetFullscreenPepper(
    CompositorDependencies* compositor_deps,
    PepperPluginInstanceImpl* plugin,
    const GURL& active_url,
    const blink::WebScreenInfo& screen_info)
    : RenderWidgetFullscreen(compositor_deps, screen_info),
      active_url_(active_url),
      plugin_(plugin),
      layer_(NULL),
      mouse_lock_dispatcher_(new FullscreenMouseLockDispatcher(this)) {}

RenderWidgetFullscreenPepper::~RenderWidgetFullscreenPepper() {
}

void RenderWidgetFullscreenPepper::Invalidate() {
  InvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

void RenderWidgetFullscreenPepper::InvalidateRect(const blink::WebRect& rect) {
  didInvalidateRect(rect);
}

void RenderWidgetFullscreenPepper::ScrollRect(
    int dx, int dy, const blink::WebRect& rect) {
}

void RenderWidgetFullscreenPepper::Destroy() {
  // This function is called by the plugin instance as it's going away, so reset
  // plugin_ to NULL to avoid calling into a dangling pointer e.g. on Close().
  plugin_ = NULL;

  // After calling Destroy(), the plugin instance assumes that the layer is not
  // used by us anymore, so it may destroy the layer before this object goes
  // away.
  SetLayer(NULL);

  Send(new ViewHostMsg_Close(routing_id_));
  Release();
}

void RenderWidgetFullscreenPepper::PepperDidChangeCursor(
    const blink::WebCursorInfo& cursor) {
  didChangeCursor(cursor);
}

void RenderWidgetFullscreenPepper::SetLayer(blink::WebLayer* layer) {
  layer_ = layer;
  if (!layer_) {
    if (compositor_)
      compositor_->clearRootLayer();
    return;
  }
  if (!layerTreeView())
    initializeLayerTreeView();
  layer_->setBounds(blink::WebSize(size()));
  layer_->setDrawsContent(true);
  compositor_->setDeviceScaleFactor(device_scale_factor_);
  compositor_->setRootLayer(*layer_);
}

bool RenderWidgetFullscreenPepper::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetFullscreenPepper, msg)
    IPC_MESSAGE_FORWARD(ViewMsg_LockMouse_ACK,
                        mouse_lock_dispatcher_.get(),
                        MouseLockDispatcher::OnLockMouseACK)
    IPC_MESSAGE_FORWARD(ViewMsg_MouseLockLost,
                        mouse_lock_dispatcher_.get(),
                        MouseLockDispatcher::OnMouseLockLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  return RenderWidgetFullscreen::OnMessageReceived(msg);
}

void RenderWidgetFullscreenPepper::DidInitiatePaint() {
  if (plugin_)
    plugin_->ViewInitiatedPaint();
}

void RenderWidgetFullscreenPepper::DidFlushPaint() {
}

void RenderWidgetFullscreenPepper::Close() {
  // If the fullscreen window is closed (e.g. user pressed escape), reset to
  // normal mode.
  if (plugin_)
    plugin_->FlashSetFullscreen(false, false);

  // Call Close on the base class to destroy the WebWidget instance.
  RenderWidget::Close();
}

void RenderWidgetFullscreenPepper::OnResize(const ResizeParams& params) {
  if (layer_)
    layer_->setBounds(blink::WebSize(params.new_size));
  RenderWidget::OnResize(params);
}

WebWidget* RenderWidgetFullscreenPepper::CreateWebWidget() {
  return new PepperWidget(this);
}

GURL RenderWidgetFullscreenPepper::GetURLForGraphicsContext3D() {
  return active_url_;
}

void RenderWidgetFullscreenPepper::OnDeviceScaleFactorChanged() {
  if (compositor_)
    compositor_->setDeviceScaleFactor(device_scale_factor_);
}

}  // namespace content
