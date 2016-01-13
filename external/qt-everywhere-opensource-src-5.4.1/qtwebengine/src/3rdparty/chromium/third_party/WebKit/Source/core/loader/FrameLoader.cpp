/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
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

#include "config.h"
#include "core/loader/FrameLoader.h"

#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ViewportDescription.h"
#include "core/editing/Editor.h"
#include "core/editing/UndoStack.h"
#include "core/events/PageTransitionEvent.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PinchViewport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameFetchContext.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ProgressTracker.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/BackForwardClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/CreateWindow.h"
#include "core/page/EventHandler.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "platform/Logging.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/FloatRect.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/scroll/ScrollAnimator.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

using namespace HTMLNames;

bool isBackForwardLoadType(FrameLoadType type)
{
    return type == FrameLoadTypeBackForward;
}

static bool needsHistoryItemRestore(FrameLoadType type)
{
    return type == FrameLoadTypeBackForward || type == FrameLoadTypeReload || type == FrameLoadTypeReloadFromOrigin;
}

FrameLoader::FrameLoader(LocalFrame* frame)
    : m_frame(frame)
    , m_mixedContentChecker(frame)
    , m_progressTracker(ProgressTracker::create(frame))
    , m_state(FrameStateProvisional)
    , m_loadType(FrameLoadTypeStandard)
    , m_fetchContext(FrameFetchContext::create(frame))
    , m_inStopAllLoaders(false)
    , m_isComplete(false)
    , m_checkTimer(this, &FrameLoader::checkTimerFired)
    , m_shouldCallCheckCompleted(false)
    , m_didAccessInitialDocument(false)
    , m_didAccessInitialDocumentTimer(this, &FrameLoader::didAccessInitialDocumentTimerFired)
    , m_forcedSandboxFlags(SandboxNone)
    , m_willDetachClient(false)
{
}

FrameLoader::~FrameLoader()
{
}

void FrameLoader::init()
{
    m_provisionalDocumentLoader = client()->createDocumentLoader(m_frame, ResourceRequest(KURL(ParsedURLString, emptyString())), SubstituteData());
    m_provisionalDocumentLoader->startLoadingMainResource();
    m_frame->document()->cancelParsing();
    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocument);
}

FrameLoaderClient* FrameLoader::client() const
{
    return static_cast<FrameLoaderClient*>(m_frame->client());
}

void FrameLoader::setDefersLoading(bool defers)
{
    if (m_documentLoader)
        m_documentLoader->setDefersLoading(defers);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->setDefersLoading(defers);
    if (m_policyDocumentLoader)
        m_policyDocumentLoader->setDefersLoading(defers);

    if (!defers) {
        if (m_deferredHistoryLoad.isValid()) {
            loadHistoryItem(m_deferredHistoryLoad.m_item.get(), m_deferredHistoryLoad.m_type, m_deferredHistoryLoad.m_cachePolicy);
            m_deferredHistoryLoad = DeferredHistoryLoad();
        }
        m_frame->navigationScheduler().startTimer();
        startCheckCompleteTimer();
    }
}

void FrameLoader::stopLoading()
{
    m_isComplete = true; // to avoid calling completed() in finishedParsing()

    if (m_frame->document() && m_frame->document()->parsing()) {
        finishedParsing();
        m_frame->document()->setParsing(false);
    }

    if (Document* doc = m_frame->document()) {
        // FIXME: HTML5 doesn't tell us to set the state to complete when aborting, but we do anyway to match legacy behavior.
        // http://www.w3.org/Bugs/Public/show_bug.cgi?id=10537
        doc->setReadyState(Document::Complete);
    }

    // FIXME: This will cancel redirection timer, which really needs to be restarted when restoring the frame from b/f cache.
    m_frame->navigationScheduler().cancel();
}

void FrameLoader::saveScrollState()
{
    if (!m_currentItem || !m_frame->view())
        return;

    // Shouldn't clobber anything if we might still restore later.
    if (needsHistoryItemRestore(m_loadType) && !m_frame->view()->wasScrolledByUser())
        return;

    m_currentItem->setScrollPoint(m_frame->view()->scrollPosition());

    if (m_frame->settings()->pinchVirtualViewportEnabled())
        m_currentItem->setPinchViewportScrollPoint(m_frame->host()->pinchViewport().visibleRect().location());
    else
        m_currentItem->setPinchViewportScrollPoint(FloatPoint(-1, -1));

    if (m_frame->isMainFrame() && !m_frame->page()->inspectorController().deviceEmulationEnabled())
        m_currentItem->setPageScaleFactor(m_frame->page()->pageScaleFactor());

    client()->didUpdateCurrentHistoryItem();
}

void FrameLoader::clearScrollPositionAndViewState()
{
    ASSERT(m_frame->isMainFrame());
    if (!m_currentItem)
        return;
    m_currentItem->clearScrollPoint();
    m_currentItem->setPageScaleFactor(0);
}

bool FrameLoader::closeURL()
{
    saveScrollState();

    // Should only send the pagehide event here if the current document exists.
    if (m_frame->document())
        m_frame->document()->dispatchUnloadEvents();
    stopLoading();

    if (Page* page = m_frame->page())
        page->undoStack().didUnloadFrame(*m_frame);
    return true;
}

void FrameLoader::didExplicitOpen()
{
    m_isComplete = false;

    // Calling document.open counts as committing the first real document load.
    if (!m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);

    // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
    // from a subsequent window.document.open / window.document.write call.
    // Canceling redirection here works for all cases because document.open
    // implicitly precedes document.write.
    m_frame->navigationScheduler().cancel();
}

void FrameLoader::clear()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    m_frame->editor().clear();
    m_frame->document()->cancelParsing();
    m_frame->document()->prepareForDestruction();
    m_frame->document()->removeFocusedElementOfSubtree(m_frame->document());

    m_frame->selection().prepareForDestruction();
    m_frame->eventHandler().clear();
    if (m_frame->view())
        m_frame->view()->clear();

    m_frame->script().enableEval();

    m_frame->navigationScheduler().clear();

    m_checkTimer.stop();
    m_shouldCallCheckCompleted = false;

    if (m_stateMachine.isDisplayingInitialEmptyDocument())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
}

void FrameLoader::setHistoryItemStateForCommit(HistoryCommitType historyCommitType, bool isPushOrReplaceState, PassRefPtr<SerializedScriptValue> stateObject)
{
    if (m_provisionalItem)
        m_currentItem = m_provisionalItem.release();

    if (!m_currentItem || historyCommitType == StandardCommit) {
        m_currentItem = HistoryItem::create();
    } else if (!isPushOrReplaceState && m_documentLoader->url() != m_currentItem->url()) {
        m_currentItem->generateNewItemSequenceNumber();
        if (!equalIgnoringFragmentIdentifier(m_documentLoader->url(), m_currentItem->url()))
            m_currentItem->generateNewDocumentSequenceNumber();
    }

    m_currentItem->setURL(m_documentLoader->urlForHistory());
    m_currentItem->setDocumentState(m_frame->document()->formElementsState());
    m_currentItem->setTarget(m_frame->tree().uniqueName());
    if (isPushOrReplaceState)
        m_currentItem->setStateObject(stateObject);
    m_currentItem->setReferrer(Referrer(m_documentLoader->request().httpReferrer(), m_documentLoader->request().referrerPolicy()));
    m_currentItem->setFormInfoFromRequest(m_documentLoader->request());
}

static HistoryCommitType loadTypeToCommitType(FrameLoadType type)
{
    switch (type) {
    case FrameLoadTypeStandard:
        return StandardCommit;
    case FrameLoadTypeInitialInChildFrame:
        return InitialCommitInChildFrame;
    case FrameLoadTypeBackForward:
        return BackForwardCommit;
    default:
        break;
    }
    return HistoryInertCommit;
}

void FrameLoader::receivedFirstData()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    HistoryCommitType historyCommitType = loadTypeToCommitType(m_loadType);
    if (historyCommitType == StandardCommit && (m_documentLoader->urlForHistory().isEmpty() || (opener() && !m_currentItem && m_documentLoader->originalRequest().url().isEmpty())))
        historyCommitType = HistoryInertCommit;
    else if (historyCommitType == InitialCommitInChildFrame && (!m_frame->tree().top()->isLocalFrame() || MixedContentChecker::isMixedContent(toLocalFrame(m_frame->tree().top())->document()->securityOrigin(), m_documentLoader->url())))
        historyCommitType = HistoryInertCommit;
    setHistoryItemStateForCommit(historyCommitType);

    if (!m_stateMachine.committedMultipleRealLoads() && m_loadType == FrameLoadTypeStandard)
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedMultipleRealLoads);

    client()->dispatchDidCommitLoad(m_frame, m_currentItem.get(), historyCommitType);

    InspectorInstrumentation::didCommitLoad(m_frame, m_documentLoader.get());
    m_frame->page()->didCommitLoad(m_frame);
    dispatchDidClearDocumentOfWindowObject();
}

static void didFailContentSecurityPolicyCheck(FrameLoader* loader)
{
    // load event and stopAllLoaders can detach the LocalFrame, so protect it.
    RefPtr<LocalFrame> frame(loader->frame());

    // Move the page to a unique origin, and cancel the load.
    frame->document()->enforceSandboxFlags(SandboxOrigin);
    loader->stopAllLoaders();

    // Fire a load event, as timing attacks would otherwise reveal that the
    // frame was blocked. This way, it looks like every other cross-origin
    // page.
    if (FrameOwner* frameOwner = frame->owner())
        frameOwner->dispatchLoad();
}

void FrameLoader::didBeginDocument(bool dispatch)
{
    m_isComplete = false;
    m_frame->document()->setReadyState(Document::Loading);

    if (m_provisionalItem && m_loadType == FrameLoadTypeBackForward)
        m_frame->domWindow()->statePopped(m_provisionalItem->stateObject());

    if (dispatch)
        dispatchDidClearDocumentOfWindowObject();

    m_frame->document()->initContentSecurityPolicy(m_documentLoader ? ContentSecurityPolicyResponseHeaders(m_documentLoader->response()) : ContentSecurityPolicyResponseHeaders());

    if (!m_frame->document()->contentSecurityPolicy()->allowAncestors(m_frame)) {
        didFailContentSecurityPolicyCheck(this);
        return;
    }

    Settings* settings = m_frame->document()->settings();
    if (settings) {
        m_frame->document()->fetcher()->setImagesEnabled(settings->imagesEnabled());
        m_frame->document()->fetcher()->setAutoLoadImages(settings->loadsImagesAutomatically());
    }

    if (m_documentLoader) {
        const AtomicString& dnsPrefetchControl = m_documentLoader->response().httpHeaderField("X-DNS-Prefetch-Control");
        if (!dnsPrefetchControl.isEmpty())
            m_frame->document()->parseDNSPrefetchControlHeader(dnsPrefetchControl);

        String headerContentLanguage = m_documentLoader->response().httpHeaderField("Content-Language");
        if (!headerContentLanguage.isEmpty()) {
            size_t commaIndex = headerContentLanguage.find(',');
            headerContentLanguage.truncate(commaIndex); // kNotFound == -1 == don't truncate
            headerContentLanguage = headerContentLanguage.stripWhiteSpace(isHTMLSpace<UChar>);
            if (!headerContentLanguage.isEmpty())
                m_frame->document()->setContentLanguage(AtomicString(headerContentLanguage));
        }
    }

    if (m_provisionalItem && m_loadType == FrameLoadTypeBackForward)
        m_frame->document()->setStateForNewFormElements(m_provisionalItem->documentState());
}

void FrameLoader::finishedParsing()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    // This can be called from the LocalFrame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    // Null-checking the FrameView indicates whether or not we're in the destructor.
    RefPtr<LocalFrame> protector = m_frame->view() ? m_frame : 0;

    if (client())
        client()->dispatchDidFinishDocumentLoad();

    checkCompleted();

    if (!m_frame->view())
        return; // We are being destroyed by something checkCompleted called.

    // Check if the scrollbars are really needed for the content.
    // If not, remove them, relayout, and repaint.
    m_frame->view()->restoreScrollbar();
    scrollToFragmentWithParentBoundary(m_frame->document()->url());
}

void FrameLoader::loadDone()
{
    checkCompleted();
}

bool FrameLoader::allChildrenAreComplete() const
{
    for (Frame* child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame() && !toLocalFrame(child)->loader().m_isComplete)
            return false;
    }
    return true;
}

bool FrameLoader::allAncestorsAreComplete() const
{
    for (Frame* ancestor = m_frame; ancestor; ancestor = ancestor->tree().parent()) {
        if (ancestor->isLocalFrame() && !toLocalFrame(ancestor)->document()->loadEventFinished())
            return false;
    }
    return true;
}

void FrameLoader::checkCompleted()
{
    RefPtr<LocalFrame> protect(m_frame);
    m_shouldCallCheckCompleted = false;

    if (m_frame->view())
        m_frame->view()->handleLoadCompleted();

    // Have we completed before?
    if (m_isComplete)
        return;

    // Are we still parsing?
    if (m_frame->document()->parsing())
        return;

    // Still waiting imports?
    if (!m_frame->document()->haveImportsLoaded())
        return;

    // Still waiting for images/scripts?
    if (m_frame->document()->fetcher()->requestCount())
        return;

    // Still waiting for elements that don't go through a FrameLoader?
    if (m_frame->document()->isDelayingLoadEvent())
        return;

    // Any frame that hasn't completed yet?
    if (!allChildrenAreComplete())
        return;

    // OK, completed.
    m_isComplete = true;
    m_frame->document()->setReadyState(Document::Complete);
    if (m_frame->document()->loadEventStillNeeded())
        m_frame->document()->implicitClose();

    m_frame->navigationScheduler().startTimer();

    completed();
    if (m_frame->page())
        checkLoadComplete();

    if (m_frame->view())
        m_frame->view()->handleLoadCompleted();
}

void FrameLoader::checkTimerFired(Timer<FrameLoader>*)
{
    RefPtr<LocalFrame> protect(m_frame);

    if (Page* page = m_frame->page()) {
        if (page->defersLoading())
            return;
    }
    if (m_shouldCallCheckCompleted)
        checkCompleted();
}

void FrameLoader::startCheckCompleteTimer()
{
    if (!m_shouldCallCheckCompleted)
        return;
    if (m_checkTimer.isActive())
        return;
    m_checkTimer.startOneShot(0, FROM_HERE);
}

void FrameLoader::scheduleCheckCompleted()
{
    m_shouldCallCheckCompleted = true;
    startCheckCompleteTimer();
}

LocalFrame* FrameLoader::opener()
{
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    return client() ? toLocalFrame(client()->opener()) : 0;
}

void FrameLoader::setOpener(LocalFrame* opener)
{
    // If the frame is already detached, the opener has already been cleared.
    if (client())
        client()->setOpener(opener);
}

bool FrameLoader::allowPlugins(ReasonForCallingAllowPlugins reason)
{
    Settings* settings = m_frame->settings();
    bool allowed = client()->allowPlugins(settings && settings->pluginsEnabled());
    if (!allowed && reason == AboutToInstantiatePlugin)
        client()->didNotAllowPlugins();
    return allowed;
}

void FrameLoader::updateForSameDocumentNavigation(const KURL& newURL, SameDocumentNavigationSource sameDocumentNavigationSource, PassRefPtr<SerializedScriptValue> data, FrameLoadType type)
{
    // Update the data source's request with the new URL to fake the URL change
    m_frame->document()->setURL(newURL);
    documentLoader()->updateForSameDocumentNavigation(newURL, sameDocumentNavigationSource);

    // Generate start and stop notifications only when loader is completed so that we
    // don't fire them for fragment redirection that happens in window.onload handler.
    // See https://bugs.webkit.org/show_bug.cgi?id=31838
    if (m_frame->document()->loadEventFinished())
        client()->didStartLoading(NavigationWithinSameDocument);

    HistoryCommitType historyCommitType = loadTypeToCommitType(type);
    if (!m_currentItem)
        historyCommitType = HistoryInertCommit;

    setHistoryItemStateForCommit(historyCommitType, sameDocumentNavigationSource == SameDocumentNavigationHistoryApi, data);
    client()->dispatchDidNavigateWithinPage(m_currentItem.get(), historyCommitType);
    client()->dispatchDidReceiveTitle(m_frame->document()->title());
    if (m_frame->document()->loadEventFinished())
        client()->didStopLoading();
}

void FrameLoader::loadInSameDocument(const KURL& url, PassRefPtr<SerializedScriptValue> stateObject, FrameLoadType type, ClientRedirectPolicy clientRedirect)
{
    // If we have a state object, we cannot also be a new navigation.
    ASSERT(!stateObject || type == FrameLoadTypeBackForward);

    // If we have a provisional request for a different document, a fragment scroll should cancel it.
    if (m_provisionalDocumentLoader) {
        m_provisionalDocumentLoader->stopLoading();
        if (m_provisionalDocumentLoader)
            m_provisionalDocumentLoader->detachFromFrame();
        m_provisionalDocumentLoader = nullptr;
        if (!m_frame->host())
            return;
    }
    m_loadType = type;
    saveScrollState();

    KURL oldURL = m_frame->document()->url();
    // If we were in the autoscroll/panScroll mode we want to stop it before following the link to the anchor
    bool hashChange = equalIgnoringFragmentIdentifier(url, oldURL) && url.fragmentIdentifier() != oldURL.fragmentIdentifier();
    if (hashChange) {
        m_frame->eventHandler().stopAutoscroll();
        m_frame->domWindow()->enqueueHashchangeEvent(oldURL, url);
    }
    m_documentLoader->setIsClientRedirect(clientRedirect == ClientRedirect);
    m_documentLoader->setReplacesCurrentHistoryItem(m_loadType == FrameLoadTypeStandard);
    updateForSameDocumentNavigation(url, SameDocumentNavigationDefault, nullptr, type);

    m_frame->view()->setWasScrolledByUser(false);

    // It's important to model this as a load that starts and immediately finishes.
    // Otherwise, the parent frame may think we never finished loading.
    started();

    // We need to scroll to the fragment whether or not a hash change occurred, since
    // the user might have scrolled since the previous navigation.
    scrollToFragmentWithParentBoundary(url);

    m_isComplete = false;
    checkCompleted();

    m_frame->domWindow()->statePopped(stateObject ? stateObject : SerializedScriptValue::nullValue());
}

void FrameLoader::completed()
{
    RefPtr<LocalFrame> protect(m_frame);

    for (Frame* descendant = m_frame->tree().traverseNext(m_frame); descendant; descendant = descendant->tree().traverseNext(m_frame)) {
        if (descendant->isLocalFrame())
            toLocalFrame(descendant)->navigationScheduler().startTimer();
    }

    Frame* parent = m_frame->tree().parent();
    if (parent && parent->isLocalFrame())
        toLocalFrame(parent)->loader().checkCompleted();

    if (m_frame->view())
        m_frame->view()->maintainScrollPositionAtAnchor(0);
}

void FrameLoader::started()
{
    for (Frame* frame = m_frame; frame; frame = frame->tree().parent()) {
        if (frame->isLocalFrame())
            toLocalFrame(frame)->loader().m_isComplete = false;
    }
}

void FrameLoader::setReferrerForFrameRequest(ResourceRequest& request, ShouldSendReferrer shouldSendReferrer, Document* originDocument)
{
    if (shouldSendReferrer == NeverSendReferrer) {
        request.clearHTTPReferrer();
        return;
    }

    // Always use the initiating document to generate the referrer.
    // We need to generateReferrerHeader(), because we might not have enforced ReferrerPolicy or https->http
    // referrer suppression yet.
    String argsReferrer(request.httpReferrer());
    if (argsReferrer.isEmpty())
        argsReferrer = originDocument->outgoingReferrer();
    String referrer = SecurityPolicy::generateReferrerHeader(originDocument->referrerPolicy(), request.url(), argsReferrer);

    request.setHTTPReferrer(Referrer(referrer, originDocument->referrerPolicy()));
    RefPtr<SecurityOrigin> referrerOrigin = SecurityOrigin::createFromString(referrer);
    addHTTPOriginIfNeeded(request, referrerOrigin->toAtomicString());
}

bool FrameLoader::isScriptTriggeredFormSubmissionInChildFrame(const FrameLoadRequest& request) const
{
    // If this is a child frame and the form submission was triggered by a script, lock the back/forward list
    // to match IE and Opera.
    // See https://bugs.webkit.org/show_bug.cgi?id=32383 for the original motivation for this.
    if (!m_frame->tree().parent() || UserGestureIndicator::processingUserGesture())
        return false;
    return request.formState() && request.formState()->formSubmissionTrigger() == SubmittedByJavaScript;
}

FrameLoadType FrameLoader::determineFrameLoadType(const FrameLoadRequest& request)
{
    if (m_frame->tree().parent() && !m_stateMachine.committedFirstRealDocumentLoad())
        return FrameLoadTypeInitialInChildFrame;
    if (!m_frame->tree().parent() && !m_frame->page()->backForward().backForwardListCount())
        return FrameLoadTypeStandard;
    if (m_provisionalDocumentLoader && request.substituteData().failingURL() == m_provisionalDocumentLoader->url() && m_loadType == FrameLoadTypeBackForward)
        return FrameLoadTypeBackForward;
    if (request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData)
        return FrameLoadTypeReload;
    if (request.resourceRequest().cachePolicy() == ReloadBypassingCache)
        return FrameLoadTypeReloadFromOrigin;
    if (request.lockBackForwardList() || isScriptTriggeredFormSubmissionInChildFrame(request))
        return FrameLoadTypeRedirectWithLockedBackForwardList;
    if (!request.originDocument() && request.resourceRequest().url() == m_documentLoader->urlForHistory())
        return FrameLoadTypeSame;
    if (request.substituteData().failingURL() == m_documentLoader->urlForHistory() && m_loadType == FrameLoadTypeReload)
        return FrameLoadTypeReload;
    return FrameLoadTypeStandard;
}

bool FrameLoader::prepareRequestForThisFrame(FrameLoadRequest& request)
{
    // If no origin Document* was specified, skip security checks and assume the caller has fully initialized the FrameLoadRequest.
    if (!request.originDocument())
        return true;

    KURL url = request.resourceRequest().url();
    if (m_frame->script().executeScriptIfJavaScriptURL(url))
        return false;

    if (!request.originDocument()->securityOrigin()->canDisplay(url)) {
        reportLocalLoadFailed(m_frame, url.elidedString());
        return false;
    }

    if (!request.formState() && request.frameName().isEmpty())
        request.setFrameName(m_frame->document()->baseTarget());

    setReferrerForFrameRequest(request.resourceRequest(), request.shouldSendReferrer(), request.originDocument());
    return true;
}

static bool shouldOpenInNewWindow(LocalFrame* targetFrame, const FrameLoadRequest& request, const NavigationAction& action)
{
    if (!targetFrame && !request.frameName().isEmpty())
        return true;
    // FIXME: This case is a workaround for the fact that ctrl+clicking a form submission incorrectly
    // sends as a GET rather than a POST if it creates a new window in a different process.
    return request.formState() && action.shouldOpenInNewWindow();
}

void FrameLoader::load(const FrameLoadRequest& passedRequest)
{
    ASSERT(m_frame->document());

    RefPtr<LocalFrame> protect(m_frame);

    if (m_inStopAllLoaders)
        return;

    FrameLoadRequest request(passedRequest);
    if (!prepareRequestForThisFrame(request))
        return;

    RefPtr<LocalFrame> targetFrame = request.formState() ? 0 : findFrameForNavigation(AtomicString(request.frameName()), request.formState() ? request.formState()->sourceDocument() : m_frame->document());
    if (targetFrame && targetFrame != m_frame) {
        request.setFrameName("_self");
        targetFrame->loader().load(request);
        if (Page* page = targetFrame->page())
            page->chrome().focus();
        return;
    }

    FrameLoadType newLoadType = determineFrameLoadType(request);
    NavigationAction action(request.resourceRequest(), newLoadType, request.formState(), request.triggeringEvent());
    if (shouldOpenInNewWindow(targetFrame.get(), request, action)) {
        if (action.policy() == NavigationPolicyDownload)
            client()->loadURLExternally(action.resourceRequest(), NavigationPolicyDownload);
        else
            createWindowForRequest(request, *m_frame, action.policy(), request.shouldSendReferrer());
        return;
    }

    const KURL& url = request.resourceRequest().url();
    if (!action.shouldOpenInNewWindow() && shouldPerformFragmentNavigation(request.formState(), request.resourceRequest().httpMethod(), newLoadType, url)) {
        m_documentLoader->setTriggeringAction(action);
        if (shouldTreatURLAsSameAsCurrent(url))
            newLoadType = FrameLoadTypeRedirectWithLockedBackForwardList;
        loadInSameDocument(url, nullptr, newLoadType, request.clientRedirect());
        return;
    }
    bool sameURL = url == m_documentLoader->urlForHistory();
    loadWithNavigationAction(action, newLoadType, request.formState(), request.substituteData(), request.clientRedirect());
    // Example of this case are sites that reload the same URL with a different cookie
    // driving the generated content, or a master frame with links that drive a target
    // frame, where the user has clicked on the same link repeatedly.
    if (sameURL && newLoadType != FrameLoadTypeReload && newLoadType != FrameLoadTypeReloadFromOrigin && request.resourceRequest().httpMethod() != "POST")
        m_loadType = FrameLoadTypeSame;
}

SubstituteData FrameLoader::defaultSubstituteDataForURL(const KURL& url)
{
    if (!shouldTreatURLAsSrcdocDocument(url))
        return SubstituteData();
    String srcdoc = m_frame->deprecatedLocalOwner()->fastGetAttribute(srcdocAttr);
    ASSERT(!srcdoc.isNull());
    CString encodedSrcdoc = srcdoc.utf8();
    return SubstituteData(SharedBuffer::create(encodedSrcdoc.data(), encodedSrcdoc.length()), "text/html", "UTF-8", KURL());
}

void FrameLoader::reportLocalLoadFailed(LocalFrame* frame, const String& url)
{
    ASSERT(!url.isEmpty());
    if (!frame)
        return;

    frame->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Not allowed to load local resource: " + url);
}

static ResourceRequest requestFromHistoryItem(HistoryItem* item, ResourceRequestCachePolicy cachePolicy)
{
    RefPtr<FormData> formData = item->formData();
    ResourceRequest request(item->url(), item->referrer());
    request.setCachePolicy(cachePolicy);
    if (formData) {
        request.setHTTPMethod("POST");
        request.setHTTPBody(formData);
        request.setHTTPContentType(item->formContentType());
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::createFromString(item->referrer().referrer);
        FrameLoader::addHTTPOriginIfNeeded(request, securityOrigin->toAtomicString());
    }
    return request;
}

void FrameLoader::reload(ReloadPolicy reloadPolicy, const KURL& overrideURL, const AtomicString& overrideEncoding)
{
    if (!m_currentItem)
        return;

    ResourceRequestCachePolicy cachePolicy = reloadPolicy == EndToEndReload ? ReloadBypassingCache : ReloadIgnoringCacheData;
    ResourceRequest request = requestFromHistoryItem(m_currentItem.get(), cachePolicy);
    if (!overrideURL.isEmpty()) {
        request.setURL(overrideURL);
        request.clearHTTPReferrer();
    }

    FrameLoadType type = reloadPolicy == EndToEndReload ? FrameLoadTypeReloadFromOrigin : FrameLoadTypeReload;
    loadWithNavigationAction(NavigationAction(request, type), type, nullptr, SubstituteData(), NotClientRedirect, overrideEncoding);
}

void FrameLoader::stopAllLoaders()
{
    if (m_frame->document()->pageDismissalEventBeingDispatched() != Document::NoDismissal)
        return;

    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_inStopAllLoaders)
        return;

    // Calling stopLoading() on the provisional document loader can blow away
    // the frame from underneath.
    RefPtr<LocalFrame> protect(m_frame);

    m_inStopAllLoaders = true;

    for (RefPtr<Frame> child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child.get())->loader().stopAllLoaders();
    }
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading();
    if (m_documentLoader)
        m_documentLoader->stopLoading();

    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->detachFromFrame();
    m_provisionalDocumentLoader = nullptr;

    m_checkTimer.stop();

    m_inStopAllLoaders = false;

    // detachFromParent() can be called multiple times on same LocalFrame, which
    // means we may no longer have a FrameLoaderClient to talk to.
    if (client())
        client()->didStopAllLoaders();
}

void FrameLoader::didAccessInitialDocument()
{
    // We only need to notify the client once, and only for the main frame.
    if (isLoadingMainFrame() && !m_didAccessInitialDocument) {
        m_didAccessInitialDocument = true;
        // Notify asynchronously, since this is called within a JavaScript security check.
        m_didAccessInitialDocumentTimer.startOneShot(0, FROM_HERE);
    }
}

void FrameLoader::didAccessInitialDocumentTimerFired(Timer<FrameLoader>*)
{
    client()->didAccessInitialDocument();
}

void FrameLoader::notifyIfInitialDocumentAccessed()
{
    if (m_didAccessInitialDocumentTimer.isActive()) {
        m_didAccessInitialDocumentTimer.stop();
        didAccessInitialDocumentTimerFired(0);
    }
}

bool FrameLoader::isLoading() const
{
    if (m_provisionalDocumentLoader)
        return true;
    return m_documentLoader && m_documentLoader->isLoading();
}

void FrameLoader::commitProvisionalLoad()
{
    ASSERT(client()->hasWebView());
    ASSERT(m_state == FrameStateProvisional);
    RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
    RefPtr<LocalFrame> protect(m_frame);

    // Check if the destination page is allowed to access the previous page's timing information.
    if (m_frame->document()) {
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(pdl->request().url());
        pdl->timing()->setHasSameOriginAsPreviousDocument(securityOrigin->canRequest(m_frame->document()->url()));
    }

    // The call to closeURL() invokes the unload event handler, which can execute arbitrary
    // JavaScript. If the script initiates a new load, we need to abandon the current load,
    // or the two will stomp each other.
    // detachChildren will similarly trigger child frame unload event handlers.
    if (m_documentLoader) {
        client()->dispatchWillClose();
        closeURL();
    }
    detachChildren();
    if (pdl != m_provisionalDocumentLoader)
        return;
    if (m_documentLoader)
        m_documentLoader->detachFromFrame();
    m_documentLoader = m_provisionalDocumentLoader.release();
    m_state = FrameStateCommittedPage;

    if (isLoadingMainFrame())
        m_frame->page()->chrome().client().needTouchEvents(false);

    client()->transitionToCommittedForNewPage();
    m_frame->navigationScheduler().cancel();
    m_frame->editor().clearLastEditCommand();

    // If we are still in the process of initializing an empty document then
    // its frame is not in a consistent state for rendering, so avoid setJSStatusBarText
    // since it may cause clients to attempt to render the frame.
    if (!m_stateMachine.creatingInitialEmptyDocument()) {
        LocalDOMWindow* window = m_frame->domWindow();
        window->setStatus(String());
        window->setDefaultStatus(String());
    }
    started();
}

bool FrameLoader::isLoadingMainFrame() const
{
    return m_frame->isMainFrame();
}

FrameLoadType FrameLoader::loadType() const
{
    return m_loadType;
}

// This function is an incomprehensible mess and is only used in checkLoadCompleteForThisFrame.
// If you're thinking of using it elsewhere, stop right now and reconsider your life.
static bool isDocumentDoneLoading(Document* document)
{
    if (!document->loader())
        return true;
    if (document->loader()->isLoadingMainResource())
        return false;
    if (!document->loadEventFinished()) {
        if (document->loader()->isLoading() || document->isDelayingLoadEvent())
            return false;
    }
    if (document->fetcher()->requestCount())
        return false;
    if (document->processingLoadEvent())
        return false;
    if (document->hasActiveParser())
        return false;
    return true;
}

bool FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(client()->hasWebView());
    RefPtr<LocalFrame> protect(m_frame);

    bool allChildrenAreDoneLoading = true;
    for (RefPtr<Frame> child = m_frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            allChildrenAreDoneLoading &= toLocalFrame(child.get())->loader().checkLoadCompleteForThisFrame();
    }

    if (m_state == FrameStateProvisional && m_provisionalDocumentLoader) {
        const ResourceError& error = m_provisionalDocumentLoader->mainDocumentError();
        if (error.isNull())
            return false;
        RefPtr<DocumentLoader> loader = m_provisionalDocumentLoader;
        client()->dispatchDidFailProvisionalLoad(error);
        if (loader != m_provisionalDocumentLoader)
            return false;
        m_provisionalDocumentLoader->detachFromFrame();
        m_provisionalDocumentLoader = nullptr;
        m_progressTracker->progressCompleted();
        m_state = FrameStateComplete;
        return true;
    }

    if (!allChildrenAreDoneLoading)
        return false;

    if (m_state == FrameStateComplete)
        return true;
    if (m_provisionalDocumentLoader || !m_documentLoader)
        return false;
    if (!isDocumentDoneLoading(m_frame->document()) && !m_inStopAllLoaders)
        return false;

    m_state = FrameStateComplete;

    // FIXME: Is this subsequent work important if we already navigated away?
    // Maybe there are bugs because of that, or extra work we can skip because
    // the new page is ready.

    // Retry restoring scroll offset since FrameStateComplete disables content
    // size clamping.
    restoreScrollPositionAndViewState();

    if (!m_stateMachine.committedFirstRealDocumentLoad())
        return true;

    m_progressTracker->progressCompleted();
    m_frame->domWindow()->finishedLoading();

    const ResourceError& error = m_documentLoader->mainDocumentError();
    if (!error.isNull()) {
        client()->dispatchDidFailLoad(error);
    } else {
        // Report mobile vs. desktop page statistics. This will only report on Android.
        if (m_frame->isMainFrame())
            m_frame->document()->viewportDescription().reportMobilePageStats(m_frame);

        client()->dispatchDidFinishLoad();
    }
    m_loadType = FrameLoadTypeStandard;
    return true;
}

void FrameLoader::restoreScrollPositionAndViewState()
{
    FrameView* view = m_frame->view();
    if (!m_frame->page() || !view || !m_currentItem || !m_stateMachine.committedFirstRealDocumentLoad())
        return;

    if (!needsHistoryItemRestore(m_loadType))
        return;

    // This tries to balance 1. restoring as soon as possible, 2. detecting
    // clamping to avoid repeatedly popping the scroll position down as the
    // page height increases, 3. ignore clamp detection after load completes
    // because that may be because the page will never reach its previous
    // height.
    float mainFrameScale = m_frame->settings()->pinchVirtualViewportEnabled() ? 1 : m_currentItem->pageScaleFactor();
    bool canRestoreWithoutClamping = view->clampOffsetAtScale(m_currentItem->scrollPoint(), mainFrameScale) == m_currentItem->scrollPoint();
    bool canRestoreWithoutAnnoyingUser = !view->wasScrolledByUser() && (canRestoreWithoutClamping || m_state == FrameStateComplete);
    if (!canRestoreWithoutAnnoyingUser)
        return;

    if (m_frame->isMainFrame() && m_currentItem->pageScaleFactor()) {
        FloatPoint pinchViewportOffset(m_currentItem->pinchViewportScrollPoint());
        IntPoint frameScrollOffset(m_currentItem->scrollPoint());

        m_frame->page()->setPageScaleFactor(m_currentItem->pageScaleFactor(), frameScrollOffset);

        if (m_frame->settings()->pinchVirtualViewportEnabled()) {
            // If the pinch viewport's offset is (-1, -1) it means the history item
            // is an old version of HistoryItem so distribute the scroll between
            // the main frame and the pinch viewport as best as we can.
            // FIXME(bokan): This legacy distribution can be removed once the virtual viewport
            // pinch path is enabled on all platforms for at least one release.
            if (pinchViewportOffset.x() == -1 && pinchViewportOffset.y() == -1)
                pinchViewportOffset = FloatPoint(frameScrollOffset - view->scrollPosition());

            m_frame->host()->pinchViewport().setLocation(pinchViewportOffset);
        }
    } else {
        view->setScrollPositionNonProgrammatically(m_currentItem->scrollPoint());
    }

    if (m_frame->isMainFrame()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_frame->page()->scrollingCoordinator())
            scrollingCoordinator->frameViewRootLayerDidChange(view);
    }
}

void FrameLoader::detachChildren()
{
    typedef Vector<RefPtr<LocalFrame> > FrameVector;
    FrameVector childrenToDetach;
    childrenToDetach.reserveCapacity(m_frame->tree().childCount());
    for (Frame* child = m_frame->tree().lastChild(); child; child = child->tree().previousSibling()) {
        if (child->isLocalFrame())
            childrenToDetach.append(toLocalFrame(child));
    }
    FrameVector::iterator end = childrenToDetach.end();
    for (FrameVector::iterator it = childrenToDetach.begin(); it != end; ++it)
        (*it)->loader().detachFromParent();
}

// Called every time a resource is completely loaded or an error is received.
void FrameLoader::checkLoadComplete()
{
    ASSERT(client()->hasWebView());
    if (Page* page = m_frame->page()) {
        if (page->mainFrame()->isLocalFrame())
            page->deprecatedLocalMainFrame()->loader().checkLoadCompleteForThisFrame();
    }
}

String FrameLoader::userAgent(const KURL& url) const
{
    String userAgent = client()->userAgent(url);
    InspectorInstrumentation::applyUserAgentOverride(m_frame, &userAgent);
    return userAgent;
}

void FrameLoader::frameDetached()
{
    // stopAllLoaders can detach the LocalFrame, so protect it.
    RefPtr<LocalFrame> protect(m_frame);
    stopAllLoaders();
    detachFromParent();
}

void FrameLoader::detachFromParent()
{
    // Temporary explosions. We should never re-enter this code when this condition is true.
    RELEASE_ASSERT(!m_willDetachClient);

    // stopAllLoaders can detach the LocalFrame, so protect it.
    RefPtr<LocalFrame> protect(m_frame);

    closeURL();
    detachChildren();
    // stopAllLoaders() needs to be called after detachChildren(), because detachedChildren()
    // will trigger the unload event handlers of any child frames, and those event
    // handlers might start a new subresource load in this frame.
    stopAllLoaders();

    InspectorInstrumentation::frameDetachedFromParent(m_frame);

    if (m_documentLoader)
        m_documentLoader->detachFromFrame();
    m_documentLoader = nullptr;

    if (!client())
        return;

    TemporaryChange<bool> willDetachClient(m_willDetachClient, true);

    // FIXME: All this code belongs up in Page.
    Frame* parent = m_frame->tree().parent();
    if (parent && parent->isLocalFrame()) {
        m_frame->setView(nullptr);
        // FIXME: Shouldn't need to check if page() is null here.
        if (m_frame->owner() && m_frame->page())
            m_frame->page()->decrementSubframeCount();
        m_frame->willDetachFrameHost();
        detachClient();
        toLocalFrame(parent)->loader().scheduleCheckCompleted();
    } else {
        m_frame->setView(nullptr);
        m_frame->willDetachFrameHost();
        detachClient();
    }
    m_frame->detachFromFrameHost();
}

void FrameLoader::detachClient()
{
    ASSERT(client());

    // Finish all cleanup work that might require talking to the embedder.
    m_progressTracker.clear();
    setOpener(0);
    // Notify ScriptController that the frame is closing, since its cleanup ends up calling
    // back to FrameLoaderClient via V8WindowShell.
    m_frame->script().clearForClose();

    // client() should never be null because that means we somehow re-entered
    // the frame detach code... but it is sometimes.
    // FIXME: Understand why this is happening so we can document this insanity.
    if (client()) {
        // After this, we must no longer talk to the client since this clears
        // its owning reference back to our owning LocalFrame.
        client()->detachedFromParent();
        m_frame->clearClient();
    }
}

void FrameLoader::addHTTPOriginIfNeeded(ResourceRequest& request, const AtomicString& origin)
{
    if (!request.httpOrigin().isEmpty())
        return;  // Request already has an Origin header.

    // Don't send an Origin header for GET or HEAD to avoid privacy issues.
    // For example, if an intranet page has a hyperlink to an external web
    // site, we don't want to include the Origin of the request because it
    // will leak the internal host name. Similar privacy concerns have lead
    // to the widespread suppression of the Referer header at the network
    // layer.
    if (request.httpMethod() == "GET" || request.httpMethod() == "HEAD")
        return;

    // For non-GET and non-HEAD methods, always send an Origin header so the
    // server knows we support this feature.

    if (origin.isEmpty()) {
        // If we don't know what origin header to attach, we attach the value
        // for an empty origin.
        request.setHTTPOrigin(SecurityOrigin::createUnique()->toAtomicString());
        return;
    }

    request.setHTTPOrigin(origin);
}

void FrameLoader::receivedMainResourceError(const ResourceError& error)
{
    // Retain because the stop may release the last reference to it.
    RefPtr<LocalFrame> protect(m_frame);

    if (m_frame->document()->parser())
        m_frame->document()->parser()->stopParsing();

    // FIXME: We really ought to be able to just check for isCancellation() here, but there are some
    // ResourceErrors that setIsCancellation() but aren't created by ResourceError::cancelledError().
    ResourceError c(ResourceError::cancelledError(KURL()));
    if ((error.errorCode() != c.errorCode() || error.domain() != c.domain()) && m_frame->owner()) {
        // FIXME: For now, fallback content doesn't work cross process.
        ASSERT(m_frame->owner()->isLocal());
        m_frame->deprecatedLocalOwner()->renderFallbackContent();
    }

    checkCompleted();
    if (m_frame->page())
        checkLoadComplete();
}

bool FrameLoader::shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType loadType, const KURL& url)
{
    ASSERT(loadType != FrameLoadTypeReloadFromOrigin);
    // We don't do this if we are submitting a form with method other than "GET", explicitly reloading,
    // currently displaying a frameset, or if the URL does not have a fragment.
    return (!isFormSubmission || equalIgnoringCase(httpMethod, "GET"))
        && loadType != FrameLoadTypeReload
        && loadType != FrameLoadTypeSame
        && loadType != FrameLoadTypeBackForward
        && url.hasFragmentIdentifier()
        && equalIgnoringFragmentIdentifier(m_frame->document()->url(), url)
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && !m_frame->document()->isFrameSet();
}

void FrameLoader::scrollToFragmentWithParentBoundary(const KURL& url)
{
    FrameView* view = m_frame->view();
    if (!view)
        return;

    // Leaking scroll position to a cross-origin ancestor would permit the so-called "framesniffing" attack.
    RefPtr<LocalFrame> boundaryFrame(url.hasFragmentIdentifier() ? m_frame->document()->findUnsafeParentScrollPropagationBoundary() : 0);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(false);

    view->scrollToFragment(url);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(true);
}

bool FrameLoader::shouldClose()
{
    Page* page = m_frame->page();
    if (!page || !page->chrome().canRunBeforeUnloadConfirmPanel())
        return true;

    // Store all references to each subframe in advance since beforeunload's event handler may modify frame
    Vector<RefPtr<LocalFrame> > targetFrames;
    targetFrames.append(m_frame);
    for (Frame* child = m_frame->tree().firstChild(); child; child = child->tree().traverseNext(m_frame)) {
        // FIXME: There is not yet any way to dispatch events to out-of-process frames.
        if (child->isLocalFrame())
            targetFrames.append(toLocalFrame(child));
    }

    bool shouldClose = false;
    {
        NavigationDisablerForBeforeUnload navigationDisabler;
        size_t i;

        bool didAllowNavigation = false;
        for (i = 0; i < targetFrames.size(); i++) {
            if (!targetFrames[i]->tree().isDescendantOf(m_frame))
                continue;
            if (!targetFrames[i]->document()->dispatchBeforeUnloadEvent(page->chrome(), didAllowNavigation))
                break;
        }

        if (i == targetFrames.size())
            shouldClose = true;
    }
    return shouldClose;
}

void FrameLoader::loadWithNavigationAction(const NavigationAction& action, FrameLoadType type, PassRefPtrWillBeRawPtr<FormState> formState, const SubstituteData& substituteData, ClientRedirectPolicy clientRedirect, const AtomicString& overrideEncoding)
{
    ASSERT(client()->hasWebView());
    if (m_frame->document()->pageDismissalEventBeingDispatched() != Document::NoDismissal)
        return;

    const ResourceRequest& request = action.resourceRequest();

    // The current load should replace the history item if it is the first real
    // load of the frame.
    bool replacesCurrentHistoryItem = false;
    if (type == FrameLoadTypeRedirectWithLockedBackForwardList
        || !m_stateMachine.committedFirstRealDocumentLoad()) {
        replacesCurrentHistoryItem = true;
    }

    m_policyDocumentLoader = client()->createDocumentLoader(m_frame, request, substituteData.isValid() ? substituteData : defaultSubstituteDataForURL(request.url()));
    m_policyDocumentLoader->setTriggeringAction(action);
    m_policyDocumentLoader->setReplacesCurrentHistoryItem(replacesCurrentHistoryItem);
    m_policyDocumentLoader->setIsClientRedirect(clientRedirect == ClientRedirect);

    Frame* parent = m_frame->tree().parent();
    if (parent && parent->isLocalFrame())
        m_policyDocumentLoader->setOverrideEncoding(toLocalFrame(parent)->loader().documentLoader()->overrideEncoding());
    else if (!overrideEncoding.isEmpty())
        m_policyDocumentLoader->setOverrideEncoding(overrideEncoding);
    else if (m_documentLoader)
        m_policyDocumentLoader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    // stopAllLoaders can detach the LocalFrame, so protect it.
    RefPtr<LocalFrame> protect(m_frame);
    if ((!m_policyDocumentLoader->shouldContinueForNavigationPolicy(request) || !shouldClose()) && m_policyDocumentLoader) {
        m_policyDocumentLoader->detachFromFrame();
        m_policyDocumentLoader = nullptr;
        return;
    }

    if (m_provisionalDocumentLoader) {
        m_provisionalDocumentLoader->stopLoading();
        if (m_provisionalDocumentLoader)
            m_provisionalDocumentLoader->detachFromFrame();
        m_provisionalDocumentLoader = nullptr;
    }
    m_checkTimer.stop();

    // <rdar://problem/6250856> - In certain circumstances on pages with multiple frames, stopAllLoaders()
    // might detach the current FrameLoader, in which case we should bail on this newly defunct load.
    if (!m_frame->page() || !m_policyDocumentLoader)
        return;

    if (isLoadingMainFrame())
        m_frame->page()->inspectorController().resume();
    m_frame->navigationScheduler().cancel();

    m_provisionalDocumentLoader = m_policyDocumentLoader.release();
    m_loadType = type;
    m_state = FrameStateProvisional;

    if (formState)
        client()->dispatchWillSubmitForm(formState->form());

    m_progressTracker->progressStarted();
    if (m_provisionalDocumentLoader->isClientRedirect())
        m_provisionalDocumentLoader->appendRedirect(m_frame->document()->url());
    m_provisionalDocumentLoader->appendRedirect(m_provisionalDocumentLoader->request().url());
    client()->dispatchDidStartProvisionalLoad();
    ASSERT(m_provisionalDocumentLoader);
    m_provisionalDocumentLoader->startLoadingMainResource();
}

void FrameLoader::applyUserAgent(ResourceRequest& request)
{
    String userAgent = this->userAgent(request.url());
    ASSERT(!userAgent.isNull());
    request.setHTTPUserAgent(AtomicString(userAgent));
}

bool FrameLoader::shouldInterruptLoadForXFrameOptions(const String& content, const KURL& url, unsigned long requestIdentifier)
{
    UseCounter::count(m_frame->domWindow()->document(), UseCounter::XFrameOptions);

    Frame* topFrame = m_frame->tree().top();
    if (m_frame == topFrame)
        return false;

    XFrameOptionsDisposition disposition = parseXFrameOptionsHeader(content);

    switch (disposition) {
    case XFrameOptionsSameOrigin: {
        UseCounter::count(m_frame->domWindow()->document(), UseCounter::XFrameOptionsSameOrigin);
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
        // Out-of-process ancestors are always a different origin.
        if (!topFrame->isLocalFrame() || !origin->isSameSchemeHostPort(toLocalFrame(topFrame)->document()->securityOrigin()))
            return true;
        for (Frame* frame = m_frame->tree().parent(); frame; frame = frame->tree().parent()) {
            if (!frame->isLocalFrame() || !origin->isSameSchemeHostPort(toLocalFrame(frame)->document()->securityOrigin())) {
                UseCounter::count(m_frame->domWindow()->document(), UseCounter::XFrameOptionsSameOriginWithBadAncestorChain);
                break;
            }
        }
        return false;
    }
    case XFrameOptionsDeny:
        return true;
    case XFrameOptionsAllowAll:
        return false;
    case XFrameOptionsConflict:
        m_frame->document()->addConsoleMessageWithRequestIdentifier(JSMessageSource, ErrorMessageLevel, "Multiple 'X-Frame-Options' headers with conflicting values ('" + content + "') encountered when loading '" + url.elidedString() + "'. Falling back to 'DENY'.", requestIdentifier);
        return true;
    case XFrameOptionsInvalid:
        m_frame->document()->addConsoleMessageWithRequestIdentifier(JSMessageSource, ErrorMessageLevel, "Invalid 'X-Frame-Options' header encountered when loading '" + url.elidedString() + "': '" + content + "' is not a recognized directive. The header will be ignored.", requestIdentifier);
        return false;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool FrameLoader::shouldTreatURLAsSameAsCurrent(const KURL& url) const
{
    return m_currentItem && url == m_currentItem->url();
}

bool FrameLoader::shouldTreatURLAsSrcdocDocument(const KURL& url) const
{
    if (!equalIgnoringCase(url.string(), "about:srcdoc"))
        return false;
    HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
    if (!isHTMLIFrameElement(ownerElement))
        return false;
    return ownerElement->fastHasAttribute(srcdocAttr);
}

LocalFrame* FrameLoader::findFrameForNavigation(const AtomicString& name, Document* activeDocument)
{
    ASSERT(activeDocument);
    Frame* frame = m_frame->tree().find(name);
    if (!frame || !frame->isLocalFrame() || !activeDocument->canNavigate(toLocalFrame(*frame)))
        return 0;
    return toLocalFrame(frame);
}

void FrameLoader::loadHistoryItem(HistoryItem* item, HistoryLoadType historyLoadType, ResourceRequestCachePolicy cachePolicy)
{
    RefPtr<LocalFrame> protect(m_frame);
    if (m_frame->page()->defersLoading()) {
        m_deferredHistoryLoad = DeferredHistoryLoad(item, historyLoadType, cachePolicy);
        return;
    }

    m_provisionalItem = item;
    if (historyLoadType == HistorySameDocumentLoad) {
        loadInSameDocument(item->url(), item->stateObject(), FrameLoadTypeBackForward, NotClientRedirect);
        restoreScrollPositionAndViewState();
        return;
    }
    loadWithNavigationAction(NavigationAction(requestFromHistoryItem(item, cachePolicy), FrameLoadTypeBackForward), FrameLoadTypeBackForward, nullptr, SubstituteData());
}

void FrameLoader::dispatchDocumentElementAvailable()
{
    client()->documentElementAvailable();
}

void FrameLoader::dispatchDidClearDocumentOfWindowObject()
{
    if (!m_frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return;

    if (Page* page = m_frame->page())
        page->inspectorController().didClearDocumentOfWindowObject(m_frame);
    InspectorInstrumentation::didClearDocumentOfWindowObject(m_frame);

    // We just cleared the document, not the entire window object, but for the
    // embedder that's close enough.
    client()->dispatchDidClearWindowObjectInMainWorld();
}

void FrameLoader::dispatchDidClearWindowObjectInMainWorld()
{
    if (!m_frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return;

    client()->dispatchDidClearWindowObjectInMainWorld();
}

SandboxFlags FrameLoader::effectiveSandboxFlags() const
{
    SandboxFlags flags = m_forcedSandboxFlags;
    // FIXME: We need a way to propagate sandbox flags to out-of-process frames.
    Frame* parentFrame = m_frame->tree().parent();
    if (parentFrame && parentFrame->isLocalFrame())
        flags |= toLocalFrame(parentFrame)->document()->sandboxFlags();
    if (FrameOwner* frameOwner = m_frame->owner())
        flags |= frameOwner->sandboxFlags();
    return flags;
}

} // namespace WebCore
