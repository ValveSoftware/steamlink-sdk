// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebServiceWorkerResponse.h"

#include "platform/blob/BlobData.h"

namespace blink {

class WebServiceWorkerResponsePrivate : public RefCounted<WebServiceWorkerResponsePrivate> {
public:
    unsigned short status;
    WebString statusText;
    HashMap<String, String> headers;
    RefPtr<WebCore::BlobDataHandle> blobDataHandle;
};

WebServiceWorkerResponse::WebServiceWorkerResponse()
    : m_private(adoptRef(new WebServiceWorkerResponsePrivate))
{
}

void WebServiceWorkerResponse::reset()
{
    m_private.reset();
}

void WebServiceWorkerResponse::assign(const WebServiceWorkerResponse& other)
{
    m_private = other.m_private;
}

void WebServiceWorkerResponse::setStatus(unsigned short status)
{
    m_private->status = status;
}

unsigned short WebServiceWorkerResponse::status() const
{
    return m_private->status;
}

void WebServiceWorkerResponse::setStatusText(const WebString& statusText)
{
    m_private->statusText = statusText;
}

WebString WebServiceWorkerResponse::statusText() const
{
    return m_private->statusText;
}

void WebServiceWorkerResponse::setHeader(const WebString& key, const WebString& value)
{
    m_private->headers.set(key, value);
}

WebVector<WebString> WebServiceWorkerResponse::getHeaderKeys() const
{
    Vector<String> keys;
    copyKeysToVector(m_private->headers, keys);
    return keys;
}

WebString WebServiceWorkerResponse::getHeader(const WebString& key) const
{
    return m_private->headers.get(key);
}

WebString WebServiceWorkerResponse::blobUUID() const
{
    if (!m_private->blobDataHandle)
        return WebString();
    return m_private->blobDataHandle->uuid();
}

void WebServiceWorkerResponse::setHeaders(const HashMap<String, String>& headers)
{
    m_private->headers = headers;
}

const HashMap<String, String>& WebServiceWorkerResponse::headers() const
{
    return m_private->headers;
}

void WebServiceWorkerResponse::setBlobDataHandle(PassRefPtr<WebCore::BlobDataHandle> blobDataHandle)
{
    m_private->blobDataHandle = blobDataHandle;
}

PassRefPtr<WebCore::BlobDataHandle> WebServiceWorkerResponse::blobDataHandle() const
{
    return m_private->blobDataHandle;
}

} // namespace blink
