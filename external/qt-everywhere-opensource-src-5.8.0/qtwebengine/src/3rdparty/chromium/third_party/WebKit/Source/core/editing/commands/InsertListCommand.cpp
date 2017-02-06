/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
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

#include "core/editing/commands/InsertListCommand.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLLIElement.h"
#include "core/html/HTMLUListElement.h"

namespace blink {

using namespace HTMLNames;

static Node* enclosingListChild(Node* node, Node* listNode)
{
    Node* listChild = enclosingListChild(node);
    while (listChild && enclosingList(listChild) != listNode)
        listChild = enclosingListChild(listChild->parentNode());
    return listChild;
}

HTMLUListElement* InsertListCommand::fixOrphanedListChild(Node* node, EditingState* editingState)
{
    HTMLUListElement* listElement = HTMLUListElement::create(document());
    insertNodeBefore(listElement, node, editingState);
    if (editingState->isAborted())
        return nullptr;
    removeNode(node, editingState);
    if (editingState->isAborted())
        return nullptr;
    appendNode(node, listElement, editingState);
    if (editingState->isAborted())
        return nullptr;
    return listElement;
}

HTMLElement* InsertListCommand::mergeWithNeighboringLists(HTMLElement* passedList, EditingState* editingState)
{
    HTMLElement* list = passedList;
    Element* previousList = ElementTraversal::previousSibling(*list);
    if (canMergeLists(previousList, list)) {
        mergeIdenticalElements(previousList, list, editingState);
        if (editingState->isAborted())
            return nullptr;
    }

    if (!list)
        return nullptr;

    Element* nextSibling = ElementTraversal::nextSibling(*list);
    if (!nextSibling || !nextSibling->isHTMLElement())
        return list;

    HTMLElement* nextList = toHTMLElement(nextSibling);
    if (canMergeLists(list, nextList)) {
        mergeIdenticalElements(list, nextList, editingState);
        if (editingState->isAborted())
            return nullptr;
        return nextList;
    }
    return list;
}

bool InsertListCommand::selectionHasListOfType(const VisibleSelection& selection, const HTMLQualifiedName& listTag)
{
    VisiblePosition start = selection.visibleStart();

    if (!enclosingList(start.deepEquivalent().anchorNode()))
        return false;

    VisiblePosition end = startOfParagraph(selection.visibleEnd());
    while (start.isNotNull() && start.deepEquivalent() != end.deepEquivalent()) {
        HTMLElement* listElement = enclosingList(start.deepEquivalent().anchorNode());
        if (!listElement || !listElement->hasTagName(listTag))
            return false;
        start = startOfNextParagraph(start);
    }

    return true;
}

InsertListCommand::InsertListCommand(Document& document, Type type)
    : CompositeEditCommand(document), m_type(type)
{
}

static bool inSameTreeAndOrdered(const VisiblePosition& shouldBeFormer, const VisiblePosition& shouldBeLater)
{
    const Position formerPosition = shouldBeFormer.deepEquivalent();
    const Position laterPosition = shouldBeLater.deepEquivalent();
    return Position::commonAncestorTreeScope(formerPosition, laterPosition) && comparePositions(formerPosition, laterPosition) <= 0;
}

void InsertListCommand::doApply(EditingState* editingState)
{
    if (!endingSelection().isNonOrphanedCaretOrRange())
        return;

    if (!endingSelection().rootEditableElement())
        return;

    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition visibleStart = endingSelection().visibleStart();
    // When a selection ends at the start of a paragraph, we rarely paint
    // the selection gap before that paragraph, because there often is no gap.
    // In a case like this, it's not obvious to the user that the selection
    // ends "inside" that paragraph, so it would be confusing if InsertUn{Ordered}List
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left
    // margin/padding, but not others.  We should make the gap painting more consistent and
    // then use a left margin/padding rule here.
    if (visibleEnd.deepEquivalent() != visibleStart.deepEquivalent() && isStartOfParagraph(visibleEnd, CanSkipOverEditingBoundary)) {
        setEndingSelection(VisibleSelection(visibleStart, previousPositionOf(visibleEnd, CannotCrossEditingBoundary), endingSelection().isDirectional()));
        if (!endingSelection().rootEditableElement())
            return;
    }

    const HTMLQualifiedName& listTag = (m_type == OrderedList) ? olTag : ulTag;
    if (endingSelection().isRange()) {
        bool forceListCreation = false;
        VisibleSelection selection = selectionForParagraphIteration(endingSelection());
        DCHECK(selection.isRange());
        VisiblePosition startOfSelection = selection.visibleStart();
        VisiblePosition endOfSelection = selection.visibleEnd();
        VisiblePosition startOfLastParagraph = startOfParagraph(endOfSelection, CanSkipOverEditingBoundary);

        Range* currentSelection = firstRangeOf(endingSelection());
        ContainerNode* scopeForStartOfSelection = nullptr;
        ContainerNode* scopeForEndOfSelection = nullptr;
        // FIXME: This is an inefficient way to keep selection alive because
        // indexForVisiblePosition walks from the beginning of the document to the
        // endOfSelection everytime this code is executed. But not using index is hard
        // because there are so many ways we can los eselection inside doApplyForSingleParagraph.
        int indexForStartOfSelection = indexForVisiblePosition(startOfSelection, scopeForStartOfSelection);
        int indexForEndOfSelection = indexForVisiblePosition(endOfSelection, scopeForEndOfSelection);

        if (startOfParagraph(startOfSelection, CanSkipOverEditingBoundary).deepEquivalent() != startOfLastParagraph.deepEquivalent()) {
            forceListCreation = !selectionHasListOfType(selection, listTag);

            VisiblePosition startOfCurrentParagraph = startOfSelection;
            while (inSameTreeAndOrdered(startOfCurrentParagraph, startOfLastParagraph) && !inSameParagraph(startOfCurrentParagraph, startOfLastParagraph, CanCrossEditingBoundary)) {
                // doApply() may operate on and remove the last paragraph of the selection from the document
                // if it's in the same list item as startOfCurrentParagraph.  Return early to avoid an
                // infinite loop and because there is no more work to be done.
                // FIXME(<rdar://problem/5983974>): The endingSelection() may be incorrect here.  Compute
                // the new location of endOfSelection and use it as the end of the new selection.
                if (!startOfLastParagraph.deepEquivalent().inShadowIncludingDocument())
                    return;
                setEndingSelection(startOfCurrentParagraph);

                // Save and restore endOfSelection and startOfLastParagraph when necessary
                // since moveParagraph and movePragraphWithClones can remove nodes.
                bool singleParagraphResult = doApplyForSingleParagraph(forceListCreation, listTag, *currentSelection, editingState);
                if (editingState->isAborted())
                    return;
                if (!singleParagraphResult)
                    break;
                if (endOfSelection.isNull() || endOfSelection.isOrphan() || startOfLastParagraph.isNull() || startOfLastParagraph.isOrphan()) {
                    endOfSelection = visiblePositionForIndex(indexForEndOfSelection, scopeForEndOfSelection);
                    // If endOfSelection is null, then some contents have been deleted from the document.
                    // This should never happen and if it did, exit early immediately because we've lost the loop invariant.
                    DCHECK(endOfSelection.isNotNull());
                    if (endOfSelection.isNull() || !rootEditableElementOf(endOfSelection))
                        return;
                    startOfLastParagraph = startOfParagraph(endOfSelection, CanSkipOverEditingBoundary);
                }

                startOfCurrentParagraph = startOfNextParagraph(endingSelection().visibleStart());
            }
            setEndingSelection(endOfSelection);
        }
        doApplyForSingleParagraph(forceListCreation, listTag, *currentSelection, editingState);
        if (editingState->isAborted())
            return;
        // Fetch the end of the selection, for the reason mentioned above.
        if (endOfSelection.isNull() || endOfSelection.isOrphan()) {
            endOfSelection = visiblePositionForIndex(indexForEndOfSelection, scopeForEndOfSelection);
            if (endOfSelection.isNull())
                return;
        }
        if (startOfSelection.isNull() || startOfSelection.isOrphan()) {
            startOfSelection = visiblePositionForIndex(indexForStartOfSelection, scopeForStartOfSelection);
            if (startOfSelection.isNull())
                return;
        }
        setEndingSelection(VisibleSelection(startOfSelection, endOfSelection, endingSelection().isDirectional()));
        return;
    }

    DCHECK(firstRangeOf(endingSelection()));
    doApplyForSingleParagraph(false, listTag, *firstRangeOf(endingSelection()), editingState);
}

bool InsertListCommand::doApplyForSingleParagraph(bool forceCreateList, const HTMLQualifiedName& listTag, Range& currentSelection, EditingState* editingState)
{
    // FIXME: This will produce unexpected results for a selection that starts just before a
    // table and ends inside the first cell, selectionForParagraphIteration should probably
    // be renamed and deployed inside setEndingSelection().
    Node* selectionNode = endingSelection().start().anchorNode();
    Node* listChildNode = enclosingListChild(selectionNode);
    bool switchListType = false;
    if (listChildNode) {
        if (!listChildNode->parentNode()->hasEditableStyle())
            return false;
        // Remove the list child.
        HTMLElement* listElement = enclosingList(listChildNode);
        if (listElement) {
            if (!listElement->hasEditableStyle()) {
                // Since, |listElement| is uneditable, we can't move |listChild|
                // out from |listElement|.
                return false;
            }
            if (!listElement->parentNode()->hasEditableStyle()) {
                // Since parent of |listElement| is uneditable, we can not remove
                // |listElement| for switching list type neither unlistify.
                return false;
            }
        }
        if (!listElement) {
            listElement = fixOrphanedListChild(listChildNode, editingState);
            if (editingState->isAborted())
                return false;
            listElement = mergeWithNeighboringLists(listElement, editingState);
            if (editingState->isAborted())
                return false;
        }
        DCHECK(listElement->hasEditableStyle());
        DCHECK(listElement->parentNode()->hasEditableStyle());
        if (!listElement->hasTagName(listTag)) {
            // |listChildNode| will be removed from the list and a list of type
            // |m_type| will be created.
            switchListType = true;
        }

        // If the list is of the desired type, and we are not removing the list,
        // then exit early.
        if (!switchListType && forceCreateList)
            return true;

        // If the entire list is selected, then convert the whole list.
        if (switchListType && isNodeVisiblyContainedWithin(*listElement, currentSelection)) {
            bool rangeStartIsInList = visiblePositionBeforeNode(*listElement).deepEquivalent() == createVisiblePosition(currentSelection.startPosition()).deepEquivalent();
            bool rangeEndIsInList = visiblePositionAfterNode(*listElement).deepEquivalent() == createVisiblePosition(currentSelection.endPosition()).deepEquivalent();

            HTMLElement* newList = createHTMLElement(document(), listTag);
            insertNodeBefore(newList, listElement, editingState);
            if (editingState->isAborted())
                return false;

            Node* firstChildInList = enclosingListChild(VisiblePosition::firstPositionInNode(listElement).deepEquivalent().anchorNode(), listElement);
            Element* outerBlock = firstChildInList && isBlockFlowElement(*firstChildInList) ? toElement(firstChildInList) : listElement;

            moveParagraphWithClones(VisiblePosition::firstPositionInNode(listElement), VisiblePosition::lastPositionInNode(listElement), newList, outerBlock, editingState);
            if (editingState->isAborted())
                return false;

            // Manually remove listNode because moveParagraphWithClones sometimes leaves it behind in the document.
            // See the bug 33668 and editing/execCommand/insert-list-orphaned-item-with-nested-lists.html.
            // FIXME: This might be a bug in moveParagraphWithClones or deleteSelection.
            if (listElement && listElement->inShadowIncludingDocument()) {
                removeNode(listElement, editingState);
                if (editingState->isAborted())
                    return false;
            }

            newList = mergeWithNeighboringLists(newList, editingState);
            if (editingState->isAborted())
                return false;

            // Restore the start and the end of current selection if they started inside listNode
            // because moveParagraphWithClones could have removed them.
            if (rangeStartIsInList && newList)
                currentSelection.setStart(newList, 0, IGNORE_EXCEPTION);
            if (rangeEndIsInList && newList)
                currentSelection.setEnd(newList, Position::lastOffsetInNode(newList), IGNORE_EXCEPTION);

            setEndingSelection(VisiblePosition::firstPositionInNode(newList));

            return true;
        }

        unlistifyParagraph(endingSelection().visibleStart(), listElement, listChildNode, editingState);
        if (editingState->isAborted())
            return false;
    }

    if (!listChildNode || switchListType || forceCreateList)
        listifyParagraph(endingSelection().visibleStart(), listTag, editingState);

    return true;
}

void InsertListCommand::unlistifyParagraph(const VisiblePosition& originalStart, HTMLElement* listElement, Node* listChildNode, EditingState* editingState)
{
    // Since, unlistify paragraph inserts nodes into parent and removes node
    // from parent, if parent of |listElement| should be editable.
    DCHECK(listElement->parentNode()->hasEditableStyle());
    Node* nextListChild;
    Node* previousListChild;
    VisiblePosition start;
    VisiblePosition end;
    DCHECK(listChildNode);
    if (isHTMLLIElement(*listChildNode)) {
        start = VisiblePosition::firstPositionInNode(listChildNode);
        end = VisiblePosition::lastPositionInNode(listChildNode);
        nextListChild = listChildNode->nextSibling();
        previousListChild = listChildNode->previousSibling();
    } else {
        // A paragraph is visually a list item minus a list marker.  The paragraph will be moved.
        start = startOfParagraph(originalStart, CanSkipOverEditingBoundary);
        end = endOfParagraph(start, CanSkipOverEditingBoundary);
        nextListChild = enclosingListChild(nextPositionOf(end).deepEquivalent().anchorNode(), listElement);
        DCHECK_NE(nextListChild, listChildNode);
        previousListChild = enclosingListChild(previousPositionOf(start).deepEquivalent().anchorNode(), listElement);
        DCHECK_NE(previousListChild, listChildNode);
    }
    // When removing a list, we must always create a placeholder to act as a point of insertion
    // for the list content being removed.
    HTMLBRElement* placeholder = HTMLBRElement::create(document());
    HTMLElement* elementToInsert = placeholder;
    // If the content of the list item will be moved into another list, put it in a list item
    // so that we don't create an orphaned list child.
    if (enclosingList(listElement)) {
        elementToInsert = HTMLLIElement::create(document());
        appendNode(placeholder, elementToInsert, editingState);
        if (editingState->isAborted())
            return;
    }

    if (nextListChild && previousListChild) {
        // We want to pull listChildNode out of listNode, and place it before nextListChild
        // and after previousListChild, so we split listNode and insert it between the two lists.
        // But to split listNode, we must first split ancestors of listChildNode between it and listNode,
        // if any exist.
        // FIXME: We appear to split at nextListChild as opposed to listChildNode so that when we remove
        // listChildNode below in moveParagraphs, previousListChild will be removed along with it if it is
        // unrendered. But we ought to remove nextListChild too, if it is unrendered.
        splitElement(listElement, splitTreeToNode(nextListChild, listElement));
        insertNodeBefore(elementToInsert, listElement, editingState);
    } else if (nextListChild || listChildNode->parentNode() != listElement) {
        // Just because listChildNode has no previousListChild doesn't mean there isn't any content
        // in listNode that comes before listChildNode, as listChildNode could have ancestors
        // between it and listNode. So, we split up to listNode before inserting the placeholder
        // where we're about to move listChildNode to.
        if (listChildNode->parentNode() != listElement)
            splitElement(listElement, splitTreeToNode(listChildNode, listElement));
        insertNodeBefore(elementToInsert, listElement, editingState);
    } else {
        insertNodeAfter(elementToInsert, listElement, editingState);
    }
    if (editingState->isAborted())
        return;

    VisiblePosition insertionPoint = VisiblePosition::beforeNode(placeholder);
    moveParagraphs(start, end, insertionPoint, editingState, PreserveSelection, PreserveStyle, listChildNode);
}

static HTMLElement* adjacentEnclosingList(const VisiblePosition& pos, const VisiblePosition& adjacentPos, const HTMLQualifiedName& listTag)
{
    HTMLElement* listElement = outermostEnclosingList(adjacentPos.deepEquivalent().anchorNode());

    if (!listElement)
        return 0;

    Element* previousCell = enclosingTableCell(pos.deepEquivalent());
    Element* currentCell = enclosingTableCell(adjacentPos.deepEquivalent());

    if (!listElement->hasTagName(listTag)
        || listElement->contains(pos.deepEquivalent().anchorNode())
        || previousCell != currentCell
        || enclosingList(listElement) != enclosingList(pos.deepEquivalent().anchorNode()))
        return 0;

    return listElement;
}

void InsertListCommand::listifyParagraph(const VisiblePosition& originalStart, const HTMLQualifiedName& listTag, EditingState* editingState)
{
    const VisiblePosition& start = startOfParagraph(originalStart, CanSkipOverEditingBoundary);
    const VisiblePosition& end = endOfParagraph(start, CanSkipOverEditingBoundary);

    if (start.isNull() || end.isNull())
        return;

    // Check for adjoining lists.
    HTMLElement* const previousList = adjacentEnclosingList(start, previousPositionOf(start, CannotCrossEditingBoundary), listTag);
    HTMLElement* const nextList = adjacentEnclosingList(start, nextPositionOf(end, CannotCrossEditingBoundary), listTag);
    if (previousList || nextList) {
        // Place list item into adjoining lists.
        HTMLLIElement* listItemElement = HTMLLIElement::create(document());
        if (previousList)
            appendNode(listItemElement, previousList, editingState);
        else
            insertNodeAt(listItemElement, Position::beforeNode(nextList), editingState);
        if (editingState->isAborted())
            return;

        moveParagraphOverPositionIntoEmptyListItem(start, listItemElement, editingState);
        if (editingState->isAborted())
            return;

        if (canMergeLists(previousList, nextList))
            mergeIdenticalElements(previousList, nextList, editingState);

        return;
    }

    // Create new list element.

    // Inserting the list into an empty paragraph that isn't held open
    // by a br or a '\n', will invalidate start and end.  Insert
    // a placeholder and then recompute start and end.
    Position startPos = start.deepEquivalent();
    if (start.deepEquivalent() == end.deepEquivalent() && isEnclosingBlock(start.deepEquivalent().anchorNode())) {
        HTMLBRElement* placeholder = insertBlockPlaceholder(startPos, editingState);
        if (editingState->isAborted())
            return;
        startPos = Position::beforeNode(placeholder);
    }

    // Insert the list at a position visually equivalent to start of the
    // paragraph that is being moved into the list.
    // Try to avoid inserting it somewhere where it will be surrounded by
    // inline ancestors of start, since it is easier for editing to produce
    // clean markup when inline elements are pushed down as far as possible.
    Position insertionPos(mostBackwardCaretPosition(startPos));
    // Also avoid the containing list item.
    Node* const listChild = enclosingListChild(insertionPos.anchorNode());
    if (isHTMLLIElement(listChild))
        insertionPos = Position::inParentBeforeNode(*listChild);

    HTMLElement* listElement = createHTMLElement(document(), listTag);
    insertNodeAt(listElement, insertionPos, editingState);
    if (editingState->isAborted())
        return;
    HTMLLIElement* listItemElement = HTMLLIElement::create(document());
    appendNode(listItemElement, listElement, editingState);
    if (editingState->isAborted())
        return;

    // We inserted the list at the start of the content we're about to move
    // Update the start of content, so we don't try to move the list into itself.  bug 19066
    // Layout is necessary since start's node's inline layoutObjects may have been destroyed by the insertion
    // The end of the content may have changed after the insertion and layout so update it as well.
    if (insertionPos == startPos)
        moveParagraphOverPositionIntoEmptyListItem(originalStart, listItemElement, editingState);
    else
        moveParagraphOverPositionIntoEmptyListItem(createVisiblePosition(startPos), listItemElement, editingState);
    if (editingState->isAborted())
        return;

    mergeWithNeighboringLists(listElement, editingState);
}

void InsertListCommand::moveParagraphOverPositionIntoEmptyListItem(const VisiblePosition& pos, HTMLLIElement* listItemElement, EditingState* editingState)
{
    DCHECK(!listItemElement->hasChildren());
    HTMLBRElement* placeholder = HTMLBRElement::create(document());
    appendNode(placeholder, listItemElement, editingState);
    if (editingState->isAborted())
        return;
    // Inserting list element and list item list may change start of pargraph
    // to move. We calculate start of paragraph again.
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    const VisiblePosition& start = startOfParagraph(pos, CanSkipOverEditingBoundary);
    const VisiblePosition& end = endOfParagraph(pos, CanSkipOverEditingBoundary);
    moveParagraph(start, end, VisiblePosition::beforeNode(placeholder), editingState, PreserveSelection);
}

DEFINE_TRACE(InsertListCommand)
{
    CompositeEditCommand::trace(visitor);
}

} // namespace blink
