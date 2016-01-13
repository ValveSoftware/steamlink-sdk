// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorResourceContentLoader.h"

#include "FetchInitiatorTypeNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourcePtr.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/frame/LocalFrame.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/page/Page.h"

namespace WebCore {

class InspectorResourceContentLoader::ResourceClient FINAL : private StyleSheetResourceClient {
public:
    ResourceClient(InspectorResourceContentLoader* loader)
        : m_loader(loader)
    {
    }

    void waitForResource(Resource* resource)
    {
        resource->addClient(this);
    }

private:
    InspectorResourceContentLoader* m_loader;

    virtual void setCSSStyleSheet(const String&, const KURL&, const String&, const CSSStyleSheetResource*) OVERRIDE;

    friend class InspectorResourceContentLoader;
};

void InspectorResourceContentLoader::ResourceClient::setCSSStyleSheet(const String&, const KURL& url, const String&, const CSSStyleSheetResource* resource)
{
    if (m_loader)
        m_loader->resourceFinished(this);
    const_cast<CSSStyleSheetResource*>(resource)->removeClient(this);
    delete this;
}

InspectorResourceContentLoader::InspectorResourceContentLoader(Page* page)
    : m_allRequestsStarted(false)
{
    Vector<Document*> documents;
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        LocalFrame* localFrame = toLocalFrame(frame);
        documents.append(localFrame->document());
        documents.appendVector(InspectorPageAgent::importsForFrame(localFrame));
    }
    for (Vector<Document*>::const_iterator documentIt = documents.begin(); documentIt != documents.end(); ++documentIt) {
        Document* document = *documentIt;

        HashSet<String> urlsToFetch;
        Vector<CSSStyleSheet*> styleSheets;
        InspectorCSSAgent::collectAllDocumentStyleSheets(document, styleSheets);
        for (Vector<CSSStyleSheet*>::const_iterator stylesheetIt = styleSheets.begin(); stylesheetIt != styleSheets.end(); ++stylesheetIt) {
            CSSStyleSheet* styleSheet = *stylesheetIt;
            if (styleSheet->isInline() || !styleSheet->contents()->loadCompleted())
                continue;
            String url = styleSheet->baseURL().string();
            if (url.isEmpty() || urlsToFetch.contains(url))
                continue;
            urlsToFetch.add(url);
            FetchRequest request(ResourceRequest(url), FetchInitiatorTypeNames::internal);
            ResourcePtr<Resource> resource = document->fetcher()->fetchCSSStyleSheet(request);
            // Prevent garbage collection by holding a reference to this resource.
            m_resources.append(resource.get());
            ResourceClient* resourceClient = new ResourceClient(this);
            m_pendingResourceClients.add(resourceClient);
            resourceClient->waitForResource(resource.get());
        }
    }

    m_allRequestsStarted = true;
    checkDone();
}

void InspectorResourceContentLoader::addListener(PassOwnPtr<VoidCallback> callback)
{
    m_callbacks.append(callback);
    checkDone();
}

InspectorResourceContentLoader::~InspectorResourceContentLoader()
{
    stop();
}

void InspectorResourceContentLoader::stop()
{
    HashSet<ResourceClient*> pendingResourceClients;
    m_pendingResourceClients.swap(pendingResourceClients);
    for (HashSet<ResourceClient*>::const_iterator it = pendingResourceClients.begin(); it != pendingResourceClients.end(); ++it)
        (*it)->m_loader = 0;
    m_resources.clear();
    // Make sure all callbacks are called to prevent infinite waiting time.
    checkDone();
}

bool InspectorResourceContentLoader::hasFinished()
{
    return m_allRequestsStarted && m_pendingResourceClients.size() == 0;
}

void InspectorResourceContentLoader::checkDone()
{
    if (!hasFinished())
        return;
    Vector<OwnPtr<VoidCallback> > callbacks;
    callbacks.swap(m_callbacks);
    for (Vector<OwnPtr<VoidCallback> >::const_iterator it = callbacks.begin(); it != callbacks.end(); ++it)
        (*it)->handleEvent();
}

void InspectorResourceContentLoader::resourceFinished(ResourceClient* client)
{
    m_pendingResourceClients.remove(client);
    checkDone();
}

} // namespace WebCore
