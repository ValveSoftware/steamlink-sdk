/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef LinkLoader_h
#define LinkLoader_h

#include "core/CoreExport.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceOwner.h"
#include "core/loader/LinkLoaderClient.h"
#include "core/loader/LinkPreloadResourceClients.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/PrerenderClient.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class LinkRelAttribute;
class NetworkHintsInterface;
class PrerenderHandle;
struct ViewportDescriptionWrapper;

// The LinkLoader can load link rel types icon, dns-prefetch, subresource, prefetch and prerender.
class CORE_EXPORT LinkLoader final : public GarbageCollectedFinalized<LinkLoader>, public ResourceOwner<Resource, ResourceClient>, public PrerenderClient {
    USING_GARBAGE_COLLECTED_MIXIN(LinkLoader);
public:
    static LinkLoader* create(LinkLoaderClient* client)
    {
        return new LinkLoader(client);
    }
    ~LinkLoader() override;

    // from ResourceClient
    void notifyFinished(Resource*) override;
    String debugName() const override { return "LinkLoader"; }

    // from PrerenderClient
    void didStartPrerender() override;
    void didStopPrerender() override;
    void didSendLoadForPrerender() override;
    void didSendDOMContentLoadedForPrerender() override;

    void triggerEvents(const Resource*);

    void released();
    bool loadLink(const LinkRelAttribute&, CrossOriginAttributeValue, const String& type, const String& as, const String& media, const KURL&, Document&, const NetworkHintsInterface&);
    enum CanLoadResources { OnlyLoadResources, DoNotLoadResources, LoadResourcesAndPreconnect };
    static void loadLinksFromHeader(const String& headerValue, const KURL& baseURL, Document*, const NetworkHintsInterface&, CanLoadResources, ViewportDescriptionWrapper*);
    static bool getResourceTypeFromAsAttribute(const String& as, Resource::Type&);

    DECLARE_TRACE();

private:
    explicit LinkLoader(LinkLoaderClient*);

    void linkLoadTimerFired(Timer<LinkLoader>*);
    void linkLoadingErrorTimerFired(Timer<LinkLoader>*);
    void createLinkPreloadResourceClient(Resource*);

    Member<LinkLoaderClient> m_client;

    Timer<LinkLoader> m_linkLoadTimer;
    Timer<LinkLoader> m_linkLoadingErrorTimer;

    Member<PrerenderHandle> m_prerender;
    Member<LinkPreloadResourceClient> m_linkPreloadResourceClient;
};

} // namespace blink

#endif
