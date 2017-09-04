/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/commands/ApplyBlockElementCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLElement.h"
#include "core/style/ComputedStyle.h"

namespace blink {

using namespace HTMLNames;

ApplyBlockElementCommand::ApplyBlockElementCommand(
    Document& document,
    const QualifiedName& tagName,
    const AtomicString& inlineStyle)
    : CompositeEditCommand(document),
      m_tagName(tagName),
      m_inlineStyle(inlineStyle) {}

ApplyBlockElementCommand::ApplyBlockElementCommand(Document& document,
                                                   const QualifiedName& tagName)
    : CompositeEditCommand(document), m_tagName(tagName) {}

void ApplyBlockElementCommand::doApply(EditingState* editingState) {
  if (!endingSelection().rootEditableElement())
    return;

  // ApplyBlockElementCommands are only created directly by editor commands'
  // execution, which updates layout before entering doApply().
  DCHECK(!document().needsLayoutTreeUpdate());

  VisiblePosition visibleEnd = endingSelection().visibleEnd();
  VisiblePosition visibleStart = endingSelection().visibleStart();
  if (visibleStart.isNull() || visibleStart.isOrphan() || visibleEnd.isNull() ||
      visibleEnd.isOrphan())
    return;

  // When a selection ends at the start of a paragraph, we rarely paint
  // the selection gap before that paragraph, because there often is no gap.
  // In a case like this, it's not obvious to the user that the selection
  // ends "inside" that paragraph, so it would be confusing if Indent/Outdent
  // operated on that paragraph.
  // FIXME: We paint the gap before some paragraphs that are indented with left
  // margin/padding, but not others.  We should make the gap painting more
  // consistent and then use a left margin/padding rule here.
  if (visibleEnd.deepEquivalent() != visibleStart.deepEquivalent() &&
      isStartOfParagraph(visibleEnd)) {
    const Position& newEnd =
        previousPositionOf(visibleEnd, CannotCrossEditingBoundary)
            .deepEquivalent();
    SelectionInDOMTree::Builder builder;
    builder.collapse(visibleStart.toPositionWithAffinity());
    if (newEnd.isNotNull())
      builder.extend(newEnd);
    builder.setIsDirectional(endingSelection().isDirectional());
    const SelectionInDOMTree& newSelection = builder.build();
    if (newSelection.isNone())
      return;
    setEndingSelection(newSelection);
  }

  VisibleSelection selection =
      selectionForParagraphIteration(endingSelection());
  VisiblePosition startOfSelection = selection.visibleStart();
  VisiblePosition endOfSelection = selection.visibleEnd();
  DCHECK(!startOfSelection.isNull());
  DCHECK(!endOfSelection.isNull());
  ContainerNode* startScope = nullptr;
  int startIndex = indexForVisiblePosition(startOfSelection, startScope);
  ContainerNode* endScope = nullptr;
  int endIndex = indexForVisiblePosition(endOfSelection, endScope);

  formatSelection(startOfSelection, endOfSelection, editingState);
  if (editingState->isAborted())
    return;

  document().updateStyleAndLayoutIgnorePendingStylesheets();

  DCHECK_EQ(startScope, endScope);
  DCHECK_GE(startIndex, 0);
  DCHECK_LE(startIndex, endIndex);
  if (startScope == endScope && startIndex >= 0 && startIndex <= endIndex) {
    VisiblePosition start(visiblePositionForIndex(startIndex, startScope));
    VisiblePosition end(visiblePositionForIndex(endIndex, endScope));
    if (start.isNotNull() && end.isNotNull()) {
      setEndingSelection(
          SelectionInDOMTree::Builder()
              .collapse(start.toPositionWithAffinity())
              .extend(end.deepEquivalent())
              .setIsDirectional(endingSelection().isDirectional())
              .build());
    }
  }
}

static bool isAtUnsplittableElement(const Position& pos) {
  Node* node = pos.anchorNode();
  return node == rootEditableElementOf(pos) ||
         node == enclosingNodeOfType(pos, &isTableCell);
}

void ApplyBlockElementCommand::formatSelection(
    const VisiblePosition& startOfSelection,
    const VisiblePosition& endOfSelection,
    EditingState* editingState) {
  // Special case empty unsplittable elements because there's nothing to split
  // and there's nothing to move.
  const Position& caretPosition =
      mostForwardCaretPosition(startOfSelection.deepEquivalent());
  if (isAtUnsplittableElement(caretPosition)) {
    HTMLElement* blockquote = createBlockElement();
    insertNodeAt(blockquote, caretPosition, editingState);
    if (editingState->isAborted())
      return;
    HTMLBRElement* placeholder = HTMLBRElement::create(document());
    appendNode(placeholder, blockquote, editingState);
    if (editingState->isAborted())
      return;
    setEndingSelection(SelectionInDOMTree::Builder()
                           .collapse(Position::beforeNode(placeholder))
                           .setIsDirectional(endingSelection().isDirectional())
                           .build());
    return;
  }

  HTMLElement* blockquoteForNextIndent = nullptr;
  VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
  const VisiblePosition& visibleEndOfLastParagraph =
      endOfParagraph(endOfSelection);
  const Position& endOfNextLastParagraph =
      endOfParagraph(nextPositionOf(visibleEndOfLastParagraph))
          .deepEquivalent();
  Position endOfLastParagraph = visibleEndOfLastParagraph.deepEquivalent();

  bool atEnd = false;
  while (endOfCurrentParagraph.deepEquivalent() != endOfNextLastParagraph &&
         !atEnd) {
    if (endOfCurrentParagraph.deepEquivalent() == endOfLastParagraph)
      atEnd = true;

    Position start, end;
    rangeForParagraphSplittingTextNodesIfNeeded(endOfCurrentParagraph,
                                                endOfLastParagraph, start, end);
    endOfCurrentParagraph = createVisiblePosition(end);

    Node* enclosingCell = enclosingNodeOfType(start, &isTableCell);
    PositionWithAffinity endOfNextParagraph =
        endOfNextParagrahSplittingTextNodesIfNeeded(
            endOfCurrentParagraph, endOfLastParagraph, start, end)
            .toPositionWithAffinity();

    formatRange(start, end, endOfLastParagraph, blockquoteForNextIndent,
                editingState);
    if (editingState->isAborted())
      return;

    // Don't put the next paragraph in the blockquote we just created for this
    // paragraph unless the next paragraph is in the same cell.
    if (enclosingCell &&
        enclosingCell !=
            enclosingNodeOfType(endOfNextParagraph.position(), &isTableCell))
      blockquoteForNextIndent = nullptr;

    // indentIntoBlockquote could move more than one paragraph if the paragraph
    // is in a list item or a table. As a result,
    // |endOfNextLastParagraph| could refer to a position no longer in the
    // document.
    if (endOfNextLastParagraph.isNotNull() &&
        !endOfNextLastParagraph.isConnected())
      break;
    // Sanity check: Make sure our moveParagraph calls didn't remove
    // endOfNextParagraph.anchorNode() If somehow, e.g. mutation
    // event handler, we did, return to prevent crashes.
    if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.isConnected())
      return;

    document().updateStyleAndLayoutIgnorePendingStylesheets();
    endOfCurrentParagraph = createVisiblePosition(endOfNextParagraph);
  }
}

static bool isNewLineAtPosition(const Position& position) {
  Node* textNode = position.computeContainerNode();
  int offset = position.offsetInContainerNode();
  if (!textNode || !textNode->isTextNode() || offset < 0 ||
      offset >= textNode->maxCharacterOffset())
    return false;

  TrackExceptionState exceptionState;
  String textAtPosition =
      toText(textNode)->substringData(offset, 1, exceptionState);
  if (exceptionState.hadException())
    return false;

  return textAtPosition[0] == '\n';
}

static const ComputedStyle* computedStyleOfEnclosingTextNode(
    const Position& position) {
  if (!position.isOffsetInAnchor() || !position.computeContainerNode() ||
      !position.computeContainerNode()->isTextNode())
    return 0;
  return position.computeContainerNode()->computedStyle();
}

void ApplyBlockElementCommand::rangeForParagraphSplittingTextNodesIfNeeded(
    const VisiblePosition& endOfCurrentParagraph,
    Position& endOfLastParagraph,
    Position& start,
    Position& end) {
  start = startOfParagraph(endOfCurrentParagraph).deepEquivalent();
  end = endOfCurrentParagraph.deepEquivalent();

  bool isStartAndEndOnSameNode = false;
  if (const ComputedStyle* startStyle =
          computedStyleOfEnclosingTextNode(start)) {
    isStartAndEndOnSameNode =
        computedStyleOfEnclosingTextNode(end) &&
        start.computeContainerNode() == end.computeContainerNode();
    bool isStartAndEndOfLastParagraphOnSameNode =
        computedStyleOfEnclosingTextNode(endOfLastParagraph) &&
        start.computeContainerNode() ==
            endOfLastParagraph.computeContainerNode();

    // Avoid obtanining the start of next paragraph for start
    // TODO(yosin) We should use |PositionMoveType::CodePoint| for
    // |previousPositionOf()|.
    if (startStyle->preserveNewline() && isNewLineAtPosition(start) &&
        !isNewLineAtPosition(
            previousPositionOf(start, PositionMoveType::CodeUnit)) &&
        start.offsetInContainerNode() > 0)
      start = startOfParagraph(createVisiblePosition(previousPositionOf(
                                   end, PositionMoveType::CodeUnit)))
                  .deepEquivalent();

    // If start is in the middle of a text node, split.
    if (!startStyle->collapseWhiteSpace() &&
        start.offsetInContainerNode() > 0) {
      int startOffset = start.offsetInContainerNode();
      Text* startText = toText(start.computeContainerNode());
      splitTextNode(startText, startOffset);
      document().updateStyleAndLayoutTree();

      start = Position::firstPositionInNode(startText);
      if (isStartAndEndOnSameNode) {
        DCHECK_GE(end.offsetInContainerNode(), startOffset);
        end = Position(startText, end.offsetInContainerNode() - startOffset);
      }
      if (isStartAndEndOfLastParagraphOnSameNode) {
        DCHECK_GE(endOfLastParagraph.offsetInContainerNode(), startOffset);
        endOfLastParagraph =
            Position(startText,
                     endOfLastParagraph.offsetInContainerNode() - startOffset);
      }
    }
  }

  if (const ComputedStyle* endStyle = computedStyleOfEnclosingTextNode(end)) {
    bool isEndAndEndOfLastParagraphOnSameNode =
        computedStyleOfEnclosingTextNode(endOfLastParagraph) &&
        end.anchorNode() == endOfLastParagraph.anchorNode();
    // Include \n at the end of line if we're at an empty paragraph
    if (endStyle->preserveNewline() && start == end &&
        end.offsetInContainerNode() <
            end.computeContainerNode()->maxCharacterOffset()) {
      int endOffset = end.offsetInContainerNode();
      // TODO(yosin) We should use |PositionMoveType::CodePoint| for
      // |previousPositionOf()|.
      if (!isNewLineAtPosition(
              previousPositionOf(end, PositionMoveType::CodeUnit)) &&
          isNewLineAtPosition(end))
        end = Position(end.computeContainerNode(), endOffset + 1);
      if (isEndAndEndOfLastParagraphOnSameNode &&
          end.offsetInContainerNode() >=
              endOfLastParagraph.offsetInContainerNode())
        endOfLastParagraph = end;
    }

    // If end is in the middle of a text node, split.
    if (endStyle->userModify() != READ_ONLY &&
        !endStyle->collapseWhiteSpace() && end.offsetInContainerNode() &&
        end.offsetInContainerNode() <
            end.computeContainerNode()->maxCharacterOffset()) {
      Text* endContainer = toText(end.computeContainerNode());
      splitTextNode(endContainer, end.offsetInContainerNode());
      document().updateStyleAndLayoutTree();

      if (isStartAndEndOnSameNode)
        start = firstPositionInOrBeforeNode(endContainer->previousSibling());
      if (isEndAndEndOfLastParagraphOnSameNode) {
        if (endOfLastParagraph.offsetInContainerNode() ==
            end.offsetInContainerNode())
          endOfLastParagraph =
              lastPositionInOrAfterNode(endContainer->previousSibling());
        else
          endOfLastParagraph = Position(
              endContainer, endOfLastParagraph.offsetInContainerNode() -
                                end.offsetInContainerNode());
      }
      end = Position::lastPositionInNode(endContainer->previousSibling());
    }
  }
}

VisiblePosition
ApplyBlockElementCommand::endOfNextParagrahSplittingTextNodesIfNeeded(
    VisiblePosition& endOfCurrentParagraph,
    Position& endOfLastParagraph,
    Position& start,
    Position& end) {
  const VisiblePosition& endOfNextParagraph =
      endOfParagraph(nextPositionOf(endOfCurrentParagraph));
  const Position& endOfNextParagraphPosition =
      endOfNextParagraph.deepEquivalent();
  const ComputedStyle* style =
      computedStyleOfEnclosingTextNode(endOfNextParagraphPosition);
  if (!style)
    return endOfNextParagraph;

  Text* const endOfNextParagraphText =
      toText(endOfNextParagraphPosition.computeContainerNode());
  if (!style->preserveNewline() ||
      !endOfNextParagraphPosition.offsetInContainerNode() ||
      !isNewLineAtPosition(
          Position::firstPositionInNode(endOfNextParagraphText)))
    return endOfNextParagraph;

  // \n at the beginning of the text node immediately following the current
  // paragraph is trimmed by moveParagraphWithClones. If endOfNextParagraph was
  // pointing at this same text node, endOfNextParagraph will be shifted by one
  // paragraph. Avoid this by splitting "\n"
  splitTextNode(endOfNextParagraphText, 1);
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  Text* const previousText =
      endOfNextParagraphText->previousSibling() &&
              endOfNextParagraphText->previousSibling()->isTextNode()
          ? toText(endOfNextParagraphText->previousSibling())
          : nullptr;
  if (endOfNextParagraphText == start.computeContainerNode() && previousText) {
    DCHECK_LT(start.offsetInContainerNode(),
              endOfNextParagraphPosition.offsetInContainerNode());
    start = Position(previousText, start.offsetInContainerNode());
  }
  if (endOfNextParagraphText == end.computeContainerNode() && previousText) {
    DCHECK_LT(end.offsetInContainerNode(),
              endOfNextParagraphPosition.offsetInContainerNode());
    end = Position(previousText, end.offsetInContainerNode());
  }
  if (endOfNextParagraphText == endOfLastParagraph.computeContainerNode()) {
    if (endOfLastParagraph.offsetInContainerNode() <
        endOfNextParagraphPosition.offsetInContainerNode()) {
      // We can only fix endOfLastParagraph if the previous node was still text
      // and hasn't been modified by script.
      if (previousText &&
          static_cast<unsigned>(endOfLastParagraph.offsetInContainerNode()) <=
              previousText->length()) {
        endOfLastParagraph =
            Position(previousText, endOfLastParagraph.offsetInContainerNode());
      }
    } else {
      endOfLastParagraph =
          Position(endOfNextParagraphText,
                   endOfLastParagraph.offsetInContainerNode() - 1);
    }
  }

  return createVisiblePosition(
      Position(endOfNextParagraphText,
               endOfNextParagraphPosition.offsetInContainerNode() - 1));
}

HTMLElement* ApplyBlockElementCommand::createBlockElement() const {
  HTMLElement* element = createHTMLElement(document(), m_tagName);
  if (m_inlineStyle.length())
    element->setAttribute(styleAttr, m_inlineStyle);
  return element;
}

}  // namespace blink
