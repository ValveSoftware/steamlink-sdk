// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility.h"

#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebDocument;
using blink::WebFrame;
using blink::WebView;

namespace content {

RendererAccessibility::RendererAccessibility(
    RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      render_view_(render_view) {
}

RendererAccessibility::~RendererAccessibility() {
}

WebDocument RendererAccessibility::GetMainDocument() {
  WebView* view = render_view()->GetWebView();
  WebFrame* main_frame = view ? view->mainFrame() : NULL;

  if (main_frame)
    return main_frame->document();

  return WebDocument();
}

}  // namespace content
