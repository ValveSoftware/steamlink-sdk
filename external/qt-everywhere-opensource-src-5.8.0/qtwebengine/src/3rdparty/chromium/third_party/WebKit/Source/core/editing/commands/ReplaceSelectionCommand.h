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

#ifndef ReplaceSelectionCommand_h
#define ReplaceSelectionCommand_h

#include "core/dom/NodeTraversal.h"
#include "core/editing/commands/CompositeEditCommand.h"

namespace blink {

class DocumentFragment;
class ReplacementFragment;

class ReplaceSelectionCommand final : public CompositeEditCommand {
public:
    enum CommandOption {
        SelectReplacement = 1 << 0,
        SmartReplace = 1 << 1,
        MatchStyle = 1 << 2,
        PreventNesting = 1 << 3,
        MovingParagraph = 1 << 4,
        SanitizeFragment = 1 << 5
    };

    typedef unsigned CommandOptions;

    static ReplaceSelectionCommand* create(Document& document, DocumentFragment* fragment, CommandOptions options, EditAction action = EditActionPaste)
    {
        return new ReplaceSelectionCommand(document, fragment, options, action);
    }

    EphemeralRange insertedRange() const;

    DECLARE_VIRTUAL_TRACE();

private:
    ReplaceSelectionCommand(Document&, DocumentFragment*, CommandOptions, EditAction);

    void doApply(EditingState*) override;
    EditAction editingAction() const override;
    bool isReplaceSelectionCommand() const override;

    class InsertedNodes {
        STACK_ALLOCATED();
    public:
        void respondToNodeInsertion(Node&);
        void willRemoveNodePreservingChildren(Node&);
        void willRemoveNode(Node&);
        void didReplaceNode(Node&, Node& newNode);

        Node* firstNodeInserted() const { return m_firstNodeInserted.get(); }
        Node* lastLeafInserted() const { return m_lastNodeInserted ? &NodeTraversal::lastWithinOrSelf(*m_lastNodeInserted) : 0; }
        Node* pastLastLeaf() const { return m_lastNodeInserted ? NodeTraversal::next(NodeTraversal::lastWithinOrSelf(*m_lastNodeInserted)) : 0; }

    private:
        Member<Node> m_firstNodeInserted;
        Member<Node> m_lastNodeInserted;
    };

    Node* insertAsListItems(HTMLElement* listElement, Element* insertionBlock, const Position&, InsertedNodes&, EditingState*);

    void updateNodesInserted(Node*);
    bool shouldRemoveEndBR(HTMLBRElement*, const VisiblePosition&);

    bool shouldMergeStart(bool, bool, bool);
    bool shouldMergeEnd(bool selectionEndWasEndOfParagraph);
    bool shouldMerge(const VisiblePosition&, const VisiblePosition&);

    void mergeEndIfNeeded(EditingState*);

    void removeUnrenderedTextNodesAtEnds(InsertedNodes&);

    void removeRedundantStylesAndKeepStyleSpanInline(InsertedNodes&, EditingState*);
    void makeInsertedContentRoundTrippableWithHTMLTreeBuilder(const InsertedNodes&, EditingState*);
    void moveElementOutOfAncestor(Element*, Element* ancestor, EditingState*);
    void handleStyleSpans(InsertedNodes&, EditingState*);

    VisiblePosition positionAtStartOfInsertedContent() const;
    VisiblePosition positionAtEndOfInsertedContent() const;

    bool shouldPerformSmartReplace() const;
    void addSpacesForSmartReplace(EditingState*);
    void completeHTMLReplacement(const Position& lastPositionToSelect, EditingState*);
    void mergeTextNodesAroundPosition(Position&, Position& positionOnlyToBeUpdated, EditingState*);

    bool performTrivialReplace(const ReplacementFragment&, EditingState*);

    Position m_startOfInsertedContent;
    Position m_endOfInsertedContent;
    Member<EditingStyle> m_insertionStyle;
    bool m_selectReplacement;
    bool m_smartReplace;
    bool m_matchStyle;
    Member<DocumentFragment> m_documentFragment;
    bool m_preventNesting;
    bool m_movingParagraph;
    EditAction m_editAction;
    bool m_sanitizeFragment;
    bool m_shouldMergeEnd;

    Position m_startOfInsertedRange;
    Position m_endOfInsertedRange;
};

DEFINE_TYPE_CASTS(ReplaceSelectionCommand, CompositeEditCommand, command, command->isReplaceSelectionCommand(), command.isReplaceSelectionCommand());

} // namespace blink

#endif // ReplaceSelectionCommand_h
