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

#include "core/editing/spellcheck/SpellChecker.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/CompositeEditCommand.h"
#include "core/editing/commands/ReplaceSelectionCommand.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/TextCheckingParagraph.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLInputElement.h"
#include "core/layout/LayoutTextControl.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Page.h"
#include "core/page/SpellCheckerClient.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextCheckerClient.h"

namespace blink {

using namespace HTMLNames;

namespace {

bool isPositionInTextField(const Position& selectionStart) {
  TextControlElement* textControl = enclosingTextControl(selectionStart);
  return isHTMLInputElement(textControl) &&
         toHTMLInputElement(textControl)->isTextField();
}

bool isPositionInTextArea(const Position& position) {
  TextControlElement* textControl = enclosingTextControl(position);
  return isHTMLTextAreaElement(textControl);
}

bool isSelectionInTextFormControl(const VisibleSelection& selection) {
  return !!enclosingTextControl(selection.start());
}

static bool isSpellCheckingEnabledFor(const Position& position) {
  if (position.isNull())
    return false;
  // TODO(tkent): The following password type check should be done in
  // HTMLElement::spellcheck(). crbug.com/371567
  if (TextControlElement* textControl = enclosingTextControl(position)) {
    if (isHTMLInputElement(textControl) &&
        toHTMLInputElement(textControl)->type() == InputTypeNames::password)
      return false;
  }
  if (HTMLElement* element =
          Traversal<HTMLElement>::firstAncestorOrSelf(*position.anchorNode())) {
    if (element->isSpellCheckingEnabled())
      return true;
  }
  return false;
}

static bool isSpellCheckingEnabledFor(const VisibleSelection& selection) {
  if (selection.isNone())
    return false;
  return isSpellCheckingEnabledFor(selection.start());
}

static EphemeralRange expandEndToSentenceBoundary(const EphemeralRange& range) {
  DCHECK(range.isNotNull());
  const VisiblePosition& visibleEnd =
      createVisiblePosition(range.endPosition());
  DCHECK(visibleEnd.isNotNull());
  const Position& sentenceEnd = endOfSentence(visibleEnd).deepEquivalent();
  // TODO(xiaochengh): |sentenceEnd < range.endPosition()| is possible,
  // which would trigger a DCHECK in EphemeralRange's constructor if we return
  // it directly. However, this shouldn't happen and needs to be fixed.
  return EphemeralRange(
      range.startPosition(),
      sentenceEnd.isNotNull() && sentenceEnd > range.endPosition()
          ? sentenceEnd
          : range.endPosition());
}

static EphemeralRange expandRangeToSentenceBoundary(
    const EphemeralRange& range) {
  DCHECK(range.isNotNull());
  const VisiblePosition& visibleStart =
      createVisiblePosition(range.startPosition());
  DCHECK(visibleStart.isNotNull());
  const Position& sentenceStart =
      startOfSentence(visibleStart).deepEquivalent();
  // TODO(xiaochengh): |sentenceStart > range.startPosition()| is possible,
  // which would trigger a DCHECK in EphemeralRange's constructor if we return
  // it directly. However, this shouldn't happen and needs to be fixed.
  return expandEndToSentenceBoundary(EphemeralRange(
      sentenceStart.isNotNull() && sentenceStart < range.startPosition()
          ? sentenceStart
          : range.startPosition(),
      range.endPosition()));
}

SelectionInDOMTree selectWord(const VisiblePosition& position) {
  // TODO(yosin): We should fix |startOfWord()| and |endOfWord()| not to return
  // null position.
  const VisiblePosition& start = startOfWord(position, LeftWordIfOnBoundary);
  const VisiblePosition& end = endOfWord(position, RightWordIfOnBoundary);
  return SelectionInDOMTree::Builder()
      .setBaseAndExtentDeprecated(start.deepEquivalent(), end.deepEquivalent())
      .setAffinity(start.affinity())
      .build();
}

}  // namespace

SpellChecker* SpellChecker::create(LocalFrame& frame) {
  return new SpellChecker(frame);
}

static SpellCheckerClient& emptySpellCheckerClient() {
  DEFINE_STATIC_LOCAL(EmptySpellCheckerClient, client, ());
  return client;
}

SpellCheckerClient& SpellChecker::spellCheckerClient() const {
  if (Page* page = frame().page())
    return page->spellCheckerClient();
  return emptySpellCheckerClient();
}

TextCheckerClient& SpellChecker::textChecker() const {
  return spellCheckerClient().textChecker();
}

SpellChecker::SpellChecker(LocalFrame& frame)
    : m_frame(&frame),
      m_spellCheckRequester(SpellCheckRequester::create(frame)) {}

bool SpellChecker::isSpellCheckingEnabled() const {
  return spellCheckerClient().isSpellCheckingEnabled();
}

void SpellChecker::toggleSpellCheckingEnabled() {
  spellCheckerClient().toggleSpellCheckingEnabled();
  if (isSpellCheckingEnabled())
    return;
  for (Frame* frame = this->frame().page()->mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    for (Node& node :
         NodeTraversal::startsAt(toLocalFrame(frame)->document()->rootNode())) {
      if (node.isElementNode())
        toElement(node).setAlreadySpellChecked(false);
    }
  }
}

void SpellChecker::didBeginEditing(Element* element) {
  if (!isSpellCheckingEnabled())
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term we should use idle time spell checker to prevent
  // synchronous layout caused by spell checking (see crbug.com/517298).
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame().document()->lifecycle());

  bool isTextField = false;
  TextControlElement* enclosingTextControlElement = nullptr;
  if (!isTextControlElement(*element)) {
    enclosingTextControlElement =
        enclosingTextControl(Position::firstPositionInNode(element));
  }
  element = enclosingTextControlElement ? enclosingTextControlElement : element;
  Element* parent = element;
  if (isTextControlElement(*element)) {
    TextControlElement* textControl = toTextControlElement(element);
    parent = textControl;
    element = textControl->innerEditorElement();
    if (!element)
      return;
    isTextField = isHTMLInputElement(*textControl) &&
                  toHTMLInputElement(*textControl).isTextField();
  }

  if (isTextField || !parent->isAlreadySpellChecked()) {
    if (editingIgnoresContent(*element))
      return;
    // We always recheck textfields because markers are removed from them on
    // blur.
    const VisibleSelection selection = createVisibleSelection(
        SelectionInDOMTree::Builder().selectAllChildren(*element).build());
    markMisspellingsInternal(selection);
    if (!isTextField)
      parent->setAlreadySpellChecked(true);
  }
}

void SpellChecker::ignoreSpelling() {
  removeMarkers(frame().selection().selection(), DocumentMarker::Spelling);
}

void SpellChecker::advanceToNextMisspelling(bool startBeforeSelection) {
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame().document()->lifecycle());

  // The basic approach is to search in two phases - from the selection end to
  // the end of the doc, and then we wrap and search from the doc start to
  // (approximately) where we started.

  // Start at the end of the selection, search to edge of document. Starting at
  // the selection end makes repeated "check spelling" commands work.
  VisibleSelection selection(frame().selection().selection());
  Position spellingSearchStart, spellingSearchEnd;
  Range::selectNodeContents(frame().document(), spellingSearchStart,
                            spellingSearchEnd);

  bool startedWithSelection = false;
  if (selection.start().anchorNode()) {
    startedWithSelection = true;
    if (startBeforeSelection) {
      VisiblePosition start(selection.visibleStart());
      // We match AppKit's rule: Start 1 character before the selection.
      VisiblePosition oneBeforeStart = previousPositionOf(start);
      spellingSearchStart =
          (oneBeforeStart.isNotNull() ? oneBeforeStart : start)
              .toParentAnchoredPosition();
    } else {
      spellingSearchStart = selection.visibleEnd().toParentAnchoredPosition();
    }
  }

  Position position = spellingSearchStart;
  if (!isEditablePosition(position)) {
    // This shouldn't happen in very often because the Spelling menu items
    // aren't enabled unless the selection is editable.  This can happen in Mail
    // for a mix of non-editable and editable content (like Stationary), when
    // spell checking the whole document before sending the message.  In that
    // case the document might not be editable, but there are editable pockets
    // that need to be spell checked.

    if (!frame().document()->documentElement())
      return;
    position = firstEditableVisiblePositionAfterPositionInRoot(
                   position, *frame().document()->documentElement())
                   .deepEquivalent();
    if (position.isNull())
      return;

    spellingSearchStart = position.parentAnchoredEquivalent();
    startedWithSelection = false;  // won't need to wrap
  }

  // topNode defines the whole range we want to operate on
  ContainerNode* topNode = highestEditableRoot(position);
  // TODO(yosin): |lastOffsetForEditing()| is wrong here if
  // |editingIgnoresContent(highestEditableRoot())| returns true, e.g. <table>
  spellingSearchEnd = Position::editingPositionOf(
      topNode, EditingStrategy::lastOffsetForEditing(topNode));

  // If spellingSearchRange starts in the middle of a word, advance to the
  // next word so we start checking at a word boundary. Going back by one char
  // and then forward by a word does the trick.
  if (startedWithSelection) {
    VisiblePosition oneBeforeStart =
        previousPositionOf(createVisiblePosition(spellingSearchStart));
    if (oneBeforeStart.isNotNull() &&
        rootEditableElementOf(oneBeforeStart) ==
            rootEditableElementOf(spellingSearchStart))
      spellingSearchStart =
          endOfWord(oneBeforeStart).toParentAnchoredPosition();
    // else we were already at the start of the editable node
  }

  if (spellingSearchStart == spellingSearchEnd)
    return;  // nothing to search in

  // We go to the end of our first range instead of the start of it, just to be
  // sure we don't get foiled by any word boundary problems at the start. It
  // means we might do a tiny bit more searching.
  Node* searchEndNodeAfterWrap = spellingSearchEnd.computeContainerNode();
  int searchEndOffsetAfterWrap = spellingSearchEnd.offsetInContainerNode();

  std::pair<String, int> misspelledItem(String(), 0);
  String& misspelledWord = misspelledItem.first;
  int& misspellingOffset = misspelledItem.second;
  misspelledItem = findFirstMisspelling(spellingSearchStart, spellingSearchEnd);

  // If we did not find a misspelled word, wrap and try again (but don't bother
  // if we started at the beginning of the block rather than at a selection).
  if (startedWithSelection && !misspelledWord) {
    spellingSearchStart = Position::editingPositionOf(topNode, 0);
    // going until the end of the very first chunk we tested is far enough
    spellingSearchEnd = Position::editingPositionOf(searchEndNodeAfterWrap,
                                                    searchEndOffsetAfterWrap);
    misspelledItem =
        findFirstMisspelling(spellingSearchStart, spellingSearchEnd);
  }

  if (!misspelledWord.isEmpty()) {
    // We found a misspelling. Select the misspelling, update the spelling
    // panel, and store a marker so we draw the red squiggle later.

    const EphemeralRange misspellingRange = calculateCharacterSubrange(
        EphemeralRange(spellingSearchStart, spellingSearchEnd),
        misspellingOffset, misspelledWord.length());
    frame().selection().setSelection(SelectionInDOMTree::Builder()
                                         .setBaseAndExtent(misspellingRange)
                                         .build());
    frame().selection().revealSelection();
    spellCheckerClient().updateSpellingUIWithMisspelledWord(misspelledWord);
    frame().document()->markers().addMarker(misspellingRange.startPosition(),
                                            misspellingRange.endPosition(),
                                            DocumentMarker::Spelling);
  }
}

void SpellChecker::showSpellingGuessPanel() {
  if (spellCheckerClient().spellingUIIsShowing()) {
    spellCheckerClient().showSpellingUI(false);
    return;
  }

  advanceToNextMisspelling(true);
  spellCheckerClient().showSpellingUI(true);
}

void SpellChecker::clearMisspellingsForMovingParagraphs(
    const VisibleSelection& movingSelection) {
  removeMarkers(movingSelection, DocumentMarker::MisspellingMarkers());
}

void SpellChecker::markMisspellingsForMovingParagraphs(
    const VisibleSelection& movingSelection) {
  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term we should use idle time spell checker to prevent
  // synchronous layout caused by spell checking (see crbug.com/517298).
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame().document()->lifecycle());

  markMisspellingsInternal(movingSelection);
}

void SpellChecker::markMisspellingsInternal(const VisibleSelection& selection) {
  if (!isSpellCheckingEnabled() || !isSpellCheckingEnabledFor(selection))
    return;

  const EphemeralRange& range = selection.toNormalizedEphemeralRange();
  if (range.isNull())
    return;

  // If we're not in an editable node, bail.
  Node* editableNode = range.startPosition().computeContainerNode();
  if (!editableNode || !hasEditableStyle(*editableNode))
    return;

  TextCheckingParagraph fullParagraphToCheck(
      expandRangeToSentenceBoundary(range));
  chunkAndMarkAllMisspellings(fullParagraphToCheck);
}

void SpellChecker::markMisspellingsAfterApplyingCommand(
    const CompositeEditCommand& cmd) {
  if (!isSpellCheckingEnabled())
    return;
  if (!isSpellCheckingEnabledFor(cmd.endingSelection()))
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term we should use idle time spell checker to prevent
  // synchronous layout caused by spell checking (see crbug.com/517298).
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  // Use type-based conditioning instead of polymorphism so that all spell
  // checking code can be encapsulated in SpellChecker.

  if (cmd.isTypingCommand()) {
    markMisspellingsAfterTypingCommand(toTypingCommand(cmd));
    return;
  }

  if (!cmd.isReplaceSelectionCommand())
    return;

  // Note: Request spell checking for and only for |ReplaceSelectionCommand|s
  // created in |Editor::replaceSelectionWithFragment()|.
  // TODO(xiaochengh): May also need to do this after dragging crbug.com/298046.
  if (cmd.inputType() != InputEvent::InputType::InsertFromPaste)
    return;

  markMisspellingsAfterReplaceSelectionCommand(toReplaceSelectionCommand(cmd));
}

void SpellChecker::markMisspellingsAfterTypingCommand(
    const TypingCommand& cmd) {
  m_spellCheckRequester->cancelCheck();

  // Take a look at the selection that results after typing and determine
  // whether we need to spellcheck.  Since the word containing the current
  // selection is never marked, this does a check to see if typing made a new
  // word that is not in the current selection.  Basically, you get this by
  // being at the end of a word and typing a space.
  VisiblePosition start = createVisiblePosition(
      cmd.endingSelection().start(), cmd.endingSelection().affinity());
  VisiblePosition previous = previousPositionOf(start);

  VisiblePosition wordStartOfPrevious =
      startOfWord(previous, LeftWordIfOnBoundary);

  if (cmd.commandTypeOfOpenCommand() ==
      TypingCommand::InsertParagraphSeparator) {
    VisiblePosition nextWord = nextWordPosition(start);
    // TODO(yosin): We should make |endOfWord()| not to return null position.
    VisibleSelection words = createVisibleSelection(
        SelectionInDOMTree::Builder()
            .setBaseAndExtentDeprecated(wordStartOfPrevious.deepEquivalent(),
                                        endOfWord(nextWord).deepEquivalent())
            .setAffinity(wordStartOfPrevious.affinity())
            .build());
    markMisspellingsAfterLineBreak(words);
    return;
  }

  if (previous.isNull())
    return;
  VisiblePosition currentWordStart = startOfWord(start, LeftWordIfOnBoundary);
  if (wordStartOfPrevious.deepEquivalent() == currentWordStart.deepEquivalent())
    return;
  markMisspellingsAfterTypingToWord(wordStartOfPrevious);
}

void SpellChecker::markMisspellingsAfterLineBreak(
    const VisibleSelection& wordSelection) {
  TRACE_EVENT0("blink", "SpellChecker::markMisspellingsAfterLineBreak");

  markMisspellingsInternal(wordSelection);
}

void SpellChecker::markMisspellingsAfterTypingToWord(
    const VisiblePosition& wordStart) {
  TRACE_EVENT0("blink", "SpellChecker::markMisspellingsAfterTypingToWord");

  VisibleSelection adjacentWords =
      createVisibleSelection(selectWord(wordStart));
  markMisspellingsInternal(adjacentWords);
}

bool SpellChecker::isSpellCheckingEnabledInFocusedNode() const {
  Node* focusedNode = frame().selection().start().anchorNode();
  if (!focusedNode)
    return false;
  const Element* focusedElement = focusedNode->isElementNode()
                                      ? toElement(focusedNode)
                                      : focusedNode->parentElement();
  if (!focusedElement)
    return false;
  return focusedElement->isSpellCheckingEnabled();
}

void SpellChecker::markMisspellingsAfterReplaceSelectionCommand(
    const ReplaceSelectionCommand& cmd) {
  TRACE_EVENT0("blink",
               "SpellChecker::markMisspellingsAfterReplaceSelectionCommand");

  const EphemeralRange& insertedRange = cmd.insertedRange();
  if (insertedRange.isNull())
    return;

  Node* node = cmd.endingSelection().rootEditableElement();
  if (!node)
    return;

  EphemeralRange paragraphRange(Position::firstPositionInNode(node),
                                Position::lastPositionInNode(node));
  TextCheckingParagraph textToCheck(insertedRange, paragraphRange);
  chunkAndMarkAllMisspellings(textToCheck);
}

void SpellChecker::chunkAndMarkAllMisspellings(
    const TextCheckingParagraph& fullParagraphToCheck) {
  if (fullParagraphToCheck.isEmpty())
    return;
  const EphemeralRange& paragraphRange = fullParagraphToCheck.paragraphRange();

  // Since the text may be quite big chunk it up and adjust to the sentence
  // boundary.
  const int kChunkSize = 16 * 1024;

  // Check the full paragraph instead if the paragraph is short, which saves
  // the cost on sentence boundary finding.
  if (fullParagraphToCheck.rangeLength() <= kChunkSize) {
    SpellCheckRequest* request = SpellCheckRequest::create(paragraphRange, 0);
    if (request)
      m_spellCheckRequester->requestCheckingFor(request);
    return;
  }

  CharacterIterator checkRangeIterator(
      fullParagraphToCheck.checkingRange(),
      TextIteratorEmitsObjectReplacementCharacter);
  for (int requestNum = 0; !checkRangeIterator.atEnd(); requestNum++) {
    EphemeralRange chunkRange =
        checkRangeIterator.calculateCharacterSubrange(0, kChunkSize);
    EphemeralRange checkRange = requestNum
                                    ? expandEndToSentenceBoundary(chunkRange)
                                    : expandRangeToSentenceBoundary(chunkRange);

    SpellCheckRequest* request =
        SpellCheckRequest::create(checkRange, requestNum);
    if (request)
      m_spellCheckRequester->requestCheckingFor(request);

    if (!checkRangeIterator.atEnd()) {
      checkRangeIterator.advance(1);
      // The layout should be already update due to the initialization of
      // checkRangeIterator, so comparePositions can be directly called.
      if (comparePositions(chunkRange.endPosition(), checkRange.endPosition()) <
          0)
        checkRangeIterator.advance(TextIterator::rangeLength(
            chunkRange.endPosition(), checkRange.endPosition()));
    }
  }
}

static void addMarker(Document* document,
                      const EphemeralRange& checkingRange,
                      DocumentMarker::MarkerType type,
                      int location,
                      int length,
                      const String& description,
                      uint32_t hash) {
  DCHECK_GT(length, 0);
  DCHECK_GE(location, 0);
  const EphemeralRange& rangeToMark =
      calculateCharacterSubrange(checkingRange, location, length);
  document->markers().addMarker(rangeToMark.startPosition(),
                                rangeToMark.endPosition(), type, description,
                                hash);
}

void SpellChecker::markAndReplaceFor(
    SpellCheckRequest* request,
    const Vector<TextCheckingResult>& results) {
  TRACE_EVENT0("blink", "SpellChecker::markAndReplaceFor");
  DCHECK(request);
  if (!frame().selection().isAvailable()) {
    // "editing/spelling/spellcheck-async-remove-frame.html" reaches here.
    return;
  }
  if (!request->isValid())
    return;
  if (request->rootEditableElement()->document() !=
      frame().selection().document()) {
    // we ignore |request| made for another document.
    // "editing/spelling/spellcheck-sequencenum.html" and others reach here.
    return;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame().document()->lifecycle());

  TextCheckingParagraph paragraph(request->checkingRange(),
                                  request->checkingRange());

  // TODO(xiaochengh): The following comment does not match the current behavior
  // and should be rewritten.
  // Expand the range to encompass entire paragraphs, since text checking needs
  // that much context.
  int selectionOffset = 0;
  int ambiguousBoundaryOffset = -1;

  if (frame().selection().isCaret()) {
    // TODO(xiaochengh): The following comment does not match the current
    // behavior and should be rewritten.
    // Attempt to save the caret position so we can restore it later if needed
    const Position& caretPosition = frame().selection().end();
    selectionOffset = paragraph.offsetTo(caretPosition);
    if (selectionOffset > 0 &&
        static_cast<unsigned>(selectionOffset) <= paragraph.text().length() &&
        isAmbiguousBoundaryCharacter(
            paragraph.textCharAt(selectionOffset - 1))) {
      ambiguousBoundaryOffset = selectionOffset - 1;
    }
  }

  const int spellingRangeEndOffset = paragraph.checkingEnd();
  for (const TextCheckingResult& result : results) {
    const int resultLocation = result.location + paragraph.checkingStart();
    const int resultLength = result.length;
    const bool resultEndsAtAmbiguousBoundary =
        ambiguousBoundaryOffset >= 0 &&
        resultLocation + resultLength == ambiguousBoundaryOffset;

    // Only mark misspelling if:
    // 1. Result falls within spellingRange.
    // 2. The word in question doesn't end at an ambiguous boundary. For
    //    instance, we would not mark "wouldn'" as misspelled right after
    //    apostrophe is typed.
    switch (result.decoration) {
      case TextDecorationTypeSpelling:
        if (resultLocation < paragraph.checkingStart() ||
            resultLocation + resultLength > spellingRangeEndOffset ||
            resultEndsAtAmbiguousBoundary)
          continue;
        addMarker(frame().document(), paragraph.checkingRange(),
                  DocumentMarker::Spelling, resultLocation, resultLength,
                  result.replacement, result.hash);
        continue;

      case TextDecorationTypeGrammar:
        if (!paragraph.checkingRangeCovers(resultLocation, resultLength))
          continue;
        DCHECK_GT(resultLength, 0);
        DCHECK_GE(resultLocation, 0);
        for (const GrammarDetail& detail : result.details) {
          DCHECK_GT(detail.length, 0);
          DCHECK_GE(detail.location, 0);
          if (!paragraph.checkingRangeCovers(resultLocation + detail.location,
                                             detail.length))
            continue;
          addMarker(frame().document(), paragraph.checkingRange(),
                    DocumentMarker::Grammar, resultLocation + detail.location,
                    detail.length, result.replacement, result.hash);
        }
        continue;

      case TextDecorationTypeInvisibleSpellcheck:
        if (resultLocation < paragraph.checkingStart() ||
            resultLocation + resultLength > spellingRangeEndOffset)
          continue;
        addMarker(frame().document(), paragraph.checkingRange(),
                  DocumentMarker::InvisibleSpellcheck, resultLocation,
                  resultLength, result.replacement, result.hash);
        continue;
    }
    NOTREACHED();
  }
}

void SpellChecker::updateMarkersForWordsAffectedByEditing(
    bool doNotRemoveIfSelectionAtWordBoundary) {
  DCHECK(frame().selection().isAvailable());
  TRACE_EVENT0("blink", "SpellChecker::updateMarkersForWordsAffectedByEditing");
  if (!isSpellCheckingEnabledFor(frame().selection().selection()))
    return;

  Document* document = frame().document();
  DCHECK(document);

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document->updateStyleAndLayoutIgnorePendingStylesheets();

  // We want to remove the markers from a word if an editing command will change
  // the word. This can happen in one of several scenarios:
  // 1. Insert in the middle of a word.
  // 2. Appending non whitespace at the beginning of word.
  // 3. Appending non whitespace at the end of word.
  // Note that, appending only whitespaces at the beginning or end of word won't
  // change the word, so we don't need to remove the markers on that word. Of
  // course, if current selection is a range, we potentially will edit two words
  // that fall on the boundaries of selection, and remove words between the
  // selection boundaries.
  VisiblePosition startOfSelection =
      frame().selection().selection().visibleStart();
  VisiblePosition endOfSelection = frame().selection().selection().visibleEnd();
  if (startOfSelection.isNull())
    return;
  // First word is the word that ends after or on the start of selection.
  VisiblePosition startOfFirstWord =
      startOfWord(startOfSelection, LeftWordIfOnBoundary);
  VisiblePosition endOfFirstWord =
      endOfWord(startOfSelection, LeftWordIfOnBoundary);
  // Last word is the word that begins before or on the end of selection
  VisiblePosition startOfLastWord =
      startOfWord(endOfSelection, RightWordIfOnBoundary);
  VisiblePosition endOfLastWord =
      endOfWord(endOfSelection, RightWordIfOnBoundary);

  if (startOfFirstWord.isNull()) {
    startOfFirstWord = startOfWord(startOfSelection, RightWordIfOnBoundary);
    endOfFirstWord = endOfWord(startOfSelection, RightWordIfOnBoundary);
  }

  if (endOfLastWord.isNull()) {
    startOfLastWord = startOfWord(endOfSelection, LeftWordIfOnBoundary);
    endOfLastWord = endOfWord(endOfSelection, LeftWordIfOnBoundary);
  }

  // If doNotRemoveIfSelectionAtWordBoundary is true, and first word ends at the
  // start of selection, we choose next word as the first word.
  if (doNotRemoveIfSelectionAtWordBoundary &&
      endOfFirstWord.deepEquivalent() == startOfSelection.deepEquivalent()) {
    startOfFirstWord = nextWordPosition(startOfFirstWord);
    endOfFirstWord = endOfWord(startOfFirstWord, RightWordIfOnBoundary);
    if (startOfFirstWord.deepEquivalent() == endOfSelection.deepEquivalent())
      return;
  }

  // If doNotRemoveIfSelectionAtWordBoundary is true, and last word begins at
  // the end of selection, we choose previous word as the last word.
  if (doNotRemoveIfSelectionAtWordBoundary &&
      startOfLastWord.deepEquivalent() == endOfSelection.deepEquivalent()) {
    startOfLastWord = previousWordPosition(startOfLastWord);
    endOfLastWord = endOfWord(startOfLastWord, RightWordIfOnBoundary);
    if (endOfLastWord.deepEquivalent() == startOfSelection.deepEquivalent())
      return;
  }

  if (startOfFirstWord.isNull() || endOfFirstWord.isNull() ||
      startOfLastWord.isNull() || endOfLastWord.isNull())
    return;

  const Position& removeMarkerStart = startOfFirstWord.deepEquivalent();
  const Position& removeMarkerEnd = endOfLastWord.deepEquivalent();
  if (removeMarkerStart > removeMarkerEnd) {
    // editing/inserting/insert-br-008.html and more reach here.
    // TODO(yosin): To avoid |DCHECK(removeMarkerStart <= removeMarkerEnd)|
    // in |EphemeralRange| constructor, we have this if-statement. Once we
    // fix |startOfWord()| and |endOfWord()|, we should remove this
    // if-statement.
    return;
  }

  // Now we remove markers on everything between startOfFirstWord and
  // endOfLastWord. However, if an autocorrection change a single word to
  // multiple words, we want to remove correction mark from all the resulted
  // words even we only edit one of them. For example, assuming autocorrection
  // changes "avantgarde" to "avant garde", we will have CorrectionIndicator
  // marker on both words and on the whitespace between them. If we then edit
  // garde, we would like to remove the marker from word "avant" and whitespace
  // as well. So we need to get the continous range of of marker that contains
  // the word in question, and remove marker on that whole range.
  const EphemeralRange wordRange(removeMarkerStart, removeMarkerEnd);
  document->markers().removeMarkers(
      wordRange, DocumentMarker::MisspellingMarkers(),
      DocumentMarkerController::RemovePartiallyOverlappingMarker);
}

void SpellChecker::didEndEditingOnTextField(Element* e) {
  TRACE_EVENT0("blink", "SpellChecker::didEndEditingOnTextField");

  // Remove markers when deactivating a selection in an <input type="text"/>.
  // Prevent new ones from appearing too.
  m_spellCheckRequester->cancelCheck();
  TextControlElement* textControlElement = toTextControlElement(e);
  HTMLElement* innerEditor = textControlElement->innerEditorElement();
  DocumentMarker::MarkerTypes markerTypes(DocumentMarker::Spelling);
  markerTypes.add(DocumentMarker::Grammar);
  for (Node& node : NodeTraversal::inclusiveDescendantsOf(*innerEditor))
    frame().document()->markers().removeMarkers(&node, markerTypes);
}

void SpellChecker::replaceMisspelledRange(const String& text) {
  EphemeralRange caretRange =
      frame().selection().selection().toNormalizedEphemeralRange();
  if (caretRange.isNull())
    return;
  DocumentMarkerVector markers = frame().document()->markers().markersInRange(
      caretRange, DocumentMarker::MisspellingMarkers());
  if (markers.size() < 1 ||
      markers[0]->startOffset() >= markers[0]->endOffset())
    return;
  EphemeralRange markerRange =
      EphemeralRange(Position(caretRange.startPosition().computeContainerNode(),
                              markers[0]->startOffset()),
                     Position(caretRange.endPosition().computeContainerNode(),
                              markers[0]->endOffset()));
  if (markerRange.isNull())
    return;
  frame().selection().setSelection(
      SelectionInDOMTree::Builder().setBaseAndExtent(markerRange).build());

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  frame().editor().replaceSelectionWithText(text, false, false);
}

static bool shouldCheckOldSelection(const Position& oldSelectionStart) {
  if (!oldSelectionStart.isConnected())
    return false;
  if (isPositionInTextField(oldSelectionStart))
    return false;
  if (isPositionInTextArea(oldSelectionStart))
    return true;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term we should use idle time spell checker to prevent
  // synchronous layout caused by spell checking (see crbug.com/517298).
  oldSelectionStart.document()->updateStyleAndLayoutIgnorePendingStylesheets();

  return isEditablePosition(oldSelectionStart);
}

void SpellChecker::respondToChangedSelection(
    const Position& oldSelectionStart,
    FrameSelection::SetSelectionOptions options) {
  TRACE_EVENT0("blink", "SpellChecker::respondToChangedSelection");
  if (!isSpellCheckingEnabledFor(oldSelectionStart))
    return;

  // When spell checking is off, existing markers disappear after the selection
  // changes.
  if (!isSpellCheckingEnabled()) {
    frame().document()->markers().removeMarkers(DocumentMarker::Spelling);
    frame().document()->markers().removeMarkers(DocumentMarker::Grammar);
    return;
  }

  if (!(options & FrameSelection::CloseTyping))
    return;
  if (!shouldCheckOldSelection(oldSelectionStart))
    return;

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term we should use idle time spell checker to prevent
  // synchronous layout caused by spell checking (see crbug.com/517298).
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame().document()->lifecycle());

  VisibleSelection newAdjacentWords;
  const VisibleSelection newSelection = frame().selection().selection();
  if (isSelectionInTextFormControl(newSelection)) {
    const Position newStart = newSelection.start();
    newAdjacentWords.setWithoutValidation(
        TextControlElement::startOfWord(newStart),
        TextControlElement::endOfWord(newStart));
  } else {
    if (newSelection.isContentEditable()) {
      newAdjacentWords =
          createVisibleSelection(selectWord(newSelection.visibleStart()));
    }
  }

  // When typing we check spelling elsewhere, so don't redo it here.
  // If this is a change in selection resulting from a delete operation,
  // oldSelection may no longer be in the document.
  // FIXME(http://crbug.com/382809): if oldSelection is on a textarea
  // element, we cause synchronous layout.
  spellCheckOldSelection(oldSelectionStart, newAdjacentWords);
}

void SpellChecker::removeSpellingMarkers() {
  frame().document()->markers().removeMarkers(
      DocumentMarker::MisspellingMarkers());
}

void SpellChecker::removeSpellingMarkersUnderWords(
    const Vector<String>& words) {
  MarkerRemoverPredicate removerPredicate(words);

  DocumentMarkerController& markerController = frame().document()->markers();
  markerController.removeMarkers(removerPredicate);
  markerController.repaintMarkers();
}

void SpellChecker::spellCheckAfterBlur() {
  if (!frame().selection().selection().isContentEditable())
    return;

  if (isPositionInTextField(frame().selection().selection().start())) {
    // textFieldDidEndEditing() and textFieldDidBeginEditing() handle this.
    return;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // In the long term we should use idle time spell checker to prevent
  // synchronous layout caused by spell checking (see crbug.com/517298).
  frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      frame().document()->lifecycle());

  VisibleSelection empty;
  spellCheckOldSelection(frame().selection().selection().start(), empty);
}

void SpellChecker::spellCheckOldSelection(
    const Position& oldSelectionStart,
    const VisibleSelection& newAdjacentWords) {
  if (!isSpellCheckingEnabled())
    return;

  TRACE_EVENT0("blink", "SpellChecker::spellCheckOldSelection");

  VisiblePosition oldStart = createVisiblePosition(oldSelectionStart);
  VisibleSelection oldAdjacentWords =
      createVisibleSelection(selectWord(oldStart));
  if (oldAdjacentWords == newAdjacentWords)
    return;
  markMisspellingsInternal(oldAdjacentWords);
}

static Node* findFirstMarkable(Node* node) {
  while (node) {
    if (!node->layoutObject())
      return 0;
    if (node->layoutObject()->isText())
      return node;
    if (node->layoutObject()->isTextControl())
      node = toLayoutTextControl(node->layoutObject())
                 ->textControlElement()
                 ->visiblePositionForIndex(1)
                 .deepEquivalent()
                 .anchorNode();
    else if (node->hasChildren())
      node = node->firstChild();
    else
      node = node->nextSibling();
  }

  return 0;
}

bool SpellChecker::selectionStartHasMarkerFor(
    DocumentMarker::MarkerType markerType,
    int from,
    int length) const {
  Node* node = findFirstMarkable(frame().selection().start().anchorNode());
  if (!node)
    return false;

  unsigned startOffset = static_cast<unsigned>(from);
  unsigned endOffset = static_cast<unsigned>(from + length);
  DocumentMarkerVector markers = frame().document()->markers().markersFor(node);
  for (size_t i = 0; i < markers.size(); ++i) {
    DocumentMarker* marker = markers[i];
    if (marker->startOffset() <= startOffset &&
        endOffset <= marker->endOffset() && marker->type() == markerType)
      return true;
  }

  return false;
}

bool SpellChecker::selectionStartHasSpellingMarkerFor(int from,
                                                      int length) const {
  return selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

void SpellChecker::removeMarkers(const VisibleSelection& selection,
                                 DocumentMarker::MarkerTypes markerTypes) {
  DCHECK(!frame().document()->needsLayoutTreeUpdate());

  const EphemeralRange& range = selection.toNormalizedEphemeralRange();
  if (range.isNull())
    return;

  frame().document()->markers().removeMarkers(range, markerTypes);
}

void SpellChecker::cancelCheck() {
  m_spellCheckRequester->cancelCheck();
}

void SpellChecker::requestTextChecking(const Element& element) {
  if (!element.isSpellCheckingEnabled())
    return;
  const EphemeralRange rangeToCheck = EphemeralRange::rangeOfContents(element);
  m_spellCheckRequester->requestCheckingFor(
      SpellCheckRequest::create(rangeToCheck));
}

DEFINE_TRACE(SpellChecker) {
  visitor->trace(m_frame);
  visitor->trace(m_spellCheckRequester);
}

void SpellChecker::prepareForLeakDetection() {
  m_spellCheckRequester->prepareForLeakDetection();
}

Vector<TextCheckingResult> SpellChecker::findMisspellings(const String& text) {
  Vector<UChar> characters;
  text.appendTo(characters);
  unsigned length = text.length();

  TextBreakIterator* iterator = wordBreakIterator(characters.data(), length);
  if (!iterator)
    return Vector<TextCheckingResult>();

  Vector<TextCheckingResult> results;
  int wordStart = iterator->current();
  while (wordStart >= 0) {
    int wordEnd = iterator->next();
    if (wordEnd < 0)
      break;
    int wordLength = wordEnd - wordStart;
    int misspellingLocation = -1;
    int misspellingLength = 0;
    textChecker().checkSpellingOfString(
        String(characters.data() + wordStart, wordLength), &misspellingLocation,
        &misspellingLength);
    if (misspellingLength > 0) {
      DCHECK_GE(misspellingLocation, 0);
      DCHECK_LE(misspellingLocation + misspellingLength, wordLength);
      TextCheckingResult misspelling;
      misspelling.decoration = TextDecorationTypeSpelling;
      misspelling.location = wordStart + misspellingLocation;
      misspelling.length = misspellingLength;
      results.append(misspelling);
    }
    wordStart = wordEnd;
  }
  return results;
}

std::pair<String, int> SpellChecker::findFirstMisspelling(const Position& start,
                                                          const Position& end) {
  String misspelledWord;

  // Initialize out parameters; they will be updated if we find something to
  // return.
  String firstFoundItem;
  int firstFoundOffset = 0;

  // Expand the search range to encompass entire paragraphs, since text checking
  // needs that much context. Determine the character offset from the start of
  // the paragraph to the start of the original search range, since we will want
  // to ignore results in this area.
  Position paragraphStart =
      startOfParagraph(createVisiblePosition(start)).toParentAnchoredPosition();
  Position paragraphEnd = end;
  int totalRangeLength =
      TextIterator::rangeLength(paragraphStart, paragraphEnd);
  paragraphEnd =
      endOfParagraph(createVisiblePosition(start)).toParentAnchoredPosition();

  int rangeStartOffset = TextIterator::rangeLength(paragraphStart, start);
  int totalLengthProcessed = 0;

  bool firstIteration = true;
  bool lastIteration = false;
  while (totalLengthProcessed < totalRangeLength) {
    // Iterate through the search range by paragraphs, checking each one for
    // spelling.
    int currentLength = TextIterator::rangeLength(paragraphStart, paragraphEnd);
    int currentStartOffset = firstIteration ? rangeStartOffset : 0;
    int currentEndOffset = currentLength;
    if (inSameParagraph(createVisiblePosition(paragraphStart),
                        createVisiblePosition(end))) {
      // Determine the character offset from the end of the original search
      // range to the end of the paragraph, since we will want to ignore results
      // in this area.
      currentEndOffset = TextIterator::rangeLength(paragraphStart, end);
      lastIteration = true;
    }
    if (currentStartOffset < currentEndOffset) {
      String paragraphString =
          plainText(EphemeralRange(paragraphStart, paragraphEnd));
      if (paragraphString.length() > 0) {
        int spellingLocation = 0;

        Vector<TextCheckingResult> results = findMisspellings(paragraphString);

        for (unsigned i = 0; i < results.size(); i++) {
          const TextCheckingResult* result = &results[i];
          if (result->location >= currentStartOffset &&
              result->location + result->length <= currentEndOffset) {
            DCHECK_GT(result->length, 0);
            DCHECK_GE(result->location, 0);
            spellingLocation = result->location;
            misspelledWord =
                paragraphString.substring(result->location, result->length);
            DCHECK(misspelledWord.length());
            break;
          }
        }

        if (!misspelledWord.isEmpty()) {
          int spellingOffset = spellingLocation - currentStartOffset;
          if (!firstIteration)
            spellingOffset += TextIterator::rangeLength(start, paragraphStart);
          firstFoundOffset = spellingOffset;
          firstFoundItem = misspelledWord;
          break;
        }
      }
    }
    if (lastIteration ||
        totalLengthProcessed + currentLength >= totalRangeLength)
      break;
    VisiblePosition newParagraphStart =
        startOfNextParagraph(createVisiblePosition(paragraphEnd));
    paragraphStart = newParagraphStart.toParentAnchoredPosition();
    paragraphEnd = endOfParagraph(newParagraphStart).toParentAnchoredPosition();
    firstIteration = false;
    totalLengthProcessed += currentLength;
  }
  return std::make_pair(firstFoundItem, firstFoundOffset);
}

}  // namespace blink
