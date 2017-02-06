// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webcookiejar_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/frame_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_frame_impl.h"

using blink::WebString;
using blink::WebURL;

namespace content {

void RendererWebCookieJarImpl::setCookie(
    const WebURL& url, const WebURL& first_party_for_cookies,
    const WebString& value) {
  std::string value_utf8 = base::UTF16ToUTF8(base::StringPiece16(value));
  sender_->Send(new FrameHostMsg_SetCookie(
      sender_->GetRoutingID(), url, first_party_for_cookies, value_utf8));
}

WebString RendererWebCookieJarImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  std::string value_utf8;
  sender_->Send(new FrameHostMsg_GetCookies(
      sender_->GetRoutingID(), url, first_party_for_cookies, &value_utf8));
  return WebString::fromUTF8(value_utf8);
}

bool RendererWebCookieJarImpl::cookiesEnabled(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  bool cookies_enabled = false;
  sender_->Send(new FrameHostMsg_CookiesEnabled(
      sender_->GetRoutingID(), url, first_party_for_cookies, &cookies_enabled));
  return cookies_enabled;
}

}  // namespace content
