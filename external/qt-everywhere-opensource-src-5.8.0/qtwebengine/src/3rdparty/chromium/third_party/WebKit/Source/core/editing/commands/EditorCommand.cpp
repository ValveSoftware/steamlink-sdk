/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Igalia S.L.
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

#include "core/editing/Editor.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/clipboard/Pasteboard.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/DocumentFragment.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionModifier.h"
#include "core/editing/commands/CreateLinkCommand.h"
#include "core/editing/commands/EditorCommandNames.h"
#include "core/editing/commands/FormatBlockCommand.h"
#include "core/editing/commands/IndentOutdentCommand.h"
#include "core/editing/commands/InsertListCommand.h"
#include "core/editing/commands/ReplaceSelectionCommand.h"
#include "core/editing/commands/TypingCommand.h"
#include "core/editing/commands/UnlinkCommand.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/Event.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFontElement.h"
#include "core/html/HTMLHRElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutBox.h"
#include "core/page/ChromeClient.h"
#include "core/page/EditorClient.h"
#include "platform/Histogram.h"
#include "platform/KillRing.h"
#include "platform/UserGestureIndicator.h"
#include "platform/scroll/Scrollbar.h"
#include "public/platform/Platform.h"
#include "public/platform/WebEditingCommandType.h"
#include "wtf/StringExtras.h"
#include "wtf/text/AtomicString.h"

#include <iterator>

namespace blink {

using namespace HTMLNames;

namespace {

struct CommandNameEntry {
    const char* name;
    WebEditingCommandType type;
};

const CommandNameEntry kCommandNameEntries[] = {
#define V(name) { #name, WebEditingCommandType::name },
    FOR_EACH_BLINK_EDITING_COMMAND_NAME(V)
#undef V
};
// Handles all commands except WebEditingCommandType::Invalid.
static_assert(arraysize(kCommandNameEntries) + 1 == static_cast<size_t>(WebEditingCommandType::NumberOfCommandTypes),
    "must handle all valid WebEditingCommandType");

WebEditingCommandType WebEditingCommandTypeFromCommandName(const String& commandName)
{
    const CommandNameEntry* result = std::lower_bound(
        std::begin(kCommandNameEntries), std::end(kCommandNameEntries), commandName,
        [](const CommandNameEntry& entry, const String& needle)
        {
            return codePointCompareIgnoringASCIICase(needle, entry.name) > 0;
        });
    if (result != std::end(kCommandNameEntries) && codePointCompareIgnoringASCIICase(commandName, result->name) == 0)
        return result->type;
    return WebEditingCommandType::Invalid;
}

InputEvent::InputType InputTypeFromCommandType(WebEditingCommandType commandType)
{
    switch (commandType) {
    case WebEditingCommandType::Delete:
    case WebEditingCommandType::DeleteBackward:
    case WebEditingCommandType::DeleteBackwardByDecomposingPreviousCharacter:
    case WebEditingCommandType::DeleteForward:
    case WebEditingCommandType::DeleteToBeginningOfLine:
    case WebEditingCommandType::DeleteToBeginningOfParagraph:
    case WebEditingCommandType::DeleteToEndOfLine:
    case WebEditingCommandType::DeleteToEndOfParagraph:
    case WebEditingCommandType::DeleteToMark:
    case WebEditingCommandType::DeleteWordBackward:
    case WebEditingCommandType::DeleteWordForward:
        return InputEvent::InputType::DeleteContent;
    case WebEditingCommandType::Redo:
        return InputEvent::InputType::Redo;
    case WebEditingCommandType::Undo:
        return InputEvent::InputType::Undo;
    case WebEditingCommandType::InsertBacktab:
    case WebEditingCommandType::InsertText:
        return InputEvent::InputType::InsertText;
    default:
        return InputEvent::InputType::None;
    }
}

RangeVector* RangesFromCurrentSelectionOrExtendCaret(const LocalFrame& frame, SelectionDirection direction, TextGranularity granularity)
{
    SelectionModifier selectionModifier(frame, frame.selection().selection());
    if (selectionModifier.selection().isCaret())
        selectionModifier.modify(FrameSelection::AlterationExtend, direction, granularity);
    RangeVector* ranges = new RangeVector;
    // We only supports single selections.
    if (selectionModifier.selection().isNone())
        return ranges;
    ranges->append(firstRangeOf(selectionModifier.selection()));
    return ranges;
}

} // anonymous namespace

class EditorInternalCommand {
public:
    WebEditingCommandType commandType;
    bool (*execute)(LocalFrame&, Event*, EditorCommandSource, const String&);
    bool (*isSupportedFromDOM)(LocalFrame*);
    bool (*isEnabled)(LocalFrame&, Event*, EditorCommandSource);
    TriState (*state)(LocalFrame&, Event*);
    String (*value)(LocalFrame&, Event*);
    bool isTextInsertion;
    // TODO(yosin) We should have |canExecute()|, which checks clipboard
    // accessibility to simplify |Editor::Command::execute()|.
    bool allowExecutionWhenDisabled;
};

static const bool notTextInsertion = false;
static const bool isTextInsertion = true;

static const bool allowExecutionWhenDisabled = true;
static const bool doNotAllowExecutionWhenDisabled = false;

// Related to Editor::selectionForCommand.
// Certain operations continue to use the target control's selection even if the event handler
// already moved the selection outside of the text control.
static LocalFrame* targetFrame(LocalFrame& frame, Event* event)
{
    if (!event)
        return &frame;
    Node* node = event->target()->toNode();
    if (!node)
        return &frame;
    return node->document().frame();
}

static bool applyCommandToFrame(LocalFrame& frame, EditorCommandSource source, EditAction action, StylePropertySet* style)
{
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        frame.editor().applyStyleToSelection(style, action);
        return true;
    case CommandFromDOM:
        frame.editor().applyStyle(style);
        return true;
    }
    NOTREACHED();
    return false;
}

static bool executeApplyStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const String& propertyValue)
{
    MutableStylePropertySet* style = MutableStylePropertySet::create(HTMLQuirksMode);
    style->setProperty(propertyID, propertyValue);
    return applyCommandToFrame(frame, source, action, style);
}

static bool executeApplyStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, CSSValueID propertyValue)
{
    MutableStylePropertySet* style = MutableStylePropertySet::create(HTMLQuirksMode);
    style->setProperty(propertyID, propertyValue);
    return applyCommandToFrame(frame, source, action, style);
}

// FIXME: executeToggleStyleInList does not handle complicated cases such as <b><u>hello</u>world</b> properly.
//        This function must use Editor::selectionHasStyle to determine the current style but we cannot fix this
//        until https://bugs.webkit.org/show_bug.cgi?id=27818 is resolved.
static bool executeToggleStyleInList(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, CSSValue* value)
{
    EditingStyle* selectionStyle = EditingStyle::styleAtSelectionStart(frame.selection().selection());
    if (!selectionStyle || !selectionStyle->style())
        return false;

    const CSSValue* selectedCSSValue = selectionStyle->style()->getPropertyCSSValue(propertyID);
    String newStyle("none");
    if (selectedCSSValue->isValueList()) {
        CSSValueList* selectedCSSValueList = toCSSValueList(selectedCSSValue)->copy();
        if (!selectedCSSValueList->removeAll(*value))
            selectedCSSValueList->append(*value);
        if (selectedCSSValueList->length())
            newStyle = selectedCSSValueList->cssText();

    } else if (selectedCSSValue->cssText() == "none") {
        newStyle = value->cssText();
    }

    // FIXME: We shouldn't be having to convert new style into text.  We should have setPropertyCSSValue.
    MutableStylePropertySet* newMutableStyle = MutableStylePropertySet::create(HTMLQuirksMode);
    newMutableStyle->setProperty(propertyID, newStyle);
    return applyCommandToFrame(frame, source, action, newMutableStyle);
}

static bool executeToggleStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const char* offValue, const char* onValue)
{
    // Style is considered present when
    // Mac: present at the beginning of selection
    // other: present throughout the selection

    bool styleIsPresent;
    if (frame.editor().behavior().shouldToggleStyleBasedOnStartOfSelection())
        styleIsPresent = frame.editor().selectionStartHasStyle(propertyID, onValue);
    else
        styleIsPresent = frame.editor().selectionHasStyle(propertyID, onValue) == TrueTriState;

    EditingStyle* style = EditingStyle::create(propertyID, styleIsPresent ? offValue : onValue);
    return applyCommandToFrame(frame, source, action, style->style());
}

static bool executeApplyParagraphStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const String& propertyValue)
{
    MutableStylePropertySet* style = MutableStylePropertySet::create(HTMLQuirksMode);
    style->setProperty(propertyID, propertyValue);
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        frame.editor().applyParagraphStyleToSelection(style, action);
        return true;
    case CommandFromDOM:
        frame.editor().applyParagraphStyle(style);
        return true;
    }
    NOTREACHED();
    return false;
}

static bool executeInsertFragment(LocalFrame& frame, DocumentFragment* fragment)
{
    DCHECK(frame.document());
    return ReplaceSelectionCommand::create(*frame.document(), fragment, ReplaceSelectionCommand::PreventNesting, EditActionUnspecified)->apply();
}

static bool executeInsertElement(LocalFrame& frame, HTMLElement* content)
{
    DCHECK(frame.document());
    DocumentFragment* fragment = DocumentFragment::create(*frame.document());
    TrackExceptionState exceptionState;
    fragment->appendChild(content, exceptionState);
    if (exceptionState.hadException())
        return false;
    return executeInsertFragment(frame, fragment);
}

static bool expandSelectionToGranularity(LocalFrame& frame, TextGranularity granularity)
{
    VisibleSelection selection = frame.selection().selection();
    selection.expandUsingGranularity(granularity);
    const EphemeralRange newRange = selection.toNormalizedEphemeralRange();
    if (newRange.isNull())
        return false;
    if (newRange.isCollapsed())
        return false;
    TextAffinity affinity = frame.selection().affinity();
    frame.selection().setSelectedRange(newRange, affinity, SelectionDirectionalMode::NonDirectional, FrameSelection::CloseTyping);
    return true;
}

static TriState selectionListState(const FrameSelection& selection, const QualifiedName& tagName)
{
    if (selection.isCaret()) {
        if (enclosingElementWithTag(selection.selection().start(), tagName))
            return TrueTriState;
    } else if (selection.isRange()) {
        Element* startElement = enclosingElementWithTag(selection.selection().start(), tagName);
        Element* endElement = enclosingElementWithTag(selection.selection().end(), tagName);
        if (startElement && endElement && startElement == endElement)
            return TrueTriState;
    }

    return FalseTriState;
}

static TriState stateStyle(LocalFrame& frame, CSSPropertyID propertyID, const char* desiredValue)
{
    if (frame.editor().behavior().shouldToggleStyleBasedOnStartOfSelection())
        return frame.editor().selectionStartHasStyle(propertyID, desiredValue) ? TrueTriState : FalseTriState;
    return frame.editor().selectionHasStyle(propertyID, desiredValue);
}

static String valueStyle(LocalFrame& frame, CSSPropertyID propertyID)
{
    // FIXME: Rather than retrieving the style at the start of the current selection,
    // we should retrieve the style present throughout the selection for non-Mac platforms.
    return frame.editor().selectionStartCSSPropertyValue(propertyID);
}

static TriState stateTextWritingDirection(LocalFrame& frame, WritingDirection direction)
{
    bool hasNestedOrMultipleEmbeddings;
    WritingDirection selectionDirection = EditingStyle::textDirectionForSelection(frame.selection().selection(),
        frame.selection().typingStyle(), hasNestedOrMultipleEmbeddings);
    // FXIME: We should be returning MixedTriState when selectionDirection == direction && hasNestedOrMultipleEmbeddings
    return (selectionDirection == direction && !hasNestedOrMultipleEmbeddings) ? TrueTriState : FalseTriState;
}

static unsigned verticalScrollDistance(LocalFrame& frame)
{
    Element* focusedElement = frame.document()->focusedElement();
    if (!focusedElement)
        return 0;
    LayoutObject* layoutObject = focusedElement->layoutObject();
    if (!layoutObject || !layoutObject->isBox())
        return 0;
    LayoutBox& layoutBox = toLayoutBox(*layoutObject);
    const ComputedStyle* style = layoutBox.style();
    if (!style)
        return 0;
    if (!(style->overflowY() == OverflowScroll || style->overflowY() == OverflowAuto || focusedElement->hasEditableStyle()))
        return 0;
    int height = std::min<int>(layoutBox.clientHeight(), frame.view()->visibleHeight());
    return static_cast<unsigned>(max(max<int>(height * ScrollableArea::minFractionToStepWhenPaging(), height - ScrollableArea::maxOverlapBetweenPages()), 1));
}

static EphemeralRange unionEphemeralRanges(const EphemeralRange& range1, const EphemeralRange& range2)
{
    const Position startPosition = range1.startPosition().compareTo(range2.startPosition()) <= 0 ? range1.startPosition() : range2.startPosition();
    const Position endPosition = range1.endPosition().compareTo(range2.endPosition()) <= 0 ? range1.endPosition() : range2.endPosition();
    return EphemeralRange(startPosition, endPosition);
}

// Execute command functions

static bool executeBackColor(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionSetBackgroundColor, CSSPropertyBackgroundColor, value);
}

static bool canWriteClipboard(LocalFrame& frame, EditorCommandSource source)
{
    if (source == CommandFromMenuOrKeyBinding)
        return true;
    Settings* settings = frame.settings();
    bool defaultValue = (settings && settings->javaScriptCanAccessClipboard()) || UserGestureIndicator::utilizeUserGesture();
    return frame.editor().client().canCopyCut(&frame, defaultValue);
}

static bool executeCopy(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    // To support |allowExecutionWhenDisabled|, we need to check clipboard
    // accessibility here rather than |Editor::Command::execute()|.
    // TODO(yosin) We should move checking |canWriteClipboard()| to
    // |Editor::Command::execute()| with introducing appropriate predicate, e.g.
    // |canExecute()|. See also "Cut", and "Paste" command.
    if (!canWriteClipboard(frame, source))
        return false;
    frame.editor().copy();
    return true;
}

static bool executeCreateLink(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    if (value.isEmpty())
        return false;
    DCHECK(frame.document());
    return CreateLinkCommand::create(*frame.document(), value)->apply();
}

static bool executeCut(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    // To support |allowExecutionWhenDisabled|, we need to check clipboard
    // accessibility here rather than |Editor::Command::execute()|.
    // TODO(yosin) We should move checking |canWriteClipboard()| to
    // |Editor::Command::execute()| with introducing appropriate predicate, e.g.
    // |canExecute()|. See also "Copy", and "Paste" command.
    if (!canWriteClipboard(frame, source))
        return false;
    frame.editor().cut();
    return true;
}

static bool executeDefaultParagraphSeparator(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    if (equalIgnoringCase(value, "div"))
        frame.editor().setDefaultParagraphSeparator(EditorParagraphSeparatorIsDiv);
    else if (equalIgnoringCase(value, "p"))
        frame.editor().setDefaultParagraphSeparator(EditorParagraphSeparatorIsP);

    return true;
}

static bool executeDelete(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding: {
        // Doesn't modify the text if the current selection isn't a range.
        frame.editor().performDelete();
        return true;
    }
    case CommandFromDOM:
        // If the current selection is a caret, delete the preceding character. IE performs forwardDelete, but we currently side with Firefox.
        // Doesn't scroll to make the selection visible, or modify the kill ring (this time, siding with IE, not Firefox).
        DCHECK(frame.document());
        TypingCommand::deleteKeyPressed(*frame.document(), frame.selection().granularity() == WordGranularity ? TypingCommand::SmartDelete : 0);
        return true;
    }
    NOTREACHED();
    return false;
}

static bool executeDeleteBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteBackwardByDecomposingPreviousCharacter(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DLOG(ERROR) << "DeleteBackwardByDecomposingPreviousCharacter is not implemented, doing DeleteBackward instead";
    frame.editor().deleteWithDirection(DirectionBackward, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionForward, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteToBeginningOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, LineBoundary, true, false);
    return true;
}

static bool executeDeleteToBeginningOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph (like DeleteToEndOfParagraph).
    frame.editor().deleteWithDirection(DirectionForward, LineBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph.
    frame.editor().deleteWithDirection(DirectionForward, ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    const EphemeralRange mark = frame.editor().mark().toNormalizedEphemeralRange();
    if (mark.isNotNull()) {
        bool selected = frame.selection().setSelectedRange(unionEphemeralRanges(mark, frame.editor().selectedRange()), TextAffinity::Downstream, SelectionDirectionalMode::NonDirectional, FrameSelection::CloseTyping);
        DCHECK(selected);
        if (!selected)
            return false;
    }
    frame.editor().performDelete();
    frame.editor().setMark(frame.selection().selection());
    return true;
}

static bool executeDeleteWordBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, WordGranularity, true, false);
    return true;
}

static bool executeDeleteWordForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionForward, WordGranularity, true, false);
    return true;
}

static bool executeFindString(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    return frame.editor().findString(value, CaseInsensitive | WrapAround);
}

static bool executeFontName(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionSetFont, CSSPropertyFontFamily, value);
}

static bool executeFontSize(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    CSSValueID size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return false;
    return executeApplyStyle(frame, source, EditActionChangeAttributes, CSSPropertyFontSize, size);
}

static bool executeFontSizeDelta(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionChangeAttributes, CSSPropertyWebkitFontSizeDelta, value);
}

static bool executeForeColor(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditActionSetColor, CSSPropertyColor, value);
}

static bool executeFormatBlock(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    String tagName = value.lower();
    if (tagName[0] == '<' && tagName[tagName.length() - 1] == '>')
        tagName = tagName.substring(1, tagName.length() - 2);

    AtomicString localName, prefix;
    if (!Document::parseQualifiedName(AtomicString(tagName), prefix, localName, IGNORE_EXCEPTION))
        return false;
    QualifiedName qualifiedTagName(prefix, localName, xhtmlNamespaceURI);

    DCHECK(frame.document());
    FormatBlockCommand* command = FormatBlockCommand::create(*frame.document(), qualifiedTagName);
    command->apply();
    return command->didApply();
}

static bool executeForwardDelete(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    EditingState editingState;
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        frame.editor().deleteWithDirection(DirectionForward, CharacterGranularity, false, true);
        return true;
    case CommandFromDOM:
        // Doesn't scroll to make the selection visible, or modify the kill ring.
        // ForwardDelete is not implemented in IE or Firefox, so this behavior is only needed for
        // backward compatibility with ourselves, and for consistency with Delete.
        DCHECK(frame.document());
        TypingCommand::forwardDeleteKeyPressed(*frame.document(), &editingState);
        if (editingState.isAborted())
            return false;
        return true;
    }
    NOTREACHED();
    return false;
}

static bool executeIgnoreSpelling(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.spellChecker().ignoreSpelling();
    return true;
}

static bool executeIndent(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DCHECK(frame.document());
    return IndentOutdentCommand::create(*frame.document(), IndentOutdentCommand::Indent)->apply();
}

static bool executeInsertBacktab(LocalFrame& frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\t", event, TextEventInputBackTab);
}

static bool executeInsertHorizontalRule(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    DCHECK(frame.document());
    HTMLHRElement* rule = HTMLHRElement::create(*frame.document());
    if (!value.isEmpty())
        rule->setIdAttribute(AtomicString(value));
    return executeInsertElement(frame, rule);
}

static bool executeInsertHTML(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    DCHECK(frame.document());
    return executeInsertFragment(frame, createFragmentFromMarkup(*frame.document(), value, ""));
}

static bool executeInsertImage(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    DCHECK(frame.document());
    HTMLImageElement* image = HTMLImageElement::create(*frame.document());
    if (!value.isEmpty())
        image->setSrc(value);
    return executeInsertElement(frame, image);
}

static bool executeInsertLineBreak(LocalFrame& frame, Event* event, EditorCommandSource source, const String&)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\n", event, TextEventInputLineBreak);
    case CommandFromDOM:
        // Doesn't scroll to make the selection visible, or modify the kill ring.
        // InsertLineBreak is not implemented in IE or Firefox, so this behavior is only needed for
        // backward compatibility with ourselves, and for consistency with other commands.
        DCHECK(frame.document());
        return TypingCommand::insertLineBreak(*frame.document());
    }
    NOTREACHED();
    return false;
}

static bool executeInsertNewline(LocalFrame& frame, Event* event, EditorCommandSource, const String&)
{
    LocalFrame* targetFrame = blink::targetFrame(frame, event);
    return targetFrame->eventHandler().handleTextInputEvent("\n", event, targetFrame->editor().canEditRichly() ? TextEventInputKeyboard : TextEventInputLineBreak);
}

static bool executeInsertNewlineInQuotedContent(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DCHECK(frame.document());
    return TypingCommand::insertParagraphSeparatorInQuotedContent(*frame.document());
}

static bool executeInsertOrderedList(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DCHECK(frame.document());
    return InsertListCommand::create(*frame.document(), InsertListCommand::OrderedList)->apply();
}

static bool executeInsertParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DCHECK(frame.document());
    return TypingCommand::insertParagraphSeparator(*frame.document());
}

static bool executeInsertTab(LocalFrame& frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\t", event);
}

static bool executeInsertText(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    DCHECK(frame.document());
    TypingCommand::insertText(*frame.document(), value, 0);
    return true;
}

static bool executeInsertUnorderedList(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DCHECK(frame.document());
    return InsertListCommand::create(*frame.document(), InsertListCommand::UnorderedList)->apply();
}

static bool executeJustifyCenter(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionCenter, CSSPropertyTextAlign, "center");
}

static bool executeJustifyFull(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionJustify, CSSPropertyTextAlign, "justify");
}

static bool executeJustifyLeft(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionAlignLeft, CSSPropertyTextAlign, "left");
}

static bool executeJustifyRight(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditActionAlignRight, CSSPropertyTextAlign, "right");
}

static bool executeMakeTextWritingDirectionLeftToRight(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    MutableStylePropertySet* style = MutableStylePropertySet::create(HTMLQuirksMode);
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueIsolate);
    style->setProperty(CSSPropertyDirection, CSSValueLtr);
    frame.editor().applyStyle(style, EditActionSetWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionNatural(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    MutableStylePropertySet* style = MutableStylePropertySet::create(HTMLQuirksMode);
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
    frame.editor().applyStyle(style, EditActionSetWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionRightToLeft(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    MutableStylePropertySet* style = MutableStylePropertySet::create(HTMLQuirksMode);
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueIsolate);
    style->setProperty(CSSPropertyDirection, CSSValueRtl);
    frame.editor().applyStyle(style, EditActionSetWritingDirection);
    return true;
}

static bool executeMoveBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveBackwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveDown(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, LineGranularity, UserTriggered);
}

static bool executeMoveDownAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, LineGranularity, UserTriggered);
    return true;
}

static bool executeMoveForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveForwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveLeft(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionLeft, CharacterGranularity, UserTriggered);
}

static bool executeMoveLeftAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionLeft, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMovePageDown(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationMove, distance, FrameSelection::DirectionDown,
        UserTriggered, CursorAlignOnScroll::Always);
}

static bool executeMovePageDownAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationExtend, distance, FrameSelection::DirectionDown,
        UserTriggered, CursorAlignOnScroll::Always);
}

static bool executeMovePageUp(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationMove, distance, FrameSelection::DirectionUp,
        UserTriggered, CursorAlignOnScroll::Always);
}

static bool executeMovePageUpAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationExtend, distance, FrameSelection::DirectionUp,
        UserTriggered, CursorAlignOnScroll::Always);
}

static bool executeMoveRight(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionRight, CharacterGranularity, UserTriggered);
}

static bool executeMoveRightAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionRight, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfDocumentAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfParagraphAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfSentence(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfSentenceAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfDocumentAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfSentence(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfSentenceAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfParagraphAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveParagraphBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, ParagraphGranularity, UserTriggered);
    return true;
}

static bool executeMoveParagraphBackwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, ParagraphGranularity, UserTriggered);
    return true;
}

static bool executeMoveParagraphForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, ParagraphGranularity, UserTriggered);
    return true;
}

static bool executeMoveParagraphForwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, ParagraphGranularity, UserTriggered);
    return true;
}

static bool executeMoveUp(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, LineGranularity, UserTriggered);
}

static bool executeMoveUpAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, LineGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordBackwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordForwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordLeft(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionLeft, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordLeftAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionLeft, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordRight(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionRight, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordRightAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionRight, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveToLeftEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionLeft, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToLeftEndOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionLeft, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToRightEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionRight, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToRightEndOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionRight, LineBoundary, UserTriggered);
    return true;
}

static bool executeOutdent(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DCHECK(frame.document());
    return IndentOutdentCommand::create(*frame.document(), IndentOutdentCommand::Outdent)->apply();
}

static bool executeToggleOverwrite(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().toggleOverwriteModeEnabled();
    return true;
}

static bool canReadClipboard(LocalFrame& frame, EditorCommandSource source)
{
    if (source == CommandFromMenuOrKeyBinding)
        return true;
    Settings* settings = frame.settings();
    bool defaultValue = settings && settings->javaScriptCanAccessClipboard() && settings->DOMPasteAllowed();
    return frame.editor().client().canPaste(&frame, defaultValue);
}

static bool executePaste(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    // To support |allowExecutionWhenDisabled|, we need to check clipboard
    // accessibility here rather than |Editor::Command::execute()|.
    // TODO(yosin) We should move checking |canReadClipboard()| to
    // |Editor::Command::execute()| with introducing appropriate predicate, e.g.
    // |canExecute()|. See also "Copy", and "Cut" command.
    if (!canReadClipboard(frame, source))
        return false;
    frame.editor().paste();
    return true;
}

static bool executePasteGlobalSelection(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    // To support |allowExecutionWhenDisabled|, we need to check clipboard
    // accessibility here rather than |Editor::Command::execute()|.
    // TODO(yosin) We should move checking |canReadClipboard()| to
    // |Editor::Command::execute()| with introducing appropriate predicate, e.g.
    // |canExecute()|. See also "Copy", and "Cut" command.
    if (!canReadClipboard(frame, source))
        return false;
    if (!frame.editor().behavior().supportsGlobalSelection())
        return false;
    ASSERT_UNUSED(source, source == CommandFromMenuOrKeyBinding);

    bool oldSelectionMode = Pasteboard::generalPasteboard()->isSelectionMode();
    Pasteboard::generalPasteboard()->setSelectionMode(true);
    frame.editor().paste();
    Pasteboard::generalPasteboard()->setSelectionMode(oldSelectionMode);
    return true;
}

static bool executePasteAndMatchStyle(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().pasteAsPlainText();
    return true;
}

static bool executePrint(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    FrameHost* host = frame.host();
    if (!host)
        return false;
    host->chromeClient().print(&frame);
    return true;
}

static bool executeRedo(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().redo();
    return true;
}

static bool executeRemoveFormat(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().removeFormattingAndStyle();
    return true;
}

static bool executeScrollPageBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().bubblingScroll(ScrollBlockDirectionBackward, ScrollByPage);
}

static bool executeScrollPageForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().bubblingScroll(ScrollBlockDirectionForward, ScrollByPage);
}

static bool executeScrollLineUp(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().bubblingScroll(ScrollUpIgnoringWritingMode, ScrollByLine);
}

static bool executeScrollLineDown(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().bubblingScroll(ScrollDownIgnoringWritingMode, ScrollByLine);
}

static bool executeScrollToBeginningOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().bubblingScroll(ScrollBlockDirectionBackward, ScrollByDocument);
}

static bool executeScrollToEndOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().bubblingScroll(ScrollBlockDirectionForward, ScrollByDocument);
}

static bool executeSelectAll(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().selectAll();
    return true;
}

static bool executeSelectLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, LineGranularity);
}

static bool executeSelectParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, ParagraphGranularity);
}

static bool executeSelectSentence(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, SentenceGranularity);
}

static bool executeSelectToMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    const EphemeralRange mark = frame.editor().mark().toNormalizedEphemeralRange();
    EphemeralRange selection = frame.editor().selectedRange();
    if (mark.isNull() || selection.isNull())
        return false;
    frame.selection().setSelectedRange(unionEphemeralRanges(mark, selection), TextAffinity::Downstream, SelectionDirectionalMode::NonDirectional, FrameSelection::CloseTyping);
    return true;
}

static bool executeSelectWord(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, WordGranularity);
}

static bool executeSetMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().setMark(frame.selection().selection());
    return true;
}

static bool executeStrikethrough(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    CSSPrimitiveValue* lineThrough = CSSPrimitiveValue::createIdentifier(CSSValueLineThrough);
    return executeToggleStyleInList(frame, source, EditActionUnderline, CSSPropertyWebkitTextDecorationsInEffect, lineThrough);
}

static bool executeStyleWithCSS(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    frame.editor().setShouldStyleWithCSS(!equalIgnoringCase(value, "false"));
    return true;
}

static bool executeUseCSS(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    frame.editor().setShouldStyleWithCSS(equalIgnoringCase(value, "false"));
    return true;
}

static bool executeSubscript(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionSubscript, CSSPropertyVerticalAlign, "baseline", "sub");
}

static bool executeSuperscript(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionSuperscript, CSSPropertyVerticalAlign, "baseline", "super");
}

static bool executeSwapWithMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    const VisibleSelection& mark = frame.editor().mark();
    const VisibleSelection& selection = frame.selection().selection();
    if (mark.isNone() || selection.isNone())
        return false;
    frame.selection().setSelection(mark);
    frame.editor().setMark(selection);
    return true;
}

static bool executeToggleBold(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionBold, CSSPropertyFontWeight, "normal", "bold");
}

static bool executeToggleItalic(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditActionItalics, CSSPropertyFontStyle, "normal", "italic");
}

static bool executeTranspose(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().transpose();
    return true;
}

static bool executeUnderline(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    CSSPrimitiveValue* underline = CSSPrimitiveValue::createIdentifier(CSSValueUnderline);
    return executeToggleStyleInList(frame, source, EditActionUnderline, CSSPropertyWebkitTextDecorationsInEffect, underline);
}

static bool executeUndo(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().undo();
    return true;
}

static bool executeUnlink(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    DCHECK(frame.document());
    return UnlinkCommand::create(*frame.document())->apply();
}

static bool executeUnscript(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyStyle(frame, source, EditActionUnscript, CSSPropertyVerticalAlign, "baseline");
}

static bool executeUnselect(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().clear();
    return true;
}

static bool executeYank(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().insertTextWithoutSendingTextEvent(frame.editor().killRing().yank(), false, 0);
    frame.editor().killRing().setToYankedState();
    return true;
}

static bool executeYankAndSelect(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().insertTextWithoutSendingTextEvent(frame.editor().killRing().yank(), true, 0);
    frame.editor().killRing().setToYankedState();
    return true;
}

// Supported functions

static bool supported(LocalFrame*)
{
    return true;
}

static bool supportedFromMenuOrKeyBinding(LocalFrame*)
{
    return false;
}

// Enabled functions

static bool enabled(LocalFrame&, Event*, EditorCommandSource)
{
    return true;
}

static bool enabledVisibleSelection(LocalFrame& frame, Event* event, EditorCommandSource)
{
    // The term "visible" here includes a caret in editable text or a range in any text.
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return (selection.isCaret() && selection.isContentEditable()) || selection.isRange();
}

static bool caretBrowsingEnabled(LocalFrame& frame)
{
    return frame.settings() && frame.settings()->caretBrowsingEnabled();
}

static EditorCommandSource dummyEditorCommandSource = static_cast<EditorCommandSource>(0);

static bool enabledVisibleSelectionOrCaretBrowsing(LocalFrame& frame, Event* event, EditorCommandSource)
{
    // The EditorCommandSource parameter is unused in enabledVisibleSelection, so just pass a dummy variable
    return caretBrowsingEnabled(frame) || enabledVisibleSelection(frame, event, dummyEditorCommandSource);
}

static bool enabledVisibleSelectionAndMark(LocalFrame& frame, Event* event, EditorCommandSource)
{
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return ((selection.isCaret() && selection.isContentEditable()) || selection.isRange())
        && !frame.editor().mark().isNone();
}

static bool enableCaretInEditableText(LocalFrame& frame, Event* event, EditorCommandSource)
{
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return selection.isCaret() && selection.isContentEditable();
}

static bool enabledCopy(LocalFrame& frame, Event*, EditorCommandSource source)
{
    if (!canWriteClipboard(frame, source))
        return false;
    return frame.editor().canDHTMLCopy() || frame.editor().canCopy();
}

static bool enabledCut(LocalFrame& frame, Event*, EditorCommandSource source)
{
    if (!canWriteClipboard(frame, source))
        return false;
    return frame.editor().canDHTMLCut() || frame.editor().canCut();
}

static bool enabledInEditableText(LocalFrame& frame, Event* event, EditorCommandSource)
{
    // We should update selection to canonicalize with current layout and style,
    // before accessing |FrameSelection::selection()|.
    frame.selection().updateIfNeeded();
    return frame.editor().selectionForCommand(event).rootEditableElement();
}

static bool enabledDelete(LocalFrame& frame, Event* event, EditorCommandSource source)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        return frame.editor().canDelete();
    case CommandFromDOM:
        // "Delete" from DOM is like delete/backspace keypress, affects selected range if non-empty,
        // otherwise removes a character
        return enabledInEditableText(frame, event, source);
    }
    NOTREACHED();
    return false;
}

static bool enabledInEditableTextOrCaretBrowsing(LocalFrame& frame, Event* event, EditorCommandSource)
{
    // The EditorCommandSource parameter is unused in enabledInEditableText, so just pass a dummy variable
    return caretBrowsingEnabled(frame) || enabledInEditableText(frame, event, dummyEditorCommandSource);
}

static bool enabledInRichlyEditableText(LocalFrame& frame, Event*, EditorCommandSource)
{
    // We should update selection to canonicalize with current layout and style,
    // before accessing |FrameSelection::selection()|.
    frame.selection().updateIfNeeded();
    return !frame.selection().isNone() && frame.selection().isContentRichlyEditable() && frame.selection().rootEditableElement();
}

static bool enabledPaste(LocalFrame& frame, Event*, EditorCommandSource source)
{
    if (!canReadClipboard(frame, source))
        return false;
    return frame.editor().canPaste();
}

static bool enabledRangeInEditableText(LocalFrame& frame, Event*, EditorCommandSource)
{
    // We should update selection to canonicalize with current layout and style,
    // before accessing |FrameSelection::selection()|.
    frame.selection().updateIfNeeded();
    return frame.selection().isRange() && frame.selection().isContentEditable();
}

static bool enabledRangeInRichlyEditableText(LocalFrame& frame, Event*, EditorCommandSource)
{
    // We should update selection to canonicalize with current layout and style,
    // before accessing |FrameSelection::selection()|.
    frame.selection().updateIfNeeded();
    return frame.selection().isRange() && frame.selection().isContentRichlyEditable();
}

static bool enabledRedo(LocalFrame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canRedo();
}

static bool enabledUndo(LocalFrame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canUndo();
}

// State functions

static TriState stateNone(LocalFrame&, Event*)
{
    return FalseTriState;
}

static TriState stateBold(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontWeight, "bold");
}

static TriState stateItalic(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontStyle, "italic");
}

static TriState stateOrderedList(LocalFrame& frame, Event*)
{
    return selectionListState(frame.selection(), olTag);
}

static TriState stateStrikethrough(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyWebkitTextDecorationsInEffect, "line-through");
}

static TriState stateStyleWithCSS(LocalFrame& frame, Event*)
{
    return frame.editor().shouldStyleWithCSS() ? TrueTriState : FalseTriState;
}

static TriState stateSubscript(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "sub");
}

static TriState stateSuperscript(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "super");
}

static TriState stateTextWritingDirectionLeftToRight(LocalFrame& frame, Event*)
{
    return stateTextWritingDirection(frame, LeftToRightWritingDirection);
}

static TriState stateTextWritingDirectionNatural(LocalFrame& frame, Event*)
{
    return stateTextWritingDirection(frame, NaturalWritingDirection);
}

static TriState stateTextWritingDirectionRightToLeft(LocalFrame& frame, Event*)
{
    return stateTextWritingDirection(frame, RightToLeftWritingDirection);
}

static TriState stateUnderline(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyWebkitTextDecorationsInEffect, "underline");
}

static TriState stateUnorderedList(LocalFrame& frame, Event*)
{
    return selectionListState(frame.selection(), ulTag);
}

static TriState stateJustifyCenter(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "center");
}

static TriState stateJustifyFull(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "justify");
}

static TriState stateJustifyLeft(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "left");
}

static TriState stateJustifyRight(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "right");
}

// Value functions

static String valueNull(LocalFrame&, Event*)
{
    return String();
}

static String valueBackColor(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyBackgroundColor);
}

static String valueDefaultParagraphSeparator(LocalFrame& frame, Event*)
{
    switch (frame.editor().defaultParagraphSeparator()) {
    case EditorParagraphSeparatorIsDiv:
        return divTag.localName();
    case EditorParagraphSeparatorIsP:
        return pTag.localName();
    }

    NOTREACHED();
    return String();
}

static String valueFontName(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontFamily);
}

static String valueFontSize(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontSize);
}

static String valueFontSizeDelta(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyWebkitFontSizeDelta);
}

static String valueForeColor(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyColor);
}

static String valueFormatBlock(LocalFrame& frame, Event*)
{
    const VisibleSelection& selection = frame.selection().selection();
    if (!selection.isNonOrphanedCaretOrRange() || !selection.isContentEditable())
        return "";
    Element* formatBlockElement = FormatBlockCommand::elementForFormatBlockCommand(firstRangeOf(selection));
    if (!formatBlockElement)
        return "";
    return formatBlockElement->localName();
}

// Map of functions

static const EditorInternalCommand* internalCommand(const String& commandName)
{
    static const EditorInternalCommand kEditorCommands[] = {
        // Lists all commands in blink::WebEditingCommandType.
        // Must be ordered by |commandType| for index lookup.
        // Covered by unit tests in EditingCommandTest.cpp
        { WebEditingCommandType::AlignJustified, executeJustifyFull, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::AlignLeft, executeJustifyLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::AlignRight, executeJustifyRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::BackColor, executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueBackColor, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::BackwardDelete, executeDeleteBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled }, // FIXME: remove BackwardDelete when Safari for Windows stops using it.
        { WebEditingCommandType::Bold, executeToggleBold, supported, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Copy, executeCopy, supported, enabledCopy, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled },
        { WebEditingCommandType::CreateLink, executeCreateLink, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Cut, executeCut, supported, enabledCut, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled },
        { WebEditingCommandType::DefaultParagraphSeparator, executeDefaultParagraphSeparator, supported, enabled, stateNone, valueDefaultParagraphSeparator, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Delete, executeDelete, supported, enabledDelete, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteBackward, executeDeleteBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteBackwardByDecomposingPreviousCharacter, executeDeleteBackwardByDecomposingPreviousCharacter, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteForward, executeDeleteForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteToBeginningOfLine, executeDeleteToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteToBeginningOfParagraph, executeDeleteToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteToEndOfLine, executeDeleteToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteToEndOfParagraph, executeDeleteToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteToMark, executeDeleteToMark, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteWordBackward, executeDeleteWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::DeleteWordForward, executeDeleteWordForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::FindString, executeFindString, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::FontName, executeFontName, supported, enabledInRichlyEditableText, stateNone, valueFontName, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::FontSize, executeFontSize, supported, enabledInRichlyEditableText, stateNone, valueFontSize, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::FontSizeDelta, executeFontSizeDelta, supported, enabledInRichlyEditableText, stateNone, valueFontSizeDelta, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ForeColor, executeForeColor, supported, enabledInRichlyEditableText, stateNone, valueForeColor, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::FormatBlock, executeFormatBlock, supported, enabledInRichlyEditableText, stateNone, valueFormatBlock, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ForwardDelete, executeForwardDelete, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::HiliteColor, executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::IgnoreSpelling, executeIgnoreSpelling, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Indent, executeIndent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertBacktab, executeInsertBacktab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertHTML, executeInsertHTML, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertHorizontalRule, executeInsertHorizontalRule, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertImage, executeInsertImage, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertLineBreak, executeInsertLineBreak, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertNewline, executeInsertNewline, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertNewlineInQuotedContent, executeInsertNewlineInQuotedContent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertOrderedList, executeInsertOrderedList, supported, enabledInRichlyEditableText, stateOrderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertParagraph, executeInsertParagraph, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertTab, executeInsertTab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertText, executeInsertText, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::InsertUnorderedList, executeInsertUnorderedList, supported, enabledInRichlyEditableText, stateUnorderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Italic, executeToggleItalic, supported, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::JustifyCenter, executeJustifyCenter, supported, enabledInRichlyEditableText, stateJustifyCenter, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::JustifyFull, executeJustifyFull, supported, enabledInRichlyEditableText, stateJustifyFull, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::JustifyLeft, executeJustifyLeft, supported, enabledInRichlyEditableText, stateJustifyLeft, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::JustifyNone, executeJustifyLeft, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::JustifyRight, executeJustifyRight, supported, enabledInRichlyEditableText, stateJustifyRight, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MakeTextWritingDirectionLeftToRight, executeMakeTextWritingDirectionLeftToRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionLeftToRight, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MakeTextWritingDirectionNatural, executeMakeTextWritingDirectionNatural, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionNatural, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MakeTextWritingDirectionRightToLeft, executeMakeTextWritingDirectionRightToLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionRightToLeft, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveBackward, executeMoveBackward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveBackwardAndModifySelection, executeMoveBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveDown, executeMoveDown, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveDownAndModifySelection, executeMoveDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveForward, executeMoveForward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveForwardAndModifySelection, executeMoveForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveLeft, executeMoveLeft, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveLeftAndModifySelection, executeMoveLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MovePageDown, executeMovePageDown, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MovePageDownAndModifySelection, executeMovePageDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MovePageUp, executeMovePageUp, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MovePageUpAndModifySelection, executeMovePageUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveParagraphBackward, executeMoveParagraphBackward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveParagraphBackwardAndModifySelection, executeMoveParagraphBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveParagraphForward, executeMoveParagraphForward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveParagraphForwardAndModifySelection, executeMoveParagraphForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveRight, executeMoveRight, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveRightAndModifySelection, executeMoveRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfDocument, executeMoveToBeginningOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfDocumentAndModifySelection, executeMoveToBeginningOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfLine, executeMoveToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfLineAndModifySelection, executeMoveToBeginningOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfParagraph, executeMoveToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfParagraphAndModifySelection, executeMoveToBeginningOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfSentence, executeMoveToBeginningOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToBeginningOfSentenceAndModifySelection, executeMoveToBeginningOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfDocument, executeMoveToEndOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfDocumentAndModifySelection, executeMoveToEndOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfLine, executeMoveToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfLineAndModifySelection, executeMoveToEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfParagraph, executeMoveToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfParagraphAndModifySelection, executeMoveToEndOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfSentence, executeMoveToEndOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToEndOfSentenceAndModifySelection, executeMoveToEndOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToLeftEndOfLine, executeMoveToLeftEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToLeftEndOfLineAndModifySelection, executeMoveToLeftEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToRightEndOfLine, executeMoveToRightEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveToRightEndOfLineAndModifySelection, executeMoveToRightEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveUp, executeMoveUp, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveUpAndModifySelection, executeMoveUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordBackward, executeMoveWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordBackwardAndModifySelection, executeMoveWordBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordForward, executeMoveWordForward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordForwardAndModifySelection, executeMoveWordForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordLeft, executeMoveWordLeft, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordLeftAndModifySelection, executeMoveWordLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordRight, executeMoveWordRight, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::MoveWordRightAndModifySelection, executeMoveWordRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Outdent, executeOutdent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::OverWrite, executeToggleOverwrite, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Paste, executePaste, supported, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled },
        { WebEditingCommandType::PasteAndMatchStyle, executePasteAndMatchStyle, supported, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled },
        { WebEditingCommandType::PasteGlobalSelection, executePasteGlobalSelection, supportedFromMenuOrKeyBinding, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled },
        { WebEditingCommandType::Print, executePrint, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Redo, executeRedo, supported, enabledRedo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::RemoveFormat, executeRemoveFormat, supported, enabledRangeInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ScrollPageBackward, executeScrollPageBackward, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ScrollPageForward, executeScrollPageForward, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ScrollLineUp, executeScrollLineUp, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ScrollLineDown, executeScrollLineDown, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ScrollToBeginningOfDocument, executeScrollToBeginningOfDocument, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ScrollToEndOfDocument, executeScrollToEndOfDocument, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SelectAll, executeSelectAll, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SelectLine, executeSelectLine, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SelectParagraph, executeSelectParagraph, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SelectSentence, executeSelectSentence, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SelectToMark, executeSelectToMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SelectWord, executeSelectWord, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SetMark, executeSetMark, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Strikethrough, executeStrikethrough, supported, enabledInRichlyEditableText, stateStrikethrough, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::StyleWithCSS, executeStyleWithCSS, supported, enabled, stateStyleWithCSS, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Subscript, executeSubscript, supported, enabledInRichlyEditableText, stateSubscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Superscript, executeSuperscript, supported, enabledInRichlyEditableText, stateSuperscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::SwapWithMark, executeSwapWithMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ToggleBold, executeToggleBold, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ToggleItalic, executeToggleItalic, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::ToggleUnderline, executeUnderline, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Transpose, executeTranspose, supported, enableCaretInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Underline, executeUnderline, supported, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Undo, executeUndo, supported, enabledUndo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Unlink, executeUnlink, supported, enabledRangeInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Unscript, executeUnscript, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Unselect, executeUnselect, supported, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::UseCSS, executeUseCSS, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::Yank, executeYank, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::YankAndSelect, executeYankAndSelect, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
        { WebEditingCommandType::AlignCenter, executeJustifyCenter, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled },
    };
    // Handles all commands except WebEditingCommandType::Invalid.
    static_assert(arraysize(kEditorCommands) + 1 == static_cast<size_t>(WebEditingCommandType::NumberOfCommandTypes), "must handle all valid WebEditingCommandType");

    WebEditingCommandType commandType = WebEditingCommandTypeFromCommandName(commandName);
    if (commandType == WebEditingCommandType::Invalid)
        return nullptr;

    int commandIndex = static_cast<int>(commandType) - 1;
    DCHECK(commandIndex >= 0 && commandIndex < static_cast<int>(arraysize(kEditorCommands)));
    return &kEditorCommands[commandIndex];
}

Editor::Command Editor::createCommand(const String& commandName)
{
    return Command(internalCommand(commandName), CommandFromMenuOrKeyBinding, m_frame);
}

Editor::Command Editor::createCommand(const String& commandName, EditorCommandSource source)
{
    return Command(internalCommand(commandName), source, m_frame);
}

bool Editor::executeCommand(const String& commandName)
{
    // Specially handling commands that Editor::execCommand does not directly
    // support.
    if (commandName == "DeleteToEndOfParagraph") {
        if (!deleteWithDirection(DirectionForward, ParagraphBoundary, true, false))
            deleteWithDirection(DirectionForward, CharacterGranularity, true, false);
        return true;
    }
    if (commandName == "DeleteBackward")
        return createCommand(AtomicString("BackwardDelete")).execute();
    if (commandName == "DeleteForward")
        return createCommand(AtomicString("ForwardDelete")).execute();
    if (commandName == "AdvanceToNextMisspelling") {
        // Wee need to pass false here or else the currently selected word will never be skipped.
        spellChecker().advanceToNextMisspelling(false);
        return true;
    }
    if (commandName == "ToggleSpellPanel") {
        spellChecker().showSpellingGuessPanel();
        return true;
    }
    return createCommand(commandName).execute();
}

bool Editor::executeCommand(const String& commandName, const String& value)
{
    // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit for editable nodes.
    if (!canEdit() && commandName == "moveToBeginningOfDocument")
        return frame().eventHandler().bubblingScroll(ScrollUpIgnoringWritingMode, ScrollByDocument);

    if (!canEdit() && commandName == "moveToEndOfDocument")
        return frame().eventHandler().bubblingScroll(ScrollDownIgnoringWritingMode, ScrollByDocument);

    if (commandName == "showGuessPanel") {
        spellChecker().showSpellingGuessPanel();
        return true;
    }

    return createCommand(commandName).execute(value);
}

Editor::Command::Command()
    : m_command(0)
{
}

Editor::Command::Command(const EditorInternalCommand* command, EditorCommandSource source, LocalFrame* frame)
    : m_command(command)
    , m_source(source)
    , m_frame(command ? frame : nullptr)
{
    // Use separate assertions so we can tell which bad thing happened.
    if (!command)
        DCHECK(!m_frame);
    else
        DCHECK(m_frame);
}

bool Editor::Command::execute(const String& parameter, Event* triggeringEvent) const
{
    // TODO(yosin) We should move this logic into |canExecute()| member function
    // in |EditorInternalCommand| to replace |allowExecutionWhenDisabled|.
    // |allowExecutionWhenDisabled| is for "Copy", "Cut" and "Paste" commands
    // only.
    if (!isEnabled(triggeringEvent)) {
        // Let certain commands be executed when performed explicitly even if they are disabled.
        if (!isSupported() || !m_frame || !m_command->allowExecutionWhenDisabled)
            return false;
    }

    if (m_source == CommandFromMenuOrKeyBinding) {
        InputEvent::InputType inputType = InputTypeFromCommandType(m_command->commandType);
        if (inputType != InputEvent::InputType::None) {
            if (dispatchBeforeInputEditorCommand(eventTargetNodeForDocument(m_frame->document()), inputType, emptyString(), getRanges()) != DispatchEventResult::NotCanceled)
                return true;
        }
    }

    // 'beforeinput' event handler may destroy |frame()|.
    if (!m_frame || !frame().document())
        return false;

    frame().document()->updateStyleAndLayoutIgnorePendingStylesheets();
    DEFINE_STATIC_LOCAL(SparseHistogram, commandHistogram, ("WebCore.Editing.Commands"));
    commandHistogram.sample(static_cast<int>(m_command->commandType));
    return m_command->execute(*m_frame, triggeringEvent, m_source, parameter);
}

bool Editor::Command::execute(Event* triggeringEvent) const
{
    return execute(String(), triggeringEvent);
}

bool Editor::Command::isSupported() const
{
    if (!m_command)
        return false;
    switch (m_source) {
    case CommandFromMenuOrKeyBinding:
        return true;
    case CommandFromDOM:
        return m_command->isSupportedFromDOM(m_frame.get());
    }
    NOTREACHED();
    return false;
}

bool Editor::Command::isEnabled(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame)
        return false;
    return m_command->isEnabled(*m_frame, triggeringEvent, m_source);
}

TriState Editor::Command::state(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame)
        return FalseTriState;
    return m_command->state(*m_frame, triggeringEvent);
}

String Editor::Command::value(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame)
        return String();
    if (m_command->value == valueNull && m_command->state != stateNone)
        return m_command->state(*m_frame, triggeringEvent) == TrueTriState ? "true" : "false";
    return m_command->value(*m_frame, triggeringEvent);
}

bool Editor::Command::isTextInsertion() const
{
    return m_command && m_command->isTextInsertion;
}

int Editor::Command::idForHistogram() const
{
    return isSupported() ? static_cast<int>(m_command->commandType) : 0;
}

RangeVector* Editor::Command::getRanges() const
{
    if (!isSupported() || !m_frame)
        return nullptr;

    switch (m_command->commandType) {
    case WebEditingCommandType::Delete:
    case WebEditingCommandType::DeleteBackward:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionBackward, CharacterGranularity);
    case WebEditingCommandType::DeleteForward:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionForward, CharacterGranularity);
    case WebEditingCommandType::DeleteToBeginningOfLine:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionBackward, LineGranularity);
    case WebEditingCommandType::DeleteToBeginningOfParagraph:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionBackward, ParagraphGranularity);
    case WebEditingCommandType::DeleteToEndOfLine:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionForward, LineGranularity);
    case WebEditingCommandType::DeleteToEndOfParagraph:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionForward, ParagraphGranularity);
    case WebEditingCommandType::DeleteWordBackward:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionBackward, WordGranularity);
    case WebEditingCommandType::DeleteWordForward:
        return RangesFromCurrentSelectionOrExtendCaret(*m_frame, DirectionForward, WordGranularity);
    default:
        return nullptr;
    }
}

} // namespace blink
