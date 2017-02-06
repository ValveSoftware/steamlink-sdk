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

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/CompositionEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/ChromeClient.h"
#include "wtf/Optional.h"

namespace blink {

namespace {

void dispatchCompositionUpdateEvent(LocalFrame& frame, const String& text)
{
    Element* target = frame.document()->focusedElement();
    if (!target)
        return;

    CompositionEvent* event = CompositionEvent::create(EventTypeNames::compositionupdate, frame.domWindow(), text);
    target->dispatchEvent(event);
}

void dispatchCompositionEndEvent(LocalFrame& frame, const String& text)
{
    Element* target = frame.document()->focusedElement();
    if (!target)
        return;

    CompositionEvent* event = CompositionEvent::create(EventTypeNames::compositionend, frame.domWindow(), text);
    target->dispatchEvent(event);
}

// Used to insert/replace text during composition update and confirm composition.
// Procedure:
//   1. Fire 'beforeinput' event for (TODO(chongz): deleted composed text) and inserted text
//   2. Fire 'compositionupdate' event
//   3. Fire TextEvent and modify DOM
//   TODO(chongz): 4. Fire 'input' event
void insertTextDuringCompositionWithEvents(LocalFrame& frame, const String& text, TypingCommand::Options options, TypingCommand::TextCompositionType compositionType)
{
    DCHECK(compositionType == TypingCommand::TextCompositionType::TextCompositionUpdate || compositionType == TypingCommand::TextCompositionType::TextCompositionConfirm)
        << "compositionType should be TextCompositionUpdate or TextCompositionConfirm, but got " << static_cast<int>(compositionType);
    if (!frame.document())
        return;

    Element* target = frame.document()->focusedElement();
    if (!target)
        return;

    // TODO(chongz): Fire 'beforeinput' for the composed text being replaced/deleted.

    // Only the last confirmed text is cancelable.
    InputEvent::EventCancelable beforeInputCancelable = (compositionType == TypingCommand::TextCompositionType::TextCompositionUpdate) ? InputEvent::EventCancelable::NotCancelable : InputEvent::EventCancelable::IsCancelable;
    DispatchEventResult result = dispatchBeforeInputFromComposition(target, InputEvent::InputType::InsertText, text, beforeInputCancelable);

    if (beforeInputCancelable == InputEvent::EventCancelable::IsCancelable && result != DispatchEventResult::NotCanceled)
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
        TypingCommand::insertText(*frame.document(), text, options, compositionType);
        break;
    case TypingCommand::TextCompositionType::TextCompositionConfirm:
        // TODO(chongz): Use TypingCommand::insertText after TextEvent was removed. (Removed from spec since 2012)
        // See TextEvent.idl.
        frame.eventHandler().handleTextInputEvent(text, 0, TextEventInputComposition);
        break;
    default:
        NOTREACHED();
    }
    // TODO(chongz): Fire 'input' event.
}

} // anonymous namespace

InputMethodController::SelectionOffsetsScope::SelectionOffsetsScope(InputMethodController* inputMethodController)
    : m_inputMethodController(inputMethodController)
    , m_offsets(inputMethodController->getSelectionOffsets())
{
}

InputMethodController::SelectionOffsetsScope::~SelectionOffsetsScope()
{
    m_inputMethodController->setSelectionOffsets(m_offsets);
}

// ----------------------------

InputMethodController* InputMethodController::create(LocalFrame& frame)
{
    return new InputMethodController(frame);
}

InputMethodController::InputMethodController(LocalFrame& frame)
    : m_frame(&frame)
    , m_isDirty(false)
    , m_hasComposition(false)
{
}

bool InputMethodController::hasComposition() const
{
    return m_hasComposition;
}

inline Editor& InputMethodController::editor() const
{
    return frame().editor();
}

void InputMethodController::clear()
{
    m_hasComposition = false;
    if (m_compositionRange) {
        m_compositionRange->setStart(frame().document(), 0);
        m_compositionRange->collapse(true);
    }
    frame().document()->markers().removeMarkers(DocumentMarker::Composition);
    m_isDirty = false;
}

void InputMethodController::documentDetached()
{
    clear();
    m_compositionRange = nullptr;
}

void InputMethodController::selectComposition() const
{
    const EphemeralRange range = compositionEphemeralRange();
    if (range.isNull())
        return;

    // The composition can start inside a composed character sequence, so we have to override checks.
    // See <http://bugs.webkit.org/show_bug.cgi?id=15781>
    VisibleSelection selection;
    selection.setWithoutValidation(range.startPosition(), range.endPosition());
    frame().selection().setSelection(selection, 0);
}

bool InputMethodController::confirmComposition()
{
    return confirmComposition(composingText());
}

bool InputMethodController::confirmComposition(const String& text, ConfirmCompositionBehavior confirmBehavior)
{
    if (!hasComposition())
        return false;

    Optional<Editor::RevealSelectionScope> revealSelectionScope;
    if (confirmBehavior == KeepSelection)
        revealSelectionScope.emplace(&editor());

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

    if (!frame().document())
        return false;

    // If text is empty, then delete the old composition here. If text is
    // non-empty, InsertTextCommand::input will delete the old composition with
    // an optimized replace operation.
    if (text.isEmpty())
        TypingCommand::deleteSelection(*frame().document(), 0);

    clear();

    insertTextDuringCompositionWithEvents(frame(), text, 0, TypingCommand::TextCompositionType::TextCompositionConfirm);
    // Event handler might destroy document.
    if (!frame().document())
        return false;

    // No DOM update after 'compositionend'.
    dispatchCompositionEndEvent(frame(), text);

    return true;
}

bool InputMethodController::confirmCompositionOrInsertText(const String& text, ConfirmCompositionBehavior confirmBehavior)
{
    if (!hasComposition()) {
        if (!text.length())
            return false;

        if (dispatchBeforeInputInsertText(frame().document()->focusedElement(), text) != DispatchEventResult::NotCanceled)
            return false;

        editor().insertText(text, 0);
        return true;
    }

    if (text.length()) {
        confirmComposition(text);
        return true;
    }

    if (confirmBehavior == DoNotKeepSelection)
        return confirmComposition(composingText(), DoNotKeepSelection);

    SelectionOffsetsScope selectionOffsetsScope(this);
    return confirmComposition();
}

void InputMethodController::cancelComposition()
{
    if (!hasComposition())
        return;

    Editor::RevealSelectionScope revealSelectionScope(&editor());

    if (frame().selection().isNone())
        return;

    clear();

    // TODO(chongz): Update InputType::DeleteComposedCharacter with latest discussion.
    dispatchBeforeInputFromComposition(frame().document()->focusedElement(), InputEvent::InputType::DeleteComposedCharacter, emptyString(), InputEvent::EventCancelable::NotCancelable);
    dispatchCompositionUpdateEvent(frame(), emptyString());
    insertTextDuringCompositionWithEvents(frame(), emptyString(), 0, TypingCommand::TextCompositionType::TextCompositionConfirm);
    // Event handler might destroy document.
    if (!frame().document())
        return;

    // An open typing command that disagrees about current selection would cause
    // issues with typing later on.
    TypingCommand::closeTyping(m_frame);

    // No DOM update after 'compositionend'.
    dispatchCompositionEndEvent(frame(), emptyString());
}

void InputMethodController::cancelCompositionIfSelectionIsInvalid()
{
    if (!hasComposition() || editor().preventRevealSelection())
        return;

    // Check if selection start and selection end are valid.
    FrameSelection& selection = frame().selection();
    if (!selection.isNone() && !m_compositionRange->collapsed()) {
        if (selection.start().compareTo(m_compositionRange->startPosition()) >= 0
            && selection.end().compareTo(m_compositionRange->endPosition()) <= 0)
            return;
    }

    cancelComposition();
    frame().chromeClient().didCancelCompositionOnSelectionChange();
}

void InputMethodController::setComposition(const String& text, const Vector<CompositionUnderline>& underlines, int selectionStart, int selectionEnd)
{
    Editor::RevealSelectionScope revealSelectionScope(&editor());

    // Updates styles before setting selection for composition to prevent
    // inserting the previous composition text into text nodes oddly.
    // See https://bugs.webkit.org/show_bug.cgi?id=46868
    frame().document()->updateStyleAndLayoutTree();

    selectComposition();

    if (frame().selection().isNone())
        return;

    Element* target = frame().document()->focusedElement();
    if (!target)
        return;

    // Dispatch an appropriate composition event to the focused node.
    // We check the composition status and choose an appropriate composition event since this
    // function is used for three purposes:
    // 1. Starting a new composition.
    //    Send a compositionstart and a compositionupdate event when this function creates
    //    a new composition node, i.e.
    //    !hasComposition() && !text.isEmpty().
    //    Sending a compositionupdate event at this time ensures that at least one
    //    compositionupdate event is dispatched.
    // 2. Updating the existing composition node.
    //    Send a compositionupdate event when this function updates the existing composition
    //    node, i.e. hasComposition() && !text.isEmpty().
    // 3. Canceling the ongoing composition.
    //    Send a compositionend event when function deletes the existing composition node, i.e.
    //    !hasComposition() && test.isEmpty().
    if (text.isEmpty()) {
        if (hasComposition()) {
            confirmComposition(emptyString());
            return;
        }
        // It's weird to call |setComposition()| with empty text outside composition, however some IME
        // (e.g. Japanese IBus-Anthy) did this, so we simply delete selection without sending extra events.
        TypingCommand::deleteSelection(*frame().document(), TypingCommand::PreventSpellChecking);
        return;
    }

    // We should send a 'compositionstart' event only when the given text is not empty because this
    // function doesn't create a composition node when the text is empty.
    if (!hasComposition()) {
        target->dispatchEvent(CompositionEvent::create(EventTypeNames::compositionstart, frame().domWindow(), frame().selectedText()));
        if (!frame().document())
            return;
    }

    DCHECK(!text.isEmpty());

    clear();

    insertTextDuringCompositionWithEvents(frame(), text, TypingCommand::SelectInsertedText | TypingCommand::PreventSpellChecking, TypingCommand::TextCompositionUpdate);
    // Event handlers might destroy document.
    if (!frame().document())
        return;

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
        m_compositionRange = Range::create(baseNode->document());
    m_compositionRange->setStart(baseNode, baseOffset);
    m_compositionRange->setEnd(baseNode, extentOffset);

    if (baseNode->layoutObject())
        baseNode->layoutObject()->setShouldDoFullPaintInvalidation();

    // In case of exceeding the left boundary.
    int selectionOffsetsStart = static_cast<int>(getSelectionOffsets().start());
    int start = std::max(selectionOffsetsStart + selectionStart, 0);
    int end = std::max(selectionOffsetsStart + selectionEnd, start);

    Element* rootEditableElement = frame().selection().rootEditableElement();
    if (!rootEditableElement)
        return;

    // In case of exceeding the right boundary.
    // If both |value1| and |value2| exceed right boundary,
    // PlainTextRange(value1, value2)::createRange() will return a default
    // value, which is [0,0]. In order to get the correct Position in that case,
    // we should make sure |value1| is within range at least.
    const EphemeralRange& startRange = PlainTextRange(0, start).createRange(*rootEditableElement);
    const EphemeralRange& endRange = PlainTextRange(0, end).createRange(*rootEditableElement);

    // TODO(yabinh): There should be a better way to create |startPosition| and
    // |endPosition|. But for now, since we can't get |anchorNode| and |offset|,
    // we can't create the 2 Position objects directly. So we use
    // PlainTextRange::createRange as a workaround.
    const Position& startPosition = startRange.endPosition();
    const Position& endPosition = endRange.endPosition();
    const EphemeralRange selectedRange(startPosition, endPosition);
    frame().selection().setSelectedRange(selectedRange, TextAffinity::Downstream, SelectionDirectionalMode::NonDirectional, NotUserTriggered);

    if (underlines.isEmpty()) {
        frame().document()->markers().addCompositionMarker(m_compositionRange->startPosition(), m_compositionRange->endPosition(), Color::black, false, LayoutTheme::theme().platformDefaultCompositionBackgroundColor());
        return;
    }
    for (const auto& underline : underlines) {
        unsigned underlineStart = baseOffset + underline.startOffset;
        unsigned underlineEnd = baseOffset + underline.endOffset;
        EphemeralRange ephemeralLineRange = EphemeralRange(Position(baseNode, underlineStart), Position(baseNode, underlineEnd));
        if (ephemeralLineRange.isNull())
            continue;
        frame().document()->markers().addCompositionMarker(ephemeralLineRange.startPosition(), ephemeralLineRange.endPosition(), underline.color, underline.thick, underline.backgroundColor);
    }
}

void InputMethodController::setCompositionFromExistingText(const Vector<CompositionUnderline>& underlines, unsigned compositionStart, unsigned compositionEnd)
{
    Element* editable = frame().selection().rootEditableElement();
    if (!editable)
        return;

    const EphemeralRange range = PlainTextRange(compositionStart, compositionEnd).createRange(*editable);
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
        unsigned underlineStart = compositionStart + underline.startOffset;
        unsigned underlineEnd = compositionStart + underline.endOffset;
        EphemeralRange ephemeralLineRange = PlainTextRange(underlineStart, underlineEnd).createRange(*editable);
        if (ephemeralLineRange.isNull())
            continue;
        frame().document()->markers().addCompositionMarker(ephemeralLineRange.startPosition(), ephemeralLineRange.endPosition(), underline.color, underline.thick, underline.backgroundColor);
    }

    m_hasComposition = true;
    if (!m_compositionRange)
        m_compositionRange = Range::create(range.document());
    m_compositionRange->setStart(range.startPosition());
    m_compositionRange->setEnd(range.endPosition());
}

EphemeralRange InputMethodController::compositionEphemeralRange() const
{
    if (!hasComposition())
        return EphemeralRange();
    return EphemeralRange(m_compositionRange.get());
}

Range* InputMethodController::compositionRange() const
{
    return hasComposition() ? m_compositionRange : nullptr;
}

String InputMethodController::composingText() const
{
    return plainText(compositionEphemeralRange(), TextIteratorEmitsOriginalText);
}

PlainTextRange InputMethodController::getSelectionOffsets() const
{
    EphemeralRange range = firstEphemeralRangeOf(frame().selection().selection());
    if (range.isNull())
        return PlainTextRange();
    ContainerNode* editable = frame().selection().rootEditableElementOrTreeScopeRootNode();
    DCHECK(editable);
    return PlainTextRange::create(*editable, range);
}

bool InputMethodController::setSelectionOffsets(const PlainTextRange& selectionOffsets)
{
    if (selectionOffsets.isNull())
        return false;
    Element* rootEditableElement = frame().selection().rootEditableElement();
    if (!rootEditableElement)
        return false;

    const EphemeralRange range = selectionOffsets.createRange(*rootEditableElement);
    if (range.isNull())
        return false;

    return frame().selection().setSelectedRange(range, VP_DEFAULT_AFFINITY, SelectionDirectionalMode::NonDirectional, FrameSelection::CloseTyping);
}

bool InputMethodController::setEditableSelectionOffsets(const PlainTextRange& selectionOffsets)
{
    if (!editor().canEdit())
        return false;
    return setSelectionOffsets(selectionOffsets);
}

void InputMethodController::extendSelectionAndDelete(int before, int after)
{
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
        if (!setSelectionOffsets(PlainTextRange(std::max(static_cast<int>(selectionOffsets.start()) - before, 0), selectionOffsets.end() + after)))
            return;
        if (before == 0)
            break;
        ++before;
    } while (frame().selection().start() == frame().selection().end() && before <= static_cast<int>(selectionOffsets.start()));
    // TODO(chongz): New spec might want to change InputType.
    // https://github.com/w3c/editing/issues/125#issuecomment-213041256
    dispatchBeforeInputEditorCommand(m_frame->document()->focusedElement(), InputEvent::InputType::DeleteContent, emptyString(), new RangeVector(1, m_frame->selection().firstRange()));
    TypingCommand::deleteSelection(*frame().document());
}

DEFINE_TRACE(InputMethodController)
{
    visitor->trace(m_frame);
    visitor->trace(m_compositionRange);
}

} // namespace blink
