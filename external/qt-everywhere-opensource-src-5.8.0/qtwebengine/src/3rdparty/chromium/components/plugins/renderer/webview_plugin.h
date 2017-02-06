// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
#define COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_

#include <list>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace blink {
class WebFrameWidget;
class WebMouseEvent;
}

namespace content {
class RenderView;
struct WebPreferences;
}

namespace gfx {
class Size;
}

// This class implements the WebPlugin interface by forwarding drawing and
// handling input events to a WebView.
// It can be used as a placeholder for an actual plugin, using HTML for the UI.
// To show HTML data inside the WebViewPlugin,
// call web_view->mainFrame()->loadHTMLString() with the HTML data and a fake
// chrome:// URL as origin.

class WebViewPlugin : public blink::WebPlugin,
                      public blink::WebViewClient,
                      public blink::WebFrameClient,
                      public content::RenderViewObserver {
 public:
  class Delegate {
   public:
    // Called to get the V8 handle used to bind the lifetime to the frame.
    virtual v8::Local<v8::Value> GetV8Handle(v8::Isolate*) = 0;

    // Called upon a context menu event.
    virtual void ShowContextMenu(const blink::WebMouseEvent&) = 0;

    // Called when the WebViewPlugin is destroyed.
    virtual void PluginDestroyed() = 0;

    // Called to enable JavaScript pass-through to a throttled plugin, which is
    // loaded but idle. Doesn't work for blocked plugins, which is not loaded.
    virtual v8::Local<v8::Object> GetV8ScriptableObject(v8::Isolate*) const = 0;

    // Called when the unobscured rect of the plugin is updated.
    virtual void OnUnobscuredRectUpdate(const gfx::Rect& unobscured_rect) {}
  };

  // Convenience method to set up a new WebViewPlugin using |preferences|
  // and displaying |html_data|. |url| should be a (fake) data:text/html URL;
  // it is only used for navigation and never actually resolved.
  static WebViewPlugin* Create(content::RenderView* render_view,
                               Delegate* delegate,
                               const content::WebPreferences& preferences,
                               const std::string& html_data,
                               const GURL& url);

  blink::WebView* web_view() { return web_view_; }

  // When loading a plugin document (i.e. a full page plugin not embedded in
  // another page), we save all data that has been received, and replay it with
  // this method on the actual plugin.
  void ReplayReceivedData(blink::WebPlugin* plugin);

  void RestoreTitleText();

  // WebPlugin methods:
  blink::WebPluginContainer* container() const override;
  // The WebViewPlugin, by design, never fails to initialize. It's used to
  // display placeholders and error messages, so it must never fail.
  bool initialize(blink::WebPluginContainer*) override;
  void destroy() override;

  v8::Local<v8::Object> v8ScriptableObject(v8::Isolate* isolate) override;

  void updateAllLifecyclePhases() override;
  void paint(blink::WebCanvas* canvas, const blink::WebRect& rect) override;

  // Coordinates are relative to the containing window.
  void updateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      const blink::WebVector<blink::WebRect>& cut_outs_rects,
                      bool is_visible) override;

  void updateFocus(bool foucsed, blink::WebFocusType focus_type) override;
  void updateVisibility(bool) override {}

  blink::WebInputEventResult handleInputEvent(
      const blink::WebInputEvent& event,
      blink::WebCursorInfo& cursor_info) override;

  void didReceiveResponse(const blink::WebURLResponse& response) override;
  void didReceiveData(const char* data, int data_length) override;
  void didFinishLoading() override;
  void didFailLoading(const blink::WebURLError& error) override;

  // WebViewClient methods:
  bool acceptsLoadDrops() override;

  void setToolTipText(const blink::WebString&,
                      blink::WebTextDirection) override;

  void startDragging(blink::WebLocalFrame* frame,
                     const blink::WebDragData& drag_data,
                     blink::WebDragOperationsMask mask,
                     const blink::WebImage& image,
                     const blink::WebPoint& point) override;

  // TODO(ojan): Remove this override and have this class use a non-null
  // layerTreeView.
  bool allowsBrokenNullLayerTreeView() const override;

  // WebWidgetClient methods:
  void didInvalidateRect(const blink::WebRect&) override;
  void didChangeCursor(const blink::WebCursorInfo& cursor) override;
  void scheduleAnimation() override;

  // WebFrameClient methods:
  void didClearWindowObject(blink::WebLocalFrame* frame) override;

  // This method is defined in WebPlugin as well as in WebFrameClient, but with
  // different parameters. We only care about implementing the WebPlugin
  // version, so we implement this method and call the default in WebFrameClient
  // (which does nothing) to correctly overload it.
  void didReceiveResponse(unsigned identifier,
                          const blink::WebURLResponse& response) override;

 private:
  friend class base::DeleteHelper<WebViewPlugin>;
  WebViewPlugin(content::RenderView* render_view,
                Delegate* delegate,
                const content::WebPreferences& preferences);
  ~WebViewPlugin() override;

  // content::RenderViewObserver methods:
  void OnDestruct() override;
  void OnZoomLevelChanged() override;

  void UpdatePluginForNewGeometry(const blink::WebRect& window_rect,
                                  const blink::WebRect& unobscured_rect);

  // Manages its own lifetime.
  Delegate* delegate_;

  blink::WebCursorInfo current_cursor_;

  // Owns us.
  blink::WebPluginContainer* container_;

  // Owned by us, deleted via |close()|.
  blink::WebView* web_view_;

  // Owned by us, deleted via |close()|.
  blink::WebFrameWidget* web_frame_widget_;

  // Owned by us, deleted via |close()|.
  blink::WebFrame* web_frame_;
  gfx::Rect rect_;

  blink::WebURLResponse response_;
  std::list<std::string> data_;
  std::unique_ptr<blink::WebURLError> error_;
  blink::WebString old_title_;
  bool finished_loading_;
  bool focused_;
  bool is_painting_;
  bool is_resizing_;

  // Should be invalidated when destroy() is called.
  base::WeakPtrFactory<WebViewPlugin> weak_factory_;
};

#endif  // COMPONENTS_PLUGINS_RENDERER_WEBVIEW_PLUGIN_H_
