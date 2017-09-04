/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/editing/InputMethodController.h"

#include "core/InputModeNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/CompositionEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/ChromeClient.h"

namespace blink {

namespace {

void dispatchCompositionUpdateEvent(LocalFrame& frame, const String& text) {
  Element* target = frame.document()->focusedElement();
  if (!target)
    return;

  CompositionEvent* event = CompositionEvent::create(
      EventTypeNames::compositionupdate, frame.domWindow(), text);
  target->dispatchEvent(event);
}

void dispatchCompositionEndEvent(LocalFrame& frame, const String& text) {
  Element* target = frame.document()->focusedElement();
  if (!target)
    return;

  CompositionEvent* event = CompositionEvent::create(
      EventTypeNames::compositionend, frame.domWindow(), text);
  target->dispatchEvent(event);
}

// Used to insert/replace text during composition update and confirm
// composition.
// Procedure:
//   1. Fire 'beforeinput' event for (TODO(chongz): deleted composed text) and
//      inserted text
//   2. Fire 'compositionupdate' event
//   3. Fire TextEvent and modify DOM
//   TODO(chongz): 4. Fire 'input' event
void insertTextDuringCompositionWithEvents(
    LocalFrame& frame,
    const String& text,
    TypingCommand::Options options,
    TypingCommand::TextCompositionType compositionType) {
  DCHECK(compositionType ==
             TypingCommand::TextCompositionType::TextCompositionUpdate ||
         compositionType ==
             TypingCommand::TextCompositionType::TextCompositionConfirm)
      << "compositionType should be TextCompositionUpdate or "
         "TextCompositionConfirm, but got "
      << static_cast<int>(compositionType);
  if (!frame.document())
    return;

  Element* target = frame.document()->focusedElement();
  if (!target)
    return;

  // TODO(chongz): Fire 'beforeinput' for the composed text being
  // replaced/deleted.

  // Only the last confirmed text is cancelable.
  InputEvent::EventCancelable beforeInputCancelable =
      (compositionType ==
       TypingCommand::TextCompositionType::TextCompositionUpdate)
          ? InputEvent::EventCancelable::NotCancelable
          : InputEvent::EventCancelable::IsCancelable;
  DispatchEventResult result = dispatchBeforeInputFromComposition(
      target, InputEvent::InputType::InsertText, text, beforeInputCancelable);

  if (beforeInputCancelable == InputEvent::EventCancelable::IsCancelable &&
      result != DispatchEventResult::NotCanceled)
    return;

  // 'beforeinput' event handler may destroy document.
  if (!frame.document())
    return;

  dispatchCompositionUpdateEvent(frame, text);
  // 'compositionupdate' event handler may destroy document.
  if (!frame.document())
    return;

  switch (compositionType) {
    case TypingCommand::TextCompositionType::TextCompositionUpdate:
      TypingCommand::insertText(*frame.document(), text, options,
                                compositionType);
      break;
    case TypingCommand::TextCompositionType::TextCompositionConfirm:
      // TODO(chongz): Use TypingCommand::insertText after TextEvent was
      // removed. (Removed from spec since 2012)
      // See TextEvent.idl.
      frame.eventHandler().handleTextInputEvent(text, 0,
                                                TextEventInputComposition);
      break;
    default:
      NOTREACHED();
  }
  // TODO(chongz): Fire 'input' event.
}

AtomicString getInputModeAttribute(Element* element) {
  if (!element)
    return AtomicString();

  bool queryAttribute = false;
  if (isHTMLInputElement(*element)) {
    queryAttribute = toHTMLInputElement(*element).supportsInputModeAttribute();
  } else if (isHTMLTextAreaElement(*element)) {
    queryAttribute = true;
  } else {
    element->document().updateStyleAndLayoutTree();
    if (hasEditableStyle(*element))
      queryAttribute = true;
  }

  if (!queryAttribute)
    return AtomicString();

  // TODO(dtapuska): We may wish to restrict this to a yet to be proposed
  // <contenteditable> or <richtext> element Mozilla discussed at TPAC 2016.
  return element->fastGetAttribute(HTMLNames::inputmodeAttr).lower();
}

}  // anonymous namespace

InputMethodController* InputMethodController::create(LocalFrame& frame) {
  return new InputMethodController(frame);
}

InputMethodController::InputMethodController(LocalFrame& frame)
    : m_frame(&frame), m_isDirty(false), m_hasComposition(false) {}

bool InputMethodController::isAvailable() const {
  return frame().document();
}

Document& InputMethodController::document() const {
  DCHECK(isAvailable());
  return *frame().document();
}

bool InputMethodController::hasComposition() const {
  return m_hasComposition && !m_compositionRange->collapsed() &&
         m_compositionRange->isConnected();
}

inline Editor& InputMethodController::editor() const {
  return frame().editor();
}

void InputMethodController::clear() {
  m_hasComposition = false;
  if (m_compositionRange) {
    m_compositionRange->setStart(&document(), 0);
    m_compositionRange->collapse(true);
  }
  document().markers().removeMarkers(DocumentMarker::Composition);
  m_isDirty = false;
}

void InputMethodController::documentDetached() {
  clear();
  m_compositionRange = nullptr;
}

void InputMethodController::selectComposition() const {
  const EphemeralRange range = compositionEphemeralRange();
  if (range.isNull())
    return;

  // The composition can start inside a composed character sequence, so we have
  // to override checks. See <http://bugs.webkit.org/show_bug.cgi?id=15781>
  VisibleSelection selection;
  selection.setWithoutValidation(range.startPosition(), range.endPosition());
  frame().selection().setSelection(selection, 0);
}

bool InputMethodController::finishComposingText(
    ConfirmCompositionBehavior confirmBehavior) {
  if (!hasComposition())
    return false;

  if (confirmBehavior == KeepSelection) {
    PlainTextRange oldOffsets = getSelectionOffsets();
    Editor::RevealSelectionScope revealSelectionScope(&editor());

    bool result = replaceComposition(composingText());

    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    setSelectionOffsets(oldOffsets);
    return result;
  }

  return replaceCompositionAndMoveCaret(composingText(), 0);
}

bool InputMethodController::commitText(const String& text,
                                       int relativeCaretPosition) {
  if (hasComposition())
    return replaceCompositionAndMoveCaret(text, relativeCaretPosition);

  // We should do nothing in this case, because:
  // 1. No need to insert text when text is empty.
  // 2. Shouldn't move caret when relativeCaretPosition == 0 to avoid
  // duplicate selection change event.
  if (!text.length() && !relativeCaretPosition)
    return false;
  return insertTextAndMoveCaret(text, relativeCaretPosition);
}

bool InputMethodController::replaceComposition(const String& text) {
  if (!hasComposition())
    return false;

  // If the composition was set from existing text and didn't change, then
  // there's nothing to do here (and we should avoid doing anything as that
  // may clobber multi-node styled text).
  if (!m_isDirty && composingText() == text) {
    clear();
    return true;
  }

  // Select the text that will be deleted or replaced.
  selectComposition();

  if (frame().selection().isNone())
    return false;

  if (!isAvailable())
    return false;

  // If text is empty, then delete the old composition here. If text is
  // non-empty, InsertTextCommand::input will delete the old composition with
  // an optimized replace operation.
  if (text.isEmpty())
    TypingCommand::deleteSelection(document(), 0);

  clear();

  insertTextDuringCompositionWithEvents(
      frame(), text, 0,
      TypingCommand::TextCompositionType::TextCompositionConfirm);
  // Event handler might destroy document.
  if (!isAvailable())
    return false;

  // No DOM update after 'compositionend'.
  dispatchCompositionEndEvent(frame(), text);

  return true;
}

// relativeCaretPosition is relative to the end of the text.
static int computeAbsoluteCaretPosition(size_t textStart,
                                        size_t textLength,
                                        int relativeCaretPosition) {
  return textStart + textLength + relativeCaretPosition;
}

bool InputMethodController::replaceCompositionAndMoveCaret(
    const String& text,
    int relativeCaretPosition) {
  Element* rootEditableElement = frame().selection().rootEditableElement();
  if (!rootEditableElement)
    return false;
  DCHECK(hasComposition());
  PlainTextRange compositionRange =
      PlainTextRange::create(*rootEditableElement, *m_compositionRange);
  if (compositionRange.isNull())
    return false;
  int textStart = compositionRange.start();

  if (!replaceComposition(text))
    return false;

  int absoluteCaretPosition = computeAbsoluteCaretPosition(
      textStart, text.length(), relativeCaretPosition);
  return moveCaret(absoluteCaretPosition);
}

bool InputMethodController::insertText(const String& text) {
  if (dispatchBeforeInputInsertText(document().focusedElement(), text) !=
      DispatchEventResult::NotCanceled)
    return false;
  editor().insertText(text, 0);
  return true;
}

bool InputMethodController::insertTextAndMoveCaret(const String& text,
                                                   int relativeCaretPosition) {
  PlainTextRange selectionRange = getSelectionOffsets();
  if (selectionRange.isNull())
    return false;
  int textStart = selectionRange.start();

  if (text.length()) {
    if (!insertText(text))
      return false;
  }

  int absoluteCaretPosition = computeAbsoluteCaretPosition(
      textStart, text.length(), relativeCaretPosition);
  return moveCaret(absoluteCaretPosition);
}

void InputMethodController::cancelComposition() {
  if (!hasComposition())
    return;

  Editor::RevealSelectionScope revealSelectionScope(&editor());

  if (frame().selection().isNone())
    return;

  clear();

  // TODO(chongz): Figure out which InputType should we use here.
  dispatchBeforeInputFromComposition(
      document().focusedElement(),
      InputEvent::InputType::DeleteComposedCharacterBackward, nullAtom,
      InputEvent::EventCancelable::NotCancelable);
  dispatchCompositionUpdateEvent(frame(), emptyString());
  insertTextDuringCompositionWithEvents(
      frame(), emptyString(), 0,
      TypingCommand::TextCompositionType::TextCompositionConfirm);
  // Event handler might destroy document.
  if (!isAvailable())
    return;

  // An open typing command that disagrees about current selection would cause
  // issues with typing later on.
  TypingCommand::closeTyping(m_frame);

  // No DOM update after 'compositionend'.
  dispatchCompositionEndEvent(frame(), emptyString());
}

void InputMethodController::cancelCompositionIfSelectionIsInvalid() {
  if (!hasComposition() || editor().preventRevealSelection())
    return;

  // Check if selection start and selection end are valid.
  FrameSelection& selection = frame().selection();
  if (!selection.isNone() && !m_compositionRange->collapsed()) {
    if (selection.start().compareTo(m_compositionRange->startPosition()) >= 0 &&
        selection.end().compareTo(m_compositionRange->endPosition()) <= 0)
      return;
  }

  cancelComposition();
  frame().chromeClient().didCancelCompositionOnSelectionChange();
}

static size_t computeCommonPrefixLength(const String& str1,
                                        const String& str2) {
  const size_t maxCommonPrefixLength = std::min(str1.length(), str2.length());
  for (size_t index = 0; index < maxCommonPrefixLength; ++index) {
    if (str1[index] != str2[index])
      return index;
  }
  return maxCommonPrefixLength;
}

static size_t computeCommonSuffixLength(const String& str1,
                                        const String& str2) {
  const size_t length1 = str1.length();
  const size_t length2 = str2.length();
  const size_t maxCommonSuffixLength = std::min(length1, length2);
  for (size_t index = 0; index < maxCommonSuffixLength; ++index) {
    if (str1[length1 - index - 1] != str2[length2 - index - 1])
      return index;
  }
  return maxCommonSuffixLength;
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest left grapheme boundary.
static size_t computeDistanceToLeftGraphemeBoundary(const Position& position) {
  const Position& adjustedPosition = previousPositionOf(
      nextPositionOf(position, PositionMoveType::GraphemeCluster),
      PositionMoveType::GraphemeCluster);
  DCHECK_EQ(position.anchorNode(), adjustedPosition.anchorNode());
  DCHECK_GE(position.computeOffsetInContainerNode(),
            adjustedPosition.computeOffsetInContainerNode());
  return static_cast<size_t>(position.computeOffsetInContainerNode() -
                             adjustedPosition.computeOffsetInContainerNode());
}

static size_t computeCommonGraphemeClusterPrefixLengthForSetComposition(
    const String& oldText,
    const String& newText,
    const Element* rootEditableElement) {
  const size_t commonPrefixLength = computeCommonPrefixLength(oldText, newText);

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(0, commonPrefixLength).createRange(*rootEditableElement);
  if (range.isNull())
    return 0;
  const Position& position = range.endPosition();
  const size_t diff = computeDistanceToLeftGraphemeBoundary(position);
  DCHECK_GE(commonPrefixLength, diff);
  return commonPrefixLength - diff;
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest right grapheme boundary.
static size_t computeDistanceToRightGraphemeBoundary(const Position& position) {
  const Position& adjustedPosition = nextPositionOf(
      previousPositionOf(position, PositionMoveType::GraphemeCluster),
      PositionMoveType::GraphemeCluster);
  DCHECK_EQ(position.anchorNode(), adjustedPosition.anchorNode());
  DCHECK_GE(adjustedPosition.computeOffsetInContainerNode(),
            position.computeOffsetInContainerNode());
  return static_cast<size_t>(adjustedPosition.computeOffsetInContainerNode() -
                             position.computeOffsetInContainerNode());
}

static size_t computeCommonGraphemeClusterSuffixLengthForSetComposition(
    const String& oldText,
    const String& newText,
    const Element* rootEditableElement) {
  const size_t commonSuffixLength = computeCommonSuffixLength(oldText, newText);

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(0, oldText.length() - commonSuffixLength)
          .createRange(*rootEditableElement);
  if (range.isNull())
    return 0;
  const Position& position = range.endPosition();
  const size_t diff = computeDistanceToRightGraphemeBoundary(position);
  DCHECK_GE(commonSuffixLength, diff);
  return commonSuffixLength - diff;
}

void InputMethodController::setCompositionWithIncrementalText(
    const String& text,
    const Vector<CompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd) {
  Element* editable = frame().selection().rootEditableElement();
  if (!editable)
    return;

  DCHECK_LE(selectionStart, selectionEnd);
  String composing = composingText();
  const size_t commonPrefixLength =
      computeCommonGraphemeClusterPrefixLengthForSetComposition(composing, text,
                                                                editable);

  // We should ignore common prefix when finding common suffix.
  const size_t commonSuffixLength =
      computeCommonGraphemeClusterSuffixLengthForSetComposition(
          composing.right(composing.length() - commonPrefixLength),
          text.right(text.length() - commonPrefixLength), editable);

  const bool inserting =
      text.length() > commonPrefixLength + commonSuffixLength;
  const bool deleting =
      composing.length() > commonPrefixLength + commonSuffixLength;

  if (inserting || deleting) {
    // Select the text to be deleted.
    const size_t compositionStart =
        PlainTextRange::create(*editable, compositionEphemeralRange()).start();
    const size_t deletionStart = compositionStart + commonPrefixLength;
    const size_t deletionEnd =
        compositionStart + composing.length() - commonSuffixLength;
    const EphemeralRange& deletionRange =
        PlainTextRange(deletionStart, deletionEnd).createRange(*editable);
    VisibleSelection selection;
    selection.setWithoutValidation(deletionRange.startPosition(),
                                   deletionRange.endPosition());
    Document& currentDocument = document();
    frame().selection().setSelection(selection, 0);
    clear();

    // FrameSeleciton::setSelection() can change document associate to |frame|.
    if (!isAvailable() || currentDocument != document())
      return;
    if (!currentDocument.focusedElement())
      return;

    // Insert the incremental text.
    const size_t insertionLength =
        text.length() - commonPrefixLength - commonSuffixLength;
    const String& insertingText =
        text.substring(commonPrefixLength, insertionLength);
    insertTextDuringCompositionWithEvents(frame(), insertingText,
                                          TypingCommand::PreventSpellChecking,
                                          TypingCommand::TextCompositionUpdate);

    // Event handlers might destroy document.
    if (!isAvailable() || currentDocument != document())
      return;

    // TODO(yosin): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // Now recreate the composition starting at its original start, and
    // apply the specified final selection offsets.
    setCompositionFromExistingText(underlines, compositionStart,
                                   compositionStart + text.length());
  }

  selectComposition();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  const PlainTextRange& selectedRange = createSelectionRangeForSetComposition(
      selectionStart, selectionEnd, text.length());
  // We shouldn't close typing in the middle of setComposition.
  setEditableSelectionOffsets(selectedRange, NotUserTriggered);
  m_isDirty = true;
}

void InputMethodController::setComposition(
    const String& text,
    const Vector<CompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd) {
  Editor::RevealSelectionScope revealSelectionScope(&editor());

  // Updates styles before setting selection for composition to prevent
  // inserting the previous composition text into text nodes oddly.
  // See https://bugs.webkit.org/show_bug.cgi?id=46868
  document().updateStyleAndLayoutTree();

  // When the IME only wants to change a few characters at the end of the
  // composition, only touch those characters in order to preserve rich text
  // substructure.
  if (hasComposition() && text.length()) {
    return setCompositionWithIncrementalText(text, underlines, selectionStart,
                                             selectionEnd);
  }

  selectComposition();

  if (frame().selection().isNone())
    return;

  Element* target = document().focusedElement();
  if (!target)
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  PlainTextRange selectedRange = createSelectionRangeForSetComposition(
      selectionStart, selectionEnd, text.length());

  // Dispatch an appropriate composition event to the focused node.
  // We check the composition status and choose an appropriate composition event
  // since this function is used for three purposes:
  // 1. Starting a new composition.
  //    Send a compositionstart and a compositionupdate event when this function
  //    creates a new composition node, i.e. !hasComposition() &&
  //    !text.isEmpty().
  //    Sending a compositionupdate event at this time ensures that at least one
  //    compositionupdate event is dispatched.
  // 2. Updating the existing composition node.
  //    Send a compositionupdate event when this function updates the existing
  //    composition node, i.e. hasComposition() && !text.isEmpty().
  // 3. Canceling the ongoing composition.
  //    Send a compositionend event when function deletes the existing
  //    composition node, i.e. !hasComposition() && test.isEmpty().
  if (text.isEmpty()) {
    if (hasComposition()) {
      Editor::RevealSelectionScope revealSelectionScope(&editor());
      replaceComposition(emptyString());
    } else {
      // It's weird to call |setComposition()| with empty text outside
      // composition, however some IME (e.g. Japanese IBus-Anthy) did this, so
      // we simply delete selection without sending extra events.
      TypingCommand::deleteSelection(document(),
                                     TypingCommand::PreventSpellChecking);
    }

    // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    setEditableSelectionOffsets(selectedRange);
    return;
  }

  // We should send a 'compositionstart' event only when the given text is not
  // empty because this function doesn't create a composition node when the text
  // is empty.
  if (!hasComposition()) {
    target->dispatchEvent(
        CompositionEvent::create(EventTypeNames::compositionstart,
                                 frame().domWindow(), frame().selectedText()));
    if (!isAvailable())
      return;
  }

  DCHECK(!text.isEmpty());

  clear();

  insertTextDuringCompositionWithEvents(
      frame(), text,
      TypingCommand::SelectInsertedText | TypingCommand::PreventSpellChecking,
      TypingCommand::TextCompositionUpdate);
  // Event handlers might destroy document.
  if (!isAvailable())
    return;

  // TODO(yosin): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // Find out what node has the composition now.
  Position base = mostForwardCaretPosition(frame().selection().base());
  Node* baseNode = base.anchorNode();
  if (!baseNode || !baseNode->isTextNode())
    return;

  Position extent = frame().selection().extent();
  Node* extentNode = extent.anchorNode();
  if (baseNode != extentNode)
    return;

  unsigned extentOffset = extent.computeOffsetInContainerNode();
  unsigned baseOffset = base.computeOffsetInContainerNode();
  if (baseOffset + text.length() != extentOffset)
    return;

  m_isDirty = true;
  m_hasComposition = true;
  if (!m_compositionRange)
    m_compositionRange = Range::create(document());
  m_compositionRange->setStart(baseNode, baseOffset);
  m_compositionRange->setEnd(baseNode, extentOffset);

  if (baseNode->layoutObject())
    baseNode->layoutObject()->setShouldDoFullPaintInvalidation();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // We shouldn't close typing in the middle of setComposition.
  setEditableSelectionOffsets(selectedRange, NotUserTriggered);

  if (underlines.isEmpty()) {
    document().markers().addCompositionMarker(
        m_compositionRange->startPosition(), m_compositionRange->endPosition(),
        Color::black, false,
        LayoutTheme::theme().platformDefaultCompositionBackgroundColor());
    return;
  }
  for (const auto& underline : underlines) {
    unsigned underlineStart = baseOffset + underline.startOffset();
    unsigned underlineEnd = baseOffset + underline.endOffset();
    EphemeralRange ephemeralLineRange = EphemeralRange(
        Position(baseNode, underlineStart), Position(baseNode, underlineEnd));
    if (ephemeralLineRange.isNull())
      continue;
    document().markers().addCompositionMarker(
        ephemeralLineRange.startPosition(), ephemeralLineRange.endPosition(),
        underline.color(), underline.thick(), underline.backgroundColor());
  }
}

PlainTextRange InputMethodController::createSelectionRangeForSetComposition(
    int selectionStart,
    int selectionEnd,
    size_t textLength) const {
  const int selectionOffsetsStart =
      static_cast<int>(getSelectionOffsets().start());
  const int start = selectionOffsetsStart + selectionStart;
  const int end = selectionOffsetsStart + selectionEnd;
  return createRangeForSelection(start, end, textLength);
}

void InputMethodController::setCompositionFromExistingText(
    const Vector<CompositionUnderline>& underlines,
    unsigned compositionStart,
    unsigned compositionEnd) {
  Element* editable = frame().selection().rootEditableElement();
  if (!editable)
    return;

  DCHECK(!document().needsLayoutTreeUpdate());

  const EphemeralRange range =
      PlainTextRange(compositionStart, compositionEnd).createRange(*editable);
  if (range.isNull())
    return;

  const Position start = range.startPosition();
  if (rootEditableElementOf(start) != editable)
    return;

  const Position end = range.endPosition();
  if (rootEditableElementOf(end) != editable)
    return;

  clear();

  for (const auto& underline : underlines) {
    unsigned underlineStart = compositionStart + underline.startOffset();
    unsigned underlineEnd = compositionStart + underline.endOffset();
    EphemeralRange ephemeralLineRange =
        PlainTextRange(underlineStart, underlineEnd).createRange(*editable);
    if (ephemeralLineRange.isNull())
      continue;
    document().markers().addCompositionMarker(
        ephemeralLineRange.startPosition(), ephemeralLineRange.endPosition(),
        underline.color(), underline.thick(), underline.backgroundColor());
  }

  m_hasComposition = true;
  if (!m_compositionRange)
    m_compositionRange = Range::create(document());
  m_compositionRange->setStart(range.startPosition());
  m_compositionRange->setEnd(range.endPosition());
}

EphemeralRange InputMethodController::compositionEphemeralRange() const {
  if (!hasComposition())
    return EphemeralRange();
  return EphemeralRange(m_compositionRange.get());
}

Range* InputMethodController::compositionRange() const {
  return hasComposition() ? m_compositionRange : nullptr;
}

String InputMethodController::composingText() const {
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document().lifecycle());
  return plainText(compositionEphemeralRange(), TextIteratorEmitsOriginalText);
}

PlainTextRange InputMethodController::getSelectionOffsets() const {
  EphemeralRange range = firstEphemeralRangeOf(frame().selection().selection());
  if (range.isNull())
    return PlainTextRange();
  ContainerNode* editable =
      frame().selection().rootEditableElementOrTreeScopeRootNode();
  DCHECK(editable);
  return PlainTextRange::create(*editable, range);
}

bool InputMethodController::setSelectionOffsets(
    const PlainTextRange& selectionOffsets,
    FrameSelection::SetSelectionOptions options) {
  if (selectionOffsets.isNull())
    return false;
  Element* rootEditableElement = frame().selection().rootEditableElement();
  if (!rootEditableElement)
    return false;

  DCHECK(!document().needsLayoutTreeUpdate());

  const EphemeralRange range =
      selectionOffsets.createRange(*rootEditableElement);
  if (range.isNull())
    return false;

  return frame().selection().setSelectedRange(
      range, VP_DEFAULT_AFFINITY, SelectionDirectionalMode::NonDirectional,
      options);
}

bool InputMethodController::setEditableSelectionOffsets(
    const PlainTextRange& selectionOffsets,
    FrameSelection::SetSelectionOptions options) {
  if (!editor().canEdit())
    return false;
  return setSelectionOffsets(selectionOffsets, options);
}

PlainTextRange InputMethodController::createRangeForSelection(
    int start,
    int end,
    size_t textLength) const {
  // In case of exceeding the left boundary.
  start = std::max(start, 0);
  end = std::max(end, start);

  Element* rootEditableElement = frame().selection().rootEditableElement();
  if (!rootEditableElement)
    return PlainTextRange();
  const EphemeralRange& range =
      EphemeralRange::rangeOfContents(*rootEditableElement);
  if (range.isNull())
    return PlainTextRange();

  const TextIteratorBehaviorFlags behaviorFlags =
      TextIteratorEmitsObjectReplacementCharacter |
      TextIteratorEmitsCharactersBetweenAllVisiblePositions;
  TextIterator it(range.startPosition(), range.endPosition(), behaviorFlags);

  int rightBoundary = 0;
  for (; !it.atEnd(); it.advance())
    rightBoundary += it.length();

  if (hasComposition())
    rightBoundary -= compositionRange()->text().length();

  rightBoundary += textLength;

  // In case of exceeding the right boundary.
  start = std::min(start, rightBoundary);
  end = std::min(end, rightBoundary);

  return PlainTextRange(start, end);
}

bool InputMethodController::moveCaret(int newCaretPosition) {
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  PlainTextRange selectedRange =
      createRangeForSelection(newCaretPosition, newCaretPosition, 0);
  if (selectedRange.isNull())
    return false;
  return setEditableSelectionOffsets(selectedRange);
}

void InputMethodController::extendSelectionAndDelete(int before, int after) {
  if (!editor().canEdit())
    return;
  PlainTextRange selectionOffsets(getSelectionOffsets());
  if (selectionOffsets.isNull())
    return;

  // A common call of before=1 and after=0 will fail if the last character
  // is multi-code-word UTF-16, including both multi-16bit code-points and
  // Unicode combining character sequences of multiple single-16bit code-
  // points (officially called "compositions"). Try more until success.
  // http://crbug.com/355995
  //
  // FIXME: Note that this is not an ideal solution when this function is
  // called to implement "backspace". In that case, there should be some call
  // that will not delete a full multi-code-point composition but rather
  // only the last code-point so that it's possible for a user to correct
  // a composition without starting it from the beginning.
  // http://crbug.com/37993
  do {
    if (!setSelectionOffsets(PlainTextRange(
            std::max(static_cast<int>(selectionOffsets.start()) - before, 0),
            selectionOffsets.end() + after)))
      return;
    if (before == 0)
      break;
    ++before;
  } while (frame().selection().start() == frame().selection().end() &&
           before <= static_cast<int>(selectionOffsets.start()));
  // TODO(chongz): Find a way to distinguish Forward and Backward.
  dispatchBeforeInputEditorCommand(
      document().focusedElement(), InputEvent::InputType::DeleteContentBackward,
      new RangeVector(1, m_frame->selection().firstRange()));
  TypingCommand::deleteSelection(document());
}

// TODO(yabinh): We should reduce the number of selectionchange events.
void InputMethodController::deleteSurroundingText(int before, int after) {
  if (!editor().canEdit())
    return;
  const PlainTextRange selectionOffsets(getSelectionOffsets());
  if (selectionOffsets.isNull())
    return;
  Element* const rootEditableElement =
      frame().selection().rootEditableElement();
  if (!rootEditableElement)
    return;
  int selectionStart = static_cast<int>(selectionOffsets.start());
  int selectionEnd = static_cast<int>(selectionOffsets.end());

  // Select the text to be deleted before selectionStart.
  if (before > 0 && selectionStart > 0) {
    // In case of exceeding the left boundary.
    const int start = std::max(selectionStart - before, 0);

    const EphemeralRange& range =
        PlainTextRange(0, start).createRange(*rootEditableElement);
    if (range.isNull())
      return;
    const Position& position = range.endPosition();

    // Adjust the start of selection for multi-code text(a grapheme cluster
    // contains more than one code point). TODO(yabinh): Adjustment should be
    // based on code point instead of grapheme cluster.
    const size_t diff = computeDistanceToLeftGraphemeBoundary(position);
    const int adjustedStart = start - static_cast<int>(diff);
    if (!setSelectionOffsets(PlainTextRange(adjustedStart, selectionStart)))
      return;
    TypingCommand::deleteSelection(document());

    selectionEnd = selectionEnd - (selectionStart - adjustedStart);
    selectionStart = adjustedStart;
  }

  // Select the text to be deleted after selectionEnd.
  if (after > 0) {
    // Adjust the deleted range in case of exceeding the right boundary.
    const PlainTextRange range(0, selectionEnd + after);
    if (range.isNull())
      return;
    const EphemeralRange& validRange = range.createRange(*rootEditableElement);
    if (validRange.isNull())
      return;
    const int end =
        PlainTextRange::create(*rootEditableElement, validRange).end();
    const Position& position = validRange.endPosition();

    // Adjust the end of selection for multi-code text. TODO(yabinh): Adjustment
    // should be based on code point instead of grapheme cluster.
    const size_t diff = computeDistanceToRightGraphemeBoundary(position);
    const int adjustedEnd = end + static_cast<int>(diff);
    if (!setSelectionOffsets(PlainTextRange(selectionEnd, adjustedEnd)))
      return;
    TypingCommand::deleteSelection(document());
  }

  setSelectionOffsets(PlainTextRange(selectionStart, selectionEnd));
}

WebTextInputInfo InputMethodController::textInputInfo() const {
  WebTextInputInfo info;
  if (!isAvailable())
    return info;

  if (!frame().selection().isAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return info;
  }
  Element* element = frame().selection().rootEditableElement();
  if (!element)
    return info;

  info.inputMode = inputModeOfFocusedElement();
  info.type = textInputType();
  info.flags = textInputFlags();
  if (info.type == WebTextInputTypeNone)
    return info;

  if (!frame().editor().canEdit())
    return info;

  // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  see http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document().lifecycle());

  // Emits an object replacement character for each replaced element so that
  // it is exposed to IME and thus could be deleted by IME on android.
  info.value = plainText(EphemeralRange::rangeOfContents(*element),
                         TextIteratorEmitsObjectReplacementCharacter);

  if (info.value.isEmpty())
    return info;

  EphemeralRange firstRange =
      firstEphemeralRangeOf(frame().selection().selection());
  if (firstRange.isNotNull()) {
    PlainTextRange plainTextRange(PlainTextRange::create(*element, firstRange));
    if (plainTextRange.isNotNull()) {
      info.selectionStart = plainTextRange.start();
      info.selectionEnd = plainTextRange.end();
    }
  }

  EphemeralRange range = compositionEphemeralRange();
  if (range.isNotNull()) {
    PlainTextRange plainTextRange(PlainTextRange::create(*element, range));
    if (plainTextRange.isNotNull()) {
      info.compositionStart = plainTextRange.start();
      info.compositionEnd = plainTextRange.end();
    }
  }

  return info;
}

int InputMethodController::textInputFlags() const {
  Element* element = document().focusedElement();
  if (!element)
    return WebTextInputFlagNone;

  int flags = 0;

  const AtomicString& autocomplete =
      element->getAttribute(HTMLNames::autocompleteAttr);
  if (autocomplete == "on")
    flags |= WebTextInputFlagAutocompleteOn;
  else if (autocomplete == "off")
    flags |= WebTextInputFlagAutocompleteOff;

  const AtomicString& autocorrect =
      element->getAttribute(HTMLNames::autocorrectAttr);
  if (autocorrect == "on")
    flags |= WebTextInputFlagAutocorrectOn;
  else if (autocorrect == "off")
    flags |= WebTextInputFlagAutocorrectOff;

  SpellcheckAttributeState spellcheck = element->spellcheckAttributeState();
  if (spellcheck == SpellcheckAttributeTrue)
    flags |= WebTextInputFlagSpellcheckOn;
  else if (spellcheck == SpellcheckAttributeFalse)
    flags |= WebTextInputFlagSpellcheckOff;

  if (isTextControlElement(element)) {
    TextControlElement* textControl = toTextControlElement(element);
    if (textControl->supportsAutocapitalize()) {
      DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
      DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
      DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
      DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

      const AtomicString& autocapitalize = textControl->autocapitalize();
      if (autocapitalize == none)
        flags |= WebTextInputFlagAutocapitalizeNone;
      else if (autocapitalize == characters)
        flags |= WebTextInputFlagAutocapitalizeCharacters;
      else if (autocapitalize == words)
        flags |= WebTextInputFlagAutocapitalizeWords;
      else if (autocapitalize == sentences)
        flags |= WebTextInputFlagAutocapitalizeSentences;
      else
        NOTREACHED();
    }
  }

  return flags;
}

WebTextInputMode InputMethodController::inputModeOfFocusedElement() const {
  if (!RuntimeEnabledFeatures::inputModeAttributeEnabled())
    return kWebTextInputModeDefault;

  AtomicString mode = getInputModeAttribute(document().focusedElement());

  if (mode.isEmpty())
    return kWebTextInputModeDefault;
  if (mode == InputModeNames::verbatim)
    return kWebTextInputModeVerbatim;
  if (mode == InputModeNames::latin)
    return kWebTextInputModeLatin;
  if (mode == InputModeNames::latin_name)
    return kWebTextInputModeLatinName;
  if (mode == InputModeNames::latin_prose)
    return kWebTextInputModeLatinProse;
  if (mode == InputModeNames::full_width_latin)
    return kWebTextInputModeFullWidthLatin;
  if (mode == InputModeNames::kana)
    return kWebTextInputModeKana;
  if (mode == InputModeNames::kana_name)
    return kWebTextInputModeKanaName;
  if (mode == InputModeNames::katakana)
    return kWebTextInputModeKataKana;
  if (mode == InputModeNames::numeric)
    return kWebTextInputModeNumeric;
  if (mode == InputModeNames::tel)
    return kWebTextInputModeTel;
  if (mode == InputModeNames::email)
    return kWebTextInputModeEmail;
  if (mode == InputModeNames::url)
    return kWebTextInputModeUrl;
  return kWebTextInputModeDefault;
}

WebTextInputType InputMethodController::textInputType() const {
  if (!frame().selection().isAvailable()) {
    // "mouse-capture-inside-shadow.html" reaches here.
    return WebTextInputTypeNone;
  }

  // It's important to preserve the equivalence of textInputInfo().type and
  // textInputType(), so perform the same rootEditableElement() existence check
  // here for consistency.
  if (!frame().selection().selection().rootEditableElement())
    return WebTextInputTypeNone;

  if (!isAvailable())
    return WebTextInputTypeNone;

  Element* element = document().focusedElement();
  if (!element)
    return WebTextInputTypeNone;

  if (isHTMLInputElement(*element)) {
    HTMLInputElement& input = toHTMLInputElement(*element);
    const AtomicString& type = input.type();

    if (input.isDisabledOrReadOnly())
      return WebTextInputTypeNone;

    if (type == InputTypeNames::password)
      return WebTextInputTypePassword;
    if (type == InputTypeNames::search)
      return WebTextInputTypeSearch;
    if (type == InputTypeNames::email)
      return WebTextInputTypeEmail;
    if (type == InputTypeNames::number)
      return WebTextInputTypeNumber;
    if (type == InputTypeNames::tel)
      return WebTextInputTypeTelephone;
    if (type == InputTypeNames::url)
      return WebTextInputTypeURL;
    if (type == InputTypeNames::text)
      return WebTextInputTypeText;

    return WebTextInputTypeNone;
  }

  if (isHTMLTextAreaElement(*element)) {
    if (toHTMLTextAreaElement(*element).isDisabledOrReadOnly())
      return WebTextInputTypeNone;
    return WebTextInputTypeTextArea;
  }

  if (element->isHTMLElement()) {
    if (toHTMLElement(element)->isDateTimeFieldElement())
      return WebTextInputTypeDateTimeField;
  }

  document().updateStyleAndLayoutTree();
  if (hasEditableStyle(*element))
    return WebTextInputTypeContentEditable;

  return WebTextInputTypeNone;
}

DEFINE_TRACE(InputMethodController) {
  visitor->trace(m_frame);
  visitor->trace(m_compositionRange);
}

}  // namespace blink
