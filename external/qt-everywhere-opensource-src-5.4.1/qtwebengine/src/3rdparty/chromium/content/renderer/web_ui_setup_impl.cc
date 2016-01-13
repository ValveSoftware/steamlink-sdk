// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_setup_impl.h"

#include "content/public/renderer/render_view.h"
#include "content/renderer/web_ui_mojo.h"

namespace content {

// static
void WebUISetupImpl::Bind(mojo::ScopedMessagePipeHandle handle) {
  mojo::BindToPipe(new WebUISetupImpl(), handle.Pass());
}

void WebUISetupImpl::OnConnectionError() {
  delete this;
}

void WebUISetupImpl::SetWebUIHandle(
    int32 view_routing_id,
    mojo::ScopedMessagePipeHandle web_ui_handle) {
  RenderView* render_view = RenderView::FromRoutingID(view_routing_id);
  if (!render_view)
    return;
  WebUIMojo* web_ui_mojo = WebUIMojo::Get(render_view);
  if (!web_ui_mojo)
    return;
  web_ui_mojo->SetBrowserHandle(web_ui_handle.Pass());
}

}  // namespace content
