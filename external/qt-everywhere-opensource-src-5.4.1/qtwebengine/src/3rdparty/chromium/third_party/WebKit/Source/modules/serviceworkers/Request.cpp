// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Request.h"

#include "bindings/v8/Dictionary.h"
#include "core/dom/DOMURLUtilsReadOnly.h"
#include "modules/serviceworkers/RequestInit.h"
#include "platform/NotImplemented.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebServiceWorkerRequest.h"

namespace WebCore {

PassRefPtr<Request> Request::create()
{
    return create(Dictionary());
}

PassRefPtr<Request> Request::create(const Dictionary& requestInit)
{
    return adoptRef(new Request(RequestInit(requestInit)));
}

PassRefPtr<Request> Request::create(const blink::WebServiceWorkerRequest& webRequest)
{
    return adoptRef(new Request(webRequest));
}

void Request::setURL(const String& value)
{
    m_url = KURL(ParsedURLString, value);
}

void Request::setMethod(const String& value)
{
    m_method = value;
}

String Request::origin() const
{
    return DOMURLUtilsReadOnly::origin(m_url);
}

PassOwnPtr<ResourceRequest> Request::createResourceRequest() const
{
    OwnPtr<ResourceRequest> request = adoptPtr(new ResourceRequest(m_url));
    request->setHTTPMethod("GET");
    // FIXME: Fill more info.
    return request.release();
}

Request::Request(const RequestInit& requestInit)
    : m_url(KURL(ParsedURLString, requestInit.url))
    , m_method(requestInit.method)
    , m_headers(requestInit.headers)
{
    ScriptWrappable::init(this);

    if (!m_headers)
        m_headers = HeaderMap::create();
}

Request::Request(const blink::WebServiceWorkerRequest& webRequest)
    : m_url(webRequest.url())
    , m_method(webRequest.method())
    , m_headers(HeaderMap::create(webRequest.headers()))
{
    ScriptWrappable::init(this);
}

} // namespace WebCore
