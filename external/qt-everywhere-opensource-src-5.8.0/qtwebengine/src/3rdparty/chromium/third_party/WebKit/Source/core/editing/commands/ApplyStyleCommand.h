/*
 * Copyright (C) 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef ApplyStyleCommand_h
#define ApplyStyleCommand_h

#include "core/editing/WritingDirection.h"
#include "core/editing/commands/CompositeEditCommand.h"
#include "core/html/HTMLElement.h"

namespace blink {

class EditingStyle;
class HTMLSpanElement;
class StyleChange;

enum ShouldIncludeTypingStyle {
    IncludeTypingStyle,
    IgnoreTypingStyle
};

class ApplyStyleCommand final : public CompositeEditCommand {
public:
    enum EPropertyLevel { PropertyDefault, ForceBlockProperties };
    enum InlineStyleRemovalMode { RemoveIfNeeded, RemoveAlways, RemoveNone };
    enum EAddStyledElement { AddStyledElement, DoNotAddStyledElement };
    typedef bool (*IsInlineElementToRemoveFunction)(const Element*);

    static ApplyStyleCommand* create(Document& document, const EditingStyle* style, EditAction action, EPropertyLevel level = PropertyDefault)
    {
        return new ApplyStyleCommand(document, style, action, level);
    }
    static ApplyStyleCommand* create(Document& document, const EditingStyle* style, const Position& start, const Position& end)
    {
        return new ApplyStyleCommand(document, style, start, end);
    }
    static ApplyStyleCommand* create(Element* element, bool removeOnly)
    {
        return new ApplyStyleCommand(element, removeOnly);
    }
    static ApplyStyleCommand* create(Document& document, const EditingStyle* style, IsInlineElementToRemoveFunction isInlineElementToRemoveFunction, EditAction action)
    {
        return new ApplyStyleCommand(document, style, isInlineElementToRemoveFunction, action);
    }

    DECLARE_VIRTUAL_TRACE();

private:
    ApplyStyleCommand(Document&, const EditingStyle*, EditAction, EPropertyLevel);
    ApplyStyleCommand(Document&, const EditingStyle*, const Position& start, const Position& end);
    ApplyStyleCommand(Element*, bool removeOnly);
    ApplyStyleCommand(Document&, const EditingStyle*, bool (*isInlineElementToRemove)(const Element*), EditAction);

    void doApply(EditingState*) override;
    EditAction editingAction() const override;

    // style-removal helpers
    bool isStyledInlineElementToRemove(Element*) const;
    bool shouldApplyInlineStyleToRun(EditingStyle*, Node* runStart, Node* pastEndNode);
    void removeConflictingInlineStyleFromRun(EditingStyle*, Member<Node>& runStart, Member<Node>& runEnd, Node* pastEndNode, EditingState*);
    bool removeInlineStyleFromElement(EditingStyle*, HTMLElement*, EditingState*, InlineStyleRemovalMode = RemoveIfNeeded, EditingStyle* extractedStyle = nullptr);
    inline bool shouldRemoveInlineStyleFromElement(EditingStyle* style, HTMLElement* element) { return removeInlineStyleFromElement(style, element, ASSERT_NO_EDITING_ABORT, RemoveNone); }
    void replaceWithSpanOrRemoveIfWithoutAttributes(HTMLElement*, EditingState*);
    bool removeImplicitlyStyledElement(EditingStyle*, HTMLElement*, InlineStyleRemovalMode, EditingStyle* extractedStyle, EditingState*);
    bool removeCSSStyle(EditingStyle*, HTMLElement*, EditingState*, InlineStyleRemovalMode = RemoveIfNeeded, EditingStyle* extractedStyle = nullptr);
    HTMLElement* highestAncestorWithConflictingInlineStyle(EditingStyle*, Node*);
    void applyInlineStyleToPushDown(Node*, EditingStyle*, EditingState*);
    void pushDownInlineStyleAroundNode(EditingStyle*, Node*, EditingState*);
    void removeInlineStyle(EditingStyle* , const Position& start, const Position& end, EditingState*);
    bool elementFullySelected(HTMLElement&, const Position& start, const Position& end) const;

    // style-application helpers
    void applyBlockStyle(EditingStyle*, EditingState*);
    void applyRelativeFontStyleChange(EditingStyle*, EditingState*);
    void applyInlineStyle(EditingStyle*, EditingState*);
    void fixRangeAndApplyInlineStyle(EditingStyle*, const Position& start, const Position& end, EditingState*);
    void applyInlineStyleToNodeRange(EditingStyle*, Node* startNode, Node* pastEndNode, EditingState*);
    void addBlockStyle(const StyleChange&, HTMLElement*);
    void addInlineStyleIfNeeded(EditingStyle*, Node* start, Node* end, EditingState*);
    Position positionToComputeInlineStyleChange(Node*, Member<HTMLSpanElement>& dummyElement, EditingState*);
    void applyInlineStyleChange(Node* startNode, Node* endNode, StyleChange&, EAddStyledElement, EditingState*);
    void splitTextAtStart(const Position& start, const Position& end);
    void splitTextAtEnd(const Position& start, const Position& end);
    void splitTextElementAtStart(const Position& start, const Position& end);
    void splitTextElementAtEnd(const Position& start, const Position& end);
    bool shouldSplitTextElement(Element*, EditingStyle*);
    bool isValidCaretPositionInTextNode(const Position&);
    bool mergeStartWithPreviousIfIdentical(const Position& start, const Position& end, EditingState*);
    bool mergeEndWithNextIfIdentical(const Position& start, const Position& end, EditingState*);
    void cleanupUnstyledAppleStyleSpans(ContainerNode* dummySpanAncestor, EditingState*);

    void surroundNodeRangeWithElement(Node* start, Node* end, Element*, EditingState*);
    float computedFontSize(Node*);
    void joinChildTextNodes(ContainerNode*, const Position& start, const Position& end);

    HTMLElement* splitAncestorsWithUnicodeBidi(Node*, bool before, WritingDirection allowedDirection);
    void removeEmbeddingUpToEnclosingBlock(Node*, HTMLElement* unsplitAncestor, EditingState*);

    void updateStartEnd(const Position& newStart, const Position& newEnd);
    Position startPosition();
    Position endPosition();

    Member<EditingStyle> m_style;
    EditAction m_editingAction;
    EPropertyLevel m_propertyLevel;
    Position m_start;
    Position m_end;
    bool m_useEndingSelection;
    Member<Element> m_styledInlineElement;
    bool m_removeOnly;
    IsInlineElementToRemoveFunction m_isInlineElementToRemoveFunction;
};

enum ShouldStyleAttributeBeEmpty { AllowNonEmptyStyleAttribute, StyleAttributeShouldBeEmpty };
bool isEmptyFontTag(const Element*, ShouldStyleAttributeBeEmpty = StyleAttributeShouldBeEmpty);
bool isLegacyAppleHTMLSpanElement(const Node*);
bool isStyleSpanOrSpanWithOnlyStyleAttribute(const Element*);

} // namespace blink

#endif
