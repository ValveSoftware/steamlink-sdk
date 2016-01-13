// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"

#include "ipc/ipc_message.h"

namespace content {

WebContents::CreateParams::CreateParams(BrowserContext* context)
    : browser_context(context),
      site_instance(NULL),
      opener(NULL),
      opener_suppressed(false),
      routing_id(MSG_ROUTING_NONE),
      main_frame_routing_id(MSG_ROUTING_NONE),
      initially_hidden(false),
      guest_instance_id(0),
      context(NULL) {}

WebContents::CreateParams::CreateParams(
    BrowserContext* context, SiteInstance* site)
    : browser_context(context),
      site_instance(site),
      opener(NULL),
      opener_suppressed(false),
      routing_id(MSG_ROUTING_NONE),
      main_frame_routing_id(MSG_ROUTING_NONE),
      initially_hidden(false),
      guest_instance_id(0),
      context(NULL) {}

WebContents::CreateParams::~CreateParams() {
}

}  // namespace content
