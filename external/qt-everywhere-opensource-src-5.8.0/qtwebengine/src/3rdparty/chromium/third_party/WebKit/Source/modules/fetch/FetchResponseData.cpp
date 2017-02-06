// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchResponseData.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/fetch/FetchUtils.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchHeaderList.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

WebServiceWorkerResponseType fetchTypeToWebType(FetchResponseData::Type fetchType)
{
    WebServiceWorkerResponseType webType = WebServiceWorkerResponseTypeDefault;
    switch (fetchType) {
    case FetchResponseData::BasicType:
        webType = WebServiceWorkerResponseTypeBasic;
        break;
    case FetchResponseData::CORSType:
        webType = WebServiceWorkerResponseTypeCORS;
        break;
    case FetchResponseData::DefaultType:
        webType = WebServiceWorkerResponseTypeDefault;
        break;
    case FetchResponseData::ErrorType:
        webType = WebServiceWorkerResponseTypeError;
        break;
    case FetchResponseData::OpaqueType:
        webType = WebServiceWorkerResponseTypeOpaque;
        break;
    case FetchResponseData::OpaqueRedirectType:
        webType = WebServiceWorkerResponseTypeOpaqueRedirect;
        break;
    }
    return webType;
}

WebVector<WebString> headerSetToWebVector(const HTTPHeaderSet& headers)
{
    // Can't just pass *headers to the WebVector constructor because HashSet
    // iterators are not stl iterator compatible.
    WebVector<WebString> result(static_cast<size_t>(headers.size()));
    int idx = 0;
    for (const auto& header : headers)
        result[idx++] = header;
    return result;
}

} // namespace

FetchResponseData* FetchResponseData::create()
{
    // "Unless stated otherwise, a response's url is null, status is 200, status
    // message is `OK`, header list is an empty header list, and body is null."
    return new FetchResponseData(DefaultType, 200, "OK");
}

FetchResponseData* FetchResponseData::createNetworkErrorResponse()
{
    // "A network error is a response whose status is always 0, status message
    // is always the empty byte sequence, header list is aways an empty list,
    // and body is always null."
    return new FetchResponseData(ErrorType, 0, "");
}

FetchResponseData* FetchResponseData::createWithBuffer(BodyStreamBuffer* buffer)
{
    FetchResponseData* response = FetchResponseData::create();
    response->m_buffer = buffer;
    return response;
}

FetchResponseData* FetchResponseData::createBasicFilteredResponse() const
{
    DCHECK_EQ(m_type, DefaultType);
    // "A basic filtered response is a filtered response whose type is |basic|,
    // header list excludes any headers in internal response's header list whose
    // name is `Set-Cookie` or `Set-Cookie2`."
    FetchResponseData* response = new FetchResponseData(BasicType, m_status, m_statusMessage);
    response->m_url = m_url;
    for (size_t i = 0; i < m_headerList->size(); ++i) {
        const FetchHeaderList::Header* header = m_headerList->list()[i].get();
        if (FetchUtils::isForbiddenResponseHeaderName(header->first))
            continue;
        response->m_headerList->append(header->first, header->second);
    }
    response->m_buffer = m_buffer;
    response->m_mimeType = m_mimeType;
    response->m_internalResponse = const_cast<FetchResponseData*>(this);
    return response;
}

FetchResponseData* FetchResponseData::createCORSFilteredResponse() const
{
    DCHECK_EQ(m_type, DefaultType);
    HTTPHeaderSet accessControlExposeHeaderSet;
    String accessControlExposeHeaders;
    if (m_headerList->get(HTTPNames::Access_Control_Expose_Headers, accessControlExposeHeaders))
        parseAccessControlExposeHeadersAllowList(accessControlExposeHeaders, accessControlExposeHeaderSet);
    return createCORSFilteredResponse(accessControlExposeHeaderSet);
}

FetchResponseData* FetchResponseData::createCORSFilteredResponse(const HTTPHeaderSet& exposedHeaders) const
{
    DCHECK_EQ(m_type, DefaultType);
    // "A CORS filtered response is a filtered response whose type is |CORS|,
    // header list excludes all headers in internal response's header list,
    // except those whose name is either one of `Cache-Control`,
    // `Content-Language`, `Content-Type`, `Expires`, `Last-Modified`, and
    // `Pragma`, and except those whose name is one of the values resulting from
    // parsing `Access-Control-Expose-Headers` in internal response's header
    // list."
    FetchResponseData* response = new FetchResponseData(CORSType, m_status, m_statusMessage);
    response->m_url = m_url;
    for (size_t i = 0; i < m_headerList->size(); ++i) {
        const FetchHeaderList::Header* header = m_headerList->list()[i].get();
        const String& name = header->first;
        const bool explicitlyExposed = exposedHeaders.contains(name);
        if (isOnAccessControlResponseHeaderWhitelist(name) || (explicitlyExposed && !FetchUtils::isForbiddenResponseHeaderName(name))) {
            if (explicitlyExposed)
                response->m_corsExposedHeaderNames.add(name);
            response->m_headerList->append(name, header->second);
        }
    }
    response->m_buffer = m_buffer;
    response->m_mimeType = m_mimeType;
    response->m_internalResponse = const_cast<FetchResponseData*>(this);
    return response;
}

FetchResponseData* FetchResponseData::createOpaqueFilteredResponse() const
{
    DCHECK_EQ(m_type, DefaultType);
    // "An opaque filtered response is a filtered response whose type is
    // 'opaque', url list is the empty list, status is 0, status message is the
    // empty byte sequence, header list is the empty list, body is null, and
    // cache state is 'none'."
    //
    // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque
    FetchResponseData* response = new FetchResponseData(OpaqueType, 0, "");
    response->m_internalResponse = const_cast<FetchResponseData*>(this);
    return response;
}

FetchResponseData* FetchResponseData::createOpaqueRedirectFilteredResponse() const
{
    DCHECK_EQ(m_type, DefaultType);
    // "An opaque filtered response is a filtered response whose type is
    // 'opaqueredirect', status is 0, status message is the empty byte sequence,
    // header list is the empty list, body is null, and cache state is 'none'."
    //
    // https://fetch.spec.whatwg.org/#concept-filtered-response-opaque-redirect
    FetchResponseData* response = new FetchResponseData(OpaqueRedirectType, 0, "");
    response->m_url = m_url;
    response->m_internalResponse = const_cast<FetchResponseData*>(this);
    return response;
}

String FetchResponseData::mimeType() const
{
    return m_mimeType;
}

BodyStreamBuffer* FetchResponseData::internalBuffer() const
{
    if (m_internalResponse) {
        return m_internalResponse->m_buffer;
    }
    return m_buffer;
}

String FetchResponseData::internalMIMEType() const
{
    if (m_internalResponse) {
        return m_internalResponse->mimeType();
    }
    return m_mimeType;
}

FetchResponseData* FetchResponseData::clone(ScriptState* scriptState)
{
    FetchResponseData* newResponse = create();
    newResponse->m_type = m_type;
    if (m_terminationReason) {
        newResponse->m_terminationReason = wrapUnique(new TerminationReason);
        *newResponse->m_terminationReason = *m_terminationReason;
    }
    newResponse->m_url = m_url;
    newResponse->m_status = m_status;
    newResponse->m_statusMessage = m_statusMessage;
    newResponse->m_headerList = m_headerList->clone();
    newResponse->m_mimeType = m_mimeType;
    newResponse->m_responseTime = m_responseTime;
    newResponse->m_cacheStorageCacheName = m_cacheStorageCacheName;
    newResponse->m_corsExposedHeaderNames = m_corsExposedHeaderNames;

    switch (m_type) {
    case BasicType:
    case CORSType:
        ASSERT(m_internalResponse);
        ASSERT(m_buffer == m_internalResponse->m_buffer);
        ASSERT(m_internalResponse->m_type == DefaultType);
        newResponse->m_internalResponse = m_internalResponse->clone(scriptState);
        m_buffer = m_internalResponse->m_buffer;
        newResponse->m_buffer = newResponse->m_internalResponse->m_buffer;
        break;
    case DefaultType: {
        ASSERT(!m_internalResponse);
        if (m_buffer) {
            BodyStreamBuffer* new1 = nullptr;
            BodyStreamBuffer* new2 = nullptr;
            m_buffer->tee(&new1, &new2);
            m_buffer = new1;
            newResponse->m_buffer = new2;
        }
        break;
    }
    case ErrorType:
        ASSERT(!m_internalResponse);
        ASSERT(!m_buffer);
        break;
    case OpaqueType:
    case OpaqueRedirectType:
        ASSERT(m_internalResponse);
        ASSERT(!m_buffer);
        ASSERT(m_internalResponse->m_type == DefaultType);
        newResponse->m_internalResponse = m_internalResponse->clone(scriptState);
        break;
    }
    return newResponse;
}

void FetchResponseData::populateWebServiceWorkerResponse(WebServiceWorkerResponse& response)
{
    if (m_internalResponse) {
        m_internalResponse->populateWebServiceWorkerResponse(response);
        response.setResponseType(fetchTypeToWebType(m_type));
        response.setCorsExposedHeaderNames(headerSetToWebVector(m_corsExposedHeaderNames));
        return;
    }

    response.setURL(url());
    response.setStatus(status());
    response.setStatusText(statusMessage());
    response.setResponseType(fetchTypeToWebType(m_type));
    response.setResponseTime(responseTime());
    response.setCacheStorageCacheName(cacheStorageCacheName());
    response.setCorsExposedHeaderNames(headerSetToWebVector(m_corsExposedHeaderNames));
    for (size_t i = 0; i < headerList()->size(); ++i) {
        const FetchHeaderList::Header* header = headerList()->list()[i].get();
        response.appendHeader(header->first, header->second);
    }
}

FetchResponseData::FetchResponseData(Type type, unsigned short status, AtomicString statusMessage)
    : m_type(type)
    , m_status(status)
    , m_statusMessage(statusMessage)
    , m_headerList(FetchHeaderList::create())
    , m_responseTime(0)
{
}

void FetchResponseData::replaceBodyStreamBuffer(BodyStreamBuffer* buffer)
{
    if (m_type == BasicType || m_type == CORSType) {
        ASSERT(m_internalResponse);
        m_internalResponse->m_buffer = buffer;
        m_buffer = buffer;
    } else if (m_type == DefaultType) {
        ASSERT(!m_internalResponse);
        m_buffer = buffer;
    }
}

DEFINE_TRACE(FetchResponseData)
{
    visitor->trace(m_headerList);
    visitor->trace(m_internalResponse);
    visitor->trace(m_buffer);
}

} // namespace blink
