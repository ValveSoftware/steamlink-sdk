// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/webview_plugin.h"

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_view.h"
#include "gin/converter.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebCanvas;
using blink::WebCursorInfo;
using blink::WebDragData;
using blink::WebDragOperationsMask;
using blink::WebFrameWidget;
using blink::WebImage;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPoint;
using blink::WebRect;
using blink::WebString;
using blink::WebURLError;
using blink::WebURLResponse;
using blink::WebVector;
using blink::WebView;
using content::WebPreferences;

WebViewPlugin::WebViewPlugin(content::RenderView* render_view,
                             WebViewPlugin::Delegate* delegate,
                             const WebPreferences& preferences)
    : content::RenderViewObserver(render_view),
      delegate_(delegate),
      container_(nullptr),
      web_view_(WebView::create(this, blink::WebPageVisibilityStateVisible)),
      focused_(false),
      is_painting_(false),
      is_resizing_(false),
      web_frame_client_(this),
      weak_factory_(this) {
  // ApplyWebPreferences before making a WebLocalFrame so that the frame sees a
  // consistent view of our preferences.
  content::RenderView::ApplyWebPreferences(preferences, web_view_);
  WebLocalFrame* web_frame = WebLocalFrame::create(
      blink::WebTreeScopeType::Document, &web_frame_client_);
  web_view_->setMainFrame(web_frame);
  // TODO(dcheng): The main frame widget currently has a special case.
  // Eliminate this once WebView is no longer a WebWidget.
  WebFrameWidget::create(this, web_view_, web_frame);
}

// static
WebViewPlugin* WebViewPlugin::Create(content::RenderView* render_view,
                                     WebViewPlugin::Delegate* delegate,
                                     const WebPreferences& preferences,
                                     const std::string& html_data,
                                     const GURL& url) {
  DCHECK(url.is_valid()) << "Blink requires the WebView to have a valid URL.";
  WebViewPlugin* plugin = new WebViewPlugin(render_view, delegate, preferences);
  plugin->web_view()->mainFrame()->loadHTMLString(html_data, url);
  return plugin;
}

WebViewPlugin::~WebViewPlugin() {
  DCHECK(!weak_factory_.HasWeakPtrs());
  web_view_->close();
}

WebPluginContainer* WebViewPlugin::container() const { return container_; }

bool WebViewPlugin::initialize(WebPluginContainer* container) {
  DCHECK(container);
  DCHECK_EQ(this, container->plugin());
  container_ = container;

  // We must call layout again here to ensure that the container is laid
  // out before we next try to paint it, which is a requirement of the
  // document life cycle in Blink. In most cases, needsLayout is set by
  // scheduleAnimation, but due to timers controlling widget update,
  // scheduleAnimation may be invoked before this initialize call (which
  // comes through the widget update process). It doesn't hurt to mark
  // for animation again, and it does help us in the race-condition situation.
  container_->scheduleAnimation();

  old_title_ = container_->element().getAttribute("title");

  // Propagate device scale and zoom level to inner webview.
  web_view_->setDeviceScaleFactor(container_->deviceScaleFactor());
  web_view_->setZoomLevel(
      blink::WebView::zoomFactorToZoomLevel(container_->pageZoomFactor()));

  return true;
}

void WebViewPlugin::destroy() {
  weak_factory_.InvalidateWeakPtrs();

  if (delegate_) {
    delegate_->PluginDestroyed();
    delegate_ = nullptr;
  }
  container_ = nullptr;
  content::RenderViewObserver::Observe(nullptr);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

v8::Local<v8::Object> WebViewPlugin::v8ScriptableObject(v8::Isolate* isolate) {
  if (!delegate_)
    return v8::Local<v8::Object>();

  return delegate_->GetV8ScriptableObject(isolate);
}

void WebViewPlugin::updateAllLifecyclePhases() {
  web_view_->updateAllLifecyclePhases();
}

void WebViewPlugin::paint(WebCanvas* canvas, const WebRect& rect) {
  gfx::Rect paint_rect = gfx::IntersectRects(rect_, rect);
  if (paint_rect.IsEmpty())
    return;

  base::AutoReset<bool> is_painting(
        &is_painting_, true);

  paint_rect.Offset(-rect_.x(), -rect_.y());

  canvas->save();
  canvas->translate(SkIntToScalar(rect_.x()), SkIntToScalar(rect_.y()));

  // Apply inverse device scale factor, as the outer webview has already
  // applied it, and the inner webview will apply it again.
  SkScalar inverse_scale =
      SkFloatToScalar(1.0 / container_->deviceScaleFactor());
  canvas->scale(inverse_scale, inverse_scale);

  web_view_->paint(canvas, paint_rect);

  canvas->restore();
}

// Coordinates are relative to the containing window.
void WebViewPlugin::updateGeometry(const WebRect& window_rect,
                                   const WebRect& clip_rect,
                                   const WebRect& unobscured_rect,
                                   const WebVector<WebRect>& cut_outs_rects,
                                   bool is_visible) {
  DCHECK(container_);

  base::AutoReset<bool> is_resizing(&is_resizing_, true);

  if (static_cast<gfx::Rect>(window_rect) != rect_) {
    rect_ = window_rect;
    web_view_->resize(rect_.size());
  }

  // Plugin updates are forbidden during Blink layout. Therefore,
  // UpdatePluginForNewGeometry must be posted to a task to run asynchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&WebViewPlugin::UpdatePluginForNewGeometry,
                 weak_factory_.GetWeakPtr(), window_rect, unobscured_rect));
}

void WebViewPlugin::updateFocus(bool focused, blink::WebFocusType focus_type) {
  focused_ = focused;
}

blink::WebInputEventResult WebViewPlugin::handleInputEvent(
    const WebInputEvent& event,
    WebCursorInfo& cursor) {
  // For tap events, don't handle them. They will be converted to
  // mouse events later and passed to here.
  if (event.type == WebInputEvent::GestureTap)
    return blink::WebInputEventResult::NotHandled;

  // For LongPress events we return false, since otherwise the context menu will
  // be suppressed. https://crbug.com/482842
  if (event.type == WebInputEvent::GestureLongPress)
    return blink::WebInputEventResult::NotHandled;

  if (event.type == WebInputEvent::ContextMenu) {
    if (delegate_) {
      const WebMouseEvent& mouse_event =
          reinterpret_cast<const WebMouseEvent&>(event);
      delegate_->ShowContextMenu(mouse_event);
    }
    return blink::WebInputEventResult::HandledSuppressed;
  }
  current_cursor_ = cursor;
  blink::WebInputEventResult handled = web_view_->handleInputEvent(event);
  cursor = current_cursor_;

  return handled;
}

void WebViewPlugin::didReceiveResponse(const WebURLResponse& response) {
  NOTREACHED();
}

void WebViewPlugin::didReceiveData(const char* data, int data_length) {
  NOTREACHED();
}

void WebViewPlugin::didFinishLoading() {
  NOTREACHED();
}

void WebViewPlugin::didFailLoading(const WebURLError& error) {
  NOTREACHED();
}

bool WebViewPlugin::acceptsLoadDrops() { return false; }

void WebViewPlugin::setToolTipText(const WebString& text,
                                   blink::WebTextDirection hint) {
  if (container_)
    container_->element().setAttribute("title", text);
}

void WebViewPlugin::startDragging(blink::WebReferrerPolicy,
                                  const WebDragData&,
                                  WebDragOperationsMask,
                                  const WebImage&,
                                  const WebPoint&) {
  // Immediately stop dragging.
  DCHECK(web_view_->mainFrame()->isWebLocalFrame());
  web_view_->mainFrame()->toWebLocalFrame()->frameWidget()->
      dragSourceSystemDragEnded();
}

bool WebViewPlugin::allowsBrokenNullLayerTreeView() const {
  return true;
}

void WebViewPlugin::didInvalidateRect(const WebRect& rect) {
  if (container_)
    container_->invalidateRect(rect);
}

void WebViewPlugin::didChangeCursor(const WebCursorInfo& cursor) {
  current_cursor_ = cursor;
}

void WebViewPlugin::scheduleAnimation() {
  // Resizes must be self-contained: any lifecycle updating must
  // be triggerd from within the WebView or this WebViewPlugin.
  // This is because this WebViewPlugin is contained in another
  // Web View which may be in the middle of updating its lifecycle,
  // but after layout is done, and it is illegal to dirty earlier
  // lifecycle stages during later ones.
  if (is_resizing_)
    return;
  if (container_) {
    // This should never happen; see also crbug.com/545039 for context.
    CHECK(!is_painting_);
    container_->scheduleAnimation();
  }
}

void WebViewPlugin::PluginWebFrameClient::didClearWindowObject(
    WebLocalFrame* frame) {
  if (!plugin_->delegate_)
    return;

  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  DCHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();

  global->Set(gin::StringToV8(isolate, "plugin"),
              plugin_->delegate_->GetV8Handle(isolate));
}

void WebViewPlugin::OnDestruct() {}

void WebViewPlugin::OnZoomLevelChanged() {
  if (container_) {
    web_view_->setZoomLevel(
      blink::WebView::zoomFactorToZoomLevel(container_->pageZoomFactor()));
  }
}

void WebViewPlugin::UpdatePluginForNewGeometry(
    const blink::WebRect& window_rect,
    const blink::WebRect& unobscured_rect) {
  DCHECK(container_);
  if (!delegate_)
    return;

  // The delegate may instantiate a new plugin.
  delegate_->OnUnobscuredRectUpdate(gfx::Rect(unobscured_rect));
  // The delegate may have dirtied style and layout of the WebView.
  // See for example the resizePoster function in plugin_poster.html.
  // Run the lifecycle now so that it is clean.
  web_view_->updateAllLifecyclePhases();
}
