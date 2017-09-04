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
  void setCookie(const blink::WebURL& url,
                 const blink::WebURL& first_party_for_cookies,
                 const blink::WebString& value) override;
  blink::WebString cookies(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies) override;
  bool cookiesEnabled(const blink::WebURL& url,
                      const blink::WebURL& first_party_for_cookies) override;

  RenderFrameImpl* sender_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_WEBCOOKIEJAR_IMPL_H_
