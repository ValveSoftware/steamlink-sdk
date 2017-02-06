// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/TopControls.h"
#include "core/layout/LayoutBox.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ViewportScrollCallback.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

namespace {

ScrollableArea* scrollableAreaFor(const Element& element)
{
    if (!element.layoutObject() || !element.layoutObject()->isBox())
        return nullptr;

    LayoutBox* box = toLayoutBox(element.layoutObject());

    if (box->isDocumentElement())
        return element.document().view()->getScrollableArea();

    return static_cast<PaintInvalidationCapableScrollableArea*>(
        box->getScrollableArea());
}

bool fillsViewport(const Element& element)
{
    DCHECK(element.layoutObject());
    DCHECK(element.layoutObject()->isBox());

    LayoutObject* layoutObject = element.layoutObject();

    // TODO(bokan): Broken for OOPIF.
    Document& topDocument = element.document().topDocument();

    Vector<FloatQuad> quads;
    layoutObject->absoluteQuads(quads);
    DCHECK_EQ(quads.size(), 1u);

    if (!quads[0].isRectilinear())
        return false;

    LayoutRect boundingBox(quads[0].boundingBox());

    return boundingBox.location() == LayoutPoint::zero()
        && boundingBox.size() == topDocument.layoutViewItem().size();
}

bool isValidRootScroller(const Element& element)
{
    if (!element.layoutObject())
        return false;

    if (!scrollableAreaFor(element))
        return false;

    if (!fillsViewport(element))
        return false;

    return true;
}

} // namespace

ViewportScrollCallback* RootScrollerController::createViewportApplyScroll(
    TopControls* topControls, OverscrollController* overscrollController)
{
    return new ViewportScrollCallback(topControls, overscrollController);
}

RootScrollerController::RootScrollerController(Document& document, ViewportScrollCallback* applyScrollCallback)
    : m_document(&document)
    , m_viewportApplyScroll(applyScrollCallback)
{
}

DEFINE_TRACE(RootScrollerController)
{
    visitor->trace(m_document);
    visitor->trace(m_viewportApplyScroll);
    visitor->trace(m_rootScroller);
    visitor->trace(m_effectiveRootScroller);
}

void RootScrollerController::set(Element* newRootScroller)
{
    m_rootScroller = newRootScroller;
    updateEffectiveRootScroller();
}

Element* RootScrollerController::get() const
{
    return m_rootScroller;
}

Element* RootScrollerController::effectiveRootScroller() const
{
    return m_effectiveRootScroller;
}

void RootScrollerController::didUpdateLayout()
{
    updateEffectiveRootScroller();
}

void RootScrollerController::updateEffectiveRootScroller()
{
    bool rootScrollerValid =
        m_rootScroller && isValidRootScroller(*m_rootScroller);

    Element* newEffectiveRootScroller = rootScrollerValid
        ? m_rootScroller.get()
        : defaultEffectiveRootScroller();

    if (m_effectiveRootScroller == newEffectiveRootScroller)
        return;

    moveViewportApplyScroll(newEffectiveRootScroller);
    m_effectiveRootScroller = newEffectiveRootScroller;
}

void RootScrollerController::moveViewportApplyScroll(Element* target)
{
    if (!m_viewportApplyScroll)
        return;

    if (m_effectiveRootScroller)
        m_effectiveRootScroller->removeApplyScroll();

    ScrollableArea* targetScroller =
        target ? scrollableAreaFor(*target) : nullptr;

    if (targetScroller) {
        // Use disable-native-scroll since the ViewportScrollCallback needs to
        // apply scroll actions both before (TopControls) and after (overscroll)
        // scrolling the element so it will apply scroll to the element itself.
        target->setApplyScroll(
            m_viewportApplyScroll,
            "disable-native-scroll");
    }

    // Ideally, scroll customization would pass the current element to scroll to
    // the apply scroll callback but this doesn't happen today so we set it
    // through a back door here.
    m_viewportApplyScroll->setScroller(targetScroller);
}

Element* RootScrollerController::defaultEffectiveRootScroller()
{
    DCHECK(m_document);
    return m_document->documentElement();
}

} // namespace blink
