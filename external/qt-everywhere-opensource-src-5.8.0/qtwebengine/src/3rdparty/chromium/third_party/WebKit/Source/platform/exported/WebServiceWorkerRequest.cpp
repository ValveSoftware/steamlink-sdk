// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

#include "platform/blob/BlobData.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/RefCounted.h"

namespace blink {

class WebServiceWorkerRequestPrivate : public RefCounted<WebServiceWorkerRequestPrivate> {
public:
    WebServiceWorkerRequestPrivate()
        : m_mode(WebURLRequest::FetchRequestModeNoCORS)
        , m_isMainResourceLoad(false)
        , m_credentialsMode(WebURLRequest::FetchCredentialsModeOmit)
        , m_redirectMode(WebURLRequest::FetchRedirectModeFollow)
        , m_requestContext(WebURLRequest::RequestContextUnspecified)
        , m_frameType(WebURLRequest::FrameTypeNone)
        , m_clientId(WebString())
        , m_isReload(false)
    {
    }
    WebURL m_url;
    WebString m_method;
    HTTPHeaderMap m_headers;
    RefPtr<BlobDataHandle> blobDataHandle;
    Referrer m_referrer;
    WebURLRequest::FetchRequestMode m_mode;
    bool m_isMainResourceLoad;
    WebURLRequest::FetchCredentialsMode m_credentialsMode;
    WebURLRequest::FetchRedirectMode m_redirectMode;
    WebURLRequest::RequestContext m_requestContext;
    WebURLRequest::FrameType m_frameType;
    WebString m_clientId;
    bool m_isReload;
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
    m_private->m_url = url;
}

WebURL WebServiceWorkerRequest::url() const
{
    return m_private->m_url;
}

void WebServiceWorkerRequest::setMethod(const WebString& method)
{
    m_private->m_method = method;
}

WebString WebServiceWorkerRequest::method() const
{
    return m_private->m_method;
}

void WebServiceWorkerRequest::setHeader(const WebString& key, const WebString& value)
{
    if (equalIgnoringCase(key, "referer"))
        return;
    m_private->m_headers.set(key, value);
}

void WebServiceWorkerRequest::appendHeader(const WebString& key, const WebString& value)
{
    if (equalIgnoringCase(key, "referer"))
        return;
    HTTPHeaderMap::AddResult result = m_private->m_headers.add(key, value);
    if (!result.isNewEntry)
        result.storedValue->value = result.storedValue->value + ", " + String(value);
}

void WebServiceWorkerRequest::visitHTTPHeaderFields(WebHTTPHeaderVisitor* headerVisitor) const
{
    for (HTTPHeaderMap::const_iterator i = m_private->m_headers.begin(), end = m_private->m_headers.end(); i != end; ++i)
        headerVisitor->visitHeader(i->key, i->value);
}

const HTTPHeaderMap& WebServiceWorkerRequest::headers() const
{
    return m_private->m_headers;
}

void WebServiceWorkerRequest::setBlob(const WebString& uuid, long long size)
{
    m_private->blobDataHandle = BlobDataHandle::create(uuid, String(), size);
}

PassRefPtr<BlobDataHandle> WebServiceWorkerRequest::blobDataHandle() const
{
    return m_private->blobDataHandle;
}

void WebServiceWorkerRequest::setReferrer(const WebString& webReferrer, WebReferrerPolicy referrerPolicy)
{
    // WebString doesn't have the distinction between empty and null. We use
    // the null WTFString for referrer.
    ASSERT(Referrer::noReferrer() == String());
    String referrer = webReferrer.isEmpty() ? Referrer::noReferrer() : String(webReferrer);
    m_private->m_referrer = Referrer(referrer, static_cast<ReferrerPolicy>(referrerPolicy));
}

WebURL WebServiceWorkerRequest::referrerUrl() const
{
    return KURL(ParsedURLString, m_private->m_referrer.referrer);
}

WebReferrerPolicy WebServiceWorkerRequest::referrerPolicy() const
{
    return static_cast<WebReferrerPolicy>(m_private->m_referrer.referrerPolicy);
}

const Referrer& WebServiceWorkerRequest::referrer() const
{
    return m_private->m_referrer;
}

void WebServiceWorkerRequest::setMode(WebURLRequest::FetchRequestMode mode)
{
    m_private->m_mode = mode;
}

WebURLRequest::FetchRequestMode WebServiceWorkerRequest::mode() const
{
    return m_private->m_mode;
}

void WebServiceWorkerRequest::setIsMainResourceLoad(bool isMainResourceLoad)
{
    m_private->m_isMainResourceLoad = isMainResourceLoad;
}

bool WebServiceWorkerRequest::isMainResourceLoad() const
{
    return m_private->m_isMainResourceLoad;
}

void WebServiceWorkerRequest::setCredentialsMode(WebURLRequest::FetchCredentialsMode credentialsMode)
{
    m_private->m_credentialsMode = credentialsMode;
}

WebURLRequest::FetchCredentialsMode WebServiceWorkerRequest::credentialsMode() const
{
    return m_private->m_credentialsMode;
}

void WebServiceWorkerRequest::setRedirectMode(WebURLRequest::FetchRedirectMode redirectMode)
{
    m_private->m_redirectMode = redirectMode;
}

WebURLRequest::FetchRedirectMode WebServiceWorkerRequest::redirectMode() const
{
    return m_private->m_redirectMode;
}

void WebServiceWorkerRequest::setRequestContext(WebURLRequest::RequestContext requestContext)
{
    m_private->m_requestContext = requestContext;
}

WebURLRequest::RequestContext WebServiceWorkerRequest::requestContext() const
{
    return m_private->m_requestContext;
}

void WebServiceWorkerRequest::setFrameType(WebURLRequest::FrameType frameType)
{
    m_private->m_frameType = frameType;
}

WebURLRequest::FrameType WebServiceWorkerRequest::frameType() const
{
    return m_private->m_frameType;
}

void WebServiceWorkerRequest::setClientId(const WebString& clientId)
{
    m_private->m_clientId = clientId;
}

WebString WebServiceWorkerRequest::clientId() const
{
    return m_private->m_clientId;
}

void WebServiceWorkerRequest::setIsReload(bool isReload)
{
    m_private->m_isReload = isReload;
}

bool WebServiceWorkerRequest::isReload() const
{
    return m_private->m_isReload;
}

} // namespace blink
