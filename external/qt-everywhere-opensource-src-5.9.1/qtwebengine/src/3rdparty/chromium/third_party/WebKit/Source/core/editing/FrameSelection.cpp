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

#include "core/editing/FrameSelection.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/CharacterData.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/CaretBase.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameCaret.h"
#include "core/editing/GranularityStrategy.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PendingSelection.h"
#include "core/editing/SelectionController.h"
#include "core/editing/SelectionEditor.h"
#include "core/editing/SelectionModifier.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/EditorClient.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/SpatialNavigation.h"
#include "core/paint/PaintLayer.h"
#include "platform/SecureTextInput.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/text/UnicodeUtilities.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include <stdio.h>

#define EDIT_DEBUG 0

namespace blink {

using namespace HTMLNames;

static inline bool shouldAlwaysUseDirectionalSelection(LocalFrame* frame) {
  return frame->editor().behavior().shouldConsiderSelectionAsDirectional();
}

FrameSelection::FrameSelection(LocalFrame* frame)
    : m_frame(frame),
      m_pendingSelection(PendingSelection::create(*this)),
      m_selectionEditor(SelectionEditor::create(*this)),
      m_granularity(CharacterGranularity),
      m_xPosForVerticalArrowNavigation(NoXPosForVerticalArrowNavigation()),
      m_focused(frame->page() &&
                frame->page()->focusController().focusedFrame() == frame),
      m_frameCaret(new FrameCaret(frame, *m_selectionEditor)) {
  DCHECK(frame);
}

FrameSelection::~FrameSelection() {}

const Document& FrameSelection::document() const {
  DCHECK(m_document);
  return *m_document;
}

Document& FrameSelection::document() {
  DCHECK(m_document);
  return *m_document;
}

// TODO(yosin): To avoid undefined symbols in clang, we explicitly
// have specialized version of |FrameSelection::visibleSelection<Strategy>|
// before |FrameSelection::selection()| which refers this.
template <>
const VisibleSelection& FrameSelection::visibleSelection<EditingStrategy>()
    const {
  return m_selectionEditor->visibleSelection<EditingStrategy>();
}

template <>
const VisibleSelectionInFlatTree&
FrameSelection::visibleSelection<EditingInFlatTreeStrategy>() const {
  return m_selectionEditor->visibleSelection<EditingInFlatTreeStrategy>();
}

Element* FrameSelection::rootEditableElementOrDocumentElement() const {
  Element* selectionRoot = selection().rootEditableElement();
  return selectionRoot ? selectionRoot : document().documentElement();
}

ContainerNode* FrameSelection::rootEditableElementOrTreeScopeRootNode() const {
  Element* selectionRoot = selection().rootEditableElement();
  if (selectionRoot)
    return selectionRoot;

  Node* node = selection().base().computeContainerNode();
  return node ? &node->treeScope().rootNode() : 0;
}

const VisibleSelection& FrameSelection::selection() const {
  return visibleSelection<EditingStrategy>();
}

const VisibleSelectionInFlatTree& FrameSelection::selectionInFlatTree() const {
  return visibleSelection<EditingInFlatTreeStrategy>();
}

void FrameSelection::moveCaretSelection(const IntPoint& point) {
  DCHECK(!document().needsLayoutTreeUpdate());

  Element* const editable = rootEditableElement();
  if (!editable)
    return;

  const VisiblePosition position =
      visiblePositionForContentsPoint(point, frame());
  SelectionInDOMTree::Builder builder;
  builder.setIsDirectional(selection().isDirectional());
  if (position.isNotNull())
    builder.collapse(position.toPositionWithAffinity());
  setSelection(builder.build(), CloseTyping | ClearTypingStyle | UserTriggered);
}

template <typename Strategy>
void FrameSelection::setSelectionAlgorithm(
    const VisibleSelectionTemplate<Strategy>& newSelection,
    SetSelectionOptions options,
    CursorAlignOnScroll align,
    TextGranularity granularity) {
  DCHECK(isAvailable());
  DCHECK(newSelection.isValidFor(document()));
  const Document& currentDocument = document();
  if (m_granularityStrategy &&
      (options & FrameSelection::DoNotClearStrategy) == 0)
    m_granularityStrategy->Clear();
  bool closeTyping = options & CloseTyping;
  bool shouldClearTypingStyle = options & ClearTypingStyle;
  EUserTriggered userTriggered = selectionOptionsToUserTriggered(options);

  // TODO(editing-dev): We should rename variable |s| to another name to avoid
  // using one letter variable name.
  VisibleSelectionTemplate<Strategy> s = newSelection;
  if (shouldAlwaysUseDirectionalSelection(m_frame))
    s.setIsDirectional(true);

  m_granularity = granularity;

  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (closeTyping)
    TypingCommand::closeTyping(m_frame);

  if (shouldClearTypingStyle)
    clearTypingStyle();

  if (m_selectionEditor->visibleSelection<Strategy>() == s) {
    // Even if selection was not changed, selection offsets may have been
    // changed.
    m_frame->inputMethodController().cancelCompositionIfSelectionIsInvalid();
    notifyLayoutObjectOfSelectionChange(userTriggered);
    return;
  }

  const VisibleSelectionTemplate<Strategy> oldSelection =
      visibleSelection<Strategy>();
  const Position& oldSelectionStart = selection().start();

  m_selectionEditor->setVisibleSelection(s, options);
  m_frameCaret->setCaretRectNeedsUpdate();

  if (!s.isNone() && !(options & DoNotSetFocus)) {
    setFocusedNodeIfNeeded();
    // |setFocusedNodeIfNeeded()| dispatches sync events "FocusOut" and
    // "FocusIn", |m_frame| may associate to another document.
    if (!isAvailable() || document() != currentDocument) {
      // Once we get test case to reach here, we should change this
      // if-statement to |DCHECK()|.
      NOTREACHED();
      return;
    }
  }

  if (!(options & DoNotUpdateAppearance)) {
    // Hits in
    // compositing/overflow/do-not-paint-outline-into-composited-scrolling-contents.html
    DisableCompositingQueryAsserts disabler;
    m_frameCaret->stopCaretBlinkTimer();
    updateAppearance();
  }

  // Always clear the x position used for vertical arrow navigation.
  // It will be restored by the vertical arrow navigation code if necessary.
  m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation();
  // This may dispatch a synchronous focus-related events.
  selectFrameElementInParentIfFullySelected();
  if (!isAvailable() || document() != currentDocument) {
    // editing/selection/selectallchildren-crash.html and
    // editing/selection/longpress-selection-in-iframe-removed-crash.html
    // reach here.
    return;
  }
  notifyLayoutObjectOfSelectionChange(userTriggered);
  // If the selections are same in the DOM tree but not in the flat tree,
  // don't fire events. For example, if the selection crosses shadow tree
  // boundary, selection for the DOM tree is shrunk while that for the
  // flat tree is not. Additionally, this case occurs in some edge cases.
  // See also: editing/pasteboard/4076267-3.html
  if (oldSelection == m_selectionEditor->visibleSelection<Strategy>()) {
    m_frame->inputMethodController().cancelCompositionIfSelectionIsInvalid();
    return;
  }
  m_frame->editor().respondToChangedSelection(oldSelectionStart, options);
  if (userTriggered == UserTriggered) {
    ScrollAlignment alignment;

    if (m_frame->editor().behavior().shouldCenterAlignWhenSelectionIsRevealed())
      alignment = (align == CursorAlignOnScroll::Always)
                      ? ScrollAlignment::alignCenterAlways
                      : ScrollAlignment::alignCenterIfNeeded;
    else
      alignment = (align == CursorAlignOnScroll::Always)
                      ? ScrollAlignment::alignTopAlways
                      : ScrollAlignment::alignToEdgeIfNeeded;

    revealSelection(alignment, RevealExtent);
  }

  notifyAccessibilityForSelectionChange();
  notifyCompositorForSelectionChange();
  notifyEventHandlerForSelectionChange();
  m_frame->localDOMWindow()->enqueueDocumentEvent(
      Event::create(EventTypeNames::selectionchange));
}

// TODO(yosin): We will make |selectionInDOMTree| version of |SetSelection()|
// as primary function instead of wrapper.
void FrameSelection::setSelection(const SelectionInDOMTree& newSelection,
                                  SetSelectionOptions options,
                                  CursorAlignOnScroll align,
                                  TextGranularity granularity) {
  if (!newSelection.isNone()) {
    // TODO(editing-dev): The use of
    // updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    newSelection.base()
        .document()
        ->updateStyleAndLayoutIgnorePendingStylesheets();
  }
  setSelection(createVisibleSelection(newSelection), options, align,
               granularity);
}

// TODO(yosin): We will make |selectionInFlatTree| version of |SetSelection()|
// as primary function instead of wrapper.
void FrameSelection::setSelection(const SelectionInFlatTree& newSelection,
                                  SetSelectionOptions options,
                                  CursorAlignOnScroll align,
                                  TextGranularity granularity) {
  if (!newSelection.isNone()) {
    // TODO(editing-dev): The use of
    // updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    newSelection.base()
        .document()
        ->updateStyleAndLayoutIgnorePendingStylesheets();
  }
  setSelection(createVisibleSelection(newSelection), options, align,
               granularity);
}

void FrameSelection::setSelection(const VisibleSelection& newSelection,
                                  SetSelectionOptions options,
                                  CursorAlignOnScroll align,
                                  TextGranularity granularity) {
  setSelectionAlgorithm<EditingStrategy>(newSelection, options, align,
                                         granularity);
}

void FrameSelection::setSelection(
    const VisibleSelectionInFlatTree& newSelection,
    SetSelectionOptions options,
    CursorAlignOnScroll align,
    TextGranularity granularity) {
  setSelectionAlgorithm<EditingInFlatTreeStrategy>(newSelection, options, align,
                                                   granularity);
}

static bool removingNodeRemovesPosition(Node& node, const Position& position) {
  if (!position.anchorNode())
    return false;

  if (position.anchorNode() == node)
    return true;

  if (!node.isElementNode())
    return false;

  Element& element = toElement(node);
  return element.isShadowIncludingInclusiveAncestorOf(position.anchorNode());
}

static Position computePositionForChildrenRemoval(const Position& position,
                                                  ContainerNode& container) {
  Node* node = position.computeContainerNode();
  if (container.containsIncludingHostElements(*node))
    return Position::firstPositionInNode(&container);
  return position;
}

void FrameSelection::nodeChildrenWillBeRemoved(ContainerNode& container) {
  if (isNone() || !container.inActiveDocument())
    return;
  const Position& oldStart = selection().start();
  const Position& newStart =
      computePositionForChildrenRemoval(oldStart, container);
  const Position& oldEnd = selection().end();
  const Position& newEnd = computePositionForChildrenRemoval(oldEnd, container);
  const Position& oldBase = selection().base();
  const Position& newBase =
      computePositionForChildrenRemoval(oldBase, container);
  const Position& oldExtent = selection().extent();
  const Position& newExtent =
      computePositionForChildrenRemoval(oldExtent, container);
  if (newStart == oldStart && newEnd == oldEnd && newBase == oldBase &&
      newExtent == oldExtent)
    return;
  if (selection().isBaseFirst())
    m_selectionEditor->setWithoutValidation(newStart, newEnd);
  else
    m_selectionEditor->setWithoutValidation(newEnd, newStart);
  m_frameCaret->setCaretRectNeedsUpdate();
  if (document().isRunningExecCommand())
    return;
  TypingCommand::closeTyping(m_frame);
}

void FrameSelection::nodeWillBeRemoved(Node& node) {
  // There can't be a selection inside a fragment, so if a fragment's node is
  // being removed, the selection in the document that created the fragment
  // needs no adjustment.
  if (isNone() || !node.inActiveDocument())
    return;

  respondToNodeModification(
      node, removingNodeRemovesPosition(node, selection().base()),
      removingNodeRemovesPosition(node, selection().extent()),
      removingNodeRemovesPosition(node, selection().start()),
      removingNodeRemovesPosition(node, selection().end()));

  m_frameCaret->nodeWillBeRemoved(node);
}

static SelectionState selectionStateOf(const Node& node) {
  const LayoutObject* layoutObject = node.layoutObject();
  if (!layoutObject)
    return SelectionNone;
  return layoutObject->getSelectionState();
}

void FrameSelection::respondToNodeModification(Node& node,
                                               bool baseRemoved,
                                               bool extentRemoved,
                                               bool startRemoved,
                                               bool endRemoved) {
  DCHECK(node.document().isActive()) << node;

  bool clearLayoutTreeSelection = false;
  bool clearDOMTreeSelection = false;

  if (startRemoved || endRemoved) {
    Position start = selection().start();
    Position end = selection().end();
    if (startRemoved)
      updatePositionForNodeRemoval(start, node);
    if (endRemoved)
      updatePositionForNodeRemoval(end, node);

    if (Position::commonAncestorTreeScope(start, end) && start.isNotNull() &&
        end.isNotNull()) {
      if (selection().isBaseFirst())
        m_selectionEditor->setWithoutValidation(start, end);
      else
        m_selectionEditor->setWithoutValidation(end, start);
    } else {
      clearDOMTreeSelection = true;
    }

    clearLayoutTreeSelection = true;
  } else if (baseRemoved || extentRemoved) {
    // The base and/or extent are about to be removed, but the start and end
    // aren't. Change the base and extent to the start and end, but don't
    // re-validate the selection, since doing so could move the start and end
    // into the node that is about to be removed.
    if (selection().isBaseFirst())
      m_selectionEditor->setWithoutValidation(selection().start(),
                                              selection().end());
    else
      m_selectionEditor->setWithoutValidation(selection().end(),
                                              selection().start());
  } else if (selectionStateOf(node) != SelectionNone) {
    // When node to be removed is part of selection, we invalidate
    // selection to paint again.
    // TODO(yosin): We should paint changed area only rather than whole
    // selected range.
    clearLayoutTreeSelection = true;
  }

  if (clearLayoutTreeSelection)
    selection().start().document()->layoutViewItem().clearSelection();

  if (clearDOMTreeSelection)
    setSelection(VisibleSelection(), DoNotSetFocus);
  m_frameCaret->setCaretRectNeedsUpdate();

  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (!document().isRunningExecCommand())
    TypingCommand::closeTyping(m_frame);
}

static Position updatePositionAfterAdoptingTextReplacement(
    const Position& position,
    CharacterData* node,
    unsigned offset,
    unsigned oldLength,
    unsigned newLength) {
  if (!position.anchorNode() || position.anchorNode() != node ||
      !position.isOffsetInAnchor())
    return position;

  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.offsetInContainerNode(), 0);
  unsigned positionOffset =
      static_cast<unsigned>(position.offsetInContainerNode());
  // Replacing text can be viewed as a deletion followed by insertion.
  if (positionOffset >= offset && positionOffset <= offset + oldLength)
    positionOffset = offset;

  // Adjust the offset if the position is after the end of the deleted contents
  // (positionOffset > offset + oldLength) to avoid having a stale offset.
  if (positionOffset > offset + oldLength)
    positionOffset = positionOffset - oldLength + newLength;

  // Due to case folding
  // (http://unicode.org/Public/UCD/latest/ucd/CaseFolding.txt), LayoutText
  // length may be different from Text length.  A correct implementation would
  // translate the LayoutText offset to a Text offset; this is just a safety
  // precaution to avoid offset values that run off the end of the Text.
  if (positionOffset > node->length())
    positionOffset = node->length();

  // CharacterNode in VisibleSelection must be Text node, because Comment
  // and ProcessingInstruction node aren't visible.
  return Position(toText(node), positionOffset);
}

void FrameSelection::didUpdateCharacterData(CharacterData* node,
                                            unsigned offset,
                                            unsigned oldLength,
                                            unsigned newLength) {
  // The fragment check is a performance optimization. See
  // http://trac.webkit.org/changeset/30062.
  if (isNone() || !node || !node->isConnected())
    return;

  Position base = updatePositionAfterAdoptingTextReplacement(
      selection().base(), node, offset, oldLength, newLength);
  Position extent = updatePositionAfterAdoptingTextReplacement(
      selection().extent(), node, offset, oldLength, newLength);
  Position start = updatePositionAfterAdoptingTextReplacement(
      selection().start(), node, offset, oldLength, newLength);
  Position end = updatePositionAfterAdoptingTextReplacement(
      selection().end(), node, offset, oldLength, newLength);
  updateSelectionIfNeeded(base, extent, start, end);
}

static Position updatePostionAfterAdoptingTextNodesMerged(
    const Position& position,
    const Text& oldNode,
    unsigned offset) {
  if (!position.anchorNode() || !position.isOffsetInAnchor())
    return position;

  DCHECK_GE(position.offsetInContainerNode(), 0);
  unsigned positionOffset =
      static_cast<unsigned>(position.offsetInContainerNode());

  if (position.anchorNode() == &oldNode)
    return Position(toText(oldNode.previousSibling()), positionOffset + offset);

  if (position.anchorNode() == oldNode.parentNode() && positionOffset == offset)
    return Position(toText(oldNode.previousSibling()), offset);

  return position;
}

void FrameSelection::didMergeTextNodes(const Text& oldNode, unsigned offset) {
  if (isNone() || !oldNode.isConnected())
    return;
  Position base = updatePostionAfterAdoptingTextNodesMerged(selection().base(),
                                                            oldNode, offset);
  Position extent = updatePostionAfterAdoptingTextNodesMerged(
      selection().extent(), oldNode, offset);
  Position start = updatePostionAfterAdoptingTextNodesMerged(
      selection().start(), oldNode, offset);
  Position end = updatePostionAfterAdoptingTextNodesMerged(selection().end(),
                                                           oldNode, offset);
  updateSelectionIfNeeded(base, extent, start, end);
}

static Position updatePostionAfterAdoptingTextNodeSplit(
    const Position& position,
    const Text& oldNode) {
  if (!position.anchorNode() || position.anchorNode() != &oldNode ||
      !position.isOffsetInAnchor())
    return position;
  // See:
  // http://www.w3.org/TR/DOM-Level-2-Traversal-Range/ranges.html#Level-2-Range-Mutation
  DCHECK_GE(position.offsetInContainerNode(), 0);
  unsigned positionOffset =
      static_cast<unsigned>(position.offsetInContainerNode());
  unsigned oldLength = oldNode.length();
  if (positionOffset <= oldLength)
    return position;
  return Position(toText(oldNode.nextSibling()), positionOffset - oldLength);
}

void FrameSelection::didSplitTextNode(const Text& oldNode) {
  if (isNone() || !oldNode.isConnected())
    return;
  Position base =
      updatePostionAfterAdoptingTextNodeSplit(selection().base(), oldNode);
  Position extent =
      updatePostionAfterAdoptingTextNodeSplit(selection().extent(), oldNode);
  Position start =
      updatePostionAfterAdoptingTextNodeSplit(selection().start(), oldNode);
  Position end =
      updatePostionAfterAdoptingTextNodeSplit(selection().end(), oldNode);
  updateSelectionIfNeeded(base, extent, start, end);
}

void FrameSelection::updateSelectionIfNeeded(const Position& base,
                                             const Position& extent,
                                             const Position& start,
                                             const Position& end) {
  if (base == selection().base() && extent == selection().extent() &&
      start == selection().start() && end == selection().end())
    return;
  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (!document().isRunningExecCommand())
    TypingCommand::closeTyping(m_frame);
  VisibleSelection newSelection;
  if (selection().isBaseFirst())
    newSelection.setWithoutValidation(start, end);
  else
    newSelection.setWithoutValidation(end, start);
  setSelection(newSelection, DoNotSetFocus);
}

void FrameSelection::didChangeFocus() {
  // Hits in
  // virtual/gpu/compositedscrolling/scrollbars/scrollbar-miss-mousemove-disabled.html
  DisableCompositingQueryAsserts disabler;
  updateAppearance();
}

static DispatchEventResult dispatchSelectStart(
    const VisibleSelection& selection) {
  Node* selectStartTarget = selection.extent().computeContainerNode();
  if (!selectStartTarget)
    return DispatchEventResult::NotCanceled;

  return selectStartTarget->dispatchEvent(
      Event::createCancelableBubble(EventTypeNames::selectstart));
}

// The return value of |FrameSelection::modify()| is different based on
// value of |userTriggered| parameter.
// When |userTriggered| is |userTriggered|, |modify()| returns false if
// "selectstart" event is dispatched and canceled, otherwise returns true.
// When |userTriggered| is |NotUserTrigged|, return value specifies whether
// selection is modified or not.
bool FrameSelection::modify(EAlteration alter,
                            SelectionDirection direction,
                            TextGranularity granularity,
                            EUserTriggered userTriggered) {
  SelectionModifier selectionModifier(*frame(), selection(),
                                      m_xPosForVerticalArrowNavigation);
  const bool modified = selectionModifier.modify(alter, direction, granularity);
  if (userTriggered == UserTriggered &&
      selectionModifier.selection().isRange() && selection().isCaret() &&
      dispatchSelectStart(selection()) != DispatchEventResult::NotCanceled) {
    return false;
  }
  if (!modified) {
    if (userTriggered == NotUserTriggered)
      return false;
    // If spatial navigation enabled, focus navigator will move focus to
    // another element. See snav-input.html and snav-textarea.html
    if (isSpatialNavigationEnabled(m_frame))
      return false;
    // Even if selection isn't changed, we prevent to default action, e.g.
    // scroll window when caret is at end of content editable.
    return true;
  }

  const SetSelectionOptions options =
      CloseTyping | ClearTypingStyle | userTriggered;
  setSelection(selectionModifier.selection(), options);

  if (granularity == LineGranularity || granularity == ParagraphGranularity)
    m_xPosForVerticalArrowNavigation =
        selectionModifier.xPosForVerticalArrowNavigation();

  if (userTriggered == UserTriggered)
    m_granularity = CharacterGranularity;

  m_frameCaret->setCaretRectNeedsUpdate();

  return true;
}

bool FrameSelection::modify(EAlteration alter,
                            unsigned verticalDistance,
                            VerticalDirection direction) {
  SelectionModifier selectionModifier(*frame(), selection());
  if (!selectionModifier.modifyWithPageGranularity(alter, verticalDistance,
                                                   direction)) {
    return false;
  }

  setSelection(selectionModifier.selection(),
               CloseTyping | ClearTypingStyle | UserTriggered,
               alter == AlterationMove ? CursorAlignOnScroll::Always
                                       : CursorAlignOnScroll::IfNeeded);

  m_granularity = CharacterGranularity;

  return true;
}

void FrameSelection::clear() {
  m_granularity = CharacterGranularity;
  if (m_granularityStrategy)
    m_granularityStrategy->Clear();
  setSelection(VisibleSelection());
}

void FrameSelection::documentAttached(Document* document) {
  DCHECK(document);
  DCHECK(!m_document) << "FrameSelection is already attached to " << m_document;
  m_document = document;
  m_useSecureKeyboardEntryWhenActive = false;
  m_selectionEditor->documentAttached(document);
}

void FrameSelection::documentDetached(const Document& document) {
  DCHECK_EQ(m_document, document);
  m_document = nullptr;
  m_granularity = CharacterGranularity;

  LayoutViewItem view = m_frame->contentLayoutItem();
  if (!view.isNull())
    view.clearSelection();

  clearTypingStyle();
  m_selectionEditor->documentDetached(document);
  m_frameCaret->documentDetached();
}

LayoutBlock* FrameSelection::caretLayoutObject() const {
  DCHECK(selection().isValidFor(document()));
  if (!isCaret())
    return nullptr;
  return CaretBase::caretLayoutObject(selection().start().anchorNode());
}

IntRect FrameSelection::absoluteCaretBounds() {
  DCHECK(selection().isValidFor(*m_frame->document()));
  return m_frameCaret->absoluteCaretBounds();
}

void FrameSelection::invalidateCaretRect(bool forceInvalidation) {
  m_frameCaret->invalidateCaretRect(forceInvalidation);
}

void FrameSelection::dataWillChange(const CharacterData& node) {
  m_frameCaret->dataWillChange(node);
}

void FrameSelection::paintCaret(GraphicsContext& context,
                                const LayoutPoint& paintOffset) {
  m_frameCaret->paintCaret(context, paintOffset);
}

bool FrameSelection::contains(const LayoutPoint& point) {
  if (document().layoutViewItem().isNull())
    return false;

  // Treat a collapsed selection like no selection.
  const VisibleSelectionInFlatTree& visibleSelection =
      this->visibleSelection<EditingInFlatTreeStrategy>();
  if (!visibleSelection.isRange())
    return false;

  HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
  HitTestResult result(request, point);
  document().layoutViewItem().hitTest(result);
  Node* innerNode = result.innerNode();
  if (!innerNode || !innerNode->layoutObject())
    return false;

  const VisiblePositionInFlatTree& visiblePos =
      createVisiblePosition(fromPositionInDOMTree<EditingInFlatTreeStrategy>(
          innerNode->layoutObject()->positionForPoint(result.localPoint())));
  if (visiblePos.isNull())
    return false;

  const VisiblePositionInFlatTree& visibleStart =
      visibleSelection.visibleStart();
  const VisiblePositionInFlatTree& visibleEnd = visibleSelection.visibleEnd();
  if (visibleStart.isNull() || visibleEnd.isNull())
    return false;

  const PositionInFlatTree& start = visibleStart.deepEquivalent();
  const PositionInFlatTree& end = visibleEnd.deepEquivalent();
  const PositionInFlatTree& pos = visiblePos.deepEquivalent();
  return start.compareTo(pos) <= 0 && pos.compareTo(end) <= 0;
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the
// frame you entirely selected. Can't do this implicitly as part of every
// setSelection call because in some contexts it might not be good for the focus
// to move to another frame. So instead we call it from places where we are
// selecting with the mouse or the keyboard after setting the selection.
void FrameSelection::selectFrameElementInParentIfFullySelected() {
  // Find the parent frame; if there is none, then we have nothing to do.
  Frame* parent = m_frame->tree().parent();
  if (!parent)
    return;
  Page* page = m_frame->page();
  if (!page)
    return;

  // Check if the selection contains the entire frame contents; if not, then
  // there is nothing to do.
  if (!isRange())
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (!isStartOfDocument(selection().visibleStart()))
    return;
  if (!isEndOfDocument(selection().visibleEnd()))
    return;

  // FIXME: This is not yet implemented for cross-process frame relationships.
  if (!parent->isLocalFrame())
    return;

  // Get to the <iframe> or <frame> (or even <object>) element in the parent
  // frame.
  // FIXME: Doesn't work for OOPI.
  HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
  if (!ownerElement)
    return;
  ContainerNode* ownerElementParent = ownerElement->parentNode();
  if (!ownerElementParent)
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  ownerElementParent->document().updateStyleAndLayoutIgnorePendingStylesheets();

  // This method's purpose is it to make it easier to select iframes (in order
  // to delete them).  Don't do anything if the iframe isn't deletable.
  if (!blink::hasEditableStyle(*ownerElementParent))
    return;

  // Create compute positions before and after the element.
  unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
  VisiblePosition beforeOwnerElement = createVisiblePosition(
      Position(ownerElementParent, ownerElementNodeIndex));
  VisiblePosition afterOwnerElement = createVisiblePosition(
      Position(ownerElementParent, ownerElementNodeIndex + 1),
      VP_UPSTREAM_IF_POSSIBLE);

  SelectionInDOMTree::Builder builder;
  builder
      .setBaseAndExtentDeprecated(beforeOwnerElement.deepEquivalent(),
                                  afterOwnerElement.deepEquivalent())
      .setAffinity(beforeOwnerElement.affinity());

  // Focus on the parent frame, and then select from before this element to
  // after.
  VisibleSelection newSelection = createVisibleSelection(builder.build());
  page->focusController().setFocusedFrame(parent);
  // setFocusedFrame can dispatch synchronous focus/blur events.  The document
  // tree might be modified.
  if (newSelection.isNonOrphanedCaretOrRange())
    toLocalFrame(parent)->selection().setSelection(newSelection);
}

// Returns a shadow tree node for legacy shadow trees, a child of the
// ShadowRoot node for new shadow trees, or 0 for non-shadow trees.
static Node* nonBoundaryShadowTreeRootNode(const Position& position) {
  return position.anchorNode() && !position.anchorNode()->isShadowRoot()
             ? position.anchorNode()->nonBoundaryShadowTreeRootNode()
             : nullptr;
}

void FrameSelection::selectAll() {
  if (isHTMLSelectElement(document().focusedElement())) {
    HTMLSelectElement* selectElement =
        toHTMLSelectElement(document().focusedElement());
    if (selectElement->canSelectAll()) {
      selectElement->selectAll();
      return;
    }
  }

  Node* root = nullptr;
  Node* selectStartTarget = nullptr;
  if (isContentEditable()) {
    root = highestEditableRoot(selection().start());
    if (Node* shadowRoot = nonBoundaryShadowTreeRootNode(selection().start()))
      selectStartTarget = shadowRoot->ownerShadowHost();
    else
      selectStartTarget = root;
  } else {
    root = nonBoundaryShadowTreeRootNode(selection().start());
    if (root) {
      selectStartTarget = root->ownerShadowHost();
    } else {
      root = document().documentElement();
      selectStartTarget = document().body();
    }
  }
  if (!root || editingIgnoresContent(*root))
    return;

  if (selectStartTarget) {
    const Document& expectedDocument = document();
    if (selectStartTarget->dispatchEvent(Event::createCancelableBubble(
            EventTypeNames::selectstart)) != DispatchEventResult::NotCanceled)
      return;
    // |root| may be detached due to selectstart event.
    if (!root->isConnected() || expectedDocument != root->document())
      return;
  }

  setSelection(SelectionInDOMTree::Builder().selectAllChildren(*root).build());
  selectFrameElementInParentIfFullySelected();
  notifyLayoutObjectOfSelectionChange(UserTriggered);
}

bool FrameSelection::setSelectedRange(const EphemeralRange& range,
                                      TextAffinity affinity,
                                      SelectionDirectionalMode directional,
                                      SetSelectionOptions options) {
  if (range.isNull())
    return false;
  m_selectionEditor->resetLogicalRange();
  // Since |FrameSeleciton::setSelection()| dispatches events and DOM tree
  // can be modified by event handlers, we should create |Range| object before
  // calling it.
  Range* logicalRange = createRange(range);
  setSelection(SelectionInDOMTree::Builder()
                   .setBaseAndExtent(range)
                   .setAffinity(affinity)
                   .setIsDirectional(directional ==
                                     SelectionDirectionalMode::Directional)
                   .build(),
               options);
  m_selectionEditor->setLogicalRange(logicalRange);
  return true;
}

Range* FrameSelection::firstRange() const {
  return m_selectionEditor->firstRange();
}

bool FrameSelection::isInPasswordField() const {
  TextControlElement* textControl = enclosingTextControl(start());
  return isHTMLInputElement(textControl) &&
         toHTMLInputElement(textControl)->type() == InputTypeNames::password;
}

void FrameSelection::notifyAccessibilityForSelectionChange() {
  if (selection().start().isNotNull() && selection().end().isNotNull()) {
    if (AXObjectCache* cache = document().existingAXObjectCache())
      cache->selectionChanged(selection().start().computeContainerNode());
  }
}

void FrameSelection::notifyCompositorForSelectionChange() {
  if (!RuntimeEnabledFeatures::compositedSelectionUpdateEnabled())
    return;

  scheduleVisualUpdate();
}

void FrameSelection::notifyEventHandlerForSelectionChange() {
  m_frame->eventHandler().selectionController().notifySelectionChanged();
}

void FrameSelection::focusedOrActiveStateChanged() {
  bool activeAndFocused = isFocusedAndActive();

  // Trigger style invalidation from the focused element. Even though
  // the focused element hasn't changed, the evaluation of focus pseudo
  // selectors are dependent on whether the frame is focused and active.
  if (Element* element = document().focusedElement())
    element->focusStateChanged();

  document().updateStyleAndLayoutTree();

  // Because LayoutObject::selectionBackgroundColor() and
  // LayoutObject::selectionForegroundColor() check if the frame is active,
  // we have to update places those colors were painted.
  LayoutViewItem view = document().layoutViewItem();
  if (!view.isNull())
    view.invalidatePaintForSelection();

  // Caret appears in the active frame.
  if (activeAndFocused)
    setSelectionFromNone();
  else
    m_frame->spellChecker().spellCheckAfterBlur();
  m_frameCaret->setCaretVisibility(activeAndFocused ? CaretVisibility::Visible
                                                    : CaretVisibility::Hidden);

  // Update for caps lock state
  m_frame->eventHandler().capsLockStateMayHaveChanged();

  // Secure keyboard entry is set by the active frame.
  if (m_useSecureKeyboardEntryWhenActive)
    setUseSecureKeyboardEntry(activeAndFocused);
}

void FrameSelection::pageActivationChanged() {
  focusedOrActiveStateChanged();
}

void FrameSelection::updateSecureKeyboardEntryIfActive() {
  if (!isFocusedAndActive())
    return;
  setUseSecureKeyboardEntry(m_useSecureKeyboardEntryWhenActive);
}

void FrameSelection::setUseSecureKeyboardEntryWhenActive(
    bool usesSecureKeyboard) {
  if (m_useSecureKeyboardEntryWhenActive == usesSecureKeyboard)
    return;
  m_useSecureKeyboardEntryWhenActive = usesSecureKeyboard;
  updateSecureKeyboardEntryIfActive();
}

void FrameSelection::setUseSecureKeyboardEntry(bool enable) {
  if (enable)
    enableSecureTextInput();
  else
    disableSecureTextInput();
}

void FrameSelection::setFocused(bool flag) {
  if (m_focused == flag)
    return;
  m_focused = flag;

  focusedOrActiveStateChanged();
}

bool FrameSelection::isFocusedAndActive() const {
  return m_focused && m_frame->page() &&
         m_frame->page()->focusController().isActive();
}

bool FrameSelection::isAppearanceDirty() const {
  return m_pendingSelection->hasPendingSelection();
}

void FrameSelection::commitAppearanceIfNeeded(LayoutView& layoutView) {
  return m_pendingSelection->commit(layoutView);
}

void FrameSelection::updateAppearance() {
  m_frameCaret->updateAppearance();

  if (m_frame->contentLayoutItem().isNull())
    return;
  m_pendingSelection->setHasPendingSelection();
}

void FrameSelection::notifyLayoutObjectOfSelectionChange(
    EUserTriggered userTriggered) {
  if (TextControlElement* textControl = enclosingTextControl(start()))
    textControl->selectionChanged(userTriggered == UserTriggered);
}

// Helper function that tells whether a particular node is an element that has
// an entire LocalFrame and FrameView, a <frame>, <iframe>, or <object>.
static bool isFrameElement(const Node* n) {
  if (!n)
    return false;
  LayoutObject* layoutObject = n->layoutObject();
  if (!layoutObject || !layoutObject->isLayoutPart())
    return false;
  Widget* widget = toLayoutPart(layoutObject)->widget();
  return widget && widget->isFrameView();
}

void FrameSelection::setFocusedNodeIfNeeded() {
  if (isNone() || !isFocused())
    return;

  if (Element* target = rootEditableElement()) {
    // Walk up the DOM tree to search for a node to focus.
    document().updateStyleAndLayoutTreeIgnorePendingStylesheets();
    while (target) {
      // We don't want to set focus on a subframe when selecting in a parent
      // frame, so add the !isFrameElement check here. There's probably a better
      // way to make this work in the long term, but this is the safest fix at
      // this time.
      if (target->isMouseFocusable() && !isFrameElement(target)) {
        m_frame->page()->focusController().setFocusedElement(target, m_frame);
        return;
      }
      target = target->parentOrShadowHostElement();
    }
    document().clearFocusedElement();
  }
}

static String extractSelectedText(const FrameSelection& selection,
                                  TextIteratorBehavior behavior) {
  const VisibleSelectionInFlatTree& visibleSelection =
      selection.visibleSelection<EditingInFlatTreeStrategy>();
  const EphemeralRangeInFlatTree& range =
      visibleSelection.toNormalizedEphemeralRange();
  // We remove '\0' characters because they are not visibly rendered to the
  // user.
  return plainText(range, behavior).replace(0, "");
}

String FrameSelection::selectedHTMLForClipboard() const {
  const VisibleSelectionInFlatTree& visibleSelection =
      this->visibleSelection<EditingInFlatTreeStrategy>();
  const EphemeralRangeInFlatTree& range =
      visibleSelection.toNormalizedEphemeralRange();
  return createMarkup(range.startPosition(), range.endPosition(),
                      AnnotateForInterchange,
                      ConvertBlocksToInlines::NotConvert, ResolveNonLocalURLs);
}

String FrameSelection::selectedText(TextIteratorBehavior behavior) const {
  return extractSelectedText(*this, behavior);
}

String FrameSelection::selectedTextForClipboard() const {
  if (m_frame->settings() &&
      m_frame->settings()->selectionIncludesAltImageText())
    return extractSelectedText(*this, TextIteratorEmitsImageAltText);
  return extractSelectedText(*this, TextIteratorDefaultBehavior);
}

LayoutRect FrameSelection::bounds() const {
  FrameView* view = m_frame->view();
  if (!view)
    return LayoutRect();

  return intersection(unclippedBounds(),
                      LayoutRect(view->visibleContentRect()));
}

LayoutRect FrameSelection::unclippedBounds() const {
  FrameView* view = m_frame->view();
  LayoutViewItem layoutView = m_frame->contentLayoutItem();

  if (!view || layoutView.isNull())
    return LayoutRect();

  view->updateLifecycleToLayoutClean();
  return LayoutRect(layoutView.selectionBounds());
}

static inline HTMLFormElement* associatedFormElement(HTMLElement& element) {
  if (isHTMLFormElement(element))
    return &toHTMLFormElement(element);
  return element.formOwner();
}

// Scans logically forward from "start", including any child frames.
static HTMLFormElement* scanForForm(Node* start) {
  if (!start)
    return 0;

  for (HTMLElement& element : Traversal<HTMLElement>::startsAt(
           start->isHTMLElement() ? toHTMLElement(start)
                                  : Traversal<HTMLElement>::next(*start))) {
    if (HTMLFormElement* form = associatedFormElement(element))
      return form;

    if (isHTMLFrameElementBase(element)) {
      Node* childDocument = toHTMLFrameElementBase(element).contentDocument();
      if (HTMLFormElement* frameResult = scanForForm(childDocument))
        return frameResult;
    }
  }
  return 0;
}

// We look for either the form containing the current focus, or for one
// immediately after it
HTMLFormElement* FrameSelection::currentForm() const {
  // Start looking either at the active (first responder) node, or where the
  // selection is.
  Node* start = document().focusedElement();
  if (!start)
    start = this->start().anchorNode();
  if (!start)
    return 0;

  // Try walking up the node tree to find a form element.
  for (HTMLElement* element =
           Traversal<HTMLElement>::firstAncestorOrSelf(*start);
       element; element = Traversal<HTMLElement>::firstAncestor(*element)) {
    if (HTMLFormElement* form = associatedFormElement(*element))
      return form;
  }

  // Try walking forward in the node tree to find a form element.
  return scanForForm(start);
}

void FrameSelection::revealSelection(const ScrollAlignment& alignment,
                                     RevealExtentOption revealExtentOption) {
  DCHECK(isAvailable());

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // Calculation of absolute caret bounds requires clean layout.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  LayoutRect rect;

  switch (getSelectionType()) {
    case NoSelection:
      return;
    case CaretSelection:
      rect = LayoutRect(absoluteCaretBounds());
      break;
    case RangeSelection:
      rect = LayoutRect(
          revealExtentOption == RevealExtent
              ? absoluteCaretBoundsOf(createVisiblePosition(extent()))
              : enclosingIntRect(unclippedBounds()));
      break;
  }

  Position start = this->start();
  DCHECK(start.anchorNode());
  if (start.anchorNode() && start.anchorNode()->layoutObject()) {
    // FIXME: This code only handles scrolling the startContainer's layer, but
    // the selection rect could intersect more than just that.
    if (DocumentLoader* documentLoader = m_frame->loader().documentLoader())
      documentLoader->initialScrollState().wasScrolledByUser = true;
    if (start.anchorNode()->layoutObject()->scrollRectToVisible(rect, alignment,
                                                                alignment))
      updateAppearance();
  }
}

void FrameSelection::setSelectionFromNone() {
  // Put a caret inside the body if the entire frame is editable (either the
  // entire WebView is editable or designMode is on for this document).

  Document* document = m_frame->document();
  if (!isNone() || !(blink::hasEditableStyle(*document)))
    return;

  Element* documentElement = document->documentElement();
  if (!documentElement)
    return;
  if (HTMLBodyElement* body =
          Traversal<HTMLBodyElement>::firstChild(*documentElement)) {
    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    document->updateStyleAndLayoutIgnorePendingStylesheets();

    setSelection(SelectionInDOMTree::Builder()
                     .collapse(firstPositionInOrBeforeNode(body))
                     .build());
  }
}

// TODO(yoichio): We should have LocalFrame having FrameCaret,
// Editor and PendingSelection using FrameCaret directly
// and get rid of this.
bool FrameSelection::shouldShowBlockCursor() const {
  return m_frameCaret->shouldShowBlockCursor();
}

// TODO(yoichio): We should have LocalFrame having FrameCaret,
// Editor and PendingSelection using FrameCaret directly
// and get rid of this.
// TODO(yoichio): We should use "caret-shape" in "CSS Basic User Interface
// Module Level 4" https://drafts.csswg.org/css-ui-4/
// To use "caret-shape", we need to expose inserting mode information to CSS;
// https://github.com/w3c/csswg-drafts/issues/133
void FrameSelection::setShouldShowBlockCursor(bool shouldShowBlockCursor) {
  m_frameCaret->setShouldShowBlockCursor(shouldShowBlockCursor);
}

#ifndef NDEBUG

void FrameSelection::showTreeForThis() const {
  selection().showTreeForThis();
}

#endif

DEFINE_TRACE(FrameSelection) {
  visitor->trace(m_document);
  visitor->trace(m_frame);
  visitor->trace(m_pendingSelection);
  visitor->trace(m_selectionEditor);
  visitor->trace(m_typingStyle);
  visitor->trace(m_frameCaret);
}

void FrameSelection::scheduleVisualUpdate() const {
  if (Page* page = m_frame->page())
    page->animator().scheduleVisualUpdate(m_frame->localFrameRoot());
}

bool FrameSelection::selectWordAroundPosition(const VisiblePosition& position) {
  static const EWordSide wordSideList[2] = {RightWordIfOnBoundary,
                                            LeftWordIfOnBoundary};
  for (EWordSide wordSide : wordSideList) {
    VisiblePosition start = startOfWord(position, wordSide);
    VisiblePosition end = endOfWord(position, wordSide);
    String text =
        plainText(EphemeralRange(start.deepEquivalent(), end.deepEquivalent()));
    if (!text.isEmpty() && !isSeparator(text.characterStartingAt(0))) {
      setSelection(
          createVisibleSelection(SelectionInDOMTree::Builder()
                                     .collapse(start.toPositionWithAffinity())
                                     .extend(end.deepEquivalent())
                                     .build()),
          CloseTyping | ClearTypingStyle, CursorAlignOnScroll::IfNeeded,
          WordGranularity);
      return true;
    }
  }

  return false;
}

GranularityStrategy* FrameSelection::granularityStrategy() {
  // We do lazy initalization for m_granularityStrategy, because if we
  // initialize it right in the constructor - the correct settings may not be
  // set yet.
  SelectionStrategy strategyType = SelectionStrategy::Character;
  Settings* settings = m_frame ? m_frame->settings() : 0;
  if (settings && settings->selectionStrategy() == SelectionStrategy::Direction)
    strategyType = SelectionStrategy::Direction;

  if (m_granularityStrategy && m_granularityStrategy->GetType() == strategyType)
    return m_granularityStrategy.get();

  if (strategyType == SelectionStrategy::Direction)
    m_granularityStrategy = makeUnique<DirectionGranularityStrategy>();
  else
    m_granularityStrategy = makeUnique<CharacterGranularityStrategy>();
  return m_granularityStrategy.get();
}

void FrameSelection::moveRangeSelectionExtent(const IntPoint& contentsPoint) {
  if (isNone())
    return;

  VisibleSelection newSelection =
      granularityStrategy()->updateExtent(contentsPoint, m_frame);
  setSelection(newSelection,
               FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle |
                   FrameSelection::DoNotClearStrategy | UserTriggered,
               CursorAlignOnScroll::IfNeeded, CharacterGranularity);
}

// TODO(yosin): We should make |FrameSelection::moveRangeSelection()| to take
// two |IntPoint| instead of two |VisiblePosition| like
// |moveRangeSelectionExtent()|.
void FrameSelection::moveRangeSelection(const VisiblePosition& basePosition,
                                        const VisiblePosition& extentPosition,
                                        TextGranularity granularity) {
  VisibleSelection newSelection = createVisibleSelection(
      SelectionInDOMTree::Builder()
          .setBaseAndExtentDeprecated(basePosition.deepEquivalent(),
                                      extentPosition.deepEquivalent())
          .setAffinity(basePosition.affinity())
          .setGranularity(granularity)
          .build());

  if (newSelection.isNone())
    return;

  setSelection(newSelection, CloseTyping | ClearTypingStyle,
               CursorAlignOnScroll::IfNeeded, granularity);
}

void FrameSelection::updateIfNeeded() {
  DCHECK(!m_frame->document()->needsLayoutTreeUpdate());
  m_selectionEditor->updateIfNeeded();
}

void FrameSelection::setCaretVisible(bool caretIsVisible) {
  m_frameCaret->setCaretVisibility(caretIsVisible ? CaretVisibility::Visible
                                                  : CaretVisibility::Hidden);
}

bool FrameSelection::shouldPaintCaretForTesting() const {
  return m_frameCaret->shouldPaintCaretForTesting();
}

bool FrameSelection::isPreviousCaretDirtyForTesting() const {
  return m_frameCaret->isPreviousCaretDirtyForTesting();
}

bool FrameSelection::isCaretBoundsDirty() const {
  return m_frameCaret->isCaretBoundsDirty();
}

void FrameSelection::setCaretRectNeedsUpdate() {
  m_frameCaret->setCaretRectNeedsUpdate();
}

void FrameSelection::setCaretBlinkingSuspended(bool suspended) {
  m_frameCaret->setCaretBlinkingSuspended(suspended);
}

bool FrameSelection::isCaretBlinkingSuspended() const {
  return m_frameCaret->isCaretBlinkingSuspended();
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::FrameSelection& sel) {
  sel.showTreeForThis();
}

void showTree(const blink::FrameSelection* sel) {
  if (sel)
    sel->showTreeForThis();
  else
    LOG(INFO) << "Cannot showTree for <null> FrameSelection.";
}

#endif
