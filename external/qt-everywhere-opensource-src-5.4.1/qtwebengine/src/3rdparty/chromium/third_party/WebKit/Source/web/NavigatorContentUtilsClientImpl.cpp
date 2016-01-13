// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/NavigatorContentUtilsClientImpl.h"

#include "public/web/WebViewClient.h"
#include "web/WebViewImpl.h"

using namespace WebCore;

namespace blink {

PassOwnPtr<NavigatorContentUtilsClientImpl> NavigatorContentUtilsClientImpl::create(WebViewImpl* webView)
{
    return adoptPtr(new NavigatorContentUtilsClientImpl(webView));
}

NavigatorContentUtilsClientImpl::NavigatorContentUtilsClientImpl(WebViewImpl* webView)
    : m_webView(webView)
{
}

void NavigatorContentUtilsClientImpl::registerProtocolHandler(const String& scheme, const WebCore::KURL& baseURL, const WebCore::KURL& url, const String& title)
{
    m_webView->client()->registerProtocolHandler(scheme, baseURL, url, title);
}

NavigatorContentUtilsClient::CustomHandlersState NavigatorContentUtilsClientImpl::isProtocolHandlerRegistered(const String& scheme, const WebCore::KURL& baseURL, const WebCore::KURL& url)
{
    return static_cast<NavigatorContentUtilsClient::CustomHandlersState>(m_webView->client()->isProtocolHandlerRegistered(scheme, baseURL, url));
}

void NavigatorContentUtilsClientImpl::unregisterProtocolHandler(const String& scheme, const WebCore::KURL& baseURL, const WebCore::KURL& url)
{
    m_webView->client()->unregisterProtocolHandler(scheme, baseURL, url);
}

} // namespace blink

