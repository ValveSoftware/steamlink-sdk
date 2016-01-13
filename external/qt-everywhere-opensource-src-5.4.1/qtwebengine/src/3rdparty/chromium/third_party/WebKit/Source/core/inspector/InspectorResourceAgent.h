/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorResourceAgent_h
#define InspectorResourceAgent_h

#include "bindings/v8/ScriptString.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"


namespace WTF {
class String;
}

namespace WebCore {

class Resource;
struct FetchInitiatorInfo;
class Document;
class DocumentLoader;
class FormData;
class LocalFrame;
class HTTPHeaderMap;
class InspectorFrontend;
class InspectorPageAgent;
class InstrumentingAgents;
class JSONObject;
class KURL;
class NetworkResourcesData;
class Page;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class ThreadableLoaderClient;
class XHRReplayData;
class XMLHttpRequest;

class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;

typedef String ErrorString;

class InspectorResourceAgent FINAL : public InspectorBaseAgent<InspectorResourceAgent>, public InspectorBackendDispatcher::NetworkCommandHandler {
public:
    static PassOwnPtr<InspectorResourceAgent> create(InspectorPageAgent* pageAgent)
    {
        return adoptPtr(new InspectorResourceAgent(pageAgent));
    }

    virtual void setFrontend(InspectorFrontend*) OVERRIDE;
    virtual void clearFrontend() OVERRIDE;
    virtual void restore() OVERRIDE;

    virtual ~InspectorResourceAgent();

    // Called from instrumentation.
    void willSendRequest(unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, const FetchInitiatorInfo&);
    void markResourceAsCached(unsigned long identifier);
    void didReceiveResourceResponse(LocalFrame*, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didReceiveData(LocalFrame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    void didFinishLoading(unsigned long identifier, DocumentLoader*, double monotonicFinishTime, int64_t encodedDataLength);
    void didReceiveCORSRedirectResponse(LocalFrame*, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didFailLoading(unsigned long identifier, const ResourceError&);
    void didCommitLoad(LocalFrame*, DocumentLoader*);
    void scriptImported(unsigned long identifier, const String& sourceString);
    void didReceiveScriptResponse(unsigned long identifier);

    void documentThreadableLoaderStartedLoadingForClient(unsigned long identifier, ThreadableLoaderClient*);
    void willLoadXHR(XMLHttpRequest*, ThreadableLoaderClient*, const AtomicString& method, const KURL&, bool async, FormData* body, const HTTPHeaderMap& headers, bool includeCrendentials);
    void didFailXHRLoading(XMLHttpRequest*, ThreadableLoaderClient*);
    void didFinishXHRLoading(XMLHttpRequest*, ThreadableLoaderClient*, unsigned long identifier, ScriptString sourceString, const AtomicString&, const String&, const String&, unsigned);

    void willDestroyResource(Resource*);

    void applyUserAgentOverride(String* userAgent);

    // FIXME: InspectorResourceAgent should not be aware of style recalculation.
    void willRecalculateStyle(Document*);
    void didRecalculateStyle(int);
    void didScheduleStyleRecalculation(Document*);

    void frameScheduledNavigation(LocalFrame*, double);
    void frameClearedScheduledNavigation(LocalFrame*);

    PassRefPtr<TypeBuilder::Network::Initiator> buildInitiatorObject(Document*, const FetchInitiatorInfo&);

    void didCreateWebSocket(Document*, unsigned long identifier, const KURL& requestURL, const String&);
    void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest*);
    void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeRequest*, const WebSocketHandshakeResponse*);
    void didCloseWebSocket(Document*, unsigned long identifier);
    void didReceiveWebSocketFrame(unsigned long identifier, int opCode, bool masked, const char* payload, size_t payloadLength);
    void didSendWebSocketFrame(unsigned long identifier, int opCode, bool masked, const char* payload, size_t payloadLength);
    void didReceiveWebSocketFrameError(unsigned long identifier, const String&);

    // called from Internals for layout test purposes.
    void setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize);

    // Called from frontend
    virtual void enable(ErrorString*) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE;
    virtual void setUserAgentOverride(ErrorString*, const String& userAgent) OVERRIDE;
    virtual void setExtraHTTPHeaders(ErrorString*, const RefPtr<JSONObject>&) OVERRIDE;
    virtual void getResponseBody(ErrorString*, const String& requestId, String* content, bool* base64Encoded) OVERRIDE;

    virtual void replayXHR(ErrorString*, const String& requestId) OVERRIDE;

    virtual void canClearBrowserCache(ErrorString*, bool*) OVERRIDE;
    virtual void canClearBrowserCookies(ErrorString*, bool*) OVERRIDE;
    virtual void setCacheDisabled(ErrorString*, bool cacheDisabled) OVERRIDE;

    virtual void loadResourceForFrontend(ErrorString*, const String& frameId, const String& url, const RefPtr<JSONObject>* requestHeaders, PassRefPtr<LoadResourceForFrontendCallback>) OVERRIDE;

    // Called from other agents.
    void setHostId(const String&);
    bool fetchResourceContent(LocalFrame*, const KURL&, String* content, bool* base64Encoded);

private:
    InspectorResourceAgent(InspectorPageAgent*);

    void enable();

    InspectorPageAgent* m_pageAgent;
    InspectorFrontend::Network* m_frontend;
    String m_userAgentOverride;
    String m_hostId;
    OwnPtr<NetworkResourcesData> m_resourcesData;

    typedef HashMap<ThreadableLoaderClient*, RefPtr<XHRReplayData> > PendingXHRReplayDataMap;
    PendingXHRReplayDataMap m_pendingXHRReplayData;

    typedef HashMap<String, RefPtr<TypeBuilder::Network::Initiator> > FrameNavigationInitiatorMap;
    FrameNavigationInitiatorMap m_frameNavigationInitiatorMap;

    // FIXME: InspectorResourceAgent should now be aware of style recalculation.
    RefPtr<TypeBuilder::Network::Initiator> m_styleRecalculationInitiator;
    bool m_isRecalculatingStyle;
};

} // namespace WebCore


#endif // !defined(InspectorResourceAgent_h)
