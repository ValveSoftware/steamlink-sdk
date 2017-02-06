/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/Document.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/HTMLScriptElementOrSVGScriptElement.h"
#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V0CustomElementConstructorBuilder.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/HTMLElementFactory.h"
#include "core/HTMLNames.h"
#include "core/SVGElementFactory.h"
#include "core/SVGNames.h"
#include "core/XMLNSNames.h"
#include "core/XMLNames.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorPendingAnimations.h"
#include "core/animation/DocumentAnimations.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/FontFaceSet.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/invalidation/StyleInvalidator.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Attr.h"
#include "core/dom/CDATASection.h"
#include "core/dom/ClientRect.h"
#include "core/dom/Comment.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/ElementDataCache.h"
#include "core/dom/ElementRegistrationOptions.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/IntersectionObserverController.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/dom/MainThreadTaskRunner.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/NodeChildRemovalTracker.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeFilter.h"
#include "core/dom/NodeIntersectionObserverData.h"
#include "core/dom/NodeIterator.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/NodeWithIndex.h"
#include "core/dom/NthIndexCache.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ScriptRunner.h"
#include "core/dom/ScriptedAnimationController.h"
#include "core/dom/ScriptedIdleTaskController.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TouchList.h"
#include "core/dom/TransformSource.h"
#include "core/dom/TreeWalker.h"
#include "core/dom/VisitedLinkState.h"
#include "core/dom/XMLDocument.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/V0CustomElementMicrotaskRunQueue.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/DragCaretController.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/BeforeUnloadEvent.h"
#include "core/events/Event.h"
#include "core/events/EventFactory.h"
#include "core/events/EventListener.h"
#include "core/events/HashChangeEvent.h"
#include "core/events/PageTransitionEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/DOMVisualViewport.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/History.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLBaseElement.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLScriptElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/HTMLTitleElement.h"
#include "core/html/PluginDocument.h"
#include "core/html/WindowNameCollection.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasFontCache.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/forms/FormController.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/NestingLevelIncrementer.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/InstanceCounters.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/CookieJar.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameFetchContext.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/ImageLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/ChromeClient.h"
#include "core/page/EventWithHitTestResults.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "core/page/scrolling/ScrollStateCallback.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/SnapCoordinator.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGScriptElement.h"
#include "core/svg/SVGTitleElement.h"
#include "core/svg/SVGUseElement.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "core/workers/SharedWorkerRepositoryClient.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "platform/DateComponents.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/Histogram.h"
#include "platform/Language.h"
#include "platform/LengthFunctions.h"
#include "platform/Logging.h"
#include "platform/PluginScriptForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/HTTPParsers.h"
#include "platform/scheduler/CancellableTaskFactory.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/text/PlatformLocale.h"
#include "platform/text/SegmentedString.h"
#include "platform/weborigin/OriginAccessEntry.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebScheduler.h"
#include "wtf/CurrentTime.h"
#include "wtf/DateMath.h"
#include "wtf/Functional.h"
#include "wtf/HashFunctions.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/TextEncodingRegistry.h"
#include <memory>

using namespace WTF;
using namespace Unicode;

#ifndef NDEBUG
using WeakDocumentSet = blink::PersistentHeapHashSet<blink::WeakMember<blink::Document>>;
static WeakDocumentSet& liveDocumentSet();
#endif

namespace blink {

using namespace HTMLNames;

static const unsigned cMaxWriteRecursionDepth = 21;

// This amount of time must have elapsed before we will even consider scheduling a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is adequate, but a little high
// for dual G5s. :)
static const int cLayoutScheduleThreshold = 250;

// DOM Level 2 says (letters added):
//
// a) Name start characters must have one of the categories Ll, Lu, Lo, Lt, Nl.
// b) Name characters other than Name-start characters must have one of the categories Mc, Me, Mn, Lm, or Nd.
// c) Characters in the compatibility area (i.e. with character code greater than #xF900 and less than #xFFFE) are not allowed in XML names.
// d) Characters which have a font or compatibility decomposition (i.e. those with a "compatibility formatting tag" in field 5 of the database -- marked by field 5 beginning with a "<") are not allowed.
// e) The following characters are treated as name-start characters rather than name characters, because the property file classifies them as Alphabetic: [#x02BB-#x02C1], #x0559, #x06E5, #x06E6.
// f) Characters #x20DD-#x20E0 are excluded (in accordance with Unicode, section 5.14).
// g) Character #x00B7 is classified as an extender, because the property list so identifies it.
// h) Character #x0387 is added as a name character, because #x00B7 is its canonical equivalent.
// i) Characters ':' and '_' are allowed as name-start characters.
// j) Characters '-' and '.' are allowed as name characters.
//
// It also contains complete tables. If we decide it's better, we could include those instead of the following code.

static inline bool isValidNameStart(UChar32 c)
{
    // rule (e) above
    if ((c >= 0x02BB && c <= 0x02C1) || c == 0x559 || c == 0x6E5 || c == 0x6E6)
        return true;

    // rule (i) above
    if (c == ':' || c == '_')
        return true;

    // rules (a) and (f) above
    const uint32_t nameStartMask = Letter_Lowercase | Letter_Uppercase | Letter_Other | Letter_Titlecase | Number_Letter;
    if (!(Unicode::category(c) & nameStartMask))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    CharDecompositionType decompType = decompositionType(c);
    if (decompType == DecompositionFont || decompType == DecompositionCompat)
        return false;

    return true;
}

static inline bool isValidNamePart(UChar32 c)
{
    // rules (a), (e), and (i) above
    if (isValidNameStart(c))
        return true;

    // rules (g) and (h) above
    if (c == 0x00B7 || c == 0x0387)
        return true;

    // rule (j) above
    if (c == '-' || c == '.')
        return true;

    // rules (b) and (f) above
    const uint32_t otherNamePartMask = Mark_NonSpacing | Mark_Enclosing | Mark_SpacingCombining | Letter_Modifier | Number_DecimalDigit;
    if (!(Unicode::category(c) & otherNamePartMask))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    CharDecompositionType decompType = decompositionType(c);
    if (decompType == DecompositionFont || decompType == DecompositionCompat)
        return false;

    return true;
}

static bool shouldInheritSecurityOriginFromOwner(const KURL& url)
{
    // http://www.whatwg.org/specs/web-apps/current-work/#origin-0
    //
    // If a Document has the address "about:blank"
    //     The origin of the Document is the origin it was assigned when its browsing context was created.
    //
    // Note: We generalize this to all "blank" URLs and invalid URLs because we
    // treat all of these URLs as about:blank.
    //
    return url.isEmpty() || url.protocolIsAbout();
}

static Widget* widgetForElement(const Element& focusedElement)
{
    LayoutObject* layoutObject = focusedElement.layoutObject();
    if (!layoutObject || !layoutObject->isLayoutPart())
        return 0;
    return toLayoutPart(layoutObject)->widget();
}

static bool acceptsEditingFocus(const Element& element)
{
    DCHECK(element.hasEditableStyle());

    return element.document().frame() && element.rootEditableElement();
}

uint64_t Document::s_globalTreeVersion = 0;

static bool s_threadedParsingEnabledForTesting = true;

// This doesn't work with non-Document ExecutionContext.
static void runAutofocusTask(ExecutionContext* context)
{
    Document* document = toDocument(context);
    if (Element* element = document->autofocusElement()) {
        document->setAutofocusElement(0);
        element->focus();
    }
}

Document::Document(const DocumentInit& initializer, DocumentClassFlags documentClasses)
    : ContainerNode(0, CreateDocument)
    , TreeScope(*this)
    , m_hasNodesWithPlaceholderStyle(false)
    , m_evaluateMediaQueriesOnStyleRecalc(false)
    , m_pendingSheetLayout(NoLayoutWithPendingSheets)
    , m_frame(initializer.frame())
    , m_domWindow(m_frame ? m_frame->localDOMWindow() : 0)
    , m_importsController(initializer.importsController())
    , m_contextFeatures(ContextFeatures::defaultSwitch())
    , m_wellFormed(false)
    , m_printing(false)
    , m_wasPrinting(false)
    , m_paginatedForScreen(false)
    , m_compatibilityMode(NoQuirksMode)
    , m_compatibilityModeLocked(false)
    , m_executeScriptsWaitingForResourcesTask(CancellableTaskFactory::create(this, &Document::executeScriptsWaitingForResources))
    , m_hasAutofocused(false)
    , m_clearFocusedElementTimer(this, &Document::clearFocusedElementTimerFired)
    , m_domTreeVersion(++s_globalTreeVersion)
    , m_styleVersion(0)
    , m_listenerTypes(0)
    , m_mutationObserverTypes(0)
    , m_visitedLinkState(VisitedLinkState::create(*this))
    , m_visuallyOrdered(false)
    , m_readyState(Complete)
    , m_parsingState(FinishedParsing)
    , m_gotoAnchorNeededAfterStylesheetsLoad(false)
    , m_containsValidityStyleRules(false)
    , m_containsPlugins(false)
    , m_updateFocusAppearanceSelectionBahavior(SelectionBehaviorOnFocus::Reset)
    , m_ignoreDestructiveWriteCount(0)
    , m_markers(new DocumentMarkerController(*this))
    , m_updateFocusAppearanceTimer(this, &Document::updateFocusAppearanceTimerFired)
    , m_cssTarget(nullptr)
    , m_loadEventProgress(LoadEventNotRun)
    , m_startTime(currentTime())
    , m_scriptRunner(ScriptRunner::create(this))
    , m_xmlVersion("1.0")
    , m_xmlStandalone(StandaloneUnspecified)
    , m_hasXMLDeclaration(0)
    , m_designMode(false)
    , m_isRunningExecCommand(false)
    , m_hasAnnotatedRegions(false)
    , m_annotatedRegionsDirty(false)
    , m_useSecureKeyboardEntryWhenActive(false)
    , m_documentClasses(documentClasses)
    , m_isViewSource(false)
    , m_sawElementsInKnownNamespaces(false)
    , m_isSrcdocDocument(false)
    , m_isMobileDocument(false)
    , m_layoutView(0)
    , m_contextDocument(initializer.contextDocument())
    , m_hasFullscreenSupplement(false)
    , m_loadEventDelayCount(0)
    , m_loadEventDelayTimer(this, &Document::loadEventDelayTimerFired)
    , m_pluginLoadingTimer(this, &Document::pluginLoadingTimerFired)
    , m_documentTiming(*this)
    , m_writeRecursionIsTooDeep(false)
    , m_writeRecursionDepth(0)
    , m_taskRunner(MainThreadTaskRunner::create(this))
    , m_registrationContext(initializer.registrationContext(this))
    , m_elementDataCacheClearTimer(this, &Document::elementDataCacheClearTimerFired)
    , m_timeline(AnimationTimeline::create(this))
    , m_compositorPendingAnimations(new CompositorPendingAnimations())
    , m_templateDocumentHost(nullptr)
    , m_didAssociateFormControlsTimer(this, &Document::didAssociateFormControlsTimerFired)
    , m_timers(timerTaskRunner()->adoptClone())
    , m_hasViewportUnits(false)
    , m_parserSyncPolicy(AllowAsynchronousParsing)
    , m_nodeCount(0)
{
    if (m_frame) {
        DCHECK(m_frame->page());
        provideContextFeaturesToDocumentFrom(*this, *m_frame->page());

        m_fetcher = m_frame->loader().documentLoader()->fetcher();
        FrameFetchContext::provideDocumentToContext(m_fetcher->context(), this);
    } else if (m_importsController) {
        m_fetcher = FrameFetchContext::createContextAndFetcher(nullptr, this);
    } else {
        m_fetcher = ResourceFetcher::create(nullptr);
    }

    ViewportScrollCallback* applyScroll = nullptr;
    if (isInMainFrame()) {
        applyScroll = RootScrollerController::createViewportApplyScroll(
            &frameHost()->topControls(), &frameHost()->overscrollController());
    } else {
        applyScroll =
            RootScrollerController::createViewportApplyScroll(nullptr, nullptr);
    }

    m_rootScrollerController =
        RootScrollerController::create(*this, applyScroll);

    // We depend on the url getting immediately set in subframes, but we
    // also depend on the url NOT getting immediately set in opened windows.
    // See fast/dom/early-frame-url.html
    // and fast/dom/location-new-window-no-crash.html, respectively.
    // FIXME: Can/should we unify this behavior?
    if (initializer.shouldSetURL())
        setURL(initializer.url());

    initSecurityContext(initializer);
    initDNSPrefetch();

    InstanceCounters::incrementCounter(InstanceCounters::DocumentCounter);

    m_lifecycle.advanceTo(DocumentLifecycle::Inactive);

    // Since CSSFontSelector requires Document::m_fetcher and StyleEngine owns
    // CSSFontSelector, need to initialize m_styleEngine after initializing
    // m_fetcher.
    m_styleEngine = StyleEngine::create(*this);

    // The parent's parser should be suspended together with all the other objects,
    // else this new Document would have a new ExecutionContext which suspended state
    // would not match the one from the parent, and could start loading resources
    // ignoring the defersLoading flag.
    DCHECK(!parentDocument() || !parentDocument()->activeDOMObjectsAreSuspended());

#ifndef NDEBUG
    liveDocumentSet().add(this);
#endif
}

Document::~Document()
{
    DCHECK(!layoutView());
    DCHECK(!parentTreeScope());
    // If a top document with a cache, verify that it was comprehensively
    // cleared during detach.
    DCHECK(!m_axObjectCache);
    InstanceCounters::decrementCounter(InstanceCounters::DocumentCounter);
}

SelectorQueryCache& Document::selectorQueryCache()
{
    if (!m_selectorQueryCache)
        m_selectorQueryCache = wrapUnique(new SelectorQueryCache());
    return *m_selectorQueryCache;
}

MediaQueryMatcher& Document::mediaQueryMatcher()
{
    if (!m_mediaQueryMatcher)
        m_mediaQueryMatcher = MediaQueryMatcher::create(*this);
    return *m_mediaQueryMatcher;
}

void Document::mediaQueryAffectingValueChanged()
{
    m_evaluateMediaQueriesOnStyleRecalc = true;
    styleEngine().clearMediaQueryRuleSetStyleSheets();
    InspectorInstrumentation::mediaQueryResultChanged(this);
}

void Document::setCompatibilityMode(CompatibilityMode mode)
{
    if (m_compatibilityModeLocked || mode == m_compatibilityMode)
        return;
    m_compatibilityMode = mode;
    selectorQueryCache().invalidate();
}

String Document::compatMode() const
{
    return inQuirksMode() ? "BackCompat" : "CSS1Compat";
}

void Document::setDoctype(DocumentType* docType)
{
    // This should never be called more than once.
    DCHECK(!m_docType || !docType);
    m_docType = docType;
    if (m_docType) {
        this->adoptIfNeeded(*m_docType);
        if (m_docType->publicId().startsWith("-//wapforum//dtd xhtml mobile 1.", TextCaseInsensitive))
            m_isMobileDocument = true;
    }
    // Doctype affects the interpretation of the stylesheets.
    styleEngine().clearResolver();
}

DOMImplementation& Document::implementation()
{
    if (!m_implementation)
        m_implementation = DOMImplementation::create(*this);
    return *m_implementation;
}

bool Document::hasAppCacheManifest() const
{
    return isHTMLHtmlElement(documentElement()) && documentElement()->hasAttribute(manifestAttr);
}

Location* Document::location() const
{
    if (!frame())
        return 0;

    return domWindow()->location();
}

void Document::childrenChanged(const ChildrenChange& change)
{
    ContainerNode::childrenChanged(change);
    m_documentElement = ElementTraversal::firstWithin(*this);

    // For non-HTML documents the willInsertBody notification won't happen
    // so we resume as soon as we have a document element. Even for XHTML
    // documents there may never be a <body> (since the parser won't always
    // insert one), so we resume here too. That does mean XHTML documents make
    // frames when there's only a <head>, but such documents are pretty rare.
    if (m_documentElement && !isHTMLDocument())
        beginLifecycleUpdatesIfRenderingReady();
}

void Document::setRootScroller(Element* newScroller, ExceptionState& exceptionState)
{
    m_rootScrollerController->set(newScroller);
}

Element* Document::rootScroller() const
{
    return m_rootScrollerController->get();
}

const Element* Document::effectiveRootScroller() const
{
    return m_rootScrollerController->effectiveRootScroller();
}

bool Document::isViewportScrollCallback(const ScrollStateCallback* callback)
{
    if (!callback)
        return false;

    return callback == m_rootScrollerController->viewportScrollCallback();
}

bool Document::isInMainFrame() const
{
    return frame() && frame()->isMainFrame();
}

AtomicString Document::convertLocalName(const AtomicString& name)
{
    return isHTMLDocument() ? name.lower() : name;
}

Element* Document::createElement(const AtomicString& name, ExceptionState& exceptionState)
{
    if (!isValidName(name)) {
        exceptionState.throwDOMException(InvalidCharacterError, "The tag name provided ('" + name + "') is not a valid name.");
        return nullptr;
    }

    if (isXHTMLDocument() || isHTMLDocument()) {
        if (CustomElement::shouldCreateCustomElement(*this, name))
            return CustomElement::createCustomElementSync(*this, name, exceptionState);
        return HTMLElementFactory::createHTMLElement(convertLocalName(name), *this, 0, CreatedByCreateElement);
    }

    return Element::create(QualifiedName(nullAtom, name, nullAtom), this);
}

Element* Document::createElement(const AtomicString& localName, const AtomicString& typeExtension, ExceptionState& exceptionState)
{
    if (!isValidName(localName)) {
        exceptionState.throwDOMException(InvalidCharacterError, "The tag name provided ('" + localName + "') is not a valid name.");
        return nullptr;
    }

    Element* element;

    if (CustomElement::shouldCreateCustomElement(*this, localName)) {
        element = CustomElement::createCustomElementSync(*this, localName, exceptionState);
    } else if (V0CustomElement::isValidName(localName) && registrationContext()) {
        element = registrationContext()->createCustomTagElement(*this, QualifiedName(nullAtom, convertLocalName(localName), xhtmlNamespaceURI));
    } else {
        element = createElement(localName, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }

    if (!typeExtension.isEmpty())
        V0CustomElementRegistrationContext::setIsAttributeAndTypeExtension(element, typeExtension);

    return element;
}

static inline QualifiedName createQualifiedName(const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionState& exceptionState)
{
    AtomicString prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, exceptionState))
        return QualifiedName::null();

    QualifiedName qName(prefix, localName, namespaceURI);
    if (!Document::hasValidNamespaceForElements(qName)) {
        exceptionState.throwDOMException(NamespaceError, "The namespace URI provided ('" + namespaceURI + "') is not valid for the qualified name provided ('" + qualifiedName + "').");
        return QualifiedName::null();
    }

    return qName;
}

Element* Document::createElementNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionState& exceptionState)
{
    QualifiedName qName(createQualifiedName(namespaceURI, qualifiedName, exceptionState));
    if (qName == QualifiedName::null())
        return nullptr;

    if (CustomElement::shouldCreateCustomElement(*this, qName))
        return CustomElement::createCustomElementSync(*this, qName, exceptionState);
    return createElement(qName, CreatedByCreateElement);
}

Element* Document::createElementNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& typeExtension, ExceptionState& exceptionState)
{
    QualifiedName qName(createQualifiedName(namespaceURI, qualifiedName, exceptionState));
    if (qName == QualifiedName::null())
        return nullptr;

    Element* element;
    if (CustomElement::shouldCreateCustomElement(*this, qName))
        element = CustomElement::createCustomElementSync(*this, qName, exceptionState);
    else if (V0CustomElement::isValidName(qName.localName()) && registrationContext())
        element = registrationContext()->createCustomTagElement(*this, qName);
    else
        element = createElement(qName, CreatedByCreateElement);

    if (!typeExtension.isEmpty())
        V0CustomElementRegistrationContext::setIsAttributeAndTypeExtension(element, typeExtension);

    return element;
}

ScriptValue Document::registerElement(ScriptState* scriptState, const AtomicString& name, const ElementRegistrationOptions& options, ExceptionState& exceptionState, V0CustomElement::NameSet validNames)
{
    HostsUsingFeatures::countMainWorldOnly(scriptState, *this, HostsUsingFeatures::Feature::DocumentRegisterElement);

    if (!registrationContext()) {
        exceptionState.throwDOMException(NotSupportedError, "No element registration context is available.");
        return ScriptValue();
    }

    V0CustomElementConstructorBuilder constructorBuilder(scriptState, options);
    registrationContext()->registerElement(this, &constructorBuilder, name, validNames, exceptionState);
    return constructorBuilder.bindingsReturnValue();
}

V0CustomElementMicrotaskRunQueue* Document::customElementMicrotaskRunQueue()
{
    if (!m_customElementMicrotaskRunQueue)
        m_customElementMicrotaskRunQueue = V0CustomElementMicrotaskRunQueue::create();
    return m_customElementMicrotaskRunQueue.get();
}

void Document::setImportsController(HTMLImportsController* controller)
{
    DCHECK(!m_importsController || !controller);
    m_importsController = controller;
    if (!m_importsController && !loader())
        m_fetcher->clearContext();
}

HTMLImportLoader* Document::importLoader() const
{
    if (!m_importsController)
        return 0;
    return m_importsController->loaderFor(*this);
}

bool Document::haveImportsLoaded() const
{
    if (!m_importsController)
        return true;
    return !m_importsController->shouldBlockScriptExecution(*this);
}

LocalDOMWindow* Document::executingWindow()
{
    if (LocalDOMWindow* owningWindow = domWindow())
        return owningWindow;
    if (HTMLImportsController* import = this->importsController())
        return import->master()->domWindow();
    return 0;
}

LocalFrame* Document::executingFrame()
{
    LocalDOMWindow* window = executingWindow();
    if (!window)
        return 0;
    return window->frame();
}

DocumentFragment* Document::createDocumentFragment()
{
    return DocumentFragment::create(*this);
}

Text* Document::createTextNode(const String& data)
{
    return Text::create(*this, data);
}

Comment* Document::createComment(const String& data)
{
    return Comment::create(*this, data);
}

CDATASection* Document::createCDATASection(const String& data, ExceptionState& exceptionState)
{
    if (isHTMLDocument()) {
        exceptionState.throwDOMException(NotSupportedError, "This operation is not supported for HTML documents.");
        return nullptr;
    }
    if (data.contains("]]>")) {
        exceptionState.throwDOMException(InvalidCharacterError, "String cannot contain ']]>' since that is the end delimiter of a CData section.");
        return nullptr;
    }
    return CDATASection::create(*this, data);
}

ProcessingInstruction* Document::createProcessingInstruction(const String& target, const String& data, ExceptionState& exceptionState)
{
    if (!isValidName(target)) {
        exceptionState.throwDOMException(InvalidCharacterError, "The target provided ('" + target + "') is not a valid name.");
        return nullptr;
    }
    if (data.contains("?>")) {
        exceptionState.throwDOMException(InvalidCharacterError, "The data provided ('" + data + "') contains '?>'.");
        return nullptr;
    }
    return ProcessingInstruction::create(*this, target, data);
}

Text* Document::createEditingTextNode(const String& text)
{
    return Text::createEditingText(*this, text);
}

bool Document::importContainerNodeChildren(ContainerNode* oldContainerNode, ContainerNode* newContainerNode, ExceptionState& exceptionState)
{
    for (Node& oldChild : NodeTraversal::childrenOf(*oldContainerNode)) {
        Node* newChild = importNode(&oldChild, true, exceptionState);
        if (exceptionState.hadException())
            return false;
        newContainerNode->appendChild(newChild, exceptionState);
        if (exceptionState.hadException())
            return false;
    }

    return true;
}

Node* Document::importNode(Node* importedNode, bool deep, ExceptionState& exceptionState)
{
    switch (importedNode->getNodeType()) {
    case TEXT_NODE:
        return createTextNode(importedNode->nodeValue());
    case CDATA_SECTION_NODE:
        return CDATASection::create(*this, importedNode->nodeValue());
    case PROCESSING_INSTRUCTION_NODE:
        return createProcessingInstruction(importedNode->nodeName(), importedNode->nodeValue(), exceptionState);
    case COMMENT_NODE:
        return createComment(importedNode->nodeValue());
    case DOCUMENT_TYPE_NODE: {
        DocumentType* doctype = toDocumentType(importedNode);
        return DocumentType::create(this, doctype->name(), doctype->publicId(), doctype->systemId());
    }
    case ELEMENT_NODE: {
        Element* oldElement = toElement(importedNode);
        // FIXME: The following check might be unnecessary. Is it possible that
        // oldElement has mismatched prefix/namespace?
        if (!hasValidNamespaceForElements(oldElement->tagQName())) {
            exceptionState.throwDOMException(NamespaceError, "The imported node has an invalid namespace.");
            return nullptr;
        }
        Element* newElement = createElement(oldElement->tagQName(), CreatedByImportNode);

        newElement->cloneDataFromElement(*oldElement);

        if (deep) {
            if (!importContainerNodeChildren(oldElement, newElement, exceptionState))
                return nullptr;
            if (isHTMLTemplateElement(*oldElement)
                && !ensureTemplateDocument().importContainerNodeChildren(
                    toHTMLTemplateElement(oldElement)->content(),
                    toHTMLTemplateElement(newElement)->content(), exceptionState))
                return nullptr;
        }

        return newElement;
    }
    case ATTRIBUTE_NODE:
        return Attr::create(*this, QualifiedName(nullAtom, AtomicString(toAttr(importedNode)->name()), nullAtom), toAttr(importedNode)->value());
    case DOCUMENT_FRAGMENT_NODE: {
        if (importedNode->isShadowRoot()) {
            // ShadowRoot nodes should not be explicitly importable.
            // Either they are imported along with their host node, or created implicitly.
            exceptionState.throwDOMException(NotSupportedError, "The node provided is a shadow root, which may not be imported.");
            return nullptr;
        }
        DocumentFragment* oldFragment = toDocumentFragment(importedNode);
        DocumentFragment* newFragment = createDocumentFragment();
        if (deep && !importContainerNodeChildren(oldFragment, newFragment, exceptionState))
            return nullptr;

        return newFragment;
    }
    case DOCUMENT_NODE:
        exceptionState.throwDOMException(NotSupportedError, "The node provided is a document, which may not be imported.");
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

Node* Document::adoptNode(Node* source, ExceptionState& exceptionState)
{
    EventQueueScope scope;

    switch (source->getNodeType()) {
    case DOCUMENT_NODE:
        exceptionState.throwDOMException(NotSupportedError, "The node provided is of type '" + source->nodeName() + "', which may not be adopted.");
        return nullptr;
    case ATTRIBUTE_NODE: {
        Attr* attr = toAttr(source);
        if (Element* ownerElement = attr->ownerElement())
            ownerElement->removeAttributeNode(attr, exceptionState);
        break;
    }
    default:
        if (source->isShadowRoot()) {
            // ShadowRoot cannot disconnect itself from the host node.
            exceptionState.throwDOMException(HierarchyRequestError, "The node provided is a shadow root, which may not be adopted.");
            return nullptr;
        }

        if (source->isFrameOwnerElement()) {
            HTMLFrameOwnerElement* frameOwnerElement = toHTMLFrameOwnerElement(source);
            if (frame() && frame()->tree().isDescendantOf(frameOwnerElement->contentFrame())) {
                exceptionState.throwDOMException(HierarchyRequestError, "The node provided is a frame which contains this document.");
                return nullptr;
            }
        }
        if (source->parentNode()) {
            source->parentNode()->removeChild(source, exceptionState);
            if (exceptionState.hadException())
                return nullptr;
            RELEASE_ASSERT(!source->parentNode());
        }
    }

    this->adoptIfNeeded(*source);

    return source;
}

bool Document::hasValidNamespaceForElements(const QualifiedName& qName)
{
    // These checks are from DOM Core Level 2, createElementNS
    // http://www.w3.org/TR/DOM-Level-2-Core/core.html#ID-DocCrElNS
    if (!qName.prefix().isEmpty() && qName.namespaceURI().isNull()) // createElementNS(null, "html:div")
        return false;
    if (qName.prefix() == xmlAtom && qName.namespaceURI() != XMLNames::xmlNamespaceURI) // createElementNS("http://www.example.com", "xml:lang")
        return false;

    // Required by DOM Level 3 Core and unspecified by DOM Level 2 Core:
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html#ID-DocCrElNS
    // createElementNS("http://www.w3.org/2000/xmlns/", "foo:bar"), createElementNS(null, "xmlns:bar"), createElementNS(null, "xmlns")
    if (qName.prefix() == xmlnsAtom || (qName.prefix().isEmpty() && qName.localName() == xmlnsAtom))
        return qName.namespaceURI() == XMLNSNames::xmlnsNamespaceURI;
    return qName.namespaceURI() != XMLNSNames::xmlnsNamespaceURI;
}

bool Document::hasValidNamespaceForAttributes(const QualifiedName& qName)
{
    return hasValidNamespaceForElements(qName);
}

// FIXME: This should really be in a possible ElementFactory class
Element* Document::createElement(const QualifiedName& qName, CreateElementFlags flags)
{
    Element* e = nullptr;

    // FIXME: Use registered namespaces and look up in a hash to find the right factory.
    if (qName.namespaceURI() == xhtmlNamespaceURI)
        e = HTMLElementFactory::createHTMLElement(qName.localName(), *this, 0, flags);
    else if (qName.namespaceURI() == SVGNames::svgNamespaceURI)
        e = SVGElementFactory::createSVGElement(qName.localName(), *this, flags);

    if (e)
        m_sawElementsInKnownNamespaces = true;
    else
        e = Element::create(qName, this);

    if (e->prefix() != qName.prefix())
        e->setTagNameForCreateElementNS(qName);

    DCHECK(qName == e->tagQName());

    return e;
}

String Document::readyState() const
{
    DEFINE_STATIC_LOCAL(const String, loading, ("loading"));
    DEFINE_STATIC_LOCAL(const String, interactive, ("interactive"));
    DEFINE_STATIC_LOCAL(const String, complete, ("complete"));

    switch (m_readyState) {
    case Loading:
        return loading;
    case Interactive:
        return interactive;
    case Complete:
        return complete;
    }

    ASSERT_NOT_REACHED();
    return String();
}

void Document::setReadyState(ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    switch (readyState) {
    case Loading:
        if (!m_documentTiming.domLoading()) {
            m_documentTiming.markDomLoading();
        }
        break;
    case Interactive:
        if (!m_documentTiming.domInteractive())
            m_documentTiming.markDomInteractive();
        break;
    case Complete:
        if (!m_documentTiming.domComplete())
            m_documentTiming.markDomComplete();
        break;
    }

    m_readyState = readyState;
    dispatchEvent(Event::create(EventTypeNames::readystatechange));
}

bool Document::isLoadCompleted()
{
    return m_readyState == Complete;
}

AtomicString Document::encodingName() const
{
    // TextEncoding::name() returns a char*, no need to allocate a new
    // String for it each time.
    // FIXME: We should fix TextEncoding to speak AtomicString anyway.
    return AtomicString(encoding().name());
}

void Document::setContentLanguage(const AtomicString& language)
{
    if (m_contentLanguage == language)
        return;
    m_contentLanguage = language;

    // Document's style depends on the content language.
    setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Language));
}

void Document::setXMLVersion(const String& version, ExceptionState& exceptionState)
{
    if (!XMLDocumentParser::supportsXMLVersion(version)) {
        exceptionState.throwDOMException(NotSupportedError, "This document does not support the XML version '" + version + "'.");
        return;
    }

    m_xmlVersion = version;
}

void Document::setXMLStandalone(bool standalone, ExceptionState& exceptionState)
{
    m_xmlStandalone = standalone ? Standalone : NotStandalone;
}

void Document::setContent(const String& content)
{
    open();
    m_parser->append(content);
    close();
}

String Document::suggestedMIMEType() const
{
    if (isXMLDocument()) {
        if (isXHTMLDocument())
            return "application/xhtml+xml";
        if (isSVGDocument())
            return "image/svg+xml";
        return "application/xml";
    }
    if (xmlStandalone())
        return "text/xml";
    if (isHTMLDocument())
        return "text/html";

    if (DocumentLoader* documentLoader = loader())
        return documentLoader->responseMIMEType();
    return String();
}

void Document::setMimeType(const AtomicString& mimeType)
{
    m_mimeType = mimeType;
}

AtomicString Document::contentType() const
{
    if (!m_mimeType.isEmpty())
        return m_mimeType;

    if (DocumentLoader* documentLoader = loader())
        return documentLoader->mimeType();

    String mimeType = suggestedMIMEType();
    if (!mimeType.isEmpty())
        return AtomicString(mimeType);

    return AtomicString("application/xml");
}

Element* Document::elementFromPoint(int x, int y) const
{
    if (!layoutView())
        return 0;

    return TreeScope::elementFromPoint(x, y);
}

HeapVector<Member<Element>> Document::elementsFromPoint(int x, int y) const
{
    if (!layoutView())
        return HeapVector<Member<Element>>();
    return TreeScope::elementsFromPoint(x, y);
}

Range* Document::caretRangeFromPoint(int x, int y)
{
    if (!layoutView())
        return nullptr;

    HitTestResult result = hitTestInDocument(this, x, y);
    PositionWithAffinity positionWithAffinity = result.position();
    if (positionWithAffinity.position().isNull())
        return nullptr;

    Position rangeCompliantPosition = positionWithAffinity.position().parentAnchoredEquivalent();
    return Range::createAdjustedToTreeScope(*this, rangeCompliantPosition);
}

Element* Document::scrollingElement()
{
    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        if (inQuirksMode()) {
            updateStyleAndLayoutTree();
            HTMLBodyElement* body = firstBodyElement();
            if (body && body->layoutObject() && body->layoutObject()->hasOverflowClip())
                return nullptr;

            return body;
        }

        return documentElement();
    }

    return body();
}

/*
 * Performs three operations:
 *  1. Convert control characters to spaces
 *  2. Trim leading and trailing spaces
 *  3. Collapse internal whitespace.
 */
template <typename CharacterType>
static inline String canonicalizedTitle(Document* document, const String& title)
{
    unsigned length = title.length();
    unsigned builderIndex = 0;
    const CharacterType* characters = title.getCharacters<CharacterType>();

    StringBuffer<CharacterType> buffer(length);

    // Replace control characters with spaces and collapse whitespace.
    bool pendingWhitespace = false;
    for (unsigned i = 0; i < length; ++i) {
        UChar32 c = characters[i];
        if (c <= 0x20 || c == 0x7F || (WTF::Unicode::category(c) & (WTF::Unicode::Separator_Line | WTF::Unicode::Separator_Paragraph))) {
            if (builderIndex != 0)
                pendingWhitespace = true;
        } else {
            if (pendingWhitespace) {
                buffer[builderIndex++] = ' ';
                pendingWhitespace = false;
            }
            buffer[builderIndex++] = c;
        }
    }
    buffer.shrink(builderIndex);

    return String::adopt(buffer);
}

void Document::updateTitle(const String& title)
{
    if (m_rawTitle == title)
        return;

    m_rawTitle = title;

    String oldTitle = m_title;
    if (m_rawTitle.isEmpty())
        m_title = String();
    else if (m_rawTitle.is8Bit())
        m_title = canonicalizedTitle<LChar>(this, m_rawTitle);
    else
        m_title = canonicalizedTitle<UChar>(this, m_rawTitle);

    if (!m_frame || oldTitle == m_title)
        return;
    m_frame->loader().client()->dispatchDidReceiveTitle(m_title);
}

void Document::setTitle(const String& title)
{
    // Title set by JavaScript -- overrides any title elements.
    if (!m_titleElement) {
        if (isHTMLDocument() || isXHTMLDocument()) {
            HTMLElement* headElement = head();
            if (!headElement)
                return;
            m_titleElement = HTMLTitleElement::create(*this);
            headElement->appendChild(m_titleElement.get());
        } else if (isSVGDocument()) {
            Element* element = documentElement();
            if (!isSVGSVGElement(element))
                return;
            m_titleElement = SVGTitleElement::create(*this);
            element->insertBefore(m_titleElement.get(), element->firstChild());
        }
    } else {
        if (!isHTMLDocument() && !isXHTMLDocument() && !isSVGDocument())
            m_titleElement = nullptr;
    }

    if (isHTMLTitleElement(m_titleElement))
        toHTMLTitleElement(m_titleElement)->setText(title);
    else if (isSVGTitleElement(m_titleElement))
        toSVGTitleElement(m_titleElement)->setText(title);
    else
        updateTitle(title);
}

void Document::setTitleElement(Element* titleElement)
{
    // If the root element is an svg element in the SVG namespace, then let value be the child text content
    // of the first title element in the SVG namespace that is a child of the root element.
    if (isSVGSVGElement(documentElement())) {
        m_titleElement = Traversal<SVGTitleElement>::firstChild(*documentElement());
    } else {
        if (m_titleElement && m_titleElement != titleElement)
            m_titleElement = Traversal<HTMLTitleElement>::firstWithin(*this);
        else
            m_titleElement = titleElement;

        // If the root element isn't an svg element in the SVG namespace and the title element is
        // in the SVG namespace, it is ignored.
        if (isSVGTitleElement(m_titleElement)) {
            m_titleElement = nullptr;
            return;
        }
    }

    if (isHTMLTitleElement(m_titleElement))
        updateTitle(toHTMLTitleElement(m_titleElement)->text());
    else if (isSVGTitleElement(m_titleElement))
        updateTitle(toSVGTitleElement(m_titleElement)->textContent());
}

void Document::removeTitle(Element* titleElement)
{
    if (m_titleElement != titleElement)
        return;

    m_titleElement = nullptr;

    // Update title based on first title element in the document, if one exists.
    if (isHTMLDocument() || isXHTMLDocument()) {
        if (HTMLTitleElement* title = Traversal<HTMLTitleElement>::firstWithin(*this))
            setTitleElement(title);
    } else if (isSVGDocument()) {
        if (SVGTitleElement* title = Traversal<SVGTitleElement>::firstWithin(*this))
            setTitleElement(title);
    }

    if (!m_titleElement)
        updateTitle(String());
}

const AtomicString& Document::dir()
{
    Element* rootElement = documentElement();
    if (isHTMLHtmlElement(rootElement))
        return toHTMLHtmlElement(rootElement)->dir();
    return nullAtom;
}

void Document::setDir(const AtomicString& value)
{
    Element* rootElement = documentElement();
    if (isHTMLHtmlElement(rootElement))
        toHTMLHtmlElement(rootElement)->setDir(value);
}

PageVisibilityState Document::pageVisibilityState() const
{
    // The visibility of the document is inherited from the visibility of the
    // page. If there is no page associated with the document, we will assume
    // that the page is hidden, as specified by the spec:
    // http://dvcs.w3.org/hg/webperf/raw-file/tip/specs/PageVisibility/Overview.html#dom-document-hidden
    if (!m_frame || !m_frame->page())
        return PageVisibilityStateHidden;
    // While visibilitychange is being dispatched during unloading it is
    // expected that the visibility is hidden regardless of the page's
    // visibility.
    if (m_loadEventProgress >= UnloadVisibilityChangeInProgress)
        return PageVisibilityStateHidden;
    return m_frame->page()->visibilityState();
}

String Document::visibilityState() const
{
    return pageVisibilityStateString(pageVisibilityState());
}

bool Document::hidden() const
{
    return pageVisibilityState() != PageVisibilityStateVisible;
}

void Document::didChangeVisibilityState()
{
    dispatchEvent(Event::createBubble(EventTypeNames::visibilitychange));
    // Also send out the deprecated version until it can be removed.
    dispatchEvent(Event::createBubble(EventTypeNames::webkitvisibilitychange));

    if (pageVisibilityState() == PageVisibilityStateVisible)
        timeline().setAllCompositorPending();

    if (hidden() && m_canvasFontCache)
        m_canvasFontCache->pruneAll();
}

String Document::nodeName() const
{
    return "#document";
}

Node::NodeType Document::getNodeType() const
{
    return DOCUMENT_NODE;
}

FormController& Document::formController()
{
    if (!m_formController) {
        m_formController = FormController::create();
        if (m_frame && m_frame->loader().currentItem() && m_frame->loader().currentItem()->isCurrentDocument(this))
            m_frame->loader().currentItem()->setDocumentState(m_formController->formElementsState());
    }
    return *m_formController;
}

DocumentState* Document::formElementsState() const
{
    if (!m_formController)
        return 0;
    return m_formController->formElementsState();
}

void Document::setStateForNewFormElements(const Vector<String>& stateVector)
{
    if (!stateVector.size() && !m_formController)
        return;
    formController().setStateForNewFormElements(stateVector);
}

FrameView* Document::view() const
{
    return m_frame ? m_frame->view() : 0;
}

Page* Document::page() const
{
    return m_frame ? m_frame->page() : 0;
}

FrameHost* Document::frameHost() const
{
    return m_frame ? m_frame->host() : 0;
}

Settings* Document::settings() const
{
    return m_frame ? m_frame->settings() : 0;
}

Range* Document::createRange()
{
    return Range::create(*this);
}

NodeIterator* Document::createNodeIterator(Node* root, unsigned whatToShow, NodeFilter* filter)
{
    DCHECK(root);
    return NodeIterator::create(root, whatToShow, filter);
}

TreeWalker* Document::createTreeWalker(Node* root, unsigned whatToShow, NodeFilter* filter)
{
    DCHECK(root);
    return TreeWalker::create(root, whatToShow, filter);
}

bool Document::needsLayoutTreeUpdate() const
{
    if (!isActive() || !view())
        return false;
    if (needsFullLayoutTreeUpdate())
        return true;
    if (childNeedsStyleRecalc())
        return true;
    if (childNeedsStyleInvalidation())
        return true;
    if (layoutView()->wasNotifiedOfSubtreeChange())
        return true;
    return false;
}

bool Document::needsFullLayoutTreeUpdate() const
{
    if (!isActive() || !view())
        return false;
    if (!m_useElementsNeedingUpdate.isEmpty())
        return true;
    if (!m_layerUpdateSVGFilterElements.isEmpty())
        return true;
    if (needsStyleRecalc())
        return true;
    if (needsStyleInvalidation())
        return true;
    // FIXME: The childNeedsDistributionRecalc bit means either self or children, we should fix that.
    if (childNeedsDistributionRecalc())
        return true;
    if (DocumentAnimations::needsAnimationTimingUpdate(*this))
        return true;
    return false;
}

bool Document::shouldScheduleLayoutTreeUpdate() const
{
    if (!isActive())
        return false;
    if (inStyleRecalc())
        return false;
    // InPreLayout will recalc style itself. There's no reason to schedule another recalc.
    if (m_lifecycle.state() == DocumentLifecycle::InPreLayout)
        return false;
    if (!shouldScheduleLayout())
        return false;
    return true;
}

void Document::scheduleLayoutTreeUpdate()
{
    DCHECK(!hasPendingVisualUpdate());
    DCHECK(shouldScheduleLayoutTreeUpdate());
    DCHECK(needsLayoutTreeUpdate());

    if (!view()->canThrottleRendering())
        page()->animator().scheduleVisualUpdate(frame());
    m_lifecycle.ensureStateAtMost(DocumentLifecycle::VisualUpdatePending);

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "ScheduleStyleRecalculation", TRACE_EVENT_SCOPE_THREAD, "data", InspectorRecalculateStylesEvent::data(frame()));
    InspectorInstrumentation::didScheduleStyleRecalculation(this);

    ++m_styleVersion;
}

bool Document::hasPendingForcedStyleRecalc() const
{
    return hasPendingVisualUpdate() && !inStyleRecalc() && getStyleChangeType() >= SubtreeStyleChange;
}

void Document::updateStyleInvalidationIfNeeded()
{
    ScriptForbiddenScope forbidScript;

    if (!isActive())
        return;
    if (!childNeedsStyleInvalidation())
        return;
    TRACE_EVENT0("blink", "Document::updateStyleInvalidationIfNeeded");
    styleEngine().styleInvalidator().invalidate(*this);
}

void Document::setupFontBuilder(ComputedStyle& documentStyle)
{
    FontBuilder fontBuilder(*this);
    CSSFontSelector* selector = styleEngine().fontSelector();
    fontBuilder.createFontForDocument(selector, documentStyle);
}

void Document::inheritHtmlAndBodyElementStyles(StyleRecalcChange change)
{
    DCHECK(inStyleRecalc());
    DCHECK(documentElement());

    bool didRecalcDocumentElement = false;
    RefPtr<ComputedStyle> documentElementStyle = documentElement()->mutableComputedStyle();
    if (change == Force)
        documentElement()->clearAnimationStyleChange();
    if (!documentElementStyle || documentElement()->needsStyleRecalc() || change == Force) {
        documentElementStyle = ensureStyleResolver().styleForElement(documentElement());
        didRecalcDocumentElement = true;
    }

    WritingMode rootWritingMode = documentElementStyle->getWritingMode();
    TextDirection rootDirection = documentElementStyle->direction();

    HTMLElement* body = this->body();
    RefPtr<ComputedStyle> bodyStyle;

    if (body) {
        bodyStyle = body->mutableComputedStyle();
        if (didRecalcDocumentElement)
            body->clearAnimationStyleChange();
        if (!bodyStyle || body->needsStyleRecalc() || didRecalcDocumentElement)
            bodyStyle = ensureStyleResolver().styleForElement(body, documentElementStyle.get());
        rootWritingMode = bodyStyle->getWritingMode();
        rootDirection = bodyStyle->direction();
    }

    const ComputedStyle* backgroundStyle = documentElementStyle.get();
    // http://www.w3.org/TR/css3-background/#body-background
    // <html> root element with no background steals background from its first <body> child.
    // Also see LayoutBoxModelObject::backgroundStolenForBeingBody()
    if (isHTMLHtmlElement(documentElement()) && isHTMLBodyElement(body) && !backgroundStyle->hasBackground())
        backgroundStyle = bodyStyle.get();
    Color backgroundColor = backgroundStyle->visitedDependentColor(CSSPropertyBackgroundColor);
    FillLayer backgroundLayers = backgroundStyle->backgroundLayers();
    for (auto currentLayer = &backgroundLayers; currentLayer; currentLayer = currentLayer->next()) {
        // http://www.w3.org/TR/css3-background/#root-background
        // The root element background always have painting area of the whole canvas.
        currentLayer->setClip(BorderFillBox);

        // The root element doesn't scroll. It always propagates its layout overflow
        // to the viewport. Positioning background against either box is equivalent to
        // positioning against the scrolled box of the viewport.
        if (currentLayer->attachment() == ScrollBackgroundAttachment)
            currentLayer->setAttachment(LocalBackgroundAttachment);
    }
    EImageRendering imageRendering = backgroundStyle->imageRendering();

    const ComputedStyle* overflowStyle = nullptr;
    if (Element* element = viewportDefiningElement(documentElementStyle.get())) {
        if (element == body) {
            overflowStyle = bodyStyle.get();
        } else {
            DCHECK_EQ(element, documentElement());
            overflowStyle = documentElementStyle.get();

            // The body element has its own scrolling box, independent from the viewport.
            // This is a bit of a weird edge case in the CSS spec that we might want to try to
            // eliminate some day (eg. for ScrollTopLeftInterop - see http://crbug.com/157855).
            if (bodyStyle && !bodyStyle->isOverflowVisible())
                UseCounter::count(*this, UseCounter::BodyScrollsInAdditionToViewport);
        }
    }

    // Resolved rem units are stored in the matched properties cache so we need to make sure to
    // invalidate the cache if the documentElement needed to reattach or the font size changed
    // and then trigger a full document recalc. We also need to clear it here since the
    // call to styleForElement on the body above can cache bad values for rem units if the
    // documentElement's style was dirty. We could keep track of which elements depend on
    // rem units like we do for viewport styles, but we assume root font size changes are
    // rare and just invalidate the cache for now.
    if (styleEngine().usesRemUnits() && (documentElement()->needsAttach() || !documentElement()->computedStyle() || documentElement()->computedStyle()->fontSize() != documentElementStyle->fontSize())) {
        ensureStyleResolver().invalidateMatchedPropertiesCache();
        documentElement()->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::FontSizeChange));
    }

    EOverflow overflowX = OverflowAuto;
    EOverflow overflowY = OverflowAuto;
    float columnGap = 0;
    if (overflowStyle) {
        overflowX = overflowStyle->overflowX();
        overflowY = overflowStyle->overflowY();
        // Visible overflow on the viewport is meaningless, and the spec says to treat it as 'auto':
        if (overflowX == OverflowVisible)
            overflowX = OverflowAuto;
        if (overflowY == OverflowVisible)
            overflowY = OverflowAuto;
        // Column-gap is (ab)used by the current paged overflow implementation (in lack of other
        // ways to specify gaps between pages), so we have to propagate it too.
        columnGap = overflowStyle->columnGap();
    }

    ScrollSnapType snapType = overflowStyle->getScrollSnapType();
    const LengthPoint& snapDestination = overflowStyle->scrollSnapDestination();

    RefPtr<ComputedStyle> documentStyle = layoutView()->mutableStyle();
    if (documentStyle->getWritingMode() != rootWritingMode
        || documentStyle->direction() != rootDirection
        || documentStyle->visitedDependentColor(CSSPropertyBackgroundColor) != backgroundColor
        || documentStyle->backgroundLayers() != backgroundLayers
        || documentStyle->imageRendering() != imageRendering
        || documentStyle->overflowX() != overflowX
        || documentStyle->overflowY() != overflowY
        || documentStyle->columnGap() != columnGap
        || documentStyle->getScrollSnapType() != snapType
        || documentStyle->scrollSnapDestination() != snapDestination) {
        RefPtr<ComputedStyle> newStyle = ComputedStyle::clone(*documentStyle);
        newStyle->setWritingMode(rootWritingMode);
        newStyle->setDirection(rootDirection);
        newStyle->setBackgroundColor(backgroundColor);
        newStyle->accessBackgroundLayers() = backgroundLayers;
        newStyle->setImageRendering(imageRendering);
        newStyle->setOverflowX(overflowX);
        newStyle->setOverflowY(overflowY);
        newStyle->setColumnGap(columnGap);
        newStyle->setScrollSnapType(snapType);
        newStyle->setScrollSnapDestination(snapDestination);
        layoutView()->setStyle(newStyle);
        setupFontBuilder(*newStyle);
    }

    if (body) {
        if (const ComputedStyle* style = body->computedStyle()) {
            if (style->direction() != rootDirection || style->getWritingMode() != rootWritingMode)
                body->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::WritingModeChange));
        }
    }

    if (const ComputedStyle* style = documentElement()->computedStyle()) {
        if (style->direction() != rootDirection || style->getWritingMode() != rootWritingMode)
            documentElement()->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::WritingModeChange));
    }
}

#if DCHECK_IS_ON()
static void assertLayoutTreeUpdated(Node& root)
{
    for (Node& node : NodeTraversal::inclusiveDescendantsOf(root)) {
        // We leave some nodes with dirty bits in the tree because they don't
        // matter like Comment and ProcessingInstruction nodes.
        // TODO(esprehn): Don't even mark those nodes as needing recalcs in the
        // first place.
        if (!node.isElementNode()
            && !node.isTextNode()
            && !node.isShadowRoot()
            && !node.isDocumentNode())
            continue;
        DCHECK(!node.needsStyleRecalc());
        DCHECK(!node.childNeedsStyleRecalc());
        DCHECK(!node.childNeedsDistributionRecalc());
        DCHECK(!node.needsStyleInvalidation());
        DCHECK(!node.childNeedsStyleInvalidation());
        for (ShadowRoot* shadowRoot = node.youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
            assertLayoutTreeUpdated(*shadowRoot);
    }
}
#endif

void Document::updateStyleAndLayoutTree()
{
    DCHECK(isMainThread());

    ScriptForbiddenScope forbidScript;
    // We should forbid script execution for plugins here because update while layout is changing,
    // HTMLPlugin element can be reattached and plugin can be destroyed. Plugin can execute scripts
    // on destroy. It produces crash without PluginScriptForbiddenScope: crbug.com/550427.
    PluginScriptForbiddenScope pluginForbidScript;

    if (!view() || !isActive())
        return;

    if (view()->shouldThrottleRendering())
        return;

    if (!needsLayoutTreeUpdate()) {
        if (lifecycle().state() < DocumentLifecycle::StyleClean) {
            // needsLayoutTreeUpdate may change to false without any actual layout tree update.
            // For example, needsAnimationTimingUpdate may change to false when time elapses.
            // Advance lifecycle to StyleClean because style is actually clean now.
            lifecycle().advanceTo(DocumentLifecycle::InStyleRecalc);
            lifecycle().advanceTo(DocumentLifecycle::StyleClean);
        }
        return;
    }

    if (inStyleRecalc())
        return;

    // Entering here from inside layout, paint etc. would be catastrophic since recalcStyle can
    // tear down the layout tree or (unfortunately) run script. Kill the whole layoutObject if
    // someone managed to get into here in states not allowing tree mutations.
    RELEASE_ASSERT(lifecycle().stateAllowsTreeMutations());

    TRACE_EVENT_BEGIN1("blink,devtools.timeline", "UpdateLayoutTree", "beginData", InspectorRecalculateStylesEvent::data(frame()));
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "UpdateLayoutTree");

    unsigned startElementCount = styleEngine().styleForElementCount();

    InspectorInstrumentation::StyleRecalc instrumentation(this);

    DocumentAnimations::updateAnimationTimingIfNeeded(*this);
    evaluateMediaQueryListIfNeeded();
    updateUseShadowTreesIfNeeded();
    updateDistribution();
    updateStyleInvalidationIfNeeded();

    // FIXME: We should update style on our ancestor chain before proceeding
    // however doing so currently causes several tests to crash, as LocalFrame::setDocument calls Document::attach
    // before setting the LocalDOMWindow on the LocalFrame, or the SecurityOrigin on the document. The attach, in turn
    // resolves style (here) and then when we resolve style on the parent chain, we may end up
    // re-attaching our containing iframe, which when asked HTMLFrameElementBase::isURLAllowed
    // hits a null-dereference due to security code always assuming the document has a SecurityOrigin.

    updateStyle();

    notifyLayoutTreeOfSubtreeChanges();

    // As a result of the style recalculation, the currently hovered element might have been
    // detached (for example, by setting display:none in the :hover style), schedule another mouseMove event
    // to check if any other elements ended up under the mouse pointer due to re-layout.
    if (hoverNode() && !hoverNode()->layoutObject() && frame())
        frame()->eventHandler().dispatchFakeMouseMoveEventSoon();

    if (m_focusedElement && !m_focusedElement->isFocusable())
        clearFocusedElementSoon();
    layoutView()->clearHitTestCache();

    DCHECK(!DocumentAnimations::needsAnimationTimingUpdate(*this));

    unsigned elementCount = styleEngine().styleForElementCount() - startElementCount;

    TRACE_EVENT_END1("blink,devtools.timeline", "UpdateLayoutTree", "elementCount", elementCount);

#if DCHECK_IS_ON()
    assertLayoutTreeUpdated(*this);
#endif
}

void Document::updateStyle()
{
    DCHECK(!view()->shouldThrottleRendering());
    TRACE_EVENT_BEGIN0("blink,blink_style", "Document::updateStyle");
    unsigned initialElementCount = styleEngine().styleForElementCount();

    HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
    m_lifecycle.advanceTo(DocumentLifecycle::InStyleRecalc);

    StyleRecalcChange change = NoChange;
    if (getStyleChangeType() >= SubtreeStyleChange)
        change = Force;

    NthIndexCache nthIndexCache(*this);

    // FIXME: Cannot access the ensureStyleResolver() before calling styleForDocument below because
    // apparently the StyleResolver's constructor has side effects. We should fix it.
    // See printing/setPrinting.html, printing/width-overflow.html though they only fail on
    // mac when accessing the resolver by what appears to be a viewport size difference.

    if (change == Force) {
        m_hasNodesWithPlaceholderStyle = false;
        RefPtr<ComputedStyle> documentStyle = StyleResolver::styleForDocument(*this);
        StyleRecalcChange localChange = ComputedStyle::stylePropagationDiff(documentStyle.get(), layoutView()->style());
        if (localChange != NoChange)
            layoutView()->setStyle(documentStyle.release());
    }

    clearNeedsStyleRecalc();

    StyleResolver& resolver = ensureStyleResolver();

    bool shouldRecordStats;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink,blink_style", &shouldRecordStats);
    styleEngine().setStatsEnabled(shouldRecordStats);

    if (Element* documentElement = this->documentElement()) {
        inheritHtmlAndBodyElementStyles(change);
        dirtyElementsForLayerUpdate();
        if (documentElement->shouldCallRecalcStyle(change))
            documentElement->recalcStyle(change);
        while (dirtyElementsForLayerUpdate())
            documentElement->recalcStyle(NoChange);
    }

    view()->recalcOverflowAfterStyleChange();

    clearChildNeedsStyleRecalc();

    resolver.clearStyleSharingList();

    m_wasPrinting = m_printing;

    DCHECK(!needsStyleRecalc());
    DCHECK(!childNeedsStyleRecalc());
    DCHECK(inStyleRecalc());
    DCHECK_EQ(styleResolver(), &resolver);
    m_lifecycle.advanceTo(DocumentLifecycle::StyleClean);
    if (shouldRecordStats) {
        TRACE_EVENT_END2("blink,blink_style", "Document::updateStyle",
            "resolverAccessCount", styleEngine().styleForElementCount() - initialElementCount,
            "counters", styleEngine().stats()->toTracedValue());
    } else {
        TRACE_EVENT_END1("blink,blink_style", "Document::updateStyle",
            "resolverAccessCount", styleEngine().styleForElementCount() - initialElementCount);
    }
}

void Document::notifyLayoutTreeOfSubtreeChanges()
{
    if (!layoutView()->wasNotifiedOfSubtreeChange())
        return;

    m_lifecycle.advanceTo(DocumentLifecycle::InLayoutSubtreeChange);

    layoutView()->handleSubtreeModifications();
    DCHECK(!layoutView()->wasNotifiedOfSubtreeChange());

    m_lifecycle.advanceTo(DocumentLifecycle::LayoutSubtreeChangeClean);
}

bool Document::needsLayoutTreeUpdateForNode(const Node& node) const
{
    if (!node.canParticipateInFlatTree())
        return false;
    if (!needsLayoutTreeUpdate())
        return false;
    if (!node.inShadowIncludingDocument())
        return false;

    if (needsFullLayoutTreeUpdate() || node.needsStyleRecalc() || node.needsStyleInvalidation())
        return true;
    for (const ContainerNode* ancestor = LayoutTreeBuilderTraversal::parent(node); ancestor; ancestor = LayoutTreeBuilderTraversal::parent(*ancestor)) {
        if (ancestor->needsStyleRecalc() || ancestor->needsStyleInvalidation() || ancestor->needsAdjacentStyleRecalc())
            return true;
    }
    return false;
}

void Document::updateStyleAndLayoutTreeForNode(const Node* node)
{
    DCHECK(node);
    if (!needsLayoutTreeUpdateForNode(*node))
        return;
    updateStyleAndLayoutTree();
}

void Document::updateStyleAndLayoutIgnorePendingStylesheetsForNode(Node* node)
{
    DCHECK(node);
    if (!node->inActiveDocument())
        return;
    updateStyleAndLayoutIgnorePendingStylesheets();
}

void Document::updateStyleAndLayout()
{
    DCHECK(isMainThread());

    ScriptForbiddenScope forbidScript;

    FrameView* frameView = view();
    if (frameView && frameView->isInPerformLayout()) {
        // View layout should not be re-entrant.
        ASSERT_NOT_REACHED();
        return;
    }

    if (HTMLFrameOwnerElement* owner = localOwner())
        owner->document().updateStyleAndLayout();

    updateStyleAndLayoutTree();

    if (!isActive())
        return;

    if (frameView->needsLayout())
        frameView->layout();

    if (lifecycle().state() < DocumentLifecycle::LayoutClean)
        lifecycle().advanceTo(DocumentLifecycle::LayoutClean);
}

void Document::layoutUpdated()
{
    // Plugins can run script inside layout which can detach the page.
    // TODO(esprehn): Can this still happen now that all plugins are out of
    // process?
    if (frame() && frame()->page())
        frame()->page()->chromeClient().layoutUpdated(frame());

    markers().invalidateRectsForAllMarkers();

    // The layout system may perform layouts with pending stylesheets. When
    // recording first layout time, we ignore these layouts, since painting is
    // suppressed for them. We're interested in tracking the time of the
    // first real or 'paintable' layout.
    // TODO(esprehn): This doesn't really make sense, why not track the first
    // beginFrame? This will catch the first layout in a page that does lots
    // of layout thrashing even though that layout might not be followed by
    // a paint for many seconds.
    if (isRenderingReady() && body() && !styleEngine().hasPendingScriptBlockingSheets()) {
        if (!m_documentTiming.firstLayout())
            m_documentTiming.markFirstLayout();
    }

    m_rootScrollerController->didUpdateLayout();
}

void Document::setNeedsFocusedElementCheck()
{
    setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::createWithExtraData(StyleChangeReason::PseudoClass, StyleChangeExtraData::Focus));
}

void Document::clearFocusedElementSoon()
{
    if (!m_clearFocusedElementTimer.isActive())
        m_clearFocusedElementTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Document::clearFocusedElementTimerFired(Timer<Document>*)
{
    updateStyleAndLayoutTree();
    m_clearFocusedElementTimer.stop();

    if (m_focusedElement && !m_focusedElement->isFocusable())
        m_focusedElement->blur();
}

// FIXME: This is a bad idea and needs to be removed eventually.
// Other browsers load stylesheets before they continue parsing the web page.
// Since we don't, we can run JavaScript code that needs answers before the
// stylesheets are loaded. Doing a layout ignoring the pending stylesheets
// lets us get reasonable answers. The long term solution to this problem is
// to instead suspend JavaScript execution.
void Document::updateStyleAndLayoutTreeIgnorePendingStylesheets()
{
    StyleEngine::IgnoringPendingStylesheet ignoring(styleEngine());

    if (styleEngine().hasPendingScriptBlockingSheets()) {
        // FIXME: We are willing to attempt to suppress painting with outdated style info only once.
        // Our assumption is that it would be dangerous to try to stop it a second time, after page
        // content has already been loaded and displayed with accurate style information. (Our
        // suppression involves blanking the whole page at the moment. If it were more refined, we
        // might be able to do something better.) It's worth noting though that this entire method
        // is a hack, since what we really want to do is suspend JS instead of doing a layout with
        // inaccurate information.
        HTMLElement* bodyElement = body();
        if (bodyElement && !bodyElement->layoutObject() && m_pendingSheetLayout == NoLayoutWithPendingSheets) {
            m_pendingSheetLayout = DidLayoutWithPendingSheets;
            styleEngine().resolverChanged(FullStyleUpdate);
        } else if (m_hasNodesWithPlaceholderStyle) {
            // If new nodes have been added or style recalc has been done with style sheets still
            // pending, some nodes may not have had their real style calculated yet. Normally this
            // gets cleaned when style sheets arrive but here we need up-to-date style immediately.
            setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::CleanupPlaceholderStyles));
        }
    }
    updateStyleAndLayoutTree();
}

void Document::updateStyleAndLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks runPostLayoutTasks)
{
    updateStyleAndLayoutTreeIgnorePendingStylesheets();
    updateStyleAndLayout();

    if (runPostLayoutTasks == RunPostLayoutTasksSynchronously && view())
        view()->flushAnyPendingPostLayoutTasks();
}

PassRefPtr<ComputedStyle> Document::styleForElementIgnoringPendingStylesheets(Element* element)
{
    DCHECK_EQ(element->document(), this);
    StyleEngine::IgnoringPendingStylesheet ignoring(styleEngine());
    return ensureStyleResolver().styleForElement(element, element->parentNode() ? element->parentNode()->ensureComputedStyle() : 0);
}

PassRefPtr<ComputedStyle> Document::styleForPage(int pageIndex)
{
    updateDistribution();
    return ensureStyleResolver().styleForPage(pageIndex);
}

bool Document::isPageBoxVisible(int pageIndex)
{
    return styleForPage(pageIndex)->visibility() != HIDDEN; // display property doesn't apply to @page.
}

void Document::pageSizeAndMarginsInPixels(int pageIndex, IntSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    RefPtr<ComputedStyle> style = styleForPage(pageIndex);

    int width = pageSize.width();
    int height = pageSize.height();
    switch (style->getPageSizeType()) {
    case PAGE_SIZE_AUTO:
        break;
    case PAGE_SIZE_AUTO_LANDSCAPE:
        if (width < height)
            std::swap(width, height);
        break;
    case PAGE_SIZE_AUTO_PORTRAIT:
        if (width > height)
            std::swap(width, height);
        break;
    case PAGE_SIZE_RESOLVED: {
        FloatSize size = style->pageSize();
        width = size.width();
        height = size.height();
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    pageSize = IntSize(width, height);

    // The percentage is calculated with respect to the width even for margin top and bottom.
    // http://www.w3.org/TR/CSS2/box.html#margin-properties
    marginTop = style->marginTop().isAuto() ? marginTop : intValueForLength(style->marginTop(), width);
    marginRight = style->marginRight().isAuto() ? marginRight : intValueForLength(style->marginRight(), width);
    marginBottom = style->marginBottom().isAuto() ? marginBottom : intValueForLength(style->marginBottom(), width);
    marginLeft = style->marginLeft().isAuto() ? marginLeft : intValueForLength(style->marginLeft(), width);
}

void Document::setIsViewSource(bool isViewSource)
{
    m_isViewSource = isViewSource;
    if (!m_isViewSource)
        return;

    setSecurityOrigin(SecurityOrigin::createUnique());
    didUpdateSecurityOrigin();
}

bool Document::dirtyElementsForLayerUpdate()
{
    if (m_layerUpdateSVGFilterElements.isEmpty())
        return false;

    for (Element* element : m_layerUpdateSVGFilterElements)
        element->setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::SVGFilterLayerUpdate));
    m_layerUpdateSVGFilterElements.clear();
    return true;
}

void Document::scheduleSVGFilterLayerUpdateHack(Element& element)
{
    if (element.getStyleChangeType() == NeedsReattachStyleChange)
        return;
    element.setSVGFilterNeedsLayerUpdate();
    m_layerUpdateSVGFilterElements.add(&element);
    scheduleLayoutTreeUpdateIfNeeded();
}

void Document::unscheduleSVGFilterLayerUpdateHack(Element& element)
{
    element.clearSVGFilterNeedsLayerUpdate();
    m_layerUpdateSVGFilterElements.remove(&element);
}

void Document::scheduleUseShadowTreeUpdate(SVGUseElement& element)
{
    m_useElementsNeedingUpdate.add(&element);
    scheduleLayoutTreeUpdateIfNeeded();
}

void Document::unscheduleUseShadowTreeUpdate(SVGUseElement& element)
{
    m_useElementsNeedingUpdate.remove(&element);
}

void Document::updateUseShadowTreesIfNeeded()
{
    ScriptForbiddenScope forbidScript;

    if (m_useElementsNeedingUpdate.isEmpty())
        return;

    HeapHashSet<Member<SVGUseElement>> elements;
    m_useElementsNeedingUpdate.swap(elements);
    for (SVGUseElement* element : elements)
        element->buildPendingResource();
}

StyleResolver* Document::styleResolver() const
{
    return m_styleEngine->resolver();
}

StyleResolver& Document::ensureStyleResolver() const
{
    return m_styleEngine->ensureResolver();
}

void Document::attach(const AttachContext& context)
{
    DCHECK_EQ(m_lifecycle.state(), DocumentLifecycle::Inactive);
    DCHECK(!m_axObjectCache || this != &axObjectCacheOwner());

    m_layoutView = new LayoutView(this);
    setLayoutObject(m_layoutView);

    m_layoutView->setIsInWindow(true);
    m_layoutView->setStyle(StyleResolver::styleForDocument(*this));
    m_layoutView->compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterCompositingInputChange);

    ContainerNode::attach(context);

    // The TextAutosizer can't update layout view info while the Document is detached, so update now in case anything changed.
    if (TextAutosizer* autosizer = textAutosizer())
        autosizer->updatePageInfo();

    m_frame->selection().documentAttached(this);
    m_lifecycle.advanceTo(DocumentLifecycle::StyleClean);
}

void Document::detach(const AttachContext& context)
{
    TRACE_EVENT0("blink", "Document::detach");
    RELEASE_ASSERT(!m_frame || m_frame->tree().childCount() == 0);
    if (!isActive())
        return;

    // Frame navigation can cause a new Document to be attached. Don't allow that, since that will
    // cause a situation where LocalFrame still has a Document attached after this finishes!
    // Normally, it shouldn't actually be possible to trigger navigation here. However, plugins
    // (see below) can cause lots of crazy things to happen, since plugin detach involves nested
    // message loops.
    FrameNavigationDisabler navigationDisabler(*m_frame);
    // Defer widget updates to avoid plugins trying to run script inside ScriptForbiddenScope,
    // which will crash the renderer after https://crrev.com/200984
    HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
    // Don't allow script to run in the middle of detach() because a detaching Document is not in a
    // consistent state.
    ScriptForbiddenScope forbidScript;
    view()->dispose();
    m_markers->prepareForDestruction();
    if (LocalDOMWindow* window = this->domWindow())
        window->willDetachDocumentFromFrame();

    m_lifecycle.advanceTo(DocumentLifecycle::Stopping);

    if (page())
        page()->documentDetached(this);
    InspectorInstrumentation::documentDetached(this);

    if (m_frame->loader().client()->sharedWorkerRepositoryClient())
        m_frame->loader().client()->sharedWorkerRepositoryClient()->documentDetached(this);

    stopActiveDOMObjects();

    // FIXME: consider using ActiveDOMObject.
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->clearDocumentPointer();
    m_scriptedAnimationController.clear();

    m_scriptedIdleTaskController.clear();

    if (svgExtensions())
        accessSVGExtensions().pauseAnimations();

    // FIXME: This shouldn't be needed once LocalDOMWindow becomes ExecutionContext.
    if (m_domWindow)
        m_domWindow->clearEventQueue();

    if (m_layoutView)
        m_layoutView->setIsInWindow(false);

    if (registrationContext())
        registrationContext()->documentWasDetached();

    m_hoverNode = nullptr;
    m_activeHoverElement = nullptr;
    m_autofocusElement = nullptr;

    if (m_focusedElement.get()) {
        Element* oldFocusedElement = m_focusedElement;
        m_focusedElement = nullptr;
        if (frameHost())
            frameHost()->chromeClient().focusedNodeChanged(oldFocusedElement, nullptr);
    }
    m_sequentialFocusNavigationStartingPoint = nullptr;

    if (this == &axObjectCacheOwner())
        clearAXObjectCache();

    m_layoutView = nullptr;
    ContainerNode::detach(context);

    if (this != &axObjectCacheOwner()) {
        if (AXObjectCache* cache = existingAXObjectCache()) {
            // Documents that are not a root document use the AXObjectCache in
            // their root document. Node::removedFrom is called after the
            // document has been detached so it can't find the root document.
            // We do the removals here instead.
            for (Node& node : NodeTraversal::descendantsOf(*this)) {
                cache->remove(&node);
            }
        }
    }

    styleEngine().didDetach();

    frameHost()->eventHandlerRegistry().documentDetached(*this);

    m_frame->selection().documentDetached(*this);
    m_frame->inputMethodController().documentDetached();

    // If this Document is associated with a live DocumentLoader, the
    // DocumentLoader will take care of clearing the FetchContext. Deferring
    // to the DocumentLoader when possible also prevents prematurely clearing
    // the context in the case where multiple Documents end up associated with
    // a single DocumentLoader (e.g., navigating to a javascript: url).
    if (!loader())
        m_fetcher->clearContext();
    // If this document is the master for an HTMLImportsController, sever that
    // relationship. This ensures that we don't leave import loads in flight,
    // thinking they should have access to a valid frame when they don't.
    if (m_importsController) {
        m_importsController->dispose();
        setImportsController(nullptr);
    }

    m_timers.setTimerTaskRunner(
        Platform::current()->currentThread()->scheduler()->timerTaskRunner()->adoptClone());

    // This is required, as our LocalFrame might delete itself as soon as it detaches
    // us. However, this violates Node::detach() semantics, as it's never
    // possible to re-attach. Eventually Document::detach() should be renamed,
    // or this setting of the frame to 0 could be made explicit in each of the
    // callers of Document::detach().
    m_frame = nullptr;

    if (m_mediaQueryMatcher)
        m_mediaQueryMatcher->documentDetached();

    m_lifecycle.advanceTo(DocumentLifecycle::Stopped);

    // FIXME: Currently we call notifyContextDestroyed() only in
    // Document::detach(), which means that we don't call
    // notifyContextDestroyed() for a document that doesn't get detached.
    // If such a document has any observer, the observer won't get
    // a contextDestroyed() notification. This can happen for a document
    // created by DOMImplementation::createDocument().
    ExecutionContext::notifyContextDestroyed();
}

void Document::removeAllEventListeners()
{
    ContainerNode::removeAllEventListeners();

    if (LocalDOMWindow* domWindow = this->domWindow())
        domWindow->removeAllEventListeners();
}

Document& Document::axObjectCacheOwner() const
{
    // Every document has its own axObjectCache if accessibility is enabled,
    // except for page popups, which share the axObjectCache of their owner.
    Document* doc = const_cast<Document*>(this);
    if (doc->frame() && doc->frame()->pagePopupOwner()) {
        DCHECK(!doc->m_axObjectCache);
        return doc->frame()->pagePopupOwner()->document().axObjectCacheOwner();
    }
    return *doc;
}

void Document::clearAXObjectCache()
{
    DCHECK_EQ(&axObjectCacheOwner(), this);
    // Clear the cache member variable before calling delete because attempts
    // are made to access it during destruction.
    if (m_axObjectCache)
        m_axObjectCache->dispose();
    m_axObjectCache.clear();
}

AXObjectCache* Document::existingAXObjectCache() const
{
    // If the layoutObject is gone then we are in the process of destruction.
    // This method will be called before m_frame = nullptr.
    if (!axObjectCacheOwner().layoutView())
        return 0;

    return axObjectCacheOwner().m_axObjectCache.get();
}

AXObjectCache* Document::axObjectCache() const
{
    Settings* settings = this->settings();
    if (!settings || !settings->accessibilityEnabled())
        return 0;

    // The only document that actually has a AXObjectCache is the top-level
    // document.  This is because we need to be able to get from any WebCoreAXObject
    // to any other WebCoreAXObject on the same page.  Using a single cache allows
    // lookups across nested webareas (i.e. multiple documents).
    Document& cacheOwner = this->axObjectCacheOwner();

    // If the document has already been detached, do not make a new axObjectCache.
    if (!cacheOwner.layoutView())
        return 0;

    DCHECK(&cacheOwner == this || !m_axObjectCache);
    if (!cacheOwner.m_axObjectCache)
        cacheOwner.m_axObjectCache = AXObjectCache::create(cacheOwner);
    return cacheOwner.m_axObjectCache.get();
}

CanvasFontCache* Document::canvasFontCache()
{
    if (!m_canvasFontCache)
        m_canvasFontCache = CanvasFontCache::create(*this);

    return m_canvasFontCache.get();
}

DocumentParser* Document::createParser()
{
    if (isHTMLDocument())
        return HTMLDocumentParser::create(toHTMLDocument(*this), m_parserSyncPolicy);
    // FIXME: this should probably pass the frame instead
    return XMLDocumentParser::create(*this, view());
}

bool Document::isFrameSet() const
{
    if (!isHTMLDocument())
        return false;
    return isHTMLFrameSetElement(body());
}

ScriptableDocumentParser* Document::scriptableDocumentParser() const
{
    return parser() ? parser()->asScriptableDocumentParser() : 0;
}

void Document::open(Document* enteredDocument, ExceptionState& exceptionState)
{
    if (importLoader()) {
        exceptionState.throwDOMException(InvalidStateError, "Imported document doesn't support open().");
        return;
    }

    if (!isHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "Only HTML documents support open().");
        return;
    }

    if (enteredDocument) {
        if (!getSecurityOrigin()->canAccess(enteredDocument->getSecurityOrigin())) {
            exceptionState.throwSecurityError("Can only call open() on same-origin documents.");
            return;
        }
        setSecurityOrigin(enteredDocument->getSecurityOrigin());
        setURL(enteredDocument->url());
        m_cookieURL = enteredDocument->cookieURL();
    }

    open();
}

void Document::open()
{
    DCHECK(!importLoader());

    if (m_frame) {
        if (ScriptableDocumentParser* parser = scriptableDocumentParser()) {
            if (parser->isParsing()) {
                // FIXME: HTML5 doesn't tell us to check this, it might not be correct.
                if (parser->isExecutingScript())
                    return;

                if (!parser->wasCreatedByScript() && parser->hasInsertionPoint())
                    return;
            }
        }

        if (m_frame->loader().provisionalDocumentLoader())
            m_frame->loader().stopAllLoaders();
    }

    removeAllEventListenersRecursively();
    implicitOpen(ForceSynchronousParsing);
    if (ScriptableDocumentParser* parser = scriptableDocumentParser())
        parser->setWasCreatedByScript(true);

    if (m_frame)
        m_frame->loader().didExplicitOpen();
    if (m_loadEventProgress != LoadEventInProgress && pageDismissalEventBeingDispatched() == NoDismissal)
        m_loadEventProgress = LoadEventNotRun;
}

void Document::detachParser()
{
    if (!m_parser)
        return;
    m_parser->detach();
    m_parser.clear();
    DocumentParserTiming::from(*this).markParserDetached();
}

void Document::cancelParsing()
{
    detachParser();
    setParsingState(FinishedParsing);
    setReadyState(Complete);
}

DocumentParser* Document::implicitOpen(ParserSynchronizationPolicy parserSyncPolicy)
{
    detachParser();

    removeChildren();
    DCHECK(!m_focusedElement);

    setCompatibilityMode(NoQuirksMode);

    if (!threadedParsingEnabledForTesting())
        parserSyncPolicy = ForceSynchronousParsing;

    m_parserSyncPolicy = parserSyncPolicy;
    m_parser = createParser();
    DocumentParserTiming::from(*this).markParserStart();
    setParsingState(Parsing);
    setReadyState(Loading);

    return m_parser;
}

HTMLElement* Document::body() const
{
    if (!documentElement() || !isHTMLHtmlElement(documentElement()))
        return 0;

    for (HTMLElement* child = Traversal<HTMLElement>::firstChild(*documentElement()); child; child = Traversal<HTMLElement>::nextSibling(*child)) {
        if (isHTMLFrameSetElement(*child) || isHTMLBodyElement(*child))
            return child;
    }

    return 0;
}

HTMLBodyElement* Document::firstBodyElement() const
{
    if (!documentElement())
        return 0;

    for (HTMLElement* child = Traversal<HTMLElement>::firstChild(*documentElement()); child; child = Traversal<HTMLElement>::nextSibling(*child)) {
        if (isHTMLBodyElement(*child))
            return toHTMLBodyElement(child);
    }

    return 0;
}

void Document::setBody(HTMLElement* prpNewBody, ExceptionState& exceptionState)
{
    HTMLElement* newBody = prpNewBody;

    if (!newBody) {
        exceptionState.throwDOMException(HierarchyRequestError, ExceptionMessages::argumentNullOrIncorrectType(1, "HTMLElement"));
        return;
    }
    if (!documentElement()) {
        exceptionState.throwDOMException(HierarchyRequestError, "No document element exists.");
        return;
    }

    if (!isHTMLBodyElement(*newBody) && !isHTMLFrameSetElement(*newBody)) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new body element is of type '" + newBody->tagName() + "'. It must be either a 'BODY' or 'FRAMESET' element.");
        return;
    }

    HTMLElement* oldBody = body();
    if (oldBody == newBody)
        return;

    if (oldBody)
        documentElement()->replaceChild(newBody, oldBody, exceptionState);
    else
        documentElement()->appendChild(newBody, exceptionState);
}

void Document::willInsertBody()
{
    if (frame())
        frame()->loader().client()->dispatchWillInsertBody();
    // If we get to the <body> try to resume commits since we should have content
    // to paint now.
    // TODO(esprehn): Is this really optimal? We might start producing frames
    // for very little content, should we wait for some heuristic like
    // isVisuallyNonEmpty() ?
    beginLifecycleUpdatesIfRenderingReady();
}

HTMLHeadElement* Document::head() const
{
    Node* de = documentElement();
    if (!de)
        return 0;

    return Traversal<HTMLHeadElement>::firstChild(*de);
}

Element* Document::viewportDefiningElement(const ComputedStyle* rootStyle) const
{
    // If a BODY element sets non-visible overflow, it is to be propagated to the viewport, as long
    // as the following conditions are all met:
    // (1) The root element is HTML.
    // (2) It is the primary BODY element (we only assert for this, expecting callers to behave).
    // (3) The root element has visible overflow.
    // Otherwise it's the root element's properties that are to be propagated.
    Element* rootElement = documentElement();
    Element* bodyElement = body();
    if (!rootElement)
        return 0;
    if (!rootStyle) {
        rootStyle = rootElement->computedStyle();
        if (!rootStyle)
            return 0;
    }
    if (bodyElement && rootStyle->isOverflowVisible() && isHTMLHtmlElement(*rootElement))
        return bodyElement;
    return rootElement;
}

void Document::close(ExceptionState& exceptionState)
{
    // FIXME: We should follow the specification more closely:
    //        http://www.whatwg.org/specs/web-apps/current-work/#dom-document-close

    if (importLoader()) {
        exceptionState.throwDOMException(InvalidStateError, "Imported document doesn't support close().");
        return;
    }

    if (!isHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "Only HTML documents support close().");
        return;
    }

    close();
}

void Document::close()
{
    if (!scriptableDocumentParser() || !scriptableDocumentParser()->wasCreatedByScript() || !scriptableDocumentParser()->isParsing())
        return;

    if (DocumentParser* parser = m_parser)
        parser->finish();

    if (!m_frame) {
        // Because we have no frame, we don't know if all loading has completed,
        // so we just call implicitClose() immediately. FIXME: This might fire
        // the load event prematurely <http://bugs.webkit.org/show_bug.cgi?id=14568>.
        implicitClose();
        return;
    }

    m_frame->loader().checkCompleted();
}

void Document::implicitClose()
{
    DCHECK(!inStyleRecalc());
    if (processingLoadEvent() || !m_parser)
        return;
    if (frame() && frame()->navigationScheduler().locationChangePending()) {
        suppressLoadEvent();
        return;
    }

    m_loadEventProgress = LoadEventInProgress;

    ScriptableDocumentParser* parser = scriptableDocumentParser();
    m_wellFormed = parser && parser->wellFormed();

    // We have to clear the parser, in case someone document.write()s from the
    // onLoad event handler, as in Radar 3206524.
    detachParser();

    if (frame() && frame()->script().canExecuteScripts(NotAboutToExecuteScript)) {
        ImageLoader::dispatchPendingLoadEvents();
        ImageLoader::dispatchPendingErrorEvents();

        HTMLLinkElement::dispatchPendingLoadEvents();
        HTMLStyleElement::dispatchPendingLoadEvents();
    }

    // JS running below could remove the frame or destroy the LayoutView so we call
    // those two functions repeatedly and don't save them on the stack.

    // To align the HTML load event and the SVGLoad event for the outermost <svg> element, fire it from
    // here, instead of doing it from SVGElement::finishedParsingChildren.
    if (svgExtensions())
        accessSVGExtensions().dispatchSVGLoadEventToOutermostSVGElements();

    if (this->domWindow())
        this->domWindow()->documentWasClosed();

    if (frame()) {
        frame()->loader().client()->dispatchDidHandleOnloadEvents();
        loader()->applicationCacheHost()->stopDeferringEvents();
    }

    if (!frame()) {
        m_loadEventProgress = LoadEventCompleted;
        return;
    }

    // Make sure both the initial layout and reflow happen after the onload
    // fires. This will improve onload scores, and other browsers do it.
    // If they wanna cheat, we can too. -dwh

    if (frame()->navigationScheduler().locationChangePending() && elapsedTime() < cLayoutScheduleThreshold) {
        // Just bail out. Before or during the onload we were shifted to another page.
        // The old i-Bench suite does this. When this happens don't bother painting or laying out.
        m_loadEventProgress = LoadEventCompleted;
        return;
    }

    // We used to force a synchronous display and flush here.  This really isn't
    // necessary and can in fact be actively harmful if pages are loading at a rate of > 60fps
    // (if your platform is syncing flushes and limiting them to 60fps).
    if (!localOwner() || (localOwner()->layoutObject() && !localOwner()->layoutObject()->needsLayout())) {
        updateStyleAndLayoutTree();

        // Always do a layout after loading if needed.
        if (view() && layoutView() && (!layoutView()->firstChild() || layoutView()->needsLayout()))
            view()->layout();
    }

    m_loadEventProgress = LoadEventCompleted;

    if (frame() && layoutView() && settings()->accessibilityEnabled()) {
        if (AXObjectCache* cache = axObjectCache()) {
            if (this == &axObjectCacheOwner())
                cache->handleLoadComplete(this);
            else
                cache->handleLayoutComplete(this);
        }
    }

    if (svgExtensions())
        accessSVGExtensions().startAnimations();
}

bool Document::dispatchBeforeUnloadEvent(ChromeClient& chromeClient, bool isReload, bool& didAllowNavigation)
{
    if (!m_domWindow)
        return true;

    if (!body())
        return true;

    if (processingBeforeUnload())
        return false;

    BeforeUnloadEvent* beforeUnloadEvent = BeforeUnloadEvent::create();
    m_loadEventProgress = BeforeUnloadEventInProgress;
    m_domWindow->dispatchEvent(beforeUnloadEvent, this);
    m_loadEventProgress = BeforeUnloadEventCompleted;
    if (!beforeUnloadEvent->defaultPrevented())
        defaultEventHandler(beforeUnloadEvent);
    if (!frame() || beforeUnloadEvent->returnValue().isNull())
        return true;

    if (didAllowNavigation) {
        addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, "Blocked attempt to show multiple 'beforeunload' confirmation panels for a single navigation."));
        return true;
    }

    String text = beforeUnloadEvent->returnValue();
    if (chromeClient.openBeforeUnloadConfirmPanel(text, m_frame, isReload)) {
        didAllowNavigation = true;
        return true;
    }
    return false;
}

void Document::dispatchUnloadEvents()
{
    PluginScriptForbiddenScope forbidPluginDestructorScripting;
    if (m_parser)
        m_parser->stopParsing();

    if (m_loadEventProgress == LoadEventNotRun)
        return;

    if (m_loadEventProgress <= UnloadEventInProgress) {
        Element* currentFocusedElement = focusedElement();
        if (isHTMLInputElement(currentFocusedElement))
            toHTMLInputElement(*currentFocusedElement).endEditing();
        if (m_loadEventProgress < PageHideInProgress) {
            m_loadEventProgress = PageHideInProgress;
            if (LocalDOMWindow* window = domWindow())
                window->dispatchEvent(PageTransitionEvent::create(EventTypeNames::pagehide, false), this);
            if (!m_frame)
                return;

            PageVisibilityState visibilityState = pageVisibilityState();
            m_loadEventProgress = UnloadVisibilityChangeInProgress;
            if (visibilityState != PageVisibilityStateHidden && RuntimeEnabledFeatures::visibilityChangeOnUnloadEnabled()) {
                // Dispatch visibilitychange event, but don't bother doing
                // other notifications as we're about to be unloaded.
                dispatchEvent(Event::createBubble(EventTypeNames::visibilitychange));
                dispatchEvent(Event::createBubble(EventTypeNames::webkitvisibilitychange));
            }
            if (!m_frame)
                return;

            DocumentLoader* documentLoader = m_frame->loader().provisionalDocumentLoader();
            m_loadEventProgress = UnloadEventInProgress;
            Event* unloadEvent(Event::create(EventTypeNames::unload));
            if (documentLoader && !documentLoader->timing().unloadEventStart() && !documentLoader->timing().unloadEventEnd()) {
                DocumentLoadTiming& timing = documentLoader->timing();
                DCHECK(timing.navigationStart());
                timing.markUnloadEventStart();
                m_frame->localDOMWindow()->dispatchEvent(unloadEvent, this);
                timing.markUnloadEventEnd();
            } else {
                m_frame->localDOMWindow()->dispatchEvent(unloadEvent, m_frame->document());
            }
        }
        m_loadEventProgress = UnloadEventHandled;
    }

    if (!m_frame)
        return;

    // Don't remove event listeners from a transitional empty document (see https://bugs.webkit.org/show_bug.cgi?id=28716 for more information).
    bool keepEventListeners = m_frame->loader().provisionalDocumentLoader()
        && m_frame->shouldReuseDefaultView(m_frame->loader().provisionalDocumentLoader()->url());
    if (!keepEventListeners)
        removeAllEventListenersRecursively();
}

Document::PageDismissalType Document::pageDismissalEventBeingDispatched() const
{
    switch (m_loadEventProgress) {
    case BeforeUnloadEventInProgress:
        return BeforeUnloadDismissal;
    case PageHideInProgress:
        return PageHideDismissal;
    case UnloadVisibilityChangeInProgress:
        return UnloadVisibilityChangeDismissal;
    case UnloadEventInProgress:
        return UnloadDismissal;

    case LoadEventNotRun:
    case LoadEventInProgress:
    case LoadEventCompleted:
    case BeforeUnloadEventCompleted:
    case UnloadEventHandled:
        return NoDismissal;
    }
    ASSERT_NOT_REACHED();
    return NoDismissal;
}

void Document::setParsingState(ParsingState parsingState)
{
    m_parsingState = parsingState;

    if (parsing() && !m_elementDataCache)
        m_elementDataCache = ElementDataCache::create();
}

bool Document::shouldScheduleLayout() const
{
    // This function will only be called when FrameView thinks a layout is needed.
    // This enforces a couple extra rules.
    //
    //    (a) Only schedule a layout once the stylesheets are loaded.
    //    (b) Only schedule layout once we have a body element.
    if (!isActive())
        return false;

    if (isRenderingReady() && body())
        return true;

    if (documentElement() && !isHTMLHtmlElement(*documentElement()))
        return true;

    return false;
}

int Document::elapsedTime() const
{
    return static_cast<int>((currentTime() - m_startTime) * 1000);
}

void Document::write(const SegmentedString& text, Document* enteredDocument, ExceptionState& exceptionState)
{
    if (importLoader()) {
        exceptionState.throwDOMException(InvalidStateError, "Imported document doesn't support write().");
        return;
    }

    if (!isHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "Only HTML documents support write().");
        return;
    }

    if (enteredDocument && !getSecurityOrigin()->canAccess(enteredDocument->getSecurityOrigin())) {
        exceptionState.throwSecurityError("Can only call write() on same-origin documents.");
        return;
    }

    NestingLevelIncrementer nestingLevelIncrementer(m_writeRecursionDepth);

    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > 1) && m_writeRecursionIsTooDeep;
    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > cMaxWriteRecursionDepth) || m_writeRecursionIsTooDeep;

    if (m_writeRecursionIsTooDeep)
        return;

    bool hasInsertionPoint = m_parser && m_parser->hasInsertionPoint();

    if (!hasInsertionPoint && m_ignoreDestructiveWriteCount) {
        addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, ExceptionMessages::failedToExecute("write", "Document", "It isn't possible to write into a document from an asynchronously-loaded external script unless it is explicitly opened.")));
        return;
    }

    if (!hasInsertionPoint)
        open(enteredDocument, ASSERT_NO_EXCEPTION);

    DCHECK(m_parser);
    m_parser->insert(text);
}

void Document::write(const String& text, Document* enteredDocument, ExceptionState& exceptionState)
{
    write(SegmentedString(text), enteredDocument, exceptionState);
}

void Document::writeln(const String& text, Document* enteredDocument, ExceptionState& exceptionState)
{
    write(text, enteredDocument, exceptionState);
    if (exceptionState.hadException())
        return;
    write("\n", enteredDocument);
}

void Document::write(LocalDOMWindow* callingWindow, const Vector<String>& text, ExceptionState& exceptionState)
{
    DCHECK(callingWindow);
    StringBuilder builder;
    for (const String& string : text)
        builder.append(string);
    write(builder.toString(), callingWindow->document(), exceptionState);
}

void Document::writeln(LocalDOMWindow* callingWindow, const Vector<String>& text, ExceptionState& exceptionState)
{
    DCHECK(callingWindow);
    StringBuilder builder;
    for (const String& string : text)
        builder.append(string);
    writeln(builder.toString(), callingWindow->document(), exceptionState);
}

const KURL& Document::virtualURL() const
{
    return m_url;
}

KURL Document::virtualCompleteURL(const String& url) const
{
    return completeURL(url);
}

DOMTimerCoordinator* Document::timers()
{
    return &m_timers;
}

EventTarget* Document::errorEventTarget()
{
    return domWindow();
}

void Document::logExceptionToConsole(const String& errorMessage, std::unique_ptr<SourceLocation> location)
{
    ConsoleMessage* consoleMessage = ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, errorMessage, std::move(location));
    addConsoleMessage(consoleMessage);
}

void Document::setURL(const KURL& url)
{
    const KURL& newURL = url.isEmpty() ? blankURL() : url;
    if (newURL == m_url)
        return;

    m_url = newURL;
    m_accessEntryFromURL = nullptr;
    updateBaseURL();
    contextFeatures().urlDidChange(this);
}

KURL Document::validBaseElementURL() const
{
    if (m_baseElementURL.isValid())
        return m_baseElementURL;

    return KURL();
}

void Document::updateBaseURL()
{
    KURL oldBaseURL = m_baseURL;
    // DOM 3 Core: When the Document supports the feature "HTML" [DOM Level 2 HTML], the base URI is computed using
    // first the value of the href attribute of the HTML BASE element if any, and the value of the documentURI attribute
    // from the Document interface otherwise (which we store, preparsed, in m_url).
    if (!m_baseElementURL.isEmpty())
        m_baseURL = m_baseElementURL;
    else if (!m_baseURLOverride.isEmpty())
        m_baseURL = m_baseURLOverride;
    else
        m_baseURL = m_url;

    selectorQueryCache().invalidate();

    if (!m_baseURL.isValid())
        m_baseURL = KURL();

    if (m_elemSheet) {
        // Element sheet is silly. It never contains anything.
        DCHECK(!m_elemSheet->contents()->ruleCount());
        m_elemSheet = CSSStyleSheet::createInline(this, m_baseURL);
    }

    if (!equalIgnoringFragmentIdentifier(oldBaseURL, m_baseURL)) {
        // Base URL change changes any relative visited links.
        // FIXME: There are other URLs in the tree that would need to be re-evaluated on dynamic base URL change. Style should be invalidated too.
        for (HTMLAnchorElement& anchor : Traversal<HTMLAnchorElement>::startsAfter(*this))
            anchor.invalidateCachedVisitedLinkHash();
    }
}

void Document::setBaseURLOverride(const KURL& url)
{
    m_baseURLOverride = url;
    updateBaseURL();
}

void Document::processBaseElement()
{
    // Find the first href attribute in a base element and the first target attribute in a base element.
    const AtomicString* href = 0;
    const AtomicString* target = 0;
    for (HTMLBaseElement* base = Traversal<HTMLBaseElement>::firstWithin(*this); base && (!href || !target); base = Traversal<HTMLBaseElement>::next(*base)) {
        if (!href) {
            const AtomicString& value = base->fastGetAttribute(hrefAttr);
            if (!value.isNull())
                href = &value;
        }
        if (!target) {
            const AtomicString& value = base->fastGetAttribute(targetAttr);
            if (!value.isNull())
                target = &value;
        }
        if (contentSecurityPolicy()->isActive())
            UseCounter::count(*this, UseCounter::ContentSecurityPolicyWithBaseElement);
    }

    // FIXME: Since this doesn't share code with completeURL it may not handle encodings correctly.
    KURL baseElementURL;
    if (href) {
        String strippedHref = stripLeadingAndTrailingHTMLSpaces(*href);
        if (!strippedHref.isEmpty())
            baseElementURL = KURL(url(), strippedHref);
    }
    if (m_baseElementURL != baseElementURL && contentSecurityPolicy()->allowBaseURI(baseElementURL)) {
        m_baseElementURL = baseElementURL;
        updateBaseURL();
    }

    m_baseTarget = target ? *target : nullAtom;
}

String Document::userAgent() const
{
    return frame() ? frame()->loader().userAgent() : String();
}

void Document::disableEval(const String& errorMessage)
{
    if (!frame())
        return;

    frame()->script().disableEval(errorMessage);
}


void Document::didLoadAllImports()
{
    if (!haveScriptBlockingStylesheetsLoaded())
        return;
    if (!importLoader())
        styleResolverMayHaveChanged();
    didLoadAllScriptBlockingResources();
}

void Document::didRemoveAllPendingStylesheet()
{
    styleResolverMayHaveChanged();

    // Only imports on master documents can trigger rendering.
    if (HTMLImportLoader* import = importLoader())
        import->didRemoveAllPendingStylesheet();
    if (!haveImportsLoaded())
        return;
    didLoadAllScriptBlockingResources();
}

void Document::didLoadAllScriptBlockingResources()
{
    loadingTaskRunner()->postTask(BLINK_FROM_HERE, m_executeScriptsWaitingForResourcesTask->cancelAndCreate());

    if (isHTMLDocument() && body()) {
        // For HTML if we have no more stylesheets to load and we're past the body
        // tag, we should have something to paint so resume.
        beginLifecycleUpdatesIfRenderingReady();
    } else if (!isHTMLDocument() && documentElement()) {
        // For non-HTML there is no body so resume as soon as the sheets are loaded.
        beginLifecycleUpdatesIfRenderingReady();
    }

    if (m_gotoAnchorNeededAfterStylesheetsLoad && view())
        view()->processUrlFragment(m_url);
}

void Document::executeScriptsWaitingForResources()
{
    if (!isScriptExecutionReady())
        return;
    if (ScriptableDocumentParser* parser = scriptableDocumentParser())
        parser->executeScriptsWaitingForResources();
}

CSSStyleSheet& Document::elementSheet()
{
    if (!m_elemSheet)
        m_elemSheet = CSSStyleSheet::createInline(this, m_baseURL);
    return *m_elemSheet;
}

void Document::maybeHandleHttpRefresh(const String& content, HttpRefreshType httpRefreshType)
{
    if (m_isViewSource || !m_frame)
        return;

    double delay;
    String refreshURL;
    if (!parseHTTPRefresh(content, httpRefreshType == HttpRefreshFromMetaTag, delay, refreshURL))
        return;
    if (refreshURL.isEmpty())
        refreshURL = url().getString();
    else
        refreshURL = completeURL(refreshURL).getString();

    if (protocolIsJavaScript(refreshURL)) {
        String message = "Refused to refresh " + m_url.elidedString() + " to a javascript: URL";
        addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
        return;
    }

    if (httpRefreshType == HttpRefreshFromMetaTag && isSandboxed(SandboxAutomaticFeatures)) {
        String message = "Refused to execute the redirect specified via '<meta http-equiv='refresh' content='...'>'. The document is sandboxed, and the 'allow-scripts' keyword is not set.";
        addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
        return;
    }
    m_frame->navigationScheduler().scheduleRedirect(delay, refreshURL);
}

bool Document::shouldMergeWithLegacyDescription(ViewportDescription::Type origin) const
{
    return settings() && settings()->viewportMetaMergeContentQuirk() && m_legacyViewportDescription.isMetaViewportType() && m_legacyViewportDescription.type == origin;
}

void Document::setViewportDescription(const ViewportDescription& viewportDescription)
{
    if (viewportDescription.isLegacyViewportType()) {
        if (viewportDescription == m_legacyViewportDescription)
            return;
        m_legacyViewportDescription = viewportDescription;
    } else {
        if (viewportDescription == m_viewportDescription)
            return;
        m_viewportDescription = viewportDescription;

        // The UA-defined min-width is considered specifically by Android WebView quirks mode.
        if (!viewportDescription.isSpecifiedByAuthor())
            m_viewportDefaultMinWidth = viewportDescription.minWidth;
    }

    updateViewportDescription();
}

ViewportDescription Document::viewportDescription() const
{
    ViewportDescription appliedViewportDescription = m_viewportDescription;
    bool viewportMetaEnabled = settings() && settings()->viewportMetaEnabled();
    if (m_legacyViewportDescription.type != ViewportDescription::UserAgentStyleSheet && viewportMetaEnabled)
        appliedViewportDescription = m_legacyViewportDescription;
    if (shouldOverrideLegacyDescription(m_viewportDescription.type))
        appliedViewportDescription = m_viewportDescription;

    return appliedViewportDescription;
}

void Document::updateViewportDescription()
{
    if (frame() && frame()->isMainFrame()) {
        frameHost()->chromeClient().dispatchViewportPropertiesDidChange(viewportDescription());
    }
}

String Document::outgoingReferrer() const
{
    if (getSecurityOrigin()->isUnique()) {
        // Return |no-referrer|.
        return String();
    }

    // See http://www.whatwg.org/specs/web-apps/current-work/#fetching-resources
    // for why we walk the parent chain for srcdoc documents.
    const Document* referrerDocument = this;
    if (LocalFrame* frame = m_frame) {
        while (frame->document()->isSrcdocDocument()) {
            // Srcdoc documents must be local within the containing frame.
            frame = toLocalFrame(frame->tree().parent());
            // Srcdoc documents cannot be top-level documents, by definition,
            // because they need to be contained in iframes with the srcdoc.
            DCHECK(frame);
        }
        referrerDocument = frame->document();
    }
    return referrerDocument->m_url.strippedForUseAsReferrer();
}

ReferrerPolicy Document::getReferrerPolicy() const {
  ReferrerPolicy policy = ExecutionContext::getReferrerPolicy();
  // For srcdoc documents without their own policy, walk up the frame
  // tree to find the document that is either not a srcdoc or doesn't
  // have its own policy. This algorithm is defined in
  // https://html.spec.whatwg.org/multipage/browsers.html#set-up-a-browsing-context-environment-settings-object.
  if (!m_frame || policy != ReferrerPolicyDefault || !isSrcdocDocument()) {
    return policy;
  }
  LocalFrame* frame = toLocalFrame(m_frame->tree().parent());
  DCHECK(frame);
  return frame->document()->getReferrerPolicy();
}

MouseEventWithHitTestResults Document::prepareMouseEvent(const HitTestRequest& request, const LayoutPoint& documentPoint, const PlatformMouseEvent& event)
{
    DCHECK(!layoutView() || layoutView()->isLayoutView());

    // LayoutView::hitTest causes a layout, and we don't want to hit that until the first
    // layout because until then, there is nothing shown on the screen - the user can't
    // have intentionally clicked on something belonging to this page. Furthermore,
    // mousemove events before the first layout should not lead to a premature layout()
    // happening, which could show a flash of white.
    // See also the similar code in EventHandler::hitTestResultAtPoint.
    if (!layoutView() || !view() || !view()->didFirstLayout())
        return MouseEventWithHitTestResults(event, HitTestResult(request, LayoutPoint()));

    HitTestResult result(request, documentPoint);
    layoutView()->hitTest(result);

    if (!request.readOnly())
        updateHoverActiveState(request, result.innerElement());

    if (isHTMLCanvasElement(result.innerNode())) {
        PlatformMouseEvent eventWithRegion = event;
        std::pair<Element*, String> regionInfo = toHTMLCanvasElement(result.innerNode())->getControlAndIdIfHitRegionExists(result.pointInInnerNodeFrame());
        if (regionInfo.first)
            result.setInnerNode(regionInfo.first);
        eventWithRegion.setRegion(regionInfo.second);
        return MouseEventWithHitTestResults(eventWithRegion, result);
    }

    return MouseEventWithHitTestResults(event, result);
}

// DOM Section 1.1.1
bool Document::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ATTRIBUTE_NODE:
    case CDATA_SECTION_NODE:
    case DOCUMENT_FRAGMENT_NODE:
    case DOCUMENT_NODE:
    case TEXT_NODE:
        return false;
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
        return true;
    case DOCUMENT_TYPE_NODE:
    case ELEMENT_NODE:
        // Documents may contain no more than one of each of these.
        // (One Element and one DocumentType.)
        for (Node& c : NodeTraversal::childrenOf(*this)) {
            if (c.getNodeType() == type)
                return false;
        }
        return true;
    }
    return false;
}

bool Document::canAcceptChild(const Node& newChild, const Node* oldChild, ExceptionState& exceptionState) const
{
    if (oldChild && oldChild->getNodeType() == newChild.getNodeType())
        return true;

    int numDoctypes = 0;
    int numElements = 0;

    // First, check how many doctypes and elements we have, not counting
    // the child we're about to remove.
    for (Node& child : NodeTraversal::childrenOf(*this)) {
        if (oldChild && *oldChild == child)
            continue;

        switch (child.getNodeType()) {
        case DOCUMENT_TYPE_NODE:
            numDoctypes++;
            break;
        case ELEMENT_NODE:
            numElements++;
            break;
        default:
            break;
        }
    }

    // Then, see how many doctypes and elements might be added by the new child.
    if (newChild.isDocumentFragment()) {
        for (Node& child : NodeTraversal::childrenOf(toDocumentFragment(newChild))) {
            switch (child.getNodeType()) {
            case ATTRIBUTE_NODE:
            case CDATA_SECTION_NODE:
            case DOCUMENT_FRAGMENT_NODE:
            case DOCUMENT_NODE:
            case TEXT_NODE:
                exceptionState.throwDOMException(HierarchyRequestError, "Nodes of type '" + newChild.nodeName() +
                    "' may not be inserted inside nodes of type '#document'.");
                return false;
            case COMMENT_NODE:
            case PROCESSING_INSTRUCTION_NODE:
                break;
            case DOCUMENT_TYPE_NODE:
                numDoctypes++;
                break;
            case ELEMENT_NODE:
                numElements++;
                break;
            }
        }
    } else {
        switch (newChild.getNodeType()) {
        case ATTRIBUTE_NODE:
        case CDATA_SECTION_NODE:
        case DOCUMENT_FRAGMENT_NODE:
        case DOCUMENT_NODE:
        case TEXT_NODE:
            exceptionState.throwDOMException(HierarchyRequestError, "Nodes of type '" + newChild.nodeName() +
                "' may not be inserted inside nodes of type '#document'.");
            return false;
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            return true;
        case DOCUMENT_TYPE_NODE:
            numDoctypes++;
            break;
        case ELEMENT_NODE:
            numElements++;
            break;
        }
    }

    if (numElements > 1 || numDoctypes > 1) {
        exceptionState.throwDOMException(HierarchyRequestError,
            String::format("Only one %s on document allowed.",
            numElements > 1 ? "element" : "doctype"));
        return false;
    }

    return true;
}

Node* Document::cloneNode(bool deep)
{
    Document* clone = cloneDocumentWithoutChildren();
    clone->cloneDataFromDocument(*this);
    if (deep)
        cloneChildNodes(clone);
    return clone;
}

Document* Document::cloneDocumentWithoutChildren()
{
    DocumentInit init(url());
    if (isXMLDocument()) {
        if (isXHTMLDocument())
            return XMLDocument::createXHTML(init.withRegistrationContext(registrationContext()));
        return XMLDocument::create(init);
    }
    return create(init);
}

void Document::cloneDataFromDocument(const Document& other)
{
    setCompatibilityMode(other.getCompatibilityMode());
    setEncodingData(other.m_encodingData);
    setContextFeatures(other.contextFeatures());
    setSecurityOrigin(other.getSecurityOrigin()->isolatedCopy());
    setMimeType(other.contentType());
}

bool Document::isSecureContextImpl(const SecureContextCheck privilegeContextCheck) const
{
    // There may be exceptions for the secure context check defined for certain
    // schemes. The exceptions are applied only to the special scheme and to
    // sandboxed URLs from those origins, but *not* to any children.
    //
    // For example:
    //   <iframe src="http://host">
    //     <iframe src="scheme-has-exception://host"></iframe>
    //     <iframe sandbox src="scheme-has-exception://host"></iframe>
    //   </iframe>
    // both inner iframes pass this check, assuming that the scheme
    // "scheme-has-exception:" is granted an exception.
    //
    // However,
    //   <iframe src="http://host">
    //     <iframe sandbox src="http://host"></iframe>
    //   </iframe>
    // would fail the check (that is, sandbox does not grant an exception itself).
    //
    // Additionally, with
    //   <iframe src="scheme-has-exception://host">
    //     <iframe src="http://host"></iframe>
    //     <iframe sandbox src="http://host"></iframe>
    //   </iframe>
    // both inner iframes would fail the check, even though the outermost iframe
    // passes.
    //
    // In all cases, a frame must be potentially trustworthy in addition to
    // having an exception listed in order for the exception to be granted.
    if (!getSecurityOrigin()->isPotentiallyTrustworthy())
        return false;

    if (SchemeRegistry::schemeShouldBypassSecureContextCheck(getSecurityOrigin()->protocol()))
        return true;

    if (privilegeContextCheck == StandardSecureContextCheck) {
        if (!m_frame)
            return true;
        Frame* parent = m_frame->tree().parent();
        while (parent) {
            if (!parent->securityContext()->getSecurityOrigin()->isPotentiallyTrustworthy())
                return false;
            parent = parent->tree().parent();
        }
    }
    return true;
}

StyleSheetList& Document::styleSheets()
{
    if (!m_styleSheetList)
        m_styleSheetList = StyleSheetList::create(this);
    return *m_styleSheetList;
}

String Document::preferredStylesheetSet() const
{
    return m_styleEngine->preferredStylesheetSetName();
}

String Document::selectedStylesheetSet() const
{
    return m_styleEngine->selectedStylesheetSetName();
}

void Document::setSelectedStylesheetSet(const String& aString)
{
    styleEngine().setSelectedStylesheetSetName(aString);
}

void Document::evaluateMediaQueryListIfNeeded()
{
    if (!m_evaluateMediaQueriesOnStyleRecalc)
        return;
    evaluateMediaQueryList();
    m_evaluateMediaQueriesOnStyleRecalc = false;
}

void Document::evaluateMediaQueryList()
{
    if (m_mediaQueryMatcher)
        m_mediaQueryMatcher->mediaFeaturesChanged();
}

void Document::notifyResizeForViewportUnits()
{
    if (m_mediaQueryMatcher)
        m_mediaQueryMatcher->viewportChanged();
    if (!hasViewportUnits())
        return;
    ensureStyleResolver().notifyResizeForViewportUnits();
    setNeedsStyleRecalcForViewportUnits();
}

void Document::styleResolverMayHaveChanged()
{
    styleEngine().resolverChanged(hasNodesWithPlaceholderStyle() ? FullStyleUpdate : AnalyzedStyleUpdate);

    if (didLayoutWithPendingStylesheets() && !styleEngine().hasPendingScriptBlockingSheets()) {
        // We need to manually repaint because we avoid doing all repaints in layout or style
        // recalc while sheets are still loading to avoid FOUC.
        m_pendingSheetLayout = IgnoreLayoutWithPendingSheets;

        DCHECK(layoutView() || importsController());
        if (layoutView())
            layoutView()->invalidatePaintForViewAndCompositedLayers();
    }
}

void Document::setHoverNode(Node* newHoverNode)
{
    m_hoverNode = newHoverNode;
}

void Document::setActiveHoverElement(Element* newActiveElement)
{
    if (!newActiveElement) {
        m_activeHoverElement.clear();
        return;
    }

    m_activeHoverElement = newActiveElement;
}

void Document::removeFocusedElementOfSubtree(Node* node, bool amongChildrenOnly)
{
    if (!m_focusedElement)
        return;

    // We can't be focused if we're not in the document.
    if (!node->inShadowIncludingDocument())
        return;
    bool contains = node->isShadowIncludingInclusiveAncestorOf(m_focusedElement.get());
    if (contains && (m_focusedElement != node || !amongChildrenOnly))
        clearFocusedElement();
}

void Document::hoveredNodeDetached(Element& element)
{
    if (!m_hoverNode)
        return;

    m_hoverNode->updateDistribution();
    if (element != m_hoverNode && (!m_hoverNode->isTextNode() || element != FlatTreeTraversal::parent(*m_hoverNode)))
        return;

    m_hoverNode = FlatTreeTraversal::parent(element);
    while (m_hoverNode && !m_hoverNode->layoutObject())
        m_hoverNode = FlatTreeTraversal::parent(*m_hoverNode);

    // If the mouse cursor is not visible, do not clear existing
    // hover effects on the ancestors of |element| and do not invoke
    // new hover effects on any other element.
    if (!page()->isCursorVisible())
        return;

    if (frame())
        frame()->eventHandler().scheduleHoverStateUpdate();
}

void Document::activeChainNodeDetached(Element& element)
{
    if (!m_activeHoverElement)
        return;

    if (element != m_activeHoverElement)
        return;

    Node* activeNode = FlatTreeTraversal::parent(element);
    while (activeNode && activeNode->isElementNode() && !activeNode->layoutObject())
        activeNode = FlatTreeTraversal::parent(*activeNode);

    m_activeHoverElement = activeNode && activeNode->isElementNode() ? toElement(activeNode) : 0;
}

const Vector<AnnotatedRegionValue>& Document::annotatedRegions() const
{
    return m_annotatedRegions;
}

void Document::setAnnotatedRegions(const Vector<AnnotatedRegionValue>& regions)
{
    m_annotatedRegions = regions;
    setAnnotatedRegionsDirty(false);
}

bool Document::setFocusedElement(Element* prpNewFocusedElement, const FocusParams& params)
{
    DCHECK(!m_lifecycle.inDetach());

    m_clearFocusedElementTimer.stop();

    Element* newFocusedElement = prpNewFocusedElement;

    // Make sure newFocusedNode is actually in this document
    if (newFocusedElement && (newFocusedElement->document() != this))
        return true;

    if (NodeChildRemovalTracker::isBeingRemoved(newFocusedElement))
        return true;

    if (m_focusedElement == newFocusedElement)
        return true;

    bool focusChangeBlocked = false;
    Element* oldFocusedElement = m_focusedElement;
    m_focusedElement = nullptr;

    // Remove focus from the existing focus node (if any)
    if (oldFocusedElement) {
        oldFocusedElement->setFocus(false);

        // Dispatch the blur event and let the node do any other blur related activities (important for text fields)
        // If page lost focus, blur event will have already been dispatched
        if (page() && (page()->focusController().isFocused())) {
            oldFocusedElement->dispatchBlurEvent(newFocusedElement, params.type, params.sourceCapabilities);

            if (m_focusedElement) {
                // handler shifted focus
                focusChangeBlocked = true;
                newFocusedElement = nullptr;
            }

            oldFocusedElement->dispatchFocusOutEvent(EventTypeNames::focusout, newFocusedElement, params.sourceCapabilities); // DOM level 3 name for the bubbling blur event.
            // FIXME: We should remove firing DOMFocusOutEvent event when we are sure no content depends
            // on it, probably when <rdar://problem/8503958> is resolved.
            oldFocusedElement->dispatchFocusOutEvent(EventTypeNames::DOMFocusOut, newFocusedElement, params.sourceCapabilities); // DOM level 2 name for compatibility.

            if (m_focusedElement) {
                // handler shifted focus
                focusChangeBlocked = true;
                newFocusedElement = nullptr;
            }
        }

        if (view()) {
            Widget* oldWidget = widgetForElement(*oldFocusedElement);
            if (oldWidget)
                oldWidget->setFocus(false, params.type);
            else
                view()->setFocus(false, params.type);
        }
    }

    if (newFocusedElement)
        updateStyleAndLayoutTreeForNode(newFocusedElement);
    if (newFocusedElement && newFocusedElement->isFocusable()) {
        if (newFocusedElement->isRootEditableElement() && !acceptsEditingFocus(*newFocusedElement)) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusedElementDone;
        }
        // Set focus on the new node
        m_focusedElement = newFocusedElement;
        setSequentialFocusNavigationStartingPoint(m_focusedElement.get());

        m_focusedElement->setFocus(true);
        // Element::setFocus for frames can dispatch events.
        if (m_focusedElement != newFocusedElement) {
            focusChangeBlocked = true;
            goto SetFocusedElementDone;
        }
        cancelFocusAppearanceUpdate();
        m_focusedElement->updateFocusAppearance(params.selectionBehavior);

        // Dispatch the focus event and let the node do any other focus related activities (important for text fields)
        // If page lost focus, event will be dispatched on page focus, don't duplicate
        if (page() && (page()->focusController().isFocused())) {
            m_focusedElement->dispatchFocusEvent(oldFocusedElement, params.type, params.sourceCapabilities);


            if (m_focusedElement != newFocusedElement) {
                // handler shifted focus
                focusChangeBlocked = true;
                goto SetFocusedElementDone;
            }
            m_focusedElement->dispatchFocusInEvent(EventTypeNames::focusin, oldFocusedElement, params.type, params.sourceCapabilities); // DOM level 3 bubbling focus event.

            if (m_focusedElement != newFocusedElement) {
                // handler shifted focus
                focusChangeBlocked = true;
                goto SetFocusedElementDone;
            }

            // FIXME: We should remove firing DOMFocusInEvent event when we are sure no content depends
            // on it, probably when <rdar://problem/8503958> is m.
            m_focusedElement->dispatchFocusInEvent(EventTypeNames::DOMFocusIn, oldFocusedElement, params.type, params.sourceCapabilities); // DOM level 2 for compatibility.

            if (m_focusedElement != newFocusedElement) {
                // handler shifted focus
                focusChangeBlocked = true;
                goto SetFocusedElementDone;
            }
        }

        if (m_focusedElement->isRootEditableElement())
            frame()->spellChecker().didBeginEditing(m_focusedElement.get());

        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if (view()) {
            Widget* focusWidget = widgetForElement(*m_focusedElement);
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the Widget code.
                // Specifically, in WebCore this does not work well for text fields.
                updateStyleAndLayout();
                // Re-get the widget in case updating the layout changed things.
                focusWidget = widgetForElement(*m_focusedElement);
            }
            if (focusWidget)
                focusWidget->setFocus(true, params.type);
            else
                view()->setFocus(true, params.type);
        }
    }

    if (!focusChangeBlocked && m_focusedElement) {
        // Create the AXObject cache in a focus change because Chromium relies on it.
        if (AXObjectCache* cache = axObjectCache())
            cache->handleFocusedUIElementChanged(oldFocusedElement, newFocusedElement);
    }

    if (!focusChangeBlocked && frameHost())
        frameHost()->chromeClient().focusedNodeChanged(oldFocusedElement, m_focusedElement.get());

SetFocusedElementDone:
    updateStyleAndLayoutTree();
    if (LocalFrame* frame = this->frame())
        frame->selection().didChangeFocus();
    return !focusChangeBlocked;
}

void Document::clearFocusedElement()
{
    setFocusedElement(nullptr, FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
}

void Document::setSequentialFocusNavigationStartingPoint(Node* node)
{
    if (!m_frame)
        return;
    if (!node) {
        m_sequentialFocusNavigationStartingPoint = nullptr;
        return;
    }
    DCHECK_EQ(node->document(), this);
    if (!m_sequentialFocusNavigationStartingPoint)
        m_sequentialFocusNavigationStartingPoint = Range::create(*this);
    m_sequentialFocusNavigationStartingPoint->selectNodeContents(node, ASSERT_NO_EXCEPTION);
}

Element* Document::sequentialFocusNavigationStartingPoint(WebFocusType type) const
{
    if (m_focusedElement)
        return m_focusedElement.get();
    if (!m_sequentialFocusNavigationStartingPoint)
        return nullptr;
    if (!m_sequentialFocusNavigationStartingPoint->collapsed()) {
        Node* node = m_sequentialFocusNavigationStartingPoint->startContainer();
        DCHECK_EQ(node, m_sequentialFocusNavigationStartingPoint->endContainer());
        if (node->isElementNode())
            return toElement(node);
        if (Element* neighborElement = type == WebFocusTypeForward ? ElementTraversal::previous(*node) : ElementTraversal::next(*node))
            return neighborElement;
        return node->parentOrShadowHostElement();
    }

    // Range::selectNodeContents didn't select contents because the element had
    // no children.
    if (m_sequentialFocusNavigationStartingPoint->startContainer()->isElementNode()
        && !m_sequentialFocusNavigationStartingPoint->startContainer()->hasChildren()
        && m_sequentialFocusNavigationStartingPoint->startOffset() == 0)
        return toElement(m_sequentialFocusNavigationStartingPoint->startContainer());

    // A node selected by Range::selectNodeContents was removed from the
    // document tree.
    if (Node* nextNode = m_sequentialFocusNavigationStartingPoint->firstNode()) {
        if (type == WebFocusTypeForward)
            return ElementTraversal::previous(*nextNode);
        if (nextNode->isElementNode())
            return toElement(nextNode);
        return ElementTraversal::next(*nextNode);
    }
    return nullptr;
}

void Document::setCSSTarget(Element* newTarget)
{
    if (m_cssTarget)
        m_cssTarget->pseudoStateChanged(CSSSelector::PseudoTarget);
    m_cssTarget = newTarget;
    if (m_cssTarget)
        m_cssTarget->pseudoStateChanged(CSSSelector::PseudoTarget);
}

void Document::registerNodeList(const LiveNodeListBase* list)
{
    DCHECK(!m_nodeLists[list->invalidationType()].contains(list));
    m_nodeLists[list->invalidationType()].add(list);
    if (list->isRootedAtTreeScope())
        m_listsInvalidatedAtDocument.add(list);
}

void Document::unregisterNodeList(const LiveNodeListBase* list)
{
    DCHECK(m_nodeLists[list->invalidationType()].contains(list));
    m_nodeLists[list->invalidationType()].remove(list);
    if (list->isRootedAtTreeScope()) {
        DCHECK(m_listsInvalidatedAtDocument.contains(list));
        m_listsInvalidatedAtDocument.remove(list);
    }
}

void Document::registerNodeListWithIdNameCache(const LiveNodeListBase* list)
{
    DCHECK(!m_nodeLists[InvalidateOnIdNameAttrChange].contains(list));
    m_nodeLists[InvalidateOnIdNameAttrChange].add(list);
}

void Document::unregisterNodeListWithIdNameCache(const LiveNodeListBase* list)
{
    DCHECK(m_nodeLists[InvalidateOnIdNameAttrChange].contains(list));
    m_nodeLists[InvalidateOnIdNameAttrChange].remove(list);
}

void Document::attachNodeIterator(NodeIterator* ni)
{
    m_nodeIterators.add(ni);
}

void Document::detachNodeIterator(NodeIterator* ni)
{
    // The node iterator can be detached without having been attached if its root node didn't have a document
    // when the iterator was created, but has it now.
    m_nodeIterators.remove(ni);
}

void Document::moveNodeIteratorsToNewDocument(Node& node, Document& newDocument)
{
    HeapHashSet<WeakMember<NodeIterator>> nodeIteratorsList = m_nodeIterators;
    for (NodeIterator* ni : nodeIteratorsList) {
        if (ni->root() == node) {
            detachNodeIterator(ni);
            newDocument.attachNodeIterator(ni);
        }
    }
}

void Document::updateRangesAfterNodeMovedToAnotherDocument(const Node& node)
{
    DCHECK_NE(node.document(), this);
    if (m_ranges.isEmpty())
        return;

    AttachedRangeSet ranges = m_ranges;
    for (Range* range : ranges)
        range->updateOwnerDocumentIfNeeded();
}

void Document::nodeChildrenWillBeRemoved(ContainerNode& container)
{
    EventDispatchForbiddenScope assertNoEventDispatch;
    for (Range* range : m_ranges)
        range->nodeChildrenWillBeRemoved(container);

    for (NodeIterator* ni : m_nodeIterators) {
        for (Node& n : NodeTraversal::childrenOf(container))
            ni->nodeWillBeRemoved(n);
    }

    if (LocalFrame* frame = this->frame()) {
        for (Node& n : NodeTraversal::childrenOf(container)) {
            frame->eventHandler().nodeWillBeRemoved(n);
            frame->selection().nodeWillBeRemoved(n);
            frame->page()->dragCaretController().nodeWillBeRemoved(n);
        }
    }

    if (containsV1ShadowTree()) {
        for (Node& n : NodeTraversal::childrenOf(container))
            n.checkSlotChangeBeforeRemoved();
    }
}

void Document::nodeWillBeRemoved(Node& n)
{
    for (NodeIterator* ni : m_nodeIterators)
        ni->nodeWillBeRemoved(n);

    for (Range* range : m_ranges)
        range->nodeWillBeRemoved(n);

    if (LocalFrame* frame = this->frame()) {
        frame->eventHandler().nodeWillBeRemoved(n);
        frame->selection().nodeWillBeRemoved(n);
        frame->page()->dragCaretController().nodeWillBeRemoved(n);
    }

    if (containsV1ShadowTree())
        n.checkSlotChangeBeforeRemoved();
}

void Document::dataWillChange(const CharacterData& characterData)
{
    if (LocalFrame* frame = this->frame())
        frame->selection().dataWillChange(characterData);
}

void Document::didInsertText(Node* text, unsigned offset, unsigned length)
{
    for (Range* range : m_ranges)
        range->didInsertText(text, offset, length);

    m_markers->shiftMarkers(text, offset, length);
}

void Document::didRemoveText(Node* text, unsigned offset, unsigned length)
{
    for (Range* range : m_ranges)
        range->didRemoveText(text, offset, length);

    m_markers->removeMarkers(text, offset, length);
    m_markers->shiftMarkers(text, offset + length, 0 - length);
}

void Document::didMergeTextNodes(Text& oldNode, unsigned offset)
{
    if (!m_ranges.isEmpty()) {
        NodeWithIndex oldNodeWithIndex(oldNode);
        for (Range* range : m_ranges)
            range->didMergeTextNodes(oldNodeWithIndex, offset);
    }

    if (m_frame)
        m_frame->selection().didMergeTextNodes(oldNode, offset);

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::didSplitTextNode(Text& oldNode)
{
    for (Range* range : m_ranges)
        range->didSplitTextNode(oldNode);

    if (m_frame)
        m_frame->selection().didSplitTextNode(oldNode);

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::setWindowAttributeEventListener(const AtomicString& eventType, EventListener* listener)
{
    LocalDOMWindow* domWindow = this->domWindow();
    if (!domWindow)
        return;
    domWindow->setAttributeEventListener(eventType, listener);
}

EventListener* Document::getWindowAttributeEventListener(const AtomicString& eventType)
{
    LocalDOMWindow* domWindow = this->domWindow();
    if (!domWindow)
        return 0;
    return domWindow->getAttributeEventListener(eventType);
}

EventQueue* Document::getEventQueue() const
{
    if (!m_domWindow)
        return 0;
    return m_domWindow->getEventQueue();
}

void Document::enqueueAnimationFrameEvent(Event* event)
{
    ensureScriptedAnimationController().enqueueEvent(event);
}

void Document::enqueueUniqueAnimationFrameEvent(Event* event)
{
    ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::enqueueScrollEventForNode(Node* target)
{
    // Per the W3C CSSOM View Module only scroll events fired at the document should bubble.
    Event* scrollEvent = target->isDocumentNode() ? Event::createBubble(EventTypeNames::scroll) : Event::create(EventTypeNames::scroll);
    scrollEvent->setTarget(target);
    ensureScriptedAnimationController().enqueuePerFrameEvent(scrollEvent);
}

void Document::enqueueResizeEvent()
{
    Event* event = Event::create(EventTypeNames::resize);
    event->setTarget(domWindow());
    ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::enqueueMediaQueryChangeListeners(HeapVector<Member<MediaQueryListListener>>& listeners)
{
    ensureScriptedAnimationController().enqueueMediaQueryChangeListeners(listeners);
}

void Document::enqueueVisualViewportScrollEvent()
{
    Event* event = Event::create(EventTypeNames::scroll);
    event->setTarget(domWindow()->visualViewport());
    ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::enqueueVisualViewportResizeEvent()
{
    Event* event = Event::create(EventTypeNames::resize);
    event->setTarget(domWindow()->visualViewport());
    ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::dispatchEventsForPrinting()
{
    if (!m_scriptedAnimationController)
        return;
    m_scriptedAnimationController->dispatchEventsAndCallbacksForPrinting();
}

Document::EventFactorySet& Document::eventFactories()
{
    DEFINE_STATIC_LOCAL(EventFactorySet, s_eventFactory, ());
    return s_eventFactory;
}

const OriginAccessEntry& Document::accessEntryFromURL()
{
    if (!m_accessEntryFromURL) {
        m_accessEntryFromURL = wrapUnique(new OriginAccessEntry(url().protocol(), url().host(), OriginAccessEntry::AllowRegisterableDomains));
    }
    return *m_accessEntryFromURL;
}

void Document::registerEventFactory(std::unique_ptr<EventFactoryBase> eventFactory)
{
    DCHECK(!eventFactories().contains(eventFactory.get()));
    eventFactories().add(std::move(eventFactory));
}

Event* Document::createEvent(ExecutionContext* executionContext, const String& eventType, ExceptionState& exceptionState)
{
    Event* event = nullptr;
    for (const auto& factory : eventFactories()) {
        event = factory->create(executionContext, eventType);
        if (event)
            return event;
    }
    exceptionState.throwDOMException(NotSupportedError, "The provided event type ('" + eventType + "') is invalid.");
    return nullptr;
}

void Document::addMutationEventListenerTypeIfEnabled(ListenerType listenerType)
{
    if (ContextFeatures::mutationEventsEnabled(this))
        addListenerType(listenerType);
}

void Document::addListenerTypeIfNeeded(const AtomicString& eventType)
{
    if (eventType == EventTypeNames::DOMSubtreeModified) {
        UseCounter::count(*this, UseCounter::DOMSubtreeModifiedEvent);
        addMutationEventListenerTypeIfEnabled(DOMSUBTREEMODIFIED_LISTENER);
    } else if (eventType == EventTypeNames::DOMNodeInserted) {
        UseCounter::count(*this, UseCounter::DOMNodeInsertedEvent);
        addMutationEventListenerTypeIfEnabled(DOMNODEINSERTED_LISTENER);
    } else if (eventType == EventTypeNames::DOMNodeRemoved) {
        UseCounter::count(*this, UseCounter::DOMNodeRemovedEvent);
        addMutationEventListenerTypeIfEnabled(DOMNODEREMOVED_LISTENER);
    } else if (eventType == EventTypeNames::DOMNodeRemovedFromDocument) {
        UseCounter::count(*this, UseCounter::DOMNodeRemovedFromDocumentEvent);
        addMutationEventListenerTypeIfEnabled(DOMNODEREMOVEDFROMDOCUMENT_LISTENER);
    } else if (eventType == EventTypeNames::DOMNodeInsertedIntoDocument) {
        UseCounter::count(*this, UseCounter::DOMNodeInsertedIntoDocumentEvent);
        addMutationEventListenerTypeIfEnabled(DOMNODEINSERTEDINTODOCUMENT_LISTENER);
    } else if (eventType == EventTypeNames::DOMCharacterDataModified) {
        UseCounter::count(*this, UseCounter::DOMCharacterDataModifiedEvent);
        addMutationEventListenerTypeIfEnabled(DOMCHARACTERDATAMODIFIED_LISTENER);
    } else if (eventType == EventTypeNames::webkitAnimationStart || eventType == EventTypeNames::animationstart) {
        addListenerType(ANIMATIONSTART_LISTENER);
    } else if (eventType == EventTypeNames::webkitAnimationEnd || eventType == EventTypeNames::animationend) {
        addListenerType(ANIMATIONEND_LISTENER);
    } else if (eventType == EventTypeNames::webkitAnimationIteration || eventType == EventTypeNames::animationiteration) {
        addListenerType(ANIMATIONITERATION_LISTENER);
        if (view()) {
            // Need to re-evaluate time-to-effect-change for any running animations.
            view()->scheduleAnimation();
        }
    } else if (eventType == EventTypeNames::webkitTransitionEnd || eventType == EventTypeNames::transitionend) {
        addListenerType(TRANSITIONEND_LISTENER);
    } else if (eventType == EventTypeNames::scroll) {
        addListenerType(SCROLL_LISTENER);
    }
}

HTMLFrameOwnerElement* Document::localOwner() const
{
    if (!frame())
        return 0;
    // FIXME: This probably breaks the attempts to layout after a load is finished in implicitClose(), and probably tons of other things...
    return frame()->deprecatedLocalOwner();
}

bool Document::isInInvisibleSubframe() const
{
    if (!localOwner())
        return false; // this is a local root element

    // TODO(bokan): This looks like it doesn't work in OOPIF.
    DCHECK(frame());
    return !frame()->ownerLayoutObject();
}

String Document::cookie(ExceptionState& exceptionState) const
{
    if (settings() && !settings()->cookieEnabled())
        return String();

    // FIXME: The HTML5 DOM spec states that this attribute can raise an
    // InvalidStateError exception on getting if the Document has no
    // browsing context.

    if (!getSecurityOrigin()->canAccessCookies()) {
        if (isSandboxed(SandboxOrigin))
            exceptionState.throwSecurityError("The document is sandboxed and lacks the 'allow-same-origin' flag.");
        else if (url().protocolIs("data"))
            exceptionState.throwSecurityError("Cookies are disabled inside 'data:' URLs.");
        else
            exceptionState.throwSecurityError("Access is denied for this document.");
        return String();
    }

    // Suborigins are cookie-averse and thus should always return the empty
    // string, unless the 'unsafe-cookies' option is provided.
    if (getSecurityOrigin()->hasSuborigin() && !getSecurityOrigin()->suborigin()->policyContains(Suborigin::SuboriginPolicyOptions::UnsafeCookies))
        return String();

    KURL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return String();

    return cookies(this, cookieURL);
}

void Document::setCookie(const String& value, ExceptionState& exceptionState)
{
    if (settings() && !settings()->cookieEnabled())
        return;

    // FIXME: The HTML5 DOM spec states that this attribute can raise an
    // InvalidStateError exception on setting if the Document has no
    // browsing context.

    if (!getSecurityOrigin()->canAccessCookies()) {
        if (isSandboxed(SandboxOrigin))
            exceptionState.throwSecurityError("The document is sandboxed and lacks the 'allow-same-origin' flag.");
        else if (url().protocolIs("data"))
            exceptionState.throwSecurityError("Cookies are disabled inside 'data:' URLs.");
        else
            exceptionState.throwSecurityError("Access is denied for this document.");
        return;
    }

    // Suborigins are cookie-averse and thus setting should be a no-op, unless
    // the 'unsafe-cookies' option is provided.
    if (getSecurityOrigin()->hasSuborigin() && !getSecurityOrigin()->suborigin()->policyContains(Suborigin::SuboriginPolicyOptions::UnsafeCookies))
        return;

    KURL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return;

    setCookies(this, cookieURL, value);
}

const AtomicString& Document::referrer() const
{
    if (loader())
        return loader()->request().httpReferrer();
    return nullAtom;
}

String Document::domain() const
{
    return getSecurityOrigin()->domain();
}

void Document::setDomain(const String& newDomain, ExceptionState& exceptionState)
{
    UseCounter::count(*this, UseCounter::DocumentSetDomain);

    if (isSandboxed(SandboxDocumentDomain)) {
        exceptionState.throwSecurityError("Assignment is forbidden for sandboxed iframes.");
        return;
    }

    if (SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(getSecurityOrigin()->protocol())) {
        exceptionState.throwSecurityError("Assignment is forbidden for the '" + getSecurityOrigin()->protocol() + "' scheme.");
        return;
    }

    if (newDomain.isEmpty()) {
        exceptionState.throwSecurityError("'" + newDomain + "' is an empty domain.");
        return;
    }

    OriginAccessEntry accessEntry(getSecurityOrigin()->protocol(), newDomain, OriginAccessEntry::AllowSubdomains);
    OriginAccessEntry::MatchResult result = accessEntry.matchesOrigin(*getSecurityOrigin());
    if (result == OriginAccessEntry::DoesNotMatchOrigin) {
        exceptionState.throwSecurityError("'" + newDomain + "' is not a suffix of '" + domain() + "'.");
        return;
    }

    if (result == OriginAccessEntry::MatchesOriginButIsPublicSuffix) {
        exceptionState.throwSecurityError("'" + newDomain + "' is a top-level domain.");
        return;
    }

    getSecurityOrigin()->setDomainFromDOM(newDomain);
    if (m_frame)
        m_frame->script().updateSecurityOrigin(getSecurityOrigin());
}

// http://www.whatwg.org/specs/web-apps/current-work/#dom-document-lastmodified
String Document::lastModified() const
{
    DateComponents date;
    bool foundDate = false;
    if (m_frame) {
        if (DocumentLoader* documentLoader = loader()) {
            const AtomicString& httpLastModified = documentLoader->response().httpHeaderField(HTTPNames::Last_Modified);
            if (!httpLastModified.isEmpty()) {
                date.setMillisecondsSinceEpochForDateTime(convertToLocalTime(parseDate(httpLastModified)));
                foundDate = true;
            }
        }
    }
    // FIXME: If this document came from the file system, the HTML5
    // specificiation tells us to read the last modification date from the file
    // system.
    if (!foundDate)
        date.setMillisecondsSinceEpochForDateTime(convertToLocalTime(currentTimeMS()));
    return String::format("%02d/%02d/%04d %02d:%02d:%02d", date.month() + 1, date.monthDay(), date.fullYear(), date.hour(), date.minute(), date.second());
}

const KURL Document::firstPartyForCookies() const
{
    // TODO(mkwst): This doesn't properly handle HTML Import documents.

    // If this is an imported document, grab its master document's first-party:
    if (importsController() && importsController()->master() && importsController()->master() != this)
        return importsController()->master()->firstPartyForCookies();

    if (!frame())
        return SecurityOrigin::urlWithUniqueSecurityOrigin();

    // TODO(mkwst): This doesn't correctly handle sandboxed documents; we want to look at their URL,
    // but we can't because we don't know what it is.
    Frame* top = frame()->tree().top();
    KURL topDocumentURL = top->isLocalFrame()
        ? toLocalFrame(top)->document()->url()
        : KURL(KURL(), top->securityContext()->getSecurityOrigin()->toString());
    if (SchemeRegistry::shouldTreatURLSchemeAsFirstPartyWhenTopLevel(topDocumentURL.protocol()))
        return topDocumentURL;

    // We're intentionally using the URL of each document rather than the document's SecurityOrigin.
    // Sandboxing a document into a unique origin shouldn't effect first-/third-party status for
    // cookies and site data.
    const OriginAccessEntry& accessEntry = top->isLocalFrame()
        ? toLocalFrame(top)->document()->accessEntryFromURL()
        : OriginAccessEntry(topDocumentURL.protocol(), topDocumentURL.host(), OriginAccessEntry::AllowRegisterableDomains);
    const Frame* currentFrame = frame();
    while (currentFrame) {
        // Skip over srcdoc documents, as they are always same-origin with their closest non-srcdoc parent.
        while (currentFrame->isLocalFrame() && toLocalFrame(currentFrame)->document()->isSrcdocDocument())
            currentFrame = currentFrame->tree().parent();
        DCHECK(currentFrame);

        // We use 'matchesDomain' here, as it turns out that some folks embed HTTPS login forms
        // into HTTP pages; we should allow this kind of upgrade.
        if (accessEntry.matchesDomain(*currentFrame->securityContext()->getSecurityOrigin()) == OriginAccessEntry::DoesNotMatchOrigin)
            return SecurityOrigin::urlWithUniqueSecurityOrigin();

        currentFrame = currentFrame->tree().parent();
    }

    return topDocumentURL;
}

static bool isValidNameNonASCII(const LChar* characters, unsigned length)
{
    if (!isValidNameStart(characters[0]))
        return false;

    for (unsigned i = 1; i < length; ++i) {
        if (!isValidNamePart(characters[i]))
            return false;
    }

    return true;
}

static bool isValidNameNonASCII(const UChar* characters, unsigned length)
{
    for (unsigned i = 0; i < length;) {
        bool first = i == 0;
        UChar32 c;
        U16_NEXT(characters, i, length, c); // Increments i.
        if (first ? !isValidNameStart(c) : !isValidNamePart(c))
            return false;
    }

    return true;
}

template<typename CharType>
static inline bool isValidNameASCII(const CharType* characters, unsigned length)
{
    CharType c = characters[0];
    if (!(isASCIIAlpha(c) || c == ':' || c == '_'))
        return false;

    for (unsigned i = 1; i < length; ++i) {
        c = characters[i];
        if (!(isASCIIAlphanumeric(c) || c == ':' || c == '_' || c == '-' || c == '.'))
            return false;
    }

    return true;
}

bool Document::isValidName(const String& name)
{
    unsigned length = name.length();
    if (!length)
        return false;

    if (name.is8Bit()) {
        const LChar* characters = name.characters8();

        if (isValidNameASCII(characters, length))
            return true;

        return isValidNameNonASCII(characters, length);
    }

    const UChar* characters = name.characters16();

    if (isValidNameASCII(characters, length))
        return true;

    return isValidNameNonASCII(characters, length);
}

enum QualifiedNameStatus {
    QNValid,
    QNMultipleColons,
    QNInvalidStartChar,
    QNInvalidChar,
    QNEmptyPrefix,
    QNEmptyLocalName
};

struct ParseQualifiedNameResult {
    QualifiedNameStatus status;
    UChar32 character;
    ParseQualifiedNameResult() { }
    explicit ParseQualifiedNameResult(QualifiedNameStatus status) : status(status) { }
    ParseQualifiedNameResult(QualifiedNameStatus status, UChar32 character) : status(status), character(character) { }
};

template<typename CharType>
static ParseQualifiedNameResult parseQualifiedNameInternal(const AtomicString& qualifiedName, const CharType* characters, unsigned length, AtomicString& prefix, AtomicString& localName)
{
    bool nameStart = true;
    bool sawColon = false;
    int colonPos = 0;

    for (unsigned i = 0; i < length;) {
        UChar32 c;
        U16_NEXT(characters, i, length, c)
        if (c == ':') {
            if (sawColon)
                return ParseQualifiedNameResult(QNMultipleColons);
            nameStart = true;
            sawColon = true;
            colonPos = i - 1;
        } else if (nameStart) {
            if (!isValidNameStart(c))
                return ParseQualifiedNameResult(QNInvalidStartChar, c);
            nameStart = false;
        } else {
            if (!isValidNamePart(c))
                return ParseQualifiedNameResult(QNInvalidChar, c);
        }
    }

    if (!sawColon) {
        prefix = nullAtom;
        localName = qualifiedName;
    } else {
        prefix = AtomicString(characters, colonPos);
        if (prefix.isEmpty())
            return ParseQualifiedNameResult(QNEmptyPrefix);
        int prefixStart = colonPos + 1;
        localName = AtomicString(characters + prefixStart, length - prefixStart);
    }

    if (localName.isEmpty())
        return ParseQualifiedNameResult(QNEmptyLocalName);

    return ParseQualifiedNameResult(QNValid);
}

bool Document::parseQualifiedName(const AtomicString& qualifiedName, AtomicString& prefix, AtomicString& localName, ExceptionState& exceptionState)
{
    unsigned length = qualifiedName.length();

    if (!length) {
        exceptionState.throwDOMException(InvalidCharacterError, "The qualified name provided is empty.");
        return false;
    }

    ParseQualifiedNameResult returnValue;
    if (qualifiedName.is8Bit())
        returnValue = parseQualifiedNameInternal(qualifiedName, qualifiedName.characters8(), length, prefix, localName);
    else
        returnValue = parseQualifiedNameInternal(qualifiedName, qualifiedName.characters16(), length, prefix, localName);
    if (returnValue.status == QNValid)
        return true;

    StringBuilder message;
    message.append("The qualified name provided ('");
    message.append(qualifiedName);
    message.append("') ");

    if (returnValue.status == QNMultipleColons) {
        message.append("contains multiple colons.");
    } else if (returnValue.status == QNInvalidStartChar) {
        message.append("contains the invalid name-start character '");
        message.append(returnValue.character);
        message.append("'.");
    } else if (returnValue.status == QNInvalidChar) {
        message.append("contains the invalid character '");
        message.append(returnValue.character);
        message.append("'.");
    } else if (returnValue.status == QNEmptyPrefix) {
        message.append("has an empty namespace prefix.");
    } else {
        DCHECK_EQ(returnValue.status, QNEmptyLocalName);
        message.append("has an empty local name.");
    }

    if (returnValue.status == QNInvalidStartChar || returnValue.status == QNInvalidChar)
        exceptionState.throwDOMException(InvalidCharacterError, message.toString());
    else
        exceptionState.throwDOMException(NamespaceError, message.toString());
    return false;
}

void Document::setEncodingData(const DocumentEncodingData& newData)
{
    // It's possible for the encoding of the document to change while we're decoding
    // data. That can only occur while we're processing the <head> portion of the
    // document. There isn't much user-visible content in the <head>, but there is
    // the <title> element. This function detects that situation and re-decodes the
    // document's title so that the user doesn't see an incorrectly decoded title
    // in the title bar.
    if (m_titleElement
        && encoding() != newData.encoding()
        && !ElementTraversal::firstWithin(*m_titleElement)
        && encoding() == Latin1Encoding()
        && m_titleElement->textContent().containsOnlyLatin1()) {

        CString originalBytes = m_titleElement->textContent().latin1();
        std::unique_ptr<TextCodec> codec = newTextCodec(newData.encoding());
        String correctlyDecodedTitle = codec->decode(originalBytes.data(), originalBytes.length(), DataEOF);
        m_titleElement->setTextContent(correctlyDecodedTitle);
    }

    DCHECK(newData.encoding().isValid());
    m_encodingData = newData;

    // FIXME: Should be removed as part of https://code.google.com/p/chromium/issues/detail?id=319643
    bool shouldUseVisualOrdering = m_encodingData.encoding().usesVisualOrdering();
    if (shouldUseVisualOrdering != m_visuallyOrdered) {
        m_visuallyOrdered = shouldUseVisualOrdering;
        // FIXME: How is possible to not have a layoutObject here?
        if (layoutView())
            layoutView()->mutableStyleRef().setRTLOrdering(m_visuallyOrdered ? VisualOrder : LogicalOrder);
        setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::VisuallyOrdered));
    }
}

KURL Document::completeURL(const String& url) const
{
    return completeURLWithOverride(url, m_baseURL);
}

KURL Document::completeURLWithOverride(const String& url, const KURL& baseURLOverride) const
{
    DCHECK(baseURLOverride.isEmpty() || baseURLOverride.isValid());

    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    // See also [CSS]StyleSheet::completeURL(const String&)
    if (url.isNull())
        return KURL();
    // This logic is deliberately spread over many statements in an attempt to track down http://crbug.com/312410.
    const KURL& baseURL = baseURLForOverride(baseURLOverride);
    if (!encoding().isValid())
        return KURL(baseURL, url);
    return KURL(baseURL, url, encoding());
}

const KURL& Document::baseURLForOverride(const KURL& baseURLOverride) const
{
    // This logic is deliberately spread over many statements in an attempt to track down http://crbug.com/312410.
    const KURL* baseURLFromParent = 0;
    bool shouldUseParentBaseURL = baseURLOverride.isEmpty();
    if (!shouldUseParentBaseURL) {
        const KURL& aboutBlankURL = blankURL();
        shouldUseParentBaseURL = (baseURLOverride == aboutBlankURL);
    }
    if (shouldUseParentBaseURL) {
        if (Document* parent = parentDocument())
            baseURLFromParent = &parent->baseURL();
    }
    return baseURLFromParent ? *baseURLFromParent : baseURLOverride;
}

// Support for Javascript execCommand, and related methods

static Editor::Command command(Document* document, const String& commandName)
{
    LocalFrame* frame = document->frame();
    if (!frame || frame->document() != document)
        return Editor::Command();

    document->updateStyleAndLayoutTree();
    return frame->editor().createCommand(commandName, CommandFromDOM);
}

bool Document::execCommand(const String& commandName, bool, const String& value, ExceptionState& exceptionState)
{
    if (!isHTMLDocument() && !isXHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "execCommand is only supported on HTML documents.");
        return false;
    }
    if (focusedElement() && isHTMLTextFormControlElement(*focusedElement()))
        UseCounter::count(*this, UseCounter::ExecCommandOnInputOrTextarea);

    // We don't allow recursive |execCommand()| to protect against attack code.
    // Recursive call of |execCommand()| could be happened by moving iframe
    // with script triggered by insertion, e.g. <iframe src="javascript:...">
    // <iframe onload="...">. This usage is valid as of the specification
    // although, it isn't common use case, rather it is used as attack code.
    if (m_isRunningExecCommand) {
        String message = "We don't execute document.execCommand() this time, because it is called recursively.";
        addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
        return false;
    }
    TemporaryChange<bool> executeScope(m_isRunningExecCommand, true);

    // Postpone DOM mutation events, which can execute scripts and change
    // DOM tree against implementation assumption.
    EventQueueScope eventQueueScope;
    Editor::tidyUpHTMLStructure(*this);
    Editor::Command editorCommand = command(this, commandName);

    DEFINE_STATIC_LOCAL(SparseHistogram, editorCommandHistogram, ("WebCore.Document.execCommand"));
    editorCommandHistogram.sample(editorCommand.idForHistogram());
    return editorCommand.execute(value);
}

bool Document::queryCommandEnabled(const String& commandName, ExceptionState& exceptionState)
{
    if (!isHTMLDocument() && !isXHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "queryCommandEnabled is only supported on HTML documents.");
        return false;
    }

    return command(this, commandName).isEnabled();
}

bool Document::queryCommandIndeterm(const String& commandName, ExceptionState& exceptionState)
{
    if (!isHTMLDocument() && !isXHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "queryCommandIndeterm is only supported on HTML documents.");
        return false;
    }

    return command(this, commandName).state() == MixedTriState;
}

bool Document::queryCommandState(const String& commandName, ExceptionState& exceptionState)
{
    if (!isHTMLDocument() && !isXHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "queryCommandState is only supported on HTML documents.");
        return false;
    }

    return command(this, commandName).state() == TrueTriState;
}

bool Document::queryCommandSupported(const String& commandName, ExceptionState& exceptionState)
{
    if (!isHTMLDocument() && !isXHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "queryCommandSupported is only supported on HTML documents.");
        return false;
    }

    return command(this, commandName).isSupported();
}

String Document::queryCommandValue(const String& commandName, ExceptionState& exceptionState)
{
    if (!isHTMLDocument() && !isXHTMLDocument()) {
        exceptionState.throwDOMException(InvalidStateError, "queryCommandValue is only supported on HTML documents.");
        return "";
    }

    return command(this, commandName).value();
}

KURL Document::openSearchDescriptionURL()
{
    static const char openSearchMIMEType[] = "application/opensearchdescription+xml";
    static const char openSearchRelation[] = "search";

    // FIXME: Why do only top-level frames have openSearchDescriptionURLs?
    if (!frame() || frame()->tree().parent())
        return KURL();

    // FIXME: Why do we need to wait for load completion?
    if (!loadEventFinished())
        return KURL();

    if (!head())
        return KURL();

    for (HTMLLinkElement* linkElement = Traversal<HTMLLinkElement>::firstChild(*head()); linkElement; linkElement = Traversal<HTMLLinkElement>::nextSibling(*linkElement)) {
        if (!equalIgnoringCase(linkElement->type(), openSearchMIMEType) || !equalIgnoringCase(linkElement->rel(), openSearchRelation))
            continue;
        if (linkElement->href().isEmpty())
            continue;

        // Count usage; perhaps we can lock this to secure contexts.
        UseCounter::Feature osdDisposition;
        RefPtr<SecurityOrigin> target = SecurityOrigin::create(linkElement->href());
        if (isSecureContext()) {
            osdDisposition = target->isPotentiallyTrustworthy()
                ? UseCounter::OpenSearchSecureOriginSecureTarget
                : UseCounter::OpenSearchSecureOriginInsecureTarget;
        } else {
            osdDisposition = target->isPotentiallyTrustworthy()
                ? UseCounter::OpenSearchInsecureOriginSecureTarget
                : UseCounter::OpenSearchInsecureOriginInsecureTarget;
        }
        UseCounter::count(*this, osdDisposition);

        return linkElement->href();
    }

    return KURL();
}

void Document::currentScriptForBinding(HTMLScriptElementOrSVGScriptElement& scriptElement) const
{
    if (Element* script = currentScript()) {
        if (script->isInV1ShadowTree())
            return;
        if (isHTMLScriptElement(script))
            scriptElement.setHTMLScriptElement(toHTMLScriptElement(script));
        else if (isSVGScriptElement(script))
            scriptElement.setSVGScriptElement(toSVGScriptElement(script));
    }
}

void Document::pushCurrentScript(Element* newCurrentScript)
{
    DCHECK(isHTMLScriptElement(newCurrentScript) || isSVGScriptElement(newCurrentScript));
    m_currentScriptStack.append(newCurrentScript);
}

void Document::popCurrentScript()
{
    DCHECK(!m_currentScriptStack.isEmpty());
    m_currentScriptStack.removeLast();
}

void Document::setTransformSource(std::unique_ptr<TransformSource> source)
{
    m_transformSource = std::move(source);
}

String Document::designMode() const
{
    return inDesignMode() ? "on" : "off";
}

void Document::setDesignMode(const String& value)
{
    bool newValue = m_designMode;
    if (equalIgnoringCase(value, "on")) {
        newValue = true;
        UseCounter::count(*this, UseCounter::DocumentDesignModeEnabeld);
    } else if (equalIgnoringCase(value, "off")) {
        newValue = false;
    }
    if (newValue == m_designMode)
        return;
    m_designMode = newValue;
    setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::DesignMode));
}

Document* Document::parentDocument() const
{
    if (!m_frame)
        return 0;
    Frame* parent = m_frame->tree().parent();
    if (!parent || !parent->isLocalFrame())
        return 0;
    return toLocalFrame(parent)->document();
}

Document& Document::topDocument() const
{
    // FIXME: Not clear what topDocument() should do in the OOPI case--should it return the topmost
    // available Document, or something else?
    Document* doc = const_cast<Document*>(this);
    for (HTMLFrameOwnerElement* element = doc->localOwner(); element; element = doc->localOwner())
        doc = &element->document();

    DCHECK(doc);
    return *doc;
}

Document* Document::contextDocument()
{
    if (m_contextDocument)
        return m_contextDocument;
    if (m_frame)
        return this;
    return nullptr;
}

Attr* Document::createAttribute(const AtomicString& name, ExceptionState& exceptionState)
{
    if (isHTMLDocument() && name != name.lower())
        UseCounter::count(*this, UseCounter::HTMLDocumentCreateAttributeNameNotLowercase);
    return createAttributeNS(nullAtom, name, exceptionState, true);
}

Attr* Document::createAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionState& exceptionState, bool shouldIgnoreNamespaceChecks)
{
    AtomicString prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName, exceptionState))
        return nullptr;

    QualifiedName qName(prefix, localName, namespaceURI);

    if (!shouldIgnoreNamespaceChecks && !hasValidNamespaceForAttributes(qName)) {
        exceptionState.throwDOMException(NamespaceError, "The namespace URI provided ('" + namespaceURI + "') is not valid for the qualified name provided ('" + qualifiedName + "').");
        return nullptr;
    }

    return Attr::create(*this, qName, emptyAtom);
}

const SVGDocumentExtensions* Document::svgExtensions()
{
    return m_svgExtensions.get();
}

SVGDocumentExtensions& Document::accessSVGExtensions()
{
    if (!m_svgExtensions)
        m_svgExtensions = new SVGDocumentExtensions(this);
    return *m_svgExtensions;
}

bool Document::hasSVGRootNode() const
{
    return isSVGSVGElement(documentElement());
}

HTMLCollection* Document::images()
{
    return ensureCachedCollection<HTMLCollection>(DocImages);
}

HTMLCollection* Document::applets()
{
    return ensureCachedCollection<HTMLCollection>(DocApplets);
}

HTMLCollection* Document::embeds()
{
    return ensureCachedCollection<HTMLCollection>(DocEmbeds);
}

HTMLCollection* Document::scripts()
{
    return ensureCachedCollection<HTMLCollection>(DocScripts);
}

HTMLCollection* Document::links()
{
    return ensureCachedCollection<HTMLCollection>(DocLinks);
}

HTMLCollection* Document::forms()
{
    return ensureCachedCollection<HTMLCollection>(DocForms);
}

HTMLCollection* Document::anchors()
{
    return ensureCachedCollection<HTMLCollection>(DocAnchors);
}

HTMLAllCollection* Document::allForBinding()
{
    UseCounter::count(*this, UseCounter::DocumentAll);
    return all();
}

HTMLAllCollection* Document::all()
{
    return ensureCachedCollection<HTMLAllCollection>(DocAll);
}

HTMLCollection* Document::windowNamedItems(const AtomicString& name)
{
    return ensureCachedCollection<WindowNameCollection>(WindowNamedItems, name);
}

DocumentNameCollection* Document::documentNamedItems(const AtomicString& name)
{
    return ensureCachedCollection<DocumentNameCollection>(DocumentNamedItems, name);
}

void Document::finishedParsing()
{
    DCHECK(!scriptableDocumentParser() || !m_parser->isParsing());
    DCHECK(!scriptableDocumentParser() || m_readyState != Loading);
    setParsingState(InDOMContentLoaded);
    DocumentParserTiming::from(*this).markParserStop();

    // FIXME: DOMContentLoaded is dispatched synchronously, but this should be dispatched in a queued task,
    // See https://crbug.com/425790
    if (!m_documentTiming.domContentLoadedEventStart())
        m_documentTiming.markDomContentLoadedEventStart();
    dispatchEvent(Event::createBubble(EventTypeNames::DOMContentLoaded));
    if (!m_documentTiming.domContentLoadedEventEnd())
        m_documentTiming.markDomContentLoadedEventEnd();
    setParsingState(FinishedParsing);

    // Ensure Custom Element callbacks are drained before DOMContentLoaded.
    // FIXME: Remove this ad-hoc checkpoint when DOMContentLoaded is dispatched in a
    // queued task, which will do a checkpoint anyway. https://crbug.com/425790
    Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());

    if (LocalFrame* frame = this->frame()) {
        // Don't update the layout tree if we haven't requested the main resource yet to avoid
        // adding extra latency. Note that the first layout tree update can be expensive since it
        // triggers the parsing of the default stylesheets which are compiled-in.
        const bool mainResourceWasAlreadyRequested = frame->loader().stateMachine()->committedFirstRealDocumentLoad();

        // FrameLoader::finishedParsing() might end up calling Document::implicitClose() if all
        // resource loads are complete. HTMLObjectElements can start loading their resources from
        // post attach callbacks triggered by recalcStyle().  This means if we parse out an <object>
        // tag and then reach the end of the document without updating styles, we might not have yet
        // started the resource load and might fire the window load event too early.  To avoid this
        // we force the styles to be up to date before calling FrameLoader::finishedParsing().
        // See https://bugs.webkit.org/show_bug.cgi?id=36864 starting around comment 35.
        if (mainResourceWasAlreadyRequested)
            updateStyleAndLayoutTree();

        beginLifecycleUpdatesIfRenderingReady();

        frame->loader().finishedParsing();

        TRACE_EVENT_INSTANT1("devtools.timeline", "MarkDOMContent", TRACE_EVENT_SCOPE_THREAD, "data", InspectorMarkLoadEvent::data(frame));
        InspectorInstrumentation::domContentLoadedEventFired(frame);
    }

    // Schedule dropping of the ElementDataCache. We keep it alive for a while after parsing finishes
    // so that dynamically inserted content can also benefit from sharing optimizations.
    // Note that we don't refresh the timer on cache access since that could lead to huge caches being kept
    // alive indefinitely by something innocuous like JS setting .innerHTML repeatedly on a timer.
    m_elementDataCacheClearTimer.startOneShot(10, BLINK_FROM_HERE);

    // Parser should have picked up all preloads by now
    m_fetcher->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);
}

void Document::elementDataCacheClearTimerFired(Timer<Document>*)
{
    m_elementDataCache.clear();
}

void Document::beginLifecycleUpdatesIfRenderingReady()
{
    if (!isActive())
        return;
    if (!isRenderingReady())
        return;
    if (LocalFrame* frame = this->frame()) {
        // Avoid pumping frames for the initially empty document.
        if (!frame->loader().stateMachine()->committedFirstRealDocumentLoad())
            return;
        // The compositor will "defer commits" for the main frame until we
        // explicitly request them.
        if (frame->isMainFrame())
            frame->page()->chromeClient().beginLifecycleUpdates();
    }
}

Vector<IconURL> Document::iconURLs(int iconTypesMask)
{
    IconURL firstFavicon;
    IconURL firstTouchIcon;
    IconURL firstTouchPrecomposedIcon;
    Vector<IconURL> secondaryIcons;

    // Start from the last child node so that icons seen later take precedence as required by the spec.
    for (HTMLLinkElement* linkElement = head() ? Traversal<HTMLLinkElement>::firstChild(*head()) : 0; linkElement; linkElement = Traversal<HTMLLinkElement>::nextSibling(*linkElement)) {
        if (!(linkElement->getIconType() & iconTypesMask))
            continue;
        if (linkElement->href().isEmpty())
            continue;

        IconURL newURL(linkElement->href(), linkElement->iconSizes(), linkElement->type(), linkElement->getIconType());
        if (linkElement->getIconType() == Favicon) {
            if (firstFavicon.m_iconType != InvalidIcon)
                secondaryIcons.append(firstFavicon);
            firstFavicon = newURL;
        } else if (linkElement->getIconType() == TouchIcon) {
            if (firstTouchIcon.m_iconType != InvalidIcon)
                secondaryIcons.append(firstTouchIcon);
            firstTouchIcon = newURL;
        } else if (linkElement->getIconType() == TouchPrecomposedIcon) {
            if (firstTouchPrecomposedIcon.m_iconType != InvalidIcon)
                secondaryIcons.append(firstTouchPrecomposedIcon);
            firstTouchPrecomposedIcon = newURL;
        } else {
            ASSERT_NOT_REACHED();
        }
    }

    Vector<IconURL> iconURLs;
    if (firstFavicon.m_iconType != InvalidIcon)
        iconURLs.append(firstFavicon);
    else if (m_url.protocolIsInHTTPFamily() && iconTypesMask & Favicon)
        iconURLs.append(IconURL::defaultFavicon(m_url));

    if (firstTouchIcon.m_iconType != InvalidIcon)
        iconURLs.append(firstTouchIcon);
    if (firstTouchPrecomposedIcon.m_iconType != InvalidIcon)
        iconURLs.append(firstTouchPrecomposedIcon);
    for (int i = secondaryIcons.size() - 1; i >= 0; --i)
        iconURLs.append(secondaryIcons[i]);
    return iconURLs;
}

Color Document::themeColor() const
{
    for (HTMLMetaElement* metaElement = head() ? Traversal<HTMLMetaElement>::firstChild(*head()) : 0; metaElement; metaElement = Traversal<HTMLMetaElement>::nextSibling(*metaElement)) {
        Color color = Color::transparent;
        if (equalIgnoringCase(metaElement->name(), "theme-color") && CSSParser::parseColor(color, metaElement->content().getString().stripWhiteSpace(), true))
            return color;
    }
    return Color();
}

HTMLLinkElement* Document::linkManifest() const
{
    HTMLHeadElement* head = this->head();
    if (!head)
        return 0;

    // The first link element with a manifest rel must be used. Others are ignored.
    for (HTMLLinkElement* linkElement = Traversal<HTMLLinkElement>::firstChild(*head); linkElement; linkElement = Traversal<HTMLLinkElement>::nextSibling(*linkElement)) {
        if (!linkElement->relAttribute().isManifest())
            continue;
        return linkElement;
    }

    return 0;
}

void Document::setUseSecureKeyboardEntryWhenActive(bool usesSecureKeyboard)
{
    if (m_useSecureKeyboardEntryWhenActive == usesSecureKeyboard)
        return;

    m_useSecureKeyboardEntryWhenActive = usesSecureKeyboard;
    m_frame->selection().updateSecureKeyboardEntryIfActive();
}

bool Document::useSecureKeyboardEntryWhenActive() const
{
    return m_useSecureKeyboardEntryWhenActive;
}

void Document::initSecurityContext(const DocumentInit& initializer)
{
    DCHECK(!getSecurityOrigin());

    if (!initializer.hasSecurityContext()) {
        // No source for a security context.
        // This can occur via document.implementation.createDocument().
        m_cookieURL = KURL(ParsedURLString, emptyString());
        setSecurityOrigin(SecurityOrigin::createUnique());
        initContentSecurityPolicy();
        // Unique security origins cannot have a suborigin
        return;
    }

    // In the common case, create the security context from the currently
    // loading URL with a fresh content security policy.
    enforceSandboxFlags(initializer.getSandboxFlags());
    setInsecureRequestPolicy(initializer.getInsecureRequestPolicy());
    if (initializer.insecureNavigationsToUpgrade()) {
        for (auto toUpgrade : *initializer.insecureNavigationsToUpgrade())
            addInsecureNavigationUpgrade(toUpgrade);
    }

    if (isSandboxed(SandboxOrigin)) {
        m_cookieURL = m_url;
        setSecurityOrigin(SecurityOrigin::createUnique());
        // If we're supposed to inherit our security origin from our
        // owner, but we're also sandboxed, the only things we inherit are
        // the origin's potential trustworthiness and the ability to
        // load local resources. The latter lets about:blank iframes in
        // file:// URL documents load images and other resources from
        // the file system.
        if (initializer.owner() && initializer.owner()->getSecurityOrigin()->isPotentiallyTrustworthy())
            getSecurityOrigin()->setUniqueOriginIsPotentiallyTrustworthy(true);
        if (initializer.owner() && initializer.owner()->getSecurityOrigin()->canLoadLocalResources())
            getSecurityOrigin()->grantLoadLocalResources();
    } else if (initializer.owner()) {
        m_cookieURL = initializer.owner()->cookieURL();
        // We alias the SecurityOrigins to match Firefox, see Bug 15313
        // https://bugs.webkit.org/show_bug.cgi?id=15313
        setSecurityOrigin(initializer.owner()->getSecurityOrigin());
    } else {
        m_cookieURL = m_url;
        setSecurityOrigin(SecurityOrigin::create(m_url));
    }

    // Set the address space before setting up CSP, as the latter may override
    // the former via the 'treat-as-public-address' directive (see
    // https://mikewest.github.io/cors-rfc1918/#csp).
    if (initializer.isHostedInReservedIPRange()) {
        setAddressSpace(getSecurityOrigin()->isLocalhost() ? WebAddressSpaceLocal : WebAddressSpacePrivate);
    } else if (getSecurityOrigin()->isLocal()) {
        // "Local" security origins (like 'file://...') are treated as having
        // a local address space.
        //
        // TODO(mkwst): It's not entirely clear that this is a good idea.
        setAddressSpace(WebAddressSpaceLocal);
    } else {
        setAddressSpace(WebAddressSpacePublic);
    }

    if (importsController()) {
        // If this document is an HTML import, grab a reference to it's master document's Content
        // Security Policy. We don't call 'initContentSecurityPolicy' in this case, as we can't
        // rebind the master document's policy object: its ExecutionContext needs to remain tied
        // to the master document.
        setContentSecurityPolicy(importsController()->master()->contentSecurityPolicy());
    } else {
        initContentSecurityPolicy();
    }

    if (getSecurityOrigin()->hasSuborigin())
        enforceSuborigin(*getSecurityOrigin()->suborigin());

    if (Settings* settings = initializer.settings()) {
        if (!settings->webSecurityEnabled()) {
            // Web security is turned off. We should let this document access every other document. This is used primary by testing
            // harnesses for web sites.
            getSecurityOrigin()->grantUniversalAccess();
        } else if (getSecurityOrigin()->isLocal()) {
            if (settings->allowUniversalAccessFromFileURLs()) {
                // Some clients want local URLs to have universal access, but that setting is dangerous for other clients.
                getSecurityOrigin()->grantUniversalAccess();
            } else if (!settings->allowFileAccessFromFileURLs()) {
                // Some clients do not want local URLs to have access to other local URLs.
                getSecurityOrigin()->blockLocalAccessFromLocalOrigin();
            }
        }
    }

    if (initializer.shouldTreatURLAsSrcdocDocument()) {
        m_isSrcdocDocument = true;
        setBaseURLOverride(initializer.parentBaseURL());
    }

    if (getSecurityOrigin()->isUnique() && SecurityOrigin::create(m_url)->isPotentiallyTrustworthy())
        getSecurityOrigin()->setUniqueOriginIsPotentiallyTrustworthy(true);

    if (getSecurityOrigin()->hasSuborigin())
        enforceSuborigin(*getSecurityOrigin()->suborigin());
}

void Document::initContentSecurityPolicy(ContentSecurityPolicy* csp)
{
    setContentSecurityPolicy(csp ? csp : ContentSecurityPolicy::create());
    if (m_frame && m_frame->tree().parent() && m_frame->tree().parent()->isLocalFrame()) {
        ContentSecurityPolicy* parentCSP = toLocalFrame(m_frame->tree().parent())->document()->contentSecurityPolicy();
        if (shouldInheritSecurityOriginFromOwner(m_url)) {
            contentSecurityPolicy()->copyStateFrom(parentCSP);
        } else if (isPluginDocument()) {
            // Per CSP2, plugin-types for plugin documents in nested browsing
            // contexts gets inherited from the parent.
            contentSecurityPolicy()->copyPluginTypesFrom(parentCSP);
        }
    }
    contentSecurityPolicy()->bindToExecutionContext(this);
}

bool Document::isSecureTransitionTo(const KURL& url) const
{
    RefPtr<SecurityOrigin> other = SecurityOrigin::create(url);
    return getSecurityOrigin()->canAccess(other.get());
}

bool Document::allowInlineEventHandler(Node* node, EventListener* listener, const String& contextURL, const WTF::OrdinalNumber& contextLine)
{
    if (!ContentSecurityPolicy::shouldBypassMainWorld(this) && !contentSecurityPolicy()->allowInlineEventHandler(listener->code(), contextURL, contextLine))
        return false;

    // HTML says that inline script needs browsing context to create its execution environment.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#event-handler-attributes
    // Also, if the listening node came from other document, which happens on context-less event dispatching,
    // we also need to ask the owner document of the node.
    LocalFrame* frame = executingFrame();
    if (!frame)
        return false;
    if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return false;
    if (node && node->document() != this && !node->document().allowInlineEventHandler(node, listener, contextURL, contextLine))
        return false;

    return true;
}

bool Document::allowExecutingScripts(Node* node)
{
    // FIXME: Eventually we'd like to evaluate scripts which are inserted into a
    // viewless document but this'll do for now.
    // See http://bugs.webkit.org/show_bug.cgi?id=5727
    LocalFrame* frame = executingFrame();
    if (!frame)
        return false;
    if (!node->document().executingFrame())
        return false;
    if (!frame->script().canExecuteScripts(AboutToExecuteScript))
        return false;
    return true;
}

void Document::enforceSandboxFlags(SandboxFlags mask)
{
    RefPtr<SecurityOrigin> standInOrigin = getSecurityOrigin();
    applySandboxFlags(mask);
    // Send a notification if the origin has been updated.
    if (standInOrigin && !standInOrigin->isUnique() && getSecurityOrigin()->isUnique()) {
        getSecurityOrigin()->setUniqueOriginIsPotentiallyTrustworthy(standInOrigin->isPotentiallyTrustworthy());
        if (frame())
            frame()->loader().client()->didUpdateToUniqueOrigin();
    }
}

void Document::updateSecurityOrigin(PassRefPtr<SecurityOrigin> origin)
{
    setSecurityOrigin(origin);
    didUpdateSecurityOrigin();
}

void Document::didUpdateSecurityOrigin()
{
    if (!m_frame)
        return;
    m_frame->script().updateSecurityOrigin(getSecurityOrigin());
}

bool Document::isContextThread() const
{
    return isMainThread();
}

void Document::updateFocusAppearanceSoon(SelectionBehaviorOnFocus selectionbehavioronfocus)
{
    m_updateFocusAppearanceSelectionBahavior = selectionbehavioronfocus;
    if (!m_updateFocusAppearanceTimer.isActive())
        m_updateFocusAppearanceTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Document::cancelFocusAppearanceUpdate()
{
    m_updateFocusAppearanceTimer.stop();
}

void Document::updateFocusAppearanceTimerFired(Timer<Document>*)
{
    Element* element = focusedElement();
    if (!element)
        return;
    updateStyleAndLayout();
    if (element->isFocusable())
        element->updateFocusAppearance(m_updateFocusAppearanceSelectionBahavior);
}

void Document::attachRange(Range* range)
{
    DCHECK(!m_ranges.contains(range));
    m_ranges.add(range);
}

void Document::detachRange(Range* range)
{
    // We don't ASSERT m_ranges.contains(range) to allow us to call this
    // unconditionally to fix: https://bugs.webkit.org/show_bug.cgi?id=26044
    m_ranges.remove(range);
}

void Document::initDNSPrefetch()
{
    Settings* settings = this->settings();

    m_haveExplicitlyDisabledDNSPrefetch = false;
    m_isDNSPrefetchEnabled = settings && settings->dnsPrefetchingEnabled() && getSecurityOrigin()->protocol() == "http";

    // Inherit DNS prefetch opt-out from parent frame
    if (Document* parent = parentDocument()) {
        if (!parent->isDNSPrefetchEnabled())
            m_isDNSPrefetchEnabled = false;
    }
}

void Document::parseDNSPrefetchControlHeader(const String& dnsPrefetchControl)
{
    if (equalIgnoringCase(dnsPrefetchControl, "on") && !m_haveExplicitlyDisabledDNSPrefetch) {
        m_isDNSPrefetchEnabled = true;
        return;
    }

    m_isDNSPrefetchEnabled = false;
    m_haveExplicitlyDisabledDNSPrefetch = true;
}

IntersectionObserverController* Document::intersectionObserverController()
{
    return m_intersectionObserverController;
}

IntersectionObserverController& Document::ensureIntersectionObserverController()
{
    if (!m_intersectionObserverController)
        m_intersectionObserverController = IntersectionObserverController::create(this);
    return *m_intersectionObserverController;
}

NodeIntersectionObserverData& Document::ensureIntersectionObserverData()
{
    if (!m_intersectionObserverData)
        m_intersectionObserverData = new NodeIntersectionObserverData();
    return *m_intersectionObserverData;
}

void Document::reportBlockedScriptExecutionToInspector(const String& directiveText)
{
    InspectorInstrumentation::scriptExecutionBlockedByCSP(this, directiveText);
}

static void runAddConsoleMessageTask(MessageSource source, MessageLevel level, const String& message, ExecutionContext* context)
{
    context->addConsoleMessage(ConsoleMessage::create(source, level, message));
}

void Document::addConsoleMessage(ConsoleMessage* consoleMessage)
{
    if (!isContextThread()) {
        m_taskRunner->postTask(BLINK_FROM_HERE, createCrossThreadTask(&runAddConsoleMessageTask, consoleMessage->source(), consoleMessage->level(), consoleMessage->message()));
        return;
    }

    if (!m_frame)
        return;

    if (consoleMessage->location()->isUnknown()) {
        // TODO(dgozman): capture correct location at call places instead.
        unsigned lineNumber = 0;
        if (!isInDocumentWrite() && scriptableDocumentParser()) {
            ScriptableDocumentParser* parser = scriptableDocumentParser();
            if (parser->isParsingAtLineNumber())
                lineNumber = parser->lineNumber().oneBasedInt();
        }
        consoleMessage = ConsoleMessage::create(consoleMessage->source(), consoleMessage->level(), consoleMessage->message(), SourceLocation::create(url().getString(), lineNumber, 0, nullptr));
    }
    m_frame->console().addMessage(consoleMessage);
}

// FIXME(crbug.com/305497): This should be removed after ExecutionContext-LocalDOMWindow migration.
void Document::postTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task, const String& taskNameForInstrumentation)
{
    m_taskRunner->postTask(location, std::move(task), taskNameForInstrumentation);
}

void Document::postInspectorTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task)
{
    m_taskRunner->postInspectorTask(location, std::move(task));
}

void Document::tasksWereSuspended()
{
    scriptRunner()->suspend();

    if (m_parser)
        m_parser->suspendScheduledTasks();
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->suspend();
}

void Document::tasksWereResumed()
{
    scriptRunner()->resume();

    if (m_parser)
        m_parser->resumeScheduledTasks();
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->resume();

    MutationObserver::resumeSuspendedObservers();
    if (m_domWindow)
        DOMWindowPerformance::performance(*m_domWindow)->resumeSuspendedObservers();
}

// FIXME: suspendScheduledTasks(), resumeScheduledTasks(), tasksNeedSuspension()
// should be moved to LocalDOMWindow once it inherits ExecutionContext
void Document::suspendScheduledTasks()
{
    ExecutionContext::suspendScheduledTasks();
    m_taskRunner->suspend();
}

void Document::resumeScheduledTasks()
{
    ExecutionContext::resumeScheduledTasks();
    m_taskRunner->resume();
}

bool Document::tasksNeedSuspension()
{
    Page* page = this->page();
    return page && page->defersLoading();
}

void Document::addToTopLayer(Element* element, const Element* before)
{
    if (element->isInTopLayer())
        return;

    DCHECK(!m_topLayerElements.contains(element));
    DCHECK(!before || m_topLayerElements.contains(before));
    if (before) {
        size_t beforePosition = m_topLayerElements.find(before);
        m_topLayerElements.insert(beforePosition, element);
    } else {
        m_topLayerElements.append(element);
    }
    element->setIsInTopLayer(true);
}

void Document::removeFromTopLayer(Element* element)
{
    if (!element->isInTopLayer())
        return;
    size_t position = m_topLayerElements.find(element);
    DCHECK_NE(position, kNotFound);
    m_topLayerElements.remove(position);
    element->setIsInTopLayer(false);
}

HTMLDialogElement* Document::activeModalDialog() const
{
    if (m_topLayerElements.isEmpty())
        return 0;
    return toHTMLDialogElement(m_topLayerElements.last().get());
}

void Document::exitPointerLock()
{
    if (!page())
        return;
    if (Element* target = page()->pointerLockController().element()) {
        if (target->document() != this)
            return;
        page()->pointerLockController().requestPointerUnlock();
    }
}

Element* Document::pointerLockElement() const
{
    if (!page() || page()->pointerLockController().lockPending())
        return 0;
    if (Element* element = page()->pointerLockController().element()) {
        if (element->document() == this)
            return element;
    }
    return 0;
}

void Document::suppressLoadEvent()
{
    if (!loadEventFinished())
        m_loadEventProgress = LoadEventCompleted;
}

void Document::decrementLoadEventDelayCount()
{
    DCHECK(m_loadEventDelayCount);
    --m_loadEventDelayCount;

    if (!m_loadEventDelayCount)
        checkLoadEventSoon();
}

void Document::checkLoadEventSoon()
{
    if (frame() && !m_loadEventDelayTimer.isActive())
        m_loadEventDelayTimer.startOneShot(0, BLINK_FROM_HERE);
}

bool Document::isDelayingLoadEvent()
{
    // Always delay load events until after garbage collection.
    // This way we don't have to explicitly delay load events via
    // incrementLoadEventDelayCount and decrementLoadEventDelayCount in
    // Node destructors.
    if (ThreadState::current()->sweepForbidden()) {
        if (!m_loadEventDelayCount)
            checkLoadEventSoon();
        return true;
    }
    return m_loadEventDelayCount;
}

void Document::loadEventDelayTimerFired(Timer<Document>*)
{
    if (frame())
        frame()->loader().checkCompleted();
}

void Document::loadPluginsSoon()
{
    // FIXME: Remove this timer once we don't need to compute layout to load plugins.
    if (!m_pluginLoadingTimer.isActive())
        m_pluginLoadingTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Document::pluginLoadingTimerFired(Timer<Document>*)
{
    updateStyleAndLayout();
}

ScriptedAnimationController& Document::ensureScriptedAnimationController()
{
    if (!m_scriptedAnimationController) {
        m_scriptedAnimationController = ScriptedAnimationController::create(this);
        // We need to make sure that we don't start up the animation controller on a background tab, for example.
        if (!page())
            m_scriptedAnimationController->suspend();
    }
    return *m_scriptedAnimationController;
}

int Document::requestAnimationFrame(FrameRequestCallback* callback)
{
    return ensureScriptedAnimationController().registerCallback(callback);
}

void Document::cancelAnimationFrame(int id)
{
    if (!m_scriptedAnimationController)
        return;
    m_scriptedAnimationController->cancelCallback(id);
}

void Document::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    if (!m_scriptedAnimationController)
        return;
    m_scriptedAnimationController->serviceScriptedAnimations(monotonicAnimationStartTime);
}

ScriptedIdleTaskController& Document::ensureScriptedIdleTaskController()
{
    if (!m_scriptedIdleTaskController)
        m_scriptedIdleTaskController = ScriptedIdleTaskController::create(this);
    return *m_scriptedIdleTaskController;
}

int Document::requestIdleCallback(IdleRequestCallback* callback, const IdleRequestOptions& options)
{
    return ensureScriptedIdleTaskController().registerCallback(callback, options);
}

void Document::cancelIdleCallback(int id)
{
    if (!m_scriptedIdleTaskController)
        return;
    m_scriptedIdleTaskController->cancelCallback(id);
}

Touch* Document::createTouch(DOMWindow* window, EventTarget* target, int identifier, double pageX, double pageY, double screenX, double screenY, double radiusX, double radiusY, float rotationAngle, float force) const
{
    // Match behavior from when these types were integers, and avoid surprises from someone explicitly
    // passing Infinity/NaN.
    if (!std::isfinite(pageX))
        pageX = 0;
    if (!std::isfinite(pageY))
        pageY = 0;
    if (!std::isfinite(screenX))
        screenX = 0;
    if (!std::isfinite(screenY))
        screenY = 0;
    if (!std::isfinite(radiusX))
        radiusX = 0;
    if (!std::isfinite(radiusY))
        radiusY = 0;
    if (!std::isfinite(rotationAngle))
        rotationAngle = 0;
    if (!std::isfinite(force))
        force = 0;

    // FIXME: It's not clear from the documentation at
    // http://developer.apple.com/library/safari/#documentation/UserExperience/Reference/DocumentAdditionsReference/DocumentAdditions/DocumentAdditions.html
    // when this method should throw and nor is it by inspection of iOS behavior. It would be nice to verify any cases where it throws under iOS
    // and implement them here. See https://bugs.webkit.org/show_bug.cgi?id=47819
    LocalFrame* frame = window && window->isLocalDOMWindow() ? blink::toLocalDOMWindow(window)->frame() : this->frame();
    return Touch::create(frame, target, identifier, FloatPoint(screenX, screenY), FloatPoint(pageX, pageY), FloatSize(radiusX, radiusY), rotationAngle, force, String());
}

TouchList* Document::createTouchList(HeapVector<Member<Touch>>& touches) const
{
    return TouchList::adopt(touches);
}

DocumentLoader* Document::loader() const
{
    if (!m_frame)
        return 0;

    DocumentLoader* loader = m_frame->loader().documentLoader();
    if (!loader)
        return 0;

    if (m_frame->document() != this)
        return 0;

    return loader;
}

Node* eventTargetNodeForDocument(Document* doc)
{
    if (!doc)
        return 0;
    Node* node = doc->focusedElement();
    if (!node && doc->isPluginDocument()) {
        PluginDocument* pluginDocument = toPluginDocument(doc);
        node = pluginDocument->pluginNode();
    }
    if (!node && doc->isHTMLDocument())
        node = doc->body();
    if (!node)
        node = doc->documentElement();
    return node;
}

void Document::adjustFloatQuadsForScrollAndAbsoluteZoom(Vector<FloatQuad>& quads, LayoutObject& layoutObject)
{
    if (!view())
        return;

    LayoutRect visibleContentRect(view()->visibleContentRect());
    for (size_t i = 0; i < quads.size(); ++i) {
        quads[i].move(-FloatSize(visibleContentRect.x().toFloat(), visibleContentRect.y().toFloat()));
        adjustFloatQuadForAbsoluteZoom(quads[i], layoutObject);
    }
}

void Document::adjustFloatRectForScrollAndAbsoluteZoom(FloatRect& rect, LayoutObject& layoutObject)
{
    if (!view())
        return;

    LayoutRect visibleContentRect(view()->visibleContentRect());
    rect.move(-FloatSize(visibleContentRect.x().toFloat(), visibleContentRect.y().toFloat()));
    adjustFloatRectForAbsoluteZoom(rect, layoutObject);
}

void Document::setThreadedParsingEnabledForTesting(bool enabled)
{
    s_threadedParsingEnabledForTesting = enabled;
}

bool Document::threadedParsingEnabledForTesting()
{
    return s_threadedParsingEnabledForTesting;
}

SnapCoordinator* Document::snapCoordinator()
{
    if (RuntimeEnabledFeatures::cssScrollSnapPointsEnabled() && !m_snapCoordinator)
        m_snapCoordinator = SnapCoordinator::create();

    return m_snapCoordinator.get();
}

void Document::setContextFeatures(ContextFeatures& features)
{
    m_contextFeatures = &features;
}

static LayoutObject* nearestCommonHoverAncestor(LayoutObject* obj1, LayoutObject* obj2)
{
    if (!obj1 || !obj2)
        return 0;

    for (LayoutObject* currObj1 = obj1; currObj1; currObj1 = currObj1->hoverAncestor()) {
        for (LayoutObject* currObj2 = obj2; currObj2; currObj2 = currObj2->hoverAncestor()) {
            if (currObj1 == currObj2)
                return currObj1;
        }
    }

    return 0;
}

void Document::updateHoverActiveState(const HitTestRequest& request, Element* innerElement)
{
    DCHECK(!request.readOnly());

    if (request.active() && m_frame)
        m_frame->eventHandler().notifyElementActivated();

    Element* innerElementInDocument = innerElement;
    while (innerElementInDocument && innerElementInDocument->document() != this) {
        innerElementInDocument->document().updateHoverActiveState(request, innerElementInDocument);
        innerElementInDocument = innerElementInDocument->document().localOwner();
    }

    updateDistribution();
    Element* oldActiveElement = activeHoverElement();
    if (oldActiveElement && !request.active()) {
        // The oldActiveElement layoutObject is null, dropped on :active by setting display: none,
        // for instance. We still need to clear the ActiveChain as the mouse is released.
        for (Node* node = oldActiveElement; node; node = FlatTreeTraversal::parent(*node)) {
            DCHECK(!node->isTextNode());
            node->setActive(false);
            m_userActionElements.setInActiveChain(node, false);
        }
        setActiveHoverElement(nullptr);
    } else {
        Element* newActiveElement = innerElementInDocument;
        if (!oldActiveElement && newActiveElement && !newActiveElement->isDisabledFormControl() && request.active() && !request.touchMove()) {
            // We are setting the :active chain and freezing it. If future moves happen, they
            // will need to reference this chain.
            for (Node* node = newActiveElement; node; node = FlatTreeTraversal::parent(*node)) {
                DCHECK(!node->isTextNode());
                m_userActionElements.setInActiveChain(node, true);
            }
            setActiveHoverElement(newActiveElement);
        }
    }
    // If the mouse has just been pressed, set :active on the chain. Those (and only those)
    // nodes should remain :active until the mouse is released.
    bool allowActiveChanges = !oldActiveElement && activeHoverElement();

    // If the mouse is down and if this is a mouse move event, we want to restrict changes in
    // :hover/:active to only apply to elements that are in the :active chain that we froze
    // at the time the mouse went down.
    bool mustBeInActiveChain = request.active() && request.move();

    Node* oldHoverNode = hoverNode();

    // Check to see if the hovered node has changed.
    // If it hasn't, we do not need to do anything.
    Node* newHoverNode = innerElementInDocument;
    while (newHoverNode && !newHoverNode->layoutObject())
        newHoverNode = newHoverNode->parentOrShadowHostNode();

    // Update our current hover node.
    setHoverNode(newHoverNode);

    // We have two different objects. Fetch their layoutObjects.
    LayoutObject* oldHoverObj = oldHoverNode ? oldHoverNode->layoutObject() : 0;
    LayoutObject* newHoverObj = newHoverNode ? newHoverNode->layoutObject() : 0;

    // Locate the common ancestor layout object for the two layoutObjects.
    LayoutObject* ancestor = nearestCommonHoverAncestor(oldHoverObj, newHoverObj);
    Node* ancestorNode(ancestor ? ancestor->node() : 0);

    HeapVector<Member<Node>, 32> nodesToRemoveFromChain;
    HeapVector<Member<Node>, 32> nodesToAddToChain;

    if (oldHoverObj != newHoverObj) {
        // If the old hovered node is not nil but it's layoutObject is, it was probably detached as part of the :hover style
        // (for instance by setting display:none in the :hover pseudo-class). In this case, the old hovered element (and its ancestors)
        // must be updated, to ensure it's normal style is re-applied.
        if (oldHoverNode && !oldHoverObj) {
            for (Node& node : NodeTraversal::inclusiveAncestorsOf(*oldHoverNode)) {
                if (!mustBeInActiveChain || (node.isElementNode() && toElement(node).inActiveChain()))
                    nodesToRemoveFromChain.append(node);
            }

        }

        // The old hover path only needs to be cleared up to (and not including) the common ancestor;
        for (LayoutObject* curr = oldHoverObj; curr && curr != ancestor; curr = curr->hoverAncestor()) {
            if (curr->node() && !curr->isText() && (!mustBeInActiveChain || curr->node()->inActiveChain()))
                nodesToRemoveFromChain.append(curr->node());
        }

        // TODO(mustaq): The two loops above may push a single node twice into nodesToRemoveFromChain. There must be a better way.
    }

    // Now set the hover state for our new object up to the root.
    for (LayoutObject* curr = newHoverObj; curr; curr = curr->hoverAncestor()) {
        if (curr->node() && !curr->isText() && (!mustBeInActiveChain || curr->node()->inActiveChain()))
            nodesToAddToChain.append(curr->node());
    }

    size_t removeCount = nodesToRemoveFromChain.size();
    for (size_t i = 0; i < removeCount; ++i) {
        nodesToRemoveFromChain[i]->setHovered(false);
    }

    bool sawCommonAncestor = false;
    size_t addCount = nodesToAddToChain.size();
    for (size_t i = 0; i < addCount; ++i) {
        // Elements past the common ancestor do not change hover state, but might change active state.
        if (ancestorNode && nodesToAddToChain[i] == ancestorNode)
            sawCommonAncestor = true;
        if (allowActiveChanges)
            nodesToAddToChain[i]->setActive(true);
        if (!sawCommonAncestor || nodesToAddToChain[i] == m_hoverNode) {
            nodesToAddToChain[i]->setHovered(true);
        }
    }
}

bool Document::haveScriptBlockingStylesheetsLoaded() const
{
    return m_styleEngine->haveScriptBlockingStylesheetsLoaded();
}

bool Document::haveRenderBlockingStylesheetsLoaded() const
{
    if (RuntimeEnabledFeatures::cssInBodyDoesNotBlockPaintEnabled())
        return m_styleEngine->haveRenderBlockingStylesheetsLoaded();
    return m_styleEngine->haveScriptBlockingStylesheetsLoaded();
}

Locale& Document::getCachedLocale(const AtomicString& locale)
{
    AtomicString localeKey = locale;
    if (locale.isEmpty() || !RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
        return Locale::defaultLocale();
    LocaleIdentifierToLocaleMap::AddResult result = m_localeCache.add(localeKey, nullptr);
    if (result.isNewEntry)
        result.storedValue->value = Locale::create(localeKey);
    return *(result.storedValue->value);
}

AnimationClock& Document::animationClock()
{
    DCHECK(page());
    return page()->animator().clock();
}

Document& Document::ensureTemplateDocument()
{
    if (isTemplateDocument())
        return *this;

    if (m_templateDocument)
        return *m_templateDocument;

    if (isHTMLDocument()) {
        DocumentInit init = DocumentInit::fromContext(contextDocument(), blankURL()).withNewRegistrationContext();
        m_templateDocument = HTMLDocument::create(init);
    } else {
        m_templateDocument = Document::create(DocumentInit(blankURL()));
    }

    m_templateDocument->m_templateDocumentHost = this; // balanced in dtor.

    return *m_templateDocument.get();
}

void Document::didAssociateFormControl(Element* element)
{
    if (!frame() || !frame()->page())
        return;
    m_associatedFormControls.add(element);
    // We add a slight delay because this could be called rapidly.
    if (!m_didAssociateFormControlsTimer.isActive())
        m_didAssociateFormControlsTimer.startOneShot(0.3, BLINK_FROM_HERE);
}

void Document::removeFormAssociation(Element* element)
{
    auto it = m_associatedFormControls.find(element);
    if (it == m_associatedFormControls.end())
        return;
    m_associatedFormControls.remove(it);
    if (m_associatedFormControls.isEmpty())
        m_didAssociateFormControlsTimer.stop();
}

void Document::didAssociateFormControlsTimerFired(Timer<Document>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_didAssociateFormControlsTimer);
    if (!frame() || !frame()->page())
        return;

    HeapVector<Member<Element>> associatedFormControls;
    copyToVector(m_associatedFormControls, associatedFormControls);

    frame()->page()->chromeClient().didAssociateFormControls(associatedFormControls, frame());
    m_associatedFormControls.clear();
}

float Document::devicePixelRatio() const
{
    return m_frame ? m_frame->devicePixelRatio() : 1.0;
}

TextAutosizer* Document::textAutosizer()
{
    if (!m_textAutosizer)
        m_textAutosizer = TextAutosizer::create(this);
    return m_textAutosizer.get();
}

void Document::setAutofocusElement(Element* element)
{
    if (!element) {
        m_autofocusElement = nullptr;
        return;
    }
    if (m_hasAutofocused)
        return;
    m_hasAutofocused = true;
    DCHECK(!m_autofocusElement);
    m_autofocusElement = element;
    m_taskRunner->postTask(BLINK_FROM_HERE, createSameThreadTask(&runAutofocusTask));
}

Element* Document::activeElement() const
{
    if (Element* element = adjustedFocusedElement())
        return element;
    return body();
}

bool Document::hasFocus() const
{
    return page() && page()->focusController().isDocumentFocused(*this);
}

template<unsigned type>
bool shouldInvalidateNodeListCachesForAttr(const HeapHashSet<WeakMember<const LiveNodeListBase>> nodeLists[], const QualifiedName& attrName)
{
    if (!nodeLists[type].isEmpty() && LiveNodeListBase::shouldInvalidateTypeOnAttributeChange(static_cast<NodeListInvalidationType>(type), attrName))
        return true;
    return shouldInvalidateNodeListCachesForAttr<type + 1>(nodeLists, attrName);
}

template<>
bool shouldInvalidateNodeListCachesForAttr<numNodeListInvalidationTypes>(const HeapHashSet<WeakMember<const LiveNodeListBase>>[], const QualifiedName&)
{
    return false;
}

bool Document::shouldInvalidateNodeListCaches(const QualifiedName* attrName) const
{
    if (attrName) {
        return shouldInvalidateNodeListCachesForAttr<DoNotInvalidateOnAttributeChanges + 1>(m_nodeLists, *attrName);
    }

    for (int type = 0; type < numNodeListInvalidationTypes; ++type) {
        if (!m_nodeLists[type].isEmpty())
            return true;
    }

    return false;
}

void Document::invalidateNodeListCaches(const QualifiedName* attrName)
{
    for (const LiveNodeListBase* list : m_listsInvalidatedAtDocument)
        list->invalidateCacheForAttribute(attrName);
}

void Document::platformColorsChanged()
{
    if (!isActive())
        return;

    styleEngine().platformColorsChanged();
}

v8::Local<v8::Object> Document::wrap(v8::Isolate* isolate, v8::Local<v8::Object> creationContext)
{
    DCHECK(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperType = wrapperTypeInfo();

    if (frame() && frame()->script().initializeMainWorld()) {
        // initializeMainWorld may have created a wrapper for the object, retry from the start.
        v8::Local<v8::Object> wrapper = DOMDataStore::getWrapper(this, isolate);
        if (!wrapper.IsEmpty())
            return wrapper;
    }

    v8::Local<v8::Object> wrapper = V8DOMWrapper::createWrapper(isolate, creationContext, wrapperType);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    wrapperType->installConditionallyEnabledProperties(wrapper, isolate);
    return associateWithWrapper(isolate, wrapperType, wrapper);
}

v8::Local<v8::Object> Document::associateWithWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperType, v8::Local<v8::Object> wrapper)
{
    wrapper = V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperType, wrapper);
    DOMWrapperWorld& world = DOMWrapperWorld::current(isolate);
    if (world.isMainWorld() && frame())
        frame()->script().windowProxy(world)->updateDocumentWrapper(wrapper);
    return wrapper;
}

bool Document::isSecureContext(String& errorMessage, const SecureContextCheck privilegeContextCheck) const
{
    if (isSecureContextImpl(privilegeContextCheck))
        return true;
    errorMessage = SecurityOrigin::isPotentiallyTrustworthyErrorMessage();
    return false;
}

bool Document::isSecureContext(const SecureContextCheck privilegeContextCheck) const
{
    return isSecureContextImpl(privilegeContextCheck);
}

WebTaskRunner* Document::loadingTaskRunner() const
{
    if (frame())
        return frame()->frameScheduler()->loadingTaskRunner();
    if (m_importsController)
        return m_importsController->master()->loadingTaskRunner();
    if (m_contextDocument)
        return m_contextDocument->loadingTaskRunner();
    return Platform::current()->currentThread()->scheduler()->loadingTaskRunner();
}

WebTaskRunner* Document::timerTaskRunner() const
{
    if (frame())
        return m_frame->frameScheduler()->timerTaskRunner();
    if (m_importsController)
        return m_importsController->master()->timerTaskRunner();
    if (m_contextDocument)
        return m_contextDocument->timerTaskRunner();
    return Platform::current()->currentThread()->scheduler()->timerTaskRunner();
}

void Document::enforceInsecureRequestPolicy(WebInsecureRequestPolicy policy)
{
    // Combine the new policy with the existing policy, as a base policy may be
    // inherited from a remote parent before this page's policy is set. In other
    // words, insecure requests should be upgraded or blocked if _either_ the
    // existing policy or the newly enforced policy triggers upgrades or
    // blockage.
    setInsecureRequestPolicy(getInsecureRequestPolicy() | policy);
    if (frame())
        frame()->loader().client()->didEnforceInsecureRequestPolicy(getInsecureRequestPolicy());
}

void Document::setShadowCascadeOrder(ShadowCascadeOrder order)
{
    DCHECK_NE(order, ShadowCascadeOrder::ShadowCascadeNone);

    if (order == m_shadowCascadeOrder)
        return;

    if (order == ShadowCascadeOrder::ShadowCascadeV0) {
        m_mayContainV0Shadow = true;
        if (m_shadowCascadeOrder == ShadowCascadeOrder::ShadowCascadeV1)
            UseCounter::count(*this, UseCounter::MixedShadowRootV0AndV1);
    }

    // For V0 -> V1 upgrade, we need style recalculation for the whole document.
    if (m_shadowCascadeOrder == ShadowCascadeOrder::ShadowCascadeV0 && order == ShadowCascadeOrder::ShadowCascadeV1) {
        this->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Shadow));
        UseCounter::count(*this, UseCounter::MixedShadowRootV0AndV1);
    }

    if (order > m_shadowCascadeOrder)
        m_shadowCascadeOrder = order;
}

LayoutViewItem Document::layoutViewItem() const
{
    return LayoutViewItem(m_layoutView);
}

DEFINE_TRACE(Document)
{
    visitor->trace(m_importsController);
    visitor->trace(m_docType);
    visitor->trace(m_implementation);
    visitor->trace(m_autofocusElement);
    visitor->trace(m_focusedElement);
    visitor->trace(m_sequentialFocusNavigationStartingPoint);
    visitor->trace(m_hoverNode);
    visitor->trace(m_activeHoverElement);
    visitor->trace(m_documentElement);
    visitor->trace(m_rootScrollerController);
    visitor->trace(m_titleElement);
    visitor->trace(m_axObjectCache);
    visitor->trace(m_markers);
    visitor->trace(m_cssTarget);
    visitor->trace(m_currentScriptStack);
    visitor->trace(m_scriptRunner);
    visitor->trace(m_listsInvalidatedAtDocument);
    for (int i = 0; i < numNodeListInvalidationTypes; ++i)
        visitor->trace(m_nodeLists[i]);
    visitor->trace(m_topLayerElements);
    visitor->trace(m_elemSheet);
    visitor->trace(m_nodeIterators);
    visitor->trace(m_ranges);
    visitor->trace(m_styleEngine);
    visitor->trace(m_formController);
    visitor->trace(m_visitedLinkState);
    visitor->trace(m_frame);
    visitor->trace(m_domWindow);
    visitor->trace(m_fetcher);
    visitor->trace(m_parser);
    visitor->trace(m_contextFeatures);
    visitor->trace(m_styleSheetList);
    visitor->trace(m_documentTiming);
    visitor->trace(m_mediaQueryMatcher);
    visitor->trace(m_scriptedAnimationController);
    visitor->trace(m_scriptedIdleTaskController);
    visitor->trace(m_textAutosizer);
    visitor->trace(m_registrationContext);
    visitor->trace(m_customElementMicrotaskRunQueue);
    visitor->trace(m_elementDataCache);
    visitor->trace(m_associatedFormControls);
    visitor->trace(m_useElementsNeedingUpdate);
    visitor->trace(m_layerUpdateSVGFilterElements);
    visitor->trace(m_timers);
    visitor->trace(m_templateDocument);
    visitor->trace(m_templateDocumentHost);
    visitor->trace(m_userActionElements);
    visitor->trace(m_svgExtensions);
    visitor->trace(m_timeline);
    visitor->trace(m_compositorPendingAnimations);
    visitor->trace(m_contextDocument);
    visitor->trace(m_canvasFontCache);
    visitor->trace(m_intersectionObserverController);
    visitor->trace(m_intersectionObserverData);
    visitor->trace(m_snapCoordinator);
    Supplementable<Document>::trace(visitor);
    TreeScope::trace(visitor);
    ContainerNode::trace(visitor);
    ExecutionContext::trace(visitor);
    SecurityContext::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(Document)
{
    visitor->traceWrappers(m_importsController);
    visitor->traceWrappers(m_implementation);
    visitor->traceWrappers(m_styleSheetList);
    visitor->traceWrappers(m_styleEngine);
    visitor->traceWrappers(
        Supplementable<Document>::m_supplements.get(
            FontFaceSet::supplementName()));
    for (int i = 0; i < numNodeListInvalidationTypes; ++i) {
        for (auto list : m_nodeLists[i]) {
            visitor->traceWrappers(list);
        }
    }
    ContainerNode::traceWrappers(visitor);
}

template class CORE_TEMPLATE_EXPORT Supplement<Document>;

} // namespace blink

#ifndef NDEBUG
using namespace blink;

static WeakDocumentSet& liveDocumentSet()
{
    DEFINE_STATIC_LOCAL(WeakDocumentSet, set, ());
    return set;
}

void showLiveDocumentInstances()
{
    WeakDocumentSet& set = liveDocumentSet();
    fprintf(stderr, "There are %u documents currently alive:\n", set.size());
    for (Document* document : set)
        fprintf(stderr, "- Document %p URL: %s\n", document, document->url().getString().utf8().data());
}
#endif
