/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef DocumentLoader_h
#define DocumentLoader_h

#include "core/CoreExport.h"
#include "core/dom/ViewportDescription.h"
#include "core/dom/WeakIdentifierMap.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/fetch/SubstituteData.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentWriter.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/SharedBuffer.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/WebLoadingBehaviorFlag.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class ApplicationCacheHost;
class ResourceFetcher;
class DocumentInit;
class LocalFrame;
class FrameLoader;
class ResourceLoader;
class WebDocumentSubresourceFilter;

class CORE_EXPORT DocumentLoader : public GarbageCollectedFinalized<DocumentLoader>, private RawResourceClient {
public:
    static DocumentLoader* create(LocalFrame* frame, const ResourceRequest& request, const SubstituteData& data)
    {
        return new DocumentLoader(frame, request, data);
    }
    ~DocumentLoader() override;

    LocalFrame* frame() const { return m_frame; }

    virtual void detachFromFrame();

    unsigned long mainResourceIdentifier() const;

    void replaceDocumentWhileExecutingJavaScriptURL(const DocumentInit&, const String& source);

    const AtomicString& mimeType() const;

    const ResourceRequest& originalRequest() const;

    const ResourceRequest& request() const;

    ResourceFetcher* fetcher() const { return m_fetcher.get(); }

    void setSubresourceFilter(std::unique_ptr<WebDocumentSubresourceFilter>);
    WebDocumentSubresourceFilter* subresourceFilter() const { return m_subresourceFilter.get(); }

    const SubstituteData& substituteData() const { return m_substituteData; }

    const KURL& url() const;
    const KURL& unreachableURL() const;
    const KURL& urlForHistory() const;

    const AtomicString& responseMIMEType() const;

    void didChangePerformanceTiming();
    void didObserveLoadingBehavior(WebLoadingBehaviorFlag);
    void updateForSameDocumentNavigation(const KURL&, SameDocumentNavigationSource);
    const ResourceResponse& response() const { return m_response; }
    bool isClientRedirect() const { return m_isClientRedirect; }
    void setIsClientRedirect(bool isClientRedirect) { m_isClientRedirect = isClientRedirect; }
    bool replacesCurrentHistoryItem() const { return m_replacesCurrentHistoryItem; }
    void setReplacesCurrentHistoryItem(bool replacesCurrentHistoryItem) { m_replacesCurrentHistoryItem = replacesCurrentHistoryItem; }

    bool isCommittedButEmpty() const { return m_state == Committed; }

    void setSentDidFinishLoad() { m_state = SentDidFinishLoad; }
    bool sentDidFinishLoad() const { return m_state == SentDidFinishLoad; }

    NavigationType getNavigationType() const { return m_navigationType; }
    void setNavigationType(NavigationType navigationType) { m_navigationType = navigationType; }

    void startLoadingMainResource();

    void acceptDataFromThreadedReceiver(const char* data, int dataLength, int encodedDataLength);
    DocumentLoadTiming& timing() { return m_documentLoadTiming; }
    const DocumentLoadTiming& timing() const { return m_documentLoadTiming; }

    ApplicationCacheHost* applicationCacheHost() const { return m_applicationCacheHost.get(); }

    void clearRedirectChain();
    void appendRedirect(const KURL&);

    ContentSecurityPolicy* releaseContentSecurityPolicy() { return m_contentSecurityPolicy.release(); }

    ClientHintsPreferences& clientHintsPreferences() { return m_clientHintsPreferences; }

    struct InitialScrollState {
        DISALLOW_NEW();
        InitialScrollState()
            : wasScrolledByUser(false)
            , didRestoreFromHistory(false)
        {
        }

        bool wasScrolledByUser;
        bool didRestoreFromHistory;
    };
    InitialScrollState& initialScrollState() { return m_initialScrollState; }

    void setWasBlockedAfterXFrameOptionsOrCSP() { m_wasBlockedAfterXFrameOptionsOrCSP = true; }
    bool wasBlockedAfterXFrameOptionsOrCSP() { return m_wasBlockedAfterXFrameOptionsOrCSP; }

    Resource* startPreload(Resource::Type, FetchRequest&);

    DECLARE_VIRTUAL_TRACE();

protected:
    DocumentLoader(LocalFrame*, const ResourceRequest&, const SubstituteData&);

    Vector<KURL> m_redirectChain;

private:
    static DocumentWriter* createWriterFor(const DocumentInit&, const AtomicString& mimeType, const AtomicString& encoding, bool dispatchWindowObjectAvailable, ParserSynchronizationPolicy, const KURL& overridingURL = KURL());

    void ensureWriter(const AtomicString& mimeType, const KURL& overridingURL = KURL());
    void endWriting(DocumentWriter*);

    FrameLoader* frameLoader() const;

    void commitIfReady();
    void commitData(const char* bytes, size_t length);
    ResourceLoader* mainResourceLoader() const;
    void clearMainResourceHandle();

    bool maybeCreateArchive();

    void finishedLoading(double finishTime);
    void cancelLoadAfterXFrameOptionsOrCSPDenied(const ResourceResponse&);
    void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) final;
    void responseReceived(Resource*, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) final;
    void dataReceived(Resource*, const char* data, size_t length) final;
    void processData(const char* data, size_t length);
    void notifyFinished(Resource*) final;
    String debugName() const override { return "DocumentLoader"; }

    bool maybeLoadEmpty();

    bool isRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

    bool shouldContinueForResponse() const;

    Member<LocalFrame> m_frame;
    Member<ResourceFetcher> m_fetcher;
    std::unique_ptr<WebDocumentSubresourceFilter> m_subresourceFilter;

    Member<RawResource> m_mainResource;

    Member<DocumentWriter> m_writer;

    // A reference to actual request used to create the data source.
    // The only part of this request that should change is the url, and
    // that only in the case of a same-document navigation.
    ResourceRequest m_originalRequest;

    SubstituteData m_substituteData;

    // The 'working' request. It may be mutated
    // several times from the original request to include additional
    // headers, cookie information, canonicalization and redirects.
    ResourceRequest m_request;

    ResourceResponse m_response;

    bool m_isClientRedirect;
    bool m_replacesCurrentHistoryItem;

    NavigationType m_navigationType;

    DocumentLoadTiming m_documentLoadTiming;

    double m_timeOfLastDataReceived;

    Member<ApplicationCacheHost> m_applicationCacheHost;

    Member<ContentSecurityPolicy> m_contentSecurityPolicy;
    ClientHintsPreferences m_clientHintsPreferences;
    InitialScrollState m_initialScrollState;

    bool m_wasBlockedAfterXFrameOptionsOrCSP;

    enum State {
        NotStarted,
        Provisional,
        Committed,
        DataReceived,
        MainResourceDone,
        SentDidFinishLoad
    };
    State m_state;

    // Used to protect against reentrancy into dataReceived().
    bool m_inDataReceived;
    RefPtr<SharedBuffer> m_dataBuffer;
};

DECLARE_WEAK_IDENTIFIER_MAP(DocumentLoader);

} // namespace blink

#endif // DocumentLoader_h
