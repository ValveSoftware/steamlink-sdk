/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/workers/WorkerThreadStartupData.h"

#include "platform/network/ContentSecurityPolicyParsers.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

WorkerThreadStartupData::WorkerThreadStartupData(const KURL& scriptURL, const String& userAgent, const String& sourceCode, std::unique_ptr<Vector<char>> cachedMetaData, WorkerThreadStartMode startMode, const Vector<CSPHeaderAndType>* contentSecurityPolicyHeaders, const String& referrerPolicy, const SecurityOrigin* starterOrigin, WorkerClients* workerClients, WebAddressSpace addressSpace, const Vector<String>* originTrialTokens, V8CacheOptions v8CacheOptions)
    : m_scriptURL(scriptURL.copy())
    , m_userAgent(userAgent.isolatedCopy())
    , m_sourceCode(sourceCode.isolatedCopy())
    , m_cachedMetaData(std::move(cachedMetaData))
    , m_startMode(startMode)
    , m_referrerPolicy(referrerPolicy.isolatedCopy())
    , m_starterOriginPrivilegeData(starterOrigin ? starterOrigin->createPrivilegeData() : nullptr)
    , m_workerClients(workerClients)
    , m_addressSpace(addressSpace)
    , m_v8CacheOptions(v8CacheOptions)
{
    m_contentSecurityPolicyHeaders = wrapUnique(new Vector<CSPHeaderAndType>());
    if (contentSecurityPolicyHeaders) {
        for (const auto& header : *contentSecurityPolicyHeaders) {
            CSPHeaderAndType copiedHeader(header.first.isolatedCopy(), header.second);
            m_contentSecurityPolicyHeaders->append(copiedHeader);
        }
    }

    m_originTrialTokens = std::unique_ptr<Vector<String>>(new Vector<String>());
    if (originTrialTokens) {
        for (const String& token : *originTrialTokens)
            m_originTrialTokens->append(token.isolatedCopy());
    }
}

WorkerThreadStartupData::~WorkerThreadStartupData()
{
}

} // namespace blink
