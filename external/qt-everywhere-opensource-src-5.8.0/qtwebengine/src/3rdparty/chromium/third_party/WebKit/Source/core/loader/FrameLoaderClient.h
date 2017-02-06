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

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/IconURL.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/frame/FrameClient.h"
#include "core/html/LinkResource.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebEffectiveConnectionType.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebLoadingBehaviorFlag.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include <memory>
#include <v8.h>

namespace blink {

class Document;
class DocumentLoader;
class FetchRequest;
struct FrameLoadRequest;
class HTMLFormElement;
class HTMLFrameElementBase;
class HTMLFrameOwnerElement;
class HTMLMediaElement;
class HTMLPlugInElement;
class HistoryItem;
class KURL;
class LocalFrame;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class SharedWorkerRepositoryClient;
class SubstituteData;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebCookieJar;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMediaPlayerSource;
class WebMediaSession;
class WebMediaStream;
class WebRTCPeerConnectionHandler;
class WebServiceWorkerProvider;
class WebSocketHandle;
class Widget;

class CORE_EXPORT FrameLoaderClient : public FrameClient {
public:
    ~FrameLoaderClient() override {}

    virtual bool hasWebView() const = 0; // mainly for assertions

    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse) = 0;
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&) = 0;
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier) = 0;
    virtual void dispatchDidLoadResourceFromMemoryCache(const ResourceRequest&, const ResourceResponse&) = 0;

    virtual void dispatchDidHandleOnloadEvents() = 0;
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() = 0;
    virtual void dispatchDidNavigateWithinPage(HistoryItem*, HistoryCommitType, bool contentInitiated) { }
    virtual void dispatchWillClose() = 0;
    virtual void dispatchDidStartProvisionalLoad(double triggeringEventTime) = 0;
    virtual void dispatchDidReceiveTitle(const String&) = 0;
    virtual void dispatchDidChangeIcons(IconType) = 0;
    virtual void dispatchDidCommitLoad(HistoryItem*, HistoryCommitType) = 0;
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&, HistoryCommitType) = 0;
    virtual void dispatchDidFailLoad(const ResourceError&, HistoryCommitType) = 0;
    virtual void dispatchDidFinishDocumentLoad() = 0;
    virtual void dispatchDidFinishLoad() = 0;
    virtual void dispatchDidChangeThemeColor() = 0;

    virtual NavigationPolicy decidePolicyForNavigation(const ResourceRequest&, DocumentLoader*, NavigationType, NavigationPolicy, bool shouldReplaceCurrentEntry, bool isClientRedirect) = 0;
    virtual bool hasPendingNavigation() = 0;

    virtual void dispatchWillSendSubmitEvent(HTMLFormElement*) = 0;
    virtual void dispatchWillSubmitForm(HTMLFormElement*) = 0;

    virtual void didStartLoading(LoadStartType) = 0;
    virtual void progressEstimateChanged(double progressEstimate) = 0;
    virtual void didStopLoading() = 0;

    virtual void loadURLExternally(const ResourceRequest&, NavigationPolicy, const String& suggestedName, bool replacesCurrentHistoryItem) = 0;

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

    // The frame displayed content with certificate errors with the
    // given URL and security info.
    virtual void didDisplayContentWithCertificateErrors(const KURL&, const CString& securityInfo) = 0;
    // The frame ran content with certificate errors with the given URL
    // and security info.
    virtual void didRunContentWithCertificateErrors(const KURL&, const CString& securityInfo) = 0;

    // Will be called when |PerformanceTiming| events are updated
    virtual void didChangePerformanceTiming() { }

    // Will be called when a particular loading code path has been used. This
    // propogates renderer loading behavior to the browser process for
    // histograms.
    virtual void didObserveLoadingBehavior(WebLoadingBehaviorFlag) { }

    // Transmits the change in the set of watched CSS selectors property
    // that match any element on the frame.
    virtual void selectorMatchChanged(const Vector<String>& addedSelectors, const Vector<String>& removedSelectors) = 0;

    virtual DocumentLoader* createDocumentLoader(LocalFrame*, const ResourceRequest&, const SubstituteData&) = 0;

    virtual String userAgent() = 0;

    virtual String doNotTrackValue() = 0;

    virtual void transitionToCommittedForNewPage() = 0;

    virtual LocalFrame* createFrame(const FrameLoadRequest&, const AtomicString& name, HTMLFrameOwnerElement*) = 0;
    // Whether or not plugin creation should fail if the HTMLPlugInElement isn't in the DOM after plugin initialization.
    enum DetachedPluginPolicy {
        FailOnDetachedPlugin,
        AllowDetachedPlugin,
    };
    virtual bool canCreatePluginWithoutRenderer(const String& mimeType) const = 0;
    virtual Widget* createPlugin(HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually, DetachedPluginPolicy) = 0;

    virtual std::unique_ptr<WebMediaPlayer> createWebMediaPlayer(HTMLMediaElement&, const WebMediaPlayerSource&, WebMediaPlayerClient*) = 0;

    virtual std::unique_ptr<WebMediaSession> createWebMediaSession() = 0;

    virtual ObjectContentType getObjectContentType(const KURL&, const String& mimeType, bool shouldPreferPlugInsForImages) = 0;

    virtual void didCreateNewDocument() = 0;
    virtual void dispatchDidClearWindowObjectInMainWorld() = 0;
    virtual void documentElementAvailable() = 0;
    virtual void runScriptsAtDocumentElementAvailable() = 0;
    virtual void runScriptsAtDocumentReady(bool documentIsEmpty) = 0;

    virtual void didCreateScriptContext(v8::Local<v8::Context>, int extensionGroup, int worldId) = 0;
    virtual void willReleaseScriptContext(v8::Local<v8::Context>, int worldId) = 0;
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) = 0;

    virtual void didChangeScrollOffset() { }
    virtual void didUpdateCurrentHistoryItem() { }

    virtual bool allowScript(bool enabledPerSettings) { return enabledPerSettings; }
    virtual bool allowScriptFromSource(bool enabledPerSettings, const KURL&) { return enabledPerSettings; }
    virtual bool allowPlugins(bool enabledPerSettings) { return enabledPerSettings; }
    virtual bool allowImage(bool enabledPerSettings, const KURL&) { return enabledPerSettings; }
    virtual bool allowMedia(const KURL&) { return true; }
    virtual bool allowDisplayingInsecureContent(bool enabledPerSettings, const KURL&) { return enabledPerSettings; }
    virtual bool allowRunningInsecureContent(bool enabledPerSettings, SecurityOrigin*, const KURL&) { return enabledPerSettings; }
    virtual bool allowAutoplay(bool defaultValue) { return defaultValue; }

    // This callback notifies the client that the frame was about to run
    // JavaScript but did not because allowScript returned false. We
    // have a separate callback here because there are a number of places
    // that need to know if JavaScript is enabled but are not necessarily
    // preparing to execute script.
    virtual void didNotAllowScript() { }
    // This callback is similar, but for plugins.
    virtual void didNotAllowPlugins() { }

    // This callback notifies the client that the frame created a Keygen element.
    virtual void didUseKeygen() { }

    virtual WebCookieJar* cookieJar() const = 0;

    virtual void didChangeName(const String& name, const String& uniqueName) { }

    virtual void didEnforceInsecureRequestPolicy(WebInsecureRequestPolicy) {}

    virtual void didUpdateToUniqueOrigin() {}

    virtual void didChangeSandboxFlags(Frame* childFrame, SandboxFlags) { }

    // Called when a new Content Security Policy is added to the frame's
    // document.  This can be triggered by handling of HTTP headers, handling
    // of <meta> element, or by inheriting CSP from the parent (in case of
    // about:blank).
    virtual void didAddContentSecurityPolicy(const String& headerValue, ContentSecurityPolicyHeaderType, ContentSecurityPolicyHeaderSource) { }

    virtual void didChangeFrameOwnerProperties(HTMLFrameElementBase*) { }

    virtual void dispatchWillOpenWebSocket(WebSocketHandle*) { }

    virtual void dispatchWillStartUsingPeerConnectionHandler(WebRTCPeerConnectionHandler*) { }

    virtual bool allowWebGL(bool enabledPerSettings) { return enabledPerSettings; }

    // If an HTML document is being loaded, informs the embedder that the document will have its <body> attached soon.
    virtual void dispatchWillInsertBody() { }

    virtual void dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority, int intraPriorityValue) { }

    virtual std::unique_ptr<WebServiceWorkerProvider> createServiceWorkerProvider() = 0;

    virtual bool isControlledByServiceWorker(DocumentLoader&) = 0;

    virtual int64_t serviceWorkerID(DocumentLoader&) = 0;

    virtual SharedWorkerRepositoryClient* sharedWorkerRepositoryClient() { return 0; }

    virtual std::unique_ptr<WebApplicationCacheHost> createApplicationCacheHost(WebApplicationCacheHostClient*) = 0;

    virtual void dispatchDidChangeManifest() { }

    virtual unsigned backForwardLength() { return 0; }

    virtual bool isFrameLoaderClientImpl() const { return false; }

    // Called when elements preventing the sudden termination of the frame
    // become present or stop being present. |type| is the type of element
    // (BeforeUnload handler, Unload handler).
    enum SuddenTerminationDisablerType {
        BeforeUnloadHandler,
        UnloadHandler,
    };
    virtual void suddenTerminationDisablerChanged(bool present, SuddenTerminationDisablerType) { }

    virtual LinkResource* createServiceWorkerLinkResource(HTMLLinkElement*) { return nullptr; }

    // Effective connection type when this frame was loaded.
    virtual WebEffectiveConnectionType getEffectiveConnectionType() { return WebEffectiveConnectionType::TypeUnknown; }
};

} // namespace blink

#endif // FrameLoaderClient_h
