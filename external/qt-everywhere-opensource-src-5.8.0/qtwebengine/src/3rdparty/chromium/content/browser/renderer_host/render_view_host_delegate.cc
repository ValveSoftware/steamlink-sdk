// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate.h"

#include "content/public/common/web_preferences.h"
#include "url/gurl.h"

namespace content {

RenderViewHostDelegateView* RenderViewHostDelegate::GetDelegateView() {
  return NULL;
}

bool RenderViewHostDelegate::OnMessageReceived(RenderViewHost* render_view_host,
                                               const IPC::Message& message) {
  return false;
}

WebContents* RenderViewHostDelegate::GetAsWebContents() {
  return NULL;
}

SessionStorageNamespace* RenderViewHostDelegate::GetSessionStorageNamespace(
    SiteInstance* instance) {
  return NULL;
}

SessionStorageNamespaceMap
RenderViewHostDelegate::GetSessionStorageNamespaceMap() {
  return SessionStorageNamespaceMap();
}

FrameTree* RenderViewHostDelegate::GetFrameTree() {
  return NULL;
}

double RenderViewHostDelegate::GetPendingPageZoomLevel() {
  return 0.0;
}

bool RenderViewHostDelegate::IsNeverVisible() {
  return false;
}

bool RenderViewHostDelegate::IsVirtualKeyboardRequested() {
  return false;
}

bool RenderViewHostDelegate::IsOverridingUserAgent() {
  return false;
}

}  // namespace content
