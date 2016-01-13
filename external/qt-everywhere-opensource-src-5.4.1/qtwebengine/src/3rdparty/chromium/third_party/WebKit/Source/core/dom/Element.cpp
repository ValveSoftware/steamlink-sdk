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

#include "config.h"
#include "core/dom/Element.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/CSSValueKeywords.h"
#include "core/SVGNames.h"
#include "core/XMLNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSValuePool.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverParentScope.h"
#include "core/dom/Attr.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/DatasetDOMStringMap.h"
#include "core/dom/ElementDataCache.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/MutationObserverInterestGroup.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/NamedNodeMap.h"
#include "core/dom/NoEventDispatchAssertion.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/PresentationAttributeStyle.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/RenderTreeBuilder.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/Text.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementRegistrationContext.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/TextIterator.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/events/EventDispatcher.h"
#include "core/events/FocusEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/ClassList.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormControlsCollection.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLOptionsCollection.h"
#include "core/html/HTMLTableRowsCollection.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scroll/ScrollableArea.h"
#include "wtf/BitVector.h"
#include "wtf/HashFunctions.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

namespace WebCore {

using namespace HTMLNames;
using namespace XMLNames;

typedef WillBeHeapVector<RefPtrWillBeMember<Attr> > AttrNodeList;

static Attr* findAttrNodeInList(const AttrNodeList& attrNodeList, const QualifiedName& name)
{
    AttrNodeList::const_iterator end = attrNodeList.end();
    for (AttrNodeList::const_iterator it = attrNodeList.begin(); it != end; ++it) {
        if ((*it)->qualifiedName() == name)
            return it->get();
    }
    return 0;
}

PassRefPtrWillBeRawPtr<Element> Element::create(const QualifiedName& tagName, Document* document)
{
    return adoptRefWillBeNoop(new Element(tagName, document, CreateElement));
}

Element::Element(const QualifiedName& tagName, Document* document, ConstructionType type)
    : ContainerNode(document, type)
    , m_tagName(tagName)
{
    ScriptWrappable::init(this);
}

Element::~Element()
{
    ASSERT(needsAttach());

#if !ENABLE(OILPAN)
    if (hasRareData())
        elementRareData()->clearShadow();

    if (isCustomElement())
        CustomElement::wasDestroyed(this);

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    // With Oilpan, either the Element has been removed from the Document
    // or the Document is dead as well. If the Element has been removed from
    // the Document the element has already been removed from the pending
    // resources. If the document is also dead, there is no need to remove
    // the element from the pending resources.
    if (hasPendingResources()) {
        document().accessSVGExtensions().removeElementFromPendingResources(this);
        ASSERT(!hasPendingResources());
    }
#endif
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT(hasRareData());
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

bool Element::rendererIsFocusable() const
{
    // Elements in canvas fallback content are not rendered, but they are allowed to be
    // focusable as long as their canvas is displayed and visible.
    if (isInCanvasSubtree()) {
        const Element* e = this;
        while (e && !e->hasLocalName(canvasTag))
            e = e->parentElement();
        ASSERT(e);
        return e->renderer() && e->renderer()->style()->visibility() == VISIBLE;
    }

    // FIXME: These asserts should be in Node::isFocusable, but there are some
    // callsites like Document::setFocusedElement that would currently fail on
    // them. See crbug.com/251163
    if (!renderer()) {
        // We can't just use needsStyleRecalc() because if the node is in a
        // display:none tree it might say it needs style recalc but the whole
        // document is actually up to date.
        ASSERT(!document().childNeedsStyleRecalc());
    }

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Hyatt wants to fix that some day with a "has visible content" flag or the like.
    if (!renderer() || renderer()->style()->visibility() != VISIBLE)
        return false;

    return true;
}

PassRefPtrWillBeRawPtr<Node> Element::cloneNode(bool deep)
{
    return deep ? cloneElementWithChildren() : cloneElementWithoutChildren();
}

PassRefPtrWillBeRawPtr<Element> Element::cloneElementWithChildren()
{
    RefPtrWillBeRawPtr<Element> clone = cloneElementWithoutChildren();
    cloneChildNodes(clone.get());
    return clone.release();
}

PassRefPtrWillBeRawPtr<Element> Element::cloneElementWithoutChildren()
{
    RefPtrWillBeRawPtr<Element> clone = cloneElementWithoutAttributesAndChildren();
    // This will catch HTML elements in the wrong namespace that are not correctly copied.
    // This is a sanity check as HTML overloads some of the DOM methods.
    ASSERT(isHTMLElement() == clone->isHTMLElement());

    clone->cloneDataFromElement(*this);
    return clone.release();
}

PassRefPtrWillBeRawPtr<Element> Element::cloneElementWithoutAttributesAndChildren()
{
    return document().createElement(tagQName(), false);
}

PassRefPtrWillBeRawPtr<Attr> Element::detachAttribute(size_t index)
{
    ASSERT(elementData());
    const Attribute& attribute = elementData()->attributeAt(index);
    RefPtrWillBeRawPtr<Attr> attrNode = attrIfExists(attribute.name());
    if (attrNode)
        detachAttrNodeAtIndex(attrNode.get(), index);
    else {
        attrNode = Attr::create(document(), attribute.name(), attribute.value());
        removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    }
    return attrNode.release();
}

void Element::detachAttrNodeAtIndex(Attr* attr, size_t index)
{
    ASSERT(attr);
    ASSERT(elementData());

    const Attribute& attribute = elementData()->attributeAt(index);
    ASSERT(attribute.name() == attr->qualifiedName());
    detachAttrNodeFromElementWithValue(attr, attribute.value());
    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::removeAttribute(const QualifiedName& name)
{
    if (!elementData())
        return;

    size_t index = elementData()->findAttributeIndexByName(name);
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

ActiveAnimations* Element::activeAnimations() const
{
    if (hasRareData())
        return elementRareData()->activeAnimations();
    return 0;
}

ActiveAnimations& Element::ensureActiveAnimations()
{
    ElementRareData& rareData = ensureElementRareData();
    if (!rareData.activeAnimations())
        rareData.setActiveAnimations(adoptPtrWillBeNoop(new ActiveAnimations()));
    return *rareData.activeAnimations();
}

bool Element::hasActiveAnimations() const
{
    if (!hasRareData())
        return false;

    ActiveAnimations* activeAnimations = elementRareData()->activeAnimations();
    return activeAnimations && !activeAnimations->isEmpty();
}

Node::NodeType Element::nodeType() const
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
        ASSERT(isStyledElement());
        synchronizeStyleAttributeInternal();
    }
    if (elementData()->m_animatedSVGAttributesAreDirty) {
        ASSERT(isSVGElement());
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(anyQName());
    }
}

inline void Element::synchronizeAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return;
    if (UNLIKELY(name == styleAttr && elementData()->m_styleAttributeIsDirty)) {
        ASSERT(isStyledElement());
        synchronizeStyleAttributeInternal();
        return;
    }
    if (UNLIKELY(elementData()->m_animatedSVGAttributesAreDirty)) {
        ASSERT(isSVGElement());
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
        ASSERT(isStyledElement());
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
    if (const Attribute* attribute = findAttributeByName(name))
        return attribute->value();
    return nullAtom;
}

bool Element::shouldIgnoreAttributeCase() const
{
    return isHTMLElement() && document().isHTMLDocument();
}

void Element::scrollIntoView(bool alignToTop)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    // Align to the top / bottom and to the closest edge.
    if (alignToTop)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignBottomAlways);
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    if (centerIfNeeded)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

void Element::scrollByUnits(int units, ScrollGranularity granularity)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    if (!renderer()->hasOverflowClip())
        return;

    ScrollDirection direction = ScrollDown;
    if (units < 0) {
        direction = ScrollUp;
        units = -units;
    }
    toRenderBox(renderer())->scroll(direction, granularity, units);
}

void Element::scrollByLines(int lines)
{
    scrollByUnits(lines, ScrollByLine);
}

void Element::scrollByPages(int pages)
{
    scrollByUnits(pages, ScrollByPage);
}

static float localZoomForRenderer(RenderObject& renderer)
{
    // FIXME: This does the wrong thing if two opposing zooms are in effect and canceled each
    // other out, but the alternative is that we'd have to crawl up the whole render tree every
    // time (or store an additional bit in the RenderStyle to indicate that a zoom was specified).
    float zoomFactor = 1;
    if (renderer.style()->effectiveZoom() != 1) {
        // Need to find the nearest enclosing RenderObject that set up
        // a differing zoom, and then we divide our result by it to eliminate the zoom.
        RenderObject* prev = &renderer;
        for (RenderObject* curr = prev->parent(); curr; curr = curr->parent()) {
            if (curr->style()->effectiveZoom() != prev->style()->effectiveZoom()) {
                zoomFactor = prev->style()->zoom();
                break;
            }
            prev = curr;
        }
        if (prev->isRenderView())
            zoomFactor = prev->style()->zoom();
    }
    return zoomFactor;
}

static double adjustForLocalZoom(LayoutUnit value, RenderObject& renderer)
{
    float zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1)
        return value.toDouble();
    return value.toDouble() / zoomFactor;
}

int Element::offsetLeft()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return lroundf(adjustForLocalZoom(renderer->offsetLeft(), *renderer));
    return 0;
}

int Element::offsetTop()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return lroundf(adjustForLocalZoom(renderer->pixelSnappedOffsetTop(), *renderer));
    return 0;
}

int Element::offsetWidth()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetWidth(), *renderer).round();
    return 0;
}

int Element::offsetHeight()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetHeight(), *renderer).round();
    return 0;
}

Element* Element::offsetParentForBindings()
{
    Element* element = offsetParent();
    if (!element || !element->isInShadowTree())
        return element;
    return element->containingShadowRoot()->shouldExposeToBindings() ? element : 0;
}

Element* Element::offsetParent()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderObject* renderer = this->renderer())
        return renderer->offsetParent();
    return 0;
}

int Element::clientLeft()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(roundToInt(renderer->clientLeft()), *renderer);
    return 0;
}

int Element::clientTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(roundToInt(renderer->clientTop()), *renderer);
    return 0;
}

int Element::clientWidth()
{
    document().updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inQuirksMode = document().inQuirksMode();
    if ((!inQuirksMode && document().documentElement() == this)
        || (inQuirksMode && isHTMLElement() && document().body() == this)) {
        if (FrameView* view = document().view()) {
            if (RenderView* renderView = document().renderView())
                return adjustLayoutUnitForAbsoluteZoom(view->layoutSize().width(), *renderView);
        }
    }

    if (RenderBox* renderer = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientWidth(), *renderer).round();
    return 0;
}

int Element::clientHeight()
{
    document().updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inQuirksMode = document().inQuirksMode();

    if ((!inQuirksMode && document().documentElement() == this)
        || (inQuirksMode && isHTMLElement() && document().body() == this)) {
        if (FrameView* view = document().view()) {
            if (RenderView* renderView = document().renderView())
                return adjustLayoutUnitForAbsoluteZoom(view->layoutSize().height(), *renderView);
        }
    }

    if (RenderBox* renderer = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientHeight(), *renderer).round();
    return 0;
}

int Element::scrollLeft()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (document().documentElement() != this) {
        if (RenderBox* rend = renderBox())
            return adjustDoubleForAbsoluteZoom(rend->scrollLeft(), *rend);
        return 0;
    }

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        if (document().inQuirksMode())
            return 0;

        if (FrameView* view = document().view()) {
            if (RenderView* renderView = document().renderView())
                return adjustDoubleForAbsoluteZoom(view->scrollX(), *renderView);
        }
    }

    return 0;
}

int Element::scrollTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (document().documentElement() != this) {
        if (RenderBox* rend = renderBox())
            return adjustLayoutUnitForAbsoluteZoom(rend->scrollTop(), *rend);
        return 0;
    }

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        if (document().inQuirksMode())
            return 0;

        if (FrameView* view = document().view()) {
            if (RenderView* renderView = document().renderView())
                return adjustDoubleForAbsoluteZoom(view->scrollY(), *renderView);
        }
    }

    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (document().documentElement() != this) {
        if (RenderBox* rend = renderBox())
            rend->setScrollLeft(LayoutUnit::fromFloatRound(newLeft * rend->style()->effectiveZoom()));
        return;
    }

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        if (document().inQuirksMode())
            return;

        LocalFrame* frame = document().frame();
        if (!frame)
            return;
        FrameView* view = frame->view();
        if (!view)
            return;

        view->setScrollPosition(IntPoint(roundf(newLeft * frame->pageZoomFactor()), view->scrollY()));
    }
}

void Element::setScrollLeft(const Dictionary& scrollOptionsHorizontal, ExceptionState& exceptionState)
{
    String scrollBehaviorString;
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    if (scrollOptionsHorizontal.get("behavior", scrollBehaviorString)) {
        if (!ScrollableArea::scrollBehaviorFromString(scrollBehaviorString, scrollBehavior)) {
            exceptionState.throwTypeError("The ScrollBehavior provided is invalid.");
            return;
        }
    }

    int position;
    if (!scrollOptionsHorizontal.get("x", position)) {
        exceptionState.throwTypeError("ScrollOptionsHorizontal must include an 'x' member.");
        return;
    }

    // FIXME: Use scrollBehavior to decide whether to scroll smoothly or instantly.
    setScrollLeft(position);
}

void Element::setScrollTop(int newTop)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (document().documentElement() != this) {
        if (RenderBox* rend = renderBox())
            rend->setScrollTop(LayoutUnit::fromFloatRound(newTop * rend->style()->effectiveZoom()));
        return;
    }

    if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
        if (document().inQuirksMode())
            return;

        LocalFrame* frame = document().frame();
        if (!frame)
            return;
        FrameView* view = frame->view();
        if (!view)
            return;

        view->setScrollPosition(IntPoint(view->scrollX(), roundf(newTop * frame->pageZoomFactor())));
    }
}

void Element::setScrollTop(const Dictionary& scrollOptionsVertical, ExceptionState& exceptionState)
{
    String scrollBehaviorString;
    ScrollBehavior scrollBehavior = ScrollBehaviorAuto;
    if (scrollOptionsVertical.get("behavior", scrollBehaviorString)) {
        if (!ScrollableArea::scrollBehaviorFromString(scrollBehaviorString, scrollBehavior)) {
            exceptionState.throwTypeError("The ScrollBehavior provided is invalid.");
            return;
        }
    }

    int position;
    if (!scrollOptionsVertical.get("y", position)) {
        exceptionState.throwTypeError("ScrollOptionsVertical must include a 'y' member.");
        return;
    }

    // FIXME: Use scrollBehavior to decide whether to scroll smoothly or instantly.
    setScrollTop(position);
}

int Element::scrollWidth()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(rend->scrollWidth(), *rend).toDouble();
    return 0;
}

int Element::scrollHeight()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(rend->scrollHeight(), *rend).toDouble();
    return 0;
}

IntRect Element::boundsInRootViewSpace()
{
    document().updateLayoutIgnorePendingStylesheets();

    FrameView* view = document().view();
    if (!view)
        return IntRect();

    Vector<FloatQuad> quads;
    if (isSVGElement() && renderer()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return IntRect();

    IntRect result = quads[0].enclosingBoundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].enclosingBoundingBox());

    result = view->contentsToRootView(result);
    return result;
}

PassRefPtrWillBeRawPtr<ClientRectList> Element::getClientRects()
{
    document().updateLayoutIgnorePendingStylesheets();

    RenderBoxModelObject* renderBoxModelObject = this->renderBoxModelObject();
    if (!renderBoxModelObject)
        return ClientRectList::create();

    // FIXME: Handle SVG elements.
    // FIXME: Handle table/inline-table with a caption.

    Vector<FloatQuad> quads;
    renderBoxModelObject->absoluteQuads(quads);
    document().adjustFloatQuadsForScrollAndAbsoluteZoom(quads, *renderBoxModelObject);
    return ClientRectList::create(quads);
}

PassRefPtrWillBeRawPtr<ClientRect> Element::getBoundingClientRect()
{
    document().updateLayoutIgnorePendingStylesheets();

    Vector<FloatQuad> quads;
    if (isSVGElement() && renderer() && !renderer()->isSVGRoot()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return ClientRect::create();

    FloatRect result = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].boundingBox());

    ASSERT(renderer());
    document().adjustFloatRectForScrollAndAbsoluteZoom(result, *renderer());
    return ClientRect::create(result);
}

IntRect Element::screenRect() const
{
    if (!renderer())
        return IntRect();
    // FIXME: this should probably respect transforms
    return document().view()->contentsToScreen(renderer()->absoluteBoundingBoxRectIgnoringTransforms());
}

const AtomicString& Element::getAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(localName);
    if (const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase()))
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

    size_t index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedLocalName, false) : kNotFound;
    const QualifiedName& qName = index != kNotFound ? attributeAt(index).name() : QualifiedName(nullAtom, caseAdjustedLocalName, nullAtom);
    setAttributeInternal(index, qName, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    synchronizeAttribute(name);
    size_t index = elementData() ? elementData()->findAttributeIndexByName(name) : kNotFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomicString& value)
{
    size_t index = elementData() ? elementData()->findAttributeIndexByName(name) : kNotFound;
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

    const Attribute& existingAttribute = attributeAt(index);
    QualifiedName existingAttributeName = existingAttribute.name();

    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(existingAttributeName, existingAttribute.value(), newValue);

    if (newValue != existingAttribute.value()) {
        // If there is an Attr node hooked to this attribute, the Attr::setValue() call below
        // will write into the ElementData.
        // FIXME: Refactor this so it makes some sense.
        if (RefPtrWillBeRawPtr<Attr> attrNode = inSynchronizationOfLazyAttribute ? nullptr : attrIfExists(existingAttributeName))
            attrNode->setValue(newValue);
        else
            ensureUniqueElementData().attributeAt(index).setValue(newValue);
    }

    if (!inSynchronizationOfLazyAttribute)
        didModifyAttribute(existingAttributeName, newValue);
}

static inline AtomicString makeIdForStyleResolution(const AtomicString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.lower();
    return value;
}

void Element::attributeChanged(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (ElementShadow* parentElementShadow = shadowWhereNodeCanBeDistributed(*this)) {
        if (shouldInvalidateDistributionWhenAttributeChanged(parentElementShadow, name, newValue))
            parentElementShadow->setNeedsDistributionRecalc();
    }

    parseAttribute(name, newValue);

    document().incDOMTreeVersion();

    StyleResolver* styleResolver = document().styleResolver();
    bool testShouldInvalidateStyle = inActiveDocument() && styleResolver && styleChangeType() < SubtreeStyleChange;

    if (isStyledElement() && name == styleAttr) {
        styleAttributeChanged(newValue, reason);
    } else if (isStyledElement() && isPresentationAttribute(name)) {
        elementData()->m_presentationAttributeStyleIsDirty = true;
        setNeedsStyleRecalc(LocalStyleChange);
    }

    if (isIdAttributeName(name)) {
        AtomicString oldId = elementData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
        if (newId != oldId) {
            elementData()->setIdForStyleResolution(newId);
            if (testShouldInvalidateStyle)
                styleResolver->ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForIdChange(oldId, newId, *this);
        }
    } else if (name == classAttr) {
        classAttributeChanged(newValue);
    } else if (name == HTMLNames::nameAttr) {
        setHasName(!newValue.isNull());
    }

    invalidateNodeListCachesInAncestors(&name, this);

    // If there is currently no StyleResolver, we can't be sure that this attribute change won't affect style.
    if (!styleResolver)
        setNeedsStyleRecalc(SubtreeStyleChange);

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->handleAttributeChanged(name, this);
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
        CustomElementRegistrationContext::setTypeExtension(this, newValue);
    attributeChanged(name, newValue, reason);
}

template <typename CharacterType>
static inline bool classStringHasClassName(const CharacterType* characters, unsigned length)
{
    ASSERT(length > 0);

    unsigned i = 0;
    do {
        if (isNotHTMLSpace<CharacterType>(characters[i]))
            break;
        ++i;
    } while (i < length);

    return i < length;
}

static inline bool classStringHasClassName(const AtomicString& newClassString)
{
    unsigned length = newClassString.length();

    if (!length)
        return false;

    if (newClassString.is8Bit())
        return classStringHasClassName(newClassString.characters8(), length);
    return classStringHasClassName(newClassString.characters16(), length);
}

void Element::classAttributeChanged(const AtomicString& newClassString)
{
    StyleResolver* styleResolver = document().styleResolver();
    bool testShouldInvalidateStyle = inActiveDocument() && styleResolver && styleChangeType() < SubtreeStyleChange;

    ASSERT(elementData());
    if (classStringHasClassName(newClassString)) {
        const bool shouldFoldCase = document().inQuirksMode();
        const SpaceSplitString oldClasses = elementData()->classNames();
        elementData()->setClass(newClassString, shouldFoldCase);
        const SpaceSplitString& newClasses = elementData()->classNames();
        if (testShouldInvalidateStyle)
            styleResolver->ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForClassChange(oldClasses, newClasses, *this);
    } else {
        const SpaceSplitString& oldClasses = elementData()->classNames();
        if (testShouldInvalidateStyle)
            styleResolver->ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForClassChange(oldClasses, *this);
        elementData()->clearClass();
    }

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();
}

bool Element::shouldInvalidateDistributionWhenAttributeChanged(ElementShadow* elementShadow, const QualifiedName& name, const AtomicString& newValue)
{
    ASSERT(elementShadow);
    const SelectRuleFeatureSet& featureSet = elementShadow->ensureSelectFeatureSet();

    if (isIdAttributeName(name)) {
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
        if (classStringHasClassName(newClassString)) {
            const bool shouldFoldCase = document().inQuirksMode();
            const SpaceSplitString& oldClasses = elementData()->classNames();
            const SpaceSplitString newClasses(newClassString, shouldFoldCase);
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

bool Element::isJavaScriptURLAttribute(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    size_t destination = 0;
    for (size_t source = 0; source < attributeVector.size(); ++source) {
        if (isEventHandlerAttribute(attributeVector[source])
            || isJavaScriptURLAttribute(attributeVector[source])
            || isHTMLContentAttribute(attributeVector[source]))
            continue;

        if (source != destination)
            attributeVector[destination] = attributeVector[source];

        ++destination;
    }
    attributeVector.shrink(destination);
}

void Element::parserSetAttributes(const Vector<Attribute>& attributeVector)
{
    ASSERT(!inDocument());
    ASSERT(!parentNode());
    ASSERT(!m_elementData);

    if (attributeVector.isEmpty())
        return;

    if (document().elementDataCache())
        m_elementData = document().elementDataCache()->cachedShareableElementDataWithAttributes(attributeVector);
    else
        m_elementData = ShareableElementData::createWithAttributes(attributeVector);

    // Use attributeVector instead of m_elementData because attributeChanged might modify m_elementData.
    for (unsigned i = 0; i < attributeVector.size(); ++i)
        attributeChangedFromParserOrByCloning(attributeVector[i].name(), attributeVector[i].value(), ModifiedDirectly);
}

bool Element::hasAttributes() const
{
    synchronizeAllAttributes();
    return elementData() && elementData()->hasAttributes();
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

void Element::setPrefix(const AtomicString& prefix, ExceptionState& exceptionState)
{
    UseCounter::countDeprecation(document(), UseCounter::ElementSetPrefix);

    if (!prefix.isEmpty() && !Document::isValidName(prefix)) {
        exceptionState.throwDOMException(InvalidCharacterError, "The prefix '" + prefix + "' is not a valid name.");
        return;
    }

    // FIXME: Raise NamespaceError if prefix is malformed per the Namespaces in XML specification.

    const AtomicString& nodeNamespaceURI = namespaceURI();
    if (nodeNamespaceURI.isEmpty() && !prefix.isEmpty()) {
        exceptionState.throwDOMException(NamespaceError, "No namespace is set, so a namespace prefix may not be set.");
        return;
    }

    if (prefix == xmlAtom && nodeNamespaceURI != XMLNames::xmlNamespaceURI) {
        exceptionState.throwDOMException(NamespaceError, "The prefix '" + xmlAtom + "' may not be set on namespace '" + nodeNamespaceURI + "'.");
        return;
    }

    if (exceptionState.hadException())
        return;

    m_tagName.setPrefix(prefix.isEmpty() ? AtomicString() : prefix);
}

const AtomicString& Element::locateNamespacePrefix(const AtomicString& namespaceToLocate) const
{
    if (!prefix().isNull() && namespaceURI() == namespaceToLocate)
        return prefix();

    if (hasAttributes()) {
        AttributeCollection attributes = this->attributes();
        AttributeCollection::const_iterator end = attributes.end();
        for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
            if (it->prefix() == xmlnsAtom && it->value() == namespaceToLocate)
                return it->localName();
        }
    }

    if (Element* parent = parentElement())
        return parent->locateNamespacePrefix(namespaceToLocate);

    return nullAtom;
}

KURL Element::baseURI() const
{
    const AtomicString& baseAttribute = fastGetAttribute(baseAttr);
    KURL base(KURL(), baseAttribute);
    if (!base.protocol().isEmpty())
        return base;

    ContainerNode* parent = parentNode();
    if (!parent)
        return base;

    const KURL& parentBase = parent->baseURI();
    if (parentBase.isNull())
        return base;

    return KURL(parentBase, baseAttribute);
}

const AtomicString Element::imageSourceURL() const
{
    return getAttribute(srcAttr);
}

bool Element::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != NONE;
}

RenderObject* Element::createRenderer(RenderStyle* style)
{
    return RenderObject::createObject(this, style);
}

Node::InsertionNotificationRequest Element::insertedInto(ContainerNode* insertionPoint)
{
    // need to do superclass processing first so inDocument() is true
    // by the time we reach updateId
    ContainerNode::insertedInto(insertionPoint);

    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

    ASSERT(!hasRareData() || !elementRareData()->hasPseudoElements());

    if (!insertionPoint->isInTreeScope())
        return InsertionDone;

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();

    if (isUpgradedCustomElement() && inDocument())
        CustomElement::didEnterDocument(this, document());

    TreeScope& scope = insertionPoint->treeScope();
    if (scope != treeScope())
        return InsertionDone;

    const AtomicString& idValue = getIdAttribute();
    if (!idValue.isNull())
        updateId(scope, nullAtom, idValue);

    const AtomicString& nameValue = getNameAttribute();
    if (!nameValue.isNull())
        updateName(nullAtom, nameValue);

    if (isHTMLLabelElement(*this)) {
        if (scope.shouldCacheLabelsByForAttribute())
            updateLabel(scope, nullAtom, fastGetAttribute(forAttr));
    }

    if (parentElement() && parentElement()->isInCanvasSubtree())
        setIsInCanvasSubtree(true);

    return InsertionDone;
}

void Element::removedFrom(ContainerNode* insertionPoint)
{
    bool wasInDocument = insertionPoint->inDocument();

    ASSERT(!hasRareData() || !elementRareData()->hasPseudoElements());

    if (containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);

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

        if (isHTMLLabelElement(*this)) {
            TreeScope& treeScope = insertionPoint->treeScope();
            if (treeScope.shouldCacheLabelsByForAttribute())
                updateLabel(treeScope, fastGetAttribute(forAttr), nullAtom);
        }
    }

    ContainerNode::removedFrom(insertionPoint);
    if (wasInDocument) {
        if (hasPendingResources())
            document().accessSVGExtensions().removeElementFromPendingResources(this);

        if (isUpgradedCustomElement())
            CustomElement::didLeaveDocument(this, insertionPoint->document());
    }

    document().removeFromTopLayer(this);

    clearElementFlag(IsInCanvasSubtree);

    if (hasRareData())
        elementRareData()->clearRestyleFlags();
}

void Element::attach(const AttachContext& context)
{
    ASSERT(document().inStyleRecalc());

    // We've already been through detach when doing an attach, but we might
    // need to clear any state that's been added since then.
    if (hasRareData() && styleChangeType() == NeedsReattachStyleChange) {
        ElementRareData* data = elementRareData();
        data->clearComputedStyle();
        // Only clear the style state if we're not going to reuse the style from recalcStyle.
        if (!context.resolvedStyle)
            data->resetStyleState();
    }

    RenderTreeBuilder(this, context.resolvedStyle).createRendererForElementIfNeeded();

    addCallbackSelectors();

    StyleResolverParentScope parentScope(*this);

    createPseudoElementIfNeeded(BEFORE);

    // When a shadow root exists, it does the work of attaching the children.
    if (ElementShadow* shadow = this->shadow())
        shadow->attach(context);

    ContainerNode::attach(context);

    createPseudoElementIfNeeded(AFTER);
    createPseudoElementIfNeeded(BACKDROP);

    if (hasRareData() && !renderer()) {
        if (ActiveAnimations* activeAnimations = elementRareData()->activeAnimations()) {
            activeAnimations->cssAnimations().cancel();
            activeAnimations->setAnimationStyleChange(false);
        }
    }
}

void Element::detach(const AttachContext& context)
{
    HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
    cancelFocusAppearanceUpdate();
    removeCallbackSelectors();
    if (svgFilterNeedsLayerUpdate())
        document().unscheduleSVGFilterLayerUpdateHack(*this);
    if (hasRareData()) {
        ElementRareData* data = elementRareData();
        data->clearPseudoElements();

        // attach() will perform the below steps for us when inside recalcStyle.
        if (!document().inStyleRecalc()) {
            data->resetStyleState();
            data->clearComputedStyle();
        }

        if (ActiveAnimations* activeAnimations = data->activeAnimations()) {
            if (context.performingReattach) {
                // FIXME: We call detach from within style recalc, so compositingState is not up to date.
                // https://code.google.com/p/chromium/issues/detail?id=339847
                DisableCompositingQueryAsserts disabler;

                // FIXME: restart compositor animations rather than pull back to the main thread
                activeAnimations->cancelAnimationOnCompositor();
            } else {
                activeAnimations->cssAnimations().cancel();
                activeAnimations->setAnimationStyleChange(false);
            }
        }

        if (ElementShadow* shadow = data->shadow())
            shadow->detach(context);
    }
    ContainerNode::detach(context);
}

bool Element::pseudoStyleCacheIsInvalid(const RenderStyle* currentStyle, RenderStyle* newStyle)
{
    ASSERT(currentStyle == renderStyle());
    ASSERT(renderer());

    if (!currentStyle)
        return false;

    const PseudoStyleCache* pseudoStyleCache = currentStyle->cachedPseudoStyles();
    if (!pseudoStyleCache)
        return false;

    size_t cacheSize = pseudoStyleCache->size();
    for (size_t i = 0; i < cacheSize; ++i) {
        RefPtr<RenderStyle> newPseudoStyle;
        PseudoId pseudoId = pseudoStyleCache->at(i)->styleType();
        if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED)
            newPseudoStyle = renderer()->uncachedFirstLineStyle(newStyle);
        else
            newPseudoStyle = renderer()->getUncachedPseudoStyle(PseudoStyleRequest(pseudoId), newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*newPseudoStyle != *pseudoStyleCache->at(i)) {
            if (pseudoId < FIRST_INTERNAL_PSEUDOID)
                newStyle->setHasPseudoStyle(pseudoId);
            newStyle->addCachedPseudoStyle(newPseudoStyle);
            if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED) {
                // FIXME: We should do an actual diff to determine whether a repaint vs. layout
                // is needed, but for now just assume a layout will be required. The diff code
                // in RenderObject::setStyle would need to be factored out so that it could be reused.
                renderer()->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
            }
            return true;
        }
    }
    return false;
}

PassRefPtr<RenderStyle> Element::styleForRenderer()
{
    ASSERT(document().inStyleRecalc());

    RefPtr<RenderStyle> style;

    // FIXME: Instead of clearing updates that may have been added from calls to styleForElement
    // outside recalcStyle, we should just never set them if we're not inside recalcStyle.
    if (ActiveAnimations* activeAnimations = this->activeAnimations())
        activeAnimations->cssAnimations().setPendingUpdate(nullptr);

    if (hasCustomStyleCallbacks())
        style = customStyleForRenderer();
    if (!style)
        style = originalStyleForRenderer();
    ASSERT(style);

    // styleForElement() might add active animations so we need to get it again.
    if (ActiveAnimations* activeAnimations = this->activeAnimations()) {
        activeAnimations->cssAnimations().maybeApplyPendingUpdate(this);
        activeAnimations->updateAnimationFlags(*style);
    }

    document().didRecalculateStyleForElement();
    return style.release();
}

PassRefPtr<RenderStyle> Element::originalStyleForRenderer()
{
    ASSERT(document().inStyleRecalc());
    return document().ensureStyleResolver().styleForElement(this);
}

void Element::recalcStyle(StyleRecalcChange change, Text* nextTextSibling)
{
    ASSERT(document().inStyleRecalc());
    ASSERT(!parentOrShadowHostNode()->needsStyleRecalc());

    if (hasCustomStyleCallbacks())
        willRecalcStyle(change);

    if (change >= Inherit || needsStyleRecalc()) {
        if (hasRareData()) {
            ElementRareData* data = elementRareData();
            data->resetStyleState();
            data->clearComputedStyle();

            if (change >= Inherit) {
                if (ActiveAnimations* activeAnimations = data->activeAnimations())
                    activeAnimations->setAnimationStyleChange(false);
            }
        }
        if (parentRenderStyle())
            change = recalcOwnStyle(change);
        clearNeedsStyleRecalc();
    }

    // If we reattached we don't need to recalc the style of our descendants anymore.
    if ((change >= UpdatePseudoElements && change < Reattach) || childNeedsStyleRecalc()) {
        recalcChildStyle(change);
        clearChildNeedsStyleRecalc();
    }

    if (hasCustomStyleCallbacks())
        didRecalcStyle(change);

    if (change == Reattach)
        reattachWhitespaceSiblings(nextTextSibling);
}

StyleRecalcChange Element::recalcOwnStyle(StyleRecalcChange change)
{
    ASSERT(document().inStyleRecalc());
    ASSERT(!parentOrShadowHostNode()->needsStyleRecalc());
    ASSERT(change >= Inherit || needsStyleRecalc());
    ASSERT(parentRenderStyle());

    RefPtr<RenderStyle> oldStyle = renderStyle();
    RefPtr<RenderStyle> newStyle = styleForRenderer();
    StyleRecalcChange localChange = RenderStyle::stylePropagationDiff(oldStyle.get(), newStyle.get());

    ASSERT(newStyle);

    if (localChange == Reattach) {
        AttachContext reattachContext;
        reattachContext.resolvedStyle = newStyle.get();
        bool rendererWillChange = needsAttach() || renderer();
        reattach(reattachContext);
        if (rendererWillChange || renderer())
            return Reattach;
        return ReattachNoRenderer;
    }

    ASSERT(oldStyle);

    if (localChange != NoChange)
        updateCallbackSelectors(oldStyle.get(), newStyle.get());

    if (RenderObject* renderer = this->renderer()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(oldStyle.get(), newStyle.get()) || svgFilterNeedsLayerUpdate()) {
            renderer->setStyle(newStyle.get());
        } else {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            // FIXME: We may be able to remove this hack, see discussion in
            // https://codereview.chromium.org/30453002/
            renderer->setStyleInternal(newStyle.get());
        }
    }

    if (styleChangeType() >= SubtreeStyleChange)
        return Force;

    if (change > Inherit || localChange > Inherit)
        return max(localChange, change);

    if (localChange < Inherit && (oldStyle->hasPseudoElementStyle() || newStyle->hasPseudoElementStyle()))
        return UpdatePseudoElements;

    return localChange;
}

void Element::recalcChildStyle(StyleRecalcChange change)
{
    ASSERT(document().inStyleRecalc());
    ASSERT(change >= UpdatePseudoElements || childNeedsStyleRecalc());
    ASSERT(!needsStyleRecalc());

    StyleResolverParentScope parentScope(*this);

    updatePseudoElement(BEFORE, change);

    if (change > UpdatePseudoElements || childNeedsStyleRecalc()) {
        for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
            if (root->shouldCallRecalcStyle(change))
                root->recalcStyle(change);
        }
    }

    if (change < Force && hasRareData() && childNeedsStyleRecalc())
        checkForChildrenAdjacentRuleChanges();

    if (change > UpdatePseudoElements || childNeedsStyleRecalc()) {
        // This loop is deliberately backwards because we use insertBefore in the rendering tree, and want to avoid
        // a potentially n^2 loop to find the insertion point while resolving style. Having us start from the last
        // child and work our way back means in the common case, we'll find the insertion point in O(1) time.
        // See crbug.com/288225
        StyleResolver& styleResolver = document().ensureStyleResolver();
        Text* lastTextNode = 0;
        for (Node* child = lastChild(); child; child = child->previousSibling()) {
            if (child->isTextNode()) {
                toText(child)->recalcTextStyle(change, lastTextNode);
                lastTextNode = toText(child);
            } else if (child->isElementNode()) {
                Element* element = toElement(child);
                if (element->shouldCallRecalcStyle(change))
                    element->recalcStyle(change, lastTextNode);
                else if (element->supportsStyleSharing())
                    styleResolver.addToStyleSharingList(*element);
                if (element->renderer())
                    lastTextNode = 0;
            }
        }
    }

    updatePseudoElement(AFTER, change);
    updatePseudoElement(BACKDROP, change);
}

void Element::updateCallbackSelectors(RenderStyle* oldStyle, RenderStyle* newStyle)
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
    updateCallbackSelectors(0, renderStyle());
}

void Element::removeCallbackSelectors()
{
    updateCallbackSelectors(renderStyle(), 0);
}

ElementShadow* Element::shadow() const
{
    return hasRareData() ? elementRareData()->shadow() : 0;
}

ElementShadow& Element::ensureShadow()
{
    return ensureElementRareData().ensureShadow();
}

void Element::didAffectSelector(AffectedSelectorMask mask)
{
    setNeedsStyleRecalc(SubtreeStyleChange);
    if (ElementShadow* elementShadow = shadowWhereNodeCanBeDistributed(*this))
        elementShadow->didAffectSelector(mask);
}

void Element::setAnimationStyleChange(bool animationStyleChange)
{
    if (animationStyleChange && document().inStyleRecalc())
        return;
    if (ActiveAnimations* activeAnimations = elementRareData()->activeAnimations())
        activeAnimations->setAnimationStyleChange(animationStyleChange);
}

void Element::setNeedsAnimationStyleRecalc()
{
    if (styleChangeType() != NoStyleChange)
        return;

    setNeedsStyleRecalc(LocalStyleChange);
    setAnimationStyleChange(true);
}

void Element::setNeedsCompositingUpdate()
{
    if (!document().isActive())
        return;
    RenderBoxModelObject* renderer = renderBoxModelObject();
    if (!renderer)
        return;
    if (!renderer->hasLayer())
        return;
    renderer->layer()->setNeedsCompositingInputsUpdate();
    document().renderView()->compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterCompositingInputChange);
}

void Element::setCustomElementDefinition(PassRefPtr<CustomElementDefinition> definition)
{
    if (!hasRareData() && !definition)
        return;
    ASSERT(!customElementDefinition());
    ensureElementRareData().setCustomElementDefinition(definition);
}

CustomElementDefinition* Element::customElementDefinition() const
{
    if (hasRareData())
        return elementRareData()->customElementDefinition();
    return 0;
}

PassRefPtrWillBeRawPtr<ShadowRoot> Element::createShadowRoot(ExceptionState& exceptionState)
{
    if (alwaysCreateUserAgentShadowRoot())
        ensureUserAgentShadowRoot();

    // Some elements make assumptions about what kind of renderers they allow
    // as children so we can't allow author shadows on them for now. An override
    // flag is provided for testing how author shadows interact on these elements.
    if (!areAuthorShadowsAllowed() && !RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled()) {
        exceptionState.throwDOMException(HierarchyRequestError, "Author-created shadow roots are disabled for this element.");
        return nullptr;
    }

    return PassRefPtrWillBeRawPtr<ShadowRoot>(ensureShadow().addShadowRoot(*this, ShadowRoot::AuthorShadowRoot));
}

ShadowRoot* Element::shadowRoot() const
{
    ElementShadow* elementShadow = shadow();
    if (!elementShadow)
        return 0;
    ShadowRoot* shadowRoot = elementShadow->youngestShadowRoot();
    if (shadowRoot->type() == ShadowRoot::AuthorShadowRoot)
        return shadowRoot;
    return 0;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    if (ElementShadow* elementShadow = shadow()) {
        if (ShadowRoot* shadowRoot = elementShadow->oldestShadowRoot()) {
            ASSERT(shadowRoot->type() == ShadowRoot::UserAgentShadowRoot);
            return shadowRoot;
        }
    }

    return 0;
}

ShadowRoot& Element::ensureUserAgentShadowRoot()
{
    if (ShadowRoot* shadowRoot = userAgentShadowRoot())
        return *shadowRoot;
    ShadowRoot& shadowRoot = ensureShadow().addShadowRoot(*this, ShadowRoot::UserAgentShadowRoot);
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
    RenderStyle* style = renderStyle();

    if (!style && !styleAffectedByEmpty())
        return;

    if (!style || (styleAffectedByEmpty() && (!style->emptyState() || hasChildren())))
        setNeedsStyleRecalc(SubtreeStyleChange);
}

void Element::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    checkForEmptyStyleChange();
    if (!changedByParser)
        checkForSiblingStyleChanges(false, beforeChange, afterChange, childCountDelta);

    if (ElementShadow* shadow = this->shadow())
        shadow->setNeedsDistributionRecalc();
}

void Element::finishParsingChildren()
{
    setIsFinishedParsingChildren(true);
    checkForEmptyStyleChange();
    checkForSiblingStyleChanges(true, lastChild(), 0, 0);
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
            result.appendLiteral("; ");
        result.appendLiteral("id=");
        result.append(s);
    }

    s = getAttribute(classAttr);
    if (s.length() > 0) {
        if (result.length() > 0)
            result.appendLiteral("; ");
        result.appendLiteral("class=");
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}
#endif

WillBeHeapVector<RefPtrWillBeMember<Attr> >* Element::attrNodeList()
{
    return hasRareData() ? elementRareData()->attrNodeList() : 0;
}

WillBeHeapVector<RefPtrWillBeMember<Attr> >& Element::ensureAttrNodeList()
{
    setHasSyntheticAttrChildNodes(true);
    return ensureElementRareData().ensureAttrNodeList();
}

void Element::removeAttrNodeList()
{
    ASSERT(hasSyntheticAttrChildNodes());
    if (hasRareData())
        elementRareData()->removeAttrNodeList();
    setHasSyntheticAttrChildNodes(false);
}

PassRefPtrWillBeRawPtr<Attr> Element::setAttributeNode(Attr* attrNode, ExceptionState& exceptionState)
{
    if (!attrNode) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "Attr"));
        return nullptr;
    }

    RefPtrWillBeRawPtr<Attr> oldAttrNode = attrIfExists(attrNode->qualifiedName());
    if (oldAttrNode.get() == attrNode)
        return attrNode; // This Attr is already attached to the element.

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode->ownerElement()) {
        exceptionState.throwDOMException(InUseAttributeError, "The node provided is an attribute node that is already an attribute of another Element; attribute nodes must be explicitly cloned.");
        return nullptr;
    }

    synchronizeAllAttributes();
    UniqueElementData& elementData = ensureUniqueElementData();

    size_t index = elementData.findAttributeIndexByName(attrNode->qualifiedName(), shouldIgnoreAttributeCase());
    AtomicString localName;
    if (index != kNotFound) {
        const Attribute& attr = elementData.attributeAt(index);

        // If the name of the ElementData attribute doesn't
        // (case-sensitively) match that of the Attr node, record it
        // on the Attr so that it can correctly resolve the value on
        // the Element.
        if (!attr.name().matches(attrNode->qualifiedName()))
            localName = attr.localName();

        if (oldAttrNode) {
            detachAttrNodeFromElementWithValue(oldAttrNode.get(), attr.value());
        } else {
            // FIXME: using attrNode's name rather than the
            // Attribute's for the replaced Attr is compatible with
            // all but Gecko (and, arguably, the DOM Level1 spec text.)
            // Consider switching.
            oldAttrNode = Attr::create(document(), attrNode->qualifiedName(), attr.value());
        }
    }

    setAttributeInternal(index, attrNode->qualifiedName(), attrNode->value(), NotInSynchronizationOfLazyAttribute);

    attrNode->attachToElement(this, localName);
    treeScope().adoptIfNeeded(*attrNode);
    ensureAttrNodeList().append(attrNode);

    return oldAttrNode.release();
}

PassRefPtrWillBeRawPtr<Attr> Element::setAttributeNodeNS(Attr* attr, ExceptionState& exceptionState)
{
    return setAttributeNode(attr, exceptionState);
}

PassRefPtrWillBeRawPtr<Attr> Element::removeAttributeNode(Attr* attr, ExceptionState& exceptionState)
{
    if (!attr) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "Attr"));
        return nullptr;
    }
    if (attr->ownerElement() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node provided is owned by another element.");
        return nullptr;
    }

    ASSERT(document() == attr->document());

    synchronizeAttribute(attr->qualifiedName());

    size_t index = elementData()->findAttrNodeIndex(attr);
    if (index == kNotFound) {
        exceptionState.throwDOMException(NotFoundError, "The attribute was not found on this element.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<Attr> guard(attr);
    detachAttrNodeAtIndex(attr, index);
    return guard.release();
}

void Element::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == tabindexAttr) {
        int tabindex = 0;
        if (value.isEmpty()) {
            clearTabIndexExplicitlyIfNeeded();
            if (treeScope().adjustedFocusedElement() == this) {
                // We might want to call blur(), but it's dangerous to dispatch
                // events here.
                document().setNeedsFocusedElementCheck();
            }
        } else if (parseHTMLInteger(value, tabindex)) {
            // Clamp tabindex to the range of 'short' to match Firefox's behavior.
            setTabIndexExplicitly(max(static_cast<int>(std::numeric_limits<short>::min()), std::min(tabindex, static_cast<int>(std::numeric_limits<short>::max()))));
        }
    }
}

bool Element::parseAttributeName(QualifiedName& out, const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionState& exceptionState)
{
    AtomicString prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, exceptionState))
        return false;
    ASSERT(!exceptionState.hadException());

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
    ASSERT_WITH_SECURITY_IMPLICATION(index < attributeCount());

    UniqueElementData& elementData = ensureUniqueElementData();

    QualifiedName name = elementData.attributeAt(index).name();
    AtomicString valueBeingRemoved = elementData.attributeAt(index).value();

    if (!inSynchronizationOfLazyAttribute) {
        if (!valueBeingRemoved.isNull())
            willModifyAttribute(name, valueBeingRemoved, nullAtom);
    }

    if (RefPtrWillBeRawPtr<Attr> attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), elementData.attributeAt(index).value());

    elementData.removeAttributeAt(index);

    if (!inSynchronizationOfLazyAttribute)
        didRemoveAttribute(name);
}

void Element::appendAttributeInternal(const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, nullAtom, value);
    ensureUniqueElementData().appendAttribute(name, value);
    if (!inSynchronizationOfLazyAttribute)
        didAddAttribute(name, value);
}

void Element::removeAttribute(const AtomicString& name)
{
    if (!elementData())
        return;

    AtomicString localName = shouldIgnoreAttributeCase() ? name.lower() : name;
    size_t index = elementData()->findAttributeIndexByName(localName, false);
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

PassRefPtrWillBeRawPtr<Attr> Element::getAttributeNode(const AtomicString& localName)
{
    if (!elementData())
        return nullptr;
    synchronizeAttribute(localName);
    const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase());
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

PassRefPtrWillBeRawPtr<Attr> Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (!elementData())
        return nullptr;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    const Attribute* attribute = elementData()->findAttributeByName(qName);
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

bool Element::hasAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return false;
    synchronizeAttribute(localName);
    return elementData()->findAttributeByName(shouldIgnoreAttributeCase() ? localName.lower() : localName, false);
}

bool Element::hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->findAttributeByName(qName);
}

void Element::focus(bool restorePreviousSelection, FocusType type)
{
    if (!inDocument())
        return;

    if (document().focusedElement() == this)
        return;

    if (!document().isActive())
        return;

    document().updateLayoutIgnorePendingStylesheets();
    if (!isFocusable())
        return;

    RefPtrWillBeRawPtr<Node> protect(this);
    if (!document().page()->focusController().setFocusedElement(this, document().frame(), type))
        return;

    // Setting the focused node above might have invalidated the layout due to scripts.
    document().updateLayoutIgnorePendingStylesheets();
    if (!isFocusable())
        return;

    cancelFocusAppearanceUpdate();
    updateFocusAppearance(restorePreviousSelection);
}

void Element::updateFocusAppearance(bool /*restorePreviousSelection*/)
{
    if (isRootEditableElement()) {
        // Taking the ownership since setSelection() may release the last reference to |frame|.
        RefPtr<LocalFrame> frame(document().frame());
        if (!frame)
            return;

        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection().rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this), DOWNSTREAM);
        // Passing DoNotSetFocus as this function is called after FocusController::setFocusedElement()
        // and we don't want to change the focus to a new Element.
        frame->selection().setSelection(newSelection, FrameSelection::DoNotSetFocus);
        frame->selection().revealSelection();
    } else if (renderer() && !renderer()->isWidget())
        renderer()->scrollRectToVisible(boundingBox());
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    if (treeScope().adjustedFocusedElement() == this) {
        Document& doc = document();
        if (doc.page())
            doc.page()->focusController().setFocusedElement(0, doc.frame());
        else
            doc.setFocusedElement(nullptr);
    }
}

bool Element::supportsFocus() const
{
    // FIXME: supportsFocus() can be called when layout is not up to date.
    // Logic that deals with the renderer should be moved to rendererIsFocusable().
    // But supportsFocus must return true when the element is editable, or else
    // it won't be focusable. Furthermore, supportsFocus cannot just return true
    // always or else tabIndex() will change for all HTML elements.
    return hasElementFlag(TabIndexWasSetExplicitly) || (rendererIsEditable() && parentNode() && !parentNode()->rendererIsEditable())
        || supportsSpatialNavigationFocus();
}

bool Element::supportsSpatialNavigationFocus() const
{
    // This function checks whether the element satisfies the extended criteria
    // for the element to be focusable, introduced by spatial navigation feature,
    // i.e. checks if click or keyboard event handler is specified.
    // This is the way to make it possible to navigate to (focus) elements
    // which web designer meant for being active (made them respond to click events).

    if (!document().settings() || !document().settings()->spatialNavigationEnabled())
        return false;
    return hasEventListeners(EventTypeNames::click)
        || hasEventListeners(EventTypeNames::keydown)
        || hasEventListeners(EventTypeNames::keypress)
        || hasEventListeners(EventTypeNames::keyup);
}

bool Element::isFocusable() const
{
    return inDocument() && supportsFocus() && !isInert() && rendererIsFocusable();
}

bool Element::isKeyboardFocusable() const
{
    return isFocusable() && tabIndex() >= 0;
}

bool Element::isMouseFocusable() const
{
    return isFocusable();
}

void Element::dispatchFocusEvent(Element* oldFocusedElement, FocusType)
{
    RefPtrWillBeRawPtr<FocusEvent> event = FocusEvent::create(EventTypeNames::focus, false, false, document().domWindow(), 0, oldFocusedElement);
    EventDispatcher::dispatchEvent(this, FocusEventDispatchMediator::create(event.release()));
}

void Element::dispatchBlurEvent(Element* newFocusedElement)
{
    RefPtrWillBeRawPtr<FocusEvent> event = FocusEvent::create(EventTypeNames::blur, false, false, document().domWindow(), 0, newFocusedElement);
    EventDispatcher::dispatchEvent(this, BlurEventDispatchMediator::create(event.release()));
}

void Element::dispatchFocusInEvent(const AtomicString& eventType, Element* oldFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == EventTypeNames::focusin || eventType == EventTypeNames::DOMFocusIn);
    dispatchScopedEventDispatchMediator(FocusInEventDispatchMediator::create(FocusEvent::create(eventType, true, false, document().domWindow(), 0, oldFocusedElement)));
}

void Element::dispatchFocusOutEvent(const AtomicString& eventType, Element* newFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == EventTypeNames::focusout || eventType == EventTypeNames::DOMFocusOut);
    dispatchScopedEventDispatchMediator(FocusOutEventDispatchMediator::create(FocusEvent::create(eventType, true, false, document().domWindow(), 0, newFocusedElement)));
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
    if (RefPtrWillBeRawPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(html, this, AllowScriptingContent, "innerHTML", exceptionState)) {
        ContainerNode* container = this;
        if (isHTMLTemplateElement(*this))
            container = toHTMLTemplateElement(this)->content();
        replaceChildrenWithFragment(container, fragment.release(), exceptionState);
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

    RefPtrWillBeRawPtr<Element> parent = toElement(p);
    RefPtrWillBeRawPtr<Node> prev = previousSibling();
    RefPtrWillBeRawPtr<Node> next = nextSibling();

    RefPtrWillBeRawPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(html, parent.get(), AllowScriptingContent, "outerHTML", exceptionState);
    if (exceptionState.hadException())
        return;

    parent->replaceChild(fragment.release(), this, exceptionState);
    RefPtrWillBeRawPtr<Node> node = next ? next->previousSibling() : 0;
    if (!exceptionState.hadException() && node && node->isTextNode())
        mergeWithNextTextNode(node.release(), exceptionState);

    if (!exceptionState.hadException() && prev && prev->isTextNode())
        mergeWithNextTextNode(prev.release(), exceptionState);
}

Node* Element::insertAdjacent(const String& where, Node* newChild, ExceptionState& exceptionState)
{
    if (equalIgnoringCase(where, "beforeBegin")) {
        if (ContainerNode* parent = this->parentNode()) {
            parent->insertBefore(newChild, this, exceptionState);
            if (!exceptionState.hadException())
                return newChild;
        }
        return 0;
    }

    if (equalIgnoringCase(where, "afterBegin")) {
        insertBefore(newChild, firstChild(), exceptionState);
        return exceptionState.hadException() ? 0 : newChild;
    }

    if (equalIgnoringCase(where, "beforeEnd")) {
        appendChild(newChild, exceptionState);
        return exceptionState.hadException() ? 0 : newChild;
    }

    if (equalIgnoringCase(where, "afterEnd")) {
        if (ContainerNode* parent = this->parentNode()) {
            parent->insertBefore(newChild, nextSibling(), exceptionState);
            if (!exceptionState.hadException())
                return newChild;
        }
        return 0;
    }

    exceptionState.throwDOMException(SyntaxError, "The value provided ('" + where + "') is not one of 'beforeBegin', 'afterBegin', 'beforeEnd', or 'afterEnd'.");
    return 0;
}

// Step 1 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
static Element* contextElementForInsertion(const String& where, Element* element, ExceptionState& exceptionState)
{
    if (equalIgnoringCase(where, "beforeBegin") || equalIgnoringCase(where, "afterEnd")) {
        ContainerNode* parent = element->parentNode();
        if (!parent || !parent->isElementNode()) {
            exceptionState.throwDOMException(NoModificationAllowedError, "The element has no parent.");
            return 0;
        }
        return toElement(parent);
    }
    if (equalIgnoringCase(where, "afterBegin") || equalIgnoringCase(where, "beforeEnd"))
        return element;
    exceptionState.throwDOMException(SyntaxError, "The value provided ('" + where + "') is not one of 'beforeBegin', 'afterBegin', 'beforeEnd', or 'afterEnd'.");
    return 0;
}

Element* Element::insertAdjacentElement(const String& where, Element* newChild, ExceptionState& exceptionState)
{
    if (!newChild) {
        // IE throws COM Exception E_INVALIDARG; this is the best DOM exception alternative.
        exceptionState.throwTypeError("The node provided is null.");
        return 0;
    }

    Node* returnValue = insertAdjacent(where, newChild, exceptionState);
    return toElement(returnValue);
}

void Element::insertAdjacentText(const String& where, const String& text, ExceptionState& exceptionState)
{
    insertAdjacent(where, document().createTextNode(text).get(), exceptionState);
}

void Element::insertAdjacentHTML(const String& where, const String& markup, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<Element> contextElement = contextElementForInsertion(where, this, exceptionState);
    if (!contextElement)
        return;

    RefPtrWillBeRawPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, contextElement.get(), AllowScriptingContent, "insertAdjacentHTML", exceptionState);
    if (!fragment)
        return;
    insertAdjacent(where, fragment.get(), exceptionState);
}

String Element::innerText()
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return textContent(true);

    return plainText(rangeOfContents(const_cast<Element*>(this)).get());
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

    ASSERT(content.length() == totalLength);
    return content.toString();
}

const AtomicString& Element::shadowPseudoId() const
{
    if (ShadowRoot* root = containingShadowRoot()) {
        if (root->type() == ShadowRoot::UserAgentShadowRoot)
            return fastGetAttribute(pseudoAttr);
    }
    return nullAtom;
}

void Element::setShadowPseudoId(const AtomicString& id)
{
    ASSERT(CSSSelector::parsePseudoType(id) == CSSSelector::PseudoWebKitCustomElement || CSSSelector::parsePseudoType(id) == CSSSelector::PseudoUserAgentCustomElement);
    setAttribute(pseudoAttr, id);
}

bool Element::isInDescendantTreeOf(const Element* shadowHost) const
{
    ASSERT(shadowHost);
    ASSERT(isShadowHost(shadowHost));

    const ShadowRoot* shadowRoot = containingShadowRoot();
    while (shadowRoot) {
        const Element* ancestorShadowHost = shadowRoot->shadowHost();
        if (ancestorShadowHost == shadowHost)
            return true;
        shadowRoot = ancestorShadowHost->containingShadowRoot();
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

RenderStyle* Element::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (PseudoElement* element = pseudoElement(pseudoElementSpecifier))
        return element->computedStyle();

    // FIXME: Find and use the renderer from the pseudo element instead of the actual element so that the 'length'
    // properties, which are only known by the renderer because it did the layout, will be correct and so that the
    // values returned for the ":selection" pseudo-element will be correct.
    if (RenderStyle* usedStyle = renderStyle()) {
        if (pseudoElementSpecifier) {
            RenderStyle* cachedPseudoStyle = usedStyle->getCachedPseudoStyle(pseudoElementSpecifier);
            return cachedPseudoStyle ? cachedPseudoStyle : usedStyle;
         } else
            return usedStyle;
    }

    if (!inActiveDocument())
        // FIXME: Try to do better than this. Ensure that styleForElement() works for elements that are not in the
        // document tree and figure out when to destroy the computed style for such elements.
        return 0;

    ElementRareData& rareData = ensureElementRareData();
    if (!rareData.computedStyle())
        rareData.setComputedStyle(document().styleForElementIgnoringPendingStylesheets(this));
    return pseudoElementSpecifier ? rareData.computedStyle()->getCachedPseudoStyle(pseudoElementSpecifier) : rareData.computedStyle();
}

AtomicString Element::computeInheritedLanguage() const
{
    const Node* n = this;
    AtomicString value;
    // The language property is inherited, so we iterate over the parents to find the first language.
    do {
        if (n->isElementNode()) {
            if (const ElementData* elementData = toElement(n)->elementData()) {
                // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                if (const Attribute* attribute = elementData->findAttributeByName(XMLNames::langAttr))
                    value = attribute->value();
                else if (const Attribute* attribute = elementData->findAttributeByName(HTMLNames::langAttr))
                    value = attribute->value();
            }
        } else if (n->isDocumentNode()) {
            // checking the MIME content-language
            value = toDocument(n)->contentLanguage();
        }

        n = n->parentNode();
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

void Element::normalizeAttributes()
{
    if (!hasAttributes())
        return;
    WillBeHeapVector<RefPtrWillBeMember<Attr> >* attrNodes = attrNodeList();
    if (!attrNodes)
        return;
    // Copy the Attr Vector because Node::normalize() can fire synchronous JS
    // events (e.g. DOMSubtreeModified) and a JS listener could add / remove
    // attributes while we are iterating.
    WillBeHeapVector<RefPtrWillBeMember<Attr> > attrNodesCopy(*attrNodes);
    for (size_t i = 0; i < attrNodesCopy.size(); ++i)
        attrNodesCopy[i]->normalize();
}

void Element::updatePseudoElement(PseudoId pseudoId, StyleRecalcChange change)
{
    ASSERT(!needsStyleRecalc());
    PseudoElement* element = pseudoElement(pseudoId);
    if (element && (change == UpdatePseudoElements || element->shouldCallRecalcStyle(change))) {

        // Need to clear the cached style if the PseudoElement wants a recalc so it
        // computes a new style.
        if (element->needsStyleRecalc())
            renderer()->style()->removeCachedPseudoStyle(pseudoId);

        // PseudoElement styles hang off their parent element's style so if we needed
        // a style recalc we should Force one on the pseudo.
        // FIXME: We should figure out the right text sibling to pass.
        element->recalcStyle(change == UpdatePseudoElements ? Force : change);

        // Wait until our parent is not displayed or pseudoElementRendererIsNeeded
        // is false, otherwise we could continously create and destroy PseudoElements
        // when RenderObject::isChildAllowed on our parent returns false for the
        // PseudoElement's renderer for each style recalc.
        if (!renderer() || !pseudoElementRendererIsNeeded(renderer()->getCachedPseudoStyle(pseudoId)))
            elementRareData()->setPseudoElement(pseudoId, nullptr);
    } else if (change >= UpdatePseudoElements) {
        createPseudoElementIfNeeded(pseudoId);
    }
}

void Element::createPseudoElementIfNeeded(PseudoId pseudoId)
{
    if (isPseudoElement())
        return;

    // Document::ensureStyleResolver is not inlined and shows up on profiles, avoid it here.
    StyleEngine* engine = document().styleEngine();
    RefPtrWillBeRawPtr<PseudoElement> element = engine->ensureResolver().createPseudoElementIfNeeded(*this, pseudoId);
    if (!element)
        return;

    if (pseudoId == BACKDROP)
        document().addToTopLayer(element.get(), this);
    element->insertedInto(this);
    element->attach();

    InspectorInstrumentation::pseudoElementCreated(element.get());

    ensureElementRareData().setPseudoElement(pseudoId, element.release());
}

PseudoElement* Element::pseudoElement(PseudoId pseudoId) const
{
    return hasRareData() ? elementRareData()->pseudoElement(pseudoId) : 0;
}

RenderObject* Element::pseudoElementRenderer(PseudoId pseudoId) const
{
    if (PseudoElement* element = pseudoElement(pseudoId))
        return element->renderer();
    return 0;
}

bool Element::matches(const String& selectors, ExceptionState& exceptionState)
{
    SelectorQuery* selectorQuery = document().selectorQueryCache().add(AtomicString(selectors), document(), exceptionState);
    if (!selectorQuery)
        return false;
    return selectorQuery->matches(*this);
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

KURL Element::getURLAttribute(const QualifiedName& name) const
{
#if ASSERT_ENABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

KURL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if ASSERT_ENABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    String value = stripLeadingAndTrailingHTMLSpaces(getAttribute(name));
    if (value.isEmpty())
        return KURL();
    return document().completeURL(value);
}

int Element::getIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).string().toInt();
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    setAttribute(attributeName, AtomicString::number(value));
}

unsigned Element::getUnsignedIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).string().toUInt();
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

void Element::webkitRequestFullscreen()
{
    FullscreenElementStack::from(document()).requestFullScreenForElement(this, ALLOW_KEYBOARD_INPUT, FullscreenElementStack::EnforceIFrameAllowFullScreenRequirement);
}

void Element::webkitRequestFullScreen(unsigned short flags)
{
    FullscreenElementStack::from(document()).requestFullScreenForElement(this, (flags | LEGACY_MOZILLA_REQUEST), FullscreenElementStack::EnforceIFrameAllowFullScreenRequirement);
}

void Element::setContainsFullScreenElement(bool flag)
{
    setElementFlag(ContainsFullScreenElement, flag);
    setNeedsStyleRecalc(SubtreeStyleChange);
}

static Element* parentCrossingFrameBoundaries(Element* element)
{
    ASSERT(element);
    return element->parentElement() ? element->parentElement() : element->document().ownerElement();
}

void Element::setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool flag)
{
    Element* element = this;
    while ((element = parentCrossingFrameBoundaries(element)))
        element->setContainsFullScreenElement(flag);
}

void Element::setIsInTopLayer(bool inTopLayer)
{
    if (isInTopLayer() == inTopLayer)
        return;
    setElementFlag(IsInTopLayer, inTopLayer);

    // We must ensure a reattach occurs so the renderer is inserted in the correct sibling order under RenderView according to its
    // top layer position, or in its usual place if not in the top layer.
    lazyReattachIfAttached();
}

void Element::webkitRequestPointerLock()
{
    if (document().page())
        document().page()->pointerLockController().requestPointerLock(this);
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

#ifndef NDEBUG
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
    if (!inDocument() || isInShadowTree())
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

    updateId(treeScope(), oldId, newId);
}

inline void Element::updateId(TreeScope& scope, const AtomicString& oldId, const AtomicString& newId)
{
    ASSERT(isInTreeScope());
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope.removeElementById(oldId, this);
    if (!newId.isEmpty())
        scope.addElementById(newId, this);

    if (shouldRegisterAsExtraNamedItem())
        updateExtraNamedItemRegistration(oldId, newId);
}

void Element::updateLabel(TreeScope& scope, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue)
{
    ASSERT(isHTMLLabelElement(this));

    if (!inDocument())
        return;

    if (oldForAttributeValue == newForAttributeValue)
        return;

    if (!oldForAttributeValue.isEmpty())
        scope.removeLabel(oldForAttributeValue, toHTMLLabelElement(this));
    if (!newForAttributeValue.isEmpty())
        scope.addLabel(newForAttributeValue, toHTMLLabelElement(this));
}

void Element::willModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (isIdAttributeName(name)) {
        updateId(oldValue, newValue);
    } else if (name == HTMLNames::nameAttr) {
        updateName(oldValue, newValue);
    } else if (name == HTMLNames::forAttr && isHTMLLabelElement(*this)) {
        TreeScope& scope = treeScope();
        if (scope.shouldCacheLabelsByForAttribute())
            updateLabel(scope, oldValue, newValue);
    }

    if (oldValue != newValue) {
        if (inActiveDocument() && document().styleResolver() && styleChangeType() < SubtreeStyleChange)
            document().ensureStyleResolver().ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForAttributeChange(name, *this);

        if (isUpgradedCustomElement())
            CustomElement::attributeDidChange(this, name.localName(), oldValue, newValue);
    }

    if (OwnPtrWillBeRawPtr<MutationObserverInterestGroup> recipients = MutationObserverInterestGroup::createForAttributesMutation(*this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(this, name, oldValue));

    InspectorInstrumentation::willModifyDOMAttr(this, oldValue, newValue);
}

void Element::didAddAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(this, name.localName(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(this, name.localName(), value);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name)
{
    attributeChanged(name, nullAtom);
    InspectorInstrumentation::didRemoveDOMAttr(this, name.localName());
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

PassRefPtrWillBeRawPtr<HTMLCollection> Element::ensureCachedHTMLCollection(CollectionType type)
{
    if (HTMLCollection* collection = cachedHTMLCollection(type))
        return collection;

    if (type == TableRows) {
        ASSERT(isHTMLTableElement(this));
        return ensureRareData().ensureNodeLists().addCache<HTMLTableRowsCollection>(*this, type);
    } else if (type == SelectOptions) {
        ASSERT(isHTMLSelectElement(this));
        return ensureRareData().ensureNodeLists().addCache<HTMLOptionsCollection>(*this, type);
    } else if (type == FormControls) {
        ASSERT(isHTMLFormElement(this) || isHTMLFieldSetElement(this));
        return ensureRareData().ensureNodeLists().addCache<HTMLFormControlsCollection>(*this, type);
    }
    return ensureRareData().ensureNodeLists().addCache<HTMLCollection>(*this, type);
}

void Element::scheduleSVGFilterLayerUpdateHack()
{
    document().scheduleSVGFilterLayerUpdateHack(*this);
}

HTMLCollection* Element::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() && rareData()->nodeLists() ? rareData()->nodeLists()->cached<HTMLCollection>(type) : 0;
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

PassRefPtrWillBeRawPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (AttrNodeList* attrNodeList = this->attrNodeList())
        return findAttrNodeInList(*attrNodeList, name);
    return nullptr;
}

PassRefPtrWillBeRawPtr<Attr> Element::ensureAttr(const QualifiedName& name)
{
    AttrNodeList& attrNodeList = ensureAttrNodeList();
    RefPtrWillBeRawPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, name);
    if (!attrNode) {
        attrNode = Attr::create(*this, name);
        treeScope().adoptIfNeeded(*attrNode);
        attrNodeList.append(attrNode);
    }
    return attrNode.release();
}

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomicString& value)
{
    ASSERT(hasSyntheticAttrChildNodes());
    attrNode->detachFromElementWithValue(value);

    AttrNodeList* list = attrNodeList();
    for (unsigned i = 0; i < list->size(); ++i) {
        if (list->at(i)->qualifiedName() == attrNode->qualifiedName()) {
            list->remove(i);
            if (list->isEmpty())
                removeAttrNodeList();
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void Element::detachAllAttrNodesFromElement()
{
    AttrNodeList* list = this->attrNodeList();
    ASSERT(list);

    AttributeCollection attributes = this->attributes();
    AttributeCollection::const_iterator end = attributes.end();
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
        if (RefPtrWillBeRawPtr<Attr> attrNode = findAttrNodeInList(*list, it->name()))
            attrNode->detachFromElementWithValue(it->value());
    }

    removeAttrNodeList();
}

void Element::willRecalcStyle(StyleRecalcChange)
{
    ASSERT(hasCustomStyleCallbacks());
}

void Element::didRecalcStyle(StyleRecalcChange)
{
    ASSERT(hasCustomStyleCallbacks());
}


PassRefPtr<RenderStyle> Element::customStyleForRenderer()
{
    ASSERT(hasCustomStyleCallbacks());
    return nullptr;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasSyntheticAttrChildNodes())
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
        const_cast<Element&>(other).m_elementData = static_cast<const UniqueElementData*>(other.m_elementData.get())->makeShareableCopy();

    if (!other.m_elementData->isUnique() && !ownerDocumentsHaveDifferentCaseSensitivity && !needsURLResolutionForInlineStyle(other, other.document(), document()))
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    AttributeCollection attributes = m_elementData->attributes();
    AttributeCollection::const_iterator end = attributes.end();
    for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it)
        attributeChangedFromParserOrByCloning(it->name(), it->value(), ModifiedByCloning);
}

void Element::cloneDataFromElement(const Element& other)
{
    cloneAttributesFromElement(other);
    copyNonAttributePropertiesFromElement(other);
}

void Element::createUniqueElementData()
{
    if (!m_elementData)
        m_elementData = UniqueElementData::create();
    else {
        ASSERT(!m_elementData->isUnique());
        m_elementData = static_cast<ShareableElementData*>(m_elementData.get())->makeUniqueCopy();
    }
}

InputMethodContext& Element::inputMethodContext()
{
    return ensureElementRareData().ensureInputMethodContext(toHTMLElement(this));
}

bool Element::hasInputMethodContext() const
{
    return hasRareData() && elementRareData()->hasInputMethodContext();
}

void Element::synchronizeStyleAttributeInternal() const
{
    ASSERT(isStyledElement());
    ASSERT(elementData());
    ASSERT(elementData()->m_styleAttributeIsDirty);
    elementData()->m_styleAttributeIsDirty = false;
    const StylePropertySet* inlineStyle = this->inlineStyle();
    const_cast<Element*>(this)->setSynchronizedLazyAttribute(styleAttr,
        inlineStyle ? AtomicString(inlineStyle->asText()) : nullAtom);
}

CSSStyleDeclaration* Element::style()
{
    if (!isStyledElement())
        return 0;
    return &ensureElementRareData().ensureInlineCSSStyleDeclaration(this);
}

MutableStylePropertySet& Element::ensureMutableInlineStyle()
{
    ASSERT(isStyledElement());
    RefPtr<StylePropertySet>& inlineStyle = ensureUniqueElementData().m_inlineStyle;
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
    ASSERT(isStyledElement());
    RefPtr<StylePropertySet>& inlineStyle = elementData()->m_inlineStyle;

    // Avoid redundant work if we're using shared attribute data with already parsed inline style.
    if (inlineStyle && !elementData()->isUnique())
        return;

    // We reconstruct the property set instead of mutating if there is no CSSOM wrapper.
    // This makes wrapperless property sets immutable and so cacheable.
    if (inlineStyle && !inlineStyle->isMutable())
        inlineStyle.clear();

    if (!inlineStyle) {
        inlineStyle = BisonCSSParser::parseInlineStyleDeclaration(newStyleString, this);
    } else {
        ASSERT(inlineStyle->isMutable());
        static_pointer_cast<MutableStylePropertySet>(inlineStyle)->parseDeclaration(newStyleString, document().elementSheet().contents());
    }
}

void Element::styleAttributeChanged(const AtomicString& newStyleString, AttributeModificationReason modificationReason)
{
    ASSERT(isStyledElement());
    WTF::OrdinalNumber startLineNumber = WTF::OrdinalNumber::beforeFirst();
    if (document().scriptableDocumentParser() && !document().isInDocumentWrite())
        startLineNumber = document().scriptableDocumentParser()->lineNumber();

    if (newStyleString.isNull()) {
        ensureUniqueElementData().m_inlineStyle.clear();
    } else if (modificationReason == ModifiedByCloning || document().contentSecurityPolicy()->allowInlineStyle(document().url(), startLineNumber)) {
        setInlineStyleFromString(newStyleString);
    }

    elementData()->m_styleAttributeIsDirty = false;

    setNeedsStyleRecalc(LocalStyleChange);
    InspectorInstrumentation::didInvalidateStyleAttr(this);
}

void Element::inlineStyleChanged()
{
    ASSERT(isStyledElement());
    setNeedsStyleRecalc(LocalStyleChange);
    ASSERT(elementData());
    elementData()->m_styleAttributeIsDirty = true;
    InspectorInstrumentation::didInvalidateStyleAttr(this);
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    ASSERT(isStyledElement());
    ensureMutableInlineStyle().setProperty(propertyID, cssValuePool().createIdentifierValue(identifier), important);
    inlineStyleChanged();
    return true;
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, double value, CSSPrimitiveValue::UnitType unit, bool important)
{
    ASSERT(isStyledElement());
    ensureMutableInlineStyle().setProperty(propertyID, cssValuePool().createValue(value, unit), important);
    inlineStyleChanged();
    return true;
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, const String& value, bool important)
{
    ASSERT(isStyledElement());
    bool changes = ensureMutableInlineStyle().setProperty(propertyID, value, important, document().elementSheet().contents());
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool Element::removeInlineStyleProperty(CSSPropertyID propertyID)
{
    ASSERT(isStyledElement());
    if (!inlineStyle())
        return false;
    bool changes = ensureMutableInlineStyle().removeProperty(propertyID);
    if (changes)
        inlineStyleChanged();
    return changes;
}

void Element::removeAllInlineStyleProperties()
{
    ASSERT(isStyledElement());
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
    ASSERT(isStyledElement());
    style->setProperty(propertyID, cssValuePool().createIdentifierValue(identifier));
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, double value, CSSPrimitiveValue::UnitType unit)
{
    ASSERT(isStyledElement());
    style->setProperty(propertyID, cssValuePool().createValue(value, unit));
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, const String& value)
{
    ASSERT(isStyledElement());
    style->setProperty(propertyID, value, false);
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
    if (hasActiveAnimations())
        return false;
    // Turn off style sharing for elements that can gain layers for reasons outside of the style system.
    // See comments in RenderObject::setStyle().
    // FIXME: Why does gaining a layer from outside the style system require disabling sharing?
    if (isHTMLFrameElementBase(*this)
        || isHTMLEmbedElement(*this)
        || isHTMLObjectElement(*this)
        || isHTMLAppletElement(*this)
        || isHTMLCanvasElement(*this))
        return false;
    if (FullscreenElementStack::isActiveFullScreenElement(this))
        return false;
    return true;
}

void Element::trace(Visitor* visitor)
{
    if (hasRareData())
        visitor->trace(elementRareData());

    ContainerNode::trace(visitor);
}

} // namespace WebCore
