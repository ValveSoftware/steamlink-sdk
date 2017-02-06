/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CompositeEditCommand_h
#define CompositeEditCommand_h

#include "core/CSSPropertyNames.h"
#include "core/editing/commands/EditCommand.h"
#include "core/editing/commands/EditingState.h"
#include "core/editing/commands/UndoStep.h"
#include "wtf/Vector.h"

namespace blink {

class EditingStyle;
class Element;
class HTMLBRElement;
class HTMLElement;
class HTMLSpanElement;
class Text;

class EditCommandComposition final : public UndoStep {
public:
    static EditCommandComposition* create(Document*, const VisibleSelection&, const VisibleSelection&, EditAction);

    bool belongsTo(const LocalFrame&) const override;
    void unapply() override;
    void reapply() override;
    EditAction editingAction() const override { return m_editAction; }
    void append(SimpleEditCommand*);

    const VisibleSelection& startingSelection() const { return m_startingSelection; }
    const VisibleSelection& endingSelection() const { return m_endingSelection; }
    void setStartingSelection(const VisibleSelection&);
    void setEndingSelection(const VisibleSelection&);
    Element* startingRootEditableElement() const { return m_startingRootEditableElement.get(); }
    Element* endingRootEditableElement() const { return m_endingRootEditableElement.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    EditCommandComposition(Document*, const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction);

    Member<Document> m_document;
    VisibleSelection m_startingSelection;
    VisibleSelection m_endingSelection;
    HeapVector<Member<SimpleEditCommand>> m_commands;
    Member<Element> m_startingRootEditableElement;
    Member<Element> m_endingRootEditableElement;
    EditAction m_editAction;
};

class CompositeEditCommand : public EditCommand {
public:
    enum ShouldPreserveSelection { PreserveSelection, DoNotPreserveSelection };
    enum ShouldPreserveStyle { PreserveStyle, DoNotPreserveStyle };

    ~CompositeEditCommand() override;

    // Returns |false| if the command failed.  e.g. It's aborted.
    bool apply();
    bool isFirstCommand(EditCommand* command) { return !m_commands.isEmpty() && m_commands.first() == command; }
    EditCommandComposition* composition() { return m_composition.get(); }
    EditCommandComposition* ensureComposition();

    virtual bool isReplaceSelectionCommand() const;
    virtual bool isTypingCommand() const;
    virtual bool preservesTypingStyle() const;
    virtual void setShouldRetainAutocorrectionIndicator(bool);
    virtual bool shouldStopCaretBlinking() const { return false; }

    DECLARE_VIRTUAL_TRACE();

protected:
    explicit CompositeEditCommand(Document&);

    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(Node*, ContainerNode* parent, EditingState*);
    void applyCommandToComposite(EditCommand*, EditingState*);
    void applyCommandToComposite(CompositeEditCommand*, const VisibleSelection&, EditingState*);
    void applyStyle(const EditingStyle*, EditingState*);
    void applyStyle(const EditingStyle*, const Position& start, const Position& end, EditingState*);
    void applyStyledElement(Element*, EditingState*);
    void removeStyledElement(Element*, EditingState*);
    void deleteSelection(EditingState*, bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool expandForSpecialElements = true, bool sanitizeMarkup = true);
    void deleteSelection(const VisibleSelection&, EditingState*, bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool expandForSpecialElements = true, bool sanitizeMarkup = true);
    virtual void deleteTextFromNode(Text*, unsigned offset, unsigned count);
    bool isRemovableBlock(const Node*);
    void insertNodeAfter(Node*, Node* refChild, EditingState*);
    void insertNodeAt(Node*, const Position&, EditingState*);
    void insertNodeAtTabSpanPosition(Node*, const Position&, EditingState*);
    void insertNodeBefore(Node*, Node* refChild, EditingState*, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    void insertParagraphSeparator(EditingState*, bool useDefaultParagraphElement = false, bool pasteBlockqutoeIntoUnquotedArea = false);
    void insertTextIntoNode(Text*, unsigned offset, const String& text);
    void mergeIdenticalElements(Element*, Element*, EditingState*);
    void rebalanceWhitespace();
    void rebalanceWhitespaceAt(const Position&);
    void rebalanceWhitespaceOnTextSubstring(Text*, int startOffset, int endOffset);
    void prepareWhitespaceAtPositionForSplit(Position&);
    void replaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(const VisiblePosition&);
    bool canRebalance(const Position&) const;
    bool shouldRebalanceLeadingWhitespaceFor(const String&) const;
    void removeCSSProperty(Element*, CSSPropertyID);
    void removeElementAttribute(Element*, const QualifiedName& attribute);
    void removeChildrenInRange(Node*, unsigned from, unsigned to, EditingState*);
    virtual void removeNode(Node*, EditingState*, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    HTMLSpanElement* replaceElementWithSpanPreservingChildrenAndAttributes(HTMLElement*);
    void removeNodePreservingChildren(Node*, EditingState*, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    void removeNodeAndPruneAncestors(Node*, EditingState*, Node* excludeNode = nullptr);
    void moveRemainingSiblingsToNewParent(Node*, Node* pastLastNodeToMove, Element* newParent, EditingState*);
    void updatePositionForNodeRemovalPreservingChildren(Position&, Node&);
    void prune(Node*, EditingState*, Node* excludeNode = nullptr);
    void replaceTextInNode(Text*, unsigned offset, unsigned count, const String& replacementText);
    Position replaceSelectedTextInNode(const String&);
    void replaceTextInNodePreservingMarkers(Text*, unsigned offset, unsigned count, const String& replacementText);
    Position positionOutsideTabSpan(const Position&);
    void setNodeAttribute(Element*, const QualifiedName& attribute, const AtomicString& value);
    void splitElement(Element*, Node* atChild);
    void splitTextNode(Text*, unsigned offset);
    void splitTextNodeContainingElement(Text*, unsigned offset);
    void wrapContentsInDummySpan(Element*);

    void deleteInsignificantText(Text*, unsigned start, unsigned end);
    void deleteInsignificantText(const Position& start, const Position& end);
    void deleteInsignificantTextDownstream(const Position&);

    HTMLBRElement* appendBlockPlaceholder(Element*, EditingState*);
    HTMLBRElement* insertBlockPlaceholder(const Position&, EditingState*);
    HTMLBRElement* addBlockPlaceholderIfNeeded(Element*, EditingState*);
    void removePlaceholderAt(const Position&);

    HTMLElement* insertNewDefaultParagraphElementAt(const Position&, EditingState*);

    HTMLElement* moveParagraphContentsToNewBlockIfNecessary(const Position&, EditingState*);

    void pushAnchorElementDown(Element*, EditingState*);

    void moveParagraph(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, EditingState*, ShouldPreserveSelection = DoNotPreserveSelection, ShouldPreserveStyle = PreserveStyle, Node* constrainingAncestor = nullptr);
    void moveParagraphs(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, EditingState*, ShouldPreserveSelection = DoNotPreserveSelection, ShouldPreserveStyle = PreserveStyle, Node* constrainingAncestor = nullptr);
    void moveParagraphWithClones(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, HTMLElement* blockElement, Node* outerNode, EditingState*);
    void cloneParagraphUnderNewElement(const Position& start, const Position& end, Node* outerNode, Element* blockElement, EditingState*);
    void cleanupAfterDeletion(EditingState*, VisiblePosition destination = VisiblePosition());

    bool breakOutOfEmptyListItem(EditingState*);
    bool breakOutOfEmptyMailBlockquotedParagraph(EditingState*);

    Position positionAvoidingSpecialElementBoundary(const Position&, EditingState*);

    Node* splitTreeToNode(Node*, Node*, bool splitAncestor = false);

    HeapVector<Member<EditCommand>> m_commands;

private:
    bool isCompositeEditCommand() const final { return true; }

    Member<EditCommandComposition> m_composition;
};

DEFINE_TYPE_CASTS(CompositeEditCommand, EditCommand, command, command->isCompositeEditCommand(), command.isCompositeEditCommand());

} // namespace blink

#endif // CompositeEditCommand_h
