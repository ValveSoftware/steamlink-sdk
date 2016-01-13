// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorContentUtilsClientImpl_h
#define NavigatorContentUtilsClientImpl_h

#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class WebViewImpl;

class NavigatorContentUtilsClientImpl FINAL : public WebCore::NavigatorContentUtilsClient {
public:
    static PassOwnPtr<NavigatorContentUtilsClientImpl> create(WebViewImpl*);
    virtual ~NavigatorContentUtilsClientImpl() { }

    virtual void registerProtocolHandler(const String& scheme, const WebCore::KURL& baseURL, const WebCore::KURL&, const String& title) OVERRIDE;
    virtual CustomHandlersState isProtocolHandlerRegistered(const String& scheme, const WebCore::KURL& baseURL, const WebCore::KURL&) OVERRIDE;
    virtual void unregisterProtocolHandler(const String& scheme, const WebCore::KURL& baseURL, const WebCore::KURL&) OVERRIDE;

private:
    explicit NavigatorContentUtilsClientImpl(WebViewImpl*);

    WebViewImpl* m_webView;
};

} // namespace blink

#endif // NavigatorContentUtilsClientImpl_h
