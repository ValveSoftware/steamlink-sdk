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

#include "config.h"
#include "core/loader/LinkLoader.h"

#include "core/FetchInitiatorTypeNames.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/Settings.h"
#include "core/html/LinkRelAttribute.h"
#include "core/loader/PrerenderHandle.h"
#include "platform/Prerender.h"
#include "platform/network/DNS.h"

namespace WebCore {

static unsigned prerenderRelTypesFromRelAttribute(const LinkRelAttribute& relAttribute)
{
    unsigned result = 0;
    if (relAttribute.isLinkPrerender())
        result |= PrerenderRelTypePrerender;
    if (relAttribute.isLinkNext())
        result |= PrerenderRelTypeNext;

    return result;
}

LinkLoader::LinkLoader(LinkLoaderClient* client)
    : m_client(client)
    , m_linkLoadTimer(this, &LinkLoader::linkLoadTimerFired)
    , m_linkLoadingErrorTimer(this, &LinkLoader::linkLoadingErrorTimerFired)
{
}

LinkLoader::~LinkLoader()
{
}

void LinkLoader::linkLoadTimerFired(Timer<LinkLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_linkLoadTimer);
    m_client->linkLoaded();
}

void LinkLoader::linkLoadingErrorTimerFired(Timer<LinkLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_linkLoadingErrorTimer);
    m_client->linkLoadingErrored();
}

void LinkLoader::notifyFinished(Resource* resource)
{
    ASSERT(this->resource() == resource);

    if (resource->errorOccurred())
        m_linkLoadingErrorTimer.startOneShot(0, FROM_HERE);
    else
        m_linkLoadTimer.startOneShot(0, FROM_HERE);

    clearResource();
}

void LinkLoader::didStartPrerender()
{
    m_client->didStartLinkPrerender();
}

void LinkLoader::didStopPrerender()
{
    m_client->didStopLinkPrerender();
}

void LinkLoader::didSendLoadForPrerender()
{
    m_client->didSendLoadForLinkPrerender();
}

void LinkLoader::didSendDOMContentLoadedForPrerender()
{
    m_client->didSendDOMContentLoadedForLinkPrerender();
}

bool LinkLoader::loadLink(const LinkRelAttribute& relAttribute, const AtomicString& crossOriginMode, const String& type, const KURL& href, Document& document)
{
    if (relAttribute.isDNSPrefetch()) {
        Settings* settings = document.settings();
        // FIXME: The href attribute of the link element can be in "//hostname" form, and we shouldn't attempt
        // to complete that as URL <https://bugs.webkit.org/show_bug.cgi?id=48857>.
        if (settings && settings->dnsPrefetchingEnabled() && href.isValid() && !href.isEmpty())
            prefetchDNS(href.host());
    }

    // FIXME(crbug.com/323096): Should take care of import.
    if ((relAttribute.isLinkPrefetch() || relAttribute.isLinkSubresource() || relAttribute.isTransitionExitingStylesheet()) && href.isValid() && document.frame()) {
        if (!m_client->shouldLoadLink())
            return false;
        Resource::Type type = relAttribute.isLinkSubresource() ?  Resource::LinkSubresource : Resource::LinkPrefetch;
        FetchRequest linkRequest(ResourceRequest(document.completeURL(href)), FetchInitiatorTypeNames::link);
        if (!crossOriginMode.isNull())
            linkRequest.setCrossOriginAccessControl(document.securityOrigin(), crossOriginMode);
        setResource(document.fetcher()->fetchLinkResource(type, linkRequest));
    }

    if (const unsigned prerenderRelTypes = prerenderRelTypesFromRelAttribute(relAttribute)) {
        if (!m_prerender) {
            m_prerender = PrerenderHandle::create(document, this, href, prerenderRelTypes);
        } else if (m_prerender->url() != href) {
            m_prerender->cancel();
            m_prerender = PrerenderHandle::create(document, this, href, prerenderRelTypes);
        }
        // TODO(gavinp): Handle changes to rel types of existing prerenders.
    } else if (m_prerender) {
        m_prerender->cancel();
        m_prerender.clear();
    }
    return true;
}

void LinkLoader::released()
{
    // Only prerenders need treatment here; other links either use the Resource interface, or are notionally
    // atomic (dns prefetch).
    if (m_prerender) {
        m_prerender->cancel();
        m_prerender.clear();
    }
}

}
