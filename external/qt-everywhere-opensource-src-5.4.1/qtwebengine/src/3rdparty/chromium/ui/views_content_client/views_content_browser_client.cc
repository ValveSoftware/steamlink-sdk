// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_browser_client.h"

#include "content/shell/browser/shell_browser_context.h"
#include "ui/views_content_client/views_content_client_main_parts.h"

namespace ui {

ViewsContentBrowserClient::ViewsContentBrowserClient(
    ViewsContentClient* views_content_client)
    : views_content_main_parts_(NULL),
      views_content_client_(views_content_client) {
}

ViewsContentBrowserClient::~ViewsContentBrowserClient() {
}

content::BrowserMainParts* ViewsContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  views_content_main_parts_ =
      ViewsContentClientMainParts::Create(parameters, views_content_client_);
  return views_content_main_parts_;
}

net::URLRequestContextGetter*
ViewsContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  content::ShellBrowserContext* shell_context =
      views_content_main_parts_->browser_context();
  return shell_context->CreateRequestContext(protocol_handlers,
                                             request_interceptors.Pass());
}

}  // namespace ui
