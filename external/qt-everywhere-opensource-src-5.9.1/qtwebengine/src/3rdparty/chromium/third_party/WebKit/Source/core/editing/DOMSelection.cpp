/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/DOMSelection.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/dom/TreeScope.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "wtf/text/WTFString.h"

namespace blink {

static Node* selectionShadowAncestor(LocalFrame* frame) {
  Node* node = frame->selection().selection().base().anchorNode();
  if (!node)
    return 0;

  if (!node->isInShadowTree())
    return 0;

  return frame->document()->ancestorInThisScope(node);
}

DOMSelection::DOMSelection(const TreeScope* treeScope)
    : DOMWindowProperty(treeScope->rootNode().document().frame()),
      m_treeScope(treeScope) {}

void DOMSelection::clearTreeScope() {
  m_treeScope = nullptr;
}

bool DOMSelection::isAvailable() const {
  return frame() && frame()->selection().isAvailable();
}

const VisibleSelection& DOMSelection::visibleSelection() const {
  DCHECK(frame());
  return frame()->selection().selection();
}

static Position anchorPosition(const VisibleSelection& selection) {
  Position anchor =
      selection.isBaseFirst() ? selection.start() : selection.end();
  return anchor.parentAnchoredEquivalent();
}

static Position focusPosition(const VisibleSelection& selection) {
  Position focus =
      selection.isBaseFirst() ? selection.end() : selection.start();
  return focus.parentAnchoredEquivalent();
}

static Position basePosition(const VisibleSelection& selection) {
  return selection.base().parentAnchoredEquivalent();
}

static Position extentPosition(const VisibleSelection& selection) {
  return selection.extent().parentAnchoredEquivalent();
}

Node* DOMSelection::anchorNode() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedNode(anchorPosition(visibleSelection()));
}

int DOMSelection::anchorOffset() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedOffset(anchorPosition(visibleSelection()));
}

Node* DOMSelection::focusNode() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedNode(focusPosition(visibleSelection()));
}

int DOMSelection::focusOffset() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedOffset(focusPosition(visibleSelection()));
}

Node* DOMSelection::baseNode() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedNode(basePosition(visibleSelection()));
}

int DOMSelection::baseOffset() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedOffset(basePosition(visibleSelection()));
}

Node* DOMSelection::extentNode() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedNode(extentPosition(visibleSelection()));
}

int DOMSelection::extentOffset() const {
  if (!isAvailable())
    return 0;

  return shadowAdjustedOffset(extentPosition(visibleSelection()));
}

bool DOMSelection::isCollapsed() const {
  if (!isAvailable() || selectionShadowAncestor(frame()))
    return true;
  return !frame()->selection().isRange();
}

String DOMSelection::type() const {
  if (!isAvailable())
    return String();

  FrameSelection& selection = frame()->selection();

  // This is a WebKit DOM extension, incompatible with an IE extension
  // IE has this same attribute, but returns "none", "text" and "control"
  // http://msdn.microsoft.com/en-us/library/ms534692(VS.85).aspx
  if (selection.isNone())
    return "None";
  if (selection.isCaret())
    return "Caret";
  return "Range";
}

int DOMSelection::rangeCount() const {
  if (!isAvailable())
    return 0;
  return frame()->selection().isNone() ? 0 : 1;
}

void DOMSelection::collapse(Node* node,
                            int offset,
                            ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  if (!node) {
    UseCounter::count(frame(), UseCounter::SelectionCollapseNull);
    frame()->selection().clear();
    return;
  }

  if (offset < 0) {
    exceptionState.throwDOMException(
        IndexSizeError, String::number(offset) + " is not a valid offset.");
    return;
  }

  if (!isValidForPosition(node))
    return;
  Range::checkNodeWOffset(node, offset, exceptionState);
  if (exceptionState.hadException())
    return;

  frame()->selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(node, offset))
          .setIsDirectional(frame()->selection().isDirectional())
          .build());
}

void DOMSelection::collapseToEnd(ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  const VisibleSelection& selection = frame()->selection().selection();

  if (selection.isNone()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "there is no selection.");
    return;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term, we should change FrameSelection::setSelection to take a
  // parameter that does not require clean layout, so that modifying selection
  // no longer performs synchronous layout by itself.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  SelectionInDOMTree::Builder builder;
  builder.collapse(selection.end());
  frame()->selection().setSelection(createVisibleSelection(builder.build()));
}

void DOMSelection::collapseToStart(ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  const VisibleSelection& selection = frame()->selection().selection();

  if (selection.isNone()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "there is no selection.");
    return;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term, we should change FrameSelection::setSelection to take a
  // parameter that does not require clean layout, so that modifying selection
  // no longer performs synchronous layout by itself.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  SelectionInDOMTree::Builder builder;
  builder.collapse(selection.start());
  frame()->selection().setSelection(createVisibleSelection(builder.build()));
}

void DOMSelection::empty() {
  if (!isAvailable())
    return;
  frame()->selection().clear();
}

void DOMSelection::setBaseAndExtent(Node* baseNode,
                                    int baseOffset,
                                    Node* extentNode,
                                    int extentOffset,
                                    ExceptionState& exceptionState) {
  if (!isAvailable())
    return;

  if (baseOffset < 0) {
    exceptionState.throwDOMException(
        IndexSizeError,
        String::number(baseOffset) + " is not a valid base offset.");
    return;
  }

  if (extentOffset < 0) {
    exceptionState.throwDOMException(
        IndexSizeError,
        String::number(extentOffset) + " is not a valid extent offset.");
    return;
  }

  if (!baseNode || !extentNode)
    UseCounter::count(frame(), UseCounter::SelectionSetBaseAndExtentNull);

  if (!isValidForPosition(baseNode) || !isValidForPosition(extentNode))
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term, we should change FrameSelection::setSelection to take a
  // parameter that does not require clean layout, so that modifying selection
  // no longer performs synchronous layout by itself.
  // TODO(editing-dev): Once SVG USE element doesn't modifies DOM tree, we
  // should get rid of this update layout call.
  // See http://crbug.com/566281
  // See "svg/text/textpath-reference-crash.html"
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  frame()->selection().setSelection(
      SelectionInDOMTree::Builder()
          .setBaseAndExtentDeprecated(Position(baseNode, baseOffset),
                                      Position(extentNode, extentOffset))
          .setIsDirectional(true)
          .build());
}

void DOMSelection::modify(const String& alterString,
                          const String& directionString,
                          const String& granularityString) {
  if (!isAvailable())
    return;

  FrameSelection::EAlteration alter;
  if (equalIgnoringCase(alterString, "extend"))
    alter = FrameSelection::AlterationExtend;
  else if (equalIgnoringCase(alterString, "move"))
    alter = FrameSelection::AlterationMove;
  else
    return;

  SelectionDirection direction;
  if (equalIgnoringCase(directionString, "forward"))
    direction = DirectionForward;
  else if (equalIgnoringCase(directionString, "backward"))
    direction = DirectionBackward;
  else if (equalIgnoringCase(directionString, "left"))
    direction = DirectionLeft;
  else if (equalIgnoringCase(directionString, "right"))
    direction = DirectionRight;
  else
    return;

  TextGranularity granularity;
  if (equalIgnoringCase(granularityString, "character"))
    granularity = CharacterGranularity;
  else if (equalIgnoringCase(granularityString, "word"))
    granularity = WordGranularity;
  else if (equalIgnoringCase(granularityString, "sentence"))
    granularity = SentenceGranularity;
  else if (equalIgnoringCase(granularityString, "line"))
    granularity = LineGranularity;
  else if (equalIgnoringCase(granularityString, "paragraph"))
    granularity = ParagraphGranularity;
  else if (equalIgnoringCase(granularityString, "lineboundary"))
    granularity = LineBoundary;
  else if (equalIgnoringCase(granularityString, "sentenceboundary"))
    granularity = SentenceBoundary;
  else if (equalIgnoringCase(granularityString, "paragraphboundary"))
    granularity = ParagraphBoundary;
  else if (equalIgnoringCase(granularityString, "documentboundary"))
    granularity = DocumentBoundary;
  else
    return;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  frame()->selection().modify(alter, direction, granularity);
}

void DOMSelection::extend(Node* node,
                          int offset,
                          ExceptionState& exceptionState) {
  DCHECK(node);

  if (!isAvailable())
    return;

  if (offset < 0) {
    exceptionState.throwDOMException(
        IndexSizeError, String::number(offset) + " is not a valid offset.");
    return;
  }
  if (static_cast<unsigned>(offset) > node->lengthOfContents()) {
    exceptionState.throwDOMException(
        IndexSizeError,
        String::number(offset) + " is larger than the given node's length.");
    return;
  }

  if (!isValidForPosition(node))
    return;

  const Position& base = frame()->selection().base();
  if (base.isNull()) {
    // TODO(editing-dev): We should throw |InvalidStateError| if selection is
    // none to follow the spec.
    frame()->selection().setSelection(SelectionInDOMTree::Builder()
                                          .collapse(Position(node, offset))
                                          .setIsDirectional(true)
                                          .build());
    return;
  }
  frame()->selection().setSelection(SelectionInDOMTree::Builder()
                                        .collapse(base)
                                        .extend(Position(node, offset))
                                        .setIsDirectional(true)
                                        .build());
}

Range* DOMSelection::getRangeAt(int index, ExceptionState& exceptionState) {
  if (!isAvailable())
    return nullptr;

  if (index < 0 || index >= rangeCount()) {
    exceptionState.throwDOMException(
        IndexSizeError, String::number(index) + " is not a valid index.");
    return nullptr;
  }

  // If you're hitting this, you've added broken multi-range selection support
  DCHECK_EQ(rangeCount(), 1);

  Position anchor = anchorPosition(visibleSelection());
  if (!anchor.anchorNode()->isInShadowTree())
    return frame()->selection().firstRange();

  Node* node = shadowAdjustedNode(anchor);
  if (!node)  // crbug.com/595100
    return nullptr;
  if (!visibleSelection().isBaseFirst())
    return Range::create(*anchor.document(), focusNode(), focusOffset(), node,
                         anchorOffset());
  return Range::create(*anchor.document(), node, anchorOffset(), focusNode(),
                       focusOffset());
}

void DOMSelection::removeAllRanges() {
  if (!isAvailable())
    return;
  frame()->selection().clear();
}

void DOMSelection::addRange(Range* newRange) {
  DCHECK(newRange);

  if (!isAvailable())
    return;

  if (newRange->ownerDocument() != frame()->document())
    return;

  if (!newRange->isConnected()) {
    addConsoleError("The given range isn't in document.");
    return;
  }

  FrameSelection& selection = frame()->selection();

  if (newRange->ownerDocument() != selection.document()) {
    // "editing/selection/selection-in-iframe-removed-crash.html" goes here.
    return;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term, we should change FrameSelection::setSelection to take a
  // parameter that does not require clean layout, so that modifying selection
  // no longer performs synchronous layout by itself.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  if (selection.isNone()) {
    selection.setSelectedRange(EphemeralRange(newRange), VP_DEFAULT_AFFINITY);
    return;
  }

  Range* originalRange = selection.firstRange();

  if (originalRange->startContainer()->document() !=
      newRange->startContainer()->document()) {
    addConsoleError(
        "The given range does not belong to the current selection's document.");
    return;
  }
  if (originalRange->startContainer()->treeScope() !=
      newRange->startContainer()->treeScope()) {
    addConsoleError(
        "The given range and the current selection belong to two different "
        "document fragments.");
    return;
  }

  if (originalRange->compareBoundaryPoints(Range::kStartToEnd, newRange,
                                           ASSERT_NO_EXCEPTION) < 0 ||
      newRange->compareBoundaryPoints(Range::kStartToEnd, originalRange,
                                      ASSERT_NO_EXCEPTION) < 0) {
    addConsoleError("Discontiguous selection is not supported.");
    return;
  }

  // FIXME: "Merge the ranges if they intersect" is Blink-specific behavior;
  // other browsers supporting discontiguous selection (obviously) keep each
  // Range added and return it in getRangeAt(). But it's unclear if we can
  // really do the same, since we don't support discontiguous selection. Further
  // discussions at
  // <https://code.google.com/p/chromium/issues/detail?id=353069>.

  Range* start = originalRange->compareBoundaryPoints(
                     Range::kStartToStart, newRange, ASSERT_NO_EXCEPTION) < 0
                     ? originalRange
                     : newRange;
  Range* end = originalRange->compareBoundaryPoints(Range::kEndToEnd, newRange,
                                                    ASSERT_NO_EXCEPTION) < 0
                   ? newRange
                   : originalRange;
  const EphemeralRange merged =
      EphemeralRange(start->startPosition(), end->endPosition());
  TextAffinity affinity = selection.selection().affinity();
  selection.setSelectedRange(merged, affinity);
}

void DOMSelection::deleteFromDocument() {
  if (!isAvailable())
    return;

  FrameSelection& selection = frame()->selection();

  if (selection.isNone())
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // |VisibleSelection::toNormalizedEphemeralRange| requires clean layout.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  Range* selectedRange =
      createRange(selection.selection().toNormalizedEphemeralRange());
  if (!selectedRange)
    return;

  selectedRange->deleteContents(ASSERT_NO_EXCEPTION);

  setBaseAndExtent(selectedRange->startContainer(),
                   selectedRange->startOffset(),
                   selectedRange->startContainer(),
                   selectedRange->startOffset(), ASSERT_NO_EXCEPTION);
}

bool DOMSelection::containsNode(const Node* n, bool allowPartial) const {
  DCHECK(n);

  if (!isAvailable())
    return false;

  FrameSelection& selection = frame()->selection();

  if (frame()->document() != n->document() || selection.isNone())
    return false;

  unsigned nodeIndex = n->nodeIndex();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // |VisibleSelection::toNormalizedEphemeralRange| requires clean layout.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  const EphemeralRange selectedRange =
      selection.selection().toNormalizedEphemeralRange();

  ContainerNode* parentNode = n->parentNode();
  if (!parentNode)
    return false;

  const Position startPosition =
      selectedRange.startPosition().toOffsetInAnchor();
  const Position endPosition = selectedRange.endPosition().toOffsetInAnchor();
  TrackExceptionState exceptionState;
  bool nodeFullySelected =
      Range::compareBoundaryPoints(
          parentNode, nodeIndex, startPosition.computeContainerNode(),
          startPosition.offsetInContainerNode(), exceptionState) >= 0 &&
      !exceptionState.hadException() &&
      Range::compareBoundaryPoints(
          parentNode, nodeIndex + 1, endPosition.computeContainerNode(),
          endPosition.offsetInContainerNode(), exceptionState) <= 0 &&
      !exceptionState.hadException();
  if (exceptionState.hadException())
    return false;
  if (nodeFullySelected)
    return true;

  bool nodeFullyUnselected =
      (Range::compareBoundaryPoints(
           parentNode, nodeIndex, endPosition.computeContainerNode(),
           endPosition.offsetInContainerNode(), exceptionState) > 0 &&
       !exceptionState.hadException()) ||
      (Range::compareBoundaryPoints(
           parentNode, nodeIndex + 1, startPosition.computeContainerNode(),
           startPosition.offsetInContainerNode(), exceptionState) < 0 &&
       !exceptionState.hadException());
  DCHECK(!exceptionState.hadException());
  if (nodeFullyUnselected)
    return false;

  return allowPartial || n->isTextNode();
}

void DOMSelection::selectAllChildren(Node* n, ExceptionState& exceptionState) {
  DCHECK(n);

  // This doesn't (and shouldn't) select text node characters.
  setBaseAndExtent(n, 0, n, n->countChildren(), exceptionState);
}

String DOMSelection::toString() {
  if (!isAvailable())
    return String();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame()->document()->lifecycle());

  const EphemeralRange range =
      frame()->selection().selection().toNormalizedEphemeralRange();
  return plainText(range, TextIteratorForSelectionToString);
}

Node* DOMSelection::shadowAdjustedNode(const Position& position) const {
  if (position.isNull())
    return 0;

  Node* containerNode = position.computeContainerNode();
  Node* adjustedNode = m_treeScope->ancestorInThisScope(containerNode);

  if (!adjustedNode)
    return 0;

  if (containerNode == adjustedNode)
    return containerNode;

  DCHECK(!adjustedNode->isShadowRoot()) << adjustedNode;
  return adjustedNode->parentOrShadowHostNode();
}

int DOMSelection::shadowAdjustedOffset(const Position& position) const {
  if (position.isNull())
    return 0;

  Node* containerNode = position.computeContainerNode();
  Node* adjustedNode = m_treeScope->ancestorInThisScope(containerNode);

  if (!adjustedNode)
    return 0;

  if (containerNode == adjustedNode)
    return position.computeOffsetInContainerNode();

  return adjustedNode->nodeIndex();
}

bool DOMSelection::isValidForPosition(Node* node) const {
  DCHECK(frame());
  if (!node)
    return true;
  return node->document() == frame()->document() && node->isConnected();
}

void DOMSelection::addConsoleError(const String& message) {
  if (m_treeScope)
    m_treeScope->document().addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

DEFINE_TRACE(DOMSelection) {
  visitor->trace(m_treeScope);
  DOMWindowProperty::trace(visitor);
}

}  // namespace blink
