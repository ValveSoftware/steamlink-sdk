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
 */

#ifndef InspectorPageAgent_h
#define InspectorPageAgent_h


#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Page.h"
#include "core/page/ChromeClient.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Resource;
class Document;
class DocumentLoader;
class InspectedFrames;
class InspectorCSSAgent;
class InspectorResourceContentLoader;
class KURL;
class LocalFrame;
class SharedBuffer;
class TextResourceDecoder;
class V8InspectorSession;

using blink::protocol::Maybe;

class CORE_EXPORT InspectorPageAgent final : public InspectorBaseAgent<protocol::Page::Metainfo> {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void pageLayoutInvalidated(bool resized) { }
        virtual void setPausedInDebuggerMessage(const String&) { }
        virtual void waitForCreateWindow(LocalFrame*) { }
    };

    enum ResourceType {
        DocumentResource,
        StylesheetResource,
        ImageResource,
        FontResource,
        MediaResource,
        ScriptResource,
        TextTrackResource,
        XHRResource,
        FetchResource,
        EventSourceResource,
        WebSocketResource,
        ManifestResource,
        OtherResource
    };

    static InspectorPageAgent* create(InspectedFrames*, Client*, InspectorResourceContentLoader*, V8InspectorSession*);

    static HeapVector<Member<Document>> importsForFrame(LocalFrame*);
    static bool cachedResourceContent(Resource*, String* result, bool* base64Encoded);
    static bool sharedBufferContent(PassRefPtr<SharedBuffer>, const String& mimeType, const String& textEncodingName, String* result, bool* base64Encoded);

    static PassRefPtr<SharedBuffer> resourceData(LocalFrame*, const KURL&, String* textEncodingName);
    static Resource* cachedResource(LocalFrame*, const KURL&);
    static String resourceTypeJson(ResourceType);
    static ResourceType cachedResourceType(const Resource&);
    static String cachedResourceTypeJson(const Resource&);

    // Page API for frontend
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void addScriptToEvaluateOnLoad(ErrorString*, const String& scriptSource, String* identifier) override;
    void removeScriptToEvaluateOnLoad(ErrorString*, const String& identifier) override;
    void setAutoAttachToCreatedPages(ErrorString*, bool autoAttach) override;
    void reload(ErrorString*, const Maybe<bool>& bypassCache, const Maybe<String>& scriptToEvaluateOnLoad) override;
    void navigate(ErrorString*, const String& url, String* frameId) override;
    void getResourceTree(ErrorString*, std::unique_ptr<protocol::Page::FrameResourceTree>* frameTree) override;
    void getResourceContent(ErrorString*, const String& frameId, const String& url, std::unique_ptr<GetResourceContentCallback>) override;
    void searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const Maybe<bool>& caseSensitive, const Maybe<bool>& isRegex, std::unique_ptr<SearchInResourceCallback>) override;
    void setDocumentContent(ErrorString*, const String& frameId, const String& html) override;
    void startScreencast(ErrorString*, const Maybe<String>& format, const Maybe<int>& quality, const Maybe<int>& maxWidth, const Maybe<int>& maxHeight, const Maybe<int>& everyNthFrame) override;
    void stopScreencast(ErrorString*) override;
    void setOverlayMessage(ErrorString*, const Maybe<String>& message) override;
    void setBlockedEventsWarningThreshold(ErrorString*, double threshold) override;

    // InspectorInstrumentation API
    void didClearDocumentOfWindowObject(LocalFrame*);
    void domContentLoadedEventFired(LocalFrame*);
    void loadEventFired(LocalFrame*);
    void didCommitLoad(LocalFrame*, DocumentLoader*);
    void frameAttachedToParent(LocalFrame*);
    void frameDetachedFromParent(LocalFrame*);
    void frameStartedLoading(LocalFrame*);
    void frameStoppedLoading(LocalFrame*);
    void frameScheduledNavigation(LocalFrame*, double delay);
    void frameClearedScheduledNavigation(LocalFrame*);
    void willRunJavaScriptDialog(const String& message, ChromeClient::DialogType);
    void didRunJavaScriptDialog(bool result);
    void didUpdateLayout();
    void didResizeMainFrame();
    void didRecalculateStyle();
    void windowCreated(LocalFrame*);

    // Inspector Controller API
    void restore() override;
    bool screencastEnabled();

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorPageAgent(InspectedFrames*, Client*, InspectorResourceContentLoader*, V8InspectorSession*);

    void finishReload();
    void getResourceContentAfterResourcesContentLoaded(const String& frameId, const String& url, std::unique_ptr<GetResourceContentCallback>);
    void searchContentAfterResourcesContentLoaded(const String& frameId, const String& url, const String& query, bool caseSensitive, bool isRegex, std::unique_ptr<SearchInResourceCallback>);

    static bool dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result);

    std::unique_ptr<protocol::Page::Frame> buildObjectForFrame(LocalFrame*);
    std::unique_ptr<protocol::Page::FrameResourceTree> buildObjectForFrameTree(LocalFrame*);
    Member<InspectedFrames> m_inspectedFrames;
    V8InspectorSession* m_v8Session;
    Client* m_client;
    long m_lastScriptIdentifier;
    String m_pendingScriptToEvaluateOnLoadOnce;
    String m_scriptToEvaluateOnLoadOnce;
    bool m_enabled;
    bool m_reloading;
    Member<InspectorResourceContentLoader> m_inspectorResourceContentLoader;
    int m_resourceContentLoaderClientId;
};


} // namespace blink


#endif // !defined(InspectorPagerAgent_h)
