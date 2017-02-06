// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebRemoteFrameImpl.h"

#include "core/dom/Fullscreen.h"
#include "core/dom/RemoteSecurityContext.h"
#include "core/dom/SecurityContext.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutObject.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebRect.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebPerformance.h"
#include "public/web/WebRange.h"
#include "public/web/WebTreeScopeType.h"
#include "web/RemoteFrameOwner.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include <v8/include/v8.h>

namespace blink {

WebRemoteFrame* WebRemoteFrame::create(WebTreeScopeType scope, WebRemoteFrameClient* client, WebFrame* opener)
{
    return WebRemoteFrameImpl::create(scope, client, opener);
}

WebRemoteFrameImpl* WebRemoteFrameImpl::create(WebTreeScopeType scope, WebRemoteFrameClient* client, WebFrame* opener)
{
    WebRemoteFrameImpl* frame = new WebRemoteFrameImpl(scope, client);
    frame->setOpener(opener);
    return frame;
}

WebRemoteFrameImpl::~WebRemoteFrameImpl()
{
}

DEFINE_TRACE(WebRemoteFrameImpl)
{
    visitor->trace(m_frameClient);
    visitor->trace(m_frame);
    visitor->template registerWeakMembers<WebFrame, &WebFrame::clearWeakFrames>(this);
    WebFrame::traceFrames(visitor, this);
    WebFrameImplBase::trace(visitor);
}

bool WebRemoteFrameImpl::isWebLocalFrame() const
{
    return false;
}

WebLocalFrame* WebRemoteFrameImpl::toWebLocalFrame()
{
    NOTREACHED();
    return nullptr;
}

bool WebRemoteFrameImpl::isWebRemoteFrame() const
{
    return true;
}

WebRemoteFrame* WebRemoteFrameImpl::toWebRemoteFrame()
{
    return this;
}

void WebRemoteFrameImpl::close()
{
    m_selfKeepAlive.clear();
}

WebString WebRemoteFrameImpl::uniqueName() const
{
    NOTREACHED();
    return WebString();
}

WebString WebRemoteFrameImpl::assignedName() const
{
    NOTREACHED();
    return WebString();
}

void WebRemoteFrameImpl::setName(const WebString&)
{
    NOTREACHED();
}

WebVector<WebIconURL> WebRemoteFrameImpl::iconURLs(int iconTypesMask) const
{
    NOTREACHED();
    return WebVector<WebIconURL>();
}

void WebRemoteFrameImpl::setRemoteWebLayer(WebLayer* webLayer)
{
    if (!frame())
        return;

    frame()->setRemotePlatformLayer(webLayer);
}

void WebRemoteFrameImpl::setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::setCanHaveScrollbars(bool)
{
    NOTREACHED();
}

WebSize WebRemoteFrameImpl::scrollOffset() const
{
    NOTREACHED();
    return WebSize();
}

void WebRemoteFrameImpl::setScrollOffset(const WebSize&)
{
    NOTREACHED();
}

WebSize WebRemoteFrameImpl::contentsSize() const
{
    NOTREACHED();
    return WebSize();
}

bool WebRemoteFrameImpl::hasVisibleContent() const
{
    NOTREACHED();
    return false;
}

WebRect WebRemoteFrameImpl::visibleContentRect() const
{
    NOTREACHED();
    return WebRect();
}

bool WebRemoteFrameImpl::hasHorizontalScrollbar() const
{
    NOTREACHED();
    return false;
}

bool WebRemoteFrameImpl::hasVerticalScrollbar() const
{
    NOTREACHED();
    return false;
}

WebView* WebRemoteFrameImpl::view() const
{
    if (!frame())
        return nullptr;
    return WebViewImpl::fromPage(frame()->page());
}

WebDocument WebRemoteFrameImpl::document() const
{
    // TODO(dcheng): this should also ASSERT_NOT_REACHED, but a lot of
    // code tries to access the document of a remote frame at the moment.
    return WebDocument();
}

WebPerformance WebRemoteFrameImpl::performance() const
{
    NOTREACHED();
    return WebPerformance();
}

void WebRemoteFrameImpl::dispatchUnloadEvent()
{
    NOTREACHED();
}

void WebRemoteFrameImpl::executeScript(const WebScriptSource&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::executeScriptInIsolatedWorld(
    int worldID, const WebScriptSource* sources, unsigned numSources,
    int extensionGroup)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::addMessageToConsole(const WebConsoleMessage&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::collectGarbage()
{
    NOTREACHED();
}

v8::Local<v8::Value> WebRemoteFrameImpl::executeScriptAndReturnValue(
    const WebScriptSource&)
{
    NOTREACHED();
    return v8::Local<v8::Value>();
}

void WebRemoteFrameImpl::executeScriptInIsolatedWorld(
    int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
    int extensionGroup, WebVector<v8::Local<v8::Value>>* results)
{
    NOTREACHED();
}

v8::Local<v8::Value> WebRemoteFrameImpl::callFunctionEvenIfScriptDisabled(
    v8::Local<v8::Function>,
    v8::Local<v8::Value>,
    int argc,
    v8::Local<v8::Value> argv[])
{
    NOTREACHED();
    return v8::Local<v8::Value>();
}

v8::Local<v8::Context> WebRemoteFrameImpl::mainWorldScriptContext() const
{
    NOTREACHED();
    return v8::Local<v8::Context>();
}

v8::Local<v8::Context> WebRemoteFrameImpl::deprecatedMainWorldScriptContext() const
{
    return toV8Context(frame(), DOMWrapperWorld::mainWorld());
}

void WebRemoteFrameImpl::reload(WebFrameLoadType)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::reloadWithOverrideURL(const WebURL& overrideUrl, WebFrameLoadType)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::loadRequest(const WebURLRequest&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::loadHTMLString(
    const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
    bool replace)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::stopLoading()
{
    // TODO(dcheng,japhet): Calling this method should stop loads
    // in all subframes, both remote and local.
}

WebDataSource* WebRemoteFrameImpl::provisionalDataSource() const
{
    NOTREACHED();
    return nullptr;
}

WebDataSource* WebRemoteFrameImpl::dataSource() const
{
    NOTREACHED();
    return nullptr;
}

void WebRemoteFrameImpl::enableViewSourceMode(bool enable)
{
    NOTREACHED();
}

bool WebRemoteFrameImpl::isViewSourceModeEnabled() const
{
    NOTREACHED();
    return false;
}

void WebRemoteFrameImpl::setReferrerForRequest(WebURLRequest&, const WebURL& referrer)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::dispatchWillSendRequest(WebURLRequest&)
{
    NOTREACHED();
}

WebURLLoader* WebRemoteFrameImpl::createAssociatedURLLoader(const WebURLLoaderOptions&)
{
    NOTREACHED();
    return nullptr;
}

unsigned WebRemoteFrameImpl::unloadListenerCount() const
{
    NOTREACHED();
    return 0;
}

void WebRemoteFrameImpl::insertText(const WebString&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::setMarkedText(const WebString&, unsigned location, unsigned length)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::unmarkText()
{
    NOTREACHED();
}

bool WebRemoteFrameImpl::hasMarkedText() const
{
    NOTREACHED();
    return false;
}

WebRange WebRemoteFrameImpl::markedRange() const
{
    NOTREACHED();
    return WebRange();
}

bool WebRemoteFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const
{
    NOTREACHED();
    return false;
}

size_t WebRemoteFrameImpl::characterIndexForPoint(const WebPoint&) const
{
    NOTREACHED();
    return 0;
}

bool WebRemoteFrameImpl::executeCommand(const WebString&)
{
    NOTREACHED();
    return false;
}

bool WebRemoteFrameImpl::executeCommand(const WebString&, const WebString& value)
{
    NOTREACHED();
    return false;
}

bool WebRemoteFrameImpl::isCommandEnabled(const WebString&) const
{
    NOTREACHED();
    return false;
}

void WebRemoteFrameImpl::enableContinuousSpellChecking(bool)
{
}

bool WebRemoteFrameImpl::isContinuousSpellCheckingEnabled() const
{
    return false;
}

void WebRemoteFrameImpl::requestTextChecking(const WebElement&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::removeSpellingMarkers()
{
    NOTREACHED();
}

bool WebRemoteFrameImpl::hasSelection() const
{
    NOTREACHED();
    return false;
}

WebRange WebRemoteFrameImpl::selectionRange() const
{
    NOTREACHED();
    return WebRange();
}

WebString WebRemoteFrameImpl::selectionAsText() const
{
    NOTREACHED();
    return WebString();
}

WebString WebRemoteFrameImpl::selectionAsMarkup() const
{
    NOTREACHED();
    return WebString();
}

bool WebRemoteFrameImpl::selectWordAroundCaret()
{
    NOTREACHED();
    return false;
}

void WebRemoteFrameImpl::selectRange(const WebPoint& base, const WebPoint& extent)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::selectRange(const WebRange&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::moveRangeSelection(const WebPoint& base, const WebPoint& extent, WebFrame::TextGranularity granularity)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::moveCaretSelection(const WebPoint&)
{
    NOTREACHED();
}

bool WebRemoteFrameImpl::setEditableSelectionOffsets(int start, int end)
{
    NOTREACHED();
    return false;
}

bool WebRemoteFrameImpl::setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines)
{
    NOTREACHED();
    return false;
}

void WebRemoteFrameImpl::extendSelectionAndDelete(int before, int after)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::setCaretVisible(bool)
{
    NOTREACHED();
}

int WebRemoteFrameImpl::printBegin(const WebPrintParams&, const WebNode& constrainToNode)
{
    NOTREACHED();
    return 0;
}

float WebRemoteFrameImpl::printPage(int pageToPrint, WebCanvas*)
{
    NOTREACHED();
    return 0.0;
}

float WebRemoteFrameImpl::getPrintPageShrink(int page)
{
    NOTREACHED();
    return 0.0;
}

void WebRemoteFrameImpl::printEnd()
{
    NOTREACHED();
}

bool WebRemoteFrameImpl::isPrintScalingDisabledForPlugin(const WebNode&)
{
    NOTREACHED();
    return false;
}

bool WebRemoteFrameImpl::hasCustomPageSizeStyle(int pageIndex)
{
    NOTREACHED();
    return false;
}

bool WebRemoteFrameImpl::isPageBoxVisible(int pageIndex)
{
    NOTREACHED();
    return false;
}

void WebRemoteFrameImpl::pageSizeAndMarginsInPixels(
    int pageIndex,
    WebSize& pageSize,
    int& marginTop,
    int& marginRight,
    int& marginBottom,
    int& marginLeft)
{
    NOTREACHED();
}

WebString WebRemoteFrameImpl::pageProperty(const WebString& propertyName, int pageIndex)
{
    NOTREACHED();
    return WebString();
}

void WebRemoteFrameImpl::printPagesWithBoundaries(WebCanvas*, const WebSize&)
{
    NOTREACHED();
}

void WebRemoteFrameImpl::dispatchMessageEventWithOriginCheck(
    const WebSecurityOrigin& intendedTargetOrigin,
    const WebDOMEvent&)
{
    NOTREACHED();
}

WebRect WebRemoteFrameImpl::selectionBoundsRect() const
{
    NOTREACHED();
    return WebRect();
}

bool WebRemoteFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    NOTREACHED();
    return false;
}

WebString WebRemoteFrameImpl::layerTreeAsText(bool showDebugInfo) const
{
    NOTREACHED();
    return WebString();
}

WebLocalFrame* WebRemoteFrameImpl::createLocalChild(WebTreeScopeType scope, const WebString& name, const WebString& uniqueName, WebSandboxFlags sandboxFlags, WebFrameClient* client, WebFrame* previousSibling, const WebFrameOwnerProperties& frameOwnerProperties, WebFrame* opener)
{
    WebLocalFrameImpl* child = WebLocalFrameImpl::create(scope, client, opener);
    insertAfter(child, previousSibling);
    RemoteFrameOwner* owner = RemoteFrameOwner::create(static_cast<SandboxFlags>(sandboxFlags), frameOwnerProperties);
    // FIXME: currently this calls LocalFrame::init() on the created LocalFrame, which may
    // result in the browser observing two navigations to about:blank (one from the initial
    // frame creation, and one from swapping it into the remote process). FrameLoader might
    // need a special initialization function for this case to avoid that duplicate navigation.
    child->initializeCoreFrame(frame()->host(), owner, name, uniqueName);
    // Partially related with the above FIXME--the init() call may trigger JS dispatch. However,
    // if the parent is remote, it should never be detached synchronously...
    DCHECK(child->frame());
    return child;
}

void WebRemoteFrameImpl::initializeCoreFrame(FrameHost* host, FrameOwner* owner, const AtomicString& name, const AtomicString& uniqueName)
{
    setCoreFrame(RemoteFrame::create(m_frameClient.get(), host, owner));
    frame()->createView();
    m_frame->tree().setPrecalculatedName(name, uniqueName);
}

WebRemoteFrame* WebRemoteFrameImpl::createRemoteChild(WebTreeScopeType scope, const WebString& name, const WebString& uniqueName, WebSandboxFlags sandboxFlags, WebRemoteFrameClient* client, WebFrame* opener)
{
    WebRemoteFrameImpl* child = WebRemoteFrameImpl::create(scope, client, opener);
    appendChild(child);
    RemoteFrameOwner* owner = RemoteFrameOwner::create(static_cast<SandboxFlags>(sandboxFlags), WebFrameOwnerProperties());
    child->initializeCoreFrame(frame()->host(), owner, name, uniqueName);
    return child;
}

void WebRemoteFrameImpl::setCoreFrame(RemoteFrame* frame)
{
    m_frame = frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::fromFrame(RemoteFrame& frame)
{
    if (!frame.client())
        return nullptr;
    return static_cast<RemoteFrameClientImpl*>(frame.client())->webFrame();
}

void WebRemoteFrameImpl::initializeFromFrame(WebLocalFrame* source) const
{
    DCHECK(source);
    WebLocalFrameImpl* localFrameImpl = toWebLocalFrameImpl(source);
    client()->initializeChildFrame(localFrameImpl->frame()->page()->deviceScaleFactor());
}

void WebRemoteFrameImpl::setReplicatedOrigin(const WebSecurityOrigin& origin) const
{
    DCHECK(frame());
    frame()->securityContext()->setReplicatedOrigin(origin);

    // If the origin of a remote frame changed, the accessibility object for the owner
    // element now points to a different child.
    //
    // TODO(dmazzoni, dcheng): there's probably a better way to solve this.
    // Run SitePerProcessAccessibilityBrowserTest.TwoCrossSiteNavigations to
    // ensure an alternate fix works.  http://crbug.com/566222
    FrameOwner* owner = frame()->owner();
    if (owner && owner->isLocal()) {
        HTMLElement* ownerElement = toHTMLFrameOwnerElement(owner);
        AXObjectCache* cache = ownerElement->document().existingAXObjectCache();
        if (cache)
            cache->childrenChanged(ownerElement);
    }
}

void WebRemoteFrameImpl::setReplicatedSandboxFlags(WebSandboxFlags flags) const
{
    DCHECK(frame());
    frame()->securityContext()->enforceSandboxFlags(static_cast<SandboxFlags>(flags));
}

void WebRemoteFrameImpl::setReplicatedName(const WebString& name, const WebString& uniqueName) const
{
    DCHECK(frame());
    frame()->tree().setPrecalculatedName(name, uniqueName);
}

void WebRemoteFrameImpl::addReplicatedContentSecurityPolicyHeader(const WebString& headerValue, WebContentSecurityPolicyType type, WebContentSecurityPolicySource source) const
{
    frame()->securityContext()->contentSecurityPolicy()->addPolicyFromHeaderValue(
        headerValue,
        static_cast<ContentSecurityPolicyHeaderType>(type),
        static_cast<ContentSecurityPolicyHeaderSource>(source));
}

void WebRemoteFrameImpl::resetReplicatedContentSecurityPolicy() const
{
    frame()->securityContext()->resetReplicatedContentSecurityPolicy();
}

void WebRemoteFrameImpl::setReplicatedInsecureRequestPolicy(WebInsecureRequestPolicy policy) const
{
    DCHECK(frame());
    frame()->securityContext()->setInsecureRequestPolicy(policy);
}

void WebRemoteFrameImpl::setReplicatedPotentiallyTrustworthyUniqueOrigin(bool isUniqueOriginPotentiallyTrustworthy) const
{
    DCHECK(frame());
    // If |isUniqueOriginPotentiallyTrustworthy| is true, then the origin must be unique.
    DCHECK(!isUniqueOriginPotentiallyTrustworthy || frame()->securityContext()->getSecurityOrigin()->isUnique());
    frame()->securityContext()->getSecurityOrigin()->setUniqueOriginIsPotentiallyTrustworthy(isUniqueOriginPotentiallyTrustworthy);
}

void WebRemoteFrameImpl::DispatchLoadEventForFrameOwner() const
{
    DCHECK(frame()->owner()->isLocal());
    frame()->owner()->dispatchLoad();
}

void WebRemoteFrameImpl::didStartLoading()
{
    frame()->setIsLoading(true);
}

void WebRemoteFrameImpl::didStopLoading()
{
    frame()->setIsLoading(false);
    if (parent() && parent()->isWebLocalFrame()) {
        WebLocalFrameImpl* parentFrame =
            toWebLocalFrameImpl(parent()->toWebLocalFrame());
        parentFrame->frame()->loader().checkCompleted();
    }
}

bool WebRemoteFrameImpl::isIgnoredForHitTest() const
{
    HTMLFrameOwnerElement* owner = frame()->deprecatedLocalOwner();
    if (!owner || !owner->layoutObject())
        return false;
    return owner->layoutObject()->style()->pointerEvents() == PE_NONE;
}

void WebRemoteFrameImpl::willEnterFullScreen()
{
    // This should only ever be called when the FrameOwner is local.
    HTMLFrameOwnerElement* ownerElement = toHTMLFrameOwnerElement(frame()->owner());

    // Call requestFullscreen() on |ownerElement| to make it the provisional
    // fullscreen element in FullscreenController, and to prepare
    // fullscreenchange events that will need to fire on it and its (local)
    // ancestors. The events will be triggered if/when fullscreen is entered.
    //
    // Passing |forCrossProcessAncestor| to requestFullscreen is necessary
    // because:
    // - |ownerElement| will need :-webkit-full-screen-ancestor style in
    //   addition to :-webkit-full-screen.
    // - there's no need to resend the ToggleFullscreen IPC to the browser
    //   process.
    //
    // TODO(alexmos): currently, this assumes prefixed requests, but in the
    // future, this should plumb in information about which request type
    // (prefixed or unprefixed) to use for firing fullscreen events.
    Fullscreen::from(ownerElement->document()).requestFullscreen(*ownerElement, Fullscreen::PrefixedRequest, true /* forCrossProcessAncestor */);
}

WebRemoteFrameImpl::WebRemoteFrameImpl(WebTreeScopeType scope, WebRemoteFrameClient* client)
    : WebRemoteFrame(scope)
    , m_frameClient(RemoteFrameClientImpl::create(this))
    , m_client(client)
    , m_selfKeepAlive(this)
{
}

} // namespace blink
