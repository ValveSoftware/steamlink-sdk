/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "core/editing/commands/InsertParagraphSeparatorCommand.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/InsertLineBreakCommand.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLQuoteElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"

namespace blink {

using namespace HTMLNames;

// When inserting a new line, we want to avoid nesting empty divs if we can.  Otherwise, when
// pasting, it's easy to have each new line be a div deeper than the previous.  E.g., in the case
// below, we want to insert at ^ instead of |.
// <div>foo<div>bar</div>|</div>^
static Element* highestVisuallyEquivalentDivBelowRoot(Element* startBlock)
{
    Element* curBlock = startBlock;
    // We don't want to return a root node (if it happens to be a div, e.g., in a document fragment) because there are no
    // siblings for us to append to.
    while (!curBlock->nextSibling() && isHTMLDivElement(*curBlock->parentElement()) && curBlock->parentElement()->parentElement()) {
        if (curBlock->parentElement()->hasAttributes())
            break;
        curBlock = curBlock->parentElement();
    }
    return curBlock;
}

InsertParagraphSeparatorCommand::InsertParagraphSeparatorCommand(Document& document, bool mustUseDefaultParagraphElement, bool pasteBlockquoteIntoUnquotedArea)
    : CompositeEditCommand(document)
    , m_mustUseDefaultParagraphElement(mustUseDefaultParagraphElement)
    , m_pasteBlockquoteIntoUnquotedArea(pasteBlockquoteIntoUnquotedArea)
{
}

bool InsertParagraphSeparatorCommand::preservesTypingStyle() const
{
    return true;
}

void InsertParagraphSeparatorCommand::calculateStyleBeforeInsertion(const Position &pos)
{
    // It is only important to set a style to apply later if we're at the boundaries of
    // a paragraph. Otherwise, content that is moved as part of the work of the command
    // will lend their styles to the new paragraph without any extra work needed.
    VisiblePosition visiblePos = createVisiblePosition(pos, VP_DEFAULT_AFFINITY);
    if (!isStartOfParagraph(visiblePos) && !isEndOfParagraph(visiblePos))
        return;

    DCHECK(pos.isNotNull());
    m_style = EditingStyle::create(pos);
    m_style->mergeTypingStyle(pos.document());
}

void InsertParagraphSeparatorCommand::applyStyleAfterInsertion(Element* originalEnclosingBlock, EditingState* editingState)
{
    // Not only do we break out of header tags, but we also do not preserve the typing style,
    // in order to match other browsers.
    if (originalEnclosingBlock->hasTagName(h1Tag)
        || originalEnclosingBlock->hasTagName(h2Tag)
        || originalEnclosingBlock->hasTagName(h3Tag)
        || originalEnclosingBlock->hasTagName(h4Tag)
        || originalEnclosingBlock->hasTagName(h5Tag)) {
        return;
    }

    if (!m_style)
        return;

    m_style->prepareToApplyAt(endingSelection().start());
    if (!m_style->isEmpty())
        applyStyle(m_style.get(), editingState);
}

bool InsertParagraphSeparatorCommand::shouldUseDefaultParagraphElement(Element* enclosingBlock) const
{
    if (m_mustUseDefaultParagraphElement)
        return true;

    // Assumes that if there was a range selection, it was already deleted.
    if (!isEndOfBlock(endingSelection().visibleStart()))
        return false;

    return enclosingBlock->hasTagName(h1Tag)
        || enclosingBlock->hasTagName(h2Tag)
        || enclosingBlock->hasTagName(h3Tag)
        || enclosingBlock->hasTagName(h4Tag)
        || enclosingBlock->hasTagName(h5Tag);
}

void InsertParagraphSeparatorCommand::getAncestorsInsideBlock(const Node* insertionNode, Element* outerBlock, HeapVector<Member<Element>>& ancestors)
{
    ancestors.clear();

    // Build up list of ancestors elements between the insertion node and the outer block.
    if (insertionNode != outerBlock) {
        for (Element* n = insertionNode->parentElement(); n && n != outerBlock; n = n->parentElement())
            ancestors.append(n);
    }
}

Element* InsertParagraphSeparatorCommand::cloneHierarchyUnderNewBlock(const HeapVector<Member<Element>>& ancestors, Element* blockToInsert, EditingState* editingState)
{
    // Make clones of ancestors in between the start node and the start block.
    Element* parent = blockToInsert;
    for (size_t i = ancestors.size(); i != 0; --i) {
        Element* child = ancestors[i - 1]->cloneElementWithoutChildren();
        // It should always be okay to remove id from the cloned elements, since the originals are not deleted.
        child->removeAttribute(idAttr);
        appendNode(child, parent, editingState);
        if (editingState->isAborted())
            return nullptr;
        parent = child;
    }

    return parent;
}

void InsertParagraphSeparatorCommand::doApply(EditingState* editingState)
{
    if (!endingSelection().isNonOrphanedCaretOrRange())
        return;

    Position insertionPosition = endingSelection().start();

    TextAffinity affinity = endingSelection().affinity();

    // Delete the current selection.
    if (endingSelection().isRange()) {
        calculateStyleBeforeInsertion(insertionPosition);
        deleteSelection(editingState, false, true);
        if (editingState->isAborted())
            return;
        insertionPosition = endingSelection().start();
        affinity = endingSelection().affinity();
    }

    // FIXME: The parentAnchoredEquivalent conversion needs to be moved into enclosingBlock.
    Element* startBlock = enclosingBlock(insertionPosition.parentAnchoredEquivalent().computeContainerNode());
    Node* listChildNode = enclosingListChild(insertionPosition.parentAnchoredEquivalent().computeContainerNode());
    HTMLElement* listChild = listChildNode && listChildNode->isHTMLElement() ? toHTMLElement(listChildNode) : 0;
    Position canonicalPos = createVisiblePosition(insertionPosition).deepEquivalent();
    if (!startBlock
        || !startBlock->nonShadowBoundaryParentNode()
        || isTableCell(startBlock)
        || isHTMLFormElement(*startBlock)
        // FIXME: If the node is hidden, we don't have a canonical position so we will do the wrong thing for tables and <hr>. https://bugs.webkit.org/show_bug.cgi?id=40342
        || (!canonicalPos.isNull() && isDisplayInsideTable(canonicalPos.anchorNode()))
        || (!canonicalPos.isNull() && isHTMLHRElement(*canonicalPos.anchorNode()))) {
        applyCommandToComposite(InsertLineBreakCommand::create(document()), editingState);
        return;
    }

    // Use the leftmost candidate.
    insertionPosition = mostBackwardCaretPosition(insertionPosition);
    if (!isVisuallyEquivalentCandidate(insertionPosition))
        insertionPosition = mostForwardCaretPosition(insertionPosition);

    // Adjust the insertion position after the delete
    const Position originalInsertionPosition = insertionPosition;
    const Element* enclosingAnchor = enclosingAnchorElement(originalInsertionPosition);
    insertionPosition = positionAvoidingSpecialElementBoundary(insertionPosition, editingState);
    if (editingState->isAborted())
        return;
    if (listChild == enclosingAnchor) {
        // |positionAvoidingSpecialElementBoundary()| creates new A element and
        // move to another place.
        listChild = toHTMLElement(enclosingAnchorElement(originalInsertionPosition));
    }
    VisiblePosition visiblePos = createVisiblePosition(insertionPosition, affinity);
    calculateStyleBeforeInsertion(insertionPosition);

    //---------------------------------------------------------------------
    // Handle special case of typing return on an empty list item
    if (breakOutOfEmptyListItem(editingState) || editingState->isAborted())
        return;

    //---------------------------------------------------------------------
    // Prepare for more general cases.

    bool isFirstInBlock = isStartOfBlock(visiblePos);
    bool isLastInBlock = isEndOfBlock(visiblePos);
    bool nestNewBlock = false;

    // Create block to be inserted.
    Element* blockToInsert = nullptr;
    if (startBlock->isRootEditableElement()) {
        blockToInsert = createDefaultParagraphElement(document());
        nestNewBlock = true;
    } else if (shouldUseDefaultParagraphElement(startBlock)) {
        blockToInsert = createDefaultParagraphElement(document());
    } else {
        blockToInsert = startBlock->cloneElementWithoutChildren();
    }

    //---------------------------------------------------------------------
    // Handle case when position is in the last visible position in its block,
    // including when the block is empty.
    if (isLastInBlock) {
        if (nestNewBlock) {
            if (isFirstInBlock && !lineBreakExistsAtVisiblePosition(visiblePos)) {
                // The block is empty.  Create an empty block to
                // represent the paragraph that we're leaving.
                HTMLElement* extraBlock = createDefaultParagraphElement(document());
                appendNode(extraBlock, startBlock, editingState);
                if (editingState->isAborted())
                    return;
                appendBlockPlaceholder(extraBlock, editingState);
                if (editingState->isAborted())
                    return;
            }
            appendNode(blockToInsert, startBlock, editingState);
            if (editingState->isAborted())
                return;
        } else {
            // We can get here if we pasted a copied portion of a blockquote with a newline at the end and are trying to paste it
            // into an unquoted area. We then don't want the newline within the blockquote or else it will also be quoted.
            if (m_pasteBlockquoteIntoUnquotedArea) {
                if (HTMLQuoteElement* highestBlockquote = toHTMLQuoteElement(highestEnclosingNodeOfType(canonicalPos, &isMailHTMLBlockquoteElement)))
                    startBlock = highestBlockquote;
            }

            if (listChild && listChild != startBlock) {
                Element* listChildToInsert = listChild->cloneElementWithoutChildren();
                appendNode(blockToInsert, listChildToInsert, editingState);
                if (editingState->isAborted())
                    return;
                insertNodeAfter(listChildToInsert, listChild, editingState);
            } else {
                // Most of the time we want to stay at the nesting level of the startBlock (e.g., when nesting within lists). However,
                // for div nodes, this can result in nested div tags that are hard to break out of.
                Element* siblingElement = startBlock;
                if (isHTMLDivElement(*blockToInsert))
                    siblingElement = highestVisuallyEquivalentDivBelowRoot(startBlock);
                insertNodeAfter(blockToInsert, siblingElement, editingState);
            }
            if (editingState->isAborted())
                return;
        }

        // Recreate the same structure in the new paragraph.

        HeapVector<Member<Element>> ancestors;
        getAncestorsInsideBlock(positionOutsideTabSpan(insertionPosition).anchorNode(), startBlock, ancestors);
        Element* parent = cloneHierarchyUnderNewBlock(ancestors, blockToInsert, editingState);
        if (editingState->isAborted())
            return;

        appendBlockPlaceholder(parent, editingState);
        if (editingState->isAborted())
            return;

        setEndingSelection(VisibleSelection(Position::firstPositionInNode(parent), TextAffinity::Downstream, endingSelection().isDirectional()));
        return;
    }


    //---------------------------------------------------------------------
    // Handle case when position is in the first visible position in its block, and
    // similar case where previous position is in another, presumeably nested, block.
    if (isFirstInBlock || !inSameBlock(visiblePos, previousPositionOf(visiblePos))) {
        Node* refNode = nullptr;
        insertionPosition = positionOutsideTabSpan(insertionPosition);

        if (isFirstInBlock && !nestNewBlock) {
            if (listChild && listChild != startBlock) {
                Element* listChildToInsert = listChild->cloneElementWithoutChildren();
                appendNode(blockToInsert, listChildToInsert, editingState);
                if (editingState->isAborted())
                    return;
                insertNodeBefore(listChildToInsert, listChild, editingState);
                if (editingState->isAborted())
                    return;
            } else {
                refNode = startBlock;
            }
        } else if (isFirstInBlock && nestNewBlock) {
            // startBlock should always have children, otherwise isLastInBlock would be true and it's handled above.
            DCHECK(startBlock->hasChildren());
            refNode = startBlock->firstChild();
        } else if (insertionPosition.anchorNode() == startBlock && nestNewBlock) {
            refNode = NodeTraversal::childAt(*startBlock, insertionPosition.computeEditingOffset());
            DCHECK(refNode); // must be true or we'd be in the end of block case
        } else {
            refNode = insertionPosition.anchorNode();
        }

        // find ending selection position easily before inserting the paragraph
        insertionPosition = mostForwardCaretPosition(insertionPosition);

        if (refNode) {
            insertNodeBefore(blockToInsert, refNode, editingState);
            if (editingState->isAborted())
                return;
        }

        // Recreate the same structure in the new paragraph.

        HeapVector<Member<Element>> ancestors;
        insertionPosition = positionAvoidingSpecialElementBoundary(positionOutsideTabSpan(insertionPosition), editingState);
        if (editingState->isAborted())
            return;
        getAncestorsInsideBlock(insertionPosition.anchorNode(), startBlock, ancestors);

        Element* placeholder = cloneHierarchyUnderNewBlock(ancestors, blockToInsert, editingState);
        if (editingState->isAborted())
            return;
        appendBlockPlaceholder(placeholder, editingState);
        if (editingState->isAborted())
            return;

        // In this case, we need to set the new ending selection.
        setEndingSelection(VisibleSelection(insertionPosition, TextAffinity::Downstream, endingSelection().isDirectional()));
        return;
    }

    //---------------------------------------------------------------------
    // Handle the (more complicated) general case,

    // All of the content in the current block after visiblePos is
    // about to be wrapped in a new paragraph element.  Add a br before
    // it if visiblePos is at the start of a paragraph so that the
    // content will move down a line.
    if (isStartOfParagraph(visiblePos)) {
        HTMLBRElement* br = HTMLBRElement::create(document());
        insertNodeAt(br, insertionPosition, editingState);
        if (editingState->isAborted())
            return;
        insertionPosition = Position::inParentAfterNode(*br);
        // If the insertion point is a break element, there is nothing else
        // we need to do.
        if (visiblePos.deepEquivalent().anchorNode()->layoutObject()->isBR()) {
            setEndingSelection(VisibleSelection(insertionPosition, TextAffinity::Downstream, endingSelection().isDirectional()));
            return;
        }
    }

    // Move downstream. Typing style code will take care of carrying along the
    // style of the upstream position.
    insertionPosition = mostForwardCaretPosition(insertionPosition);

    // At this point, the insertionPosition's node could be a container, and we want to make sure we include
    // all of the correct nodes when building the ancestor list.  So this needs to be the deepest representation of the position
    // before we walk the DOM tree.
    insertionPosition = positionOutsideTabSpan(createVisiblePosition(insertionPosition).deepEquivalent());

    // If the returned position lies either at the end or at the start of an element that is ignored by editing
    // we should move to its upstream or downstream position.
    if (editingIgnoresContent(insertionPosition.anchorNode())) {
        if (insertionPosition.atLastEditingPositionForNode())
            insertionPosition = mostForwardCaretPosition(insertionPosition);
        else if (insertionPosition.atFirstEditingPositionForNode())
            insertionPosition = mostBackwardCaretPosition(insertionPosition);
    }

    // Make sure we do not cause a rendered space to become unrendered.
    // FIXME: We need the affinity for pos, but mostForwardCaretPosition does not give it
    Position leadingWhitespace = leadingWhitespacePosition(insertionPosition, VP_DEFAULT_AFFINITY);
    // FIXME: leadingWhitespacePosition is returning the position before preserved newlines for positions
    // after the preserved newline, causing the newline to be turned into a nbsp.
    if (leadingWhitespace.isNotNull() && leadingWhitespace.anchorNode()->isTextNode()) {
        Text* textNode = toText(leadingWhitespace.anchorNode());
        DCHECK(!textNode->layoutObject() || textNode->layoutObject()->style()->collapseWhiteSpace()) << textNode;
        replaceTextInNodePreservingMarkers(textNode, leadingWhitespace.computeOffsetInContainerNode(), 1, nonBreakingSpaceString());
    }

    // Split at pos if in the middle of a text node.
    Position positionAfterSplit;
    if (insertionPosition.isOffsetInAnchor() && insertionPosition.computeContainerNode()->isTextNode()) {
        Text* textNode = toText(insertionPosition.computeContainerNode());
        int textOffset = insertionPosition.offsetInContainerNode();
        bool atEnd = static_cast<unsigned>(textOffset) >= textNode->length();
        if (textOffset > 0 && !atEnd) {
            splitTextNode(textNode, textOffset);
            positionAfterSplit = Position::firstPositionInNode(textNode);
            insertionPosition = Position(textNode->previousSibling(), textOffset);
            visiblePos = createVisiblePosition(insertionPosition);
        }
    }

    // If we got detached due to mutation events, just bail out.
    if (!startBlock->parentNode())
        return;

    // Put the added block in the tree.
    if (nestNewBlock) {
        appendNode(blockToInsert, startBlock, editingState);
    } else if (listChild && listChild != startBlock) {
        Element* listChildToInsert = listChild->cloneElementWithoutChildren();
        appendNode(blockToInsert, listChildToInsert, editingState);
        if (editingState->isAborted())
            return;
        insertNodeAfter(listChildToInsert, listChild, editingState);
    } else {
        insertNodeAfter(blockToInsert, startBlock, editingState);
    }
    if (editingState->isAborted())
        return;

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // If the paragraph separator was inserted at the end of a paragraph, an empty line must be
    // created.  All of the nodes, starting at visiblePos, are about to be added to the new paragraph
    // element.  If the first node to be inserted won't be one that will hold an empty line open, add a br.
    if (isEndOfParagraph(visiblePos) && !lineBreakExistsAtVisiblePosition(visiblePos)) {
        appendNode(HTMLBRElement::create(document()), blockToInsert, editingState);
        if (editingState->isAborted())
            return;
    }

    // Move the start node and the siblings of the start node.
    if (createVisiblePosition(insertionPosition).deepEquivalent() != VisiblePosition::beforeNode(blockToInsert).deepEquivalent()) {
        Node* n;
        if (insertionPosition.computeContainerNode() == startBlock) {
            n = insertionPosition.computeNodeAfterPosition();
        } else {
            Node* splitTo = insertionPosition.computeContainerNode();
            if (splitTo->isTextNode() && insertionPosition.offsetInContainerNode() >= caretMaxOffset(splitTo))
                splitTo = NodeTraversal::next(*splitTo, startBlock);
            if (splitTo)
                splitTreeToNode(splitTo, startBlock);

            for (n = startBlock->firstChild(); n; n = n->nextSibling()) {
                VisiblePosition beforeNodePosition = VisiblePosition::beforeNode(n);
                if (!beforeNodePosition.isNull() && comparePositions(createVisiblePosition(insertionPosition), beforeNodePosition) <= 0)
                    break;
            }
        }

        moveRemainingSiblingsToNewParent(n, blockToInsert, blockToInsert, editingState);
        if (editingState->isAborted())
            return;
    }

    // Handle whitespace that occurs after the split
    if (positionAfterSplit.isNotNull()) {
        document().updateStyleAndLayoutIgnorePendingStylesheets();
        // TODO(yosin) |isRenderedCharacter()| should be removed, and we should
        // use |VisiblePosition::characterAfter()|.
        if (!isRenderedCharacter(positionAfterSplit)) {
            // Clear out all whitespace and insert one non-breaking space
            DCHECK(!positionAfterSplit.computeContainerNode()->layoutObject() || positionAfterSplit.computeContainerNode()->layoutObject()->style()->collapseWhiteSpace()) << positionAfterSplit;
            deleteInsignificantTextDownstream(positionAfterSplit);
            if (positionAfterSplit.anchorNode()->isTextNode())
                insertTextIntoNode(toText(positionAfterSplit.computeContainerNode()), 0, nonBreakingSpaceString());
        }
    }

    setEndingSelection(VisibleSelection(Position::firstPositionInNode(blockToInsert), TextAffinity::Downstream, endingSelection().isDirectional()));
    applyStyleAfterInsertion(startBlock, editingState);
}

DEFINE_TRACE(InsertParagraphSeparatorCommand)
{
    visitor->trace(m_style);
    CompositeEditCommand::trace(visitor);
}


} // namespace blink
