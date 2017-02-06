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

#include "core/editing/commands/InsertLineBreakCommand.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Text.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"

namespace blink {

using namespace HTMLNames;

InsertLineBreakCommand::InsertLineBreakCommand(Document& document)
    : CompositeEditCommand(document)
{
}

bool InsertLineBreakCommand::preservesTypingStyle() const
{
    return true;
}

// Whether we should insert a break element or a '\n'.
bool InsertLineBreakCommand::shouldUseBreakElement(const Position& insertionPos)
{
    // An editing position like [input, 0] actually refers to the position before
    // the input element, and in that case we need to check the input element's
    // parent's layoutObject.
    Position p(insertionPos.parentAnchoredEquivalent());
    return isRichlyEditablePosition(p) && p.anchorNode()->layoutObject() && !p.anchorNode()->layoutObject()->style()->preserveNewline();
}

void InsertLineBreakCommand::doApply(EditingState* editingState)
{
    deleteSelection(editingState);
    if (editingState->isAborted())
        return;
    VisibleSelection selection = endingSelection();
    if (!selection.isNonOrphanedCaretOrRange())
        return;

    VisiblePosition caret(selection.visibleStart());
    // FIXME: If the node is hidden, we should still be able to insert text.
    // For now, we return to avoid a crash.  https://bugs.webkit.org/show_bug.cgi?id=40342
    if (caret.isNull())
        return;

    Position pos(caret.deepEquivalent());

    pos = positionAvoidingSpecialElementBoundary(pos, editingState);
    if (editingState->isAborted())
        return;

    pos = positionOutsideTabSpan(pos);

    Node* nodeToInsert = nullptr;
    if (shouldUseBreakElement(pos))
        nodeToInsert = HTMLBRElement::create(document());
    else
        nodeToInsert = document().createTextNode("\n");

    // FIXME: Need to merge text nodes when inserting just after or before text.

    if (isEndOfParagraph(caret) && !lineBreakExistsAtVisiblePosition(caret)) {
        bool needExtraLineBreak = !isHTMLHRElement(*pos.anchorNode()) && !isHTMLTableElement(*pos.anchorNode());

        insertNodeAt(nodeToInsert, pos, editingState);
        if (editingState->isAborted())
            return;

        if (needExtraLineBreak) {
            Node* extraNode;
            // TODO(tkent): Can we remove HTMLTextFormControlElement dependency?
            if (HTMLTextFormControlElement* textControl = enclosingTextFormControl(nodeToInsert))
                extraNode = textControl->createPlaceholderBreakElement();
            else
                extraNode = nodeToInsert->cloneNode(false);
            insertNodeAfter(extraNode, nodeToInsert, editingState);
            if (editingState->isAborted())
                return;
            nodeToInsert = extraNode;
        }

        VisiblePosition endingPosition = VisiblePosition::beforeNode(nodeToInsert);
        setEndingSelection(VisibleSelection(endingPosition, endingSelection().isDirectional()));
    } else if (pos.computeEditingOffset() <= caretMinOffset(pos.anchorNode())) {
        insertNodeAt(nodeToInsert, pos, editingState);
        if (editingState->isAborted())
            return;

        // Insert an extra br or '\n' if the just inserted one collapsed.
        if (!isStartOfParagraph(VisiblePosition::beforeNode(nodeToInsert))) {
            insertNodeBefore(nodeToInsert->cloneNode(false), nodeToInsert, editingState);
            if (editingState->isAborted())
                return;
        }

        setEndingSelection(VisibleSelection(Position::inParentAfterNode(*nodeToInsert), TextAffinity::Downstream, endingSelection().isDirectional()));
    // If we're inserting after all of the rendered text in a text node, or into a non-text node,
    // a simple insertion is sufficient.
    } else if (!pos.anchorNode()->isTextNode() || pos.computeOffsetInContainerNode() >= caretMaxOffset(pos.anchorNode())) {
        insertNodeAt(nodeToInsert, pos, editingState);
        if (editingState->isAborted())
            return;
        setEndingSelection(VisibleSelection(Position::inParentAfterNode(*nodeToInsert), TextAffinity::Downstream, endingSelection().isDirectional()));
    } else if (pos.anchorNode()->isTextNode()) {
        // Split a text node
        Text* textNode = toText(pos.anchorNode());
        splitTextNode(textNode, pos.computeOffsetInContainerNode());
        insertNodeBefore(nodeToInsert, textNode, editingState);
        if (editingState->isAborted())
            return;
        Position endingPosition = Position::firstPositionInNode(textNode);

        // Handle whitespace that occurs after the split
        document().updateStyleAndLayoutIgnorePendingStylesheets();
        // TODO(yosin) |isRenderedCharacter()| should be removed, and we should
        // use |VisiblePosition::characterAfter()|.
        if (!isRenderedCharacter(endingPosition)) {
            Position positionBeforeTextNode(Position::inParentBeforeNode(*textNode));
            // Clear out all whitespace and insert one non-breaking space
            deleteInsignificantTextDownstream(endingPosition);
            DCHECK(!textNode->layoutObject() || textNode->layoutObject()->style()->collapseWhiteSpace());
            // Deleting insignificant whitespace will remove textNode if it contains nothing but insignificant whitespace.
            if (textNode->inShadowIncludingDocument()) {
                insertTextIntoNode(textNode, 0, nonBreakingSpaceString());
            } else {
                Text* nbspNode = document().createTextNode(nonBreakingSpaceString());
                insertNodeAt(nbspNode, positionBeforeTextNode, editingState);
                if (editingState->isAborted())
                    return;
                endingPosition = Position::firstPositionInNode(nbspNode);
            }
        }

        setEndingSelection(VisibleSelection(endingPosition, TextAffinity::Downstream, endingSelection().isDirectional()));
    }

    // Handle the case where there is a typing style.

    EditingStyle* typingStyle = document().frame()->selection().typingStyle();

    if (typingStyle && !typingStyle->isEmpty()) {
        // Apply the typing style to the inserted line break, so that if the selection
        // leaves and then comes back, new input will have the right style.
        // FIXME: We shouldn't always apply the typing style to the line break here,
        // see <rdar://problem/5794462>.
        applyStyle(typingStyle, firstPositionInOrBeforeNode(nodeToInsert), lastPositionInOrAfterNode(nodeToInsert), editingState);
        if (editingState->isAborted())
            return;
        // Even though this applyStyle operates on a Range, it still sets an endingSelection().
        // It tries to set a VisibleSelection around the content it operated on. So, that VisibleSelection
        // will either (a) select the line break we inserted, or it will (b) be a caret just
        // before the line break (if the line break is at the end of a block it isn't selectable).
        // So, this next call sets the endingSelection() to a caret just after the line break
        // that we inserted, or just before it if it's at the end of a block.
        setEndingSelection(endingSelection().visibleEnd());
    }

    rebalanceWhitespace();
}

} // namespace blink
