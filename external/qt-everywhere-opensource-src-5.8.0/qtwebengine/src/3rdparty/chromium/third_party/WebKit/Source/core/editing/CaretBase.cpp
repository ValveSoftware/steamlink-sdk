/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/CaretBase.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBlockItem.h"
#include "core/layout/api/LayoutItem.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

CaretBase::CaretBase(CaretVisibility visibility)
    : m_caretVisibility(visibility)
{
}

CaretBase::~CaretBase() = default;

DEFINE_TRACE(CaretBase)
{
}

void CaretBase::clearCaretRect()
{
    m_caretLocalRect = LayoutRect();
}

static inline bool caretRendersInsideNode(Node* node)
{
    return node && !isDisplayInsideTable(node) && !editingIgnoresContent(node);
}

LayoutBlock* CaretBase::caretLayoutObject(Node* node)
{
    if (!node)
        return nullptr;

    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return nullptr;

    // if caretNode is a block and caret is inside it then caret should be painted by that block
    bool paintedByBlock = layoutObject->isLayoutBlock() && caretRendersInsideNode(node);
    // TODO(yoichio): This function is called at least
    // DocumentLifeCycle::LayoutClean but caretRendersInsideNode above can
    // layout. Thus |node->layoutObject()| can be changed then this is bad
    // design. We should make caret painting algorithm clean.
    CHECK_EQ(layoutObject, node->layoutObject()) << "Layout tree should not changed";
    return paintedByBlock ? toLayoutBlock(layoutObject) : layoutObject->containingBlock();
}

static void mapCaretRectToCaretPainter(LayoutItem caretLayoutItem, LayoutBlockItem caretPainterItem, LayoutRect& caretRect)
{
    // FIXME: This shouldn't be called on un-rooted subtrees.
    // FIXME: This should probably just use mapLocalToAncestor.
    // Compute an offset between the caretLayoutItem and the caretPainterItem.

    DCHECK(caretLayoutItem.isDescendantOf(caretPainterItem));

    bool unrooted = false;
    while (caretLayoutItem != caretPainterItem) {
        LayoutItem containerItem = caretLayoutItem.container();
        if (containerItem.isNull()) {
            unrooted = true;
            break;
        }
        caretRect.move(caretLayoutItem.offsetFromContainer(containerItem));
        caretLayoutItem = containerItem;
    }

    if (unrooted)
        caretRect = LayoutRect();
}

bool CaretBase::updateCaretRect(const PositionWithAffinity& caretPosition)
{
    m_caretLocalRect = LayoutRect();

    if (caretPosition.position().isNull())
        return false;

    DCHECK(caretPosition.position().anchorNode()->layoutObject());

    // First compute a rect local to the layoutObject at the selection start.
    LayoutObject* layoutObject;
    m_caretLocalRect = localCaretRectOfPosition(caretPosition, layoutObject);

    // Get the layoutObject that will be responsible for painting the caret
    // (which is either the layoutObject we just found, or one of its containers).
    LayoutBlockItem caretPainterItem = LayoutBlockItem(caretLayoutObject(caretPosition.position().anchorNode()));

    mapCaretRectToCaretPainter(LayoutItem(layoutObject), caretPainterItem, m_caretLocalRect);

    return true;
}

bool CaretBase::updateCaretRect(const VisiblePosition& caretPosition)
{
    return updateCaretRect(caretPosition.toPositionWithAffinity());
}

IntRect CaretBase::absoluteBoundsForLocalRect(Node* node, const LayoutRect& rect) const
{
    LayoutBlock* caretPainter = caretLayoutObject(node);
    if (!caretPainter)
        return IntRect();

    LayoutRect localRect(rect);
    caretPainter->flipForWritingMode(localRect);
    return caretPainter->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

DisplayItemClient* CaretBase::displayItemClientForCaret(Node* node)
{
    LayoutBlock* caretLayoutBlock = caretLayoutObject(node);
    if (!caretLayoutBlock)
        return nullptr;
    if (caretLayoutBlock->usesCompositedScrolling())
        return static_cast<DisplayItemClient*>(caretLayoutBlock->layer()->graphicsLayerBackingForScrolling());
    return caretLayoutBlock;
}

// TODO(yoichio): |node| is FrameSelection::m_previousCaretNode and this is bad
// design. We should use only previous layoutObject or Rectangle to invalidate
// old caret.
void CaretBase::invalidateLocalCaretRect(Node* node, const LayoutRect& rect)
{
    LayoutBlock* caretLayoutBlock = caretLayoutObject(node);
    if (!caretLayoutBlock)
        return;

    // FIXME: Need to over-paint 1 pixel to workaround some rounding problems.
    // https://bugs.webkit.org/show_bug.cgi?id=108283
    LayoutRect inflatedRect = rect;
    inflatedRect.inflate(LayoutUnit(1));

    // FIXME: We should not allow paint invalidation out of paint invalidation state. crbug.com/457415
    DisablePaintInvalidationStateAsserts disabler;

    node->layoutObject()->invalidatePaintRectangle(inflatedRect, displayItemClientForCaret(node));
}

bool CaretBase::shouldRepaintCaret(Node& node) const
{
    // If PositionAnchorType::BeforeAnchor or PositionAnchorType::AfterAnchor,
    // carets need to be repainted not only when the node is contentEditable but
    // also when its parentNode() is contentEditable.
    return node.isContentEditable() || (node.parentNode() && node.parentNode()->isContentEditable());
}

bool CaretBase::shouldRepaintCaret(const LayoutViewItem view) const
{
    DCHECK(view);
    if (FrameView* frameView = view.frameView()) {
        LocalFrame& frame = frameView->frame(); // The frame where the selection started
        return frame.settings() && frame.settings()->caretBrowsingEnabled();
    }
    return false;
}

void CaretBase::invalidateCaretRect(Node* node, bool caretRectChanged)
{
    if (caretRectChanged)
        return;

    if (LayoutViewItem view = node->document().layoutViewItem()) {
        if (node->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable) || shouldRepaintCaret(view))
            invalidateLocalCaretRect(node, localCaretRectWithoutUpdate());
    }
}

void CaretBase::paintCaret(Node* node, GraphicsContext& context, const LayoutPoint& paintOffset, DisplayItem::Type displayItemType) const
{
    if (m_caretVisibility == CaretVisibility::Hidden)
        return;

    DisplayItemClient* displayItemClient = displayItemClientForCaret(node);
    if (!displayItemClient)
        return;

    if (DrawingRecorder::useCachedDrawingIfPossible(context, *displayItemClient, displayItemType))
        return;

    LayoutRect drawingRect = localCaretRectWithoutUpdate();
    if (LayoutBlock* layoutObject = caretLayoutObject(node))
        layoutObject->flipForWritingMode(drawingRect);
    drawingRect.moveBy(roundedIntPoint(paintOffset));

    Color caretColor = Color::black;

    Element* element;
    if (node->isElementNode())
        element = toElement(node);
    else
        element = node->parentElement();

    if (element && element->layoutObject())
        caretColor = element->layoutObject()->resolveColor(CSSPropertyColor);

    DrawingRecorder drawingRecorder(context, *displayItemClientForCaret(node), DisplayItem::Caret, FloatRect(drawingRect));

    context.fillRect(FloatRect(drawingRect), caretColor);
}

void CaretBase::setCaretVisibility(CaretVisibility visibility)
{
    m_caretVisibility = visibility;
}

} // namespace blink
