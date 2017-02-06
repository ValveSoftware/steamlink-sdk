// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/IntersectionObserverCallback.h"
#include "core/dom/IntersectionObserverController.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "core/dom/IntersectionObserverInit.h"
#include "core/dom/NodeIntersectionObserverData.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/Timer.h"
#include <algorithm>

namespace blink {

namespace {

// Internal implementation of IntersectionObserverCallback when using
// IntersectionObserver with an EventCallback.
class IntersectionObserverCallbackImpl final : public IntersectionObserverCallback {
    WTF_MAKE_NONCOPYABLE(IntersectionObserverCallbackImpl);
public:
    IntersectionObserverCallbackImpl(ExecutionContext* context, std::unique_ptr<IntersectionObserver::EventCallback> callback)
        : m_context(context)
        , m_callback(std::move(callback))
    {}

    void handleEvent(const HeapVector<Member<IntersectionObserverEntry>>& entries, IntersectionObserver&) override
    {
        (*m_callback.get())(entries);
    }

    ExecutionContext* getExecutionContext() const override
    {
        return m_context;
    }

    DEFINE_INLINE_TRACE()
    {
        IntersectionObserverCallback::trace(visitor);
        visitor->trace(m_context);
    }

private:
    WeakMember<ExecutionContext> m_context;
    std::unique_ptr<IntersectionObserver::EventCallback> m_callback;
};

void parseRootMargin(String rootMarginParameter, Vector<Length>& rootMargin, ExceptionState& exceptionState)
{
    // TODO(szager): Make sure this exact syntax and behavior is spec-ed somewhere.

    // The root margin argument accepts syntax similar to that for CSS margin:
    //
    // "1px" = top/right/bottom/left
    // "1px 2px" = top/bottom left/right
    // "1px 2px 3px" = top left/right bottom
    // "1px 2px 3px 4px" = top left right bottom
    CSSTokenizer::Scope tokenizerScope(rootMarginParameter);
    CSSParserTokenRange tokenRange = tokenizerScope.tokenRange();
    while (tokenRange.peek().type() != EOFToken && !exceptionState.hadException()) {
        if (rootMargin.size() == 4) {
            exceptionState.throwDOMException(SyntaxError, "Extra text found at the end of rootMargin.");
            break;
        }
        const CSSParserToken& token = tokenRange.consumeIncludingWhitespace();
        switch (token.type()) {
        case PercentageToken:
            rootMargin.append(Length(token.numericValue(), Percent));
            break;
        case DimensionToken:
            switch (token.unitType()) {
            case CSSPrimitiveValue::UnitType::Pixels:
                rootMargin.append(Length(static_cast<int>(floor(token.numericValue())), Fixed));
                break;
            case CSSPrimitiveValue::UnitType::Percentage:
                rootMargin.append(Length(token.numericValue(), Percent));
                break;
            default:
                exceptionState.throwDOMException(SyntaxError, "rootMargin must be specified in pixels or percent.");
            }
            break;
        default:
            exceptionState.throwDOMException(SyntaxError, "rootMargin must be specified in pixels or percent.");
        }
    }
}

void parseThresholds(const DoubleOrDoubleArray& thresholdParameter, Vector<float>& thresholds, ExceptionState& exceptionState)
{
    if (thresholdParameter.isDouble()) {
        thresholds.append(static_cast<float>(thresholdParameter.getAsDouble()));
    } else {
        for (auto thresholdValue : thresholdParameter.getAsDoubleArray())
            thresholds.append(static_cast<float>(thresholdValue));
    }

    for (auto thresholdValue : thresholds) {
        if (thresholdValue < 0.0 || thresholdValue > 1.0) {
            exceptionState.throwRangeError("Threshold values must be between 0 and 1");
            break;
        }
    }

    std::sort(thresholds.begin(), thresholds.end());
}

// Returns the root Node of a given Document to use as the IntersectionObserver
// root when no root is given.
// TODO(szager): it doesn't support RemoteFrames, see https://crbug.com/615156
Node* getRootNode(Document* document)
{
    Frame* mainFrame = document->frame()->tree().top();
    if (mainFrame && mainFrame->isLocalFrame())
        return toLocalFrame(mainFrame)->document();
    return nullptr;
}

} // anonymous namespace

IntersectionObserver* IntersectionObserver::create(const IntersectionObserverInit& observerInit, IntersectionObserverCallback& callback, ExceptionState& exceptionState)
{
    Node* root = observerInit.root();
    if (!root) {
        ExecutionContext* context = callback.getExecutionContext();
        DCHECK(context->isDocument());
        root = getRootNode(toDocument(context));
    }
    if (!root) {
        exceptionState.throwDOMException(HierarchyRequestError, "Unable to get root node in main frame to track.");
        return nullptr;
    }

    Vector<Length> rootMargin;
    if (observerInit.hasRootMargin())
        parseRootMargin(observerInit.rootMargin(), rootMargin, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    Vector<float> thresholds;
    if (observerInit.hasThreshold())
        parseThresholds(observerInit.threshold(), thresholds, exceptionState);
    else
        thresholds.append(0);
    if (exceptionState.hadException())
        return nullptr;

    return new IntersectionObserver(callback, *root, rootMargin, thresholds);
}

IntersectionObserver* IntersectionObserver::create(const Vector<Length>& rootMargin, const Vector<float>& thresholds, Document* document, std::unique_ptr<EventCallback> callback)
{
    Node* root = getRootNode(document);
    if (!root)
        return nullptr;

    IntersectionObserverCallbackImpl* intersectionObserverCallback = new IntersectionObserverCallbackImpl(document, std::move(callback));
    return new IntersectionObserver(*intersectionObserverCallback, *root, rootMargin, thresholds);
}

IntersectionObserver::IntersectionObserver(IntersectionObserverCallback& callback, Node& root, const Vector<Length>& rootMargin, const Vector<float>& thresholds)
    : m_callback(&callback)
    , m_root(&root)
    , m_thresholds(thresholds)
    , m_topMargin(Fixed)
    , m_rightMargin(Fixed)
    , m_bottomMargin(Fixed)
    , m_leftMargin(Fixed)
{
    switch (rootMargin.size()) {
    case 0:
        break;
    case 1:
        m_topMargin = m_rightMargin = m_bottomMargin = m_leftMargin = rootMargin[0];
        break;
    case 2:
        m_topMargin = m_bottomMargin = rootMargin[0];
        m_rightMargin = m_leftMargin = rootMargin[1];
        break;
    case 3:
        m_topMargin = rootMargin[0];
        m_rightMargin = m_leftMargin = rootMargin[1];
        m_bottomMargin = rootMargin[2];
        break;
    case 4:
        m_topMargin = rootMargin[0];
        m_rightMargin = rootMargin[1];
        m_bottomMargin = rootMargin[2];
        m_leftMargin = rootMargin[3];
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    root.document().ensureIntersectionObserverController().addTrackedObserver(*this);
}

void IntersectionObserver::clearWeakMembers(Visitor* visitor)
{
    if (ThreadHeap::isHeapObjectAlive(m_root))
        return;
    IgnorableExceptionState exceptionState;
    disconnect(exceptionState);
    m_root = nullptr;
}

LayoutObject* IntersectionObserver::rootLayoutObject() const
{
    Node* node = rootNode();
    if (node->isDocumentNode())
        return LayoutAPIShim::layoutObjectFrom(toDocument(node)->layoutViewItem());
    return toElement(node)->layoutObject();
}

void IntersectionObserver::observe(Element* target, ExceptionState& exceptionState)
{
    if (!m_root) {
        exceptionState.throwDOMException(InvalidStateError, "observe() called on an IntersectionObserver with an invalid root.");
        return;
    }

    if (!target || m_root.get() == target)
        return;

    if (target->ensureIntersectionObserverData().getObservationFor(*this))
        return;
    bool shouldReportRootBounds = false;
    bool isDOMDescendant = false;
    LocalFrame* targetFrame = target->document().frame();
    LocalFrame* rootFrame = m_root->document().frame();

    if (target->document() == rootNode()->document()) {
        shouldReportRootBounds = true;
        isDOMDescendant = target->isDescendantOf(rootNode());
    } else if (targetFrame && rootFrame) {
        shouldReportRootBounds = targetFrame->securityContext()->getSecurityOrigin()->canAccess(rootFrame->securityContext()->getSecurityOrigin());
        isDOMDescendant = (targetFrame->tree().top() == rootFrame);
    }

    IntersectionObservation* observation = new IntersectionObservation(*this, *target, shouldReportRootBounds);
    target->ensureIntersectionObserverData().addObservation(*observation);
    m_observations.add(observation);

    if (!isDOMDescendant) {
        m_root->document().addConsoleMessage(ConsoleMessage::create(
            JSMessageSource, WarningMessageLevel,
            "IntersectionObserver.observe(target): target element is not a descendant of root."));
        return;
    }

    if (!rootFrame)
        return;
    if (FrameView* rootFrameView = rootFrame->view())
        rootFrameView->scheduleAnimation();
}

void IntersectionObserver::unobserve(Element* target, ExceptionState& exceptionState)
{
    if (!m_root) {
        exceptionState.throwDOMException(InvalidStateError, "unobserve() called on an IntersectionObserver with an invalid root.");
        return;
    }

    if (!target || !target->intersectionObserverData())
        return;
    // TODO(szager): unobserve callback
    if (IntersectionObservation* observation = target->intersectionObserverData()->getObservationFor(*this))
        observation->disconnect();
}

void IntersectionObserver::computeIntersectionObservations()
{
    Document* callbackDocument = toDocument(m_callback->getExecutionContext());
    if (!callbackDocument)
        return;
    LocalDOMWindow* callbackDOMWindow = callbackDocument->domWindow();
    if (!callbackDOMWindow)
        return;
    DOMHighResTimeStamp timestamp = DOMWindowPerformance::performance(*callbackDOMWindow)->now();
    for (auto& observation : m_observations)
        observation->computeIntersectionObservations(timestamp);
}

void IntersectionObserver::disconnect(ExceptionState& exceptionState)
{
    if (!m_root) {
        exceptionState.throwDOMException(InvalidStateError, "disconnect() called on an IntersectionObserver with an invalid root.");
        return;
    }

    for (auto& observation : m_observations)
        observation->clearRootAndRemoveFromTarget();
    m_observations.clear();
}

void IntersectionObserver::removeObservation(IntersectionObservation& observation)
{
    m_observations.remove(&observation);
}

HeapVector<Member<IntersectionObserverEntry>> IntersectionObserver::takeRecords(ExceptionState& exceptionState)
{
    HeapVector<Member<IntersectionObserverEntry>> entries;

    if (!m_root)
        exceptionState.throwDOMException(InvalidStateError, "takeRecords() called on an IntersectionObserver with an invalid root.");
    else
        entries.swap(m_entries);

    return entries;
}

Element* IntersectionObserver::root() const
{
    Node* node = rootNode();
    if (node && !node->isDocumentNode())
        return toElement(node);
    return nullptr;
}

static void appendLength(StringBuilder& stringBuilder, const Length& length)
{
    stringBuilder.appendNumber(length.intValue());
    if (length.type() == Percent)
        stringBuilder.append('%');
    else
        stringBuilder.append("px", 2);
}

String IntersectionObserver::rootMargin() const
{
    StringBuilder stringBuilder;
    appendLength(stringBuilder, m_topMargin);
    stringBuilder.append(' ');
    appendLength(stringBuilder, m_rightMargin);
    stringBuilder.append(' ');
    appendLength(stringBuilder, m_bottomMargin);
    stringBuilder.append(' ');
    appendLength(stringBuilder, m_leftMargin);
    return stringBuilder.toString();
}

void IntersectionObserver::enqueueIntersectionObserverEntry(IntersectionObserverEntry& entry)
{
    m_entries.append(&entry);
    toDocument(m_callback->getExecutionContext())->ensureIntersectionObserverController().scheduleIntersectionObserverForDelivery(*this);
}

static LayoutUnit computeMargin(const Length& length, LayoutUnit referenceLength)
{
    if (length.type() == Percent)
        return LayoutUnit(static_cast<int>(referenceLength.toFloat() * length.percent() / 100.0));
    DCHECK_EQ(length.type(), Fixed);
    return LayoutUnit(length.intValue());
}

void IntersectionObserver::applyRootMargin(LayoutRect& rect) const
{
    // TODO(szager): Make sure the spec is clear that left/right margins are resolved against
    // width and not height.
    LayoutUnit topMargin = computeMargin(m_topMargin, rect.height());
    LayoutUnit rightMargin = computeMargin(m_rightMargin, rect.width());
    LayoutUnit bottomMargin = computeMargin(m_bottomMargin, rect.height());
    LayoutUnit leftMargin = computeMargin(m_leftMargin, rect.width());

    rect.setX(rect.x() - leftMargin);
    rect.setWidth(rect.width() + leftMargin + rightMargin);
    rect.setY(rect.y() - topMargin);
    rect.setHeight(rect.height() + topMargin + bottomMargin);
}

unsigned IntersectionObserver::firstThresholdGreaterThan(float ratio) const
{
    unsigned result = 0;
    while (result < m_thresholds.size() && m_thresholds[result] <= ratio)
        ++result;
    return result;
}

void IntersectionObserver::deliver()
{
    if (m_entries.isEmpty())
        return;

    HeapVector<Member<IntersectionObserverEntry>> entries;
    entries.swap(m_entries);
    m_callback->handleEvent(entries, *this);
}

DEFINE_TRACE(IntersectionObserver)
{
    visitor->template registerWeakMembers<IntersectionObserver, &IntersectionObserver::clearWeakMembers>(this);
    visitor->trace(m_callback);
    visitor->trace(m_observations);
    visitor->trace(m_entries);
}

} // namespace blink
