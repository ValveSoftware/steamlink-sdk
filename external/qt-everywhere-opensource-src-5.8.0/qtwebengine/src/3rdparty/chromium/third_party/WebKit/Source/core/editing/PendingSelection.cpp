/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "core/editing/PendingSelection.h"

#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/LayoutView.h"

namespace blink {

PendingSelection::PendingSelection(FrameSelection& frameSelection)
    : m_frameSelection(&frameSelection)
    , m_hasPendingSelection(false)
{
}

const VisibleSelection& PendingSelection::visibleSelection() const
{
    return m_frameSelection->selection();
}

static bool isSelectionInDocument(const VisibleSelectionInFlatTree& visibleSelection, const Document& document)
{
    const PositionInFlatTree& start = visibleSelection.start();
    if (start.isNotNull() && (!start.inShadowIncludingDocument() || start.document() != document))
        return false;
    const PositionInFlatTree& end = visibleSelection.end();
    if (end.isNotNull() && (!end.inShadowIncludingDocument() || end.document() != document))
        return false;
    const PositionInFlatTree extent = visibleSelection.extent();
    if (extent.isNotNull() && (!extent.inShadowIncludingDocument() || extent.document() != document))
        return false;
    return true;
}

VisibleSelectionInFlatTree PendingSelection::calcVisibleSelection(const VisibleSelectionInFlatTree& originalSelection) const
{
    const PositionInFlatTree& start = originalSelection.start();
    const PositionInFlatTree& end = originalSelection.end();
    SelectionType selectionType = originalSelection.getSelectionType();
    const TextAffinity affinity = originalSelection.affinity();

    bool paintBlockCursor = m_frameSelection->shouldShowBlockCursor() && selectionType == SelectionType::CaretSelection && !isLogicalEndOfLine(createVisiblePosition(end, affinity));
    VisibleSelectionInFlatTree selection;
    if (enclosingTextFormControl(start.computeContainerNode())) {
        // TODO(yosin) We should use |PositionMoveType::CodePoint| to avoid
        // ending paint at middle of character.
        PositionInFlatTree endPosition = paintBlockCursor ? nextPositionOf(originalSelection.extent(), PositionMoveType::CodeUnit) : end;
        selection.setWithoutValidation(start, endPosition);
        return selection;
    }

    const VisiblePositionInFlatTree& visibleStart = createVisiblePosition(start, selectionType == SelectionType::RangeSelection ? TextAffinity::Downstream : affinity);
    if (paintBlockCursor) {
        VisiblePositionInFlatTree visibleExtent = createVisiblePosition(end, affinity);
        visibleExtent = nextPositionOf(visibleExtent, CanSkipOverEditingBoundary);
        return VisibleSelectionInFlatTree(visibleStart, visibleExtent);
    }
    const VisiblePositionInFlatTree visibleEnd = createVisiblePosition(end, selectionType == SelectionType::RangeSelection ? TextAffinity::Upstream : affinity);
    return VisibleSelectionInFlatTree(visibleStart, visibleEnd);
}

void PendingSelection::commit(LayoutView& layoutView)
{
    if (!hasPendingSelection())
        return;
    DCHECK(!layoutView.needsLayout());
    m_hasPendingSelection = false;

    const VisibleSelectionInFlatTree& originalSelection = m_frameSelection->visibleSelection<EditingInFlatTreeStrategy>();

    // Skip if pending VisibilePositions became invalid before we reach here.
    if (!isSelectionInDocument(originalSelection, layoutView.document()))
        return;

    // Construct a new VisibleSolution, since visibleSelection() is not necessarily
    // valid, and the following steps assume a valid selection.
    // See <https://bugs.webkit.org/show_bug.cgi?id=69563> and
    // <rdar://problem/10232866>.
    const VisibleSelectionInFlatTree& selection = calcVisibleSelection(originalSelection);

    if (!selection.isRange()) {
        layoutView.clearSelection();
        return;
    }

    // Use the rightmost candidate for the start of the selection, and the
    // leftmost candidate for the end of the selection. Example: foo <a>bar</a>.
    // Imagine that a line wrap occurs after 'foo', and that 'bar' is selected.
    // If we pass [foo, 3] as the start of the selection, the selection painting
    // code will think that content on the line containing 'foo' is selected
    // and will fill the gap before 'bar'.
    PositionInFlatTree startPos = selection.start();
    PositionInFlatTree candidate = mostForwardCaretPosition(startPos);
    if (isVisuallyEquivalentCandidate(candidate))
        startPos = candidate;
    PositionInFlatTree endPos = selection.end();
    candidate = mostBackwardCaretPosition(endPos);
    if (isVisuallyEquivalentCandidate(candidate))
        endPos = candidate;

    // We can get into a state where the selection endpoints map to the same
    // |VisiblePosition| when a selection is deleted because we don't yet notify
    // the |FrameSelection| of text removal.
    if (startPos.isNull() || endPos.isNull() || selection.visibleStart().deepEquivalent() == selection.visibleEnd().deepEquivalent())
        return;
    LayoutObject* startLayoutObject = startPos.anchorNode()->layoutObject();
    LayoutObject* endLayoutObject = endPos.anchorNode()->layoutObject();
    if (!startLayoutObject || !endLayoutObject)
        return;
    DCHECK(layoutView == startLayoutObject->view());
    DCHECK(layoutView == endLayoutObject->view());
    layoutView.setSelection(startLayoutObject, startPos.computeEditingOffset(), endLayoutObject, endPos.computeEditingOffset());
}

DEFINE_TRACE(PendingSelection)
{
    visitor->trace(m_frameSelection);
}

} // namespace blink
