// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webcookiejar_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/cookie_data.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebCookie.h"

using blink::WebCookie;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

void RendererWebCookieJarImpl::setCookie(
    const WebURL& url, const WebURL& first_party_for_cookies,
    const WebString& value) {
  std::string value_utf8 = base::UTF16ToUTF8(value);
  sender_->Send(new ViewHostMsg_SetCookie(
      sender_->GetRoutingID(), url, first_party_for_cookies, value_utf8));
}

WebString RendererWebCookieJarImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  std::string value_utf8;
  sender_->Send(new ViewHostMsg_GetCookies(
      sender_->GetRoutingID(), url, first_party_for_cookies, &value_utf8));
  return WebString::fromUTF8(value_utf8);
}

WebString RendererWebCookieJarImpl::cookieRequestHeaderFieldValue(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  return cookies(url, first_party_for_cookies);
}

void RendererWebCookieJarImpl::rawCookies(
    const WebURL& url, const WebURL& first_party_for_cookies,
    WebVector<WebCookie>& raw_cookies) {
  std::vector<CookieData> cookies;
  sender_->Send(new ViewHostMsg_GetRawCookies(
      url, first_party_for_cookies, &cookies));

  WebVector<WebCookie> result(cookies.size());
  for (size_t i = 0; i < cookies.size(); ++i) {
    const CookieData& c = cookies[i];
    result[i] = WebCookie(WebString::fromUTF8(c.name),
                          WebString::fromUTF8(c.value),
                          WebString::fromUTF8(c.domain),
                          WebString::fromUTF8(c.path),
                          c.expires,
                          c.http_only,
                          c.secure,
                          c.session);
  }
  raw_cookies.swap(result);
}

void RendererWebCookieJarImpl::deleteCookie(
    const WebURL& url, const WebString& cookie_name) {
  std::string cookie_name_utf8 = base::UTF16ToUTF8(cookie_name);
  sender_->Send(new ViewHostMsg_DeleteCookie(url, cookie_name_utf8));
}

bool RendererWebCookieJarImpl::cookiesEnabled(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  bool cookies_enabled;
  sender_->Send(new ViewHostMsg_CookiesEnabled(
      sender_->GetRoutingID(), url, first_party_for_cookies, &cookies_enabled));
  return cookies_enabled;
}

}  // namespace content
