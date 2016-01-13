/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef FrameLoaderClient_h
#define FrameLoaderClient_h

#include "core/dom/IconURL.h"
#include "core/frame/FrameClient.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include <v8.h>

namespace blink {
class WebCookieJar;
class WebRTCPeerConnectionHandler;
class WebServiceWorkerProvider;
class WebServiceWorkerProviderClient;
class WebSocketHandle;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
}

namespace WebCore {

    class Color;
    class DOMWindowExtension;
    class DOMWrapperWorld;
    class DocumentLoader;
    class Element;
    class FetchRequest;
    class FrameLoader;
    class FrameNetworkingContext;
    class HTMLAppletElement;
    class HTMLFormElement;
    class HTMLFrameOwnerElement;
    class HTMLPlugInElement;
    class HistoryItem;
    class IntSize;
    class KURL;
    class LocalFrame;
    class MessageEvent;
    class Page;
    class PluginView;
    class ResourceError;
    class ResourceHandle;
    class ResourceRequest;
    class ResourceResponse;
    class SecurityOrigin;
    class SharedBuffer;
    class SharedWorkerRepositoryClient;
    class SocketStreamHandle;
    class SubstituteData;
    class Widget;

    class FrameLoaderClient : public FrameClient {
    public:
        virtual ~FrameLoaderClient() { }

        virtual bool hasWebView() const = 0; // mainly for assertions

        virtual void detachedFromParent() = 0;

        virtual void dispatchWillRequestAfterPreconnect(ResourceRequest&) { }
        virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse) = 0;
        virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&) = 0;
        virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier) = 0;
        virtual void dispatchDidLoadResourceFromMemoryCache(const ResourceRequest&, const ResourceResponse&) = 0;

        virtual void dispatchDidHandleOnloadEvents() = 0;
        virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() = 0;
        virtual void dispatchDidNavigateWithinPage(HistoryItem*, HistoryCommitType) { }
        virtual void dispatchWillClose() = 0;
        virtual void dispatchDidStartProvisionalLoad() = 0;
        virtual void dispatchDidReceiveTitle(const String&) = 0;
        virtual void dispatchDidChangeIcons(IconType) = 0;
        virtual void dispatchDidCommitLoad(LocalFrame*, HistoryItem*, HistoryCommitType) = 0;
        virtual void dispatchDidFailProvisionalLoad(const ResourceError&) = 0;
        virtual void dispatchDidFailLoad(const ResourceError&) = 0;
        virtual void dispatchDidFinishDocumentLoad() = 0;
        virtual void dispatchDidFinishLoad() = 0;
        virtual void dispatchDidFirstVisuallyNonEmptyLayout() = 0;
        virtual void dispatchDidChangeThemeColor() = 0;

        virtual NavigationPolicy decidePolicyForNavigation(const ResourceRequest&, DocumentLoader*, NavigationPolicy) = 0;

        virtual void dispatchWillRequestResource(FetchRequest*) { }

        virtual void dispatchWillSendSubmitEvent(HTMLFormElement*) = 0;
        virtual void dispatchWillSubmitForm(HTMLFormElement*) = 0;

        virtual void didStartLoading(LoadStartType) = 0;
        virtual void progressEstimateChanged(double progressEstimate) = 0;
        virtual void didStopLoading() = 0;

        virtual void loadURLExternally(const ResourceRequest&, NavigationPolicy, const String& suggestedName = String()) = 0;

        virtual bool navigateBackForward(int offset) const = 0;

        // Another page has accessed the initial empty document of this frame.
        // It is no longer safe to display a provisional URL, since a URL spoof
        // is now possible.
        virtual void didAccessInitialDocument() { }

        // This frame has displayed inactive content (such as an image) from an
        // insecure source.  Inactive content cannot spread to other frames.
        virtual void didDisplayInsecureContent() = 0;

        // The indicated security origin has run active content (such as a
        // script) from an insecure source.  Note that the insecure content can
        // spread to other frames in the same origin.
        virtual void didRunInsecureContent(SecurityOrigin*, const KURL&) = 0;
        virtual void didDetectXSS(const KURL&, bool didBlockEntirePage) = 0;
        virtual void didDispatchPingLoader(const KURL&) = 0;

        // Transmits the change in the set of watched CSS selectors property
        // that match any element on the frame.
        virtual void selectorMatchChanged(const Vector<String>& addedSelectors, const Vector<String>& removedSelectors) = 0;

        virtual PassRefPtr<DocumentLoader> createDocumentLoader(LocalFrame*, const ResourceRequest&, const SubstituteData&) = 0;

        virtual String userAgent(const KURL&) = 0;

        virtual String doNotTrackValue() = 0;

        virtual void transitionToCommittedForNewPage() = 0;

        virtual PassRefPtr<LocalFrame> createFrame(const KURL&, const AtomicString& name, const Referrer&, HTMLFrameOwnerElement*) = 0;
        // Whether or not plugin creation should fail if the HTMLPlugInElement isn't in the DOM after plugin initialization.
        enum DetachedPluginPolicy {
            FailOnDetachedPlugin,
            AllowDetachedPlugin,
        };
        virtual bool canCreatePluginWithoutRenderer(const String& mimeType) const = 0;
        virtual PassRefPtr<Widget> createPlugin(HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually, DetachedPluginPolicy) = 0;

        virtual PassRefPtr<Widget> createJavaAppletWidget(HTMLAppletElement*, const KURL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues) = 0;

        virtual ObjectContentType objectContentType(const KURL&, const String& mimeType, bool shouldPreferPlugInsForImages) = 0;

        virtual void dispatchDidClearWindowObjectInMainWorld() = 0;
        virtual void documentElementAvailable() = 0;

        virtual void didCreateScriptContext(v8::Handle<v8::Context>, int extensionGroup, int worldId) = 0;
        virtual void willReleaseScriptContext(v8::Handle<v8::Context>, int worldId) = 0;
        virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) = 0;

        virtual void didChangeScrollOffset() { }
        virtual void didUpdateCurrentHistoryItem() { }

        virtual bool allowScript(bool enabledPerSettings) { return enabledPerSettings; }
        virtual bool allowScriptFromSource(bool enabledPerSettings, const KURL&) { return enabledPerSettings; }
        virtual bool allowPlugins(bool enabledPerSettings) { return enabledPerSettings; }
        virtual bool allowImage(bool enabledPerSettings, const KURL&) { return enabledPerSettings; }
        virtual bool allowDisplayingInsecureContent(bool enabledPerSettings, SecurityOrigin*, const KURL&) { return enabledPerSettings; }
        virtual bool allowRunningInsecureContent(bool enabledPerSettings, SecurityOrigin*, const KURL&) { return enabledPerSettings; }

        // This callback notifies the client that the frame was about to run
        // JavaScript but did not because allowScript returned false. We
        // have a separate callback here because there are a number of places
        // that need to know if JavaScript is enabled but are not necessarily
        // preparing to execute script.
        virtual void didNotAllowScript() { }
        // This callback is similar, but for plugins.
        virtual void didNotAllowPlugins() { }

        virtual blink::WebCookieJar* cookieJar() const = 0;

        // Returns true if the embedder intercepted the postMessage call
        virtual bool willCheckAndDispatchMessageEvent(SecurityOrigin* /*target*/, MessageEvent*) const { return false; }

        virtual void didChangeName(const String&) { }

        virtual void dispatchWillOpenSocketStream(SocketStreamHandle*) { }
        virtual void dispatchWillOpenWebSocket(blink::WebSocketHandle*) { }

        virtual void dispatchWillStartUsingPeerConnectionHandler(blink::WebRTCPeerConnectionHandler*) { }

        virtual void didRequestAutocomplete(HTMLFormElement*) = 0;

        virtual bool allowWebGL(bool enabledPerSettings) { return enabledPerSettings; }
        // Informs the embedder that a WebGL canvas inside this frame received a lost context
        // notification with the given GL_ARB_robustness guilt/innocence code (see Extensions3D.h).
        virtual void didLoseWebGLContext(int) { }

        // If an HTML document is being loaded, informs the embedder that the document will have its <body> attached soon.
        virtual void dispatchWillInsertBody() { }

        virtual void dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority, int intraPriorityValue) { }

        virtual PassOwnPtr<blink::WebServiceWorkerProvider> createServiceWorkerProvider() = 0;

        virtual SharedWorkerRepositoryClient* sharedWorkerRepositoryClient() { return 0; }

        virtual PassOwnPtr<blink::WebApplicationCacheHost> createApplicationCacheHost(blink::WebApplicationCacheHostClient*) = 0;

        virtual void didStopAllLoaders() { }

        virtual void dispatchDidChangeManifest() { }

        virtual bool isFrameLoaderClientImpl() const { return false; }
    };

} // namespace WebCore

#endif // FrameLoaderClient_h
