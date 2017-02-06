/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/DOMURL.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/URLSearchParams.h"
#include "core/fetch/MemoryCache.h"
#include "core/fileapi/Blob.h"
#include "core/html/PublicURLManager.h"
#include "platform/blob/BlobURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/TemporaryChange.h"

namespace blink {

DOMURL::DOMURL(const String& url, const KURL& base, ExceptionState& exceptionState)
{
    if (!base.isValid()) {
        exceptionState.throwTypeError("Invalid base URL");
        return;
    }

    m_url = KURL(base, url);
    if (!m_url.isValid())
        exceptionState.throwTypeError("Invalid URL");
}

DOMURL::~DOMURL()
{
}

DEFINE_TRACE(DOMURL)
{
    visitor->trace(m_searchParams);
}

void DOMURL::setInput(const String& value)
{
    KURL url(blankURL(), value);
    if (url.isValid()) {
        m_url = url;
        m_input = String();
    } else {
        m_url = KURL();
        m_input = value;
    }
    update();
}

void DOMURL::setSearch(const String& value)
{
    DOMURLUtils::setSearch(value);
    if (!value.isEmpty() && value[0] == '?')
        updateSearchParams(value.substring(1));
    else
        updateSearchParams(value);
}

String DOMURL::createPublicURL(ExecutionContext* executionContext, URLRegistrable* registrable, const String& uuid)
{
    KURL publicURL = BlobURL::createPublicURL(executionContext->getSecurityOrigin());
    if (publicURL.isEmpty())
        return String();

    executionContext->publicURLManager().registerURL(executionContext->getSecurityOrigin(), publicURL, registrable, uuid);

    return publicURL.getString();
}

void DOMURL::revokeObjectUUID(ExecutionContext* executionContext, const String& uuid)
{
    if (!executionContext)
        return;

    executionContext->publicURLManager().revoke(uuid);
}

URLSearchParams* DOMURL::searchParams()
{
    if (!m_searchParams)
        m_searchParams = URLSearchParams::create(url().query(), this);

    return m_searchParams;
}

void DOMURL::update()
{
    updateSearchParams(url().query());
}

void DOMURL::updateSearchParams(const String& queryString)
{
    if (!m_searchParams)
        return;

    TemporaryChange<bool> scope(m_isInUpdate, true);
    ASSERT(m_searchParams->urlObject() == this);
    m_searchParams->setInput(queryString);
}

} // namespace blink
