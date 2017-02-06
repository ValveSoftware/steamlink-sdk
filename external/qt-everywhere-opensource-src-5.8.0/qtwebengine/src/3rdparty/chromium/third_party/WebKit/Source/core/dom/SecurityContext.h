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

#ifndef SecurityContext_h
#define SecurityContext_h

#include "core/CoreExport.h"
#include "core/dom/SandboxFlags.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/Suborigin.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SecurityOrigin;
class ContentSecurityPolicy;
class KURL;

class CORE_EXPORT SecurityContext : public GarbageCollectedMixin {
    WTF_MAKE_NONCOPYABLE(SecurityContext);
public:
    DECLARE_VIRTUAL_TRACE();

    using InsecureNavigationsSet = HashSet<unsigned, WTF::AlreadyHashed>;

    SecurityOrigin* getSecurityOrigin() const { return m_securityOrigin.get(); }
    ContentSecurityPolicy* contentSecurityPolicy() const { return m_contentSecurityPolicy.get(); }

    // Explicitly override the security origin for this security context.
    // Note: It is dangerous to change the security origin of a script context
    //       that already contains content.
    void setSecurityOrigin(PassRefPtr<SecurityOrigin>);
    virtual void didUpdateSecurityOrigin() = 0;

    SandboxFlags getSandboxFlags() const { return m_sandboxFlags; }
    bool isSandboxed(SandboxFlags mask) const { return m_sandboxFlags & mask; }
    virtual void enforceSandboxFlags(SandboxFlags mask);

    void setAddressSpace(WebAddressSpace space) { m_addressSpace = space; }
    WebAddressSpace addressSpace() const { return m_addressSpace; }
    String addressSpaceForBindings() const;

    void addInsecureNavigationUpgrade(unsigned hashedHost) { m_insecureNavigationsToUpgrade.add(hashedHost); }
    InsecureNavigationsSet* insecureNavigationsToUpgrade() { return &m_insecureNavigationsToUpgrade; }

    virtual void setInsecureRequestPolicy(WebInsecureRequestPolicy policy) { m_insecureRequestPolicy = policy; }
    WebInsecureRequestPolicy getInsecureRequestPolicy() const { return m_insecureRequestPolicy; }

    void enforceSuborigin(const Suborigin&);

protected:
    SecurityContext();
    virtual ~SecurityContext();

    void setContentSecurityPolicy(ContentSecurityPolicy*);

    void applySandboxFlags(SandboxFlags mask);

private:
    RefPtr<SecurityOrigin> m_securityOrigin;
    Member<ContentSecurityPolicy> m_contentSecurityPolicy;

    SandboxFlags m_sandboxFlags;

    WebAddressSpace m_addressSpace;
    WebInsecureRequestPolicy m_insecureRequestPolicy;
    InsecureNavigationsSet m_insecureNavigationsToUpgrade;
};

} // namespace blink

#endif // SecurityContext_h
