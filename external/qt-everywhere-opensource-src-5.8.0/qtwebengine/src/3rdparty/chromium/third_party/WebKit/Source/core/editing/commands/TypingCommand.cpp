/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc.  All rights reserved.
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

#include "core/editing/commands/TypingCommand.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/SelectionModifier.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/BreakBlockquoteCommand.h"
#include "core/editing/commands/InsertLineBreakCommand.h"
#include "core/editing/commands/InsertParagraphSeparatorCommand.h"
#include "core/editing/commands/InsertTextCommand.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBRElement.h"
#include "core/layout/LayoutObject.h"

namespace blink {

using namespace HTMLNames;

TypingCommand::TypingCommand(Document& document, ETypingCommand commandType, const String &textToInsert, Options options, TextGranularity granularity, TextCompositionType compositionType)
    : CompositeEditCommand(document)
    , m_commandType(commandType)
    , m_textToInsert(textToInsert)
    , m_openForMoreTyping(true)
    , m_selectInsertedText(options & SelectInsertedText)
    , m_smartDelete(options & SmartDelete)
    , m_granularity(granularity)
    , m_compositionType(compositionType)
    , m_killRing(options & KillRing)
    , m_openedByBackwardDelete(false)
    , m_shouldRetainAutocorrectionIndicator(options & RetainAutocorrectionIndicator)
    , m_shouldPreventSpellChecking(options & PreventSpellChecking)
{
    updatePreservesTypingStyle(m_commandType);
}

void TypingCommand::deleteSelection(Document& document, Options options)
{
    LocalFrame* frame = document.frame();
    DCHECK(frame);

    if (!frame->selection().isRange())
        return;

    if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(frame)) {
        updateSelectionIfDifferentFromCurrentSelection(lastTypingCommand, frame);

        lastTypingCommand->setShouldPreventSpellChecking(options & PreventSpellChecking);
        // InputMethodController uses this function to delete composition
        // selection.  It won't be aborted.
        lastTypingCommand->deleteSelection(options & SmartDelete, ASSERT_NO_EDITING_ABORT);
        return;
    }

    TypingCommand::create(document, DeleteSelection, "", options)->apply();
}

void TypingCommand::deleteKeyPressed(Document& document, Options options, TextGranularity granularity)
{
    if (granularity == CharacterGranularity) {
        LocalFrame* frame = document.frame();
        if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(frame)) {
            // If the last typing command is not Delete, open a new typing command.
            // We need to group continuous delete commands alone in a single typing command.
            if (lastTypingCommand->commandTypeOfOpenCommand() == DeleteKey) {
                updateSelectionIfDifferentFromCurrentSelection(lastTypingCommand, frame);
                lastTypingCommand->setShouldPreventSpellChecking(options & PreventSpellChecking);
                EditingState editingState;
                lastTypingCommand->deleteKeyPressed(granularity, options & KillRing, &editingState);
                return;
            }
        }
    }

    TypingCommand::create(document, DeleteKey, "", options, granularity)->apply();
}

void TypingCommand::forwardDeleteKeyPressed(Document& document, EditingState* editingState, Options options, TextGranularity granularity)
{
    // FIXME: Forward delete in TextEdit appears to open and close a new typing command.
    if (granularity == CharacterGranularity) {
        LocalFrame* frame = document.frame();
        if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(frame)) {
            updateSelectionIfDifferentFromCurrentSelection(lastTypingCommand, frame);
            lastTypingCommand->setShouldPreventSpellChecking(options & PreventSpellChecking);
            lastTypingCommand->forwardDeleteKeyPressed(granularity, options & KillRing, editingState);
            return;
        }
    }

    TypingCommand::create(document, ForwardDeleteKey, "", options, granularity)->apply();
}

void TypingCommand::updateSelectionIfDifferentFromCurrentSelection(TypingCommand* typingCommand, LocalFrame* frame)
{
    DCHECK(frame);
    VisibleSelection currentSelection = frame->selection().selection();
    if (currentSelection == typingCommand->endingSelection())
        return;

    typingCommand->setStartingSelection(currentSelection);
    typingCommand->setEndingSelection(currentSelection);
}

static String dispatchBeforeTextInsertedEvent(const String& text, const VisibleSelection& selectionForInsertion, bool insertionIsForUpdatingComposition)
{
    if (insertionIsForUpdatingComposition)
        return text;

    String newText = text;
    if (Node* startNode = selectionForInsertion.start().computeContainerNode()) {
        if (startNode->rootEditableElement()) {
            // Send BeforeTextInsertedEvent. The event handler will update text if necessary.
            BeforeTextInsertedEvent* evt = BeforeTextInsertedEvent::create(text);
            startNode->rootEditableElement()->dispatchEvent(evt);
            newText = evt->text();
        }
    }
    return newText;
}

void TypingCommand::insertText(Document& document, const String& text, Options options, TextCompositionType composition)
{
    LocalFrame* frame = document.frame();
    DCHECK(frame);

    if (!text.isEmpty())
        document.frame()->spellChecker().updateMarkersForWordsAffectedByEditing(isSpaceOrNewline(text[0]));

    insertText(document, text, frame->selection().selection(), options, composition);
}

// FIXME: We shouldn't need to take selectionForInsertion. It should be identical to FrameSelection's current selection.
void TypingCommand::insertText(Document& document, const String& text, const VisibleSelection& selectionForInsertion, Options options, TextCompositionType compositionType)
{
    LocalFrame* frame = document.frame();
    DCHECK(frame);

    VisibleSelection currentSelection = frame->selection().selection();

    String newText = dispatchBeforeTextInsertedEvent(text, selectionForInsertion, compositionType == TextCompositionUpdate);

    // Set the starting and ending selection appropriately if we are using a selection
    // that is different from the current selection.  In the future, we should change EditCommand
    // to deal with custom selections in a general way that can be used by all of the commands.
    if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(frame)) {
        if (lastTypingCommand->endingSelection() != selectionForInsertion) {
            lastTypingCommand->setStartingSelection(selectionForInsertion);
            lastTypingCommand->setEndingSelection(selectionForInsertion);
        }

        lastTypingCommand->setCompositionType(compositionType);
        lastTypingCommand->setShouldRetainAutocorrectionIndicator(options & RetainAutocorrectionIndicator);
        lastTypingCommand->setShouldPreventSpellChecking(options & PreventSpellChecking);
        EditingState editingState;
        lastTypingCommand->insertText(newText, options & SelectInsertedText, &editingState);
        // Nothing to do even if the command was aborted.
        return;
    }

    TypingCommand* command = TypingCommand::create(document, InsertText, newText, options, compositionType);
    bool changeSelection = selectionForInsertion != currentSelection;
    if (changeSelection) {
        command->setStartingSelection(selectionForInsertion);
        command->setEndingSelection(selectionForInsertion);
    }
    command->apply();
    if (changeSelection) {
        command->setEndingSelection(currentSelection);
        frame->selection().setSelection(currentSelection);
    }
}

bool TypingCommand::insertLineBreak(Document& document)
{
    if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document.frame())) {
        lastTypingCommand->setShouldRetainAutocorrectionIndicator(false);
        EditingState editingState;
        lastTypingCommand->insertLineBreak(&editingState);
        return !editingState.isAborted();
    }

    return TypingCommand::create(document, InsertLineBreak, "", 0)->apply();
}

bool TypingCommand::insertParagraphSeparatorInQuotedContent(Document& document)
{
    if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document.frame())) {
        EditingState editingState;
        lastTypingCommand->insertParagraphSeparatorInQuotedContent(&editingState);
        return !editingState.isAborted();
    }

    return TypingCommand::create(document, InsertParagraphSeparatorInQuotedContent)->apply();
}

bool TypingCommand::insertParagraphSeparator(Document& document)
{
    if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(document.frame())) {
        lastTypingCommand->setShouldRetainAutocorrectionIndicator(false);
        EditingState editingState;
        lastTypingCommand->insertParagraphSeparator(&editingState);
        return !editingState.isAborted();
    }

    return TypingCommand::create(document, InsertParagraphSeparator, "", 0)->apply();
}

TypingCommand* TypingCommand::lastTypingCommandIfStillOpenForTyping(LocalFrame* frame)
{
    DCHECK(frame);

    CompositeEditCommand* lastEditCommand = frame->editor().lastEditCommand();
    if (!lastEditCommand || !lastEditCommand->isTypingCommand() || !static_cast<TypingCommand*>(lastEditCommand)->isOpenForMoreTyping())
        return nullptr;

    return static_cast<TypingCommand*>(lastEditCommand);
}

void TypingCommand::closeTyping(LocalFrame* frame)
{
    if (TypingCommand* lastTypingCommand = lastTypingCommandIfStillOpenForTyping(frame))
        lastTypingCommand->closeTyping();
}

void TypingCommand::doApply(EditingState* editingState)
{
    if (!endingSelection().isNonOrphanedCaretOrRange())
        return;

    if (m_commandType == DeleteKey) {
        if (m_commands.isEmpty())
            m_openedByBackwardDelete = true;
    }

    switch (m_commandType) {
    case DeleteSelection:
        deleteSelection(m_smartDelete, editingState);
        return;
    case DeleteKey:
        deleteKeyPressed(m_granularity, m_killRing, editingState);
        return;
    case ForwardDeleteKey:
        forwardDeleteKeyPressed(m_granularity, m_killRing, editingState);
        return;
    case InsertLineBreak:
        insertLineBreak(editingState);
        return;
    case InsertParagraphSeparator:
        insertParagraphSeparator(editingState);
        return;
    case InsertParagraphSeparatorInQuotedContent:
        insertParagraphSeparatorInQuotedContent(editingState);
        return;
    case InsertText:
        insertText(m_textToInsert, m_selectInsertedText, editingState);
        return;
    }

    NOTREACHED();
}

EditAction TypingCommand::editingAction() const
{
    return EditActionTyping;
}

void TypingCommand::markMisspellingsAfterTyping(ETypingCommand commandType)
{
    LocalFrame* frame = document().frame();
    if (!frame)
        return;

    if (!frame->spellChecker().isContinuousSpellCheckingEnabled())
        return;
    if (!SpellChecker::isSpellCheckingEnabledFor(endingSelection()))
        return;

    frame->spellChecker().cancelCheck();

    // Take a look at the selection that results after typing and determine whether we need to spellcheck.
    // Since the word containing the current selection is never marked, this does a check to
    // see if typing made a new word that is not in the current selection. Basically, you
    // get this by being at the end of a word and typing a space.
    VisiblePosition start = createVisiblePosition(endingSelection().start(), endingSelection().affinity());
    VisiblePosition previous = previousPositionOf(start);

    VisiblePosition p1 = startOfWord(previous, LeftWordIfOnBoundary);

    if (commandType == InsertParagraphSeparator) {
        VisiblePosition p2 = nextWordPosition(start);
        VisibleSelection words(p1, endOfWord(p2));
        frame->spellChecker().markMisspellingsAfterLineBreak(words);
    } else if (previous.isNotNull()) {
        VisiblePosition p2 = startOfWord(start, LeftWordIfOnBoundary);
        if (p1.deepEquivalent() != p2.deepEquivalent())
            frame->spellChecker().markMisspellingsAfterTypingToWord(p1, endingSelection());
    }
}

void TypingCommand::typingAddedToOpenCommand(ETypingCommand commandTypeForAddedTyping)
{
    LocalFrame* frame = document().frame();
    if (!frame)
        return;

    updatePreservesTypingStyle(commandTypeForAddedTyping);
    updateCommandTypeOfOpenCommand(commandTypeForAddedTyping);

    // The old spellchecking code requires that checking be done first, to prevent issues like that in 6864072, where <doesn't> is marked as misspelled.
    markMisspellingsAfterTyping(commandTypeForAddedTyping);
    frame->editor().appliedEditing(this);
}

void TypingCommand::insertText(const String &text, bool selectInsertedText, EditingState* editingState)
{
    if (text.isEmpty()) {
        insertTextRunWithoutNewlines(text, selectInsertedText, editingState);
        return;
    }
    // FIXME: Need to implement selectInsertedText for cases where more than one insert is involved.
    // This requires support from insertTextRunWithoutNewlines and insertParagraphSeparator for extending
    // an existing selection; at the moment they can either put the caret after what's inserted or
    // select what's inserted, but there's no way to "extend selection" to include both an old selection
    // that ends just before where we want to insert text and the newly inserted text.
    unsigned offset = 0;
    size_t newline;
    while ((newline = text.find('\n', offset)) != kNotFound) {
        if (newline > offset) {
            const bool notSelectInsertedText = false;
            insertTextRunWithoutNewlines(text.substring(offset, newline - offset), notSelectInsertedText, editingState);
            if (editingState->isAborted())
                return;
        }

        insertParagraphSeparator(editingState);
        if (editingState->isAborted())
            return;

        offset = newline + 1;
    }

    if (!offset) {
        insertTextRunWithoutNewlines(text, selectInsertedText, editingState);
        return;
    }

    if (text.length() > offset)
        insertTextRunWithoutNewlines(text.substring(offset, text.length() - offset), selectInsertedText, editingState);

}

void TypingCommand::insertTextRunWithoutNewlines(const String &text, bool selectInsertedText, EditingState* editingState)
{
    InsertTextCommand* command = InsertTextCommand::create(document(), text, selectInsertedText,
        m_compositionType == TextCompositionNone ? InsertTextCommand::RebalanceLeadingAndTrailingWhitespaces : InsertTextCommand::RebalanceAllWhitespaces);

    applyCommandToComposite(command, endingSelection(), editingState);
    if (editingState->isAborted())
        return;
    typingAddedToOpenCommand(InsertText);
}

static bool canAppendNewLineFeedToSelection(const VisibleSelection& selection)
{
    Element* element = selection.rootEditableElement();
    if (!element)
        return false;

    BeforeTextInsertedEvent* event = BeforeTextInsertedEvent::create(String("\n"));
    element->dispatchEvent(event);
    return event->text().length();
}

void TypingCommand::insertLineBreak(EditingState* editingState)
{
    if (!canAppendNewLineFeedToSelection(endingSelection()))
        return;

    applyCommandToComposite(InsertLineBreakCommand::create(document()), editingState);
    if (editingState->isAborted())
        return;
    typingAddedToOpenCommand(InsertLineBreak);
}

void TypingCommand::insertParagraphSeparator(EditingState* editingState)
{
    if (!canAppendNewLineFeedToSelection(endingSelection()))
        return;

    applyCommandToComposite(InsertParagraphSeparatorCommand::create(document()), editingState);
    if (editingState->isAborted())
        return;
    typingAddedToOpenCommand(InsertParagraphSeparator);
}

void TypingCommand::insertParagraphSeparatorInQuotedContent(EditingState* editingState)
{
    // If the selection starts inside a table, just insert the paragraph separator normally
    // Breaking the blockquote would also break apart the table, which is unecessary when inserting a newline
    if (enclosingNodeOfType(endingSelection().start(), &isTableStructureNode)) {
        insertParagraphSeparator(editingState);
        return;
    }

    applyCommandToComposite(BreakBlockquoteCommand::create(document()), editingState);
    if (editingState->isAborted())
        return;
    typingAddedToOpenCommand(InsertParagraphSeparatorInQuotedContent);
}

bool TypingCommand::makeEditableRootEmpty(EditingState* editingState)
{
    Element* root = endingSelection().rootEditableElement();
    if (!root || !root->hasChildren())
        return false;

    if (root->firstChild() == root->lastChild()) {
        if (isHTMLBRElement(root->firstChild())) {
            // If there is a single child and it could be a placeholder, leave it alone.
            if (root->layoutObject() && root->layoutObject()->isLayoutBlockFlow())
                return false;
        }
    }

    while (Node* child = root->firstChild()) {
        removeNode(child, editingState);
        if (editingState->isAborted())
            return false;
    }

    addBlockPlaceholderIfNeeded(root, editingState);
    if (editingState->isAborted())
        return false;
    setEndingSelection(VisibleSelection(Position::firstPositionInNode(root), TextAffinity::Downstream, endingSelection().isDirectional()));

    return true;
}

void TypingCommand::deleteKeyPressed(TextGranularity granularity, bool killRing, EditingState* editingState)
{
    LocalFrame* frame = document().frame();
    if (!frame)
        return;

    frame->spellChecker().updateMarkersForWordsAffectedByEditing(false);

    VisibleSelection selectionToDelete;
    VisibleSelection selectionAfterUndo;

    switch (endingSelection().getSelectionType()) {
    case RangeSelection:
        selectionToDelete = endingSelection();
        selectionAfterUndo = selectionToDelete;
        break;
    case CaretSelection: {
        // After breaking out of an empty mail blockquote, we still want continue with the deletion
        // so actual content will get deleted, and not just the quote style.
        bool breakOutResult = breakOutOfEmptyMailBlockquotedParagraph(editingState);
        if (editingState->isAborted())
            return;
        if (breakOutResult)
            typingAddedToOpenCommand(DeleteKey);

        m_smartDelete = false;

        SelectionModifier selectionModifier(*frame, endingSelection());
        selectionModifier.modify(FrameSelection::AlterationExtend, DirectionBackward, granularity);
        if (killRing && selectionModifier.selection().isCaret() && granularity != CharacterGranularity)
            selectionModifier.modify(FrameSelection::AlterationExtend, DirectionBackward, CharacterGranularity);

        VisiblePosition visibleStart(endingSelection().visibleStart());
        if (previousPositionOf(visibleStart, CannotCrossEditingBoundary).isNull()) {
            // When the caret is at the start of the editable area in an empty list item, break out of the list item.
            bool breakOutOfEmptyListItemResult = breakOutOfEmptyListItem(editingState);
            if (editingState->isAborted())
                return;
            if (breakOutOfEmptyListItemResult) {
                typingAddedToOpenCommand(DeleteKey);
                return;
            }
            // When there are no visible positions in the editing root, delete its entire contents.
            if (nextPositionOf(visibleStart, CannotCrossEditingBoundary).isNull() && makeEditableRootEmpty(editingState)) {
                typingAddedToOpenCommand(DeleteKey);
                return;
            }
            if (editingState->isAborted())
                return;
        }

        // If we have a caret selection at the beginning of a cell, we have nothing to do.
        Node* enclosingTableCell = enclosingNodeOfType(visibleStart.deepEquivalent(), &isTableCell);
        if (enclosingTableCell && visibleStart.deepEquivalent() == VisiblePosition::firstPositionInNode(enclosingTableCell).deepEquivalent())
            return;

        // If the caret is at the start of a paragraph after a table, move content into the last table cell.
        if (isStartOfParagraph(visibleStart) && tableElementJustBefore(previousPositionOf(visibleStart, CannotCrossEditingBoundary))) {
            // Unless the caret is just before a table.  We don't want to move a table into the last table cell.
            if (tableElementJustAfter(visibleStart))
                return;
            // Extend the selection backward into the last cell, then deletion will handle the move.
            selectionModifier.modify(FrameSelection::AlterationExtend, DirectionBackward, granularity);
        // If the caret is just after a table, select the table and don't delete anything.
        } else if (Element* table = tableElementJustBefore(visibleStart)) {
            setEndingSelection(VisibleSelection(Position::beforeNode(table), endingSelection().start(), TextAffinity::Downstream, endingSelection().isDirectional()));
            typingAddedToOpenCommand(DeleteKey);
            return;
        }

        selectionToDelete = selectionModifier.selection();

        if (granularity == CharacterGranularity && selectionToDelete.end().computeContainerNode() == selectionToDelete.start().computeContainerNode()
            && selectionToDelete.end().computeOffsetInContainerNode() - selectionToDelete.start().computeOffsetInContainerNode() > 1) {
            // If there are multiple Unicode code points to be deleted, adjust the range to match platform conventions.
            selectionToDelete.setWithoutValidation(selectionToDelete.end(), previousPositionOf(selectionToDelete.end(), PositionMoveType::BackwardDeletion));
        }

        if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start()) {
            selectionAfterUndo = selectionToDelete;
        } else {
            // It's a little tricky to compute what the starting selection would have been in the original document.
            // We can't let the VisibleSelection class's validation kick in or it'll adjust for us based on
            // the current state of the document and we'll get the wrong result.
            selectionAfterUndo.setWithoutValidation(startingSelection().end(), selectionToDelete.extent());
        }
        break;
    }
    case NoSelection:
        NOTREACHED();
        break;
    }

    DCHECK(!selectionToDelete.isNone());
    if (selectionToDelete.isNone())
        return;

    if (selectionToDelete.isCaret())
        return;

    if (killRing)
        frame->editor().addToKillRing(selectionToDelete.toNormalizedEphemeralRange());
    // On Mac, make undo select everything that has been deleted, unless an undo will undo more than just this deletion.
    // FIXME: This behaves like TextEdit except for the case where you open with text insertion and then delete
    // more text than you insert.  In that case all of the text that was around originally should be selected.
    if (frame->editor().behavior().shouldUndoOfDeleteSelectText() && m_openedByBackwardDelete)
        setStartingSelection(selectionAfterUndo);
    CompositeEditCommand::deleteSelection(selectionToDelete, editingState, m_smartDelete);
    if (editingState->isAborted())
        return;
    setSmartDelete(false);
    typingAddedToOpenCommand(DeleteKey);
}

void TypingCommand::forwardDeleteKeyPressed(TextGranularity granularity, bool killRing, EditingState* editingState)
{
    LocalFrame* frame = document().frame();
    if (!frame)
        return;

    frame->spellChecker().updateMarkersForWordsAffectedByEditing(false);

    VisibleSelection selectionToDelete;
    VisibleSelection selectionAfterUndo;

    switch (endingSelection().getSelectionType()) {
    case RangeSelection:
        selectionToDelete = endingSelection();
        selectionAfterUndo = selectionToDelete;
        break;
    case CaretSelection: {
        m_smartDelete = false;

        // Handle delete at beginning-of-block case.
        // Do nothing in the case that the caret is at the start of a
        // root editable element or at the start of a document.
        SelectionModifier selectionModifier(*frame, endingSelection());
        selectionModifier.modify(FrameSelection::AlterationExtend, DirectionForward, granularity);
        if (killRing && selectionModifier.selection().isCaret() && granularity != CharacterGranularity)
            selectionModifier.modify(FrameSelection::AlterationExtend, DirectionForward, CharacterGranularity);

        Position downstreamEnd = mostForwardCaretPosition(endingSelection().end());
        VisiblePosition visibleEnd = endingSelection().visibleEnd();
        Node* enclosingTableCell = enclosingNodeOfType(visibleEnd.deepEquivalent(), &isTableCell);
        if (enclosingTableCell && visibleEnd.deepEquivalent() == VisiblePosition::lastPositionInNode(enclosingTableCell).deepEquivalent())
            return;
        if (visibleEnd.deepEquivalent() == endOfParagraph(visibleEnd).deepEquivalent())
            downstreamEnd = mostForwardCaretPosition(nextPositionOf(visibleEnd, CannotCrossEditingBoundary).deepEquivalent());
        // When deleting tables: Select the table first, then perform the deletion
        if (isDisplayInsideTable(downstreamEnd.computeContainerNode()) && downstreamEnd.computeOffsetInContainerNode() <= caretMinOffset(downstreamEnd.computeContainerNode())) {
            setEndingSelection(VisibleSelection(endingSelection().end(), Position::afterNode(downstreamEnd.computeContainerNode()), TextAffinity::Downstream, endingSelection().isDirectional()));
            typingAddedToOpenCommand(ForwardDeleteKey);
            return;
        }

        // deleting to end of paragraph when at end of paragraph needs to merge the next paragraph (if any)
        if (granularity == ParagraphBoundary && selectionModifier.selection().isCaret() && isEndOfParagraph(selectionModifier.selection().visibleEnd()))
            selectionModifier.modify(FrameSelection::AlterationExtend, DirectionForward, CharacterGranularity);

        selectionToDelete = selectionModifier.selection();
        if (!startingSelection().isRange() || selectionToDelete.base() != startingSelection().start()) {
            selectionAfterUndo = selectionToDelete;
        } else {
            // It's a little tricky to compute what the starting selection would have been in the original document.
            // We can't let the VisibleSelection class's validation kick in or it'll adjust for us based on
            // the current state of the document and we'll get the wrong result.
            Position extent = startingSelection().end();
            if (extent.computeContainerNode() != selectionToDelete.end().computeContainerNode()) {
                extent = selectionToDelete.extent();
            } else {
                int extraCharacters;
                if (selectionToDelete.start().computeContainerNode() == selectionToDelete.end().computeContainerNode())
                    extraCharacters = selectionToDelete.end().computeOffsetInContainerNode() - selectionToDelete.start().computeOffsetInContainerNode();
                else
                    extraCharacters = selectionToDelete.end().computeOffsetInContainerNode();
                extent = Position(extent.computeContainerNode(), extent.computeOffsetInContainerNode() + extraCharacters);
            }
            selectionAfterUndo.setWithoutValidation(startingSelection().start(), extent);
        }
        break;
    }
    case NoSelection:
        NOTREACHED();
        break;
    }

    DCHECK(!selectionToDelete.isNone());
    if (selectionToDelete.isNone())
        return;

    if (selectionToDelete.isCaret())
        return;

    if (killRing)
        frame->editor().addToKillRing(selectionToDelete.toNormalizedEphemeralRange());
    // Make undo select what was deleted on Mac alone
    if (frame->editor().behavior().shouldUndoOfDeleteSelectText())
        setStartingSelection(selectionAfterUndo);
    CompositeEditCommand::deleteSelection(selectionToDelete, editingState, m_smartDelete);
    if (editingState->isAborted())
        return;
    setSmartDelete(false);
    typingAddedToOpenCommand(ForwardDeleteKey);
}

void TypingCommand::deleteSelection(bool smartDelete, EditingState* editingState)
{
    CompositeEditCommand::deleteSelection(editingState, smartDelete);
    if (editingState->isAborted())
        return;
    typingAddedToOpenCommand(DeleteSelection);
}

void TypingCommand::updatePreservesTypingStyle(ETypingCommand commandType)
{
    switch (commandType) {
    case DeleteSelection:
    case DeleteKey:
    case ForwardDeleteKey:
    case InsertParagraphSeparator:
    case InsertLineBreak:
        m_preservesTypingStyle = true;
        return;
    case InsertParagraphSeparatorInQuotedContent:
    case InsertText:
        m_preservesTypingStyle = false;
        return;
    }
    NOTREACHED();
    m_preservesTypingStyle = false;
}

bool TypingCommand::isTypingCommand() const
{
    return true;
}

} // namespace blink
