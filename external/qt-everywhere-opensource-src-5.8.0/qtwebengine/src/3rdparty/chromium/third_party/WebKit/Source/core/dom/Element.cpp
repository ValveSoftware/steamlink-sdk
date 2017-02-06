/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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

#include "core/dom/Element.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "core/CSSValueKeywords.h"
#include "core/SVGNames.h"
#include "core/XMLNames.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CustomCompositorAnimations.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/SelectorFilterParentScope.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/css/resolver/StyleSharingDepthScope.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Attr.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/DatasetDOMStringMap.h"
#include "core/dom/ElementDataCache.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/LayoutTreeBuilder.h"
#include "core/dom/MutationObserverInterestGroup.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/NamedNodeMap.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeIntersectionObserverData.h"
#include "core/dom/PresentationAttributeStyle.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "core/dom/custom/V0CustomElement.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/ShadowRootInit.h"
#include "core/dom/shadow/SlotAssignment.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/EventDispatcher.h"
#include "core/events/FocusEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/ScrollToOptions.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/ClassList.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormControlsCollection.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLOptionsCollection.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/HTMLTableRowsCollection.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/api/LayoutBoxItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "core/page/SpatialNavigation.h"
#include "core/page/scrolling/ScrollCustomizationCallbacks.h"
#include "core/page/scrolling/ScrollState.h"
#include "core/page/scrolling/ScrollStateCallback.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGAElement.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/BitVector.h"
#include "wtf/HashFunctions.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"
#include <memory>

namespace blink {

namespace {

// We need to retain the scroll customization callbacks until the element
// they're associated with is destroyed. It would be simplest if the callbacks
// could be stored in ElementRareData, but we can't afford the space
// increase. Instead, keep the scroll customization callbacks here. The other
// option would be to store these callbacks on the FrameHost or document, but
// that necessitates a bunch more logic for transferring the callbacks between
// FrameHosts when elements are moved around.
ScrollCustomizationCallbacks& scrollCustomizationCallbacks()
{
    DEFINE_STATIC_LOCAL(ScrollCustomizationCallbacks, scrollCustomizationCallbacks, (new ScrollCustomizationCallbacks));
    return scrollCustomizationCallbacks;
}

} // namespace

using namespace HTMLNames;
using namespace XMLNames;

enum class ClassStringContent { Empty, WhiteSpaceOnly, HasClasses };

Element* Element::create(const QualifiedName& tagName, Document* document)
{
    return new Element(tagName, document, CreateElement);
}

Element::Element(const QualifiedName& tagName, Document* document, ConstructionType type)
    : ContainerNode(document, type)
    , m_tagName(tagName)
{
}

Element::~Element()
{
    DCHECK(needsAttach());
}

inline ElementRareData* Element::elementRareData() const
{
    DCHECK(hasRareData());
    return static_cast<ElementRareData*>(rareData());
}

inline ElementRareData& Element::ensureElementRareData()
{
    return static_cast<ElementRareData&>(ensureRareData());
}

bool Element::hasElementFlagInternal(ElementFlags mask) const
{
    return elementRareData()->hasElementFlag(mask);
}

void Element::setElementFlag(ElementFlags mask, bool value)
{
    if (!hasRareData() && !value)
        return;
    ensureElementRareData().setElementFlag(mask, value);
}

void Element::clearElementFlag(ElementFlags mask)
{
    if (!hasRareData())
        return;
    elementRareData()->clearElementFlag(mask);
}

void Element::clearTabIndexExplicitlyIfNeeded()
{
    if (hasRareData())
        elementRareData()->clearTabIndexExplicitly();
}

void Element::setTabIndexExplicitly(short tabIndex)
{
    ensureElementRareData().setTabIndexExplicitly(tabIndex);
}

void Element::setTabIndex(int value)
{
    setIntegralAttribute(tabindexAttr, value);
}

short Element::tabIndex() const
{
    return hasRareData() ? elementRareData()->tabIndex() : 0;
}

bool Element::layoutObjectIsFocusable() const
{
    // Elements in canvas fallback content are not rendered, but they are allowed to be
    // focusable as long as their canvas is displayed and visible.
    if (isInCanvasSubtree()) {
        const HTMLCanvasElement* canvas = Traversal<HTMLCanvasElement>::firstAncestorOrSelf(*this);
        DCHECK(canvas);
        return canvas->layoutObject() && canvas->layoutObject()->style()->visibility() == VISIBLE;
    }

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Hyatt wants to fix that some day with a "has visible content" flag or the like.
    return layoutObject() && layoutObject()->style()->visibility() == VISIBLE;
}

Node* Element::cloneNode(bool deep)
{
    return deep ? cloneElementWithChildren() : cloneElementWithoutChildren();
}

Element* Element::cloneElementWithChildren()
{
    Element* clone = cloneElementWithoutChildren();
    cloneChildNodes(clone);
    return clone;
}

Element* Element::cloneElementWithoutChildren()
{
    Element* clone = cloneElementWithoutAttributesAndChildren();
    // This will catch HTML elements in the wrong namespace that are not correctly copied.
    // This is a sanity check as HTML overloads some of the DOM methods.
    DCHECK_EQ(isHTMLElement(), clone->isHTMLElement());

    clone->cloneDataFromElement(*this);
    return clone;
}

Element* Element::cloneElementWithoutAttributesAndChildren()
{
    return document().createElement(tagQName(), CreatedByCloneNode);
}

Attr* Element::detachAttribute(size_t index)
{
    DCHECK(elementData());
    const Attribute& attribute = elementData()->attributes().at(index);
    Attr* attrNode = attrIfExists(attribute.name());
    if (attrNode) {
        detachAttrNodeAtIndex(attrNode, index);
    } else {
        attrNode = Attr::create(document(), attribute.name(), attribute.value());
        removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    }
    return attrNode;
}

void Element::detachAttrNodeAtIndex(Attr* attr, size_t index)
{
    DCHECK(attr);
    DCHECK(elementData());

    const Attribute& attribute = elementData()->attributes().at(index);
    DCHECK(attribute.name() == attr->getQualifiedName());
    detachAttrNodeFromElementWithValue(attr, attribute.value());
    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::removeAttribute(const QualifiedName& name)
{
    if (!elementData())
        return;

    size_t index = elementData()->attributes().findIndex(name);
    if (index == kNotFound)
        return;

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::setBooleanAttribute(const QualifiedName& name, bool value)
{
    if (value)
        setAttribute(name, emptyAtom);
    else
        removeAttribute(name);
}

NamedNodeMap* Element::attributesForBindings() const
{
    ElementRareData& rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (NamedNodeMap* attributeMap = rareData.attributeMap())
        return attributeMap;

    rareData.setAttributeMap(NamedNodeMap::create(const_cast<Element*>(this)));
    return rareData.attributeMap();
}

ElementAnimations* Element::elementAnimations() const
{
    if (hasRareData())
        return elementRareData()->elementAnimations();
    return nullptr;
}

ElementAnimations& Element::ensureElementAnimations()
{
    ElementRareData& rareData = ensureElementRareData();
    if (!rareData.elementAnimations())
        rareData.setElementAnimations(new ElementAnimations());
    return *rareData.elementAnimations();
}

bool Element::hasAnimations() const
{
    if (!hasRareData())
        return false;

    ElementAnimations* elementAnimations = elementRareData()->elementAnimations();
    return elementAnimations && !elementAnimations->isEmpty();
}

Node::NodeType Element::getNodeType() const
{
    return ELEMENT_NODE;
}

bool Element::hasAttribute(const QualifiedName& name) const
{
    return hasAttributeNS(name.namespaceURI(), name.localName());
}

void Element::synchronizeAllAttributes() const
{
    if (!elementData())
        return;
    // NOTE: anyAttributeMatches in SelectorChecker.cpp
    // currently assumes that all lazy attributes have a null namespace.
    // If that ever changes we'll need to fix that code.
    if (elementData()->m_styleAttributeIsDirty) {
        DCHECK(isStyledElement());
        synchronizeStyleAttributeInternal();
    }
    if (elementData()->m_animatedSVGAttributesAreDirty) {
        DCHECK(isSVGElement());
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(anyQName());
    }
}

inline void Element::synchronizeAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return;
    if (UNLIKELY(name == styleAttr && elementData()->m_styleAttributeIsDirty)) {
        DCHECK(isStyledElement());
        synchronizeStyleAttributeInternal();
        return;
    }
    if (UNLIKELY(elementData()->m_animatedSVGAttributesAreDirty)) {
        DCHECK(isSVGElement());
        // See comment in the AtomicString version of synchronizeAttribute()
        // also.
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(name);
    }
}

void Element::synchronizeAttribute(const AtomicString& localName) const
{
    // This version of synchronizeAttribute() is streamlined for the case where you don't have a full QualifiedName,
    // e.g when called from DOM API.
    if (!elementData())
        return;
    if (elementData()->m_styleAttributeIsDirty && equalPossiblyIgnoringCase(localName, styleAttr.localName(), shouldIgnoreAttributeCase())) {
        DCHECK(isStyledElement());
        synchronizeStyleAttributeInternal();
        return;
    }
    if (elementData()->m_animatedSVGAttributesAreDirty) {
        // We're not passing a namespace argument on purpose. SVGNames::*Attr are defined w/o namespaces as well.

        // FIXME: this code is called regardless of whether name is an
        // animated SVG Attribute. It would seem we should only call this method
        // if SVGElement::isAnimatableAttribute is true, but the list of
        // animatable attributes in isAnimatableAttribute does not suffice to
        // pass all layout tests. Also, m_animatedSVGAttributesAreDirty stays
        // dirty unless synchronizeAnimatedSVGAttribute is called with
        // anyQName(). This means that even if Element::synchronizeAttribute()
        // is called on all attributes, m_animatedSVGAttributesAreDirty remains
        // true.
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(QualifiedName(nullAtom, localName, nullAtom));
    }
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(name);
    if (const Attribute* attribute = elementData()->attributes().find(name))
        return attribute->value();
    return nullAtom;
}

bool Element::shouldIgnoreAttributeCase() const
{
    return isHTMLElement() && document().isHTMLDocument();
}

void Element::scrollIntoView(bool alignToTop)
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (!layoutObject())
        return;

    bool makeVisibleInVisualViewport = !document().page()->settings().inertVisualViewport();

    LayoutRect bounds = boundingBox();
    // Align to the top / bottom and to the closest edge.
    if (alignToTop)
        layoutObject()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways, ProgrammaticScroll, makeVisibleInVisualViewport);
    else
        layoutObject()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignBottomAlways, ProgrammaticScroll, makeVisibleInVisualViewport);

    document().setSequentialFocusNavigationStartingPoint(this);
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (!layoutObject())
        return;

    bool makeVisibleInVisualViewport = !document().page()->settings().inertVisualViewport();

    LayoutRect bounds = boundingBox();
    if (centerIfNeeded)
        layoutObject()->scrollRectToVisible(bounds, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, ProgrammaticScroll, makeVisibleInVisualViewport);
    else
        layoutObject()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, ProgrammaticScroll, makeVisibleInVisualViewport);
}

void Element::setDistributeScroll(ScrollStateCallback* scrollStateCallback, String nativeScrollBehavior)
{
    scrollStateCallback->setNativeScrollBehavior(ScrollStateCallback::toNativeScrollBehavior(nativeScrollBehavior));
    scrollCustomizationCallbacks().setDistributeScroll(this, scrollStateCallback);
}

void Element::setApplyScroll(ScrollStateCallback* scrollStateCallback, String nativeScrollBehavior)
{
    scrollStateCallback->setNativeScrollBehavior(ScrollStateCallback::toNativeScrollBehavior(nativeScrollBehavior));
    scrollCustomizationCallbacks().setApplyScroll(this, scrollStateCallback);
}

void Element::removeApplyScroll()
{
    scrollCustomizationCallbacks().removeApplyScroll(this);
}

ScrollStateCallback* Element::getApplyScroll()
{
    return scrollCustomizationCallbacks().getApplyScroll(this);
}

void Element::nativeDistributeScroll(ScrollState& scrollState)
{
    if (scrollState.fullyConsumed())
        return;

    scrollState.distributeToScrollChainDescendant();

    // If the scroll doesn't propagate, and we're currently scrolling
    // an element other than this one, prevent the scroll from
    // propagating to this element.
    if (!scrollState.shouldPropagate()
        && scrollState.deltaConsumedForScrollSequence()
        && scrollState.currentNativeScrollingElement() != this) {
        return;
    }

    const double deltaX = scrollState.deltaX();
    const double deltaY = scrollState.deltaY();

    callApplyScroll(scrollState);

    if (deltaX != scrollState.deltaX() || deltaY != scrollState.deltaY())
        scrollState.setCurrentNativeScrollingElement(this);
}

void Element::callDistributeScroll(ScrollState& scrollState)
{
    ScrollStateCallback* callback = scrollCustomizationCallbacks().getDistributeScroll(this);

    // TODO(bokan): Need to add tests before we allow calling custom callbacks
    // for non-touch modalities. For now, just call into the native callback but
    // allow the viewport scroll callback so we don't disable overscroll.
    // crbug.com/623079.
    bool disableCustomCallbacks = !scrollState.isDirectManipulation()
        && !document().isViewportScrollCallback(callback);

    if (!callback || disableCustomCallbacks) {
        nativeDistributeScroll(scrollState);
        return;
    }
    if (callback->nativeScrollBehavior() != WebNativeScrollBehavior::PerformAfterNativeScroll)
        callback->handleEvent(&scrollState);
    if (callback->nativeScrollBehavior() != WebNativeScrollBehavior::DisableNativeScroll)
        nativeDistributeScroll(scrollState);
    if (callback->nativeScrollBehavior() == WebNativeScrollBehavior::PerformAfterNativeScroll)
        callback->handleEvent(&scrollState);
};

void Element::nativeApplyScroll(ScrollState& scrollState)
{
    // All elements in the scroll chain should be boxes.
    DCHECK(!layoutObject() || layoutObject()->isBox());

    if (scrollState.fullyConsumed())
        return;

    FloatSize delta(scrollState.deltaX(), scrollState.deltaY());

    if (delta.isZero())
        return;

    // TODO(esprehn): This should use updateStyleAndLayoutIgnorePendingStylesheetsForNode.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    LayoutBox* boxToScroll = nullptr;

    // We should only ever scroll the effective root scroller this way when the
    // page removes the default applyScroll (ViewportScrollCallback).
    if (document().effectiveRootScroller() == this)
        boxToScroll = document().layoutView();
    else if (layoutObject())
        boxToScroll = toLayoutBox(layoutObject());

    if (!boxToScroll)
        return;

    ScrollResult result =
        LayoutBoxItem(boxToScroll).enclosingBox().scroll(
            ScrollGranularity(static_cast<int>(scrollState.deltaGranularity())),
            delta);

    if (!result.didScroll())
        return;

    // FIXME: Native scrollers should only consume the scroll they
    // apply. See crbug.com/457765.
    scrollState.consumeDeltaNative(delta.width(), delta.height());

    // We need to setCurrentNativeScrollingElement in both the
    // distributeScroll and applyScroll default implementations so
    // that if JS overrides one of these methods, but not the
    // other, this bookkeeping remains accurate.
    scrollState.setCurrentNativeScrollingElement(this);
    if (scrollState.fromUserInput()) {
        if (DocumentLoader* documentLoader = document().loader())
            documentLoader->initialScrollState().wasScrolledByUser = true;
    }
};

void Element::callApplyScroll(ScrollState& scrollState)
{
    ScrollStateCallback* callback = scrollCustomizationCallbacks().getApplyScroll(this);

    // TODO(bokan): Need to add tests before we allow calling custom callbacks
    // for non-touch modalities. For now, just call into the native callback but
    // allow the viewport scroll callback so we don't disable overscroll.
    // crbug.com/623079.
    bool disableCustomCallbacks = !scrollState.isDirectManipulation()
        && !document().isViewportScrollCallback(callback);

    if (!callback || disableCustomCallbacks) {
        nativeApplyScroll(scrollState);
        return;
    }
    if (callback->nativeScrollBehavior() != WebNativeScrollBehavior::PerformAfterNativeScroll)
        callback->handleEvent(&scrollState);
    if (callback->nativeScrollBehavior() != WebNativeScrollBehavior::DisableNativeScroll)
        nativeApplyScroll(scrollState);
    if (callback->nativeScrollBehavior() == WebNativeScrollBehavior::PerformAfterNativeScroll)
        callback->handleEvent(&scrollState);
}

int Element::offsetLeft()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
    if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutObject->pixelSnappedOffsetLeft(offsetParent())), layoutObject->styleRef()).round();
    return 0;
}

int Element::offsetTop()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
    if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutObject->pixelSnappedOffsetTop(offsetParent())), layoutObject->styleRef()).round();
    return 0;
}

int Element::offsetWidth()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
    if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutObject->pixelSnappedOffsetWidth(offsetParent())), layoutObject->styleRef()).round();
    return 0;
}

int Element::offsetHeight()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
    if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutObject->pixelSnappedOffsetHeight(offsetParent())), layoutObject->styleRef()).round();
    return 0;
}

Element* Element::offsetParent()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    LayoutObject* layoutObject = this->layoutObject();
    return layoutObject ? layoutObject->offsetParent() : nullptr;
}

int Element::clientLeft()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (LayoutBox* layoutObject = layoutBox())
        return adjustLayoutUnitForAbsoluteZoom(layoutObject->clientLeft(), layoutObject->styleRef()).round();
    return 0;
}

int Element::clientTop()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (LayoutBox* layoutObject = layoutBox())
        return adjustLayoutUnitForAbsoluteZoom(layoutObject->clientTop(), layoutObject->styleRef()).round();
    return 0;
}

int Element::clientWidth()
{
    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inQuirksMode = document().inQuirksMode();
    if ((!inQuirksMode && document().documentElement() == this)
        || (inQuirksMode && isHTMLElement() && document().body() == this)) {
        if (LayoutViewItem layoutView = LayoutViewItem(document().layoutView())) {
            if (!RuntimeEnabledFeatures::overlayScrollbarsEnabled() || !document().frame()->isLocalRoot())
                document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
            if (document().page()->settings().forceZeroLayoutHeight())
                return adjustLayoutUnitForAbsoluteZoom(layoutView.overflowClipRect(LayoutPoint()).width(), layoutView.styleRef()).round();
            return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutView.layoutSize().width()), layoutView.styleRef()).round();
        }
    }

    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (LayoutBox* layoutObject = layoutBox())
        return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutObject->pixelSnappedClientWidth()), layoutObject->styleRef()).round();
    return 0;
}

int Element::clientHeight()
{
    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inQuirksMode = document().inQuirksMode();

    if ((!inQuirksMode && document().documentElement() == this)
        || (inQuirksMode && isHTMLElement() && document().body() == this)) {
        if (LayoutViewItem layoutView = LayoutViewItem(document().layoutView())) {
            if (!RuntimeEnabledFeatures::overlayScrollbarsEnabled() || !document().frame()->isLocalRoot())
                document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
            if (document().page()->settings().forceZeroLayoutHeight())
                return adjustLayoutUnitForAbsoluteZoom(layoutView.overflowClipRect(LayoutPoint()).height(), layoutView.styleRef()).round();
            return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutView.layoutSize().height()), layoutView.styleRef()).round();
        }
    }

    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (LayoutBox* layoutObject = layoutBox())
        return adjustLayoutUnitForAbsoluteZoom(LayoutUnit(layoutObject->pixelSnappedClientHeight()), layoutObject->styleRef()).round();
    return 0;
}

double Element::scrollLeft()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (document().scrollingElement() == this) {
        if (document().domWindow())
            return document().domWindow()->scrollX();
        return 0;
    }

    if (LayoutBox* box = layoutBox())
        return adjustScrollForAbsoluteZoom(box->scrollLeft(), *box);

    return 0;
}

double Element::scrollTop()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (document().scrollingElement() == this) {
        if (document().domWindow())
            return document().domWindow()->scrollY();
        return 0;
    }

    if (LayoutBox* box = layoutBox())
        return adjustScrollForAbsoluteZoom(box->scrollTop(), *box);

    return 0;
}

void Element::setScrollLeft(double newLeft)
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    newLeft = ScrollableArea::normalizeNonFiniteScroll(newLeft);

    if (document().scrollingElement() == this) {
        if (LocalDOMWindow* window = document().domWindow())
            window->scrollTo(newLeft, window->scrollY());
    } else {
        LayoutBox* box = layoutBox();
        if (box)
            box->setScrollLeft(LayoutUnit::fromFloatRound(newLeft * box->style()->effectiveZoom()));
    }
}

void Element::setScrollTop(double newTop)
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    newTop = ScrollableArea::normalizeNonFiniteScroll(newTop);

    if (document().scrollingElement() == this) {
        if (LocalDOMWindow* window = document().domWindow())
            window->scrollTo(window->scrollX(), newTop);
    } else {
        LayoutBox* box = layoutBox();
        if (box)
            box->setScrollTop(LayoutUnit::fromFloatRound(newTop * box->style()->effectiveZoom()));
    }
}

int Element::scrollWidth()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (document().scrollingElement() == this) {
        if (document().view())
            return adjustForAbsoluteZoom(document().view()->contentsWidth(), document().frame()->pageZoomFactor());
        return 0;
    }

    if (LayoutBox* box = layoutBox())
        return adjustForAbsoluteZoom(box->pixelSnappedScrollWidth(), box);
    return 0;
}

int Element::scrollHeight()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (document().scrollingElement() == this) {
        if (document().view())
            return adjustForAbsoluteZoom(document().view()->contentsHeight(), document().frame()->pageZoomFactor());
        return 0;
    }

    if (LayoutBox* box = layoutBox())
        return adjustForAbsoluteZoom(box->pixelSnappedScrollHeight(), box);
    return 0;
}

void Element::scrollBy(double x, double y)
{
    ScrollToOptions scrollToOptions;
    scrollToOptions.setLeft(x);
    scrollToOptions.setTop(y);
    scrollBy(scrollToOptions);
}

void Element::scrollBy(const ScrollToOptions& scrollToOptions)
{
    // FIXME: This should be removed once scroll updates are processed only after
    // the compositing update. See http://crbug.com/420741.
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (document().scrollingElement() == this) {
        scrollFrameBy(scrollToOptions);
    } else {
        scrollLayoutBoxBy(scrollToOptions);
    }
}

void Element::scrollTo(double x, double y)
{
    ScrollToOptions scrollToOptions;
    scrollToOptions.setLeft(x);
    scrollToOptions.setTop(y);
    scrollTo(scrollToOptions);
}

void Element::scrollTo(const ScrollToOptions& scrollToOptions)
{
    // FIXME: This should be removed once scroll updates are processed only after
    // the compositing update. See http://crbug.com/420741.
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (document().scrollingElement() == this) {
        scrollFrameTo(scrollToOptions);
    } else {
        scrollLayoutBoxTo(scrollToOptions);
    }
}

void Element::scrollLayoutBoxBy(const ScrollToOptions& scrollToOptions)
{
    double left = scrollToOptions.hasLeft() ? ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.left()) : 0.0;
    double top = scrollToOptions.hasTop() ? ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.top()) : 0.0;

    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);
    LayoutBox* box = layoutBox();
    if (box) {
        double currentScaledLeft = box->scrollLeft();
        double currentScaledTop = box->scrollTop();
        double newScaledLeft = left * box->style()->effectiveZoom() + currentScaledLeft;
        double newScaledTop = top * box->style()->effectiveZoom() + currentScaledTop;
        box->scrollToOffset(DoubleSize(newScaledLeft, newScaledTop), scrollBehavior);
    }
}

void Element::scrollLayoutBoxTo(const ScrollToOptions& scrollToOptions)
{
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);

    LayoutBox* box = layoutBox();
    if (box) {
        double scaledLeft = box->scrollLeft();
        double scaledTop = box->scrollTop();
        if (scrollToOptions.hasLeft())
            scaledLeft = ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.left()) * box->style()->effectiveZoom();
        if (scrollToOptions.hasTop())
            scaledTop = ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.top()) * box->style()->effectiveZoom();
        box->scrollToOffset(DoubleSize(scaledLeft, scaledTop), scrollBehavior);
    }
}

void Element::scrollFrameBy(const ScrollToOptions& scrollToOptions)
{
    double left = scrollToOptions.hasLeft() ? ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.left()) : 0.0;
    double top = scrollToOptions.hasTop() ? ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.top()) : 0.0;

    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);
    LocalFrame* frame = document().frame();
    if (!frame)
        return;
    ScrollableArea* viewport = frame->view() ? frame->view()->getScrollableArea() : 0;
    if (!viewport)
        return;

    double newScaledLeft = left * frame->pageZoomFactor() + viewport->scrollPositionDouble().x();
    double newScaledTop = top * frame->pageZoomFactor() + viewport->scrollPositionDouble().y();
    viewport->setScrollPosition(DoublePoint(newScaledLeft, newScaledTop), ProgrammaticScroll, scrollBehavior);
}

void Element::scrollFrameTo(const ScrollToOptions& scrollToOptions)
{
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    ScrollableArea::scrollBehaviorFromString(scrollToOptions.behavior(), scrollBehavior);
    LocalFrame* frame = document().frame();
    if (!frame)
        return;
    ScrollableArea* viewport = frame->view() ? frame->view()->getScrollableArea() : 0;
    if (!viewport)
        return;

    double scaledLeft = viewport->scrollPositionDouble().x();
    double scaledTop = viewport->scrollPositionDouble().y();
    if (scrollToOptions.hasLeft())
        scaledLeft = ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.left()) * frame->pageZoomFactor();
    if (scrollToOptions.hasTop())
        scaledTop = ScrollableArea::normalizeNonFiniteScroll(scrollToOptions.top()) * frame->pageZoomFactor();
    viewport->setScrollPosition(DoublePoint(scaledLeft, scaledTop), ProgrammaticScroll, scrollBehavior);
}

bool Element::hasCompositorProxy() const
{
    return hasRareData() && elementRareData()->proxiedPropertyCounts();
}

void Element::incrementCompositorProxiedProperties(uint32_t mutableProperties)
{
    ElementRareData& rareData = ensureElementRareData();
    if (!rareData.proxiedPropertyCounts())
        setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::CompositorProxy));
    rareData.incrementCompositorProxiedProperties(mutableProperties);
}

void Element::decrementCompositorProxiedProperties(uint32_t mutableProperties)
{
    ElementRareData& rareData = *elementRareData();
    rareData.decrementCompositorProxiedProperties(mutableProperties);
    if (!rareData.proxiedPropertyCounts())
        setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::CompositorProxy));
}

void Element::updateFromCompositorMutation(const CompositorMutation& mutation)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "Element::updateFromCompositorMutation");
    if (mutation.isOpacityMutated() || mutation.isTransformMutated())
        ensureElementAnimations().customCompositorAnimations().applyUpdate(*this, mutation);
}

uint32_t Element::compositorMutableProperties() const
{
    if (!hasRareData())
        return CompositorMutableProperty::kNone;
    if (CompositorProxiedPropertySet* set = elementRareData()->proxiedPropertyCounts())
        return set->proxiedProperties();
    return CompositorMutableProperty::kNone;
}

bool Element::hasNonEmptyLayoutSize() const
{
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    if (LayoutBoxModelObject* box = layoutBoxModelObject())
        return box->hasNonEmptyLayoutSize();
    return false;
}

IntRect Element::boundsInViewport() const
{
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    FrameView* view = document().view();
    if (!view)
        return IntRect();

    Vector<FloatQuad> quads;
    if (isSVGElement() && layoutObject()) {
        // Get the bounding rectangle from the SVG model.
        if (toSVGElement(this)->isSVGGraphicsElement())
            quads.append(layoutObject()->localToAbsoluteQuad(layoutObject()->objectBoundingBox()));
    } else {
        // Get the bounding rectangle from the box model.
        if (layoutBoxModelObject())
            layoutBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return IntRect();

    IntRect result = quads[0].enclosingBoundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].enclosingBoundingBox());

    return view->contentsToViewport(result);
}

ClientRectList* Element::getClientRects()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    LayoutObject* elementLayoutObject = layoutObject();
    if (!elementLayoutObject || (!elementLayoutObject->isBoxModelObject() && !elementLayoutObject->isBR()))
        return ClientRectList::create();

    // FIXME: Handle SVG elements.
    // FIXME: Handle table/inline-table with a caption.

    Vector<FloatQuad> quads;
    elementLayoutObject->absoluteQuads(quads);
    document().adjustFloatQuadsForScrollAndAbsoluteZoom(quads, *elementLayoutObject);
    return ClientRectList::create(quads);
}

ClientRect* Element::getBoundingClientRect()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    Vector<FloatQuad> quads;
    LayoutObject* elementLayoutObject = layoutObject();
    if (elementLayoutObject) {
        if (isSVGElement() && !elementLayoutObject->isSVGRoot()) {
            // Get the bounding rectangle from the SVG model.
            if (toSVGElement(this)->isSVGGraphicsElement())
                quads.append(elementLayoutObject->localToAbsoluteQuad(elementLayoutObject->objectBoundingBox()));
        } else if (elementLayoutObject->isBoxModelObject() || elementLayoutObject->isBR()) {
            elementLayoutObject->absoluteQuads(quads);
        }
    }

    if (quads.isEmpty())
        return ClientRect::create();

    FloatRect result = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].boundingBox());

    DCHECK(elementLayoutObject);
    document().adjustFloatRectForScrollAndAbsoluteZoom(result, *elementLayoutObject);
    return ClientRect::create(result);
}

const AtomicString& Element::computedRole()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
    std::unique_ptr<ScopedAXObjectCache> cache = ScopedAXObjectCache::create(document());
    return cache->get()->computedRoleForNode(this);
}

String Element::computedName()
{
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
    std::unique_ptr<ScopedAXObjectCache> cache = ScopedAXObjectCache::create(document());
    return cache->get()->computedNameForNode(this);
}

const AtomicString& Element::getAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(localName);
    if (const Attribute* attribute = elementData()->attributes().find(localName, shouldIgnoreAttributeCase()))
        return attribute->value();
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

void Element::setAttribute(const AtomicString& localName, const AtomicString& value, ExceptionState& exceptionState)
{
    if (!Document::isValidName(localName)) {
        exceptionState.throwDOMException(InvalidCharacterError, "'" + localName + "' is not a valid attribute name.");
        return;
    }

    synchronizeAttribute(localName);
    const AtomicString& caseAdjustedLocalName = shouldIgnoreAttributeCase() ? localName.lower() : localName;

    if (!elementData()) {
        setAttributeInternal(kNotFound, QualifiedName(nullAtom, caseAdjustedLocalName, nullAtom), value, NotInSynchronizationOfLazyAttribute);
        return;
    }

    AttributeCollection attributes = elementData()->attributes();
    size_t index = attributes.findIndex(caseAdjustedLocalName, false);
    const QualifiedName& qName = index != kNotFound ? attributes[index].name() : QualifiedName(nullAtom, caseAdjustedLocalName, nullAtom);
    setAttributeInternal(index, qName, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    synchronizeAttribute(name);
    size_t index = elementData() ? elementData()->attributes().findIndex(name) : kNotFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomicString& value)
{
    size_t index = elementData() ? elementData()->attributes().findIndex(name) : kNotFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute);
}

ALWAYS_INLINE void Element::setAttributeInternal(size_t index, const QualifiedName& name, const AtomicString& newValue, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (newValue.isNull()) {
        if (index != kNotFound)
            removeAttributeInternal(index, inSynchronizationOfLazyAttribute);
        return;
    }

    if (index == kNotFound) {
        appendAttributeInternal(name, newValue, inSynchronizationOfLazyAttribute);
        return;
    }

    const Attribute& existingAttribute = elementData()->attributes().at(index);
    AtomicString existingAttributeValue = existingAttribute.value();
    QualifiedName existingAttributeName = existingAttribute.name();

    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(existingAttributeName, existingAttributeValue, newValue);
    if (newValue != existingAttributeValue)
        ensureUniqueElementData().attributes().at(index).setValue(newValue);
    if (!inSynchronizationOfLazyAttribute)
        didModifyAttribute(existingAttributeName, existingAttributeValue, newValue);
}

static inline AtomicString makeIdForStyleResolution(const AtomicString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.lowerASCII();
    return value;
}

void Element::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (ElementShadow* parentElementShadow = shadowWhereNodeCanBeDistributed(*this)) {
        if (shouldInvalidateDistributionWhenAttributeChanged(parentElementShadow, name, newValue))
            parentElementShadow->setNeedsDistributionRecalc();
    }
    if (name == HTMLNames::slotAttr && oldValue != newValue) {
        if (ShadowRoot* root = v1ShadowRootOfParent())
            root->ensureSlotAssignment().hostChildSlotNameChanged(oldValue, newValue);
    }

    parseAttribute(name, oldValue, newValue);

    document().incDOMTreeVersion();

    if (name == HTMLNames::idAttr) {
        AtomicString oldId = elementData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
        if (newId != oldId) {
            elementData()->setIdForStyleResolution(newId);
            document().styleEngine().idChangedForElement(oldId, newId, *this);
        }
    } else if (name == classAttr) {
        classAttributeChanged(newValue);
    } else if (name == HTMLNames::nameAttr) {
        setHasName(!newValue.isNull());
    } else if (isStyledElement()) {
        if (name == styleAttr) {
            styleAttributeChanged(newValue, reason);
        } else if (isPresentationAttribute(name)) {
            elementData()->m_presentationAttributeStyleIsDirty = true;
            setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::fromAttribute(name));
        }
    }

    invalidateNodeListCachesInAncestors(&name, this);

    // If there is currently no StyleResolver, we can't be sure that this attribute change won't affect style.
    if (!document().styleResolver())
        setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::fromAttribute(name));

    if (inShadowIncludingDocument()) {
        if (AXObjectCache* cache = document().existingAXObjectCache())
            cache->handleAttributeChanged(name, this);
    }
}

bool Element::hasLegalLinkAttribute(const QualifiedName&) const
{
    return false;
}

const QualifiedName& Element::subResourceAttributeName() const
{
    return QualifiedName::null();
}

inline void Element::attributeChangedFromParserOrByCloning(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (name == isAttr)
        V0CustomElementRegistrationContext::setTypeExtension(this, newValue);
    attributeChanged(name, nullAtom, newValue, reason);
}

template <typename CharacterType>
static inline ClassStringContent classStringHasClassName(const CharacterType* characters, unsigned length)
{
    DCHECK_GT(length, 0u);

    unsigned i = 0;
    do {
        if (isNotHTMLSpace<CharacterType>(characters[i]))
            break;
        ++i;
    } while (i < length);

    if (i == length && length == 1)
        return ClassStringContent::Empty;
    if (i == length && length > 1)
        return ClassStringContent::WhiteSpaceOnly;

    return ClassStringContent::HasClasses;
}

static inline ClassStringContent classStringHasClassName(const AtomicString& newClassString)
{
    unsigned length = newClassString.length();

    if (!length)
        return ClassStringContent::Empty;

    if (newClassString.is8Bit())
        return classStringHasClassName(newClassString.characters8(), length);
    return classStringHasClassName(newClassString.characters16(), length);
}

void Element::classAttributeChanged(const AtomicString& newClassString)
{
    DCHECK(elementData());
    ClassStringContent classStringContentType = classStringHasClassName(newClassString);
    const bool shouldFoldCase = document().inQuirksMode();
    if (classStringContentType == ClassStringContent::HasClasses) {
        const SpaceSplitString oldClasses = elementData()->classNames();
        elementData()->setClass(newClassString, shouldFoldCase);
        const SpaceSplitString& newClasses = elementData()->classNames();
        document().styleEngine().classChangedForElement(oldClasses, newClasses, *this);
    } else {
        const SpaceSplitString& oldClasses = elementData()->classNames();
        document().styleEngine().classChangedForElement(oldClasses, *this);
        if (classStringContentType == ClassStringContent::WhiteSpaceOnly)
            elementData()->setClass(newClassString, shouldFoldCase);
        else
            elementData()->clearClass();
    }

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();
}

bool Element::shouldInvalidateDistributionWhenAttributeChanged(ElementShadow* elementShadow, const QualifiedName& name, const AtomicString& newValue)
{
    DCHECK(elementShadow);
    const SelectRuleFeatureSet& featureSet = elementShadow->ensureSelectFeatureSet();

    if (name == HTMLNames::idAttr) {
        AtomicString oldId = elementData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
        if (newId != oldId) {
            if (!oldId.isEmpty() && featureSet.hasSelectorForId(oldId))
                return true;
            if (!newId.isEmpty() && featureSet.hasSelectorForId(newId))
                return true;
        }
    }

    if (name == HTMLNames::classAttr) {
        const AtomicString& newClassString = newValue;
        if (classStringHasClassName(newClassString) == ClassStringContent::HasClasses) {
            const SpaceSplitString& oldClasses = elementData()->classNames();
            const SpaceSplitString newClasses(newClassString, document().inQuirksMode() ? SpaceSplitString::ShouldFoldCase : SpaceSplitString::ShouldNotFoldCase);
            if (featureSet.checkSelectorsForClassChange(oldClasses, newClasses))
                return true;
        } else {
            const SpaceSplitString& oldClasses = elementData()->classNames();
            if (featureSet.checkSelectorsForClassChange(oldClasses))
                return true;
        }
    }

    return featureSet.hasSelectorForAttribute(name.localName());
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

static inline bool isEventHandlerAttribute(const Attribute& attribute)
{
    return attribute.name().namespaceURI().isNull() && attribute.name().localName().startsWith("on");
}

bool Element::attributeValueIsJavaScriptURL(const Attribute& attribute)
{
    return protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));
}

bool Element::isJavaScriptURLAttribute(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && attributeValueIsJavaScriptURL(attribute);
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    size_t destination = 0;
    for (size_t source = 0; source < attributeVector.size(); ++source) {
        if (isEventHandlerAttribute(attributeVector[source])
            || isJavaScriptURLAttribute(attributeVector[source])
            || isHTMLContentAttribute(attributeVector[source])
            || isSVGAnimationAttributeSettingJavaScriptURL(attributeVector[source]))
            continue;

        if (source != destination)
            attributeVector[destination] = attributeVector[source];

        ++destination;
    }
    attributeVector.shrink(destination);
}

void Element::parserSetAttributes(const Vector<Attribute>& attributeVector)
{
    DCHECK(!inShadowIncludingDocument());
    DCHECK(!parentNode());
    DCHECK(!m_elementData);

    if (!attributeVector.isEmpty()) {
        if (document().elementDataCache())
            m_elementData = document().elementDataCache()->cachedShareableElementDataWithAttributes(attributeVector);
        else
            m_elementData = ShareableElementData::createWithAttributes(attributeVector);
    }

    parserDidSetAttributes();

    // Use attributeVector instead of m_elementData because attributeChanged might modify m_elementData.
    for (const auto& attribute : attributeVector)
        attributeChangedFromParserOrByCloning(attribute.name(), attribute.value(), ModifiedDirectly);
}

bool Element::hasEquivalentAttributes(const Element* other) const
{
    synchronizeAllAttributes();
    other->synchronizeAllAttributes();
    if (elementData() == other->elementData())
        return true;
    if (elementData())
        return elementData()->isEquivalent(other->elementData());
    if (other->elementData())
        return other->elementData()->isEquivalent(elementData());
    return true;
}

String Element::nodeName() const
{
    return m_tagName.toString();
}

const AtomicString& Element::locateNamespacePrefix(const AtomicString& namespaceToLocate) const
{
    if (!prefix().isNull() && namespaceURI() == namespaceToLocate)
        return prefix();

    AttributeCollection attributes = this->attributes();
    for (const Attribute& attr : attributes) {
        if (attr.prefix() == xmlnsAtom && attr.value() == namespaceToLocate)
            return attr.localName();
    }

    if (Element* parent = parentElement())
        return parent->locateNamespacePrefix(namespaceToLocate);

    return nullAtom;
}

const AtomicString Element::imageSourceURL() const
{
    return getAttribute(srcAttr);
}

bool Element::layoutObjectIsNeeded(const ComputedStyle& style)
{
    return style.display() != NONE;
}

LayoutObject* Element::createLayoutObject(const ComputedStyle& style)
{
    return LayoutObject::createObject(this, style);
}

Node::InsertionNotificationRequest Element::insertedInto(ContainerNode* insertionPoint)
{
    // need to do superclass processing first so inShadowIncludingDocument() is true
    // by the time we reach updateId
    ContainerNode::insertedInto(insertionPoint);

    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

    DCHECK(!hasRareData() || !elementRareData()->hasPseudoElements());

    if (!insertionPoint->isInTreeScope())
        return InsertionDone;

    if (hasRareData()) {
        ElementRareData* rareData = elementRareData();
        rareData->clearClassListValueForQuirksMode();
        if (rareData->intersectionObserverData())
            rareData->intersectionObserverData()->activateValidIntersectionObservers(*this);
    }

    if (inShadowIncludingDocument()) {
        if (getCustomElementState() == CustomElementState::Custom)
            CustomElement::enqueueConnectedCallback(this);
        else if (isUpgradedV0CustomElement())
            V0CustomElement::didAttach(this, document());
        else if (getCustomElementState() == CustomElementState::Undefined)
            CustomElement::tryToUpgrade(this);
    }

    TreeScope& scope = insertionPoint->treeScope();
    if (scope != treeScope())
        return InsertionDone;

    const AtomicString& idValue = getIdAttribute();
    if (!idValue.isNull())
        updateId(scope, nullAtom, idValue);

    const AtomicString& nameValue = getNameAttribute();
    if (!nameValue.isNull())
        updateName(nullAtom, nameValue);

    if (parentElement() && parentElement()->isInCanvasSubtree())
        setIsInCanvasSubtree(true);

    return InsertionDone;
}

void Element::removedFrom(ContainerNode* insertionPoint)
{
    bool wasInDocument = insertionPoint->inShadowIncludingDocument();

    DCHECK(!hasRareData() || !elementRareData()->hasPseudoElements());

    if (Fullscreen::isActiveFullScreenElement(*this)) {
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
        if (insertionPoint->isElementNode()) {
            toElement(insertionPoint)->setContainsFullScreenElement(false);
            toElement(insertionPoint)->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
        }
    }

    if (Fullscreen* fullscreen = Fullscreen::fromIfExists(document()))
        fullscreen->elementRemoved(*this);

    if (document().page())
        document().page()->pointerLockController().elementRemoved(this);

    setSavedLayerScrollOffset(IntSize());

    if (insertionPoint->isInTreeScope() && treeScope() == document()) {
        const AtomicString& idValue = getIdAttribute();
        if (!idValue.isNull())
            updateId(insertionPoint->treeScope(), idValue, nullAtom);

        const AtomicString& nameValue = getNameAttribute();
        if (!nameValue.isNull())
            updateName(nameValue, nullAtom);
    }

    ContainerNode::removedFrom(insertionPoint);
    if (wasInDocument) {
        if (this == document().cssTarget())
            document().setCSSTarget(nullptr);

        if (hasPendingResources())
            document().accessSVGExtensions().removeElementFromPendingResources(this);

        if (getCustomElementState() == CustomElementState::Custom)
            CustomElement::enqueueDisconnectedCallback(this);
        else if (isUpgradedV0CustomElement())
            V0CustomElement::didDetach(this, insertionPoint->document());

        if (needsStyleInvalidation())
            document().styleEngine().styleInvalidator().clearInvalidation(*this);
    }

    document().removeFromTopLayer(this);

    clearElementFlag(IsInCanvasSubtree);

    if (hasRareData()) {
        ElementRareData* data = elementRareData();

        data->clearRestyleFlags();

        if (ElementAnimations* elementAnimations = data->elementAnimations())
            elementAnimations->cssAnimations().cancel();

        if (data->intersectionObserverData())
            data->intersectionObserverData()->deactivateAllIntersectionObservers(*this);
    }

    if (document().frame())
        document().frame()->eventHandler().elementRemoved(this);
}

void Element::attach(const AttachContext& context)
{
    DCHECK(document().inStyleRecalc());

    // We've already been through detach when doing an attach, but we might
    // need to clear any state that's been added since then.
    if (hasRareData() && getStyleChangeType() == NeedsReattachStyleChange) {
        ElementRareData* data = elementRareData();
        data->clearComputedStyle();
    }

    if (!isSlotOrActiveInsertionPoint())
        LayoutTreeBuilderForElement(*this, context.resolvedStyle).createLayoutObjectIfNeeded();

    addCallbackSelectors();

    if (hasRareData() && !layoutObject()) {
        if (ElementAnimations* elementAnimations = elementRareData()->elementAnimations()) {
            elementAnimations->cssAnimations().cancel();
            elementAnimations->setAnimationStyleChange(false);
        }
    }

    SelectorFilterParentScope filterScope(*this);
    StyleSharingDepthScope sharingScope(*this);

    createPseudoElementIfNeeded(PseudoIdBefore);

    // When a shadow root exists, it does the work of attaching the children.
    if (ElementShadow* shadow = this->shadow())
        shadow->attach(context);

    ContainerNode::attach(context);

    createPseudoElementIfNeeded(PseudoIdAfter);
    createPseudoElementIfNeeded(PseudoIdBackdrop);

    // We create the first-letter element after the :before, :after and
    // children are attached because the first letter text could come
    // from any of them.
    createPseudoElementIfNeeded(PseudoIdFirstLetter);
}

void Element::detach(const AttachContext& context)
{
    HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
    cancelFocusAppearanceUpdate();
    removeCallbackSelectors();
    if (hasRareData()) {
        ElementRareData* data = elementRareData();
        data->clearPseudoElements();

        // attach() will clear the computed style for us when inside recalcStyle.
        if (!document().inStyleRecalc())
            data->clearComputedStyle();

        if (ElementAnimations* elementAnimations = data->elementAnimations()) {
            if (context.performingReattach) {
                // FIXME: We call detach from within style recalc, so compositingState is not up to date.
                // https://code.google.com/p/chromium/issues/detail?id=339847
                DisableCompositingQueryAsserts disabler;

                // FIXME: restart compositor animations rather than pull back to the main thread
                elementAnimations->restartAnimationOnCompositor();
            } else {
                elementAnimations->cssAnimations().cancel();
                elementAnimations->setAnimationStyleChange(false);
            }
            elementAnimations->clearBaseComputedStyle();
        }

        if (ElementShadow* shadow = data->shadow())
            shadow->detach(context);
    }

    ContainerNode::detach(context);

    if (!context.performingReattach && isUserActionElement()) {
        if (hovered())
            document().hoveredNodeDetached(*this);
        if (inActiveChain())
            document().activeChainNodeDetached(*this);
        document().userActionElements().didDetach(*this);
    }

    if (context.clearInvalidation)
        document().styleEngine().styleInvalidator().clearInvalidation(*this);

    if (svgFilterNeedsLayerUpdate())
        document().unscheduleSVGFilterLayerUpdateHack(*this);

    DCHECK(needsAttach());
}

bool Element::pseudoStyleCacheIsInvalid(const ComputedStyle* currentStyle, ComputedStyle* newStyle)
{
    DCHECK_EQ(currentStyle, computedStyle());
    DCHECK(layoutObject());

    if (!currentStyle)
        return false;

    const PseudoStyleCache* pseudoStyleCache = currentStyle->cachedPseudoStyles();
    if (!pseudoStyleCache)
        return false;

    size_t cacheSize = pseudoStyleCache->size();
    for (size_t i = 0; i < cacheSize; ++i) {
        RefPtr<ComputedStyle> newPseudoStyle;
        RefPtr<ComputedStyle> oldPseudoStyle = pseudoStyleCache->at(i);
        PseudoId pseudoId = oldPseudoStyle->styleType();
        if (pseudoId == PseudoIdFirstLine || pseudoId == PseudoIdFirstLineInherited)
            newPseudoStyle = layoutObject()->uncachedFirstLineStyle(newStyle);
        else
            newPseudoStyle = layoutObject()->getUncachedPseudoStyle(PseudoStyleRequest(pseudoId), newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*oldPseudoStyle != *newPseudoStyle || oldPseudoStyle->font().loadingCustomFonts() != newPseudoStyle->font().loadingCustomFonts()) {
            if (pseudoId < FirstInternalPseudoId)
                newStyle->setHasPseudoStyle(pseudoId);
            newStyle->addCachedPseudoStyle(newPseudoStyle);
            if (pseudoId == PseudoIdFirstLine || pseudoId == PseudoIdFirstLineInherited)
                layoutObject()->firstLineStyleDidChange(*oldPseudoStyle, *newPseudoStyle);
            return true;
        }
    }
    return false;
}

PassRefPtr<ComputedStyle> Element::styleForLayoutObject()
{
    DCHECK(document().inStyleRecalc());

    RefPtr<ComputedStyle> style;

    // FIXME: Instead of clearing updates that may have been added from calls to styleForElement
    // outside recalcStyle, we should just never set them if we're not inside recalcStyle.
    if (ElementAnimations* elementAnimations = this->elementAnimations())
        elementAnimations->cssAnimations().clearPendingUpdate();

    if (hasCustomStyleCallbacks())
        style = customStyleForLayoutObject();
    if (!style)
        style = originalStyleForLayoutObject();
    DCHECK(style);

    // styleForElement() might add active animations so we need to get it again.
    if (ElementAnimations* elementAnimations = this->elementAnimations()) {
        elementAnimations->cssAnimations().maybeApplyPendingUpdate(this);
        elementAnimations->updateAnimationFlags(*style);
    }

    if (style->hasTransform()) {
        if (const StylePropertySet* inlineStyle = this->inlineStyle())
            style->setHasInlineTransform(inlineStyle->hasProperty(CSSPropertyTransform));
    }

    return style.release();
}

PassRefPtr<ComputedStyle> Element::originalStyleForLayoutObject()
{
    DCHECK(document().inStyleRecalc());
    return document().ensureStyleResolver().styleForElement(this);
}

void Element::recalcStyle(StyleRecalcChange change, Text* nextTextSibling)
{
    DCHECK(document().inStyleRecalc());
    DCHECK(!document().lifecycle().inDetach());
    DCHECK(!parentOrShadowHostNode()->needsStyleRecalc());
    DCHECK(inActiveDocument());

    if (hasCustomStyleCallbacks())
        willRecalcStyle(change);

    if (change >= Inherit || needsStyleRecalc()) {
        if (hasRareData()) {
            ElementRareData* data = elementRareData();
            data->clearComputedStyle();

            if (change >= Inherit) {
                if (ElementAnimations* elementAnimations = data->elementAnimations())
                    elementAnimations->setAnimationStyleChange(false);
            }
        }
        if (parentComputedStyle())
            change = recalcOwnStyle(change);
        clearNeedsStyleRecalc();
    }

    // If we reattached we don't need to recalc the style of our descendants anymore.
    if ((change >= UpdatePseudoElements && change < Reattach) || childNeedsStyleRecalc()) {
        SelectorFilterParentScope filterScope(*this);
        StyleSharingDepthScope sharingScope(*this);

        updatePseudoElement(PseudoIdBefore, change);

        if (change > UpdatePseudoElements || childNeedsStyleRecalc()) {
            for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
                if (root->shouldCallRecalcStyle(change))
                    root->recalcStyle(change);
            }
            recalcChildStyle(change);
        }

        updatePseudoElement(PseudoIdAfter, change);
        updatePseudoElement(PseudoIdBackdrop, change);

        // If our children have changed then we need to force the first-letter
        // checks as we don't know if they effected the first letter or not.
        // This can be seen when a child transitions from floating to
        // non-floating we have to take it into account for the first letter.
        updatePseudoElement(PseudoIdFirstLetter, childNeedsStyleRecalc() ? Force : change);

        clearChildNeedsStyleRecalc();
    }

    if (hasCustomStyleCallbacks())
        didRecalcStyle(change);

    if (change == Reattach)
        reattachWhitespaceSiblingsIfNeeded(nextTextSibling);
}

StyleRecalcChange Element::recalcOwnStyle(StyleRecalcChange change)
{
    DCHECK(document().inStyleRecalc());
    DCHECK(!parentOrShadowHostNode()->needsStyleRecalc());
    DCHECK(change >= Inherit || needsStyleRecalc());
    DCHECK(parentComputedStyle());

    RefPtr<ComputedStyle> oldStyle = mutableComputedStyle();
    RefPtr<ComputedStyle> newStyle = styleForLayoutObject();
    DCHECK(newStyle);

    StyleRecalcChange localChange = ComputedStyle::stylePropagationDiff(oldStyle.get(), newStyle.get());
    if (localChange == NoChange) {
        INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), stylesUnchanged, 1);
    } else {
        INCREMENT_STYLE_STATS_COUNTER(document().styleEngine(), stylesChanged, 1);
    }

    if (localChange == Reattach) {
        AttachContext reattachContext;
        reattachContext.resolvedStyle = newStyle.get();
        bool layoutObjectWillChange = needsAttach() || layoutObject();
        reattach(reattachContext);
        if (layoutObjectWillChange || layoutObject())
            return Reattach;
        return ReattachNoLayoutObject;
    }

    DCHECK(oldStyle);

    if (localChange != NoChange)
        updateCallbackSelectors(oldStyle.get(), newStyle.get());

    if (LayoutObject* layoutObject = this->layoutObject()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(oldStyle.get(), newStyle.get()) || svgFilterNeedsLayerUpdate()) {
            layoutObject->setStyle(newStyle.get());
        } else {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            // FIXME: We may be able to remove this hack, see discussion in
            // https://codereview.chromium.org/30453002/
            layoutObject->setStyleInternal(newStyle.get());
        }
    }

    if (getStyleChangeType() >= SubtreeStyleChange)
        return Force;

    if (change > Inherit || localChange > Inherit)
        return max(localChange, change);

    if (localChange < Inherit) {
        if (oldStyle->hasChildDependentFlags()) {
            if (childNeedsStyleRecalc())
                return Inherit;
            newStyle->copyChildDependentFlagsFrom(*oldStyle);
        }
        if (oldStyle->hasPseudoElementStyle() || newStyle->hasPseudoElementStyle())
            return UpdatePseudoElements;
    }

    return localChange;
}

void Element::updateCallbackSelectors(const ComputedStyle* oldStyle, const ComputedStyle* newStyle)
{
    Vector<String> emptyVector;
    const Vector<String>& oldCallbackSelectors = oldStyle ? oldStyle->callbackSelectors() : emptyVector;
    const Vector<String>& newCallbackSelectors = newStyle ? newStyle->callbackSelectors() : emptyVector;
    if (oldCallbackSelectors.isEmpty() && newCallbackSelectors.isEmpty())
        return;
    if (oldCallbackSelectors != newCallbackSelectors)
        CSSSelectorWatch::from(document()).updateSelectorMatches(oldCallbackSelectors, newCallbackSelectors);
}

void Element::addCallbackSelectors()
{
    updateCallbackSelectors(0, computedStyle());
}

void Element::removeCallbackSelectors()
{
    updateCallbackSelectors(computedStyle(), 0);
}

ElementShadow* Element::shadow() const
{
    return hasRareData() ? elementRareData()->shadow() : nullptr;
}

ElementShadow& Element::ensureShadow()
{
    return ensureElementRareData().ensureShadow();
}

void Element::pseudoStateChanged(CSSSelector::PseudoType pseudo)
{
    // We can't schedule invaliation sets from inside style recalc otherwise
    // we'd never process them.
    // TODO(esprehn): Make this an ASSERT and fix places that call into this
    // like HTMLSelectElement.
    if (document().inStyleRecalc())
        return;
    document().styleEngine().pseudoStateChangedForElement(pseudo, *this);
}

void Element::setAnimationStyleChange(bool animationStyleChange)
{
    if (animationStyleChange && document().inStyleRecalc())
        return;
    if (!hasRareData())
        return;
    if (ElementAnimations* elementAnimations = elementRareData()->elementAnimations())
        elementAnimations->setAnimationStyleChange(animationStyleChange);
}

void Element::clearAnimationStyleChange()
{
    if (!hasRareData())
        return;
    if (ElementAnimations* elementAnimations = elementRareData()->elementAnimations())
        elementAnimations->setAnimationStyleChange(false);
}

void Element::setNeedsAnimationStyleRecalc()
{
    if (getStyleChangeType() != NoStyleChange)
        return;

    setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Animation));
    setAnimationStyleChange(true);
}

void Element::setNeedsCompositingUpdate()
{
    if (!document().isActive())
        return;
    LayoutBoxModelObject* layoutObject = layoutBoxModelObject();
    if (!layoutObject)
        return;
    if (!layoutObject->hasLayer())
        return;
    layoutObject->layer()->setNeedsCompositingInputsUpdate();
    // Changes in the return value of requiresAcceleratedCompositing change if
    // the PaintLayer is self-painting.
    layoutObject->layer()->updateSelfPaintingLayer();
}

void Element::setCustomElementDefinition(V0CustomElementDefinition* definition)
{
    if (!hasRareData() && !definition)
        return;
    DCHECK(!customElementDefinition());
    ensureElementRareData().setCustomElementDefinition(definition);
}

V0CustomElementDefinition* Element::customElementDefinition() const
{
    if (hasRareData())
        return elementRareData()->customElementDefinition();
    return nullptr;
}

ShadowRoot* Element::createShadowRoot(const ScriptState* scriptState, ExceptionState& exceptionState)
{
    HostsUsingFeatures::countMainWorldOnly(scriptState, document(), HostsUsingFeatures::Feature::ElementCreateShadowRoot);
    if (ShadowRoot* root = shadowRoot()) {
        if (root->isV1()) {
            exceptionState.throwDOMException(InvalidStateError, "Shadow root cannot be created on a host which already hosts a v1 shadow tree.");
            return nullptr;
        }
        if (root->type() == ShadowRootType::UserAgent) {
            exceptionState.throwDOMException(InvalidStateError, "Shadow root cannot be created on a host which already hosts an user-agent shadow tree.");
            return nullptr;
        }
    } else if (alwaysCreateUserAgentShadowRoot()) {
        exceptionState.throwDOMException(InvalidStateError, "Shadow root cannot be created on a host which already hosts an user-agent shadow tree.");
        return nullptr;
    }
    document().setShadowCascadeOrder(ShadowCascadeOrder::ShadowCascadeV0);

    return createShadowRootInternal(ShadowRootType::V0, exceptionState);
}

ShadowRoot* Element::attachShadow(const ScriptState* scriptState, const ShadowRootInit& shadowRootInitDict, ExceptionState& exceptionState)
{
    DCHECK(RuntimeEnabledFeatures::shadowDOMV1Enabled());

    HostsUsingFeatures::countMainWorldOnly(scriptState, document(), HostsUsingFeatures::Feature::ElementAttachShadow);

    const AtomicString& tagName = localName();
    bool tagNameIsSupported = isV0CustomElement()
        || isCustomElement()
        || tagName == HTMLNames::articleTag
        || tagName == HTMLNames::asideTag
        || tagName == HTMLNames::blockquoteTag
        || tagName == HTMLNames::bodyTag
        || tagName == HTMLNames::divTag
        || tagName == HTMLNames::footerTag
        || tagName == HTMLNames::h1Tag
        || tagName == HTMLNames::h2Tag
        || tagName == HTMLNames::h3Tag
        || tagName == HTMLNames::h4Tag
        || tagName == HTMLNames::h5Tag
        || tagName == HTMLNames::h6Tag
        || tagName == HTMLNames::headerTag
        || tagName == HTMLNames::navTag
        || tagName == HTMLNames::pTag
        || tagName == HTMLNames::sectionTag
        || tagName == HTMLNames::spanTag;
    if (!tagNameIsSupported) {
        exceptionState.throwDOMException(NotSupportedError, "This element does not support attachShadow");
        return nullptr;
    }

    if (shadowRootInitDict.hasMode() && shadowRoot()) {
        exceptionState.throwDOMException(InvalidStateError, "Shadow root cannot be created on a host which already hosts a shadow tree.");
        return nullptr;
    }

    document().setShadowCascadeOrder(ShadowCascadeOrder::ShadowCascadeV1);

    ShadowRootType type = ShadowRootType::V0;
    if (shadowRootInitDict.hasMode())
        type = shadowRootInitDict.mode() == "open" ? ShadowRootType::Open : ShadowRootType::Closed;

    if (type == ShadowRootType::Closed)
        UseCounter::count(document(), UseCounter::ElementAttachShadowClosed);
    else if (type == ShadowRootType::Open)
        UseCounter::count(document(), UseCounter::ElementAttachShadowOpen);

    ShadowRoot* shadowRoot = createShadowRootInternal(type, exceptionState);

    if (shadowRootInitDict.hasDelegatesFocus()) {
        shadowRoot->setDelegatesFocus(shadowRootInitDict.delegatesFocus());
        UseCounter::count(document(), UseCounter::ShadowRootDelegatesFocus);
    }

    return shadowRoot;
}

ShadowRoot* Element::createShadowRootInternal(ShadowRootType type, ExceptionState& exceptionState)
{
    DCHECK(!closedShadowRoot());

    if (alwaysCreateUserAgentShadowRoot())
        ensureUserAgentShadowRoot();

    // Some elements make assumptions about what kind of layoutObjects they allow
    // as children so we can't allow author shadows on them for now.
    if (!areAuthorShadowsAllowed()) {
        exceptionState.throwDOMException(HierarchyRequestError, "Author-created shadow roots are disabled for this element.");
        return nullptr;
    }

    return &ensureShadow().addShadowRoot(*this, type);
}

ShadowRoot* Element::shadowRoot() const
{
    ElementShadow* elementShadow = shadow();
    if (!elementShadow)
        return nullptr;
    return &elementShadow->youngestShadowRoot();
}

ShadowRoot* Element::openShadowRoot() const
{
    ShadowRoot* root = shadowRoot();
    if (!root)
        return nullptr;
    return root->type() == ShadowRootType::V0 || root->type() == ShadowRootType::Open ? root : nullptr;
}

ShadowRoot* Element::closedShadowRoot() const
{
    ShadowRoot* root = shadowRoot();
    if (!root)
        return nullptr;
    return root->type() == ShadowRootType::Closed ? root : nullptr;
}

ShadowRoot* Element::authorShadowRoot() const
{
    ShadowRoot* root = shadowRoot();
    if (!root)
        return nullptr;
    return root->type() != ShadowRootType::UserAgent ? root : nullptr;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    if (ElementShadow* elementShadow = shadow()) {
        if (ShadowRoot* root = elementShadow->oldestShadowRoot()) {
            DCHECK(root->type() == ShadowRootType::UserAgent);
            return root;
        }
    }

    return nullptr;
}

ShadowRoot& Element::ensureUserAgentShadowRoot()
{
    if (ShadowRoot* shadowRoot = userAgentShadowRoot())
        return *shadowRoot;
    ShadowRoot& shadowRoot = ensureShadow().addShadowRoot(*this, ShadowRootType::UserAgent);
    didAddUserAgentShadowRoot(shadowRoot);
    return shadowRoot;
}

bool Element::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case TEXT_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case CDATA_SECTION_NODE:
        return true;
    default:
        break;
    }
    return false;
}

void Element::checkForEmptyStyleChange()
{
    const ComputedStyle* style = computedStyle();

    if (!style && !styleAffectedByEmpty())
        return;
    if (!inActiveDocument())
        return;
    if (!document().styleResolver())
        return;

    if (!style || (styleAffectedByEmpty() && (!style->emptyState() || hasChildren())))
        pseudoStateChanged(CSSSelector::PseudoEmpty);
}

void Element::childrenChanged(const ChildrenChange& change)
{
    ContainerNode::childrenChanged(change);

    checkForEmptyStyleChange();
    if (!change.byParser && change.isChildElementChange())
        checkForSiblingStyleChanges(change.type == ElementRemoved ? SiblingElementRemoved : SiblingElementInserted, change.siblingChanged, change.siblingBeforeChange, change.siblingAfterChange);

    // TODO(hayato): Confirm that we can skip this if a shadow tree is v1.
    if (ElementShadow* shadow = this->shadow())
        shadow->setNeedsDistributionRecalc();
}

void Element::finishParsingChildren()
{
    setIsFinishedParsingChildren(true);
    checkForEmptyStyleChange();
    checkForSiblingStyleChanges(FinishedParsingChildren, nullptr, lastChild(), nullptr);
}

#ifndef NDEBUG
void Element::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;
    String s;

    result.append(nodeName());

    s = getIdAttribute();
    if (s.length() > 0) {
        if (result.length() > 0)
            result.append("; ");
        result.append("id=");
        result.append(s);
    }

    s = getAttribute(classAttr);
    if (s.length() > 0) {
        if (result.length() > 0)
            result.append("; ");
        result.append("class=");
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}
#endif

AttrNodeList* Element::attrNodeList()
{
    return hasRareData() ? elementRareData()->attrNodeList() : nullptr;
}

AttrNodeList& Element::ensureAttrNodeList()
{
    return ensureElementRareData().ensureAttrNodeList();
}

void Element::removeAttrNodeList()
{
    DCHECK(attrNodeList());
    if (hasRareData())
        elementRareData()->removeAttrNodeList();
}

Attr* Element::setAttributeNode(Attr* attrNode, ExceptionState& exceptionState)
{
    Attr* oldAttrNode = attrIfExists(attrNode->getQualifiedName());
    if (oldAttrNode == attrNode)
        return attrNode; // This Attr is already attached to the element.

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode->ownerElement()) {
        exceptionState.throwDOMException(InUseAttributeError, "The node provided is an attribute node that is already an attribute of another Element; attribute nodes must be explicitly cloned.");
        return nullptr;
    }

    if (!isHTMLElement() && attrNode->document().isHTMLDocument() && attrNode->name() != attrNode->name().lower())
        UseCounter::count(document(), UseCounter::NonHTMLElementSetAttributeNodeFromHTMLDocumentNameNotLowercase);

    synchronizeAllAttributes();
    const UniqueElementData& elementData = ensureUniqueElementData();

    AttributeCollection attributes = elementData.attributes();
    size_t index = attributes.findIndex(attrNode->getQualifiedName(), shouldIgnoreAttributeCase());
    AtomicString localName;
    if (index != kNotFound) {
        const Attribute& attr = attributes[index];

        // If the name of the ElementData attribute doesn't
        // (case-sensitively) match that of the Attr node, record it
        // on the Attr so that it can correctly resolve the value on
        // the Element.
        if (!attr.name().matches(attrNode->getQualifiedName()))
            localName = attr.localName();

        if (oldAttrNode) {
            detachAttrNodeFromElementWithValue(oldAttrNode, attr.value());
        } else {
            // FIXME: using attrNode's name rather than the
            // Attribute's for the replaced Attr is compatible with
            // all but Gecko (and, arguably, the DOM Level1 spec text.)
            // Consider switching.
            oldAttrNode = Attr::create(document(), attrNode->getQualifiedName(), attr.value());
        }
    }

    setAttributeInternal(index, attrNode->getQualifiedName(), attrNode->value(), NotInSynchronizationOfLazyAttribute);

    attrNode->attachToElement(this, localName);
    treeScope().adoptIfNeeded(*attrNode);
    ensureAttrNodeList().append(attrNode);

    return oldAttrNode;
}

Attr* Element::setAttributeNodeNS(Attr* attr, ExceptionState& exceptionState)
{
    return setAttributeNode(attr, exceptionState);
}

Attr* Element::removeAttributeNode(Attr* attr, ExceptionState& exceptionState)
{
    if (attr->ownerElement() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node provided is owned by another element.");
        return nullptr;
    }

    DCHECK_EQ(document(), attr->document());

    synchronizeAttribute(attr->getQualifiedName());

    size_t index = elementData()->attributes().findIndex(attr->getQualifiedName());
    if (index == kNotFound) {
        exceptionState.throwDOMException(NotFoundError, "The attribute was not found on this element.");
        return nullptr;
    }

    detachAttrNodeAtIndex(attr, index);
    return attr;
}

void Element::parseAttribute(const QualifiedName& name, const AtomicString&, const AtomicString& value)
{
    if (name == tabindexAttr) {
        int tabindex = 0;
        if (value.isEmpty()) {
            clearTabIndexExplicitlyIfNeeded();
            if (adjustedFocusedElementInTreeScope() == this) {
                // We might want to call blur(), but it's dangerous to dispatch
                // events here.
                document().setNeedsFocusedElementCheck();
            }
        } else if (parseHTMLInteger(value, tabindex)) {
            // Clamp tabindex to the range of 'short' to match Firefox's behavior.
            setTabIndexExplicitly(max(static_cast<int>(std::numeric_limits<short>::min()), std::min(tabindex, static_cast<int>(std::numeric_limits<short>::max()))));
        }
    } else if (name == XMLNames::langAttr) {
        pseudoStateChanged(CSSSelector::PseudoLang);
    }
}

bool Element::parseAttributeName(QualifiedName& out, const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionState& exceptionState)
{
    AtomicString prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, exceptionState))
        return false;
    DCHECK(!exceptionState.hadException());

    QualifiedName qName(prefix, localName, namespaceURI);

    if (!Document::hasValidNamespaceForAttributes(qName)) {
        exceptionState.throwDOMException(NamespaceError, "'" + namespaceURI + "' is an invalid namespace for attributes.");
        return false;
    }

    out = qName;
    return true;
}

void Element::setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionState& exceptionState)
{
    QualifiedName parsedName = anyName;
    if (!parseAttributeName(parsedName, namespaceURI, qualifiedName, exceptionState))
        return;
    setAttribute(parsedName, value);
}

void Element::removeAttributeInternal(size_t index, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    MutableAttributeCollection attributes = ensureUniqueElementData().attributes();
    ASSERT_WITH_SECURITY_IMPLICATION(index < attributes.size());

    QualifiedName name = attributes[index].name();
    AtomicString valueBeingRemoved = attributes[index].value();

    if (!inSynchronizationOfLazyAttribute) {
        if (!valueBeingRemoved.isNull())
            willModifyAttribute(name, valueBeingRemoved, nullAtom);
    }

    if (Attr* attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode, attributes[index].value());

    attributes.remove(index);

    if (!inSynchronizationOfLazyAttribute)
        didRemoveAttribute(name, valueBeingRemoved);
}

void Element::appendAttributeInternal(const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, nullAtom, value);
    ensureUniqueElementData().attributes().append(name, value);
    if (!inSynchronizationOfLazyAttribute)
        didAddAttribute(name, value);
}

void Element::removeAttribute(const AtomicString& name)
{
    if (!elementData())
        return;

    AtomicString localName = shouldIgnoreAttributeCase() ? name.lower() : name;
    size_t index = elementData()->attributes().findIndex(localName, false);
    if (index == kNotFound) {
        if (UNLIKELY(localName == styleAttr) && elementData()->m_styleAttributeIsDirty && isStyledElement())
            removeAllInlineStyleProperties();
        return;
    }

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    removeAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

Attr* Element::getAttributeNode(const AtomicString& localName)
{
    if (!elementData())
        return nullptr;
    synchronizeAttribute(localName);
    const Attribute* attribute = elementData()->attributes().find(localName, shouldIgnoreAttributeCase());
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

Attr* Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (!elementData())
        return nullptr;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    const Attribute* attribute = elementData()->attributes().find(qName);
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

bool Element::hasAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return false;
    synchronizeAttribute(localName);
    return elementData()->attributes().findIndex(shouldIgnoreAttributeCase() ? localName.lower() : localName, false) != kNotFound;
}

bool Element::hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->attributes().find(qName);
}

void Element::focus(const FocusParams& params)
{
    if (!inShadowIncludingDocument())
        return;

    if (document().focusedElement() == this)
        return;

    if (!document().isActive())
        return;

    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
    if (!isFocusable())
        return;

    if (authorShadowRoot() && authorShadowRoot()->delegatesFocus()) {
        if (isShadowIncludingInclusiveAncestorOf(document().focusedElement()))
            return;

        // Slide the focus to its inner node.
        Element* found = document().page()->focusController().findFocusableElementInShadowHost(*this);
        if (found && isShadowIncludingInclusiveAncestorOf(found)) {
            found->focus(FocusParams(SelectionBehaviorOnFocus::Reset, WebFocusTypeForward, nullptr));
            return;
        }
    }

    if (!document().page()->focusController().setFocusedElement(this, document().frame(), params))
        return;

    if (document().focusedElement() == this && UserGestureIndicator::processedUserGestureSinceLoad()) {
        // Bring up the keyboard in the context of anything triggered by a user
        // gesture. Since tracking that across arbitrary boundaries (eg.
        // animations) is difficult, for now we match IE's heuristic and bring
        // up the keyboard if there's been any gesture since load.
        document().page()->chromeClient().showImeIfNeeded();
    }
}

void Element::updateFocusAppearance(SelectionBehaviorOnFocus selectionBehavior)
{
    if (selectionBehavior == SelectionBehaviorOnFocus::None)
        return;
    if (isRootEditableElement()) {
        LocalFrame* frame = document().frame();
        if (!frame)
            return;

        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection().rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this), TextAffinity::Downstream);
        // Passing DoNotSetFocus as this function is called after FocusController::setFocusedElement()
        // and we don't want to change the focus to a new Element.
        frame->selection().setSelection(newSelection,  FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle | FrameSelection::DoNotSetFocus);
        frame->selection().revealSelection();
    } else if (layoutObject() && !layoutObject()->isLayoutPart()) {
        layoutObject()->scrollRectToVisible(boundingBox());
    }
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    if (adjustedFocusedElementInTreeScope() == this) {
        Document& doc = document();
        if (doc.page())
            doc.page()->focusController().setFocusedElement(0, doc.frame());
        else
            doc.clearFocusedElement();
    }
}

bool Element::supportsFocus() const
{
    // FIXME: supportsFocus() can be called when layout is not up to date.
    // Logic that deals with the layoutObject should be moved to layoutObjectIsFocusable().
    // But supportsFocus must return true when the element is editable, or else
    // it won't be focusable. Furthermore, supportsFocus cannot just return true
    // always or else tabIndex() will change for all HTML elements.
    return hasElementFlag(TabIndexWasSetExplicitly)
        || isRootEditableElement()
        || (isShadowHost(this) && authorShadowRoot() && authorShadowRoot()->delegatesFocus())
        || supportsSpatialNavigationFocus();
}

bool Element::supportsSpatialNavigationFocus() const
{
    // This function checks whether the element satisfies the extended criteria
    // for the element to be focusable, introduced by spatial navigation feature,
    // i.e. checks if click or keyboard event handler is specified.
    // This is the way to make it possible to navigate to (focus) elements
    // which web designer meant for being active (made them respond to click events).
    if (!isSpatialNavigationEnabled(document().frame()) || spatialNavigationIgnoresEventHandlers(document().frame()))
        return false;
    if (hasEventListeners(EventTypeNames::click)
        || hasEventListeners(EventTypeNames::keydown)
        || hasEventListeners(EventTypeNames::keypress)
        || hasEventListeners(EventTypeNames::keyup))
        return true;
    if (!isSVGElement())
        return false;
    return (hasEventListeners(EventTypeNames::focus)
        || hasEventListeners(EventTypeNames::blur)
        || hasEventListeners(EventTypeNames::focusin)
        || hasEventListeners(EventTypeNames::focusout));
}

bool Element::isFocusable() const
{
    // Style cannot be cleared out for non-active documents, so in that case the
    // needsLayoutTreeUpdateForNode check is invalid.
    DCHECK(!document().isActive() || !document().needsLayoutTreeUpdateForNode(*this));
    return inShadowIncludingDocument() && supportsFocus() && !isInert() && layoutObjectIsFocusable();
}

bool Element::isKeyboardFocusable() const
{
    return isFocusable() && tabIndex() >= 0;
}

bool Element::isMouseFocusable() const
{
    return isFocusable();
}

bool Element::isFocusedElementInDocument() const
{
    return this == document().focusedElement();
}

Element* Element::adjustedFocusedElementInTreeScope() const
{
    return isInTreeScope() ? containingTreeScope().adjustedFocusedElement() : nullptr;
}

void Element::dispatchFocusEvent(Element* oldFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    dispatchEvent(FocusEvent::create(EventTypeNames::focus, false, false, document().domWindow(), 0, oldFocusedElement, sourceCapabilities));
}

void Element::dispatchBlurEvent(Element* newFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    dispatchEvent(FocusEvent::create(EventTypeNames::blur, false, false, document().domWindow(), 0, newFocusedElement, sourceCapabilities));
}

void Element::dispatchFocusInEvent(const AtomicString& eventType, Element* oldFocusedElement, WebFocusType, InputDeviceCapabilities* sourceCapabilities)
{
#if DCHECK_IS_ON()
    DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif
    DCHECK(eventType == EventTypeNames::focusin || eventType == EventTypeNames::DOMFocusIn);
    dispatchScopedEvent(FocusEvent::create(eventType, true, false, document().domWindow(), 0, oldFocusedElement, sourceCapabilities));
}

void Element::dispatchFocusOutEvent(const AtomicString& eventType, Element* newFocusedElement, InputDeviceCapabilities* sourceCapabilities)
{
#if DCHECK_IS_ON()
    DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif
    DCHECK(eventType == EventTypeNames::focusout || eventType == EventTypeNames::DOMFocusOut);
    dispatchScopedEvent(FocusEvent::create(eventType, true, false, document().domWindow(), 0, newFocusedElement, sourceCapabilities));
}

String Element::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

String Element::outerHTML() const
{
    return createMarkup(this);
}

void Element::setInnerHTML(const String& html, ExceptionState& exceptionState)
{
    InspectorInstrumentation::NativeBreakpoint nativeBreakpoint(&document(), "setInnerHTML", true);
    if (DocumentFragment* fragment = createFragmentForInnerOuterHTML(html, this, AllowScriptingContent, "innerHTML", exceptionState)) {
        ContainerNode* container = this;
        if (isHTMLTemplateElement(*this))
            container = toHTMLTemplateElement(this)->content();
        replaceChildrenWithFragment(container, fragment, exceptionState);
    }
}

void Element::setOuterHTML(const String& html, ExceptionState& exceptionState)
{
    Node* p = parentNode();
    if (!p) {
        exceptionState.throwDOMException(NoModificationAllowedError, "This element has no parent node.");
        return;
    }
    if (!p->isElementNode()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "This element's parent is of type '" + p->nodeName() + "', which is not an element node.");
        return;
    }

    Element* parent = toElement(p);
    Node* prev = previousSibling();
    Node* next = nextSibling();

    DocumentFragment* fragment = createFragmentForInnerOuterHTML(html, parent, AllowScriptingContent, "outerHTML", exceptionState);
    if (exceptionState.hadException())
        return;

    parent->replaceChild(fragment, this, exceptionState);
    Node* node = next ? next->previousSibling() : nullptr;
    if (!exceptionState.hadException() && node && node->isTextNode())
        mergeWithNextTextNode(toText(node), exceptionState);

    if (!exceptionState.hadException() && prev && prev->isTextNode())
        mergeWithNextTextNode(toText(prev), exceptionState);
}

Node* Element::insertAdjacent(const String& where, Node* newChild, ExceptionState& exceptionState)
{
    if (equalIgnoringCase(where, "beforeBegin")) {
        if (ContainerNode* parent = this->parentNode()) {
            parent->insertBefore(newChild, this, exceptionState);
            if (!exceptionState.hadException())
                return newChild;
        }
        return nullptr;
    }

    if (equalIgnoringCase(where, "afterBegin")) {
        insertBefore(newChild, firstChild(), exceptionState);
        return exceptionState.hadException() ? nullptr : newChild;
    }

    if (equalIgnoringCase(where, "beforeEnd")) {
        appendChild(newChild, exceptionState);
        return exceptionState.hadException() ? nullptr : newChild;
    }

    if (equalIgnoringCase(where, "afterEnd")) {
        if (ContainerNode* parent = this->parentNode()) {
            parent->insertBefore(newChild, nextSibling(), exceptionState);
            if (!exceptionState.hadException())
                return newChild;
        }
        return nullptr;
    }

    exceptionState.throwDOMException(SyntaxError, "The value provided ('" + where + "') is not one of 'beforeBegin', 'afterBegin', 'beforeEnd', or 'afterEnd'.");
    return nullptr;
}

NodeIntersectionObserverData* Element::intersectionObserverData() const
{
    if (hasRareData())
        return elementRareData()->intersectionObserverData();
    return nullptr;
}

NodeIntersectionObserverData& Element::ensureIntersectionObserverData()
{
    return ensureElementRareData().ensureIntersectionObserverData();
}

// Step 1 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
static Element* contextElementForInsertion(const String& where, Element* element, ExceptionState& exceptionState)
{
    if (equalIgnoringCase(where, "beforeBegin") || equalIgnoringCase(where, "afterEnd")) {
        Element* parent = element->parentElement();
        if (!parent) {
            exceptionState.throwDOMException(NoModificationAllowedError, "The element has no parent.");
            return nullptr;
        }
        return parent;
    }
    if (equalIgnoringCase(where, "afterBegin") || equalIgnoringCase(where, "beforeEnd"))
        return element;
    exceptionState.throwDOMException(SyntaxError, "The value provided ('" + where + "') is not one of 'beforeBegin', 'afterBegin', 'beforeEnd', or 'afterEnd'.");
    return nullptr;
}

Element* Element::insertAdjacentElement(const String& where, Element* newChild, ExceptionState& exceptionState)
{
    Node* returnValue = insertAdjacent(where, newChild, exceptionState);
    return toElement(returnValue);
}

void Element::insertAdjacentText(const String& where, const String& text, ExceptionState& exceptionState)
{
    insertAdjacent(where, document().createTextNode(text), exceptionState);
}

void Element::insertAdjacentHTML(const String& where, const String& markup, ExceptionState& exceptionState)
{
    Element* contextElement = contextElementForInsertion(where, this, exceptionState);
    if (!contextElement)
        return;

    DocumentFragment* fragment = createFragmentForInnerOuterHTML(markup, contextElement, AllowScriptingContent, "insertAdjacentHTML", exceptionState);
    if (!fragment)
        return;
    insertAdjacent(where, fragment, exceptionState);
}

void Element::setPointerCapture(int pointerId, ExceptionState& exceptionState)
{
    if (document().frame()) {
        if (!document().frame()->eventHandler().isPointerEventActive(pointerId))
            exceptionState.throwDOMException(InvalidPointerId, "InvalidPointerId");
        else if (!inShadowIncludingDocument())
            exceptionState.throwDOMException(InvalidStateError, "InvalidStateError");
        else
            document().frame()->eventHandler().setPointerCapture(pointerId, this);
    }
}

void Element::releasePointerCapture(int pointerId, ExceptionState& exceptionState)
{
    if (document().frame()) {
        if (!document().frame()->eventHandler().isPointerEventActive(pointerId))
            exceptionState.throwDOMException(InvalidPointerId, "InvalidPointerId");
        else
            document().frame()->eventHandler().releasePointerCapture(pointerId, this);
    }
}

String Element::innerText()
{
    // We need to update layout, since plainText uses line boxes in the layout tree.
    document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

    if (!layoutObject())
        return textContent(true);

    return plainText(EphemeralRange::rangeOfContents(*this), TextIteratorForInnerText);
}

String Element::outerText()
{
    // Getting outerText is the same as getting innerText, only
    // setting is different. You would think this should get the plain
    // text for the outer range, but this is wrong, <br> for instance
    // would return different values for inner and outer text by such
    // a rule, but it doesn't in WinIE, and we want to match that.
    return innerText();
}

String Element::textFromChildren()
{
    Text* firstTextNode = 0;
    bool foundMultipleTextNodes = false;
    unsigned totalLength = 0;

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTextNode())
            continue;
        Text* text = toText(child);
        if (!firstTextNode)
            firstTextNode = text;
        else
            foundMultipleTextNodes = true;
        unsigned length = text->data().length();
        if (length > std::numeric_limits<unsigned>::max() - totalLength)
            return emptyString();
        totalLength += length;
    }

    if (!firstTextNode)
        return emptyString();

    if (firstTextNode && !foundMultipleTextNodes) {
        firstTextNode->atomize();
        return firstTextNode->data();
    }

    StringBuilder content;
    content.reserveCapacity(totalLength);
    for (Node* child = firstTextNode; child; child = child->nextSibling()) {
        if (!child->isTextNode())
            continue;
        content.append(toText(child)->data());
    }

    DCHECK_EQ(content.length(), totalLength);
    return content.toString();
}

const AtomicString& Element::shadowPseudoId() const
{
    if (ShadowRoot* root = containingShadowRoot()) {
        if (root->type() == ShadowRootType::UserAgent)
            return fastGetAttribute(pseudoAttr);
    }
    return nullAtom;
}

void Element::setShadowPseudoId(const AtomicString& id)
{
    DCHECK(CSSSelector::parsePseudoType(id, false) == CSSSelector::PseudoWebKitCustomElement || CSSSelector::parsePseudoType(id, false) == CSSSelector::PseudoBlinkInternalElement);
    setAttribute(pseudoAttr, id);
}

bool Element::isInDescendantTreeOf(const Element* shadowHost) const
{
    DCHECK(shadowHost);
    DCHECK(isShadowHost(shadowHost));

    for (const Element* ancestorShadowHost = this->shadowHost(); ancestorShadowHost; ancestorShadowHost = ancestorShadowHost->shadowHost()) {
        if (ancestorShadowHost == shadowHost)
            return true;
    }
    return false;
}

LayoutSize Element::minimumSizeForResizing() const
{
    return hasRareData() ? elementRareData()->minimumSizeForResizing() : defaultMinimumSizeForResizing();
}

void Element::setMinimumSizeForResizing(const LayoutSize& size)
{
    if (!hasRareData() && size == defaultMinimumSizeForResizing())
        return;
    ensureElementRareData().setMinimumSizeForResizing(size);
}

const ComputedStyle* Element::ensureComputedStyle(PseudoId pseudoElementSpecifier)
{
    if (PseudoElement* element = pseudoElement(pseudoElementSpecifier))
        return element->ensureComputedStyle();

    if (!inActiveDocument()) {
        // FIXME: Try to do better than this. Ensure that styleForElement() works for elements that are not in the
        // document tree and figure out when to destroy the computed style for such elements.
        return nullptr;
    }

    // FIXME: Find and use the layoutObject from the pseudo element instead of the actual element so that the 'length'
    // properties, which are only known by the layoutObject because it did the layout, will be correct and so that the
    // values returned for the ":selection" pseudo-element will be correct.
    ComputedStyle* elementStyle = mutableComputedStyle();
    if (!elementStyle) {
        ElementRareData& rareData = ensureElementRareData();
        if (!rareData.ensureComputedStyle())
            rareData.setComputedStyle(document().styleForElementIgnoringPendingStylesheets(this));
        elementStyle = rareData.ensureComputedStyle();
    }

    if (!pseudoElementSpecifier)
        return elementStyle;

    if (ComputedStyle* pseudoElementStyle = elementStyle->getCachedPseudoStyle(pseudoElementSpecifier))
        return pseudoElementStyle;

    RefPtr<ComputedStyle> result = document().ensureStyleResolver().pseudoStyleForElement(this, PseudoStyleRequest(pseudoElementSpecifier, PseudoStyleRequest::ForComputedStyle), elementStyle);
    DCHECK(result);
    return elementStyle->addCachedPseudoStyle(result.release());
}

AtomicString Element::computeInheritedLanguage() const
{
    const Node* n = this;
    AtomicString value;
    // The language property is inherited, so we iterate over the parents to find the first language.
    do {
        if (n->isElementNode()) {
            if (const ElementData* elementData = toElement(n)->elementData()) {
                AttributeCollection attributes = elementData->attributes();
                // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                if (const Attribute* attribute = attributes.find(XMLNames::langAttr))
                    value = attribute->value();
                else if (const Attribute* attribute = attributes.find(HTMLNames::langAttr))
                    value = attribute->value();
            }
        } else if (n->isDocumentNode()) {
            // checking the MIME content-language
            value = toDocument(n)->contentLanguage();
        }

        n = n->parentOrShadowHostNode();
    } while (n && value.isNull());

    return value;
}

Locale& Element::locale() const
{
    return document().getCachedLocale(computeInheritedLanguage());
}

void Element::cancelFocusAppearanceUpdate()
{
    if (document().focusedElement() == this)
        document().cancelFocusAppearanceUpdate();
}

void Element::updatePseudoElement(PseudoId pseudoId, StyleRecalcChange change)
{
    DCHECK(!needsStyleRecalc());
    PseudoElement* element = pseudoElement(pseudoId);

    if (element && (change == UpdatePseudoElements || element->shouldCallRecalcStyle(change))) {
        if (pseudoId == PseudoIdFirstLetter && updateFirstLetter(element))
            return;

        // Need to clear the cached style if the PseudoElement wants a recalc so it
        // computes a new style.
        if (element->needsStyleRecalc())
            layoutObject()->mutableStyle()->removeCachedPseudoStyle(pseudoId);

        // PseudoElement styles hang off their parent element's style so if we needed
        // a style recalc we should Force one on the pseudo.
        // FIXME: We should figure out the right text sibling to pass.
        element->recalcStyle(change == UpdatePseudoElements ? Force : change);

        // Wait until our parent is not displayed or pseudoElementLayoutObjectIsNeeded
        // is false, otherwise we could continuously create and destroy PseudoElements
        // when LayoutObject::isChildAllowed on our parent returns false for the
        // PseudoElement's layoutObject for each style recalc.
        if (!layoutObject() || !pseudoElementLayoutObjectIsNeeded(layoutObject()->getCachedPseudoStyle(pseudoId)))
            elementRareData()->setPseudoElement(pseudoId, nullptr);
    } else if (pseudoId == PseudoIdFirstLetter && element && change >= UpdatePseudoElements && !FirstLetterPseudoElement::firstLetterTextLayoutObject(*element)) {
        // This can happen if we change to a float, for example. We need to cleanup the
        // first-letter pseudoElement and then fix the text of the original remaining
        // text layoutObject.
        // This can be seen in Test 7 of fast/css/first-letter-removed-added.html
        elementRareData()->setPseudoElement(pseudoId, nullptr);
    } else if (change >= UpdatePseudoElements) {
        createPseudoElementIfNeeded(pseudoId);
    }
}

// If we're updating first letter, and the current first letter layoutObject
// is not the same as the one we're currently using we need to re-create
// the first letter layoutObject.
bool Element::updateFirstLetter(Element* element)
{
    LayoutObject* remainingTextLayoutObject = FirstLetterPseudoElement::firstLetterTextLayoutObject(*element);
    if (!remainingTextLayoutObject || remainingTextLayoutObject != toFirstLetterPseudoElement(element)->remainingTextLayoutObject()) {
        // We have to clear out the old first letter here because when it is
        // disposed it will set the original text back on the remaining text
        // layoutObject. If we dispose after creating the new one we will get
        // incorrect results due to setting the first letter back.
        if (remainingTextLayoutObject)
            element->reattach();
        else
            elementRareData()->setPseudoElement(PseudoIdFirstLetter, nullptr);
        return true;
    }
    return false;
}

void Element::createPseudoElementIfNeeded(PseudoId pseudoId)
{
    if (isPseudoElement())
        return;

    // Document::ensureStyleResolver is not inlined and shows up on profiles, avoid it here.
    PseudoElement* element = document().styleEngine().ensureResolver().createPseudoElementIfNeeded(*this, pseudoId);
    if (!element)
        return;

    if (pseudoId == PseudoIdBackdrop)
        document().addToTopLayer(element, this);
    element->insertedInto(this);
    element->attach();

    InspectorInstrumentation::pseudoElementCreated(element);

    ensureElementRareData().setPseudoElement(pseudoId, element);
}

PseudoElement* Element::pseudoElement(PseudoId pseudoId) const
{
    return hasRareData() ? elementRareData()->pseudoElement(pseudoId) : nullptr;
}

LayoutObject* Element::pseudoElementLayoutObject(PseudoId pseudoId) const
{
    if (PseudoElement* element = pseudoElement(pseudoId))
        return element->layoutObject();
    return nullptr;
}

bool Element::matches(const String& selectors, ExceptionState& exceptionState)
{
    SelectorQuery* selectorQuery = document().selectorQueryCache().add(AtomicString(selectors), document(), exceptionState);
    if (!selectorQuery)
        return false;
    return selectorQuery->matches(*this);
}

Element* Element::closest(const String& selectors, ExceptionState& exceptionState)
{
    SelectorQuery* selectorQuery = document().selectorQueryCache().add(AtomicString(selectors), document(), exceptionState);
    if (!selectorQuery)
        return nullptr;
    return selectorQuery->closest(*this);
}

DOMTokenList& Element::classList()
{
    ElementRareData& rareData = ensureElementRareData();
    if (!rareData.classList())
        rareData.setClassList(ClassList::create(this));
    return *rareData.classList();
}

DOMStringMap& Element::dataset()
{
    ElementRareData& rareData = ensureElementRareData();
    if (!rareData.dataset())
        rareData.setDataset(DatasetDOMStringMap::create(this));
    return *rareData.dataset();
}

KURL Element::hrefURL() const
{
    // FIXME: These all have href() or url(), but no common super class. Why doesn't
    // <link> implement URLUtils?
    if (isHTMLAnchorElement(*this) || isHTMLAreaElement(*this) || isHTMLLinkElement(*this))
        return getURLAttribute(hrefAttr);
    if (isSVGAElement(*this))
        return toSVGAElement(*this).legacyHrefURL(document());
    return KURL();
}

KURL Element::getURLAttribute(const QualifiedName& name) const
{
#if DCHECK_IS_ON()
    if (elementData()) {
        if (const Attribute* attribute = attributes().find(name))
            DCHECK(isURLAttribute(*attribute));
    }
#endif
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

KURL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if DCHECK_IS_ON()
    if (elementData()) {
        if (const Attribute* attribute = attributes().find(name))
            DCHECK(isURLAttribute(*attribute));
    }
#endif
    String value = stripLeadingAndTrailingHTMLSpaces(getAttribute(name));
    if (value.isEmpty())
        return KURL();
    return document().completeURL(value);
}

int Element::getIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).toInt();
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    setAttribute(attributeName, AtomicString::number(value));
}

void Element::setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value)
{
    // Range restrictions are enforced for unsigned IDL attributes that
    // reflect content attributes,
    //   http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes
    if (value > 0x7fffffffu)
        value = 0;
    setAttribute(attributeName, AtomicString::number(value));
}

double Element::getFloatingPointAttribute(const QualifiedName& attributeName, double fallbackValue) const
{
    return parseToDoubleForNumberType(getAttribute(attributeName), fallbackValue);
}

void Element::setFloatingPointAttribute(const QualifiedName& attributeName, double value)
{
    setAttribute(attributeName, AtomicString::number(value));
}

void Element::setContainsFullScreenElement(bool flag)
{
    setElementFlag(ContainsFullScreenElement, flag);
    document().styleEngine().ensureFullscreenUAStyle();
    pseudoStateChanged(CSSSelector::PseudoFullScreenAncestor);
}

// Unlike Node::parentNode, this can cross frame boundaries.
static Element* nextAncestorElement(Element* element)
{
    DCHECK(element);
    if (element->parentElement())
        return element->parentElement();

    Frame* frame = element->document().frame();
    if (!frame || !frame->owner())
        return nullptr;

    // Find the next LocalFrame on the ancestor chain, and return the
    // corresponding <iframe> element for the remote child if it exists.
    while (frame->tree().parent() && frame->tree().parent()->isRemoteFrame())
        frame = frame->tree().parent();

    if (frame->owner() && frame->owner()->isLocal())
        return toHTMLFrameOwnerElement(frame->owner());

    return nullptr;
}

void Element::setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool flag)
{
    for (Element* element = nextAncestorElement(this); element; element = nextAncestorElement(element))
        element->setContainsFullScreenElement(flag);
}

void Element::setIsInTopLayer(bool inTopLayer)
{
    if (isInTopLayer() == inTopLayer)
        return;
    setElementFlag(IsInTopLayer, inTopLayer);

    // We must ensure a reattach occurs so the layoutObject is inserted in the correct sibling order under LayoutView according to its
    // top layer position, or in its usual place if not in the top layer.
    lazyReattachIfAttached();
}

void Element::requestPointerLock()
{
    if (document().page())
        document().page()->pointerLockController().requestPointerLock(this);
}

SpellcheckAttributeState Element::spellcheckAttributeState() const
{
    const AtomicString& value = fastGetAttribute(spellcheckAttr);
    if (value == nullAtom)
        return SpellcheckAttributeDefault;
    if (equalIgnoringCase(value, "true") || equalIgnoringCase(value, ""))
        return SpellcheckAttributeTrue;
    if (equalIgnoringCase(value, "false"))
        return SpellcheckAttributeFalse;

    return SpellcheckAttributeDefault;
}

bool Element::isSpellCheckingEnabled() const
{
    for (const Element* element = this; element; element = element->parentOrShadowHostElement()) {
        switch (element->spellcheckAttributeState()) {
        case SpellcheckAttributeTrue:
            return true;
        case SpellcheckAttributeFalse:
            return false;
        case SpellcheckAttributeDefault:
            break;
        }
    }

    return true;
}

#if DCHECK_IS_ON()
bool Element::fastAttributeLookupAllowed(const QualifiedName& name) const
{
    if (name == HTMLNames::styleAttr)
        return false;

    if (isSVGElement())
        return !toSVGElement(this)->isAnimatableAttribute(name);

    return true;
}
#endif

#ifdef DUMP_NODE_STATISTICS
bool Element::hasNamedNodeMap() const
{
    return hasRareData() && elementRareData()->attributeMap();
}
#endif

inline void Element::updateName(const AtomicString& oldName, const AtomicString& newName)
{
    if (!inShadowIncludingDocument() || isInShadowTree())
        return;

    if (oldName == newName)
        return;

    if (shouldRegisterAsNamedItem())
        updateNamedItemRegistration(oldName, newName);
}

inline void Element::updateId(const AtomicString& oldId, const AtomicString& newId)
{
    if (!isInTreeScope())
        return;

    if (oldId == newId)
        return;

    updateId(containingTreeScope(), oldId, newId);
}

inline void Element::updateId(TreeScope& scope, const AtomicString& oldId, const AtomicString& newId)
{
    DCHECK(isInTreeScope());
    DCHECK_NE(oldId, newId);

    if (!oldId.isEmpty())
        scope.removeElementById(oldId, this);
    if (!newId.isEmpty())
        scope.addElementById(newId, this);

    if (shouldRegisterAsExtraNamedItem())
        updateExtraNamedItemRegistration(oldId, newId);
}

void Element::willModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (name == HTMLNames::nameAttr) {
        updateName(oldValue, newValue);
    }

    if (oldValue != newValue) {
        document().styleEngine().attributeChangedForElement(name, *this);
        if (getCustomElementState() == CustomElementState::Custom)
            CustomElement::enqueueAttributeChangedCallback(this, name, oldValue, newValue);
        else if (isUpgradedV0CustomElement())
            V0CustomElement::attributeDidChange(this, name.localName(), oldValue, newValue);
    }

    if (MutationObserverInterestGroup* recipients = MutationObserverInterestGroup::createForAttributesMutation(*this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(this, name, oldValue));

    InspectorInstrumentation::willModifyDOMAttr(this, oldValue, newValue);
}

void Element::didAddAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == HTMLNames::idAttr)
        updateId(nullAtom, value);
    attributeChanged(name, nullAtom, value);
    InspectorInstrumentation::didModifyDOMAttr(this, name, value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (name == HTMLNames::idAttr)
        updateId(oldValue, newValue);
    attributeChanged(name, oldValue, newValue);
    InspectorInstrumentation::didModifyDOMAttr(this, name, newValue);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name, const AtomicString& oldValue)
{
    if (name == HTMLNames::idAttr)
        updateId(oldValue, nullAtom);
    attributeChanged(name, oldValue, nullAtom);
    InspectorInstrumentation::didRemoveDOMAttr(this, name);
    dispatchSubtreeModifiedEvent();
}

static bool needsURLResolutionForInlineStyle(const Element& element, const Document& oldDocument, const Document& newDocument)
{
    if (oldDocument == newDocument)
        return false;
    if (oldDocument.baseURL() == newDocument.baseURL())
        return false;
    const StylePropertySet* style = element.inlineStyle();
    if (!style)
        return false;
    for (unsigned i = 0; i < style->propertyCount(); ++i) {
        // FIXME: Should handle all URL-based properties: CSSImageSetValue, CSSCursorImageValue, etc.
        if (style->propertyAt(i).value()->isImageValue())
            return true;
    }
    return false;
}

static void reResolveURLsInInlineStyle(const Document& document, MutableStylePropertySet& style)
{
    for (unsigned i = 0; i < style.propertyCount(); ++i) {
        StylePropertySet::PropertyReference property = style.propertyAt(i);
        // FIXME: Should handle all URL-based properties: CSSImageSetValue, CSSCursorImageValue, etc.
        if (property.value()->isImageValue())
            toCSSImageValue(property.value())->reResolveURL(document);
    }
}

void Element::didMoveToNewDocument(Document& oldDocument)
{
    Node::didMoveToNewDocument(oldDocument);

    // If the documents differ by quirks mode then they differ by case sensitivity
    // for class and id names so we need to go through the attribute change logic
    // to pick up the new casing in the ElementData.
    if (oldDocument.inQuirksMode() != document().inQuirksMode()) {
        if (hasID())
            setIdAttribute(getIdAttribute());
        if (hasClass())
            setAttribute(HTMLNames::classAttr, getClassAttribute());
    }

    if (needsURLResolutionForInlineStyle(*this, oldDocument, document()))
        reResolveURLsInInlineStyle(document(), ensureMutableInlineStyle());
}

void Element::updateNamedItemRegistration(const AtomicString& oldName, const AtomicString& newName)
{
    if (!document().isHTMLDocument())
        return;

    if (!oldName.isEmpty())
        toHTMLDocument(document()).removeNamedItem(oldName);

    if (!newName.isEmpty())
        toHTMLDocument(document()).addNamedItem(newName);
}

void Element::updateExtraNamedItemRegistration(const AtomicString& oldId, const AtomicString& newId)
{
    if (!document().isHTMLDocument())
        return;

    if (!oldId.isEmpty())
        toHTMLDocument(document()).removeExtraNamedItem(oldId);

    if (!newId.isEmpty())
        toHTMLDocument(document()).addExtraNamedItem(newId);
}

void Element::scheduleSVGFilterLayerUpdateHack()
{
    document().scheduleSVGFilterLayerUpdateHack(*this);
}

IntSize Element::savedLayerScrollOffset() const
{
    return hasRareData() ? elementRareData()->savedLayerScrollOffset() : IntSize();
}

void Element::setSavedLayerScrollOffset(const IntSize& size)
{
    if (size.isZero() && !hasRareData())
        return;
    ensureElementRareData().setSavedLayerScrollOffset(size);
}

Attr* Element::attrIfExists(const QualifiedName& name)
{
    if (AttrNodeList* attrNodeList = this->attrNodeList()) {
        bool shouldIgnoreCase = shouldIgnoreAttributeCase();
        for (const auto& attr : *attrNodeList) {
            if (attr->getQualifiedName().matchesPossiblyIgnoringCase(name, shouldIgnoreCase))
                return attr.get();
        }
    }
    return nullptr;
}

Attr* Element::ensureAttr(const QualifiedName& name)
{
    Attr* attrNode = attrIfExists(name);
    if (!attrNode) {
        attrNode = Attr::create(*this, name);
        treeScope().adoptIfNeeded(*attrNode);
        ensureAttrNodeList().append(attrNode);
    }
    return attrNode;
}

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomicString& value)
{
    DCHECK(attrNodeList());
    attrNode->detachFromElementWithValue(value);

    AttrNodeList* list = attrNodeList();
    size_t index = list->find(attrNode);
    DCHECK_NE(index, kNotFound);
    list->remove(index);
    if (list->isEmpty())
        removeAttrNodeList();
}

void Element::detachAllAttrNodesFromElement()
{
    AttrNodeList* list = this->attrNodeList();
    if (!list)
        return;

    AttributeCollection attributes = elementData()->attributes();
    for (const Attribute& attr : attributes) {
        if (Attr* attrNode = attrIfExists(attr.name()))
            attrNode->detachFromElementWithValue(attr.value());
    }

    removeAttrNodeList();
}

void Element::willRecalcStyle(StyleRecalcChange)
{
    DCHECK(hasCustomStyleCallbacks());
}

void Element::didRecalcStyle(StyleRecalcChange)
{
    DCHECK(hasCustomStyleCallbacks());
}


PassRefPtr<ComputedStyle> Element::customStyleForLayoutObject()
{
    DCHECK(hasCustomStyleCallbacks());
    return nullptr;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasRareData())
        detachAllAttrNodesFromElement();

    other.synchronizeAllAttributes();
    if (!other.m_elementData) {
        m_elementData.clear();
        return;
    }

    const AtomicString& oldID = getIdAttribute();
    const AtomicString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID);

    const AtomicString& oldName = getNameAttribute();
    const AtomicString& newName = other.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        updateName(oldName, newName);

    // Quirks mode makes class and id not case sensitive. We can't share the ElementData
    // if the idForStyleResolution and the className need different casing.
    bool ownerDocumentsHaveDifferentCaseSensitivity = false;
    if (other.hasClass() || other.hasID())
        ownerDocumentsHaveDifferentCaseSensitivity = other.document().inQuirksMode() != document().inQuirksMode();

    // If 'other' has a mutable ElementData, convert it to an immutable one so we can share it between both elements.
    // We can only do this if there are no presentation attributes and sharing the data won't result in different case sensitivity of class or id.
    if (other.m_elementData->isUnique()
        && !ownerDocumentsHaveDifferentCaseSensitivity
        && !other.m_elementData->presentationAttributeStyle())
        const_cast<Element&>(other).m_elementData = toUniqueElementData(other.m_elementData)->makeShareableCopy();

    if (!other.m_elementData->isUnique() && !ownerDocumentsHaveDifferentCaseSensitivity && !needsURLResolutionForInlineStyle(other, other.document(), document()))
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    AttributeCollection attributes = m_elementData->attributes();
    for (const Attribute& attr : attributes)
        attributeChangedFromParserOrByCloning(attr.name(), attr.value(), ModifiedByCloning);
}

void Element::cloneDataFromElement(const Element& other)
{
    cloneAttributesFromElement(other);
    copyNonAttributePropertiesFromElement(other);
}

void Element::createUniqueElementData()
{
    if (!m_elementData) {
        m_elementData = UniqueElementData::create();
    } else {
        DCHECK(!m_elementData->isUnique());
        m_elementData = toShareableElementData(m_elementData)->makeUniqueCopy();
    }
}

void Element::synchronizeStyleAttributeInternal() const
{
    DCHECK(isStyledElement());
    DCHECK(elementData());
    DCHECK(elementData()->m_styleAttributeIsDirty);
    elementData()->m_styleAttributeIsDirty = false;
    const StylePropertySet* inlineStyle = this->inlineStyle();
    const_cast<Element*>(this)->setSynchronizedLazyAttribute(styleAttr,
        inlineStyle ? AtomicString(inlineStyle->asText()) : nullAtom);
}

CSSStyleDeclaration* Element::style()
{
    if (!isStyledElement())
        return nullptr;
    return &ensureElementRareData().ensureInlineCSSStyleDeclaration(this);
}

StylePropertyMap* Element::styleMap()
{
    if (!isStyledElement())
        return nullptr;
    return &ensureElementRareData().ensureInlineStylePropertyMap(this);
}

MutableStylePropertySet& Element::ensureMutableInlineStyle()
{
    DCHECK(isStyledElement());
    Member<StylePropertySet>& inlineStyle = ensureUniqueElementData().m_inlineStyle;
    if (!inlineStyle) {
        CSSParserMode mode = (!isHTMLElement() || document().inQuirksMode()) ? HTMLQuirksMode : HTMLStandardMode;
        inlineStyle = MutableStylePropertySet::create(mode);
    } else if (!inlineStyle->isMutable()) {
        inlineStyle = inlineStyle->mutableCopy();
    }
    return *toMutableStylePropertySet(inlineStyle);
}

void Element::clearMutableInlineStyleIfEmpty()
{
    if (ensureMutableInlineStyle().isEmpty()) {
        ensureUniqueElementData().m_inlineStyle.clear();
    }
}

inline void Element::setInlineStyleFromString(const AtomicString& newStyleString)
{
    DCHECK(isStyledElement());
    Member<StylePropertySet>& inlineStyle = elementData()->m_inlineStyle;

    // Avoid redundant work if we're using shared attribute data with already parsed inline style.
    if (inlineStyle && !elementData()->isUnique())
        return;

    // We reconstruct the property set instead of mutating if there is no CSSOM wrapper.
    // This makes wrapperless property sets immutable and so cacheable.
    if (inlineStyle && !inlineStyle->isMutable())
        inlineStyle.clear();

    if (!inlineStyle) {
        inlineStyle = CSSParser::parseInlineStyleDeclaration(newStyleString, this);
    } else {
        DCHECK(inlineStyle->isMutable());
        static_cast<MutableStylePropertySet*>(inlineStyle.get())->parseDeclarationList(newStyleString, document().elementSheet().contents());
    }
}

void Element::styleAttributeChanged(const AtomicString& newStyleString, AttributeModificationReason modificationReason)
{
    DCHECK(isStyledElement());
    WTF::OrdinalNumber startLineNumber = WTF::OrdinalNumber::beforeFirst();
    if (document().scriptableDocumentParser() && !document().isInDocumentWrite())
        startLineNumber = document().scriptableDocumentParser()->lineNumber();

    if (newStyleString.isNull()) {
        ensureUniqueElementData().m_inlineStyle.clear();
    } else if (modificationReason == ModifiedByCloning || ContentSecurityPolicy::shouldBypassMainWorld(&document()) || document().contentSecurityPolicy()->allowInlineStyle(document().url(), String(), startLineNumber, newStyleString)) {
        setInlineStyleFromString(newStyleString);
    }

    elementData()->m_styleAttributeIsDirty = false;

    setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::StyleSheetChange));
    InspectorInstrumentation::didInvalidateStyleAttr(this);
}

void Element::inlineStyleChanged()
{
    DCHECK(isStyledElement());
    setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Inline));
    DCHECK(elementData());
    elementData()->m_styleAttributeIsDirty = true;
    InspectorInstrumentation::didInvalidateStyleAttr(this);
}

void Element::setInlineStyleProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    setInlineStyleProperty(propertyID, CSSPrimitiveValue::createIdentifier(identifier), important);
}

void Element::setInlineStyleProperty(CSSPropertyID propertyID, double value, CSSPrimitiveValue::UnitType unit, bool important)
{
    setInlineStyleProperty(propertyID, CSSPrimitiveValue::create(value, unit), important);
}

void Element::setInlineStyleProperty(CSSPropertyID propertyID, CSSValue* value, bool important)
{
    DCHECK(isStyledElement());
    ensureMutableInlineStyle().setProperty(propertyID, value, important);
    inlineStyleChanged();
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, const String& value, bool important)
{
    DCHECK(isStyledElement());
    bool changes = ensureMutableInlineStyle().setProperty(propertyID, value, important, document().elementSheet().contents());
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool Element::removeInlineStyleProperty(CSSPropertyID propertyID)
{
    DCHECK(isStyledElement());
    if (!inlineStyle())
        return false;
    bool changes = ensureMutableInlineStyle().removeProperty(propertyID);
    if (changes)
        inlineStyleChanged();
    return changes;
}

void Element::removeAllInlineStyleProperties()
{
    DCHECK(isStyledElement());
    if (!inlineStyle())
        return;
    ensureMutableInlineStyle().clear();
    inlineStyleChanged();
}

void Element::updatePresentationAttributeStyle()
{
    synchronizeAllAttributes();
    // ShareableElementData doesn't store presentation attribute style, so make sure we have a UniqueElementData.
    UniqueElementData& elementData = ensureUniqueElementData();
    elementData.m_presentationAttributeStyleIsDirty = false;
    elementData.m_presentationAttributeStyle = computePresentationAttributeStyle(*this);
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, CSSValueID identifier)
{
    DCHECK(isStyledElement());
    style->setProperty(propertyID, CSSPrimitiveValue::createIdentifier(identifier));
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, double value, CSSPrimitiveValue::UnitType unit)
{
    DCHECK(isStyledElement());
    style->setProperty(propertyID, CSSPrimitiveValue::create(value, unit));
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, const String& value)
{
    DCHECK(isStyledElement());
    style->setProperty(propertyID, value, false);
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet*  style, CSSPropertyID propertyID, CSSValue* value)
{
    DCHECK(isStyledElement());
    style->setProperty(propertyID, value);
}

bool Element::supportsStyleSharing() const
{
    if (!isStyledElement() || !parentOrShadowHostElement())
        return false;
    // If the element has inline style it is probably unique.
    if (inlineStyle())
        return false;
    if (isSVGElement() && toSVGElement(this)->animatedSMILStyleProperties())
        return false;
    // Ids stop style sharing if they show up in the stylesheets.
    if (hasID() && document().ensureStyleResolver().hasRulesForId(idForStyleResolution()))
        return false;
    // :active and :hover elements always make a chain towards the document node
    // and no siblings or cousins will have the same state. There's also only one
    // :focus element per scope so we don't need to attempt to share.
    if (isUserActionElement())
        return false;
    if (!parentOrShadowHostElement()->childrenSupportStyleSharing())
        return false;
    if (this == document().cssTarget())
        return false;
    if (isHTMLElement() && toHTMLElement(this)->hasDirectionAuto())
        return false;
    // TODO(kochi): This prevents any slotted elements from sharing styles.
    // Investigate cases where we share styles to optimize styling performance.
    if (isChildOfV1ShadowHost())
        return false;
    if (hasAnimations())
        return false;
    if (Fullscreen::isActiveFullScreenElement(*this))
        return false;
    return true;
}

void Element::logAddElementIfIsolatedWorldAndInDocument(const char element[], const QualifiedName& attr1)
{
    if (!inShadowIncludingDocument())
        return;
    V8DOMActivityLogger* activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
    if (!activityLogger)
        return;
    Vector<String, 2> argv;
    argv.append(element);
    argv.append(fastGetAttribute(attr1));
    activityLogger->logEvent("blinkAddElement", argv.size(), argv.data());
}

void Element::logAddElementIfIsolatedWorldAndInDocument(const char element[], const QualifiedName& attr1, const QualifiedName& attr2)
{
    if (!inShadowIncludingDocument())
        return;
    V8DOMActivityLogger* activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
    if (!activityLogger)
        return;
    Vector<String, 3> argv;
    argv.append(element);
    argv.append(fastGetAttribute(attr1));
    argv.append(fastGetAttribute(attr2));
    activityLogger->logEvent("blinkAddElement", argv.size(), argv.data());
}

void Element::logAddElementIfIsolatedWorldAndInDocument(const char element[], const QualifiedName& attr1, const QualifiedName& attr2, const QualifiedName& attr3)
{
    if (!inShadowIncludingDocument())
        return;
    V8DOMActivityLogger* activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
    if (!activityLogger)
        return;
    Vector<String, 4> argv;
    argv.append(element);
    argv.append(fastGetAttribute(attr1));
    argv.append(fastGetAttribute(attr2));
    argv.append(fastGetAttribute(attr3));
    activityLogger->logEvent("blinkAddElement", argv.size(), argv.data());
}

void Element::logUpdateAttributeIfIsolatedWorldAndInDocument(const char element[], const QualifiedName& attributeName, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (!inShadowIncludingDocument())
        return;
    V8DOMActivityLogger* activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();
    if (!activityLogger)
        return;
    Vector<String, 4> argv;
    argv.append(element);
    argv.append(attributeName.toString());
    argv.append(oldValue);
    argv.append(newValue);
    activityLogger->logEvent("blinkSetAttribute", argv.size(), argv.data());
}

DEFINE_TRACE(Element)
{
    if (hasRareData())
        visitor->trace(elementRareData());
    visitor->trace(m_elementData);
    ContainerNode::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(Element)
{
    if (hasRareData()) {
        visitor->traceWrappers(elementRareData());
    }
    ContainerNode::traceWrappers(visitor);
}

} // namespace blink
