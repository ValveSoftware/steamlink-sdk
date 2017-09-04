// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webapplicationcachehost_impl.h"

#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebApplicationCacheHostClient;
using blink::WebConsoleMessage;

namespace content {

RendererWebApplicationCacheHostImpl::RendererWebApplicationCacheHostImpl(
    RenderViewImpl* render_view,
    WebApplicationCacheHostClient* client,
    AppCacheBackend* backend)
    : WebApplicationCacheHostImpl(client, backend),
      routing_id_(render_view->GetRoutingID()) {}

void RendererWebApplicationCacheHostImpl::OnLogMessage(
    AppCacheLogLevel log_level, const std::string& message) {
  if (RenderThreadImpl::current()->layout_test_mode())
    return;

  RenderViewImpl* render_view = GetRenderView();
  if (!render_view || !render_view->webview() ||
      !render_view->webview()->mainFrame())
    return;

  blink::WebFrame* frame = render_view->webview()->mainFrame();
  frame->addMessageToConsole(WebConsoleMessage(
        static_cast<WebConsoleMessage::Level>(log_level),
        blink::WebString::fromUTF8(message.c_str())));
}

void RendererWebApplicationCacheHostImpl::OnContentBlocked(
    const GURL& manifest_url) {
  RenderThreadImpl::current()->Send(new ViewHostMsg_AppCacheAccessed(
      routing_id_, manifest_url, true));
}

void RendererWebApplicationCacheHostImpl::OnCacheSelected(
    const AppCacheInfo& info) {
  if (!info.manifest_url.is_empty()) {
    RenderThreadImpl::current()->Send(new ViewHostMsg_AppCacheAccessed(
        routing_id_, info.manifest_url, false));
  }
  WebApplicationCacheHostImpl::OnCacheSelected(info);
}

RenderViewImpl* RendererWebApplicationCacheHostImpl::GetRenderView() {
  return RenderViewImpl::FromRoutingID(routing_id_);
}

}  // namespace content
