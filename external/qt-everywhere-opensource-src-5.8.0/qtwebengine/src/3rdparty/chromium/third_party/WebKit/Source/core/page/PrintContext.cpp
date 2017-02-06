/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Apple Inc.
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

#include "core/page/PrintContext.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

// By shrinking to a width of 75% (1.333f) we will render the correct physical
// dimensions in paged media (i.e. cm, pt,). The shrinkage used
// to be 80% (1.25f) to match other browsers - they have since moved on.
// Wide pages will be scaled down more than this.
const float printingMinimumShrinkFactor = 1.333f;

// This number determines how small we are willing to reduce the page content
// in order to accommodate the widest line. If the page would have to be
// reduced smaller to make the widest line fit, we just clip instead (this
// behavior matches MacIE and Mozilla, at least).
// TODO(rhogan): Decide if this quirk is still required.
const float printingMaximumShrinkFactor = 2;

PrintContext::PrintContext(LocalFrame* frame)
    : m_frame(frame)
    , m_isPrinting(false)
    , m_linkedDestinationsValid(false)
{
}

PrintContext::~PrintContext()
{
    if (m_isPrinting)
        end();
}

void PrintContext::computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
{
    m_pageRects.clear();
    outPageHeight = 0;

    if (!m_frame->document() || !m_frame->view() || m_frame->document()->layoutViewItem().isNull())
        return;

    if (userScaleFactor <= 0) {
        DLOG(ERROR) << "userScaleFactor has bad value " << userScaleFactor;
        return;
    }

    LayoutViewItem view = m_frame->document()->layoutViewItem();
    const IntRect& documentRect = view.documentRect();
    FloatSize pageSize = m_frame->resizePageRectsKeepingRatio(FloatSize(printRect.width(), printRect.height()), FloatSize(documentRect.width(), documentRect.height()));
    float pageWidth = pageSize.width();
    float pageHeight = pageSize.height();

    outPageHeight = pageHeight; // this is the height of the page adjusted by margins
    pageHeight -= headerHeight + footerHeight;

    if (pageHeight <= 0) {
        DLOG(ERROR) << "pageHeight has bad value " << pageHeight;
        return;
    }

    computePageRectsWithPageSizeInternal(FloatSize(pageWidth / userScaleFactor, pageHeight / userScaleFactor));
}

void PrintContext::computePageRectsWithPageSize(const FloatSize& pageSizeInPixels)
{
    m_pageRects.clear();
    computePageRectsWithPageSizeInternal(pageSizeInPixels);
}

void PrintContext::computePageRectsWithPageSizeInternal(const FloatSize& pageSizeInPixels)
{
    if (!m_frame->document() || !m_frame->view() || m_frame->document()->layoutViewItem().isNull())
        return;

    LayoutViewItem view = m_frame->document()->layoutViewItem();

    IntRect docRect = view.documentRect();

    int pageWidth = pageSizeInPixels.width();
    // We scaled with floating point arithmetic and need to ensure results like 13329.99
    // are treated as 13330 so that we don't mistakenly assign an extra page for the
    // stray pixel.
    int pageHeight = pageSizeInPixels.height() + LayoutUnit::epsilon();

    bool isHorizontal = view.style()->isHorizontalWritingMode();

    int docLogicalHeight = isHorizontal ? docRect.height() : docRect.width();
    int pageLogicalHeight = isHorizontal ? pageHeight : pageWidth;
    int pageLogicalWidth = isHorizontal ? pageWidth : pageHeight;

    int inlineDirectionStart;
    int inlineDirectionEnd;
    int blockDirectionStart;
    int blockDirectionEnd;
    if (isHorizontal) {
        if (view.style()->isFlippedBlocksWritingMode()) {
            blockDirectionStart = docRect.maxY();
            blockDirectionEnd = docRect.y();
        } else {
            blockDirectionStart = docRect.y();
            blockDirectionEnd = docRect.maxY();
        }
        inlineDirectionStart = view.style()->isLeftToRightDirection() ? docRect.x() : docRect.maxX();
        inlineDirectionEnd = view.style()->isLeftToRightDirection() ? docRect.maxX() : docRect.x();
    } else {
        if (view.style()->isFlippedBlocksWritingMode()) {
            blockDirectionStart = docRect.maxX();
            blockDirectionEnd = docRect.x();
        } else {
            blockDirectionStart = docRect.x();
            blockDirectionEnd = docRect.maxX();
        }
        inlineDirectionStart = view.style()->isLeftToRightDirection() ? docRect.y() : docRect.maxY();
        inlineDirectionEnd = view.style()->isLeftToRightDirection() ? docRect.maxY() : docRect.y();
    }

    unsigned pageCount = ceilf((float)docLogicalHeight / pageLogicalHeight);
    for (unsigned i = 0; i < pageCount; ++i) {
        int pageLogicalTop = blockDirectionEnd > blockDirectionStart ?
            blockDirectionStart + i * pageLogicalHeight :
            blockDirectionStart - (i + 1) * pageLogicalHeight;

        int pageLogicalLeft = inlineDirectionEnd > inlineDirectionStart ? inlineDirectionStart : inlineDirectionStart - pageLogicalWidth;
        IntRect pageRect(pageLogicalLeft, pageLogicalTop, pageLogicalWidth, pageLogicalHeight);
        if (!isHorizontal)
            pageRect = pageRect.transposedRect();
        m_pageRects.append(pageRect);

    }
}

void PrintContext::begin(float width, float height)
{
    ASSERT(width > 0);
    ASSERT(height > 0);

    // This function can be called multiple times to adjust printing parameters without going back to screen mode.
    m_isPrinting = true;

    FloatSize originalPageSize = FloatSize(width, height);
    FloatSize minLayoutSize = m_frame->resizePageRectsKeepingRatio(originalPageSize, FloatSize(width * printingMinimumShrinkFactor, height * printingMinimumShrinkFactor));

    // This changes layout, so callers need to make sure that they don't paint to screen while in printing mode.
    m_frame->setPrinting(true, minLayoutSize, originalPageSize, printingMaximumShrinkFactor / printingMinimumShrinkFactor);
}

void PrintContext::end()
{
    ASSERT(m_isPrinting);
    m_isPrinting = false;
    m_frame->setPrinting(false, FloatSize(), FloatSize(), 0);
    m_linkedDestinations.clear();
    m_linkedDestinationsValid = false;
}

static LayoutBoxModelObject* enclosingBoxModelObject(LayoutObject* object)
{

    while (object && !object->isBoxModelObject())
        object = object->parent();
    if (!object)
        return nullptr;
    return toLayoutBoxModelObject(object);
}

int PrintContext::pageNumberForElement(Element* element, const FloatSize& pageSizeInPixels)
{
    element->document().updateStyleAndLayout();

    LocalFrame* frame = element->document().frame();
    FloatRect pageRect(FloatPoint(0, 0), pageSizeInPixels);
    PrintContext printContext(frame);
    printContext.begin(pageRect.width(), pageRect.height());

    LayoutBoxModelObject* box = enclosingBoxModelObject(element->layoutObject());
    if (!box)
        return -1;

    FloatSize scaledPageSize = pageSizeInPixels;
    scaledPageSize.scale(frame->view()->contentsSize().width() / pageRect.width());
    printContext.computePageRectsWithPageSize(scaledPageSize);

    int top = box->pixelSnappedOffsetTop(box->offsetParent());
    int left = box->pixelSnappedOffsetLeft(box->offsetParent());
    size_t pageNumber = 0;
    for (; pageNumber < printContext.pageCount(); pageNumber++) {
        const IntRect& page = printContext.pageRect(pageNumber);
        if (page.x() <= left && left < page.maxX() && page.y() <= top && top < page.maxY())
            return pageNumber;
    }
    return -1;
}

void PrintContext::collectLinkedDestinations(Node* node)
{
    for (Node* i = node->firstChild(); i; i = i->nextSibling())
        collectLinkedDestinations(i);

    if (!node->isLink() || !node->isElementNode())
        return;
    const AtomicString& href = toElement(node)->getAttribute(HTMLNames::hrefAttr);
    if (href.isNull())
        return;
    KURL url = node->document().completeURL(href);
    if (!url.isValid())
        return;

    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, node->document().baseURL())) {
        String name = url.fragmentIdentifier();
        if (Element* element = node->document().findAnchor(name))
            m_linkedDestinations.set(name, element);
    }
}

void PrintContext::outputLinkedDestinations(GraphicsContext& context, const IntRect& pageRect)
{
    if (!m_linkedDestinationsValid) {
        // Collect anchors in the top-level frame only because our PrintContext
        // supports only one namespace for the anchors.
        collectLinkedDestinations(frame()->document());
        m_linkedDestinationsValid = true;
    }

    for (const auto& entry : m_linkedDestinations) {
        LayoutObject* layoutObject = entry.value->layoutObject();
        if (!layoutObject || !layoutObject->frameView())
            continue;
        IntRect boundingBox = layoutObject->absoluteBoundingBoxRect();
        // TODO(bokan): boundingBox looks to be in content coordinates but
        // convertToRootFrame doesn't apply scroll offsets when converting up to
        // the root frame.
        IntPoint point = layoutObject->frameView()->convertToRootFrame(boundingBox.location());
        if (!pageRect.contains(point))
            continue;
        point.clampNegativeToZero();
        context.setURLDestinationLocation(entry.key, point);
    }
}

String PrintContext::pageProperty(LocalFrame* frame, const char* propertyName, int pageNumber)
{
    Document* document = frame->document();
    PrintContext printContext(frame);
    // Any non-zero size is OK here. We don't care about actual layout. We just want to collect
    // @page rules and figure out what declarations apply on a given page (that may or may not exist).
    printContext.begin(800, 1000);
    RefPtr<ComputedStyle> style = document->styleForPage(pageNumber);

    // Implement formatters for properties we care about.
    if (!strcmp(propertyName, "margin-left")) {
        if (style->marginLeft().isAuto())
            return String("auto");
        return String::number(style->marginLeft().value());
    }
    if (!strcmp(propertyName, "line-height"))
        return String::number(style->lineHeight().value());
    if (!strcmp(propertyName, "font-size"))
        return String::number(style->getFontDescription().computedPixelSize());
    if (!strcmp(propertyName, "font-family"))
        return style->getFontDescription().family().family().getString();
    if (!strcmp(propertyName, "size"))
        return String::number(style->pageSize().width()) + ' ' + String::number(style->pageSize().height());

    return String("pageProperty() unimplemented for: ") + propertyName;
}

bool PrintContext::isPageBoxVisible(LocalFrame* frame, int pageNumber)
{
    return frame->document()->isPageBoxVisible(pageNumber);
}

String PrintContext::pageSizeAndMarginsInPixels(LocalFrame* frame, int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    IntSize pageSize(width, height);
    frame->document()->pageSizeAndMarginsInPixels(pageNumber, pageSize, marginTop, marginRight, marginBottom, marginLeft);

    return "(" + String::number(pageSize.width()) + ", " + String::number(pageSize.height()) + ") " +
        String::number(marginTop) + ' ' + String::number(marginRight) + ' ' + String::number(marginBottom) + ' ' + String::number(marginLeft);
}

int PrintContext::numberOfPages(LocalFrame* frame, const FloatSize& pageSizeInPixels)
{
    frame->document()->updateStyleAndLayout();

    FloatRect pageRect(FloatPoint(0, 0), pageSizeInPixels);
    PrintContext printContext(frame);
    printContext.begin(pageRect.width(), pageRect.height());
    // Account for shrink-to-fit.
    FloatSize scaledPageSize = pageSizeInPixels;
    scaledPageSize.scale(frame->view()->contentsSize().width() / pageRect.width());
    printContext.computePageRectsWithPageSize(scaledPageSize);
    return printContext.pageCount();
}

DEFINE_TRACE(PrintContext)
{
    visitor->trace(m_frame);
    visitor->trace(m_linkedDestinations);
}

} // namespace blink
