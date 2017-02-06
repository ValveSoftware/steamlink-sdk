/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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
 */

#ifndef FetchRequest_h
#define FetchRequest_h

#include "core/CoreExport.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/IntegrityMetadata.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {
class SecurityOrigin;

class CORE_EXPORT FetchRequest {
    STACK_ALLOCATED();
public:
    enum DeferOption { NoDefer, LazyLoad };
    enum OriginRestriction { UseDefaultOriginRestrictionForType, RestrictToSameOrigin, NoOriginRestriction };

    struct ResourceWidth {
        DISALLOW_NEW();
        float width;
        bool isSet;

        ResourceWidth()
            : width(0)
            , isSet(false)
        {
        }
    };

    explicit FetchRequest(const ResourceRequest&, const AtomicString& initiator, const String& charset = String(), ResourceLoadPriority = ResourceLoadPriorityUnresolved);
    FetchRequest(const ResourceRequest&, const AtomicString& initiator, const ResourceLoaderOptions&);
    FetchRequest(const ResourceRequest&, const FetchInitiatorInfo&);
    ~FetchRequest();

    ResourceRequest& mutableResourceRequest() { return m_resourceRequest; }
    const ResourceRequest& resourceRequest() const { return m_resourceRequest; }
    const KURL& url() const { return m_resourceRequest.url(); }

    const String& charset() const { return m_charset; }
    void setCharset(const String& charset) { m_charset = charset; }

    const ResourceLoaderOptions& options() const { return m_options; }
    void setOptions(const ResourceLoaderOptions& options) { m_options = options; }

    ResourceLoadPriority priority() const { return m_priority; }
    void setPriority(ResourceLoadPriority priority) { m_priority = priority; }

    DeferOption defer() const { return m_defer; }
    void setDefer(DeferOption defer) { m_defer = defer; }

    ResourceWidth getResourceWidth() const { return m_resourceWidth; }
    void setResourceWidth(ResourceWidth);

    ClientHintsPreferences& clientHintsPreferences() { return m_clientHintPreferences; }

    bool forPreload() const { return m_forPreload; }
    void setForPreload(bool forPreload, double discoveryTime = 0);

    double preloadDiscoveryTime() { return m_preloadDiscoveryTime; }

    bool isLinkPreload() { return m_linkPreload; }
    void setLinkPreload(bool isLinkPreload) { m_linkPreload = isLinkPreload; }

    void setContentSecurityCheck(ContentSecurityPolicyDisposition contentSecurityPolicyOption) { m_options.contentSecurityPolicyOption = contentSecurityPolicyOption; }
    void setCrossOriginAccessControl(SecurityOrigin*, CrossOriginAttributeValue);
    OriginRestriction getOriginRestriction() const { return m_originRestriction; }
    void setOriginRestriction(OriginRestriction restriction) { m_originRestriction = restriction; }
    const IntegrityMetadataSet& integrityMetadata() const { return m_integrityMetadata; }
    void setIntegrityMetadata(const IntegrityMetadataSet& metadata) { m_integrityMetadata = metadata; }

    String contentSecurityPolicyNonce() const { return m_options.contentSecurityPolicyNonce; }
    void setContentSecurityPolicyNonce(const String& nonce) { m_options.contentSecurityPolicyNonce = nonce; }

private:
    ResourceRequest m_resourceRequest;
    String m_charset;
    ResourceLoaderOptions m_options;
    ResourceLoadPriority m_priority;
    bool m_forPreload;
    bool m_linkPreload;
    double m_preloadDiscoveryTime;
    DeferOption m_defer;
    OriginRestriction m_originRestriction;
    ResourceWidth m_resourceWidth;
    ClientHintsPreferences m_clientHintPreferences;
    IntegrityMetadataSet m_integrityMetadata;
};

} // namespace blink

#endif
