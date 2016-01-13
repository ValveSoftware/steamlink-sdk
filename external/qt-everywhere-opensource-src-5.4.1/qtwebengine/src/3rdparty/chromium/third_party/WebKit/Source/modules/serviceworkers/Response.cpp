// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Response.h"

#include "bindings/v8/Dictionary.h"
#include "core/fileapi/Blob.h"
#include "modules/serviceworkers/ResponseInit.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace WebCore {

PassRefPtr<Response> Response::create(Blob* body, const Dictionary& responseInit)
{
    RefPtr<BlobDataHandle> blobDataHandle = body ? body->blobDataHandle() : nullptr;

    // FIXME: Maybe append or override content-length and content-type headers using the blob. The spec will clarify what to do:
    // https://github.com/slightlyoff/ServiceWorker/issues/192
    return adoptRef(new Response(blobDataHandle.release(), ResponseInit(responseInit)));
}

PassRefPtr<HeaderMap> Response::headers() const
{
    // FIXME: Implement. Spec will eventually whitelist allowable headers.
    return m_headers;
}

void Response::populateWebServiceWorkerResponse(blink::WebServiceWorkerResponse& response)
{
    response.setStatus(status());
    response.setStatusText(statusText());
    response.setHeaders(m_headers->headerMap());
    response.setBlobDataHandle(m_blobDataHandle);
}

Response::Response(PassRefPtr<BlobDataHandle> blobDataHandle, const ResponseInit& responseInit)
    : m_status(responseInit.status)
    , m_statusText(responseInit.statusText)
    , m_headers(responseInit.headers)
    , m_blobDataHandle(blobDataHandle)
{
    ScriptWrappable::init(this);

    if (!m_headers)
        m_headers = HeaderMap::create();
}

} // namespace WebCore
