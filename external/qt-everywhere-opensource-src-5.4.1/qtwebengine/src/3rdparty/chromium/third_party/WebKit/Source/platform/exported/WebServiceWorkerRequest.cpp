// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebServiceWorkerRequest.h"

namespace blink {

class WebServiceWorkerRequestPrivate : public RefCounted<WebServiceWorkerRequestPrivate> {
public:
    WebURL url;
    WebString method;
    HashMap<String, String> headers;
};

WebServiceWorkerRequest::WebServiceWorkerRequest()
    : m_private(adoptRef(new WebServiceWorkerRequestPrivate))
{
}

void WebServiceWorkerRequest::reset()
{
    m_private.reset();
}

void WebServiceWorkerRequest::assign(const WebServiceWorkerRequest& other)
{
    m_private = other.m_private;
}

void WebServiceWorkerRequest::setURL(const WebURL& url)
{
    m_private->url = url;
}

WebURL WebServiceWorkerRequest::url() const
{
    return m_private->url;
}

void WebServiceWorkerRequest::setMethod(const WebString& method)
{
    m_private->method = method;
}

WebString WebServiceWorkerRequest::method() const
{
    return m_private->method;
}

void WebServiceWorkerRequest::setHeader(const WebString& key, const WebString& value)
{
    m_private->headers.set(key, value);
}

const HashMap<String, String>& WebServiceWorkerRequest::headers() const
{
    return m_private->headers;
}

} // namespace blink
