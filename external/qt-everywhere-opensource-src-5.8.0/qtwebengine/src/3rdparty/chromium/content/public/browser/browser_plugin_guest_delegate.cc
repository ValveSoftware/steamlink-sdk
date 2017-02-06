// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_delegate.h"

namespace content {

bool BrowserPluginGuestDelegate::CanRunInDetachedState() const {
  return false;
}

WebContents* BrowserPluginGuestDelegate::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  NOTREACHED();
  return nullptr;
}

WebContents* BrowserPluginGuestDelegate::GetOwnerWebContents() const {
  return nullptr;
}

bool BrowserPluginGuestDelegate::HandleFindForEmbedder(
    int request_id,
    const base::string16& search_text,
    const blink::WebFindOptions& options) {
  return false;
}

bool BrowserPluginGuestDelegate::HandleStopFindingForEmbedder(
    StopFindAction action) {
  return false;
}

}  // namespace content
