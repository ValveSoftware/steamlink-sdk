// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/view_messages.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

// Check content::BrowserControlsState, and
// blink::WebWidget::BrowserControlsState
// are kept in sync.
static_assert(int(BROWSER_CONTROLS_STATE_SHOWN) ==
                  int(blink::WebBrowserControlsShown),
              "mismatching enums: SHOWN");
static_assert(int(BROWSER_CONTROLS_STATE_HIDDEN) ==
                  int(blink::WebBrowserControlsHidden),
              "mismatching enums: HIDDEN");
static_assert(int(BROWSER_CONTROLS_STATE_BOTH) ==
                  int(blink::WebBrowserControlsBoth),
              "mismatching enums: BOTH");

blink::WebBrowserControlsState ContentToBlink(BrowserControlsState state) {
  return static_cast<blink::WebBrowserControlsState>(state);
}


// TODO(mvanouwerkerk): Stop calling this code path and delete it.
void RenderViewImpl::OnUpdateBrowserControlsState(bool enable_hiding,
                                                  bool enable_showing,
                                                  bool animate) {
  // TODO(tedchoc): Investigate why messages are getting here before the
  //                compositor has been initialized.
  LOG_IF(WARNING, !compositor_)
      << "OnUpdateBrowserControlsState was unhandled.";
  BrowserControlsState constraints = BROWSER_CONTROLS_STATE_BOTH;
  if (!enable_showing)
    constraints = BROWSER_CONTROLS_STATE_HIDDEN;
  if (!enable_hiding)
    constraints = BROWSER_CONTROLS_STATE_SHOWN;
  BrowserControlsState current = BROWSER_CONTROLS_STATE_BOTH;

  UpdateBrowserControlsState(constraints, current, animate);
}

void RenderViewImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  if (GetWebWidget())
    GetWebWidget()->updateBrowserControlsState(
        ContentToBlink(constraints), ContentToBlink(current), animate);

  top_controls_constraints_ = constraints;
}

void RenderViewImpl::didScrollWithKeyboard(const blink::WebSize& delta) {
  if (delta.height == 0)
    return;

  BrowserControlsState current = delta.height < 0
                                     ? BROWSER_CONTROLS_STATE_SHOWN
                                     : BROWSER_CONTROLS_STATE_HIDDEN;

  UpdateBrowserControlsState(top_controls_constraints_, current, true);
}

void RenderViewImpl::OnExtractSmartClipData(const gfx::Rect& rect) {
  blink::WebString clip_text;
  blink::WebString clip_html;
  blink::WebRect clip_rect;
  webview()->extractSmartClipData(rect, clip_text, clip_html, clip_rect);
  Send(new ViewHostMsg_SmartClipDataExtracted(
      routing_id_, clip_text, clip_html, clip_rect));
}

}  // namespace content
