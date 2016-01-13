/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/accessibility/AXScrollView.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/accessibility/AXScrollbar.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"

namespace WebCore {

AXScrollView::AXScrollView(ScrollView* view)
    : m_scrollView(view)
    , m_childrenDirty(false)
{
}

AXScrollView::~AXScrollView()
{
    ASSERT(isDetached());
}

void AXScrollView::detach()
{
    AXObject::detach();
    m_scrollView = 0;
}

PassRefPtr<AXScrollView> AXScrollView::create(ScrollView* view)
{
    return adoptRef(new AXScrollView(view));
}

AXObject* AXScrollView::scrollBar(AccessibilityOrientation orientation)
{
    updateScrollbars();

    switch (orientation) {
    case AccessibilityOrientationVertical:
        return m_verticalScrollbar ? m_verticalScrollbar.get() : 0;
    case AccessibilityOrientationHorizontal:
        return m_horizontalScrollbar ? m_horizontalScrollbar.get() : 0;
    }

    return 0;
}

// If this is WebKit1 then the native scroll view needs to return the
// AX information (because there are no scroll bar children in the ScrollView object in WK1).
// In WebKit2, the ScrollView object will return the AX information (because there are no platform widgets).
bool AXScrollView::isAttachment() const
{
    return false;
}

Widget* AXScrollView::widgetForAttachmentView() const
{
    return m_scrollView;
}

void AXScrollView::updateChildrenIfNecessary()
{
    if (m_childrenDirty)
        clearChildren();

    if (!m_haveChildren)
        addChildren();

    updateScrollbars();
}

void AXScrollView::updateScrollbars()
{
    if (!m_scrollView)
        return;

    if (m_scrollView->horizontalScrollbar() && !m_horizontalScrollbar) {
        m_horizontalScrollbar = addChildScrollbar(m_scrollView->horizontalScrollbar());
    } else if (!m_scrollView->horizontalScrollbar() && m_horizontalScrollbar) {
        removeChildScrollbar(m_horizontalScrollbar.get());
        m_horizontalScrollbar = nullptr;
    }

    if (m_scrollView->verticalScrollbar() && !m_verticalScrollbar) {
        m_verticalScrollbar = addChildScrollbar(m_scrollView->verticalScrollbar());
    } else if (!m_scrollView->verticalScrollbar() && m_verticalScrollbar) {
        removeChildScrollbar(m_verticalScrollbar.get());
        m_verticalScrollbar = nullptr;
    }
}

void AXScrollView::removeChildScrollbar(AXObject* scrollbar)
{
    size_t pos = m_children.find(scrollbar);
    if (pos != kNotFound) {
        m_children[pos]->detachFromParent();
        m_children.remove(pos);
    }
}

AXScrollbar* AXScrollView::addChildScrollbar(Scrollbar* scrollbar)
{
    if (!scrollbar)
        return 0;

    AXScrollbar* scrollBarObject = toAXScrollbar(axObjectCache()->getOrCreate(scrollbar));
    scrollBarObject->setParent(this);
    m_children.append(scrollBarObject);
    return scrollBarObject;
}

void AXScrollView::clearChildren()
{
    AXObject::clearChildren();
    m_verticalScrollbar = nullptr;
    m_horizontalScrollbar = nullptr;
}

bool AXScrollView::computeAccessibilityIsIgnored() const
{
    AXObject* webArea = webAreaObject();
    if (!webArea)
        return true;

    return webArea->accessibilityIsIgnored();
}

void AXScrollView::addChildren()
{
    ASSERT(!m_haveChildren);
    m_haveChildren = true;

    AXObject* webArea = webAreaObject();
    if (webArea && !webArea->accessibilityIsIgnored())
        m_children.append(webArea);

    updateScrollbars();
}

AXObject* AXScrollView::webAreaObject() const
{
    if (!m_scrollView || !m_scrollView->isFrameView())
        return 0;

    Document* doc = toFrameView(m_scrollView)->frame().document();
    if (!doc || !doc->renderView())
        return 0;

    return axObjectCache()->getOrCreate(doc);
}

AXObject* AXScrollView::accessibilityHitTest(const IntPoint& point) const
{
    AXObject* webArea = webAreaObject();
    if (!webArea)
        return 0;

    if (m_horizontalScrollbar && m_horizontalScrollbar->elementRect().contains(point))
        return m_horizontalScrollbar.get();
    if (m_verticalScrollbar && m_verticalScrollbar->elementRect().contains(point))
        return m_verticalScrollbar.get();

    return webArea->accessibilityHitTest(point);
}

LayoutRect AXScrollView::elementRect() const
{
    if (!m_scrollView)
        return LayoutRect();

    return m_scrollView->frameRect();
}

FrameView* AXScrollView::documentFrameView() const
{
    if (!m_scrollView || !m_scrollView->isFrameView())
        return 0;

    return toFrameView(m_scrollView);
}

AXObject* AXScrollView::parentObject() const
{
    if (!m_scrollView || !m_scrollView->isFrameView())
        return 0;

    // FIXME: Broken for OOPI.
    HTMLFrameOwnerElement* owner = toFrameView(m_scrollView)->frame().deprecatedLocalOwner();
    if (owner && owner->renderer())
        return axObjectCache()->getOrCreate(owner);

    return 0;
}

AXObject* AXScrollView::parentObjectIfExists() const
{
    if (!m_scrollView || !m_scrollView->isFrameView())
        return 0;

    HTMLFrameOwnerElement* owner = toFrameView(m_scrollView)->frame().deprecatedLocalOwner();
    if (owner && owner->renderer())
        return axObjectCache()->get(owner);

    return 0;
}

ScrollableArea* AXScrollView::getScrollableAreaIfScrollable() const
{
    return m_scrollView;
}

void AXScrollView::scrollTo(const IntPoint& point) const
{
    if (m_scrollView)
        m_scrollView->setScrollPosition(point);
}

} // namespace WebCore
