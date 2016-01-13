/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/testing/Internals.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptFunction.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8ThrowException.h"
#include "core/InternalRuntimeFlags.h"
#include "core/animation/AnimationTimeline.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarker.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/Range.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/TreeScope.h"
#include "core/dom/ViewportDescription.h"
#include "core/dom/shadow/ComposedTreeWalker.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/SelectRuleFeatureSet.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/SpellCheckRequester.h"
#include "core/editing/SpellChecker.h"
#include "core/editing/SurroundingText.h"
#include "core/editing/TextIterator.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/DOMPoint.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/forms/FormController.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorCounters.h"
#include "core/inspector/InspectorFrontendChannel.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/page/Page.h"
#include "core/page/PagePopupController.h"
#include "core/page/PrintContext.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderMenuList.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/testing/GCObservation.h"
#include "core/testing/InternalProfilers.h"
#include "core/testing/InternalSettings.h"
#include "core/testing/LayerRect.h"
#include "core/testing/LayerRectList.h"
#include "core/testing/MallocStatistics.h"
#include "core/testing/MockPagePopupDriver.h"
#include "core/testing/TypeConversions.h"
#include "core/workers/WorkerThread.h"
#include "platform/Cursor.h"
#include "platform/Language.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/filters/FilterOperation.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebConnectionType.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebLayer.h"
#include "wtf/InstanceCounter.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/dtoa.h"
#include "wtf/text/StringBuffer.h"
#include <v8.h>

namespace WebCore {

// FIXME: oilpan: These will be removed soon.
static MockPagePopupDriver* s_pagePopupDriver = 0;

using namespace HTMLNames;

static bool markerTypesFrom(const String& markerType, DocumentMarker::MarkerTypes& result)
{
    if (markerType.isEmpty() || equalIgnoringCase(markerType, "all"))
        result = DocumentMarker::AllMarkers();
    else if (equalIgnoringCase(markerType, "Spelling"))
        result =  DocumentMarker::Spelling;
    else if (equalIgnoringCase(markerType, "Grammar"))
        result =  DocumentMarker::Grammar;
    else if (equalIgnoringCase(markerType, "TextMatch"))
        result =  DocumentMarker::TextMatch;
    else
        return false;

    return true;
}

static SpellCheckRequester* spellCheckRequester(Document* document)
{
    if (!document || !document->frame())
        return 0;
    return &document->frame()->spellChecker().spellCheckRequester();
}

const char* Internals::internalsId = "internals";

PassRefPtrWillBeRawPtr<Internals> Internals::create(Document* document)
{
    return adoptRefWillBeNoop(new Internals(document));
}

Internals::~Internals()
{
}

void Internals::resetToConsistentState(Page* page)
{
    ASSERT(page);

    page->setDeviceScaleFactor(1);
    page->setIsCursorVisible(true);
    page->setPageScaleFactor(1, IntPoint(0, 0));
    TextRun::setAllowsRoundingHacks(false);
    WebCore::overrideUserPreferredLanguages(Vector<AtomicString>());
    delete s_pagePopupDriver;
    s_pagePopupDriver = 0;
    page->chrome().client().resetPagePopupDriver();
    if (!page->deprecatedLocalMainFrame()->spellChecker().isContinuousSpellCheckingEnabled())
        page->deprecatedLocalMainFrame()->spellChecker().toggleContinuousSpellChecking();
    if (page->deprecatedLocalMainFrame()->editor().isOverwriteModeEnabled())
        page->deprecatedLocalMainFrame()->editor().toggleOverwriteModeEnabled();

    if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
        scrollingCoordinator->reset();

    page->deprecatedLocalMainFrame()->view()->clear();
}

Internals::Internals(Document* document)
    : ContextLifecycleObserver(document)
    , m_runtimeFlags(InternalRuntimeFlags::create())
{
}

Document* Internals::contextDocument() const
{
    return toDocument(executionContext());
}

LocalFrame* Internals::frame() const
{
    if (!contextDocument())
        return 0;
    return contextDocument()->frame();
}

InternalSettings* Internals::settings() const
{
    Document* document = contextDocument();
    if (!document)
        return 0;
    Page* page = document->page();
    if (!page)
        return 0;
    return InternalSettings::from(*page);
}

InternalRuntimeFlags* Internals::runtimeFlags() const
{
    return m_runtimeFlags.get();
}

InternalProfilers* Internals::profilers()
{
    if (!m_profilers)
        m_profilers = InternalProfilers::create();
    return m_profilers.get();
}

unsigned Internals::workerThreadCount() const
{
    return WorkerThread::workerThreadCount();
}

String Internals::address(Node* node)
{
    char buf[32];
    sprintf(buf, "%p", node);

    return String(buf);
}

PassRefPtrWillBeRawPtr<GCObservation> Internals::observeGC(ScriptValue scriptValue)
{
    v8::Handle<v8::Value> observedValue = scriptValue.v8Value();
    ASSERT(!observedValue.IsEmpty());
    if (observedValue->IsNull() || observedValue->IsUndefined()) {
        V8ThrowException::throwTypeError("value to observe is null or undefined", v8::Isolate::GetCurrent());
        return nullptr;
    }

    return GCObservation::create(observedValue);
}

unsigned Internals::updateStyleAndReturnAffectedElementCount(ExceptionState& exceptionState) const
{
    Document* document = contextDocument();
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return 0;
    }

    unsigned beforeCount = document->styleEngine()->resolverAccessCount();
    document->updateRenderTreeIfNeeded();
    return document->styleEngine()->resolverAccessCount() - beforeCount;
}

unsigned Internals::needsLayoutCount(ExceptionState& exceptionState) const
{
    LocalFrame* contextFrame = frame();
    if (!contextFrame) {
        exceptionState.throwDOMException(InvalidAccessError, "No context frame is available.");
        return 0;
    }

    bool isPartial;
    unsigned needsLayoutObjects;
    unsigned totalObjects;
    contextFrame->countObjectsNeedingLayout(needsLayoutObjects, totalObjects, isPartial);
    return needsLayoutObjects;
}

bool Internals::isPreloaded(const String& url)
{
    Document* document = contextDocument();
    return document->fetcher()->isPreloaded(url);
}

bool Internals::isLoadingFromMemoryCache(const String& url)
{
    if (!contextDocument())
        return false;
    Resource* resource = memoryCache()->resourceForURL(contextDocument()->completeURL(url));
    return resource && resource->status() == Resource::Cached;
}

void Internals::crash()
{
    CRASH();
}

void Internals::setStyleResolverStatsEnabled(bool enabled)
{
    Document* document = contextDocument();
    if (enabled)
        document->ensureStyleResolver().enableStats(StyleResolver::ReportSlowStats);
    else
        document->ensureStyleResolver().disableStats();
}

String Internals::styleResolverStatsReport(ExceptionState& exceptionState) const
{
    Document* document = contextDocument();
    if (!document->ensureStyleResolver().stats()) {
        exceptionState.throwDOMException(InvalidStateError, "Style resolver stats not enabled");
        return String();
    }
    return document->ensureStyleResolver().stats()->report();
}

String Internals::styleResolverStatsTotalsReport(ExceptionState& exceptionState) const
{
    Document* document = contextDocument();
    if (!document->ensureStyleResolver().statsTotals()) {
        exceptionState.throwDOMException(InvalidStateError, "Style resolver stats not enabled");
        return String();
    }
    return document->ensureStyleResolver().statsTotals()->report();
}

bool Internals::isSharingStyle(Element* element1, Element* element2, ExceptionState& exceptionState) const
{
    if (!element1 || !element2) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(element1 ? 2 : 1, "Element"));
        return false;
    }
    return element1->renderStyle() == element2->renderStyle();
}

bool Internals::isValidContentSelect(Element* insertionPoint, ExceptionState& exceptionState)
{
    if (!insertionPoint || !insertionPoint->isInsertionPoint()) {
        exceptionState.throwDOMException(InvalidAccessError, "The insertion point provided is invalid.");
        return false;
    }

    return isHTMLContentElement(*insertionPoint) && toHTMLContentElement(*insertionPoint).isSelectValid();
}

Node* Internals::treeScopeRootNode(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }

    return &node->treeScope().rootNode();
}

Node* Internals::parentTreeScope(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }
    const TreeScope* parentTreeScope = node->treeScope().parentTreeScope();
    return parentTreeScope ? &parentTreeScope->rootNode() : 0;
}

bool Internals::hasSelectorForIdInShadow(Element* host, const AtomicString& idValue, ExceptionState& exceptionState)
{
    if (!host || !host->shadow()) {
        exceptionState.throwDOMException(InvalidAccessError, "The host element provided is invalid, or does not have a shadow.");
        return 0;
    }

    return host->shadow()->ensureSelectFeatureSet().hasSelectorForId(idValue);
}

bool Internals::hasSelectorForClassInShadow(Element* host, const AtomicString& className, ExceptionState& exceptionState)
{
    if (!host || !host->shadow()) {
        exceptionState.throwDOMException(InvalidAccessError, "The host element provided is invalid, or does not have a shadow.");
        return 0;
    }

    return host->shadow()->ensureSelectFeatureSet().hasSelectorForClass(className);
}

bool Internals::hasSelectorForAttributeInShadow(Element* host, const AtomicString& attributeName, ExceptionState& exceptionState)
{
    if (!host || !host->shadow()) {
        exceptionState.throwDOMException(InvalidAccessError, "The host element provided is invalid, or does not have a shadow.");
        return 0;
    }

    return host->shadow()->ensureSelectFeatureSet().hasSelectorForAttribute(attributeName);
}

bool Internals::hasSelectorForPseudoClassInShadow(Element* host, const String& pseudoClass, ExceptionState& exceptionState)
{
    if (!host || !host->shadow()) {
        exceptionState.throwDOMException(InvalidAccessError, "The host element provided is invalid, or does not have a shadow.");
        return 0;
    }

    const SelectRuleFeatureSet& featureSet = host->shadow()->ensureSelectFeatureSet();
    if (pseudoClass == "checked")
        return featureSet.hasSelectorForChecked();
    if (pseudoClass == "enabled")
        return featureSet.hasSelectorForEnabled();
    if (pseudoClass == "disabled")
        return featureSet.hasSelectorForDisabled();
    if (pseudoClass == "indeterminate")
        return featureSet.hasSelectorForIndeterminate();
    if (pseudoClass == "link")
        return featureSet.hasSelectorForLink();
    if (pseudoClass == "target")
        return featureSet.hasSelectorForTarget();
    if (pseudoClass == "visited")
        return featureSet.hasSelectorForVisited();

    ASSERT_NOT_REACHED();
    return false;
}

unsigned short Internals::compareTreeScopePosition(const Node* node1, const Node* node2, ExceptionState& exceptionState) const
{
    if (!node1 || !node2) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(node1 ? 2 : 1, "Node"));
        return 0;
    }
    const TreeScope* treeScope1 = node1->isDocumentNode() ? static_cast<const TreeScope*>(toDocument(node1)) :
        node1->isShadowRoot() ? static_cast<const TreeScope*>(toShadowRoot(node1)) : 0;
    const TreeScope* treeScope2 = node2->isDocumentNode() ? static_cast<const TreeScope*>(toDocument(node2)) :
        node2->isShadowRoot() ? static_cast<const TreeScope*>(toShadowRoot(node2)) : 0;
    if (!treeScope1 || !treeScope2) {
        exceptionState.throwDOMException(InvalidAccessError, String::format("The %s node is neither a document node, nor a shadow root.", treeScope1 ? "second" : "first"));
        return 0;
    }
    return treeScope1->comparePosition(*treeScope2);
}

unsigned Internals::numberOfActiveAnimations() const
{
    LocalFrame* contextFrame = frame();
    Document* document = contextFrame->document();
    return document->timeline().numberOfActiveAnimationsForTesting();
}

void Internals::pauseAnimations(double pauseTime, ExceptionState& exceptionState)
{
    if (pauseTime < 0) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::indexExceedsMinimumBound("pauseTime", pauseTime, 0.0));
        return;
    }

    frame()->view()->updateLayoutAndStyleForPainting();
    frame()->document()->timeline().pauseAnimationsForTesting(pauseTime);
}

bool Internals::hasShadowInsertionPoint(const Node* root, ExceptionState& exceptionState) const
{
    if (root && root->isShadowRoot())
        return toShadowRoot(root)->containsShadowElements();

    exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
    return 0;
}

bool Internals::hasContentElement(const Node* root, ExceptionState& exceptionState) const
{
    if (root && root->isShadowRoot())
        return toShadowRoot(root)->containsContentElements();

    exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
    return 0;
}

size_t Internals::countElementShadow(const Node* root, ExceptionState& exceptionState) const
{
    if (!root || !root->isShadowRoot()) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }
    return toShadowRoot(root)->childShadowRootCount();
}

Node* Internals::nextSiblingByWalker(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }
    ComposedTreeWalker walker(node);
    walker.nextSibling();
    return walker.get();
}

Node* Internals::firstChildByWalker(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }
    ComposedTreeWalker walker(node);
    walker.firstChild();
    return walker.get();
}

Node* Internals::lastChildByWalker(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }
    ComposedTreeWalker walker(node);
    walker.lastChild();
    return walker.get();
}

Node* Internals::nextNodeByWalker(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }
    ComposedTreeWalker walker(node);
    walker.next();
    return walker.get();
}

Node* Internals::previousNodeByWalker(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }
    ComposedTreeWalker walker(node);
    walker.previous();
    return walker.get();
}

String Internals::elementRenderTreeAsText(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return String();
    }

    String representation = externalRepresentation(element);
    if (representation.isEmpty()) {
        exceptionState.throwDOMException(InvalidAccessError, "The element provided has no external representation.");
        return String();
    }

    return representation;
}

PassRefPtrWillBeRawPtr<CSSComputedStyleDeclaration> Internals::computedStyleIncludingVisitedInfo(Node* node, ExceptionState& exceptionState) const
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return nullptr;
    }

    bool allowVisitedStyle = true;
    return CSSComputedStyleDeclaration::create(node, allowVisitedStyle);
}

ShadowRoot* Internals::shadowRoot(Element* host, ExceptionState& exceptionState)
{
    // FIXME: Internals::shadowRoot() in tests should be converted to youngestShadowRoot() or oldestShadowRoot().
    // https://bugs.webkit.org/show_bug.cgi?id=78465
    return youngestShadowRoot(host, exceptionState);
}

ShadowRoot* Internals::youngestShadowRoot(Element* host, ExceptionState& exceptionState)
{
    if (!host) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return 0;
    }

    if (ElementShadow* shadow = host->shadow())
        return shadow->youngestShadowRoot();
    return 0;
}

ShadowRoot* Internals::oldestShadowRoot(Element* host, ExceptionState& exceptionState)
{
    if (!host) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return 0;
    }

    if (ElementShadow* shadow = host->shadow())
        return shadow->oldestShadowRoot();
    return 0;
}

ShadowRoot* Internals::youngerShadowRoot(Node* shadow, ExceptionState& exceptionState)
{
    if (!shadow || !shadow->isShadowRoot()) {
        exceptionState.throwDOMException(InvalidAccessError, "The node provided is not a valid shadow root.");
        return 0;
    }

    return toShadowRoot(shadow)->youngerShadowRoot();
}

String Internals::shadowRootType(const Node* root, ExceptionState& exceptionState) const
{
    if (!root || !root->isShadowRoot()) {
        exceptionState.throwDOMException(InvalidAccessError, "The node provided is not a valid shadow root.");
        return String();
    }

    switch (toShadowRoot(root)->type()) {
    case ShadowRoot::UserAgentShadowRoot:
        return String("UserAgentShadowRoot");
    case ShadowRoot::AuthorShadowRoot:
        return String("AuthorShadowRoot");
    default:
        ASSERT_NOT_REACHED();
        return String("Unknown");
    }
}

const AtomicString& Internals::shadowPseudoId(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return nullAtom;
    }

    return element->shadowPseudoId();
}

void Internals::setShadowPseudoId(Element* element, const AtomicString& id, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return;
    }

    return element->setShadowPseudoId(id);
}

String Internals::visiblePlaceholder(Element* element)
{
    if (element && isHTMLTextFormControlElement(*element)) {
        if (toHTMLTextFormControlElement(element)->placeholderShouldBeVisible())
            return toHTMLTextFormControlElement(element)->placeholderElement()->textContent();
    }

    return String();
}

void Internals::selectColorInColorChooser(Element* element, const String& colorValue)
{
    ASSERT(element);
    if (!isHTMLInputElement(*element))
        return;
    Color color;
    if (!color.setFromString(colorValue))
        return;
    toHTMLInputElement(*element).selectColorInColorChooser(color);
}

bool Internals::hasAutofocusRequest(Document* document)
{
    if (!document)
        document = contextDocument();
    return document->autofocusElement();
}

bool Internals::hasAutofocusRequest()
{
    return hasAutofocusRequest(0);
}

Vector<String> Internals::formControlStateOfHistoryItem(ExceptionState& exceptionState)
{
    HistoryItem* mainItem = frame()->loader().currentItem();
    if (!mainItem) {
        exceptionState.throwDOMException(InvalidAccessError, "No history item is available.");
        return Vector<String>();
    }
    return mainItem->documentState();
}

void Internals::setFormControlStateOfHistoryItem(const Vector<String>& state, ExceptionState& exceptionState)
{
    HistoryItem* mainItem = frame()->loader().currentItem();
    if (!mainItem) {
        exceptionState.throwDOMException(InvalidAccessError, "No history item is available.");
        return;
    }
    mainItem->clearDocumentState();
    mainItem->setDocumentState(state);
}

void Internals::setEnableMockPagePopup(bool enabled, ExceptionState& exceptionState)
{
    Document* document = contextDocument();
    if (!document || !document->page())
        return;
    Page* page = document->page();
    if (!enabled) {
        page->chrome().client().resetPagePopupDriver();
        return;
    }
    if (!s_pagePopupDriver)
        s_pagePopupDriver = MockPagePopupDriver::create(page->deprecatedLocalMainFrame()).leakPtr();
    page->chrome().client().setPagePopupDriver(s_pagePopupDriver);
}

PassRefPtrWillBeRawPtr<PagePopupController> Internals::pagePopupController()
{
    return s_pagePopupDriver ? s_pagePopupDriver->pagePopupController() : 0;
}

PassRefPtrWillBeRawPtr<ClientRect> Internals::unscaledViewportRect(ExceptionState& exceptionState)
{
    Document* document = contextDocument();
    if (!document || !document->view()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's viewport cannot be retrieved." : "No context document can be obtained.");
        return ClientRect::create();
    }

    return ClientRect::create(document->view()->visibleContentRect());
}

PassRefPtrWillBeRawPtr<ClientRect> Internals::absoluteCaretBounds(ExceptionState& exceptionState)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "No context document can be obtained.");
        return ClientRect::create();
    }

    return ClientRect::create(document->frame()->selection().absoluteCaretBounds());
}

PassRefPtrWillBeRawPtr<ClientRect> Internals::boundingBox(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return ClientRect::create();
    }

    element->document().updateLayoutIgnorePendingStylesheets();
    RenderObject* renderer = element->renderer();
    if (!renderer)
        return ClientRect::create();
    return ClientRect::create(renderer->absoluteBoundingBoxRectIgnoringTransforms());
}

unsigned Internals::markerCountForNode(Node* node, const String& markerType, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }

    DocumentMarker::MarkerTypes markerTypes = 0;
    if (!markerTypesFrom(markerType, markerTypes)) {
        exceptionState.throwDOMException(SyntaxError, "The marker type provided ('" + markerType + "') is invalid.");
        return 0;
    }

    return node->document().markers().markersFor(node, markerTypes).size();
}

unsigned Internals::activeMarkerCountForNode(Node* node, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }

    // Only TextMatch markers can be active.
    DocumentMarker::MarkerType markerType = DocumentMarker::TextMatch;
    WillBeHeapVector<DocumentMarker*> markers = node->document().markers().markersFor(node, markerType);

    unsigned activeMarkerCount = 0;
    for (WillBeHeapVector<DocumentMarker*>::iterator iter = markers.begin(); iter != markers.end(); ++iter) {
        if ((*iter)->activeMatch())
            activeMarkerCount++;
    }

    return activeMarkerCount;
}

DocumentMarker* Internals::markerAt(Node* node, const String& markerType, unsigned index, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return 0;
    }

    DocumentMarker::MarkerTypes markerTypes = 0;
    if (!markerTypesFrom(markerType, markerTypes)) {
        exceptionState.throwDOMException(SyntaxError, "The marker type provided ('" + markerType + "') is invalid.");
        return 0;
    }

    WillBeHeapVector<DocumentMarker*> markers = node->document().markers().markersFor(node, markerTypes);
    if (markers.size() <= index)
        return 0;
    return markers[index];
}

PassRefPtrWillBeRawPtr<Range> Internals::markerRangeForNode(Node* node, const String& markerType, unsigned index, ExceptionState& exceptionState)
{
    DocumentMarker* marker = markerAt(node, markerType, index, exceptionState);
    if (!marker)
        return nullptr;
    return Range::create(node->document(), node, marker->startOffset(), node, marker->endOffset());
}

String Internals::markerDescriptionForNode(Node* node, const String& markerType, unsigned index, ExceptionState& exceptionState)
{
    DocumentMarker* marker = markerAt(node, markerType, index, exceptionState);
    if (!marker)
        return String();
    return marker->description();
}

void Internals::addTextMatchMarker(const Range* range, bool isActive)
{
    range->ownerDocument().updateLayoutIgnorePendingStylesheets();
    range->ownerDocument().markers().addTextMatchMarker(range, isActive);
}

void Internals::setMarkersActive(Node* node, unsigned startOffset, unsigned endOffset, bool active, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Node"));
        return;
    }

    node->document().markers().setMarkersActive(node, startOffset, endOffset, active);
}

void Internals::setMarkedTextMatchesAreHighlighted(Document* document, bool highlight, ExceptionState&)
{
    if (!document || !document->frame())
        return;

    document->frame()->editor().setMarkedTextMatchesAreHighlighted(highlight);
}

void Internals::setScrollViewPosition(Document* document, long x, long y, ExceptionState& exceptionState)
{
    if (!document || !document->view()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's view cannot be retrieved." : "The document provided is invalid.");
        return;
    }

    FrameView* frameView = document->view();
    bool constrainsScrollingToContentEdgeOldValue = frameView->constrainsScrollingToContentEdge();
    bool scrollbarsSuppressedOldValue = frameView->scrollbarsSuppressed();

    frameView->setConstrainsScrollingToContentEdge(false);
    frameView->setScrollbarsSuppressed(false);
    frameView->setScrollOffsetFromInternals(IntPoint(x, y));
    frameView->setScrollbarsSuppressed(scrollbarsSuppressedOldValue);
    frameView->setConstrainsScrollingToContentEdge(constrainsScrollingToContentEdgeOldValue);
}

String Internals::viewportAsText(Document* document, float, int availableWidth, int availableHeight, ExceptionState& exceptionState)
{
    if (!document || !document->page()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's page cannot be retrieved." : "The document provided is invalid.");
        return String();
    }

    document->updateLayoutIgnorePendingStylesheets();

    Page* page = document->page();

    // Update initial viewport size.
    IntSize initialViewportSize(availableWidth, availableHeight);
    document->page()->deprecatedLocalMainFrame()->view()->setFrameRect(IntRect(IntPoint::zero(), initialViewportSize));

    ViewportDescription description = page->viewportDescription();
    PageScaleConstraints constraints = description.resolve(initialViewportSize, Length());

    constraints.fitToContentsWidth(constraints.layoutSize.width(), availableWidth);

    StringBuilder builder;

    builder.appendLiteral("viewport size ");
    builder.append(String::number(constraints.layoutSize.width()));
    builder.append('x');
    builder.append(String::number(constraints.layoutSize.height()));

    builder.appendLiteral(" scale ");
    builder.append(String::number(constraints.initialScale));
    builder.appendLiteral(" with limits [");
    builder.append(String::number(constraints.minimumScale));
    builder.appendLiteral(", ");
    builder.append(String::number(constraints.maximumScale));

    builder.appendLiteral("] and userScalable ");
    builder.append(description.userZoom ? "true" : "false");

    return builder.toString();
}

bool Internals::wasLastChangeUserEdit(Element* textField, ExceptionState& exceptionState)
{
    if (!textField) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return false;
    }

    if (isHTMLInputElement(*textField))
        return toHTMLInputElement(*textField).lastChangeWasUserEdit();

    if (isHTMLTextAreaElement(*textField))
        return toHTMLTextAreaElement(*textField).lastChangeWasUserEdit();

    exceptionState.throwDOMException(InvalidNodeTypeError, "The element provided is not a TEXTAREA.");
    return false;
}

bool Internals::elementShouldAutoComplete(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return false;
    }

    if (isHTMLInputElement(*element))
        return toHTMLInputElement(*element).shouldAutocomplete();

    exceptionState.throwDOMException(InvalidNodeTypeError, "The element provided is not an INPUT.");
    return false;
}

String Internals::suggestedValue(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return String();
    }

    if (!element->isFormControlElement()) {
        exceptionState.throwDOMException(InvalidNodeTypeError, "The element provided is not a form control element.");
        return String();
    }

    String suggestedValue;
    if (isHTMLInputElement(*element))
        suggestedValue = toHTMLInputElement(*element).suggestedValue();

    if (isHTMLTextAreaElement(*element))
        suggestedValue = toHTMLTextAreaElement(*element).suggestedValue();

    if (isHTMLSelectElement(*element))
        suggestedValue = toHTMLSelectElement(*element).suggestedValue();

    return suggestedValue;
}

void Internals::setSuggestedValue(Element* element, const String& value, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return;
    }

    if (!element->isFormControlElement()) {
        exceptionState.throwDOMException(InvalidNodeTypeError, "The element provided is not a form control element.");
        return;
    }

    if (isHTMLInputElement(*element))
        toHTMLInputElement(*element).setSuggestedValue(value);

    if (isHTMLTextAreaElement(*element))
        toHTMLTextAreaElement(*element).setSuggestedValue(value);

    if (isHTMLSelectElement(*element))
        toHTMLSelectElement(*element).setSuggestedValue(value);
}

void Internals::setEditingValue(Element* element, const String& value, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return;
    }

    if (!isHTMLInputElement(*element)) {
        exceptionState.throwDOMException(InvalidNodeTypeError, "The element provided is not an INPUT.");
        return;
    }

    toHTMLInputElement(*element).setEditingValue(value);
}

void Internals::setAutofilled(Element* element, bool enabled, ExceptionState& exceptionState)
{
    if (!element->isFormControlElement()) {
        exceptionState.throwDOMException(InvalidNodeTypeError, "The element provided is not a form control element.");
        return;
    }
    toHTMLFormControlElement(element)->setAutofilled(enabled);
}

void Internals::scrollElementToRect(Element* element, long x, long y, long w, long h, ExceptionState& exceptionState)
{
    if (!element || !element->document().view()) {
        exceptionState.throwDOMException(InvalidNodeTypeError, element ? "No view can be obtained from the provided element's document." : ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return;
    }
    FrameView* frameView = element->document().view();
    frameView->scrollElementToRect(element, IntRect(x, y, w, h));
}

PassRefPtrWillBeRawPtr<Range> Internals::rangeFromLocationAndLength(Element* scope, int rangeLocation, int rangeLength, ExceptionState& exceptionState)
{
    if (!scope) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return nullptr;
    }

    // TextIterator depends on Layout information, make sure layout it up to date.
    scope->document().updateLayoutIgnorePendingStylesheets();

    return PlainTextRange(rangeLocation, rangeLocation + rangeLength).createRange(*scope);
}

unsigned Internals::locationFromRange(Element* scope, const Range* range, ExceptionState& exceptionState)
{
    if (!scope || !range) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(scope ? 2 : 1, scope ? "Range" : "Element"));
        return 0;
    }

    // PlainTextRange depends on Layout information, make sure layout it up to date.
    scope->document().updateLayoutIgnorePendingStylesheets();

    return PlainTextRange::create(*scope, *range).start();
}

unsigned Internals::lengthFromRange(Element* scope, const Range* range, ExceptionState& exceptionState)
{
    if (!scope || !range) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(scope ? 2 : 1, scope ? "Range" : "Element"));
        return 0;
    }

    // PlainTextRange depends on Layout information, make sure layout it up to date.
    scope->document().updateLayoutIgnorePendingStylesheets();

    return PlainTextRange::create(*scope, *range).length();
}

String Internals::rangeAsText(const Range* range, ExceptionState& exceptionState)
{
    if (!range) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Range"));
        return String();
    }

    return range->text();
}

PassRefPtrWillBeRawPtr<DOMPoint> Internals::touchPositionAdjustedToBestClickableNode(long x, long y, long width, long height, Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return nullptr;
    }

    document->updateLayout();

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode;
    IntPoint adjustedPoint;

    bool foundNode = document->frame()->eventHandler().bestClickableNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    if (foundNode)
        return DOMPoint::create(adjustedPoint.x(), adjustedPoint.y());

    return nullptr;
}

Node* Internals::touchNodeAdjustedToBestClickableNode(long x, long y, long width, long height, Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return 0;
    }

    document->updateLayout();

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode;
    IntPoint adjustedPoint;
    document->frame()->eventHandler().bestClickableNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    return targetNode;
}

PassRefPtrWillBeRawPtr<DOMPoint> Internals::touchPositionAdjustedToBestContextMenuNode(long x, long y, long width, long height, Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return nullptr;
    }

    document->updateLayout();

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode = 0;
    IntPoint adjustedPoint;

    bool foundNode = document->frame()->eventHandler().bestContextMenuNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    if (foundNode)
        return DOMPoint::create(adjustedPoint.x(), adjustedPoint.y());

    return DOMPoint::create(x, y);
}

Node* Internals::touchNodeAdjustedToBestContextMenuNode(long x, long y, long width, long height, Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return 0;
    }

    document->updateLayout();

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode = 0;
    IntPoint adjustedPoint;
    document->frame()->eventHandler().bestContextMenuNodeForTouchPoint(point, radius, adjustedPoint, targetNode);
    return targetNode;
}

PassRefPtrWillBeRawPtr<ClientRect> Internals::bestZoomableAreaForTouchPoint(long x, long y, long width, long height, Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return nullptr;
    }

    document->updateLayout();

    IntSize radius(width / 2, height / 2);
    IntPoint point(x + radius.width(), y + radius.height());

    Node* targetNode;
    IntRect zoomableArea;
    bool foundNode = document->frame()->eventHandler().bestZoomableAreaForTouchPoint(point, radius, zoomableArea, targetNode);
    if (foundNode)
        return ClientRect::create(zoomableArea);

    return nullptr;
}


int Internals::lastSpellCheckRequestSequence(Document* document, ExceptionState& exceptionState)
{
    SpellCheckRequester* requester = spellCheckRequester(document);

    if (!requester) {
        exceptionState.throwDOMException(InvalidAccessError, "No spell check requestor can be obtained for the provided document.");
        return -1;
    }

    return requester->lastRequestSequence();
}

int Internals::lastSpellCheckProcessedSequence(Document* document, ExceptionState& exceptionState)
{
    SpellCheckRequester* requester = spellCheckRequester(document);

    if (!requester) {
        exceptionState.throwDOMException(InvalidAccessError, "No spell check requestor can be obtained for the provided document.");
        return -1;
    }

    return requester->lastProcessedSequence();
}

Vector<AtomicString> Internals::userPreferredLanguages() const
{
    return WebCore::userPreferredLanguages();
}

// Optimally, the bindings generator would pass a Vector<AtomicString> here but
// this is not supported yet.
void Internals::setUserPreferredLanguages(const Vector<String>& languages)
{
    Vector<AtomicString> atomicLanguages;
    for (size_t i = 0; i < languages.size(); ++i)
        atomicLanguages.append(AtomicString(languages[i]));
    WebCore::overrideUserPreferredLanguages(atomicLanguages);
}

unsigned Internals::activeDOMObjectCount(Document* document, ExceptionState& exceptionState)
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return 0;
    }

    return document->activeDOMObjectCount();
}

static unsigned eventHandlerCount(Document& document, EventHandlerRegistry::EventHandlerClass handlerClass)
{
    if (!document.frameHost())
        return 0;
    EventHandlerRegistry* registry = &document.frameHost()->eventHandlerRegistry();
    unsigned count = 0;
    const EventTargetSet* targets = registry->eventHandlerTargets(handlerClass);
    if (targets) {
        for (EventTargetSet::const_iterator iter = targets->begin(); iter != targets->end(); ++iter)
            count += iter->value;
    }
    return count;
}

unsigned Internals::wheelEventHandlerCount(Document* document, ExceptionState& exceptionState)
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return 0;
    }

    return eventHandlerCount(*document, EventHandlerRegistry::WheelEvent);
}

unsigned Internals::scrollEventHandlerCount(Document* document, ExceptionState& exceptionState)
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return 0;
    }

    return eventHandlerCount(*document, EventHandlerRegistry::ScrollEvent);
}

unsigned Internals::touchEventHandlerCount(Document* document, ExceptionState& exceptionState)
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return 0;
    }

    const TouchEventTargetSet* touchHandlers = document->touchEventTargets();
    if (!touchHandlers)
        return 0;

    unsigned count = 0;
    for (TouchEventTargetSet::const_iterator iter = touchHandlers->begin(); iter != touchHandlers->end(); ++iter)
        count += iter->value;
    return count;
}

static RenderLayer* findRenderLayerForGraphicsLayer(RenderLayer* searchRoot, GraphicsLayer* graphicsLayer, IntSize* layerOffset, String* layerType)
{
    *layerOffset = IntSize();
    if (searchRoot->hasCompositedLayerMapping() && graphicsLayer == searchRoot->compositedLayerMapping()->mainGraphicsLayer()) {
        CompositedLayerMappingPtr compositedLayerMapping = searchRoot->compositedLayerMapping();
        LayoutSize offset = compositedLayerMapping->contentOffsetInCompositingLayer();
        *layerOffset = IntSize(offset.width(), offset.height());
        return searchRoot;
    }

    GraphicsLayer* layerForScrolling = searchRoot->scrollableArea() ? searchRoot->scrollableArea()->layerForScrolling() : 0;
    if (graphicsLayer == layerForScrolling) {
        *layerType = "scrolling";
        return searchRoot;
    }

    if (searchRoot->compositingState() == PaintsIntoGroupedBacking) {
        GraphicsLayer* squashingLayer = searchRoot->groupedMapping()->squashingLayer();
        if (graphicsLayer == squashingLayer) {
            *layerType ="squashing";
            *layerOffset = -searchRoot->offsetFromSquashingLayerOrigin();
            return searchRoot;
        }
    }

    GraphicsLayer* layerForHorizontalScrollbar = searchRoot->scrollableArea() ? searchRoot->scrollableArea()->layerForHorizontalScrollbar() : 0;
    if (graphicsLayer == layerForHorizontalScrollbar) {
        *layerType = "horizontalScrollbar";
        return searchRoot;
    }

    GraphicsLayer* layerForVerticalScrollbar = searchRoot->scrollableArea() ? searchRoot->scrollableArea()->layerForVerticalScrollbar() : 0;
    if (graphicsLayer == layerForVerticalScrollbar) {
        *layerType = "verticalScrollbar";
        return searchRoot;
    }

    GraphicsLayer* layerForScrollCorner = searchRoot->scrollableArea() ? searchRoot->scrollableArea()->layerForScrollCorner() : 0;
    if (graphicsLayer == layerForScrollCorner) {
        *layerType = "scrollCorner";
        return searchRoot;
    }

    // Search right to left to increase the chances that we'll choose the top-most layers in a
    // grouped mapping for squashing.
    for (RenderLayer* child = searchRoot->lastChild(); child; child = child->previousSibling()) {
        RenderLayer* foundLayer = findRenderLayerForGraphicsLayer(child, graphicsLayer, layerOffset, layerType);
        if (foundLayer)
            return foundLayer;
    }

    return 0;
}

// Given a vector of rects, merge those that are adjacent, leaving empty rects
// in the place of no longer used slots. This is intended to simplify the list
// of rects returned by an SkRegion (which have been split apart for sorting
// purposes). No attempt is made to do this efficiently (eg. by relying on the
// sort criteria of SkRegion).
static void mergeRects(blink::WebVector<blink::WebRect>& rects)
{
    for (size_t i = 0; i < rects.size(); ++i) {
        if (rects[i].isEmpty())
            continue;
        bool updated;
        do {
            updated = false;
            for (size_t j = i+1; j < rects.size(); ++j) {
                if (rects[j].isEmpty())
                    continue;
                // Try to merge rects[j] into rects[i] along the 4 possible edges.
                if (rects[i].y == rects[j].y && rects[i].height == rects[j].height) {
                    if (rects[i].x + rects[i].width == rects[j].x) {
                        rects[i].width += rects[j].width;
                        rects[j] = blink::WebRect();
                        updated = true;
                    } else if (rects[i].x == rects[j].x + rects[j].width) {
                        rects[i].x = rects[j].x;
                        rects[i].width += rects[j].width;
                        rects[j] = blink::WebRect();
                        updated = true;
                    }
                } else if (rects[i].x == rects[j].x && rects[i].width == rects[j].width) {
                    if (rects[i].y + rects[i].height == rects[j].y) {
                        rects[i].height += rects[j].height;
                        rects[j] = blink::WebRect();
                        updated = true;
                    } else if (rects[i].y == rects[j].y + rects[j].height) {
                        rects[i].y = rects[j].y;
                        rects[i].height += rects[j].height;
                        rects[j] = blink::WebRect();
                        updated = true;
                    }
                }
            }
        } while (updated);
    }
}

static void accumulateLayerRectList(RenderLayerCompositor* compositor, GraphicsLayer* graphicsLayer, LayerRectList* rects)
{
    blink::WebVector<blink::WebRect> layerRects = graphicsLayer->platformLayer()->touchEventHandlerRegion();
    if (!layerRects.isEmpty()) {
        mergeRects(layerRects);
        String layerType;
        IntSize layerOffset;
        RenderLayer* renderLayer = findRenderLayerForGraphicsLayer(compositor->rootRenderLayer(), graphicsLayer, &layerOffset, &layerType);
        Node* node = renderLayer ? renderLayer->renderer()->node() : 0;
        for (size_t i = 0; i < layerRects.size(); ++i) {
            if (!layerRects[i].isEmpty()) {
                rects->append(node, layerType, layerOffset.width(), layerOffset.height(), ClientRect::create(layerRects[i]));
            }
        }
    }

    size_t numChildren = graphicsLayer->children().size();
    for (size_t i = 0; i < numChildren; ++i)
        accumulateLayerRectList(compositor, graphicsLayer->children()[i], rects);
}

PassRefPtrWillBeRawPtr<LayerRectList> Internals::touchEventTargetLayerRects(Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->view() || !document->page() || document != contextDocument()) {
        exceptionState.throwDOMException(InvalidAccessError, "The document provided is invalid.");
        return nullptr;
    }

    // Do any pending layout and compositing update (which may call touchEventTargetRectsChange) to ensure this
    // really takes any previous changes into account.
    forceCompositingUpdate(document, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    if (RenderView* view = document->renderView()) {
        if (RenderLayerCompositor* compositor = view->compositor()) {
            if (GraphicsLayer* rootLayer = compositor->rootGraphicsLayer()) {
                RefPtrWillBeRawPtr<LayerRectList> rects = LayerRectList::create();
                accumulateLayerRectList(compositor, rootLayer, rects.get());
                return rects;
            }
        }
    }

    return nullptr;
}

PassRefPtrWillBeRawPtr<StaticNodeList> Internals::nodesFromRect(Document* document, int centerX, int centerY, unsigned topPadding, unsigned rightPadding,
    unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, bool allowChildFrameContent, ExceptionState& exceptionState) const
{
    if (!document || !document->frame() || !document->frame()->view()) {
        exceptionState.throwDOMException(InvalidAccessError, "No view can be obtained from the provided document.");
        return nullptr;
    }

    LocalFrame* frame = document->frame();
    FrameView* frameView = document->view();
    RenderView* renderView = document->renderView();

    if (!renderView)
        return nullptr;

    float zoomFactor = frame->pageZoomFactor();
    LayoutPoint point = roundedLayoutPoint(FloatPoint(centerX * zoomFactor + frameView->scrollX(), centerY * zoomFactor + frameView->scrollY()));

    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active;
    if (ignoreClipping)
        hitType |= HitTestRequest::IgnoreClipping;
    if (!allowShadowContent)
        hitType |= HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent;
    if (allowChildFrameContent)
        hitType |= HitTestRequest::AllowChildFrameContent;

    HitTestRequest request(hitType);

    // When ignoreClipping is false, this method returns null for coordinates outside of the viewport.
    if (!request.ignoreClipping() && !frameView->visibleContentRect().intersects(HitTestLocation::rectForPoint(point, topPadding, rightPadding, bottomPadding, leftPadding)))
        return nullptr;

    WillBeHeapVector<RefPtrWillBeMember<Node> > matches;

    // Need padding to trigger a rect based hit test, but we want to return a NodeList
    // so we special case this.
    if (!topPadding && !rightPadding && !bottomPadding && !leftPadding) {
        HitTestResult result(point);
        renderView->hitTest(request, result);
        if (result.innerNode())
            matches.append(result.innerNode()->deprecatedShadowAncestorNode());
    } else {
        HitTestResult result(point, topPadding, rightPadding, bottomPadding, leftPadding);
        renderView->hitTest(request, result);
        copyToVector(result.rectBasedTestResult(), matches);
    }

    return StaticNodeList::adopt(matches);
}

void Internals::emitInspectorDidBeginFrame(int frameId)
{
    contextDocument()->page()->inspectorController().didBeginFrame(frameId);
}

void Internals::emitInspectorDidCancelFrame()
{
    contextDocument()->page()->inspectorController().didCancelFrame();
}

bool Internals::hasSpellingMarker(Document* document, int from, int length, ExceptionState&)
{
    if (!document || !document->frame())
        return 0;

    return document->frame()->spellChecker().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

void Internals::setContinuousSpellCheckingEnabled(bool enabled, ExceptionState&)
{
    if (!contextDocument() || !contextDocument()->frame())
        return;

    if (enabled != contextDocument()->frame()->spellChecker().isContinuousSpellCheckingEnabled())
        contextDocument()->frame()->spellChecker().toggleContinuousSpellChecking();
}

bool Internals::isOverwriteModeEnabled(Document* document, ExceptionState&)
{
    if (!document || !document->frame())
        return 0;

    return document->frame()->editor().isOverwriteModeEnabled();
}

void Internals::toggleOverwriteModeEnabled(Document* document, ExceptionState&)
{
    if (!document || !document->frame())
        return;

    document->frame()->editor().toggleOverwriteModeEnabled();
}

unsigned Internals::numberOfLiveNodes() const
{
    return InspectorCounters::counterValue(InspectorCounters::NodeCounter);
}

unsigned Internals::numberOfLiveDocuments() const
{
    return InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
}

String Internals::dumpRefCountedInstanceCounts() const
{
    return WTF::dumpRefCountedInstanceCounts();
}

Vector<String> Internals::consoleMessageArgumentCounts(Document* document) const
{
    InstrumentingAgents* instrumentingAgents = instrumentationForPage(document->page());
    if (!instrumentingAgents)
        return Vector<String>();
    InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent();
    if (!consoleAgent)
        return Vector<String>();
    Vector<unsigned> counts = consoleAgent->consoleMessageArgumentCounts();
    Vector<String> result(counts.size());
    for (size_t i = 0; i < counts.size(); i++)
        result[i] = String::number(counts[i]);
    return result;
}

Vector<unsigned long> Internals::setMemoryCacheCapacities(unsigned long minDeadBytes, unsigned long maxDeadBytes, unsigned long totalBytes)
{
    Vector<unsigned long> result;
    result.append(memoryCache()->minDeadCapacity());
    result.append(memoryCache()->maxDeadCapacity());
    result.append(memoryCache()->capacity());
    memoryCache()->setCapacities(minDeadBytes, maxDeadBytes, totalBytes);
    return result;
}

void Internals::setInspectorResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionState& exceptionState)
{
    Page* page = contextDocument()->frame()->page();
    if (!page) {
        exceptionState.throwDOMException(InvalidAccessError, "No page can be obtained from the current context document.");
        return;
    }
    page->inspectorController().setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
}

bool Internals::hasGrammarMarker(Document* document, int from, int length, ExceptionState&)
{
    if (!document || !document->frame())
        return 0;

    return document->frame()->spellChecker().selectionStartHasMarkerFor(DocumentMarker::Grammar, from, length);
}

unsigned Internals::numberOfScrollableAreas(Document* document, ExceptionState&)
{
    unsigned count = 0;
    LocalFrame* frame = document->frame();
    if (frame->view()->scrollableAreas())
        count += frame->view()->scrollableAreas()->size();

    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame() && toLocalFrame(child)->view() && toLocalFrame(child)->view()->scrollableAreas())
            count += toLocalFrame(child)->view()->scrollableAreas()->size();
    }

    return count;
}

bool Internals::isPageBoxVisible(Document* document, int pageNumber, ExceptionState& exceptionState)
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return false;
    }

    return document->isPageBoxVisible(pageNumber);
}

String Internals::layerTreeAsText(Document* document, ExceptionState& exceptionState) const
{
    return layerTreeAsText(document, 0, exceptionState);
}

String Internals::elementLayerTreeAsText(Element* element, ExceptionState& exceptionState) const
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return String();
    }

    FrameView* frameView = element->document().view();
    frameView->updateLayoutAndStyleForPainting();

    return elementLayerTreeAsText(element, 0, exceptionState);
}

bool Internals::scrollsWithRespectTo(Element* element1, Element* element2, ExceptionState& exceptionState)
{
    if (!element1 || !element2) {
        exceptionState.throwDOMException(InvalidAccessError, String::format("The %s element provided is invalid.", element1 ? "second" : "first"));
        return 0;
    }

    element1->document().updateLayout();

    RenderObject* renderer1 = element1->renderer();
    RenderObject* renderer2 = element2->renderer();
    if (!renderer1 || !renderer1->isBox()) {
        exceptionState.throwDOMException(InvalidAccessError, renderer1 ? "The first provided element's renderer is not a box." : "The first provided element has no renderer.");
        return 0;
    }
    if (!renderer2 || !renderer2->isBox()) {
        exceptionState.throwDOMException(InvalidAccessError, renderer2 ? "The second provided element's renderer is not a box." : "The second provided element has no renderer.");
        return 0;
    }

    RenderLayer* layer1 = toRenderBox(renderer1)->layer();
    RenderLayer* layer2 = toRenderBox(renderer2)->layer();
    if (!layer1 || !layer2) {
        exceptionState.throwDOMException(InvalidAccessError, String::format("No render layer can be obtained from the %s provided element.", layer1 ? "second" : "first"));
        return 0;
    }

    return layer1->scrollsWithRespectTo(layer2);
}

bool Internals::isUnclippedDescendant(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return 0;
    }

    element->document().view()->updateLayoutAndStyleForPainting();

    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isBox()) {
        exceptionState.throwDOMException(InvalidAccessError, renderer ? "The provided element's renderer is not a box." : "The provided element has no renderer.");
        return 0;
    }

    RenderLayer* layer = toRenderBox(renderer)->layer();
    if (!layer) {
        exceptionState.throwDOMException(InvalidAccessError, "No render layer can be obtained from the provided element.");
        return 0;
    }

    // We used to compute isUnclippedDescendant only when acceleratedCompositingForOverflowScrollEnabled,
    // but now we compute it all the time.
    // FIXME: Remove this if statement and rebaseline the tests that make this assumption.
    if (!layer->compositor()->acceleratedCompositingForOverflowScrollEnabled())
        return false;

    return layer->compositingInputs().isUnclippedDescendant;
}

String Internals::layerTreeAsText(Document* document, unsigned flags, ExceptionState& exceptionState) const
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return String();
    }

    document->view()->updateLayoutAndStyleForPainting();

    return document->frame()->layerTreeAsText(flags);
}

String Internals::elementLayerTreeAsText(Element* element, unsigned flags, ExceptionState& exceptionState) const
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return String();
    }

    element->document().updateLayout();

    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isBox()) {
        exceptionState.throwDOMException(InvalidAccessError, renderer ? "The provided element's renderer is not a box." : "The provided element has no renderer.");
        return String();
    }

    RenderLayer* layer = toRenderBox(renderer)->layer();
    if (!layer
        || !layer->hasCompositedLayerMapping()
        || !layer->compositedLayerMapping()->mainGraphicsLayer()) {
        // Don't raise exception in these cases which may be normally used in tests.
        return String();
    }

    return layer->compositedLayerMapping()->mainGraphicsLayer()->layerTreeAsText(flags);
}

static RenderLayer* getRenderLayerForElement(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return 0;
    }

    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isBox()) {
        exceptionState.throwDOMException(InvalidAccessError, renderer ? "The provided element's renderer is not a box." : "The provided element has no renderer.");
        return 0;
    }

    RenderLayer* layer = toRenderBox(renderer)->layer();
    if (!layer) {
        exceptionState.throwDOMException(InvalidAccessError, "No render layer can be obtained from the provided element.");
        return 0;
    }

    return layer;
}

String Internals::repaintRectsAsText(Document* document, ExceptionState& exceptionState) const
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return String();
    }

    return document->frame()->trackedRepaintRectsAsText();
}

PassRefPtrWillBeRawPtr<ClientRectList> Internals::repaintRects(Element* element, ExceptionState& exceptionState) const
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return nullptr;
    }

    element->document().frame()->view()->updateLayoutAndStyleForPainting();

    if (RenderLayer* layer = getRenderLayerForElement(element, exceptionState)) {
        if (layer->compositingState() == PaintsIntoOwnBacking) {
            OwnPtr<Vector<FloatRect> > rects = layer->collectTrackedRepaintRects();
            ASSERT(rects.get());
            Vector<FloatQuad> quads(rects->size());
            for (size_t i = 0; i < rects->size(); ++i)
                quads[i] = FloatRect(rects->at(i));
            return ClientRectList::create(quads);
        }
    }

    exceptionState.throwDOMException(InvalidAccessError, "The provided element is not composited.");
    return nullptr;
}

String Internals::scrollingStateTreeAsText(Document* document, ExceptionState& exceptionState) const
{
    return String();
}

String Internals::mainThreadScrollingReasons(Document* document, ExceptionState& exceptionState) const
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return String();
    }

    document->frame()->view()->updateLayoutAndStyleForPainting();

    Page* page = document->page();
    if (!page)
        return String();

    return page->mainThreadScrollingReasonsAsText();
}

PassRefPtrWillBeRawPtr<ClientRectList> Internals::nonFastScrollableRects(Document* document, ExceptionState& exceptionState) const
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return nullptr;
    }

    Page* page = document->page();
    if (!page)
        return nullptr;

    return page->nonFastScrollableRects(document->frame());
}

void Internals::garbageCollectDocumentResources(Document* document, ExceptionState& exceptionState) const
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return;
    }
    ResourceFetcher* fetcher = document->fetcher();
    if (!fetcher)
        return;
    fetcher->garbageCollectDocumentResources();
}

void Internals::evictAllResources() const
{
    memoryCache()->evictResources();
}

void Internals::allowRoundingHacks() const
{
    TextRun::setAllowsRoundingHacks(true);
}

String Internals::counterValue(Element* element)
{
    if (!element)
        return String();

    return counterValueForElement(element);
}

int Internals::pageNumber(Element* element, float pageWidth, float pageHeight)
{
    if (!element)
        return 0;

    return PrintContext::pageNumberForElement(element, FloatSize(pageWidth, pageHeight));
}

Vector<String> Internals::iconURLs(Document* document, int iconTypesMask) const
{
    Vector<IconURL> iconURLs = document->iconURLs(iconTypesMask);
    Vector<String> array;

    Vector<IconURL>::const_iterator iter(iconURLs.begin());
    for (; iter != iconURLs.end(); ++iter)
        array.append(iter->m_iconURL.string());

    return array;
}

Vector<String> Internals::shortcutIconURLs(Document* document) const
{
    return iconURLs(document, Favicon);
}

Vector<String> Internals::allIconURLs(Document* document) const
{
    return iconURLs(document, Favicon | TouchIcon | TouchPrecomposedIcon);
}

int Internals::numberOfPages(float pageWidth, float pageHeight)
{
    if (!frame())
        return -1;

    return PrintContext::numberOfPages(frame(), FloatSize(pageWidth, pageHeight));
}

String Internals::pageProperty(String propertyName, int pageNumber, ExceptionState& exceptionState) const
{
    if (!frame()) {
        exceptionState.throwDOMException(InvalidAccessError, "No frame is available.");
        return String();
    }

    return PrintContext::pageProperty(frame(), propertyName.utf8().data(), pageNumber);
}

String Internals::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft, ExceptionState& exceptionState) const
{
    if (!frame()) {
        exceptionState.throwDOMException(InvalidAccessError, "No frame is available.");
        return String();
    }

    return PrintContext::pageSizeAndMarginsInPixels(frame(), pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft);
}

void Internals::setDeviceScaleFactor(float scaleFactor, ExceptionState& exceptionState)
{
    Document* document = contextDocument();
    if (!document || !document->page()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's page cannot be retrieved." : "No context document can be obtained.");
        return;
    }
    Page* page = document->page();
    page->setDeviceScaleFactor(scaleFactor);
}

void Internals::setIsCursorVisible(Document* document, bool isVisible, ExceptionState& exceptionState)
{
    if (!document || !document->page()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's page cannot be retrieved." : "No context document can be obtained.");
        return;
    }
    document->page()->setIsCursorVisible(isVisible);
}

void Internals::webkitWillEnterFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    FullscreenElementStack::from(*document).webkitWillEnterFullScreenForElement(element);
}

void Internals::webkitDidEnterFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    FullscreenElementStack::from(*document).webkitDidEnterFullScreenForElement(element);
}

void Internals::webkitWillExitFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    FullscreenElementStack::from(*document).webkitWillExitFullScreenForElement(element);
}

void Internals::webkitDidExitFullScreenForElement(Document* document, Element* element)
{
    if (!document)
        return;
    FullscreenElementStack::from(*document).webkitDidExitFullScreenForElement(element);
}

void Internals::mediaPlayerRequestFullscreen(HTMLMediaElement* mediaElement)
{
    mediaElement->mediaPlayerRequestFullscreen();
}

void Internals::registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme)
{
    SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy(scheme);
}

void Internals::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme)
{
    SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(scheme);
}

PassRefPtrWillBeRawPtr<MallocStatistics> Internals::mallocStatistics() const
{
    return MallocStatistics::create();
}

PassRefPtrWillBeRawPtr<TypeConversions> Internals::typeConversions() const
{
    return TypeConversions::create();
}

Vector<String> Internals::getReferencedFilePaths() const
{
    return frame()->loader().currentItem()->getReferencedFilePaths();
}

void Internals::startTrackingRepaints(Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->view()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's view cannot be retrieved." : "The document provided is invalid.");
        return;
    }

    FrameView* frameView = document->view();
    frameView->updateLayoutAndStyleForPainting();
    frameView->setTracksPaintInvalidations(true);
}

void Internals::stopTrackingRepaints(Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->view()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's view cannot be retrieved." : "The document provided is invalid.");
        return;
    }

    FrameView* frameView = document->view();
    frameView->updateLayoutAndStyleForPainting();
    frameView->setTracksPaintInvalidations(false);
}

void Internals::updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(ExceptionState& exceptionState)
{
    updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(0, exceptionState);
}

void Internals::updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(Node* node, ExceptionState& exceptionState)
{
    Document* document;
    if (!node) {
        document = contextDocument();
    } else if (node->isDocumentNode()) {
        document = toDocument(node);
    } else if (isHTMLIFrameElement(*node)) {
        document = toHTMLIFrameElement(*node).contentDocument();
    } else {
        exceptionState.throwTypeError("The node provided is neither a document nor an IFrame.");
        return;
    }
    document->updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasksSynchronously);
}

void Internals::forceFullRepaint(Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->view()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's view cannot be retrieved." : "The document provided is invalid.");
        return;
    }

    if (RenderView *renderView = document->renderView())
        renderView->repaintViewAndCompositedLayers();
}

PassRefPtrWillBeRawPtr<ClientRectList> Internals::draggableRegions(Document* document, ExceptionState& exceptionState)
{
    return annotatedRegions(document, true, exceptionState);
}

PassRefPtrWillBeRawPtr<ClientRectList> Internals::nonDraggableRegions(Document* document, ExceptionState& exceptionState)
{
    return annotatedRegions(document, false, exceptionState);
}

PassRefPtrWillBeRawPtr<ClientRectList> Internals::annotatedRegions(Document* document, bool draggable, ExceptionState& exceptionState)
{
    if (!document || !document->view()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's view cannot be retrieved." : "The document provided is invalid.");
        return ClientRectList::create();
    }

    document->updateLayout();
    document->view()->updateAnnotatedRegions();
    Vector<AnnotatedRegionValue> regions = document->annotatedRegions();

    Vector<FloatQuad> quads;
    for (size_t i = 0; i < regions.size(); ++i) {
        if (regions[i].draggable == draggable)
            quads.append(FloatQuad(regions[i].bounds));
    }
    return ClientRectList::create(quads);
}

static const char* cursorTypeToString(Cursor::Type cursorType)
{
    switch (cursorType) {
    case Cursor::Pointer: return "Pointer";
    case Cursor::Cross: return "Cross";
    case Cursor::Hand: return "Hand";
    case Cursor::IBeam: return "IBeam";
    case Cursor::Wait: return "Wait";
    case Cursor::Help: return "Help";
    case Cursor::EastResize: return "EastResize";
    case Cursor::NorthResize: return "NorthResize";
    case Cursor::NorthEastResize: return "NorthEastResize";
    case Cursor::NorthWestResize: return "NorthWestResize";
    case Cursor::SouthResize: return "SouthResize";
    case Cursor::SouthEastResize: return "SouthEastResize";
    case Cursor::SouthWestResize: return "SouthWestResize";
    case Cursor::WestResize: return "WestResize";
    case Cursor::NorthSouthResize: return "NorthSouthResize";
    case Cursor::EastWestResize: return "EastWestResize";
    case Cursor::NorthEastSouthWestResize: return "NorthEastSouthWestResize";
    case Cursor::NorthWestSouthEastResize: return "NorthWestSouthEastResize";
    case Cursor::ColumnResize: return "ColumnResize";
    case Cursor::RowResize: return "RowResize";
    case Cursor::MiddlePanning: return "MiddlePanning";
    case Cursor::EastPanning: return "EastPanning";
    case Cursor::NorthPanning: return "NorthPanning";
    case Cursor::NorthEastPanning: return "NorthEastPanning";
    case Cursor::NorthWestPanning: return "NorthWestPanning";
    case Cursor::SouthPanning: return "SouthPanning";
    case Cursor::SouthEastPanning: return "SouthEastPanning";
    case Cursor::SouthWestPanning: return "SouthWestPanning";
    case Cursor::WestPanning: return "WestPanning";
    case Cursor::Move: return "Move";
    case Cursor::VerticalText: return "VerticalText";
    case Cursor::Cell: return "Cell";
    case Cursor::ContextMenu: return "ContextMenu";
    case Cursor::Alias: return "Alias";
    case Cursor::Progress: return "Progress";
    case Cursor::NoDrop: return "NoDrop";
    case Cursor::Copy: return "Copy";
    case Cursor::None: return "None";
    case Cursor::NotAllowed: return "NotAllowed";
    case Cursor::ZoomIn: return "ZoomIn";
    case Cursor::ZoomOut: return "ZoomOut";
    case Cursor::Grab: return "Grab";
    case Cursor::Grabbing: return "Grabbing";
    case Cursor::Custom: return "Custom";
    }

    ASSERT_NOT_REACHED();
    return "UNKNOWN";
}

String Internals::getCurrentCursorInfo(Document* document, ExceptionState& exceptionState)
{
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "The document provided is invalid.");
        return String();
    }

    Cursor cursor = document->frame()->eventHandler().currentMouseCursor();

    StringBuilder result;
    result.append("type=");
    result.append(cursorTypeToString(cursor.type()));
    result.append(" hotSpot=");
    result.appendNumber(cursor.hotSpot().x());
    result.append(",");
    result.appendNumber(cursor.hotSpot().y());
    if (cursor.image()) {
        IntSize size = cursor.image()->size();
        result.append(" image=");
        result.appendNumber(size.width());
        result.append("x");
        result.appendNumber(size.height());
    }
    if (cursor.imageScaleFactor() != 1) {
        result.append(" scale=");
        NumberToStringBuffer buffer;
        result.append(numberToFixedPrecisionString(cursor.imageScaleFactor(), 8, buffer, true));
    }

    return result.toString();
}

PassRefPtr<ArrayBuffer> Internals::serializeObject(PassRefPtr<SerializedScriptValue> value) const
{
    String stringValue = value->toWireString();
    RefPtr<ArrayBuffer> buffer = ArrayBuffer::createUninitialized(stringValue.length(), sizeof(UChar));
    stringValue.copyTo(static_cast<UChar*>(buffer->data()), 0, stringValue.length());
    return buffer.release();
}

PassRefPtr<SerializedScriptValue> Internals::deserializeBuffer(PassRefPtr<ArrayBuffer> buffer) const
{
    String value(static_cast<const UChar*>(buffer->data()), buffer->byteLength() / sizeof(UChar));
    return SerializedScriptValue::createFromWire(value);
}

void Internals::forceReload(bool endToEnd)
{
    frame()->loader().reload(endToEnd ? EndToEndReload : NormalReload);
}

PassRefPtrWillBeRawPtr<ClientRect> Internals::selectionBounds(ExceptionState& exceptionState)
{
    Document* document = contextDocument();
    if (!document || !document->frame()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's frame cannot be retrieved." : "No context document can be obtained.");
        return nullptr;
    }

    return ClientRect::create(document->frame()->selection().bounds());
}

String Internals::markerTextForListItem(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return String();
    }
    return WebCore::markerTextForListItem(element);
}

String Internals::getImageSourceURL(Element* element, ExceptionState& exceptionState)
{
    if (!element) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return String();
    }
    return element->imageSourceURL();
}

String Internals::baseURL(Document* document, ExceptionState& exceptionState)
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return String();
    }

    return document->baseURL().string();
}

bool Internals::isSelectPopupVisible(Node* node)
{
    ASSERT(node);
    if (!isHTMLSelectElement(*node))
        return false;

    HTMLSelectElement& select = toHTMLSelectElement(*node);

    RenderObject* renderer = select.renderer();
    if (!renderer || !renderer->isMenuList())
        return false;

    RenderMenuList* menuList = toRenderMenuList(renderer);
    return menuList->popupIsVisible();
}

bool Internals::loseSharedGraphicsContext3D()
{
    OwnPtr<blink::WebGraphicsContext3DProvider> sharedProvider = adoptPtr(blink::Platform::current()->createSharedOffscreenGraphicsContext3DProvider());
    if (!sharedProvider)
        return false;
    blink::WebGraphicsContext3D* sharedContext = sharedProvider->context3d();
    sharedContext->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_EXT, GL_INNOCENT_CONTEXT_RESET_EXT);
    // To prevent tests that call loseSharedGraphicsContext3D from being
    // flaky, we call finish so that the context is guaranteed to be lost
    // synchronously (i.e. before returning).
    sharedContext->finish();
    return true;
}

void Internals::forceCompositingUpdate(Document* document, ExceptionState& exceptionState)
{
    // Hit when running content_shell with --expose-internals-for-testing.
    DisableCompositingQueryAsserts disabler;

    if (!document || !document->renderView()) {
        exceptionState.throwDOMException(InvalidAccessError, document ? "The document's render view cannot be retrieved." : "The document provided is invalid.");
        return;
    }

    document->frame()->view()->updateLayoutAndStyleForPainting();
}

void Internals::setZoomFactor(float factor)
{
    frame()->setPageZoomFactor(factor);
}

void Internals::setShouldRevealPassword(Element* element, bool reveal, ExceptionState& exceptionState)
{
    if (!isHTMLInputElement(element)) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::argumentNullOrIncorrectType(1, "Element"));
        return;
    }

    return toHTMLInputElement(*element).setShouldRevealPassword(reveal);
}

namespace {

class AddOneFunction : public ScriptFunction {
public:
    static PassOwnPtr<ScriptFunction> create(ExecutionContext* context)
    {
        return adoptPtr(new AddOneFunction(toIsolate(context)));
    }

private:
    AddOneFunction(v8::Isolate* isolate)
        : ScriptFunction(isolate)
    {
    }

    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        v8::Local<v8::Value> v8Value = value.v8Value();
        ASSERT(v8Value->IsNumber());
        int intValue = v8Value.As<v8::Integer>()->Value();
        ScriptValue result  = ScriptValue(ScriptState::current(isolate()), v8::Integer::New(isolate(), intValue + 1));
        return result;
    }
};

} // namespace

ScriptPromise Internals::createPromise(ScriptState* scriptState)
{
    return ScriptPromiseResolver::create(scriptState)->promise();
}

ScriptPromise Internals::createResolvedPromise(ScriptState* scriptState, ScriptValue value)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->resolve(value);
    return promise;
}

ScriptPromise Internals::createRejectedPromise(ScriptState* scriptState, ScriptValue value)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->reject(value);
    return promise;
}

ScriptPromise Internals::addOneToPromise(ExecutionContext* context, ScriptPromise promise)
{
    return promise.then(AddOneFunction::create(context));
}

void Internals::trace(Visitor* visitor)
{
    visitor->trace(m_runtimeFlags);
    visitor->trace(m_profilers);
}

void Internals::setValueForUser(Element* element, const String& value)
{
    toHTMLInputElement(element)->setValueForUser(value);
}

String Internals::textSurroundingNode(Node* node, int x, int y, unsigned long maxLength)
{
    if (!node)
        return String();
    blink::WebPoint point(x, y);
    SurroundingText surroundingText(VisiblePosition(node->renderer()->positionForPoint(static_cast<IntPoint>(point))).deepEquivalent().parentAnchoredEquivalent(), maxLength);
    return surroundingText.content();
}

void Internals::setFocused(bool focused)
{
    frame()->page()->focusController().setFocused(focused);
}

bool Internals::ignoreLayoutWithPendingStylesheets(Document* document, ExceptionState& exceptionState)
{
    if (!document) {
        exceptionState.throwDOMException(InvalidAccessError, "No context document is available.");
        return false;
    }

    return document->ignoreLayoutWithPendingStylesheets();
}

void Internals::setNetworkStateNotifierTestOnly(bool testOnly)
{
    networkStateNotifier().setTestUpdatesOnly(testOnly);
}

void Internals::setNetworkConnectionInfo(const String& type, ExceptionState& exceptionState)
{
    blink::WebConnectionType webtype;
    if (type == "cellular") {
        webtype = blink::ConnectionTypeCellular;
    } else if (type == "bluetooth") {
        webtype = blink::ConnectionTypeBluetooth;
    } else if (type == "ethernet") {
        webtype = blink::ConnectionTypeEthernet;
    } else if (type == "wifi") {
        webtype = blink::ConnectionTypeWifi;
    } else if (type == "other") {
        webtype = blink::ConnectionTypeOther;
    } else if (type == "none") {
        webtype = blink::ConnectionTypeNone;
    } else {
        exceptionState.throwDOMException(NotFoundError, ExceptionMessages::failedToEnumerate("connection type", type));
        return;
    }
    networkStateNotifier().setWebConnectionTypeForTest(webtype);
}

} // namespace WebCore
