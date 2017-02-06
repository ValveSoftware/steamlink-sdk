/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "core/editing/commands/CompositeEditCommand.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/commands/AppendNodeCommand.h"
#include "core/editing/commands/ApplyStyleCommand.h"
#include "core/editing/commands/DeleteFromTextNodeCommand.h"
#include "core/editing/commands/DeleteSelectionCommand.h"
#include "core/editing/commands/InsertIntoTextNodeCommand.h"
#include "core/editing/commands/InsertLineBreakCommand.h"
#include "core/editing/commands/InsertNodeBeforeCommand.h"
#include "core/editing/commands/InsertParagraphSeparatorCommand.h"
#include "core/editing/commands/MergeIdenticalElementsCommand.h"
#include "core/editing/commands/RemoveCSSPropertyCommand.h"
#include "core/editing/commands/RemoveNodeCommand.h"
#include "core/editing/commands/RemoveNodePreservingChildrenCommand.h"
#include "core/editing/commands/ReplaceNodeWithSpanCommand.h"
#include "core/editing/commands/ReplaceSelectionCommand.h"
#include "core/editing/commands/SetNodeAttributeCommand.h"
#include "core/editing/commands/SplitElementCommand.h"
#include "core/editing/commands/SplitTextNodeCommand.h"
#include "core/editing/commands/SplitTextNodeContainingElementCommand.h"
#include "core/editing/commands/WrapContentsInDummySpanCommand.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/ScopedEventQueue.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLLIElement.h"
#include "core/html/HTMLQuoteElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutListItem.h"
#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include <algorithm>

namespace blink {

using namespace HTMLNames;

EditCommandComposition* EditCommandComposition::create(Document* document,
    const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction editAction)
{
    return new EditCommandComposition(document, startingSelection, endingSelection, editAction);
}

EditCommandComposition::EditCommandComposition(Document* document, const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction editAction)
    : m_document(document)
    , m_startingSelection(startingSelection)
    , m_endingSelection(endingSelection)
    , m_startingRootEditableElement(startingSelection.rootEditableElement())
    , m_endingRootEditableElement(endingSelection.rootEditableElement())
    , m_editAction(editAction)
{
}

bool EditCommandComposition::belongsTo(const LocalFrame& frame) const
{
    DCHECK(m_document);
    return m_document->frame() == &frame;
}

void EditCommandComposition::unapply()
{
    DCHECK(m_document);
    LocalFrame* frame = m_document->frame();
    DCHECK(frame);

    // Changes to the document may have been made since the last editing operation that require a layout, as in <rdar://problem/5658603>.
    // Low level operations, like RemoveNodeCommand, don't require a layout because the high level operations that use them perform one
    // if one is necessary (like for the creation of VisiblePositions).
    m_document->updateStyleAndLayoutIgnorePendingStylesheets();

    {
        size_t size = m_commands.size();
        for (size_t i = size; i; --i)
            m_commands[i - 1]->doUnapply();
    }

    frame->editor().unappliedEditing(this);
}

void EditCommandComposition::reapply()
{
    DCHECK(m_document);
    LocalFrame* frame = m_document->frame();
    DCHECK(frame);

    // Changes to the document may have been made since the last editing operation that require a layout, as in <rdar://problem/5658603>.
    // Low level operations, like RemoveNodeCommand, don't require a layout because the high level operations that use them perform one
    // if one is necessary (like for the creation of VisiblePositions).
    m_document->updateStyleAndLayoutIgnorePendingStylesheets();

    {
        for (const auto& command : m_commands)
            command->doReapply();
    }

    frame->editor().reappliedEditing(this);
}

void EditCommandComposition::append(SimpleEditCommand* command)
{
    m_commands.append(command);
}

void EditCommandComposition::setStartingSelection(const VisibleSelection& selection)
{
    m_startingSelection = selection;
    m_startingRootEditableElement = selection.rootEditableElement();
}

void EditCommandComposition::setEndingSelection(const VisibleSelection& selection)
{
    m_endingSelection = selection;
    m_endingRootEditableElement = selection.rootEditableElement();
}

DEFINE_TRACE(EditCommandComposition)
{
    visitor->trace(m_document);
    visitor->trace(m_startingSelection);
    visitor->trace(m_endingSelection);
    visitor->trace(m_commands);
    visitor->trace(m_startingRootEditableElement);
    visitor->trace(m_endingRootEditableElement);
    UndoStep::trace(visitor);
}

CompositeEditCommand::CompositeEditCommand(Document& document)
    : EditCommand(document)
{
}

CompositeEditCommand::~CompositeEditCommand()
{
    DCHECK(isTopLevelCommand() || !m_composition);
}

bool CompositeEditCommand::apply()
{
    if (!endingSelection().isContentRichlyEditable()) {
        switch (editingAction()) {
        case EditActionTyping:
        case EditActionPaste:
        case EditActionDrag:
        case EditActionSetWritingDirection:
        case EditActionCut:
        case EditActionUnspecified:
            break;
        default:
            NOTREACHED();
            return false;
        }
    }
    ensureComposition();

    // Changes to the document may have been made since the last editing operation that require a layout, as in <rdar://problem/5658603>.
    // Low level operations, like RemoveNodeCommand, don't require a layout because the high level operations that use them perform one
    // if one is necessary (like for the creation of VisiblePositions).
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    LocalFrame* frame = document().frame();
    DCHECK(frame);
    EditingState editingState;
    {
        EventQueueScope eventQueueScope;
        doApply(&editingState);
    }

    // Only need to call appliedEditing for top-level commands,
    // and TypingCommands do it on their own (see TypingCommand::typingAddedToOpenCommand).
    if (!isTypingCommand())
        frame->editor().appliedEditing(this);
    setShouldRetainAutocorrectionIndicator(false);
    return !editingState.isAborted();
}

EditCommandComposition* CompositeEditCommand::ensureComposition()
{
    CompositeEditCommand* command = this;
    while (command && command->parent())
        command = command->parent();
    if (!command->m_composition)
        command->m_composition = EditCommandComposition::create(&document(), startingSelection(), endingSelection(), editingAction());
    return command->m_composition.get();
}

bool CompositeEditCommand::preservesTypingStyle() const
{
    return false;
}

bool CompositeEditCommand::isTypingCommand() const
{
    return false;
}

bool CompositeEditCommand::isReplaceSelectionCommand() const
{
    return false;
}

void CompositeEditCommand::setShouldRetainAutocorrectionIndicator(bool)
{
}

//
// sugary-sweet convenience functions to help create and apply edit commands in composite commands
//
void CompositeEditCommand::applyCommandToComposite(EditCommand* command, EditingState* editingState)
{
    command->setParent(this);
    command->doApply(editingState);
    if (editingState->isAborted()) {
        command->setParent(nullptr);
        return;
    }
    if (command->isSimpleEditCommand()) {
        command->setParent(0);
        ensureComposition()->append(toSimpleEditCommand(command));
    }
    m_commands.append(command);
}

void CompositeEditCommand::applyCommandToComposite(CompositeEditCommand* command, const VisibleSelection& selection, EditingState* editingState)
{
    command->setParent(this);
    if (selection != command->endingSelection()) {
        command->setStartingSelection(selection);
        command->setEndingSelection(selection);
    }
    command->doApply(editingState);
    if (!editingState->isAborted())
        m_commands.append(command);
}

void CompositeEditCommand::applyStyle(const EditingStyle* style, EditingState* editingState)
{
    applyCommandToComposite(ApplyStyleCommand::create(document(), style, EditActionChangeAttributes), editingState);
}

void CompositeEditCommand::applyStyle(const EditingStyle* style, const Position& start, const Position& end, EditingState* editingState)
{
    applyCommandToComposite(ApplyStyleCommand::create(document(), style, start, end), editingState);
}

void CompositeEditCommand::applyStyledElement(Element* element, EditingState* editingState)
{
    applyCommandToComposite(ApplyStyleCommand::create(element, false), editingState);
}

void CompositeEditCommand::removeStyledElement(Element* element, EditingState* editingState)
{
    applyCommandToComposite(ApplyStyleCommand::create(element, true), editingState);
}

void CompositeEditCommand::insertParagraphSeparator(EditingState* editingState, bool useDefaultParagraphElement, bool pasteBlockqutoeIntoUnquotedArea)
{
    applyCommandToComposite(InsertParagraphSeparatorCommand::create(document(), useDefaultParagraphElement, pasteBlockqutoeIntoUnquotedArea), editingState);
}

bool CompositeEditCommand::isRemovableBlock(const Node* node)
{
    DCHECK(node);
    if (!isHTMLDivElement(*node))
        return false;

    const HTMLDivElement& element = toHTMLDivElement(*node);
    ContainerNode* parentNode = element.parentNode();
    if (parentNode && parentNode->firstChild() != parentNode->lastChild())
        return false;

    if (!element.hasAttributes())
        return true;

    return false;
}

void CompositeEditCommand::insertNodeBefore(Node* insertChild, Node* refChild, EditingState* editingState, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    DCHECK_NE(document().body(), refChild);
    ABORT_EDITING_COMMAND_IF(!refChild->parentNode()->hasEditableStyle() && refChild->parentNode()->inActiveDocument());
    applyCommandToComposite(InsertNodeBeforeCommand::create(insertChild, refChild, shouldAssumeContentIsAlwaysEditable), editingState);
}

void CompositeEditCommand::insertNodeAfter(Node* insertChild, Node* refChild, EditingState* editingState)
{
    DCHECK(insertChild);
    DCHECK(refChild);
    DCHECK_NE(document().body(), refChild);
    ContainerNode* parent = refChild->parentNode();
    DCHECK(parent);
    DCHECK(!parent->isShadowRoot()) << parent;
    if (parent->lastChild() == refChild) {
        appendNode(insertChild, parent, editingState);
    } else {
        DCHECK(refChild->nextSibling()) << refChild;
        insertNodeBefore(insertChild, refChild->nextSibling(), editingState);
    }
}

void CompositeEditCommand::insertNodeAt(Node* insertChild, const Position& editingPosition, EditingState* editingState)
{
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    ABORT_EDITING_COMMAND_IF(!isEditablePosition(editingPosition, ContentIsEditable));
    // For editing positions like [table, 0], insert before the table,
    // likewise for replaced elements, brs, etc.
    Position p = editingPosition.parentAnchoredEquivalent();
    Node* refChild = p.anchorNode();
    int offset = p.offsetInContainerNode();

    if (canHaveChildrenForEditing(refChild)) {
        Node* child = refChild->firstChild();
        for (int i = 0; child && i < offset; i++)
            child = child->nextSibling();
        if (child)
            insertNodeBefore(insertChild, child, editingState);
        else
            appendNode(insertChild, toContainerNode(refChild), editingState);
    } else if (caretMinOffset(refChild) >= offset) {
        insertNodeBefore(insertChild, refChild, editingState);
    } else if (refChild->isTextNode() && caretMaxOffset(refChild) > offset) {
        splitTextNode(toText(refChild), offset);

        // Mutation events (bug 22634) from the text node insertion may have removed the refChild
        if (!refChild->inShadowIncludingDocument())
            return;
        insertNodeBefore(insertChild, refChild, editingState);
    } else {
        insertNodeAfter(insertChild, refChild, editingState);
    }
}

void CompositeEditCommand::appendNode(Node* node, ContainerNode* parent, EditingState* editingState)
{
    // When cloneParagraphUnderNewElement() clones the fallback content
    // of an OBJECT element, the ASSERT below may fire since the return
    // value of canHaveChildrenForEditing is not reliable until the layout
    // object of the OBJECT is created. Hence we ignore this check for OBJECTs.
    // TODO(yosin): We should move following |ABORT_EDITING_COMMAND_IF|s to
    // |AppendNodeCommand|.
    // TODO(yosin): We should get rid of |canHaveChildrenForEditing()|, since
    // |cloneParagraphUnderNewElement()| attempt to clone non-well-formed HTML,
    // produced by JavaScript.
    ABORT_EDITING_COMMAND_IF(!canHaveChildrenForEditing(parent)
        && !(parent->isElementNode() && toElement(parent)->tagQName() == objectTag));
    ABORT_EDITING_COMMAND_IF(!parent->hasEditableStyle() && parent->inActiveDocument());
    applyCommandToComposite(AppendNodeCommand::create(parent, node), editingState);
}

void CompositeEditCommand::removeChildrenInRange(Node* node, unsigned from, unsigned to, EditingState* editingState)
{
    HeapVector<Member<Node>> children;
    Node* child = NodeTraversal::childAt(*node, from);
    for (unsigned i = from; child && i < to; i++, child = child->nextSibling())
        children.append(child);

    size_t size = children.size();
    for (size_t i = 0; i < size; ++i) {
        removeNode(children[i].release(), editingState);
        if (editingState->isAborted())
            return;
    }
}

void CompositeEditCommand::removeNode(Node* node, EditingState* editingState, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    if (!node || !node->nonShadowBoundaryParentNode())
        return;
    ABORT_EDITING_COMMAND_IF(!node->document().frame());
    applyCommandToComposite(RemoveNodeCommand::create(node, shouldAssumeContentIsAlwaysEditable), editingState);
}

void CompositeEditCommand::removeNodePreservingChildren(Node* node, EditingState* editingState, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable)
{
    ABORT_EDITING_COMMAND_IF(!node->document().frame());
    applyCommandToComposite(RemoveNodePreservingChildrenCommand::create(node, shouldAssumeContentIsAlwaysEditable), editingState);
}

void CompositeEditCommand::removeNodeAndPruneAncestors(Node* node, EditingState* editingState, Node* excludeNode)
{
    DCHECK_NE(node, excludeNode);
    ContainerNode* parent = node->parentNode();
    removeNode(node, editingState);
    if (editingState->isAborted())
        return;
    prune(parent, editingState, excludeNode);
}

void CompositeEditCommand::moveRemainingSiblingsToNewParent(Node* node, Node* pastLastNodeToMove, Element* newParent, EditingState* editingState)
{
    NodeVector nodesToRemove;

    for (; node && node != pastLastNodeToMove; node = node->nextSibling())
        nodesToRemove.append(node);

    for (unsigned i = 0; i < nodesToRemove.size(); i++) {
        removeNode(nodesToRemove[i], editingState);
        if (editingState->isAborted())
            return;
        appendNode(nodesToRemove[i], newParent, editingState);
        if (editingState->isAborted())
            return;
    }
}

void CompositeEditCommand::updatePositionForNodeRemovalPreservingChildren(Position& position, Node& node)
{
    int offset = position.isOffsetInAnchor() ? position.offsetInContainerNode() : 0;
    updatePositionForNodeRemoval(position, node);
    if (offset == 0)
        return;
    position = Position(position.computeContainerNode(), offset);
}

HTMLSpanElement* CompositeEditCommand::replaceElementWithSpanPreservingChildrenAndAttributes(HTMLElement* node)
{
    // It would also be possible to implement all of ReplaceNodeWithSpanCommand
    // as a series of existing smaller edit commands.  Someone who wanted to
    // reduce the number of edit commands could do so here.
    ReplaceNodeWithSpanCommand* command = ReplaceNodeWithSpanCommand::create(node);
    // ReplaceNodeWithSpanCommand is never aborted.
    applyCommandToComposite(command, ASSERT_NO_EDITING_ABORT);
    // Returning a raw pointer here is OK because the command is retained by
    // applyCommandToComposite (thus retaining the span), and the span is also
    // in the DOM tree, and thus alive whie it has a parent.
    DCHECK(command->spanElement()->inShadowIncludingDocument()) << command->spanElement();
    return command->spanElement();
}

void CompositeEditCommand::prune(Node* node, EditingState* editingState, Node* excludeNode)
{
    if (Node* highestNodeToRemove = highestNodeToRemoveInPruning(node, excludeNode))
        removeNode(highestNodeToRemove, editingState);
}

void CompositeEditCommand::splitTextNode(Text* node, unsigned offset)
{
    // SplitTextNodeCommand is never aborted.
    applyCommandToComposite(SplitTextNodeCommand::create(node, offset), ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::splitElement(Element* element, Node* atChild)
{
    // SplitElementCommand is never aborted.
    applyCommandToComposite(SplitElementCommand::create(element, atChild), ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::mergeIdenticalElements(Element* first, Element* second, EditingState* editingState)
{
    DCHECK(!first->isDescendantOf(second)) << first << " " << second;
    DCHECK_NE(second, first);
    if (first->nextSibling() != second) {
        removeNode(second, editingState);
        if (editingState->isAborted())
            return;
        insertNodeAfter(second, first, editingState);
        if (editingState->isAborted())
            return;
    }
    applyCommandToComposite(MergeIdenticalElementsCommand::create(first, second), editingState);
}

void CompositeEditCommand::wrapContentsInDummySpan(Element* element)
{
    // WrapContentsInDummySpanCommand is never aborted.
    applyCommandToComposite(WrapContentsInDummySpanCommand::create(element), ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::splitTextNodeContainingElement(Text* text, unsigned offset)
{
    // SplitTextNodeContainingElementCommand is never aborted.
    applyCommandToComposite(SplitTextNodeContainingElementCommand::create(text, offset), ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::insertTextIntoNode(Text* node, unsigned offset, const String& text)
{
    // InsertIntoTextNodeCommand is never aborted.
    if (!text.isEmpty())
        applyCommandToComposite(InsertIntoTextNodeCommand::create(node, offset, text), ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::deleteTextFromNode(Text* node, unsigned offset, unsigned count)
{
    // DeleteFromTextNodeCommand is never aborted.
    applyCommandToComposite(DeleteFromTextNodeCommand::create(node, offset, count), ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::replaceTextInNode(Text* node, unsigned offset, unsigned count, const String& replacementText)
{
    // DeleteFromTextNodeCommand and InsertIntoTextNodeCommand are never
    // aborted.
    applyCommandToComposite(DeleteFromTextNodeCommand::create(node, offset, count), ASSERT_NO_EDITING_ABORT);
    if (!replacementText.isEmpty())
        applyCommandToComposite(InsertIntoTextNodeCommand::create(node, offset, replacementText), ASSERT_NO_EDITING_ABORT);
}

Position CompositeEditCommand::replaceSelectedTextInNode(const String& text)
{
    Position start = endingSelection().start();
    Position end = endingSelection().end();
    if (start.computeContainerNode() != end.computeContainerNode() || !start.computeContainerNode()->isTextNode() || isTabHTMLSpanElementTextNode(start.computeContainerNode()))
        return Position();

    Text* textNode = toText(start.computeContainerNode());
    replaceTextInNode(textNode, start.offsetInContainerNode(), end.offsetInContainerNode() - start.offsetInContainerNode(), text);

    return Position(textNode, start.offsetInContainerNode() + text.length());
}

static void copyMarkerTypesAndDescriptions(const DocumentMarkerVector& markerPointers, Vector<DocumentMarker::MarkerType>& types, Vector<String>& descriptions)
{
    size_t arraySize = markerPointers.size();
    types.reserveCapacity(arraySize);
    descriptions.reserveCapacity(arraySize);
    for (const auto& markerPointer : markerPointers) {
        types.append(markerPointer->type());
        descriptions.append(markerPointer->description());
    }
}

void CompositeEditCommand::replaceTextInNodePreservingMarkers(Text* node, unsigned offset, unsigned count, const String& replacementText)
{
    DocumentMarkerController& markerController = document().markers();
    Vector<DocumentMarker::MarkerType> types;
    Vector<String> descriptions;
    copyMarkerTypesAndDescriptions(markerController.markersInRange(EphemeralRange(Position(node, offset), Position(node, offset + count)), DocumentMarker::AllMarkers()), types, descriptions);
    replaceTextInNode(node, offset, count, replacementText);
    Position startPosition(node, offset);
    Position endPosition(node, offset + replacementText.length());
    DCHECK_EQ(types.size(), descriptions.size());
    for (size_t i = 0; i < types.size(); ++i)
        markerController.addMarker(startPosition, endPosition, types[i], descriptions[i]);
}

Position CompositeEditCommand::positionOutsideTabSpan(const Position& pos)
{
    if (!isTabHTMLSpanElementTextNode(pos.anchorNode()))
        return pos;

    switch (pos.anchorType()) {
    case PositionAnchorType::BeforeChildren:
    case PositionAnchorType::AfterChildren:
        NOTREACHED();
        return pos;
    case PositionAnchorType::OffsetInAnchor:
        break;
    case PositionAnchorType::BeforeAnchor:
        return Position::inParentBeforeNode(*pos.anchorNode());
    case PositionAnchorType::AfterAnchor:
        return Position::inParentAfterNode(*pos.anchorNode());
    }

    HTMLSpanElement* tabSpan = tabSpanElement(pos.computeContainerNode());
    DCHECK(tabSpan);

    if (pos.offsetInContainerNode() <= caretMinOffset(pos.computeContainerNode()))
        return Position::inParentBeforeNode(*tabSpan);

    if (pos.offsetInContainerNode() >= caretMaxOffset(pos.computeContainerNode()))
        return Position::inParentAfterNode(*tabSpan);

    splitTextNodeContainingElement(toText(pos.computeContainerNode()), pos.offsetInContainerNode());
    return Position::inParentBeforeNode(*tabSpan);
}

void CompositeEditCommand::insertNodeAtTabSpanPosition(Node* node, const Position& pos, EditingState* editingState)
{
    // insert node before, after, or at split of tab span
    insertNodeAt(node, positionOutsideTabSpan(pos), editingState);
}

void CompositeEditCommand::deleteSelection(EditingState* editingState, bool smartDelete, bool mergeBlocksAfterDelete, bool expandForSpecialElements, bool sanitizeMarkup)
{
    if (endingSelection().isRange())
        applyCommandToComposite(DeleteSelectionCommand::create(document(), smartDelete, mergeBlocksAfterDelete, expandForSpecialElements, sanitizeMarkup), editingState);
}

void CompositeEditCommand::deleteSelection(const VisibleSelection &selection, EditingState* editingState, bool smartDelete, bool mergeBlocksAfterDelete, bool expandForSpecialElements, bool sanitizeMarkup)
{
    if (selection.isRange())
        applyCommandToComposite(DeleteSelectionCommand::create(selection, smartDelete, mergeBlocksAfterDelete, expandForSpecialElements, sanitizeMarkup), editingState);
}

void CompositeEditCommand::removeCSSProperty(Element* element, CSSPropertyID property)
{
    // RemoveCSSPropertyCommand is never aborted.
    applyCommandToComposite(RemoveCSSPropertyCommand::create(document(), element, property), ASSERT_NO_EDITING_ABORT);
}

void CompositeEditCommand::removeElementAttribute(Element* element, const QualifiedName& attribute)
{
    setNodeAttribute(element, attribute, AtomicString());
}

void CompositeEditCommand::setNodeAttribute(Element* element, const QualifiedName& attribute, const AtomicString& value)
{
    // SetNodeAttributeCommand is never aborted.
    applyCommandToComposite(SetNodeAttributeCommand::create(element, attribute, value), ASSERT_NO_EDITING_ABORT);
}

static inline bool containsOnlyWhitespace(const String& text)
{
    for (unsigned i = 0; i < text.length(); ++i) {
        if (!isWhitespace(text[i]))
            return false;
    }

    return true;
}

bool CompositeEditCommand::shouldRebalanceLeadingWhitespaceFor(const String& text) const
{
    return containsOnlyWhitespace(text);
}

bool CompositeEditCommand::canRebalance(const Position& position) const
{
    Node* node = position.computeContainerNode();
    if (!position.isOffsetInAnchor() || !node || !node->isTextNode() || !node->layoutObjectIsRichlyEditable())
        return false;

    Text* textNode = toText(node);
    if (textNode->length() == 0)
        return false;

    LayoutText* layoutText = textNode->layoutObject();
    if (layoutText && !layoutText->style()->collapseWhiteSpace())
        return false;

    return true;
}

// FIXME: Doesn't go into text nodes that contribute adjacent text (siblings, cousins, etc).
void CompositeEditCommand::rebalanceWhitespaceAt(const Position& position)
{
    Node* node = position.computeContainerNode();
    if (!canRebalance(position))
        return;

    // If the rebalance is for the single offset, and neither text[offset] nor text[offset - 1] are some form of whitespace, do nothing.
    int offset = position.computeOffsetInContainerNode();
    String text = toText(node)->data();
    if (!isWhitespace(text[offset])) {
        offset--;
        if (offset < 0 || !isWhitespace(text[offset]))
            return;
    }

    rebalanceWhitespaceOnTextSubstring(toText(node), position.offsetInContainerNode(), position.offsetInContainerNode());
}

void CompositeEditCommand::rebalanceWhitespaceOnTextSubstring(Text* textNode, int startOffset, int endOffset)
{
    String text = textNode->data();
    DCHECK(!text.isEmpty());

    // Set upstream and downstream to define the extent of the whitespace surrounding text[offset].
    int upstream = startOffset;
    while (upstream > 0 && isWhitespace(text[upstream - 1]))
        upstream--;

    int downstream = endOffset;
    while ((unsigned)downstream < text.length() && isWhitespace(text[downstream]))
        downstream++;

    int length = downstream - upstream;
    if (!length)
        return;

    VisiblePosition visibleUpstreamPos = createVisiblePosition(Position(textNode, upstream));
    VisiblePosition visibleDownstreamPos = createVisiblePosition(Position(textNode, downstream));

    String string = text.substring(upstream, length);
    String rebalancedString = stringWithRebalancedWhitespace(string,
    // FIXME: Because of the problem mentioned at the top of this function, we
    // must also use nbsps at the start/end of the string because this function
    // doesn't get all surrounding whitespace, just the whitespace in the
    // current text node.
        isStartOfParagraph(visibleUpstreamPos) || upstream == 0,
        isEndOfParagraph(visibleDownstreamPos) || (unsigned)downstream == text.length());

    if (string != rebalancedString)
        replaceTextInNodePreservingMarkers(textNode, upstream, length, rebalancedString);
}

void CompositeEditCommand::prepareWhitespaceAtPositionForSplit(Position& position)
{
    if (!isRichlyEditablePosition(position))
        return;
    Node* node = position.anchorNode();
    if (!node || !node->isTextNode())
        return;
    Text* textNode = toText(node);

    if (textNode->length() == 0)
        return;
    LayoutText* layoutText = textNode->layoutObject();
    if (layoutText && !layoutText->style()->collapseWhiteSpace())
        return;

    // Delete collapsed whitespace so that inserting nbsps doesn't uncollapse it.
    Position upstreamPos = mostBackwardCaretPosition(position);
    deleteInsignificantText(upstreamPos, mostForwardCaretPosition(position));
    position = mostForwardCaretPosition(upstreamPos);

    VisiblePosition visiblePos = createVisiblePosition(position);
    VisiblePosition previousVisiblePos = previousPositionOf(visiblePos);
    replaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(previousVisiblePos);
    replaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(visiblePos);
}

void CompositeEditCommand::replaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(const VisiblePosition& visiblePosition)
{
    if (!isCollapsibleWhitespace(characterAfter(visiblePosition)))
        return;
    Position pos = mostForwardCaretPosition(visiblePosition.deepEquivalent());
    if (!pos.computeContainerNode() || !pos.computeContainerNode()->isTextNode())
        return;
    replaceTextInNodePreservingMarkers(toText(pos.computeContainerNode()), pos.offsetInContainerNode(), 1, nonBreakingSpaceString());
}

void CompositeEditCommand::rebalanceWhitespace()
{
    VisibleSelection selection = endingSelection();
    if (selection.isNone())
        return;

    rebalanceWhitespaceAt(selection.start());
    if (selection.isRange())
        rebalanceWhitespaceAt(selection.end());
}

void CompositeEditCommand::deleteInsignificantText(Text* textNode, unsigned start, unsigned end)
{
    if (!textNode || start >= end)
        return;

    document().updateStyleAndLayout();

    LayoutText* textLayoutObject = textNode->layoutObject();
    if (!textLayoutObject)
        return;

    Vector<InlineTextBox*> sortedTextBoxes;
    size_t sortedTextBoxesPosition = 0;

    for (InlineTextBox* textBox = textLayoutObject->firstTextBox(); textBox; textBox = textBox->nextTextBox())
        sortedTextBoxes.append(textBox);

    // If there is mixed directionality text, the boxes can be out of order,
    // (like Arabic with embedded LTR), so sort them first.
    if (textLayoutObject->containsReversedText())
        std::sort(sortedTextBoxes.begin(), sortedTextBoxes.end(), InlineTextBox::compareByStart);
    InlineTextBox* box = sortedTextBoxes.isEmpty() ? 0 : sortedTextBoxes[sortedTextBoxesPosition];

    if (!box) {
        // whole text node is empty
        // Removing a Text node won't dispatch synchronous events.
        removeNode(textNode, ASSERT_NO_EDITING_ABORT);
        return;
    }

    unsigned length = textNode->length();
    if (start >= length || end > length)
        return;

    unsigned removed = 0;
    InlineTextBox* prevBox = nullptr;
    String str;

    // This loop structure works to process all gaps preceding a box,
    // and also will look at the gap after the last box.
    while (prevBox || box) {
        unsigned gapStart = prevBox ? prevBox->start() + prevBox->len() : 0;
        if (end < gapStart) {
            // No more chance for any intersections
            break;
        }

        unsigned gapEnd = box ? box->start() : length;
        bool indicesIntersect = start <= gapEnd && end >= gapStart;
        int gapLen = gapEnd - gapStart;
        if (indicesIntersect && gapLen > 0) {
            gapStart = std::max(gapStart, start);
            if (str.isNull())
                str = textNode->data().substring(start, end - start);
            // remove text in the gap
            str.remove(gapStart - start - removed, gapLen);
            removed += gapLen;
        }

        prevBox = box;
        if (box) {
            if (++sortedTextBoxesPosition < sortedTextBoxes.size())
                box = sortedTextBoxes[sortedTextBoxesPosition];
            else
                box = 0;
        }
    }

    if (!str.isNull()) {
        // Replace the text between start and end with our pruned version.
        if (!str.isEmpty()) {
            replaceTextInNode(textNode, start, end - start, str);
        } else {
            // Assert that we are not going to delete all of the text in the node.
            // If we were, that should have been done above with the call to
            // removeNode and return.
            DCHECK(start > 0 || end - start < textNode->length());
            deleteTextFromNode(textNode, start, end - start);
        }
    }
}

void CompositeEditCommand::deleteInsignificantText(const Position& start, const Position& end)
{
    if (start.isNull() || end.isNull())
        return;

    if (comparePositions(start, end) >= 0)
        return;

    HeapVector<Member<Text>> nodes;
    for (Node& node : NodeTraversal::startsAt(*start.anchorNode())) {
        if (node.isTextNode())
            nodes.append(toText(&node));
        if (&node == end.anchorNode())
            break;
    }

    for (const auto& node : nodes) {
        Text* textNode = node;
        int startOffset = textNode == start.anchorNode() ? start.computeOffsetInContainerNode() : 0;
        int endOffset = textNode == end.anchorNode() ? end.computeOffsetInContainerNode() : static_cast<int>(textNode->length());
        deleteInsignificantText(textNode, startOffset, endOffset);
    }
}

void CompositeEditCommand::deleteInsignificantTextDownstream(const Position& pos)
{
    Position end = mostForwardCaretPosition(nextPositionOf(createVisiblePosition(pos, VP_DEFAULT_AFFINITY)).deepEquivalent());
    deleteInsignificantText(pos, end);
}

HTMLBRElement* CompositeEditCommand::appendBlockPlaceholder(Element* container, EditingState* editingState)
{
    if (!container)
        return nullptr;

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // Should assert isLayoutBlockFlow || isInlineFlow when deletion improves. See 4244964.
    DCHECK(container->layoutObject()) << container;

    HTMLBRElement* placeholder = HTMLBRElement::create(document());
    appendNode(placeholder, container, editingState);
    if (editingState->isAborted())
        return nullptr;
    return placeholder;
}

HTMLBRElement* CompositeEditCommand::insertBlockPlaceholder(const Position& pos, EditingState* editingState)
{
    if (pos.isNull())
        return nullptr;

    // Should assert isLayoutBlockFlow || isInlineFlow when deletion improves. See 4244964.
    DCHECK(pos.anchorNode()->layoutObject()) << pos;

    HTMLBRElement* placeholder = HTMLBRElement::create(document());
    insertNodeAt(placeholder, pos, editingState);
    if (editingState->isAborted())
        return nullptr;
    return placeholder;
}

HTMLBRElement* CompositeEditCommand::addBlockPlaceholderIfNeeded(Element* container, EditingState* editingState)
{
    if (!container)
        return nullptr;

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    LayoutObject* layoutObject = container->layoutObject();
    if (!layoutObject || !layoutObject->isLayoutBlockFlow())
        return nullptr;

    // append the placeholder to make sure it follows
    // any unrendered blocks
    LayoutBlockFlow* block = toLayoutBlockFlow(layoutObject);
    if (block->size().height() == 0 || (block->isListItem() && toLayoutListItem(block)->isEmpty()))
        return appendBlockPlaceholder(container, editingState);

    return nullptr;
}

// Assumes that the position is at a placeholder and does the removal without much checking.
void CompositeEditCommand::removePlaceholderAt(const Position& p)
{
    DCHECK(lineBreakExistsAtPosition(p)) << p;

    // We are certain that the position is at a line break, but it may be a br or a preserved newline.
    if (isHTMLBRElement(*p.anchorNode())) {
        // Removing a BR element won't dispatch synchronous events.
        removeNode(p.anchorNode(), ASSERT_NO_EDITING_ABORT);
        return;
    }

    deleteTextFromNode(toText(p.anchorNode()), p.offsetInContainerNode(), 1);
}

HTMLElement* CompositeEditCommand::insertNewDefaultParagraphElementAt(const Position& position, EditingState* editingState)
{
    HTMLElement* paragraphElement = createDefaultParagraphElement(document());
    paragraphElement->appendChild(HTMLBRElement::create(document()));
    insertNodeAt(paragraphElement, position, editingState);
    if (editingState->isAborted())
        return nullptr;
    return paragraphElement;
}

// If the paragraph is not entirely within it's own block, create one and move the paragraph into
// it, and return that block.  Otherwise return 0.
HTMLElement* CompositeEditCommand::moveParagraphContentsToNewBlockIfNecessary(const Position& pos, EditingState* editingState)
{
    DCHECK(isEditablePosition(pos, ContentIsEditable)) << pos;

    // It's strange that this function is responsible for verifying that pos has not been invalidated
    // by an earlier call to this function.  The caller, applyBlockStyle, should do this.
    VisiblePosition visiblePos = createVisiblePosition(pos, VP_DEFAULT_AFFINITY);
    VisiblePosition visibleParagraphStart = startOfParagraph(visiblePos);
    VisiblePosition visibleParagraphEnd = endOfParagraph(visiblePos);
    VisiblePosition next = nextPositionOf(visibleParagraphEnd);
    VisiblePosition visibleEnd = next.isNotNull() ? next : visibleParagraphEnd;

    Position upstreamStart = mostBackwardCaretPosition(visibleParagraphStart.deepEquivalent());
    Position upstreamEnd = mostBackwardCaretPosition(visibleEnd.deepEquivalent());

    // If there are no VisiblePositions in the same block as pos then
    // upstreamStart will be outside the paragraph
    if (comparePositions(pos, upstreamStart) < 0)
        return nullptr;

    // Perform some checks to see if we need to perform work in this function.
    if (isEnclosingBlock(upstreamStart.anchorNode())) {
        // If the block is the root editable element, always move content to a new block,
        // since it is illegal to modify attributes on the root editable element for editing.
        if (upstreamStart.anchorNode() == rootEditableElementOf(upstreamStart)) {
            // If the block is the root editable element and it contains no visible content, create a new
            // block but don't try and move content into it, since there's nothing for moveParagraphs to move.
            if (!hasRenderedNonAnonymousDescendantsWithHeight(upstreamStart.anchorNode()->layoutObject()))
                return insertNewDefaultParagraphElementAt(upstreamStart, editingState);
        } else if (isEnclosingBlock(upstreamEnd.anchorNode())) {
            if (!upstreamEnd.anchorNode()->isDescendantOf(upstreamStart.anchorNode())) {
                // If the paragraph end is a descendant of paragraph start, then we need to run
                // the rest of this function. If not, we can bail here.
                return nullptr;
            }
        } else if (enclosingBlock(upstreamEnd.anchorNode()) != upstreamStart.anchorNode()) {
            // It should be an ancestor of the paragraph start.
            // We can bail as we have a full block to work with.
            return nullptr;
        } else if (isEndOfEditableOrNonEditableContent(visibleEnd)) {
            // At the end of the editable region. We can bail here as well.
            return nullptr;
        }
    }

    if (visibleParagraphEnd.isNull())
        return nullptr;

    HTMLElement* newBlock = insertNewDefaultParagraphElementAt(upstreamStart, editingState);
    if (editingState->isAborted())
        return nullptr;

    bool endWasBr = isHTMLBRElement(*visibleParagraphEnd.deepEquivalent().anchorNode());

    // Inserting default paragraph element can change visible position. We
    // should update visible positions before use them.
    visiblePos = createVisiblePosition(pos, VP_DEFAULT_AFFINITY);
    visibleParagraphStart = startOfParagraph(visiblePos);
    visibleParagraphEnd = endOfParagraph(visiblePos);
    moveParagraphs(visibleParagraphStart, visibleParagraphEnd, VisiblePosition::firstPositionInNode(newBlock), editingState);
    if (editingState->isAborted())
        return nullptr;

    if (newBlock->lastChild() && isHTMLBRElement(*newBlock->lastChild()) && !endWasBr) {
        removeNode(newBlock->lastChild(), editingState);
        if (editingState->isAborted())
            return nullptr;
    }

    return newBlock;
}

void CompositeEditCommand::pushAnchorElementDown(Element* anchorNode, EditingState* editingState)
{
    if (!anchorNode)
        return;

    DCHECK(anchorNode->isLink()) << anchorNode;

    setEndingSelection(VisibleSelection::selectionFromContentsOfNode(anchorNode));
    applyStyledElement(anchorNode, editingState);
    if (editingState->isAborted())
        return;
    // Clones of anchorNode have been pushed down, now remove it.
    if (anchorNode->inShadowIncludingDocument())
        removeNodePreservingChildren(anchorNode, editingState);
}

// Clone the paragraph between start and end under blockElement,
// preserving the hierarchy up to outerNode.

void CompositeEditCommand::cloneParagraphUnderNewElement(const Position& start, const Position& end, Node* passedOuterNode, Element* blockElement, EditingState* editingState)
{
    DCHECK_LE(start, end);
    DCHECK(passedOuterNode);
    DCHECK(blockElement);

    // First we clone the outerNode
    Node* lastNode = nullptr;
    Node* outerNode = passedOuterNode;

    if (outerNode->isRootEditableElement()) {
        lastNode = blockElement;
    } else {
        lastNode = outerNode->cloneNode(isDisplayInsideTable(outerNode));
        appendNode(lastNode, blockElement, editingState);
        if (editingState->isAborted())
            return;
    }

    if (start.anchorNode() != outerNode && lastNode->isElementNode() && start.anchorNode()->isDescendantOf(outerNode)) {
        HeapVector<Member<Node>> ancestors;

        // Insert each node from innerNode to outerNode (excluded) in a list.
        for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*start.anchorNode())) {
            if (runner == outerNode)
                break;
            ancestors.append(runner);
        }

        // Clone every node between start.anchorNode() and outerBlock.

        for (size_t i = ancestors.size(); i != 0; --i) {
            Node* item = ancestors[i - 1].get();
            Node* child = item->cloneNode(isDisplayInsideTable(item));
            appendNode(child, toElement(lastNode), editingState);
            if (editingState->isAborted())
                return;
            lastNode = child;
        }
    }

    // Scripts specified in javascript protocol may remove |outerNode|
    // during insertion, e.g. <iframe src="javascript:...">
    if (!outerNode->inShadowIncludingDocument())
        return;

    // Handle the case of paragraphs with more than one node,
    // cloning all the siblings until end.anchorNode() is reached.

    if (start.anchorNode() != end.anchorNode() && !start.anchorNode()->isDescendantOf(end.anchorNode())) {
        // If end is not a descendant of outerNode we need to
        // find the first common ancestor to increase the scope
        // of our nextSibling traversal.
        while (outerNode && !end.anchorNode()->isDescendantOf(outerNode)) {
            outerNode = outerNode->parentNode();
        }

        if (!outerNode)
            return;

        Node* startNode = start.anchorNode();
        for (Node* node = NodeTraversal::nextSkippingChildren(*startNode, outerNode); node; node = NodeTraversal::nextSkippingChildren(*node, outerNode)) {
            // Move lastNode up in the tree as much as node was moved up in the
            // tree by NodeTraversal::nextSkippingChildren, so that the relative depth between
            // node and the original start node is maintained in the clone.
            while (startNode && lastNode && startNode->parentNode() != node->parentNode()) {
                startNode = startNode->parentNode();
                lastNode = lastNode->parentNode();
            }

            if (!lastNode || !lastNode->parentNode())
                return;

            Node* clonedNode = node->cloneNode(true);
            insertNodeAfter(clonedNode, lastNode, editingState);
            if (editingState->isAborted())
                return;
            lastNode = clonedNode;
            if (node == end.anchorNode() || end.anchorNode()->isDescendantOf(node))
                break;
        }
    }
}


// There are bugs in deletion when it removes a fully selected table/list.
// It expands and removes the entire table/list, but will let content
// before and after the table/list collapse onto one line.
// Deleting a paragraph will leave a placeholder. Remove it (and prune
// empty or unrendered parents).

void CompositeEditCommand::cleanupAfterDeletion(EditingState* editingState, VisiblePosition destination)
{
    VisiblePosition caretAfterDelete = endingSelection().visibleStart();
    Node* destinationNode = destination.deepEquivalent().anchorNode();
    if (caretAfterDelete.deepEquivalent() != destination.deepEquivalent() && isStartOfParagraph(caretAfterDelete) && isEndOfParagraph(caretAfterDelete)) {
        // Note: We want the rightmost candidate.
        Position position = mostForwardCaretPosition(caretAfterDelete.deepEquivalent());
        Node* node = position.anchorNode();

        // Bail if we'd remove an ancestor of our destination.
        if (destinationNode && destinationNode->isDescendantOf(node))
            return;

        // Normally deletion will leave a br as a placeholder.
        if (isHTMLBRElement(*node)) {
            removeNodeAndPruneAncestors(node, editingState, destinationNode);

            // If the selection to move was empty and in an empty block that
            // doesn't require a placeholder to prop itself open (like a bordered
            // div or an li), remove it during the move (the list removal code
            // expects this behavior).
        } else if (isEnclosingBlock(node)) {
            // If caret position after deletion and destination position coincides,
            // node should not be removed.
            if (!rendersInDifferentPosition(position, destination.deepEquivalent())) {
                prune(node, editingState, destinationNode);
                return;
            }
            removeNodeAndPruneAncestors(node, editingState, destinationNode);
        } else if (lineBreakExistsAtPosition(position)) {
            // There is a preserved '\n' at caretAfterDelete.
            // We can safely assume this is a text node.
            Text* textNode = toText(node);
            if (textNode->length() == 1)
                removeNodeAndPruneAncestors(node, editingState, destinationNode);
            else
                deleteTextFromNode(textNode, position.computeOffsetInContainerNode(), 1);
        }
    }
}

// This is a version of moveParagraph that preserves style by keeping the original markup
// It is currently used only by IndentOutdentCommand but it is meant to be used in the
// future by several other commands such as InsertList and the align commands.
// The blockElement parameter is the element to move the paragraph to,
// outerNode is the top element of the paragraph hierarchy.

void CompositeEditCommand::moveParagraphWithClones(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, HTMLElement* blockElement, Node* outerNode, EditingState* editingState)
{
    DCHECK(outerNode);
    DCHECK(blockElement);

    VisiblePosition beforeParagraph = previousPositionOf(startOfParagraphToMove);
    VisiblePosition afterParagraph = nextPositionOf(endOfParagraphToMove);

    // We upstream() the end and downstream() the start so that we don't include collapsed whitespace in the move.
    // When we paste a fragment, spaces after the end and before the start are treated as though they were rendered.
    Position start = mostForwardCaretPosition(startOfParagraphToMove.deepEquivalent());
    Position end = startOfParagraphToMove.deepEquivalent() == endOfParagraphToMove.deepEquivalent() ? start : mostBackwardCaretPosition(endOfParagraphToMove.deepEquivalent());
    if (comparePositions(start, end) > 0)
        end = start;

    cloneParagraphUnderNewElement(start, end, outerNode, blockElement, editingState);

    setEndingSelection(VisibleSelection(start, end));
    deleteSelection(editingState, false, false, false);
    if (editingState->isAborted())
        return;

    // There are bugs in deletion when it removes a fully selected table/list.
    // It expands and removes the entire table/list, but will let content
    // before and after the table/list collapse onto one line.

    cleanupAfterDeletion(editingState);
    if (editingState->isAborted())
        return;

    // Add a br if pruning an empty block level element caused a collapse.  For example:
    // foo^
    // <div>bar</div>
    // baz
    // Imagine moving 'bar' to ^.  'bar' will be deleted and its div pruned.  That would
    // cause 'baz' to collapse onto the line with 'foobar' unless we insert a br.
    // Must recononicalize these two VisiblePositions after the pruning above.
    // TODO(yosin): We should abort when |beforeParagraph| is a orphan when
    // we have a sample.
    beforeParagraph = createVisiblePosition(beforeParagraph.deepEquivalent());
    if (afterParagraph.isOrphan()) {
        editingState->abort();
        return;
    }
    afterParagraph = createVisiblePosition(afterParagraph.deepEquivalent());

    if (beforeParagraph.isNotNull() && !isDisplayInsideTable(beforeParagraph.deepEquivalent().anchorNode())
        && ((!isEndOfParagraph(beforeParagraph) && !isStartOfParagraph(beforeParagraph)) || beforeParagraph.deepEquivalent() == afterParagraph.deepEquivalent())) {
        // FIXME: Trim text between beforeParagraph and afterParagraph if they aren't equal.
        insertNodeAt(HTMLBRElement::create(document()), beforeParagraph.deepEquivalent(), editingState);
    }
}

void CompositeEditCommand::moveParagraph(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, const VisiblePosition& destination, EditingState* editingState, ShouldPreserveSelection shouldPreserveSelection, ShouldPreserveStyle shouldPreserveStyle, Node* constrainingAncestor)
{
    DCHECK(isStartOfParagraph(startOfParagraphToMove)) << startOfParagraphToMove;
    DCHECK(isEndOfParagraph(endOfParagraphToMove)) << endOfParagraphToMove;
    moveParagraphs(startOfParagraphToMove, endOfParagraphToMove, destination, editingState, shouldPreserveSelection, shouldPreserveStyle, constrainingAncestor);
}

void CompositeEditCommand::moveParagraphs(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, const VisiblePosition& destination, EditingState* editingState, ShouldPreserveSelection shouldPreserveSelection, ShouldPreserveStyle shouldPreserveStyle, Node* constrainingAncestor)
{
    if (startOfParagraphToMove.deepEquivalent() == destination.deepEquivalent() || startOfParagraphToMove.isNull())
        return;

    int startIndex = -1;
    int endIndex = -1;
    int destinationIndex = -1;
    bool originalIsDirectional = endingSelection().isDirectional();
    if (shouldPreserveSelection == PreserveSelection && !endingSelection().isNone()) {
        VisiblePosition visibleStart = endingSelection().visibleStart();
        VisiblePosition visibleEnd = endingSelection().visibleEnd();

        bool startAfterParagraph = comparePositions(visibleStart, endOfParagraphToMove) > 0;
        bool endBeforeParagraph = comparePositions(visibleEnd, startOfParagraphToMove) < 0;

        if (!startAfterParagraph && !endBeforeParagraph) {
            bool startInParagraph = comparePositions(visibleStart, startOfParagraphToMove) >= 0;
            bool endInParagraph = comparePositions(visibleEnd, endOfParagraphToMove) <= 0;

            startIndex = 0;
            if (startInParagraph)
                startIndex = TextIterator::rangeLength(startOfParagraphToMove.toParentAnchoredPosition(), visibleStart.toParentAnchoredPosition(), true);

            endIndex = 0;
            if (endInParagraph)
                endIndex = TextIterator::rangeLength(startOfParagraphToMove.toParentAnchoredPosition(), visibleEnd.toParentAnchoredPosition(), true);
        }
    }

    VisiblePosition beforeParagraph = previousPositionOf(startOfParagraphToMove, CannotCrossEditingBoundary);
    VisiblePosition afterParagraph = nextPositionOf(endOfParagraphToMove, CannotCrossEditingBoundary);

    // We upstream() the end and downstream() the start so that we don't include collapsed whitespace in the move.
    // When we paste a fragment, spaces after the end and before the start are treated as though they were rendered.
    Position start = mostForwardCaretPosition(startOfParagraphToMove.deepEquivalent());
    Position end = mostBackwardCaretPosition(endOfParagraphToMove.deepEquivalent());

    // FIXME: This is an inefficient way to preserve style on nodes in the paragraph to move. It
    // shouldn't matter though, since moved paragraphs will usually be quite small.
    DocumentFragment* fragment = startOfParagraphToMove.deepEquivalent() != endOfParagraphToMove.deepEquivalent() ?
        createFragmentFromMarkup(document(), createMarkup(start.parentAnchoredEquivalent(), end.parentAnchoredEquivalent(), DoNotAnnotateForInterchange, ConvertBlocksToInlines::Convert, DoNotResolveURLs, constrainingAncestor), "") : nullptr;

    // A non-empty paragraph's style is moved when we copy and move it.  We don't move
    // anything if we're given an empty paragraph, but an empty paragraph can have style
    // too, <div><b><br></b></div> for example.  Save it so that we can preserve it later.
    EditingStyle* styleInEmptyParagraph = nullptr;
    if (startOfParagraphToMove.deepEquivalent() == endOfParagraphToMove.deepEquivalent() && shouldPreserveStyle == PreserveStyle) {
        styleInEmptyParagraph = EditingStyle::create(startOfParagraphToMove.deepEquivalent());
        styleInEmptyParagraph->mergeTypingStyle(&document());
        // The moved paragraph should assume the block style of the destination.
        styleInEmptyParagraph->removeBlockProperties();
    }

    // FIXME (5098931): We should add a new insert action "WebViewInsertActionMoved" and call shouldInsertFragment here.

    setEndingSelection(VisibleSelection(start, end));
    document().frame()->spellChecker().clearMisspellingsAndBadGrammar(endingSelection());
    deleteSelection(editingState, false, false, false);
    if (editingState->isAborted())
        return;

    DCHECK(destination.deepEquivalent().inShadowIncludingDocument()) << destination;
    cleanupAfterDeletion(editingState, destination);
    if (editingState->isAborted())
        return;
    DCHECK(destination.deepEquivalent().inShadowIncludingDocument()) << destination;

    // Add a br if pruning an empty block level element caused a collapse. For example:
    // foo^
    // <div>bar</div>
    // baz
    // Imagine moving 'bar' to ^. 'bar' will be deleted and its div pruned. That would
    // cause 'baz' to collapse onto the line with 'foobar' unless we insert a br.
    // Must recononicalize these two VisiblePositions after the pruning above.
    beforeParagraph = createVisiblePosition(beforeParagraph.deepEquivalent());
    afterParagraph = createVisiblePosition(afterParagraph.deepEquivalent());
    if (beforeParagraph.isNotNull() && (!isEndOfParagraph(beforeParagraph) || beforeParagraph.deepEquivalent() == afterParagraph.deepEquivalent())) {
        // FIXME: Trim text between beforeParagraph and afterParagraph if they aren't equal.
        insertNodeAt(HTMLBRElement::create(document()), beforeParagraph.deepEquivalent(), editingState);
        if (editingState->isAborted())
            return;
        // Need an updateLayout here in case inserting the br has split a text node.
        document().updateStyleAndLayoutIgnorePendingStylesheets();
    }

    destinationIndex = TextIterator::rangeLength(Position::firstPositionInNode(document().documentElement()), destination.toParentAnchoredPosition(), true);

    setEndingSelection(VisibleSelection(destination, originalIsDirectional));
    DCHECK(!endingSelection().isNone());
    ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::SelectReplacement | ReplaceSelectionCommand::MovingParagraph;
    if (shouldPreserveStyle == DoNotPreserveStyle)
        options |= ReplaceSelectionCommand::MatchStyle;
    applyCommandToComposite(ReplaceSelectionCommand::create(document(), fragment, options), editingState);
    if (editingState->isAborted())
        return;

    document().frame()->spellChecker().markMisspellingsAndBadGrammar(endingSelection());

    // If the selection is in an empty paragraph, restore styles from the old empty paragraph to the new empty paragraph.
    bool selectionIsEmptyParagraph = endingSelection().isCaret() && isStartOfParagraph(endingSelection().visibleStart()) && isEndOfParagraph(endingSelection().visibleStart());
    if (styleInEmptyParagraph && selectionIsEmptyParagraph) {
        applyStyle(styleInEmptyParagraph, editingState);
        if (editingState->isAborted())
            return;
    }

    if (shouldPreserveSelection ==  DoNotPreserveSelection || startIndex == -1)
        return;
    Element* documentElement = document().documentElement();
    if (!documentElement)
        return;
    // Fragment creation (using createMarkup) incorrectly uses regular spaces
    // instead of nbsps for some spaces that were rendered (11475), which causes
    // spaces to be collapsed during the move operation. This results in a call
    // to rangeFromLocationAndLength with a location past the end of the
    // document (which will return null).
    EphemeralRange startRange = PlainTextRange(destinationIndex + startIndex).createRangeForSelection(*documentElement);
    if (startRange.isNull())
        return;
    EphemeralRange endRange = PlainTextRange(destinationIndex + endIndex).createRangeForSelection(*documentElement);
    if (endRange.isNull())
        return;
    setEndingSelection(VisibleSelection(startRange.startPosition(), endRange.startPosition(), TextAffinity::Downstream, originalIsDirectional));
}

// FIXME: Send an appropriate shouldDeleteRange call.
bool CompositeEditCommand::breakOutOfEmptyListItem(EditingState* editingState)
{
    Node* emptyListItem = enclosingEmptyListItem(endingSelection().visibleStart());
    if (!emptyListItem)
        return false;

    EditingStyle* style = EditingStyle::create(endingSelection().start());
    style->mergeTypingStyle(&document());

    ContainerNode* listNode = emptyListItem->parentNode();
    // FIXME: Can't we do something better when the immediate parent wasn't a list node?
    if (!listNode
        || (!isHTMLUListElement(*listNode) && !isHTMLOListElement(*listNode))
        || !listNode->hasEditableStyle()
        || listNode == emptyListItem->rootEditableElement())
        return false;

    HTMLElement* newBlock = nullptr;
    if (ContainerNode* blockEnclosingList = listNode->parentNode()) {
        if (isHTMLLIElement(*blockEnclosingList)) { // listNode is inside another list item
            if (visiblePositionAfterNode(*blockEnclosingList).deepEquivalent() == visiblePositionAfterNode(*listNode).deepEquivalent()) {
                // If listNode appears at the end of the outer list item, then move listNode outside of this list item
                // e.g. <ul><li>hello <ul><li><br></li></ul> </li></ul> should become <ul><li>hello</li> <ul><li><br></li></ul> </ul> after this section
                // If listNode does NOT appear at the end, then we should consider it as a regular paragraph.
                // e.g. <ul><li> <ul><li><br></li></ul> hello</li></ul> should become <ul><li> <div><br></div> hello</li></ul> at the end
                splitElement(toElement(blockEnclosingList), listNode);
                removeNodePreservingChildren(listNode->parentNode(), editingState);
                if (editingState->isAborted())
                    return false;
                newBlock = HTMLLIElement::create(document());
            }
            // If listNode does NOT appear at the end of the outer list item, then behave as if in a regular paragraph.
        } else if (isHTMLOListElement(*blockEnclosingList) || isHTMLUListElement(*blockEnclosingList)) {
            newBlock = HTMLLIElement::create(document());
        }
    }
    if (!newBlock)
        newBlock = createDefaultParagraphElement(document());

    Node* previousListNode = emptyListItem->isElementNode() ? ElementTraversal::previousSibling(*emptyListItem): emptyListItem->previousSibling();
    Node* nextListNode = emptyListItem->isElementNode() ? ElementTraversal::nextSibling(*emptyListItem): emptyListItem->nextSibling();
    if (isListItem(nextListNode) || isHTMLListElement(nextListNode)) {
        // If emptyListItem follows another list item or nested list, split the list node.
        if (isListItem(previousListNode) || isHTMLListElement(previousListNode))
            splitElement(toElement(listNode), emptyListItem);

        // If emptyListItem is followed by other list item or nested list, then insert newBlock before the list node.
        // Because we have splitted the element, emptyListItem is the first element in the list node.
        // i.e. insert newBlock before ul or ol whose first element is emptyListItem
        insertNodeBefore(newBlock, listNode, editingState);
        if (editingState->isAborted())
            return false;
        removeNode(emptyListItem, editingState);
        if (editingState->isAborted())
            return false;
    } else {
        // When emptyListItem does not follow any list item or nested list, insert newBlock after the enclosing list node.
        // Remove the enclosing node if emptyListItem is the only child; otherwise just remove emptyListItem.
        insertNodeAfter(newBlock, listNode, editingState);
        if (editingState->isAborted())
            return false;
        removeNode(isListItem(previousListNode) || isHTMLListElement(previousListNode) ? emptyListItem : listNode, editingState);
        if (editingState->isAborted())
            return false;
    }

    appendBlockPlaceholder(newBlock, editingState);
    if (editingState->isAborted())
        return false;
    setEndingSelection(VisibleSelection(Position::firstPositionInNode(newBlock), TextAffinity::Downstream, endingSelection().isDirectional()));

    style->prepareToApplyAt(endingSelection().start());
    if (!style->isEmpty()) {
        applyStyle(style, editingState);
        if (editingState->isAborted())
            return false;
    }

    return true;
}

// If the caret is in an empty quoted paragraph, and either there is nothing before that
// paragraph, or what is before is unquoted, and the user presses delete, unquote that paragraph.
bool CompositeEditCommand::breakOutOfEmptyMailBlockquotedParagraph(EditingState* editingState)
{
    if (!endingSelection().isCaret())
        return false;

    VisiblePosition caret = endingSelection().visibleStart();
    HTMLQuoteElement* highestBlockquote = toHTMLQuoteElement(highestEnclosingNodeOfType(caret.deepEquivalent(), &isMailHTMLBlockquoteElement));
    if (!highestBlockquote)
        return false;

    if (!isStartOfParagraph(caret) || !isEndOfParagraph(caret))
        return false;

    VisiblePosition previous = previousPositionOf(caret, CannotCrossEditingBoundary);
    // Only move forward if there's nothing before the caret, or if there's unquoted content before it.
    if (enclosingNodeOfType(previous.deepEquivalent(), &isMailHTMLBlockquoteElement))
        return false;

    HTMLBRElement* br = HTMLBRElement::create(document());
    // We want to replace this quoted paragraph with an unquoted one, so insert a br
    // to hold the caret before the highest blockquote.
    insertNodeBefore(br, highestBlockquote, editingState);
    if (editingState->isAborted())
        return false;
    VisiblePosition atBR = VisiblePosition::beforeNode(br);
    // If the br we inserted collapsed, for example foo<br><blockquote>...</blockquote>, insert
    // a second one.
    if (!isStartOfParagraph(atBR)) {
        insertNodeBefore(HTMLBRElement::create(document()), br, editingState);
        if (editingState->isAborted())
            return false;
    }
    setEndingSelection(VisibleSelection(atBR, endingSelection().isDirectional()));

    // If this is an empty paragraph there must be a line break here.
    if (!lineBreakExistsAtVisiblePosition(caret))
        return false;

    Position caretPos(mostForwardCaretPosition(caret.deepEquivalent()));
    // A line break is either a br or a preserved newline.
    DCHECK(isHTMLBRElement(caretPos.anchorNode()) || (caretPos.anchorNode()->isTextNode() && caretPos.anchorNode()->layoutObject()->style()->preserveNewline())) << caretPos;

    if (isHTMLBRElement(*caretPos.anchorNode())) {
        removeNodeAndPruneAncestors(caretPos.anchorNode(), editingState);
        if (editingState->isAborted())
            return false;
    } else if (caretPos.anchorNode()->isTextNode()) {
        DCHECK_EQ(caretPos.computeOffsetInContainerNode(), 0);
        Text* textNode = toText(caretPos.anchorNode());
        ContainerNode* parentNode = textNode->parentNode();
        // The preserved newline must be the first thing in the node, since otherwise the previous
        // paragraph would be quoted, and we verified that it wasn't above.
        deleteTextFromNode(textNode, 0, 1);
        prune(parentNode, editingState);
        if (editingState->isAborted())
            return false;
    }

    return true;
}

// Operations use this function to avoid inserting content into an anchor when at the start or the end of
// that anchor, as in NSTextView.
// FIXME: This is only an approximation of NSTextViews insertion behavior, which varies depending on how
// the caret was made.
Position CompositeEditCommand::positionAvoidingSpecialElementBoundary(const Position& original, EditingState* editingState)
{
    if (original.isNull())
        return original;

    VisiblePosition visiblePos = createVisiblePosition(original);
    Element* enclosingAnchor = enclosingAnchorElement(original);
    Position result = original;

    if (!enclosingAnchor)
        return result;

    // Don't avoid block level anchors, because that would insert content into the wrong paragraph.
    if (enclosingAnchor && !isEnclosingBlock(enclosingAnchor)) {
        VisiblePosition firstInAnchor = VisiblePosition::firstPositionInNode(enclosingAnchor);
        VisiblePosition lastInAnchor = VisiblePosition::lastPositionInNode(enclosingAnchor);
        // If visually just after the anchor, insert *inside* the anchor unless it's the last
        // VisiblePosition in the document, to match NSTextView.
        if (visiblePos.deepEquivalent() == lastInAnchor.deepEquivalent()) {
            // Make sure anchors are pushed down before avoiding them so that we don't
            // also avoid structural elements like lists and blocks (5142012).
            if (original.anchorNode() != enclosingAnchor && original.anchorNode()->parentNode() != enclosingAnchor) {
                pushAnchorElementDown(enclosingAnchor, editingState);
                if (editingState->isAborted())
                    return original;
                enclosingAnchor = enclosingAnchorElement(original);
                if (!enclosingAnchor)
                    return original;
            }
            // Don't insert outside an anchor if doing so would skip over a line break.  It would
            // probably be safe to move the line break so that we could still avoid the anchor here.
            Position downstream(mostForwardCaretPosition(visiblePos.deepEquivalent()));
            if (lineBreakExistsAtVisiblePosition(visiblePos) && downstream.anchorNode()->isDescendantOf(enclosingAnchor))
                return original;

            result = Position::inParentAfterNode(*enclosingAnchor);
        }
        // If visually just before an anchor, insert *outside* the anchor unless it's the first
        // VisiblePosition in a paragraph, to match NSTextView.
        if (visiblePos.deepEquivalent() == firstInAnchor.deepEquivalent()) {
            // Make sure anchors are pushed down before avoiding them so that we don't
            // also avoid structural elements like lists and blocks (5142012).
            if (original.anchorNode() != enclosingAnchor && original.anchorNode()->parentNode() != enclosingAnchor) {
                pushAnchorElementDown(enclosingAnchor, editingState);
                if (editingState->isAborted())
                    return original;
                enclosingAnchor = enclosingAnchorElement(original);
            }
            if (!enclosingAnchor)
                return original;

            result = Position::inParentBeforeNode(*enclosingAnchor);
        }
    }

    if (result.isNull() || !rootEditableElementOf(result))
        result = original;

    return result;
}

// Splits the tree parent by parent until we reach the specified ancestor. We use VisiblePositions
// to determine if the split is necessary. Returns the last split node.
Node* CompositeEditCommand::splitTreeToNode(Node* start, Node* end, bool shouldSplitAncestor)
{
    DCHECK(start);
    DCHECK(end);
    DCHECK_NE(start, end);

    if (shouldSplitAncestor && end->parentNode())
        end = end->parentNode();
    if (!start->isDescendantOf(end))
        return end;

    Node* endNode = end;
    Node* node = nullptr;
    for (node = start; node->parentNode() != endNode; node = node->parentNode()) {
        Element* parentElement = node->parentElement();
        if (!parentElement)
            break;
        // Do not split a node when doing so introduces an empty node.
        VisiblePosition positionInParent = VisiblePosition::firstPositionInNode(parentElement);
        VisiblePosition positionInNode = createVisiblePosition(firstPositionInOrBeforeNode(node));
        if (positionInParent.deepEquivalent() != positionInNode.deepEquivalent())
            splitElement(parentElement, node);
    }

    return node;
}

DEFINE_TRACE(CompositeEditCommand)
{
    visitor->trace(m_commands);
    visitor->trace(m_composition);
    EditCommand::trace(visitor);
}

} // namespace blink
