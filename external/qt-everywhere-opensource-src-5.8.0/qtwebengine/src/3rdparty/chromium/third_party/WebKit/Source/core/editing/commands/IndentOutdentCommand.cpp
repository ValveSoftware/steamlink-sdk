/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (IndentOutdentCommandINCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/commands/IndentOutdentCommand.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/InsertListCommand.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutObject.h"

namespace blink {

using namespace HTMLNames;

// Returns true if |node| is UL, OL, or BLOCKQUOTE with "display:block".
// "Outdent" command considers <BLOCKQUOTE style="display:inline"> makes
// indentation.
static bool isHTMLListOrBlockquoteElement(const Node* node)
{
    if (!node || !node->isHTMLElement())
        return false;
    if (!node->layoutObject() || !node->layoutObject()->isLayoutBlock())
        return false;
    const HTMLElement& element = toHTMLElement(*node);
    // TODO(yosin): We should check OL/UL element has "list-style-type" CSS
    // property to make sure they layout contents as list.
    return isHTMLUListElement(element) || isHTMLOListElement(element) || element.hasTagName(blockquoteTag);
}

IndentOutdentCommand::IndentOutdentCommand(Document& document, EIndentType typeOfAction)
    : ApplyBlockElementCommand(document, blockquoteTag, "margin: 0 0 0 40px; border: none; padding: 0px;")
    , m_typeOfAction(typeOfAction)
{
}

bool IndentOutdentCommand::tryIndentingAsListItem(const Position& start, const Position& end, EditingState* editingState)
{
    // If our selection is not inside a list, bail out.
    Node* lastNodeInSelectedParagraph = start.anchorNode();
    HTMLElement* listElement = enclosingList(lastNodeInSelectedParagraph);
    if (!listElement)
        return false;

    // Find the block that we want to indent.  If it's not a list item (e.g., a div inside a list item), we bail out.
    Element* selectedListItem = enclosingBlock(lastNodeInSelectedParagraph);

    // FIXME: we need to deal with the case where there is no li (malformed HTML)
    if (!isHTMLLIElement(selectedListItem))
        return false;

    // FIXME: previousElementSibling does not ignore non-rendered content like <span></span>.  Should we?
    Element* previousList = ElementTraversal::previousSibling(*selectedListItem);
    Element* nextList = ElementTraversal::nextSibling(*selectedListItem);

    // We should calculate visible range in list item because inserting new
    // list element will change visibility of list item, e.g. :first-child
    // CSS selector.
    HTMLElement* newList = toHTMLElement(document().createElement(listElement->tagQName(), CreatedByCloneNode));
    insertNodeBefore(newList, selectedListItem, editingState);
    if (editingState->isAborted())
        return false;

    // We should clone all the children of the list item for indenting purposes. However, in case the current
    // selection does not encompass all its children, we need to explicitally handle the same. The original
    // list item too would require proper deletion in that case.
    if (end.anchorNode() == selectedListItem || end.anchorNode()->isDescendantOf(selectedListItem->lastChild())) {
        moveParagraphWithClones(createVisiblePosition(start), createVisiblePosition(end), newList, selectedListItem, editingState);
    } else {
        moveParagraphWithClones(createVisiblePosition(start), VisiblePosition::afterNode(selectedListItem->lastChild()), newList, selectedListItem, editingState);
        if (editingState->isAborted())
            return false;
        removeNode(selectedListItem, editingState);
    }
    if (editingState->isAborted())
        return false;

    if (canMergeLists(previousList, newList)) {
        mergeIdenticalElements(previousList, newList, editingState);
        if (editingState->isAborted())
            return false;
    }
    if (canMergeLists(newList, nextList)) {
        mergeIdenticalElements(newList, nextList, editingState);
        if (editingState->isAborted())
            return false;
    }

    return true;
}

void IndentOutdentCommand::indentIntoBlockquote(const Position& start, const Position& end, HTMLElement*& targetBlockquote, EditingState* editingState)
{
    Element* enclosingCell = toElement(enclosingNodeOfType(start, &isTableCell));
    Element* elementToSplitTo;
    if (enclosingCell)
        elementToSplitTo = enclosingCell;
    else if (enclosingList(start.computeContainerNode()))
        elementToSplitTo = enclosingBlock(start.computeContainerNode());
    else
        elementToSplitTo = rootEditableElementOf(start);

    if (!elementToSplitTo)
        return;

    Node* outerBlock = (start.computeContainerNode() == elementToSplitTo) ? start.computeContainerNode() : splitTreeToNode(start.computeContainerNode(), elementToSplitTo);

    VisiblePosition startOfContents = createVisiblePosition(start);
    if (!targetBlockquote) {
        // Create a new blockquote and insert it as a child of the root editable element. We accomplish
        // this by splitting all parents of the current paragraph up to that point.
        targetBlockquote = createBlockElement();
        if (outerBlock == start.computeContainerNode())
            insertNodeAt(targetBlockquote, start, editingState);
        else
            insertNodeBefore(targetBlockquote, outerBlock, editingState);
        if (editingState->isAborted())
            return;
        startOfContents = VisiblePosition::inParentAfterNode(*targetBlockquote);
    }

    VisiblePosition endOfContents = createVisiblePosition(end);
    if (startOfContents.isNull() || endOfContents.isNull())
        return;
    moveParagraphWithClones(startOfContents, endOfContents, targetBlockquote, outerBlock, editingState);
}

void IndentOutdentCommand::outdentParagraph(EditingState* editingState)
{
    VisiblePosition visibleStartOfParagraph = startOfParagraph(endingSelection().visibleStart());
    VisiblePosition visibleEndOfParagraph = endOfParagraph(visibleStartOfParagraph);

    HTMLElement* enclosingElement = toHTMLElement(enclosingNodeOfType(visibleStartOfParagraph.deepEquivalent(), &isHTMLListOrBlockquoteElement));
    if (!enclosingElement || !enclosingElement->parentNode()->hasEditableStyle()) // We can't outdent if there is no place to go!
        return;

    // Use InsertListCommand to remove the selection from the list
    if (isHTMLOListElement(*enclosingElement)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::OrderedList), editingState);
        return;
    }
    if (isHTMLUListElement(*enclosingElement)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::UnorderedList), editingState);
        return;
    }

    // The selection is inside a blockquote i.e. enclosingNode is a blockquote
    VisiblePosition positionInEnclosingBlock = VisiblePosition::firstPositionInNode(enclosingElement);
    // If the blockquote is inline, the start of the enclosing block coincides with
    // positionInEnclosingBlock.
    VisiblePosition startOfEnclosingBlock = (enclosingElement->layoutObject() && enclosingElement->layoutObject()->isInline()) ? positionInEnclosingBlock : startOfBlock(positionInEnclosingBlock);
    VisiblePosition lastPositionInEnclosingBlock = VisiblePosition::lastPositionInNode(enclosingElement);
    VisiblePosition endOfEnclosingBlock = endOfBlock(lastPositionInEnclosingBlock);
    if (visibleStartOfParagraph.deepEquivalent() == startOfEnclosingBlock.deepEquivalent()
        && visibleEndOfParagraph.deepEquivalent() == endOfEnclosingBlock.deepEquivalent()) {
        // The blockquote doesn't contain anything outside the paragraph, so it can be totally removed.
        Node* splitPoint = enclosingElement->nextSibling();
        removeNodePreservingChildren(enclosingElement, editingState);
        if (editingState->isAborted())
            return;
        // outdentRegion() assumes it is operating on the first paragraph of an enclosing blockquote, but if there are multiply nested blockquotes and we've
        // just removed one, then this assumption isn't true. By splitting the next containing blockquote after this node, we keep this assumption true
        if (splitPoint) {
            if (Element* splitPointParent = splitPoint->parentElement()) {
                if (splitPointParent->hasTagName(blockquoteTag)
                    && !splitPoint->hasTagName(blockquoteTag)
                    && splitPointParent->parentNode()->hasEditableStyle()) // We can't outdent if there is no place to go!
                    splitElement(splitPointParent, splitPoint);
            }
        }

        document().updateStyleAndLayoutIgnorePendingStylesheets();
        visibleStartOfParagraph = createVisiblePosition(visibleStartOfParagraph.deepEquivalent());
        visibleEndOfParagraph = createVisiblePosition(visibleEndOfParagraph.deepEquivalent());
        if (visibleStartOfParagraph.isNotNull() && !isStartOfParagraph(visibleStartOfParagraph)) {
            insertNodeAt(HTMLBRElement::create(document()), visibleStartOfParagraph.deepEquivalent(), editingState);
            if (editingState->isAborted())
                return;
        }
        if (visibleEndOfParagraph.isNotNull() && !isEndOfParagraph(visibleEndOfParagraph))
            insertNodeAt(HTMLBRElement::create(document()), visibleEndOfParagraph.deepEquivalent(), editingState);
        return;
    }
    Node* splitBlockquoteNode = enclosingElement;
    if (Element* enclosingBlockFlow = enclosingBlock(visibleStartOfParagraph.deepEquivalent().anchorNode())) {
        if (enclosingBlockFlow != enclosingElement) {
            splitBlockquoteNode = splitTreeToNode(enclosingBlockFlow, enclosingElement, true);
        } else {
            // We split the blockquote at where we start outdenting.
            Node* highestInlineNode = highestEnclosingNodeOfType(visibleStartOfParagraph.deepEquivalent(), isInline, CannotCrossEditingBoundary, enclosingBlockFlow);
            splitElement(enclosingElement, highestInlineNode ? highestInlineNode : visibleStartOfParagraph.deepEquivalent().anchorNode());
        }
    }
    VisiblePosition startOfParagraphToMove = startOfParagraph(visibleStartOfParagraph);
    VisiblePosition endOfParagraphToMove = endOfParagraph(visibleEndOfParagraph);
    if (startOfParagraphToMove.isNull() || endOfParagraphToMove.isNull())
        return;
    HTMLBRElement* placeholder = HTMLBRElement::create(document());
    insertNodeBefore(placeholder, splitBlockquoteNode, editingState);
    if (editingState->isAborted())
        return;
    moveParagraph(startOfParagraphToMove, endOfParagraphToMove, VisiblePosition::beforeNode(placeholder), editingState, PreserveSelection);
}

// FIXME: We should merge this function with ApplyBlockElementCommand::formatSelection
void IndentOutdentCommand::outdentRegion(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection, EditingState* editingState)
{
    VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
    VisiblePosition endOfLastParagraph = endOfParagraph(endOfSelection);

    if (endOfCurrentParagraph.deepEquivalent() == endOfLastParagraph.deepEquivalent()) {
        outdentParagraph(editingState);
        return;
    }

    Position originalSelectionEnd = endingSelection().end();
    VisiblePosition endAfterSelection = endOfParagraph(nextPositionOf(endOfLastParagraph));

    while (endOfCurrentParagraph.deepEquivalent() != endAfterSelection.deepEquivalent()) {
        VisiblePosition endOfNextParagraph = endOfParagraph(nextPositionOf(endOfCurrentParagraph));
        if (endOfCurrentParagraph.deepEquivalent() == endOfLastParagraph.deepEquivalent())
            setEndingSelection(VisibleSelection(originalSelectionEnd, TextAffinity::Downstream));
        else
            setEndingSelection(endOfCurrentParagraph);

        outdentParagraph(editingState);
        if (editingState->isAborted())
            return;

        // outdentParagraph could move more than one paragraph if the paragraph
        // is in a list item. As a result, endAfterSelection and endOfNextParagraph
        // could refer to positions no longer in the document.
        if (endAfterSelection.isNotNull() && !endAfterSelection.deepEquivalent().inShadowIncludingDocument())
            break;

        if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.deepEquivalent().inShadowIncludingDocument()) {
            endOfCurrentParagraph = createVisiblePosition(endingSelection().end());
            endOfNextParagraph = endOfParagraph(nextPositionOf(endOfCurrentParagraph));
        }
        endOfCurrentParagraph = endOfNextParagraph;
    }
}

void IndentOutdentCommand::formatSelection(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection, EditingState* editingState)
{
    if (m_typeOfAction == Indent)
        ApplyBlockElementCommand::formatSelection(startOfSelection, endOfSelection, editingState);
    else
        outdentRegion(startOfSelection, endOfSelection, editingState);
}

void IndentOutdentCommand::formatRange(const Position& start, const Position& end, const Position&, HTMLElement*& blockquoteForNextIndent, EditingState* editingState)
{
    bool indentingAsListItemResult = tryIndentingAsListItem(start, end, editingState);
    if (editingState->isAborted())
        return;
    if (indentingAsListItemResult)
        blockquoteForNextIndent = nullptr;
    else
        indentIntoBlockquote(start, end, blockquoteForNextIndent, editingState);
}

} // namespace blink
