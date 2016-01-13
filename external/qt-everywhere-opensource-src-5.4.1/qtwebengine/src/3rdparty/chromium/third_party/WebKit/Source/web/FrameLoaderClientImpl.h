/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef FrameLoaderClientImpl_h
#define FrameLoaderClientImpl_h

#include "core/loader/FrameLoaderClient.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class WebLocalFrameImpl;
class WebPluginContainerImpl;
class WebPluginLoadObserver;

class FrameLoaderClientImpl FINAL : public WebCore::FrameLoaderClient {
public:
    FrameLoaderClientImpl(WebLocalFrameImpl* webFrame);
    virtual ~FrameLoaderClientImpl();

    WebLocalFrameImpl* webFrame() const { return m_webFrame; }

    // WebCore::FrameLoaderClient ----------------------------------------------

    // Notifies the WebView delegate that the JS window object has been cleared,
    // giving it a chance to bind native objects to the window before script
    // parsing begins.
    virtual void dispatchDidClearWindowObjectInMainWorld() OVERRIDE;
    virtual void documentElementAvailable() OVERRIDE;

    virtual void didCreateScriptContext(v8::Handle<v8::Context>, int extensionGroup, int worldId) OVERRIDE;
    virtual void willReleaseScriptContext(v8::Handle<v8::Context>, int worldId) OVERRIDE;

    // Returns true if we should allow the given V8 extension to be added to
    // the script context at the currently loading page and given extension group.
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) OVERRIDE;

    virtual bool hasWebView() const OVERRIDE;
    virtual WebCore::Frame* opener() const OVERRIDE;
    virtual void setOpener(WebCore::Frame*) OVERRIDE;
    virtual WebCore::Frame* parent() const OVERRIDE;
    virtual WebCore::Frame* top() const OVERRIDE;
    virtual WebCore::Frame* previousSibling() const OVERRIDE;
    virtual WebCore::Frame* nextSibling() const OVERRIDE;
    virtual WebCore::Frame* firstChild() const OVERRIDE;
    virtual WebCore::Frame* lastChild() const OVERRIDE;
    virtual void detachedFromParent() OVERRIDE;
    virtual void dispatchWillRequestAfterPreconnect(WebCore::ResourceRequest&) OVERRIDE;
    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse) OVERRIDE;
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&) OVERRIDE;
    virtual void dispatchDidChangeResourcePriority(unsigned long identifier, WebCore::ResourceLoadPriority, int intraPriorityValue) OVERRIDE;
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier) OVERRIDE;
    virtual void dispatchDidLoadResourceFromMemoryCache(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&) OVERRIDE;
    virtual void dispatchDidHandleOnloadEvents() OVERRIDE;
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() OVERRIDE;
    virtual void dispatchDidNavigateWithinPage(WebCore::HistoryItem*, WebCore::HistoryCommitType) OVERRIDE;
    virtual void dispatchWillClose() OVERRIDE;
    virtual void dispatchDidStartProvisionalLoad() OVERRIDE;
    virtual void dispatchDidReceiveTitle(const String&) OVERRIDE;
    virtual void dispatchDidChangeIcons(WebCore::IconType) OVERRIDE;
    virtual void dispatchDidCommitLoad(WebCore::LocalFrame*, WebCore::HistoryItem*, WebCore::HistoryCommitType) OVERRIDE;
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&) OVERRIDE;
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&) OVERRIDE;
    virtual void dispatchDidFinishDocumentLoad() OVERRIDE;
    virtual void dispatchDidFinishLoad() OVERRIDE;
    virtual void dispatchDidFirstVisuallyNonEmptyLayout() OVERRIDE;
    virtual void dispatchDidChangeThemeColor() OVERRIDE;
    virtual WebCore::NavigationPolicy decidePolicyForNavigation(const WebCore::ResourceRequest&, WebCore::DocumentLoader*, WebCore::NavigationPolicy) OVERRIDE;
    virtual void dispatchWillRequestResource(WebCore::FetchRequest*) OVERRIDE;
    virtual void dispatchWillSendSubmitEvent(WebCore::HTMLFormElement*) OVERRIDE;
    virtual void dispatchWillSubmitForm(WebCore::HTMLFormElement*) OVERRIDE;
    virtual void didStartLoading(WebCore::LoadStartType) OVERRIDE;
    virtual void didStopLoading() OVERRIDE;
    virtual void progressEstimateChanged(double progressEstimate) OVERRIDE;
    virtual void loadURLExternally(const WebCore::ResourceRequest&, WebCore::NavigationPolicy, const String& suggestedName = String()) OVERRIDE;
    virtual bool navigateBackForward(int offset) const OVERRIDE;
    virtual void didAccessInitialDocument() OVERRIDE;
    virtual void didDisplayInsecureContent() OVERRIDE;
    virtual void didRunInsecureContent(WebCore::SecurityOrigin*, const WebCore::KURL& insecureURL) OVERRIDE;
    virtual void didDetectXSS(const WebCore::KURL&, bool didBlockEntirePage) OVERRIDE;
    virtual void didDispatchPingLoader(const WebCore::KURL&) OVERRIDE;
    virtual void selectorMatchChanged(const Vector<String>& addedSelectors, const Vector<String>& removedSelectors) OVERRIDE;
    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(WebCore::LocalFrame*, const WebCore::ResourceRequest&, const WebCore::SubstituteData&) OVERRIDE;
    virtual WTF::String userAgent(const WebCore::KURL&) OVERRIDE;
    virtual WTF::String doNotTrackValue() OVERRIDE;
    virtual void transitionToCommittedForNewPage() OVERRIDE;
    virtual PassRefPtr<WebCore::LocalFrame> createFrame(const WebCore::KURL&, const WTF::AtomicString& name, const WebCore::Referrer&, WebCore::HTMLFrameOwnerElement*) OVERRIDE;
    virtual bool canCreatePluginWithoutRenderer(const String& mimeType) const;
    virtual PassRefPtr<WebCore::Widget> createPlugin(
        WebCore::HTMLPlugInElement*, const WebCore::KURL&,
        const Vector<WTF::String>&, const Vector<WTF::String>&,
        const WTF::String&, bool loadManually, DetachedPluginPolicy) OVERRIDE;
    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(
        WebCore::HTMLAppletElement*,
        const WebCore::KURL& /* base_url */,
        const Vector<WTF::String>& paramNames,
        const Vector<WTF::String>& paramValues) OVERRIDE;
    virtual WebCore::ObjectContentType objectContentType(
        const WebCore::KURL&, const WTF::String& mimeType, bool shouldPreferPlugInsForImages) OVERRIDE;
    virtual void didChangeScrollOffset() OVERRIDE;
    virtual void didUpdateCurrentHistoryItem() OVERRIDE;
    virtual bool allowScript(bool enabledPerSettings) OVERRIDE;
    virtual bool allowScriptFromSource(bool enabledPerSettings, const WebCore::KURL& scriptURL) OVERRIDE;
    virtual bool allowPlugins(bool enabledPerSettings) OVERRIDE;
    virtual bool allowImage(bool enabledPerSettings, const WebCore::KURL& imageURL) OVERRIDE;
    virtual bool allowDisplayingInsecureContent(bool enabledPerSettings, WebCore::SecurityOrigin*, const WebCore::KURL&) OVERRIDE;
    virtual bool allowRunningInsecureContent(bool enabledPerSettings, WebCore::SecurityOrigin*, const WebCore::KURL&) OVERRIDE;
    virtual void didNotAllowScript() OVERRIDE;
    virtual void didNotAllowPlugins() OVERRIDE;

    virtual WebCookieJar* cookieJar() const OVERRIDE;
    virtual bool willCheckAndDispatchMessageEvent(WebCore::SecurityOrigin* target, WebCore::MessageEvent*) const OVERRIDE;
    virtual void didChangeName(const String&) OVERRIDE;

    virtual void dispatchWillOpenSocketStream(WebCore::SocketStreamHandle*) OVERRIDE;
    virtual void dispatchWillOpenWebSocket(blink::WebSocketHandle*) OVERRIDE;

    virtual void dispatchWillStartUsingPeerConnectionHandler(blink::WebRTCPeerConnectionHandler*) OVERRIDE;

    virtual void didRequestAutocomplete(WebCore::HTMLFormElement*) OVERRIDE;

    virtual bool allowWebGL(bool enabledPerSettings) OVERRIDE;
    virtual void didLoseWebGLContext(int arbRobustnessContextLostReason) OVERRIDE;

    virtual void dispatchWillInsertBody() OVERRIDE;

    virtual PassOwnPtr<WebServiceWorkerProvider> createServiceWorkerProvider() OVERRIDE;
    virtual WebCore::SharedWorkerRepositoryClient* sharedWorkerRepositoryClient() OVERRIDE;

    virtual PassOwnPtr<WebApplicationCacheHost> createApplicationCacheHost(WebApplicationCacheHostClient*) OVERRIDE;

    virtual void didStopAllLoaders() OVERRIDE;

    virtual void dispatchDidChangeManifest() OVERRIDE;

private:
    virtual bool isFrameLoaderClientImpl() const OVERRIDE { return true; }

    PassOwnPtr<WebPluginLoadObserver> pluginLoadObserver(WebCore::DocumentLoader*);

    // The WebFrame that owns this object and manages its lifetime. Therefore,
    // the web frame object is guaranteed to exist.
    WebLocalFrameImpl* m_webFrame;
};

DEFINE_TYPE_CASTS(FrameLoaderClientImpl, WebCore::FrameLoaderClient, client, client->isFrameLoaderClientImpl(), client.isFrameLoaderClientImpl());

} // namespace blink

#endif
