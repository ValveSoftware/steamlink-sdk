/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/DeleteSelectionCommand.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingBoundary.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/HTMLTableRowElement.h"
#include "core/layout/LayoutTableCell.h"

namespace blink {

using namespace HTMLNames;

static bool isTableCellEmpty(Node* cell)
{
    DCHECK(isTableCell(cell)) << cell;
    return VisiblePosition::firstPositionInNode(cell).deepEquivalent() == VisiblePosition::lastPositionInNode(cell).deepEquivalent();
}

static bool isTableRowEmpty(Node* row)
{
    if (!isHTMLTableRowElement(row))
        return false;

    for (Node* child = row->firstChild(); child; child = child->nextSibling()) {
        if (isTableCell(child) && !isTableCellEmpty(child))
            return false;
    }
    return true;
}

DeleteSelectionCommand::DeleteSelectionCommand(Document& document, bool smartDelete, bool mergeBlocksAfterDelete, bool expandForSpecialElements, bool sanitizeMarkup)
    : CompositeEditCommand(document)
    , m_hasSelectionToDelete(false)
    , m_smartDelete(smartDelete)
    , m_mergeBlocksAfterDelete(mergeBlocksAfterDelete)
    , m_needPlaceholder(false)
    , m_expandForSpecialElements(expandForSpecialElements)
    , m_pruneStartBlockIfNecessary(false)
    , m_startsAtEmptyLine(false)
    , m_sanitizeMarkup(sanitizeMarkup)
    , m_startBlock(nullptr)
    , m_endBlock(nullptr)
    , m_typingStyle(nullptr)
    , m_deleteIntoBlockquoteStyle(nullptr)
{
}

DeleteSelectionCommand::DeleteSelectionCommand(const VisibleSelection& selection, bool smartDelete, bool mergeBlocksAfterDelete, bool expandForSpecialElements, bool sanitizeMarkup)
    : CompositeEditCommand(*selection.start().document())
    , m_hasSelectionToDelete(true)
    , m_smartDelete(smartDelete)
    , m_mergeBlocksAfterDelete(mergeBlocksAfterDelete)
    , m_needPlaceholder(false)
    , m_expandForSpecialElements(expandForSpecialElements)
    , m_pruneStartBlockIfNecessary(false)
    , m_startsAtEmptyLine(false)
    , m_sanitizeMarkup(sanitizeMarkup)
    , m_selectionToDelete(selection)
    , m_startBlock(nullptr)
    , m_endBlock(nullptr)
    , m_typingStyle(nullptr)
    , m_deleteIntoBlockquoteStyle(nullptr)
{
}

void DeleteSelectionCommand::initializeStartEnd(Position& start, Position& end)
{
    HTMLElement* startSpecialContainer = nullptr;
    HTMLElement* endSpecialContainer = nullptr;

    start = m_selectionToDelete.start();
    end = m_selectionToDelete.end();

    // For HRs, we'll get a position at (HR,1) when hitting delete from the beginning of the previous line, or (HR,0) when forward deleting,
    // but in these cases, we want to delete it, so manually expand the selection
    if (isHTMLHRElement(*start.anchorNode()))
        start = Position::beforeNode(start.anchorNode());
    else if (isHTMLHRElement(*end.anchorNode()))
        end = Position::afterNode(end.anchorNode());

    // FIXME: This is only used so that moveParagraphs can avoid the bugs in special element expansion.
    if (!m_expandForSpecialElements)
        return;

    while (1) {
        startSpecialContainer = 0;
        endSpecialContainer = 0;

        Position s = positionBeforeContainingSpecialElement(start, &startSpecialContainer);
        Position e = positionAfterContainingSpecialElement(end, &endSpecialContainer);

        if (!startSpecialContainer && !endSpecialContainer)
            break;

        if (createVisiblePosition(start).deepEquivalent() != m_selectionToDelete.visibleStart().deepEquivalent() || createVisiblePosition(end).deepEquivalent() != m_selectionToDelete.visibleEnd().deepEquivalent())
            break;

        // If we're going to expand to include the startSpecialContainer, it must be fully selected.
        if (startSpecialContainer && !endSpecialContainer && comparePositions(Position::inParentAfterNode(*startSpecialContainer), end) > -1)
            break;

        // If we're going to expand to include the endSpecialContainer, it must be fully selected.
        if (endSpecialContainer && !startSpecialContainer && comparePositions(start, Position::inParentBeforeNode(*endSpecialContainer)) > -1)
            break;

        if (startSpecialContainer && startSpecialContainer->isDescendantOf(endSpecialContainer)) {
            // Don't adjust the end yet, it is the end of a special element that contains the start
            // special element (which may or may not be fully selected).
            start = s;
        } else if (endSpecialContainer && endSpecialContainer->isDescendantOf(startSpecialContainer)) {
            // Don't adjust the start yet, it is the start of a special element that contains the end
            // special element (which may or may not be fully selected).
            end = e;
        } else {
            start = s;
            end = e;
        }
    }
}

void DeleteSelectionCommand::setStartingSelectionOnSmartDelete(const Position& start, const Position& end)
{
    bool isBaseFirst = startingSelection().isBaseFirst();
    VisiblePosition newBase = createVisiblePosition(isBaseFirst ? start : end);
    VisiblePosition newExtent = createVisiblePosition(isBaseFirst ? end : start);
    setStartingSelection(VisibleSelection(newBase, newExtent, startingSelection().isDirectional()));
}

void DeleteSelectionCommand::initializePositionData(EditingState* editingState)
{
    Position start, end;
    initializeStartEnd(start, end);
    DCHECK(start.isNotNull());
    DCHECK(end.isNotNull());
    if (!isEditablePosition(start, ContentIsEditable)) {
        editingState->abort();
        return;
    }
    if (!isEditablePosition(end, ContentIsEditable)) {
        Node* highestRoot = highestEditableRoot(start);
        DCHECK(highestRoot);
        end = lastEditablePositionBeforePositionInRoot(end, *highestRoot);
    }

    m_upstreamStart = mostBackwardCaretPosition(start);
    m_downstreamStart = mostForwardCaretPosition(start);
    m_upstreamEnd = mostBackwardCaretPosition(end);
    m_downstreamEnd = mostForwardCaretPosition(end);

    m_startRoot = rootEditableElementOf(start);
    m_endRoot = rootEditableElementOf(end);

    m_startTableRow = toHTMLTableRowElement(enclosingNodeOfType(start, &isHTMLTableRowElement));
    m_endTableRow = toHTMLTableRowElement(enclosingNodeOfType(end, &isHTMLTableRowElement));

    // Don't move content out of a table cell.
    // If the cell is non-editable, enclosingNodeOfType won't return it by default, so
    // tell that function that we don't care if it returns non-editable nodes.
    Node* startCell = enclosingNodeOfType(m_upstreamStart, &isTableCell, CanCrossEditingBoundary);
    Node* endCell = enclosingNodeOfType(m_downstreamEnd, &isTableCell, CanCrossEditingBoundary);
    // FIXME: This isn't right.  A borderless table with two rows and a single column would appear as two paragraphs.
    if (endCell && endCell != startCell)
        m_mergeBlocksAfterDelete = false;

    // Usually the start and the end of the selection to delete are pulled together as a result of the deletion.
    // Sometimes they aren't (like when no merge is requested), so we must choose one position to hold the caret
    // and receive the placeholder after deletion.
    VisiblePosition visibleEnd = createVisiblePosition(m_downstreamEnd);
    if (m_mergeBlocksAfterDelete && !isEndOfParagraph(visibleEnd))
        m_endingPosition = m_downstreamEnd;
    else
        m_endingPosition = m_downstreamStart;

    // We don't want to merge into a block if it will mean changing the quote level of content after deleting
    // selections that contain a whole number paragraphs plus a line break, since it is unclear to most users
    // that such a selection actually ends at the start of the next paragraph. This matches TextEdit behavior
    // for indented paragraphs.
    // Only apply this rule if the endingSelection is a range selection.  If it is a caret, then other operations have created
    // the selection we're deleting (like the process of creating a selection to delete during a backspace), and the user isn't in the situation described above.
    if (numEnclosingMailBlockquotes(start) != numEnclosingMailBlockquotes(end)
        && isStartOfParagraph(visibleEnd) && isStartOfParagraph(createVisiblePosition(start))
        && endingSelection().isRange()) {
        m_mergeBlocksAfterDelete = false;
        m_pruneStartBlockIfNecessary = true;
    }

    // Handle leading and trailing whitespace, as well as smart delete adjustments to the selection
    m_leadingWhitespace = leadingWhitespacePosition(m_upstreamStart, m_selectionToDelete.affinity());
    m_trailingWhitespace = trailingWhitespacePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY);

    if (m_smartDelete) {

        // skip smart delete if the selection to delete already starts or ends with whitespace
        Position pos = createVisiblePosition(m_upstreamStart, m_selectionToDelete.affinity()).deepEquivalent();
        bool skipSmartDelete = trailingWhitespacePosition(pos, VP_DEFAULT_AFFINITY, ConsiderNonCollapsibleWhitespace).isNotNull();
        if (!skipSmartDelete)
            skipSmartDelete = leadingWhitespacePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY, ConsiderNonCollapsibleWhitespace).isNotNull();

        // extend selection upstream if there is whitespace there
        bool hasLeadingWhitespaceBeforeAdjustment = leadingWhitespacePosition(m_upstreamStart, m_selectionToDelete.affinity(), ConsiderNonCollapsibleWhitespace).isNotNull();
        if (!skipSmartDelete && hasLeadingWhitespaceBeforeAdjustment) {
            VisiblePosition visiblePos = previousPositionOf(createVisiblePosition(m_upstreamStart, VP_DEFAULT_AFFINITY));
            pos = visiblePos.deepEquivalent();
            // Expand out one character upstream for smart delete and recalculate
            // positions based on this change.
            m_upstreamStart = mostBackwardCaretPosition(pos);
            m_downstreamStart = mostForwardCaretPosition(pos);
            m_leadingWhitespace = leadingWhitespacePosition(m_upstreamStart, visiblePos.affinity());

            setStartingSelectionOnSmartDelete(m_upstreamStart, m_upstreamEnd);
        }

        // trailing whitespace is only considered for smart delete if there is no leading
        // whitespace, as in the case where you double-click the first word of a paragraph.
        if (!skipSmartDelete && !hasLeadingWhitespaceBeforeAdjustment && trailingWhitespacePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY, ConsiderNonCollapsibleWhitespace).isNotNull()) {
            // Expand out one character downstream for smart delete and recalculate
            // positions based on this change.
            pos = nextPositionOf(createVisiblePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY)).deepEquivalent();
            m_upstreamEnd = mostBackwardCaretPosition(pos);
            m_downstreamEnd = mostForwardCaretPosition(pos);
            m_trailingWhitespace = trailingWhitespacePosition(m_downstreamEnd, VP_DEFAULT_AFFINITY);

            setStartingSelectionOnSmartDelete(m_downstreamStart, m_downstreamEnd);
        }
    }

    // We must pass call parentAnchoredEquivalent on the positions since some editing positions
    // that appear inside their nodes aren't really inside them.  [hr, 0] is one example.
    // FIXME: parentAnchoredEquivalent should eventually be moved into enclosing element getters
    // like the one below, since editing functions should obviously accept editing positions.
    // FIXME: Passing false to enclosingNodeOfType tells it that it's OK to return a non-editable
    // node.  This was done to match existing behavior, but it seems wrong.
    m_startBlock = enclosingNodeOfType(m_downstreamStart.parentAnchoredEquivalent(), &isEnclosingBlock, CanCrossEditingBoundary);
    m_endBlock = enclosingNodeOfType(m_upstreamEnd.parentAnchoredEquivalent(), &isEnclosingBlock, CanCrossEditingBoundary);
}

// We don't want to inherit style from an element which can't have contents.
static bool shouldNotInheritStyleFrom(const Node& node)
{
    return !node.canContainRangeEndPoint();
}

void DeleteSelectionCommand::saveTypingStyleState()
{
    // A common case is deleting characters that are all from the same text node. In
    // that case, the style at the start of the selection before deletion will be the
    // same as the style at the start of the selection after deletion (since those
    // two positions will be identical). Therefore there is no need to save the
    // typing style at the start of the selection, nor is there a reason to
    // compute the style at the start of the selection after deletion (see the
    // early return in calculateTypingStyleAfterDelete).
    if (m_upstreamStart.anchorNode() == m_downstreamEnd.anchorNode() && m_upstreamStart.anchorNode()->isTextNode())
        return;

    if (shouldNotInheritStyleFrom(*m_selectionToDelete.start().anchorNode()))
        return;

    // Figure out the typing style in effect before the delete is done.
    m_typingStyle = EditingStyle::create(m_selectionToDelete.start(), EditingStyle::EditingPropertiesInEffect);
    m_typingStyle->removeStyleAddedByElement(enclosingAnchorElement(m_selectionToDelete.start()));

    // If we're deleting into a Mail blockquote, save the style at end() instead of start()
    // We'll use this later in computeTypingStyleAfterDelete if we end up outside of a Mail blockquote
    if (enclosingNodeOfType(m_selectionToDelete.start(), isMailHTMLBlockquoteElement))
        m_deleteIntoBlockquoteStyle = EditingStyle::create(m_selectionToDelete.end());
    else
        m_deleteIntoBlockquoteStyle = nullptr;
}

bool DeleteSelectionCommand::handleSpecialCaseBRDelete(EditingState* editingState)
{
    Node* nodeAfterUpstreamStart = m_upstreamStart.computeNodeAfterPosition();
    Node* nodeAfterDownstreamStart = m_downstreamStart.computeNodeAfterPosition();
    // Upstream end will appear before BR due to canonicalization
    Node* nodeAfterUpstreamEnd = m_upstreamEnd.computeNodeAfterPosition();

    if (!nodeAfterUpstreamStart || !nodeAfterDownstreamStart)
        return false;

    // Check for special-case where the selection contains only a BR on a line by itself after another BR.
    bool upstreamStartIsBR = isHTMLBRElement(*nodeAfterUpstreamStart);
    bool downstreamStartIsBR = isHTMLBRElement(*nodeAfterDownstreamStart);
    bool isBROnLineByItself = upstreamStartIsBR && downstreamStartIsBR && nodeAfterDownstreamStart == nodeAfterUpstreamEnd;
    if (isBROnLineByItself) {
        removeNode(nodeAfterDownstreamStart, editingState);
        return true;
    }

    // FIXME: This code doesn't belong in here.
    // We detect the case where the start is an empty line consisting of BR not wrapped in a block element.
    if (upstreamStartIsBR && downstreamStartIsBR && !(isStartOfBlock(VisiblePosition::beforeNode(nodeAfterUpstreamStart)) && isEndOfBlock(VisiblePosition::afterNode(nodeAfterUpstreamStart)))) {
        m_startsAtEmptyLine = true;
        m_endingPosition = m_downstreamEnd;
    }

    return false;
}

static Position firstEditablePositionInNode(Node* node)
{
    DCHECK(node);
    Node* next = node;
    while (next && !next->hasEditableStyle())
        next = NodeTraversal::next(*next, node);
    return next ? firstPositionInOrBeforeNode(next) : Position();
}

void DeleteSelectionCommand::removeNode(Node* node, EditingState* editingState, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    if (!node)
        return;

    if (m_startRoot != m_endRoot && !(node->isDescendantOf(m_startRoot.get()) && node->isDescendantOf(m_endRoot.get()))) {
        // If a node is not in both the start and end editable roots, remove it only if its inside an editable region.
        if (!node->parentNode()->hasEditableStyle()) {
            // Don't remove non-editable atomic nodes.
            if (!node->hasChildren())
                return;
            // Search this non-editable region for editable regions to empty.
            Node* child = node->firstChild();
            while (child) {
                Node* nextChild = child->nextSibling();
                removeNode(child, editingState, shouldAssumeContentIsAlwaysEditable);
                if (editingState->isAborted())
                    return;
                // Bail if nextChild is no longer node's child.
                if (nextChild && nextChild->parentNode() != node)
                    return;
                child = nextChild;
            }

            // Don't remove editable regions that are inside non-editable ones, just clear them.
            return;
        }
    }

    if (isTableStructureNode(node) || node->isRootEditableElement()) {
        // Do not remove an element of table structure; remove its contents.
        // Likewise for the root editable element.
        Node* child = node->firstChild();
        while (child) {
            Node* remove = child;
            child = child->nextSibling();
            removeNode(remove, editingState, shouldAssumeContentIsAlwaysEditable);
            if (editingState->isAborted())
                return;
        }

        // Make sure empty cell has some height, if a placeholder can be inserted.
        document().updateStyleAndLayoutIgnorePendingStylesheets();
        LayoutObject* r = node->layoutObject();
        if (r && r->isTableCell() && toLayoutTableCell(r)->contentHeight() <= 0) {
            Position firstEditablePosition = firstEditablePositionInNode(node);
            if (firstEditablePosition.isNotNull())
                insertBlockPlaceholder(firstEditablePosition, editingState);
        }
        return;
    }

    if (node == m_startBlock) {
        VisiblePosition previous = previousPositionOf(VisiblePosition::firstPositionInNode(m_startBlock.get()));
        if (previous.isNotNull() && !isEndOfBlock(previous))
            m_needPlaceholder = true;
    }
    if (node == m_endBlock) {
        VisiblePosition next = nextPositionOf(VisiblePosition::lastPositionInNode(m_endBlock.get()));
        if (next.isNotNull() && !isStartOfBlock(next))
            m_needPlaceholder = true;
    }

    // FIXME: Update the endpoints of the range being deleted.
    updatePositionForNodeRemoval(m_endingPosition, *node);
    updatePositionForNodeRemoval(m_leadingWhitespace, *node);
    updatePositionForNodeRemoval(m_trailingWhitespace, *node);

    CompositeEditCommand::removeNode(node, editingState, shouldAssumeContentIsAlwaysEditable);
}

static void updatePositionForTextRemoval(Text* node, int offset, int count, Position& position)
{
    if (!position.isOffsetInAnchor() || position.computeContainerNode() != node)
        return;

    if (position.offsetInContainerNode() > offset + count)
        position = Position(position.computeContainerNode(), position.offsetInContainerNode() - count);
    else if (position.offsetInContainerNode() > offset)
        position = Position(position.computeContainerNode(), offset);
}

void DeleteSelectionCommand::deleteTextFromNode(Text* node, unsigned offset, unsigned count)
{
    // FIXME: Update the endpoints of the range being deleted.
    updatePositionForTextRemoval(node, offset, count, m_endingPosition);
    updatePositionForTextRemoval(node, offset, count, m_leadingWhitespace);
    updatePositionForTextRemoval(node, offset, count, m_trailingWhitespace);
    updatePositionForTextRemoval(node, offset, count, m_downstreamEnd);

    CompositeEditCommand::deleteTextFromNode(node, offset, count);
}

void DeleteSelectionCommand::makeStylingElementsDirectChildrenOfEditableRootToPreventStyleLoss(EditingState* editingState)
{
    Range* range = createRange(m_selectionToDelete.toNormalizedEphemeralRange());
    Node* node = range->firstNode();
    while (node && node != range->pastLastNode()) {
        Node* nextNode = NodeTraversal::next(*node);
        if (isHTMLStyleElement(*node) || isHTMLLinkElement(*node)) {
            nextNode = NodeTraversal::nextSkippingChildren(*node);
            Element* rootEditableElement = node->rootEditableElement();
            if (rootEditableElement) {
                removeNode(node, editingState);
                if (editingState->isAborted())
                    return;
                appendNode(node, rootEditableElement, editingState);
                if (editingState->isAborted())
                    return;
            }
        }
        node = nextNode;
    }
}

void DeleteSelectionCommand::handleGeneralDelete(EditingState* editingState)
{
    if (m_upstreamStart.isNull())
        return;

    int startOffset = m_upstreamStart.computeEditingOffset();
    Node* startNode = m_upstreamStart.anchorNode();
    DCHECK(startNode);

    makeStylingElementsDirectChildrenOfEditableRootToPreventStyleLoss(editingState);
    if (editingState->isAborted())
        return;

    // Never remove the start block unless it's a table, in which case we won't merge content in.
    if (startNode == m_startBlock.get() && !startOffset && canHaveChildrenForEditing(startNode) && !isHTMLTableElement(*startNode)) {
        startOffset = 0;
        startNode = NodeTraversal::next(*startNode);
        if (!startNode)
            return;
    }

    if (startOffset >= caretMaxOffset(startNode) && startNode->isTextNode()) {
        Text* text = toText(startNode);
        if (text->length() > (unsigned)caretMaxOffset(startNode))
            deleteTextFromNode(text, caretMaxOffset(startNode), text->length() - caretMaxOffset(startNode));
    }

    if (startOffset >= EditingStrategy::lastOffsetForEditing(startNode)) {
        startNode = NodeTraversal::nextSkippingChildren(*startNode);
        startOffset = 0;
    }

    // Done adjusting the start.  See if we're all done.
    if (!startNode)
        return;

    if (startNode == m_downstreamEnd.anchorNode()) {
        if (m_downstreamEnd.computeEditingOffset() - startOffset > 0) {
            if (startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                Text* text = toText(startNode);
                deleteTextFromNode(text, startOffset, m_downstreamEnd.computeOffsetInContainerNode() - startOffset);
            } else {
                removeChildrenInRange(startNode, startOffset, m_downstreamEnd.computeEditingOffset(), editingState);
                if (editingState->isAborted())
                    return;
                m_endingPosition = m_upstreamStart;
            }
        }

        // The selection to delete is all in one node.
        if (!startNode->layoutObject() || (!startOffset && m_downstreamEnd.atLastEditingPositionForNode())) {
            removeNode(startNode, editingState);
            if (editingState->isAborted())
                return;
        }
    } else {
        bool startNodeWasDescendantOfEndNode = m_upstreamStart.anchorNode()->isDescendantOf(m_downstreamEnd.anchorNode());
        // The selection to delete spans more than one node.
        Node* node(startNode);

        if (startOffset > 0) {
            if (startNode->isTextNode()) {
                // in a text node that needs to be trimmed
                Text* text = toText(node);
                deleteTextFromNode(text, startOffset, text->length() - startOffset);
                node = NodeTraversal::next(*node);
            } else {
                node = NodeTraversal::childAt(*startNode, startOffset);
            }
        } else if (startNode == m_upstreamEnd.anchorNode() && startNode->isTextNode()) {
            Text* text = toText(m_upstreamEnd.anchorNode());
            deleteTextFromNode(text, 0, m_upstreamEnd.computeOffsetInContainerNode());
        }

        // handle deleting all nodes that are completely selected
        while (node && node != m_downstreamEnd.anchorNode()) {
            if (comparePositions(firstPositionInOrBeforeNode(node), m_downstreamEnd) >= 0) {
                // NodeTraversal::nextSkippingChildren just blew past the end position, so stop deleting
                node = nullptr;
            } else if (!m_downstreamEnd.anchorNode()->isDescendantOf(node)) {
                Node* nextNode = NodeTraversal::nextSkippingChildren(*node);
                // if we just removed a node from the end container, update end position so the
                // check above will work
                updatePositionForNodeRemoval(m_downstreamEnd, *node);
                removeNode(node, editingState);
                if (editingState->isAborted())
                    return;
                node = nextNode;
            } else {
                Node& n = NodeTraversal::lastWithinOrSelf(*node);
                if (m_downstreamEnd.anchorNode() == n && m_downstreamEnd.computeEditingOffset() >= caretMaxOffset(&n)) {
                    removeNode(node, editingState);
                    if (editingState->isAborted())
                        return;
                    node = nullptr;
                } else {
                    node = NodeTraversal::next(*node);
                }
            }
        }

        if (m_downstreamEnd.anchorNode() != startNode && !m_upstreamStart.anchorNode()->isDescendantOf(m_downstreamEnd.anchorNode()) && m_downstreamEnd.inShadowIncludingDocument() && m_downstreamEnd.computeEditingOffset() >= caretMinOffset(m_downstreamEnd.anchorNode())) {
            if (m_downstreamEnd.atLastEditingPositionForNode() && !canHaveChildrenForEditing(m_downstreamEnd.anchorNode())) {
                // The node itself is fully selected, not just its contents.  Delete it.
                removeNode(m_downstreamEnd.anchorNode(), editingState);
            } else {
                if (m_downstreamEnd.anchorNode()->isTextNode()) {
                    // in a text node that needs to be trimmed
                    Text* text = toText(m_downstreamEnd.anchorNode());
                    if (m_downstreamEnd.computeEditingOffset() > 0) {
                        deleteTextFromNode(text, 0, m_downstreamEnd.computeEditingOffset());
                    }
                // Remove children of m_downstreamEnd.anchorNode() that come after m_upstreamStart.
                // Don't try to remove children if m_upstreamStart was inside m_downstreamEnd.anchorNode()
                // and m_upstreamStart has been removed from the document, because then we don't
                // know how many children to remove.
                // FIXME: Make m_upstreamStart a position we update as we remove content, then we can
                // always know which children to remove.
                } else if (!(startNodeWasDescendantOfEndNode && !m_upstreamStart.inShadowIncludingDocument())) {
                    int offset = 0;
                    if (m_upstreamStart.anchorNode()->isDescendantOf(m_downstreamEnd.anchorNode())) {
                        Node* n = m_upstreamStart.anchorNode();
                        while (n && n->parentNode() != m_downstreamEnd.anchorNode())
                            n = n->parentNode();
                        if (n)
                            offset = n->nodeIndex() + 1;
                    }
                    removeChildrenInRange(m_downstreamEnd.anchorNode(), offset, m_downstreamEnd.computeEditingOffset(), editingState);
                    if (editingState->isAborted())
                        return;
                    m_downstreamEnd = Position::editingPositionOf(m_downstreamEnd.anchorNode(), offset);
                }
            }
        }
    }
}

void DeleteSelectionCommand::fixupWhitespace()
{
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    // TODO(yosin) |isRenderedCharacter()| should be removed, and we should use
    // |VisiblePosition::characterAfter()| and
    // |VisiblePosition::characterBefore()|
    if (m_leadingWhitespace.isNotNull() && !isRenderedCharacter(m_leadingWhitespace) && m_leadingWhitespace.anchorNode()->isTextNode()) {
        Text* textNode = toText(m_leadingWhitespace.anchorNode());
        DCHECK(!textNode->layoutObject() || textNode->layoutObject()->style()->collapseWhiteSpace()) << textNode;
        replaceTextInNodePreservingMarkers(textNode, m_leadingWhitespace.computeOffsetInContainerNode(), 1, nonBreakingSpaceString());
    }
    if (m_trailingWhitespace.isNotNull() && !isRenderedCharacter(m_trailingWhitespace) && m_trailingWhitespace.anchorNode()->isTextNode()) {
        Text* textNode = toText(m_trailingWhitespace.anchorNode());
        DCHECK(!textNode->layoutObject() || textNode->layoutObject()->style()->collapseWhiteSpace()) << textNode;
        replaceTextInNodePreservingMarkers(textNode, m_trailingWhitespace.computeOffsetInContainerNode(), 1, nonBreakingSpaceString());
    }
}

// If a selection starts in one block and ends in another, we have to merge to bring content before the
// start together with content after the end.
void DeleteSelectionCommand::mergeParagraphs(EditingState* editingState)
{
    if (!m_mergeBlocksAfterDelete) {
        if (m_pruneStartBlockIfNecessary) {
            // We aren't going to merge into the start block, so remove it if it's empty.
            prune(m_startBlock, editingState);
            if (editingState->isAborted())
                return;
            // Removing the start block during a deletion is usually an indication that we need
            // a placeholder, but not in this case.
            m_needPlaceholder = false;
        }
        return;
    }

    // It shouldn't have been asked to both try and merge content into the start block and prune it.
    DCHECK(!m_pruneStartBlockIfNecessary);

    // FIXME: Deletion should adjust selection endpoints as it removes nodes so that we never get into this state (4099839).
    if (!m_downstreamEnd.inShadowIncludingDocument() || !m_upstreamStart.inShadowIncludingDocument())
        return;

    // FIXME: The deletion algorithm shouldn't let this happen.
    if (comparePositions(m_upstreamStart, m_downstreamEnd) > 0)
        return;

    // There's nothing to merge.
    if (m_upstreamStart == m_downstreamEnd)
        return;

    VisiblePosition startOfParagraphToMove = createVisiblePosition(m_downstreamEnd);
    VisiblePosition mergeDestination = createVisiblePosition(m_upstreamStart);

    // m_downstreamEnd's block has been emptied out by deletion.  There is no content inside of it to
    // move, so just remove it.
    Element* endBlock = enclosingBlock(m_downstreamEnd.anchorNode());
    if (!endBlock || !endBlock->contains(startOfParagraphToMove.deepEquivalent().anchorNode()) || !startOfParagraphToMove.deepEquivalent().anchorNode()) {
        removeNode(enclosingBlock(m_downstreamEnd.anchorNode()), editingState);
        return;
    }

    // We need to merge into m_upstreamStart's block, but it's been emptied out and collapsed by deletion.
    if (!mergeDestination.deepEquivalent().anchorNode() || (!mergeDestination.deepEquivalent().anchorNode()->isDescendantOf(enclosingBlock(m_upstreamStart.computeContainerNode())) && (!mergeDestination.deepEquivalent().anchorNode()->hasChildren() || !m_upstreamStart.computeContainerNode()->hasChildren())) || (m_startsAtEmptyLine && mergeDestination.deepEquivalent() != startOfParagraphToMove.deepEquivalent())) {
        insertNodeAt(HTMLBRElement::create(document()), m_upstreamStart, editingState);
        if (editingState->isAborted())
            return;
        mergeDestination = createVisiblePosition(m_upstreamStart);
    }

    if (mergeDestination.deepEquivalent() == startOfParagraphToMove.deepEquivalent())
        return;

    VisiblePosition endOfParagraphToMove = endOfParagraph(startOfParagraphToMove, CanSkipOverEditingBoundary);

    if (mergeDestination.deepEquivalent() == endOfParagraphToMove.deepEquivalent())
        return;

    // If the merge destination and source to be moved are both list items of different lists, merge them into single list.
    Node* listItemInFirstParagraph = enclosingNodeOfType(m_upstreamStart, isListItem);
    Node* listItemInSecondParagraph = enclosingNodeOfType(m_downstreamEnd, isListItem);
    if (listItemInFirstParagraph && listItemInSecondParagraph
        && listItemInFirstParagraph->parentElement() != listItemInSecondParagraph->parentElement()
        && canMergeLists(listItemInFirstParagraph->parentElement(), listItemInSecondParagraph->parentElement())) {
        mergeIdenticalElements(listItemInFirstParagraph->parentElement(), listItemInSecondParagraph->parentElement(), editingState);
        if (editingState->isAborted())
            return;
        m_endingPosition = mergeDestination.deepEquivalent();
        return;
    }

    // The rule for merging into an empty block is: only do so if its farther to the right.
    // FIXME: Consider RTL.
    if (!m_startsAtEmptyLine && isStartOfParagraph(mergeDestination) && absoluteCaretBoundsOf(startOfParagraphToMove).x() > absoluteCaretBoundsOf(mergeDestination).x()) {
        if (isHTMLBRElement(*mostForwardCaretPosition(mergeDestination.deepEquivalent()).anchorNode())) {
            removeNodeAndPruneAncestors(mostForwardCaretPosition(mergeDestination.deepEquivalent()).anchorNode(), editingState);
            if (editingState->isAborted())
                return;
            m_endingPosition = startOfParagraphToMove.deepEquivalent();
            return;
        }
    }

    // Block images, tables and horizontal rules cannot be made inline with content at mergeDestination.  If there is
    // any (!isStartOfParagraph(mergeDestination)), don't merge, just move the caret to just before the selection we deleted.
    // See https://bugs.webkit.org/show_bug.cgi?id=25439
    if (isRenderedAsNonInlineTableImageOrHR(startOfParagraphToMove.deepEquivalent().anchorNode()) && !isStartOfParagraph(mergeDestination)) {
        m_endingPosition = m_upstreamStart;
        return;
    }

    // moveParagraphs will insert placeholders if it removes blocks that would require their use, don't let block
    // removals that it does cause the insertion of *another* placeholder.
    bool needPlaceholder = m_needPlaceholder;
    bool paragraphToMergeIsEmpty = startOfParagraphToMove.deepEquivalent() == endOfParagraphToMove.deepEquivalent();
    moveParagraph(startOfParagraphToMove, endOfParagraphToMove, mergeDestination, editingState, DoNotPreserveSelection, paragraphToMergeIsEmpty ? DoNotPreserveStyle : PreserveStyle);
    if (editingState->isAborted())
        return;
    m_needPlaceholder = needPlaceholder;
    // The endingPosition was likely clobbered by the move, so recompute it (moveParagraph selects the moved paragraph).
    m_endingPosition = endingSelection().start();
}

void DeleteSelectionCommand::removePreviouslySelectedEmptyTableRows(EditingState* editingState)
{
    if (m_endTableRow && m_endTableRow->inShadowIncludingDocument() && m_endTableRow != m_startTableRow) {
        Node* row = m_endTableRow->previousSibling();
        while (row && row != m_startTableRow) {
            Node* previousRow = row->previousSibling();
            if (isTableRowEmpty(row)) {
                // Use a raw removeNode, instead of DeleteSelectionCommand's,
                // because that won't remove rows, it only empties them in
                // preparation for this function.
                CompositeEditCommand::removeNode(row, editingState);
                if (editingState->isAborted())
                    return;
            }
            row = previousRow;
        }
    }

    // Remove empty rows after the start row.
    if (m_startTableRow && m_startTableRow->inShadowIncludingDocument() && m_startTableRow != m_endTableRow) {
        Node* row = m_startTableRow->nextSibling();
        while (row && row != m_endTableRow) {
            Node* nextRow = row->nextSibling();
            if (isTableRowEmpty(row)) {
                CompositeEditCommand::removeNode(row, editingState);
                if (editingState->isAborted())
                    return;
            }
            row = nextRow;
        }
    }

    if (m_endTableRow && m_endTableRow->inShadowIncludingDocument() && m_endTableRow != m_startTableRow) {
        if (isTableRowEmpty(m_endTableRow.get())) {
            // Don't remove m_endTableRow if it's where we're putting the ending
            // selection.
            if (!m_endingPosition.anchorNode()->isDescendantOf(m_endTableRow.get())) {
                // FIXME: We probably shouldn't remove m_endTableRow unless it's
                // fully selected, even if it is empty. We'll need to start
                // adjusting the selection endpoints during deletion to know
                // whether or not m_endTableRow was fully selected here.
                CompositeEditCommand::removeNode(m_endTableRow.get(), editingState);
                if (editingState->isAborted())
                    return;
            }
        }
    }
}

void DeleteSelectionCommand::calculateTypingStyleAfterDelete()
{
    // Clearing any previously set typing style and doing an early return.
    if (!m_typingStyle) {
        document().frame()->selection().clearTypingStyle();
        return;
    }

    // Compute the difference between the style before the delete and the style now
    // after the delete has been done. Set this style on the frame, so other editing
    // commands being composed with this one will work, and also cache it on the command,
    // so the LocalFrame::appliedEditing can set it after the whole composite command
    // has completed.

    // If we deleted into a blockquote, but are now no longer in a blockquote, use the alternate typing style
    if (m_deleteIntoBlockquoteStyle && !enclosingNodeOfType(m_endingPosition, isMailHTMLBlockquoteElement, CanCrossEditingBoundary))
        m_typingStyle = m_deleteIntoBlockquoteStyle;
    m_deleteIntoBlockquoteStyle = nullptr;

    m_typingStyle->prepareToApplyAt(m_endingPosition);
    if (m_typingStyle->isEmpty())
        m_typingStyle = nullptr;
    // This is where we've deleted all traces of a style but not a whole paragraph (that's handled above).
    // In this case if we start typing, the new characters should have the same style as the just deleted ones,
    // but, if we change the selection, come back and start typing that style should be lost.  Also see
    // preserveTypingStyle() below.
    document().frame()->selection().setTypingStyle(m_typingStyle);
}

void DeleteSelectionCommand::clearTransientState()
{
    m_selectionToDelete = VisibleSelection();
    m_upstreamStart = Position();
    m_downstreamStart = Position();
    m_upstreamEnd = Position();
    m_downstreamEnd = Position();
    m_endingPosition = Position();
    m_leadingWhitespace = Position();
    m_trailingWhitespace = Position();
}

// This method removes div elements with no attributes that have only one child or no children at all.
void DeleteSelectionCommand::removeRedundantBlocks(EditingState* editingState)
{
    Node* node = m_endingPosition.computeContainerNode();
    Element* rootElement = node->rootEditableElement();

    while (node != rootElement) {
        if (isRemovableBlock(node)) {
            if (node == m_endingPosition.anchorNode())
                updatePositionForNodeRemovalPreservingChildren(m_endingPosition, *node);

            CompositeEditCommand::removeNodePreservingChildren(node, editingState);
            if (editingState->isAborted())
                return;
            node = m_endingPosition.anchorNode();
        } else {
            node = node->parentNode();
        }
    }
}

void DeleteSelectionCommand::doApply(EditingState* editingState)
{
    // If selection has not been set to a custom selection when the command was created,
    // use the current ending selection.
    if (!m_hasSelectionToDelete)
        m_selectionToDelete = endingSelection();

    if (!m_selectionToDelete.isNonOrphanedRange() || !m_selectionToDelete.isContentEditable())
        return;

    // save this to later make the selection with
    TextAffinity affinity = m_selectionToDelete.affinity();

    Position downstreamEnd = mostForwardCaretPosition(m_selectionToDelete.end());
    bool rootWillStayOpenWithoutPlaceholder = downstreamEnd.computeContainerNode() == downstreamEnd.computeContainerNode()->rootEditableElement()
        || (downstreamEnd.computeContainerNode()->isTextNode() && downstreamEnd.computeContainerNode()->parentNode() == downstreamEnd.computeContainerNode()->rootEditableElement());
    bool lineBreakAtEndOfSelectionToDelete = lineBreakExistsAtVisiblePosition(m_selectionToDelete.visibleEnd());
    m_needPlaceholder = !rootWillStayOpenWithoutPlaceholder
        && isStartOfParagraph(m_selectionToDelete.visibleStart(), CanCrossEditingBoundary)
        && isEndOfParagraph(m_selectionToDelete.visibleEnd(), CanCrossEditingBoundary)
        && !lineBreakAtEndOfSelectionToDelete;
    if (m_needPlaceholder) {
        // Don't need a placeholder when deleting a selection that starts just
        // before a table and ends inside it (we do need placeholders to hold
        // open empty cells, but that's handled elsewhere).
        if (Element* table = tableElementJustAfter(m_selectionToDelete.visibleStart())) {
            if (m_selectionToDelete.end().anchorNode()->isDescendantOf(table))
                m_needPlaceholder = false;
        }
    }

    // set up our state
    initializePositionData(editingState);
    if (editingState->isAborted())
        return;

    bool lineBreakBeforeStart = lineBreakExistsAtVisiblePosition(previousPositionOf(createVisiblePosition(m_upstreamStart)));

    // Delete any text that may hinder our ability to fixup whitespace after the
    // delete
    deleteInsignificantTextDownstream(m_trailingWhitespace);

    saveTypingStyleState();

    // deleting just a BR is handled specially, at least because we do not
    // want to replace it with a placeholder BR!
    bool brResult = handleSpecialCaseBRDelete(editingState);
    if (editingState->isAborted())
        return;
    if (brResult) {
        calculateTypingStyleAfterDelete();
        setEndingSelection(VisibleSelection(m_endingPosition, affinity, endingSelection().isDirectional()));
        clearTransientState();
        rebalanceWhitespace();
        return;
    }

    handleGeneralDelete(editingState);
    if (editingState->isAborted())
        return;

    fixupWhitespace();

    mergeParagraphs(editingState);
    if (editingState->isAborted())
        return;

    removePreviouslySelectedEmptyTableRows(editingState);
    if (editingState->isAborted())
        return;

    if (!m_needPlaceholder && rootWillStayOpenWithoutPlaceholder) {
        VisiblePosition visualEnding = createVisiblePosition(m_endingPosition);
        bool hasPlaceholder = lineBreakExistsAtVisiblePosition(visualEnding)
            && nextPositionOf(visualEnding, CannotCrossEditingBoundary).isNull();
        m_needPlaceholder = hasPlaceholder && lineBreakBeforeStart && !lineBreakAtEndOfSelectionToDelete;
    }

    HTMLBRElement* placeholder = m_needPlaceholder ? HTMLBRElement::create(document()) : nullptr;

    if (placeholder) {
        if (m_sanitizeMarkup) {
            removeRedundantBlocks(editingState);
            if (editingState->isAborted())
                return;
        }
        // handleGeneralDelete cause DOM mutation events so |m_endingPosition|
        // can be out of document.
        if (m_endingPosition.inShadowIncludingDocument()) {
            insertNodeAt(placeholder, m_endingPosition, editingState);
            if (editingState->isAborted())
                return;
        }
    }

    rebalanceWhitespaceAt(m_endingPosition);

    calculateTypingStyleAfterDelete();

    setEndingSelection(VisibleSelection(m_endingPosition, affinity, endingSelection().isDirectional()));
    clearTransientState();
}

EditAction DeleteSelectionCommand::editingAction() const
{
    // Note that DeleteSelectionCommand is also used when the user presses the Delete key,
    // but in that case there's a TypingCommand that supplies the editingAction(), so
    // the Undo menu correctly shows "Undo Typing"
    return EditActionCut;
}

// Normally deletion doesn't preserve the typing style that was present before it.  For example,
// type a character, Bold, then delete the character and start typing.  The Bold typing style shouldn't
// stick around.  Deletion should preserve a typing style that *it* sets, however.
bool DeleteSelectionCommand::preservesTypingStyle() const
{
    return m_typingStyle;
}

DEFINE_TRACE(DeleteSelectionCommand)
{
    visitor->trace(m_selectionToDelete);
    visitor->trace(m_upstreamStart);
    visitor->trace(m_downstreamStart);
    visitor->trace(m_upstreamEnd);
    visitor->trace(m_downstreamEnd);
    visitor->trace(m_endingPosition);
    visitor->trace(m_leadingWhitespace);
    visitor->trace(m_trailingWhitespace);
    visitor->trace(m_startBlock);
    visitor->trace(m_endBlock);
    visitor->trace(m_typingStyle);
    visitor->trace(m_deleteIntoBlockquoteStyle);
    visitor->trace(m_startRoot);
    visitor->trace(m_endRoot);
    visitor->trace(m_startTableRow);
    visitor->trace(m_endTableRow);
    visitor->trace(m_temporaryPlaceholder);
    CompositeEditCommand::trace(visitor);
}

} // namespace blink
