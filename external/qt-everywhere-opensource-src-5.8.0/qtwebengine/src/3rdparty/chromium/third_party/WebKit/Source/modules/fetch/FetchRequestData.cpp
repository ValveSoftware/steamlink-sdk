// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchRequestData.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/ThreadableLoader.h"
#include "core/streams/ReadableStream.h"
#include "modules/credentialmanager/PasswordCredential.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchHeaderList.h"
#include "platform/HTTPNames.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

namespace blink {

FetchRequestData* FetchRequestData::create()
{
    return new FetchRequestData();
}

FetchRequestData* FetchRequestData::create(ScriptState* scriptState, const WebServiceWorkerRequest& webRequest)
{
    FetchRequestData* request = FetchRequestData::create();
    request->m_url = webRequest.url();
    request->m_method = webRequest.method();
    for (HTTPHeaderMap::const_iterator it = webRequest.headers().begin(); it != webRequest.headers().end(); ++it)
        request->m_headerList->append(it->key, it->value);
    if (webRequest.blobDataHandle())
        request->setBuffer(new BodyStreamBuffer(scriptState, FetchBlobDataConsumerHandle::create(scriptState->getExecutionContext(), webRequest.blobDataHandle())));
    request->setContext(webRequest.requestContext());
    request->setReferrer(Referrer(webRequest.referrerUrl().string(), static_cast<ReferrerPolicy>(webRequest.referrerPolicy())));
    request->setMode(webRequest.mode());
    request->setCredentials(webRequest.credentialsMode());
    request->setRedirect(webRequest.redirectMode());
    request->setMIMEType(request->m_headerList->extractMIMEType());
    return request;
}

FetchRequestData* FetchRequestData::cloneExceptBody()
{
    FetchRequestData* request = FetchRequestData::create();
    request->m_url = m_url;
    request->m_method = m_method;
    request->m_headerList = m_headerList->clone();
    request->m_unsafeRequestFlag = m_unsafeRequestFlag;
    request->m_origin = m_origin;
    request->m_sameOriginDataURLFlag = m_sameOriginDataURLFlag;
    request->m_context = m_context;
    request->m_referrer = m_referrer;
    request->m_mode = m_mode;
    request->m_credentials = m_credentials;
    request->m_redirect = m_redirect;
    request->m_responseTainting = m_responseTainting;
    request->m_mimeType = m_mimeType;
    request->m_integrity = m_integrity;
    request->m_attachedCredential = m_attachedCredential;
    return request;
}

FetchRequestData* FetchRequestData::clone(ScriptState* scriptState)
{
    FetchRequestData* request = FetchRequestData::cloneExceptBody();
    if (m_buffer) {
        BodyStreamBuffer* new1 = nullptr;
        BodyStreamBuffer* new2 = nullptr;
        m_buffer->tee(&new1, &new2);
        m_buffer = new1;
        request->m_buffer = new2;
    }
    return request;
}

FetchRequestData* FetchRequestData::pass(ScriptState* scriptState)
{
    FetchRequestData* request = FetchRequestData::cloneExceptBody();
    if (m_buffer) {
        request->m_buffer = m_buffer;
        m_buffer = new BodyStreamBuffer(scriptState, createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle()));
        m_buffer->closeAndLockAndDisturb();
    }
    return request;
}

FetchRequestData::~FetchRequestData()
{
}

FetchRequestData::FetchRequestData()
    : m_method(HTTPNames::GET)
    , m_headerList(FetchHeaderList::create())
    , m_unsafeRequestFlag(false)
    , m_context(WebURLRequest::RequestContextUnspecified)
    , m_sameOriginDataURLFlag(false)
    , m_referrer(Referrer(clientReferrerString(), ReferrerPolicyDefault))
    , m_mode(WebURLRequest::FetchRequestModeNoCORS)
    , m_credentials(WebURLRequest::FetchCredentialsModeOmit)
    , m_redirect(WebURLRequest::FetchRedirectModeFollow)
    , m_responseTainting(BasicTainting)
{
}


void FetchRequestData::setCredentials(WebURLRequest::FetchCredentialsMode credentials)
{
    m_credentials = credentials;
    if (m_credentials != WebURLRequest::FetchCredentialsModePassword)
        m_attachedCredential.clear();
}

DEFINE_TRACE(FetchRequestData)
{
    visitor->trace(m_buffer);
    visitor->trace(m_headerList);
}

} // namespace blink
