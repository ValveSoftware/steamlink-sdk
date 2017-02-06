/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/SecurityContext.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

SecurityContext::SecurityContext()
    : m_sandboxFlags(SandboxNone)
    , m_addressSpace(WebAddressSpacePublic)
    , m_insecureRequestPolicy(kLeaveInsecureRequestsAlone)
{
}

SecurityContext::~SecurityContext()
{
}

DEFINE_TRACE(SecurityContext)
{
    visitor->trace(m_contentSecurityPolicy);
}

void SecurityContext::setSecurityOrigin(PassRefPtr<SecurityOrigin> securityOrigin)
{
    m_securityOrigin = securityOrigin;
}

void SecurityContext::setContentSecurityPolicy(ContentSecurityPolicy* contentSecurityPolicy)
{
    m_contentSecurityPolicy = contentSecurityPolicy;
}

void SecurityContext::enforceSandboxFlags(SandboxFlags mask)
{
    applySandboxFlags(mask);
}

void SecurityContext::applySandboxFlags(SandboxFlags mask)
{
    m_sandboxFlags |= mask;

    if (isSandboxed(SandboxOrigin) && getSecurityOrigin() && !getSecurityOrigin()->isUnique()) {
        setSecurityOrigin(SecurityOrigin::createUnique());
        didUpdateSecurityOrigin();
    }
}

String SecurityContext::addressSpaceForBindings() const
{
    switch (m_addressSpace) {
    case WebAddressSpacePublic:
        return "public";

    case WebAddressSpacePrivate:
        return "private";

    case WebAddressSpaceLocal:
        return "local";
    }
    ASSERT_NOT_REACHED();
    return "public";
}

// Enforces the given suborigin as part of the security origin for this
// security context. |name| must not be empty, although it may be null. A null
// name represents a lack of a suborigin.
// See: https://w3c.github.io/webappsec-suborigins/index.html
void SecurityContext::enforceSuborigin(const Suborigin& suborigin)
{
    if (!RuntimeEnabledFeatures::suboriginsEnabled())
        return;

    DCHECK(!suborigin.name().isEmpty());
    DCHECK(RuntimeEnabledFeatures::suboriginsEnabled());
    DCHECK(m_securityOrigin.get());
    DCHECK(!m_securityOrigin->hasSuborigin() || m_securityOrigin->suborigin()->name() == suborigin.name());
    m_securityOrigin->addSuborigin(suborigin);
    didUpdateSecurityOrigin();
}

} // namespace blink
