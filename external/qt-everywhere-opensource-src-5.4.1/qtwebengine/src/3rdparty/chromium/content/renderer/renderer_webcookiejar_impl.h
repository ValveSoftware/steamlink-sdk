// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_WEBCOOKIEJAR_IMPL_H_
#define CONTENT_RENDERER_RENDERER_WEBCOOKIEJAR_IMPL_H_

// TODO(darin): WebCookieJar.h is missing a WebString.h include!
#include "third_party/WebKit/public/platform/WebCookieJar.h"

namespace content {
class RenderFrameImpl;

class RendererWebCookieJarImpl : public blink::WebCookieJar {
 public:
  explicit RendererWebCookieJarImpl(RenderFrameImpl* sender)
      : sender_(sender) {
  }
  virtual ~RendererWebCookieJarImpl() {}

 private:
  // blink::WebCookieJar methods:
  virtual void setCookie(
      const blink::WebURL& url, const blink::WebURL& first_party_for_cookies,
      const blink::WebString& value);
  virtual blink::WebString cookies(
      const blink::WebURL& url, const blink::WebURL& first_party_for_cookies);
  virtual blink::WebString cookieRequestHeaderFieldValue(
      const blink::WebURL& url, const blink::WebURL& first_party_for_cookies);
  virtual void rawCookies(
      const blink::WebURL& url, const blink::WebURL& first_party_for_cookies,
      blink::WebVector<blink::WebCookie>& cookies);
  virtual void deleteCookie(
      const blink::WebURL& url, const blink::WebString& cookie_name);
  virtual bool cookiesEnabled(
      const blink::WebURL& url, const blink::WebURL& first_party_for_cookies);

  RenderFrameImpl* sender_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBCOOKIEJAR_IMPL_H_
