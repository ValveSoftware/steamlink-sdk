/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) CO. Ltd. All rights reserved.
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

#include "core/layout/svg/line/SVGRootInlineBox.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGTextLayoutEngine.h"
#include "core/layout/svg/line/SVGInlineFlowBox.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/paint/SVGRootInlineBoxPainter.h"

namespace blink {

void SVGRootInlineBox::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit, LayoutUnit) const
{
    SVGRootInlineBoxPainter(*this).paint(paintInfo, paintOffset);
}

void SVGRootInlineBox::markDirty()
{
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
        child->markDirty();
    RootInlineBox::markDirty();
}

void SVGRootInlineBox::computePerCharacterLayoutInformation()
{
    LayoutSVGText& textRoot = toLayoutSVGText(*LineLayoutAPIShim::layoutObjectFrom(block()));

    const Vector<LayoutSVGInlineText*>& descendantTextNodes = textRoot.descendantTextNodes();
    if (descendantTextNodes.isEmpty())
        return;

    if (textRoot.needsReordering())
        reorderValueLists();

    // Perform SVG text layout phase two (see SVGTextLayoutEngine for details).
    SVGTextLayoutEngine characterLayout(descendantTextNodes);
    characterLayout.layoutCharactersInTextBoxes(this);

    // Perform SVG text layout phase three (see SVGTextChunkBuilder for details).
    characterLayout.finishLayout();

    // Perform SVG text layout phase four
    // Position & resize all SVGInlineText/FlowBoxes in the inline box tree, resize the root box as well as the LayoutSVGText parent block.
    LayoutRect childRect;
    layoutChildBoxes(this, &childRect);
    layoutRootBox(childRect);
}

void SVGRootInlineBox::layoutChildBoxes(InlineFlowBox* start, LayoutRect* childRect)
{
    for (InlineBox* child = start->firstChild(); child; child = child->nextOnLine()) {
        LayoutRect boxRect;
        if (child->isSVGInlineTextBox()) {
            ASSERT(child->getLineLayoutItem().isSVGInlineText());

            SVGInlineTextBox* textBox = toSVGInlineTextBox(child);
            boxRect = textBox->calculateBoundaries();
            textBox->setX(boxRect.x());
            textBox->setY(boxRect.y());
            textBox->setLogicalWidth(boxRect.width());
            textBox->setLogicalHeight(boxRect.height());
        } else {
            // Skip generated content.
            if (!child->getLineLayoutItem().node())
                continue;

            SVGInlineFlowBox* flowBox = toSVGInlineFlowBox(child);
            layoutChildBoxes(flowBox);

            boxRect = flowBox->calculateBoundaries();
            flowBox->setX(boxRect.x());
            flowBox->setY(boxRect.y());
            flowBox->setLogicalWidth(boxRect.width());
            flowBox->setLogicalHeight(boxRect.height());
        }
        if (childRect)
            childRect->unite(boxRect);
    }
}

void SVGRootInlineBox::layoutRootBox(const LayoutRect& childRect)
{
    LineLayoutBlockFlow parentBlock = block();

    // Finally, assign the root block position, now that all content is laid out.
    LayoutRect boundingRect = childRect;
    parentBlock.setLocation(boundingRect.location());
    parentBlock.setSize(boundingRect.size());

    // Position all children relative to the parent block.
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        // Skip generated content.
        if (!child->getLineLayoutItem().node())
            continue;
        child->move(LayoutSize(-childRect.x(), -childRect.y()));
    }

    // Position ourselves.
    setX(LayoutUnit());
    setY(LayoutUnit());
    setLogicalWidth(childRect.width());
    setLogicalHeight(childRect.height());
    setLineTopBottomPositions(LayoutUnit(), boundingRect.height(), LayoutUnit(), boundingRect.height());
}

InlineBox* SVGRootInlineBox::closestLeafChildForPosition(const LayoutPoint& point)
{
    InlineBox* firstLeaf = firstLeafChild();
    InlineBox* lastLeaf = lastLeafChild();
    if (firstLeaf == lastLeaf)
        return firstLeaf;

    // FIXME: Check for vertical text!
    InlineBox* closestLeaf = nullptr;
    for (InlineBox* leaf = firstLeaf; leaf; leaf = leaf->nextLeafChild()) {
        if (!leaf->isSVGInlineTextBox())
            continue;
        if (point.y() < leaf->y())
            continue;
        if (point.y() > leaf->y() + leaf->virtualLogicalHeight())
            continue;

        closestLeaf = leaf;
        if (point.x() < leaf->left() + leaf->logicalWidth())
            return leaf;
    }

    return closestLeaf ? closestLeaf : lastLeaf;
}

static inline void swapPositioningValuesInTextBoxes(SVGInlineTextBox* firstTextBox, SVGInlineTextBox* lastTextBox)
{
    LineLayoutSVGInlineText firstTextNode = LineLayoutSVGInlineText(firstTextBox->getLineLayoutItem());
    SVGCharacterDataMap& firstCharacterDataMap = firstTextNode.characterDataMap();
    SVGCharacterDataMap::iterator itFirst = firstCharacterDataMap.find(firstTextBox->start() + 1);
    if (itFirst == firstCharacterDataMap.end())
        return;
    LineLayoutSVGInlineText lastTextNode = LineLayoutSVGInlineText(lastTextBox->getLineLayoutItem());
    SVGCharacterDataMap& lastCharacterDataMap = lastTextNode.characterDataMap();
    SVGCharacterDataMap::iterator itLast = lastCharacterDataMap.find(lastTextBox->start() + 1);
    if (itLast == lastCharacterDataMap.end())
        return;
    // We only want to perform the swap if both inline boxes are absolutely
    // positioned.
    std::swap(itFirst->value, itLast->value);
}

static inline void reverseInlineBoxRangeAndValueListsIfNeeded(Vector<InlineBox*>::iterator first, Vector<InlineBox*>::iterator last)
{
    // This is a copy of std::reverse(first, last). It additionally assures
    // that the metrics map within the layoutObjects belonging to the
    // InlineBoxes are reordered as well.
    while (true)  {
        if (first == last || first == --last)
            return;

        if ((*last)->isSVGInlineTextBox() && (*first)->isSVGInlineTextBox()) {
            SVGInlineTextBox* firstTextBox = toSVGInlineTextBox(*first);
            SVGInlineTextBox* lastTextBox = toSVGInlineTextBox(*last);

            // Reordering is only necessary for BiDi text that is _absolutely_ positioned.
            if (firstTextBox->len() == 1 && firstTextBox->len() == lastTextBox->len())
                swapPositioningValuesInTextBoxes(firstTextBox, lastTextBox);
        }

        InlineBox* temp = *first;
        *first = *last;
        *last = temp;
        ++first;
    }
}

void SVGRootInlineBox::reorderValueLists()
{
    Vector<InlineBox*> leafBoxesInLogicalOrder;
    collectLeafBoxesInLogicalOrder(leafBoxesInLogicalOrder, reverseInlineBoxRangeAndValueListsIfNeeded);
}

bool SVGRootInlineBox::nodeAtPoint(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    for (InlineBox* leaf = firstLeafChild(); leaf; leaf = leaf->nextLeafChild()) {
        if (!leaf->isSVGInlineTextBox())
            continue;
        if (leaf->nodeAtPoint(result, locationInContainer, accumulatedOffset, lineTop, lineBottom))
            return true;
    }

    return false;
}

} // namespace blink
