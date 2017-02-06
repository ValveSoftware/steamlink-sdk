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

#include "core/editing/commands/ApplyStyleCommand.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/NodeList.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/HTMLInterchange.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFontElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "platform/heap/Handle.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

static String& styleSpanClassString()
{
    DEFINE_STATIC_LOCAL(String, styleSpanClassString, ((AppleStyleSpanClass)));
    return styleSpanClassString;
}

bool isLegacyAppleHTMLSpanElement(const Node* node)
{
    if (!isHTMLSpanElement(node))
        return false;

    const HTMLSpanElement& span = toHTMLSpanElement(*node);
    if (span.getAttribute(classAttr) != styleSpanClassString())
        return false;
    UseCounter::count(span.document(), UseCounter::EditingAppleStyleSpanClass);
    return true;
}

static bool hasNoAttributeOrOnlyStyleAttribute(const HTMLElement* element, ShouldStyleAttributeBeEmpty shouldStyleAttributeBeEmpty)
{
    AttributeCollection attributes = element->attributes();
    if (attributes.isEmpty())
        return true;

    unsigned matchedAttributes = 0;
    if (element->getAttribute(classAttr) == styleSpanClassString())
        matchedAttributes++;
    if (element->hasAttribute(styleAttr) && (shouldStyleAttributeBeEmpty == AllowNonEmptyStyleAttribute
        || !element->inlineStyle() || element->inlineStyle()->isEmpty()))
        matchedAttributes++;

    DCHECK_LE(matchedAttributes, attributes.size());
    return matchedAttributes == attributes.size();
}

bool isStyleSpanOrSpanWithOnlyStyleAttribute(const Element* element)
{
    if (!isHTMLSpanElement(element))
        return false;
    return hasNoAttributeOrOnlyStyleAttribute(toHTMLSpanElement(element), AllowNonEmptyStyleAttribute);
}

static inline bool isSpanWithoutAttributesOrUnstyledStyleSpan(const Node* node)
{
    if (!isHTMLSpanElement(node))
        return false;
    return hasNoAttributeOrOnlyStyleAttribute(toHTMLSpanElement(node), StyleAttributeShouldBeEmpty);
}

bool isEmptyFontTag(const Element* element, ShouldStyleAttributeBeEmpty shouldStyleAttributeBeEmpty)
{
    if (!isHTMLFontElement(element))
        return false;

    return hasNoAttributeOrOnlyStyleAttribute(toHTMLFontElement(element), shouldStyleAttributeBeEmpty);
}

static bool offsetIsBeforeLastNodeOffset(int offset, Node* anchorNode)
{
    if (anchorNode->offsetInCharacters())
        return offset < anchorNode->maxCharacterOffset();
    int currentOffset = 0;
    for (Node* node = NodeTraversal::firstChild(*anchorNode); node && currentOffset < offset; node = NodeTraversal::nextSibling(*node))
        currentOffset++;
    return offset < currentOffset;
}

ApplyStyleCommand::ApplyStyleCommand(Document& document, const EditingStyle* style, EditAction editingAction, EPropertyLevel propertyLevel)
    : CompositeEditCommand(document)
    , m_style(style->copy())
    , m_editingAction(editingAction)
    , m_propertyLevel(propertyLevel)
    , m_start(mostForwardCaretPosition(endingSelection().start()))
    , m_end(mostBackwardCaretPosition(endingSelection().end()))
    , m_useEndingSelection(true)
    , m_styledInlineElement(nullptr)
    , m_removeOnly(false)
    , m_isInlineElementToRemoveFunction(0)
{
}

ApplyStyleCommand::ApplyStyleCommand(Document& document, const EditingStyle* style, const Position& start, const Position& end)
    : CompositeEditCommand(document)
    , m_style(style->copy())
    , m_editingAction(EditActionChangeAttributes)
    , m_propertyLevel(PropertyDefault)
    , m_start(start)
    , m_end(end)
    , m_useEndingSelection(false)
    , m_styledInlineElement(nullptr)
    , m_removeOnly(false)
    , m_isInlineElementToRemoveFunction(0)
{
}

ApplyStyleCommand::ApplyStyleCommand(Element* element, bool removeOnly)
    : CompositeEditCommand(element->document())
    , m_style(EditingStyle::create())
    , m_editingAction(EditActionChangeAttributes)
    , m_propertyLevel(PropertyDefault)
    , m_start(mostForwardCaretPosition(endingSelection().start()))
    , m_end(mostBackwardCaretPosition(endingSelection().end()))
    , m_useEndingSelection(true)
    , m_styledInlineElement(element)
    , m_removeOnly(removeOnly)
    , m_isInlineElementToRemoveFunction(0)
{
}

ApplyStyleCommand::ApplyStyleCommand(Document& document, const EditingStyle* style, IsInlineElementToRemoveFunction isInlineElementToRemoveFunction, EditAction editingAction)
    : CompositeEditCommand(document)
    , m_style(style->copy())
    , m_editingAction(editingAction)
    , m_propertyLevel(PropertyDefault)
    , m_start(mostForwardCaretPosition(endingSelection().start()))
    , m_end(mostBackwardCaretPosition(endingSelection().end()))
    , m_useEndingSelection(true)
    , m_styledInlineElement(nullptr)
    , m_removeOnly(true)
    , m_isInlineElementToRemoveFunction(isInlineElementToRemoveFunction)
{
}

void ApplyStyleCommand::updateStartEnd(const Position& newStart, const Position& newEnd)
{
    DCHECK_GE(comparePositions(newEnd, newStart), 0);

    if (!m_useEndingSelection && (newStart != m_start || newEnd != m_end))
        m_useEndingSelection = true;

    setEndingSelection(VisibleSelection(newStart, newEnd, VP_DEFAULT_AFFINITY, endingSelection().isDirectional()));
    m_start = newStart;
    m_end = newEnd;
}

Position ApplyStyleCommand::startPosition()
{
    if (m_useEndingSelection)
        return endingSelection().start();

    return m_start;
}

Position ApplyStyleCommand::endPosition()
{
    if (m_useEndingSelection)
        return endingSelection().end();

    return m_end;
}

void ApplyStyleCommand::doApply(EditingState* editingState)
{
    switch (m_propertyLevel) {
    case PropertyDefault: {
        // Apply the block-centric properties of the style.
        EditingStyle* blockStyle = m_style->extractAndRemoveBlockProperties();
        if (!blockStyle->isEmpty()) {
            applyBlockStyle(blockStyle, editingState);
            if (editingState->isAborted())
                return;
        }
        // Apply any remaining styles to the inline elements.
        if (!m_style->isEmpty() || m_styledInlineElement || m_isInlineElementToRemoveFunction) {
            applyRelativeFontStyleChange(m_style.get(), editingState);
            if (editingState->isAborted())
                return;
            applyInlineStyle(m_style.get(), editingState);
            if (editingState->isAborted())
                return;
        }
        break;
    }
    case ForceBlockProperties:
        // Force all properties to be applied as block styles.
        applyBlockStyle(m_style.get(), editingState);
        break;
    }
}

EditAction ApplyStyleCommand::editingAction() const
{
    return m_editingAction;
}

void ApplyStyleCommand::applyBlockStyle(EditingStyle *style, EditingState* editingState)
{
    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // get positions we want to use for applying style
    Position start = startPosition();
    Position end = endPosition();
    if (comparePositions(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }

    VisiblePosition visibleStart = createVisiblePosition(start);
    VisiblePosition visibleEnd = createVisiblePosition(end);

    if (visibleStart.isNull() || visibleStart.isOrphan() || visibleEnd.isNull() || visibleEnd.isOrphan())
        return;

    // Save and restore the selection endpoints using their indices in the document, since
    // addBlockStyleIfNeeded may moveParagraphs, which can remove these endpoints.
    // Calculate start and end indices from the start of the tree that they're in.
    Node& scope = NodeTraversal::highestAncestorOrSelf(*visibleStart.deepEquivalent().anchorNode());
    Range* startRange = Range::create(document(), Position::firstPositionInNode(&scope), visibleStart.deepEquivalent().parentAnchoredEquivalent());
    Range* endRange = Range::create(document(), Position::firstPositionInNode(&scope), visibleEnd.deepEquivalent().parentAnchoredEquivalent());
    int startIndex = TextIterator::rangeLength(startRange->startPosition(), startRange->endPosition(), true);
    int endIndex = TextIterator::rangeLength(endRange->startPosition(), endRange->endPosition(), true);

    VisiblePosition paragraphStart(startOfParagraph(visibleStart));
    VisiblePosition nextParagraphStart(nextPositionOf(endOfParagraph(paragraphStart)));
    VisiblePosition beyondEnd(nextPositionOf(endOfParagraph(visibleEnd)));
    while (paragraphStart.isNotNull() && paragraphStart.deepEquivalent() != beyondEnd.deepEquivalent()) {
        StyleChange styleChange(style, paragraphStart.deepEquivalent());
        if (styleChange.cssStyle().length() || m_removeOnly) {
            Element* block = enclosingBlock(paragraphStart.deepEquivalent().anchorNode());
            const Position& paragraphStartToMove = paragraphStart.deepEquivalent();
            if (!m_removeOnly && isEditablePosition(paragraphStartToMove)) {
                HTMLElement* newBlock = moveParagraphContentsToNewBlockIfNecessary(paragraphStartToMove, editingState);
                if (editingState->isAborted())
                    return;
                if (newBlock)
                    block = newBlock;
            }
            if (block && block->isHTMLElement()) {
                removeCSSStyle(style, toHTMLElement(block), editingState);
                if (editingState->isAborted())
                    return;
                if (!m_removeOnly)
                    addBlockStyle(styleChange, toHTMLElement(block));
            }

            if (nextParagraphStart.isOrphan())
                nextParagraphStart = nextPositionOf(endOfParagraph(paragraphStart));
        }

        paragraphStart = nextParagraphStart;
        nextParagraphStart = nextPositionOf(endOfParagraph(paragraphStart));
    }

    EphemeralRange startEphemeralRange = PlainTextRange(startIndex).createRangeForSelection(toContainerNode(scope));
    if (startEphemeralRange.isNull())
        return;
    EphemeralRange endEphemeralRange = PlainTextRange(endIndex).createRangeForSelection(toContainerNode(scope));
    if (endEphemeralRange.isNull())
        return;
    updateStartEnd(startEphemeralRange.startPosition(), endEphemeralRange.startPosition());
}

static MutableStylePropertySet* copyStyleOrCreateEmpty(const StylePropertySet* style)
{
    if (!style)
        return MutableStylePropertySet::create(HTMLQuirksMode);
    return style->mutableCopy();
}

void ApplyStyleCommand::applyRelativeFontStyleChange(EditingStyle* style, EditingState* editingState)
{
    static const float MinimumFontSize = 0.1f;

    if (!style || !style->hasFontSizeDelta())
        return;

    Position start = startPosition();
    Position end = endPosition();
    if (comparePositions(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }

    // Join up any adjacent text nodes.
    if (start.anchorNode()->isTextNode()) {
        joinChildTextNodes(start.anchorNode()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    if (end.anchorNode()->isTextNode() && start.anchorNode()->parentNode() != end.anchorNode()->parentNode()) {
        joinChildTextNodes(end.anchorNode()->parentNode(), start, end);
        start = startPosition();
        end = endPosition();
    }

    if (start.isNull() || end.isNull())
        return;

    // Split the start text nodes if needed to apply style.
    if (isValidCaretPositionInTextNode(start)) {
        splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
    }

    if (isValidCaretPositionInTextNode(end)) {
        splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
    }

    // Calculate loop end point.
    // If the end node is before the start node (can only happen if the end node is
    // an ancestor of the start node), we gather nodes up to the next sibling of the end node
    Node* beyondEnd;
    DCHECK(start.anchorNode());
    DCHECK(end.anchorNode());
    if (start.anchorNode()->isDescendantOf(end.anchorNode()))
        beyondEnd = NodeTraversal::nextSkippingChildren(*end.anchorNode());
    else
        beyondEnd = NodeTraversal::next(*end.anchorNode());

    start = mostBackwardCaretPosition(start); // Move upstream to ensure we do not add redundant spans.
    Node* startNode = start.anchorNode();
    DCHECK(startNode);

    // Make sure we're not already at the end or the next NodeTraversal::next() will traverse
    // past it.
    if (startNode == beyondEnd)
        return;

    if (startNode->isTextNode() && start.computeOffsetInContainerNode() >= caretMaxOffset(startNode)) {
        // Move out of text node if range does not include its characters.
        startNode = NodeTraversal::next(*startNode);
        if (!startNode)
            return;
    }

    // Store away font size before making any changes to the document.
    // This ensures that changes to one node won't effect another.
    HeapHashMap<Member<Node>, float> startingFontSizes;
    for (Node* node = startNode; node != beyondEnd; node = NodeTraversal::next(*node)) {
        DCHECK(node);
        startingFontSizes.set(node, computedFontSize(node));
    }

    // These spans were added by us. If empty after font size changes, they can be removed.
    HeapVector<Member<HTMLElement>> unstyledSpans;

    Node* lastStyledNode = nullptr;
    for (Node* node = startNode; node != beyondEnd; node = NodeTraversal::next(*node)) {
        DCHECK(node);
        HTMLElement* element = nullptr;
        if (node->isHTMLElement()) {
            // Only work on fully selected nodes.
            if (!elementFullySelected(toHTMLElement(*node), start, end))
                continue;
            element = toHTMLElement(node);
        } else if (node->isTextNode() && node->layoutObject() && node->parentNode() != lastStyledNode) {
            // Last styled node was not parent node of this text node, but we wish to style this
            // text node. To make this possible, add a style span to surround this text node.
            HTMLSpanElement* span = HTMLSpanElement::create(document());
            surroundNodeRangeWithElement(node, node, span, editingState);
            if (editingState->isAborted())
                return;
            element = span;
        }  else {
            // Only handle HTML elements and text nodes.
            continue;
        }
        lastStyledNode = node;

        MutableStylePropertySet* inlineStyle = copyStyleOrCreateEmpty(element->inlineStyle());
        float currentFontSize = computedFontSize(node);
        float desiredFontSize = max(MinimumFontSize, startingFontSizes.get(node) + style->fontSizeDelta());
        CSSValue* value = inlineStyle->getPropertyCSSValue(CSSPropertyFontSize);
        if (value) {
            element->removeInlineStyleProperty(CSSPropertyFontSize);
            currentFontSize = computedFontSize(node);
        }
        if (currentFontSize != desiredFontSize) {
            inlineStyle->setProperty(CSSPropertyFontSize, CSSPrimitiveValue::create(desiredFontSize, CSSPrimitiveValue::UnitType::Pixels), false);
            setNodeAttribute(element, styleAttr, AtomicString(inlineStyle->asText()));
        }
        if (inlineStyle->isEmpty()) {
            removeElementAttribute(element, styleAttr);
            if (isSpanWithoutAttributesOrUnstyledStyleSpan(element))
                unstyledSpans.append(element);
        }
    }

    for (const auto& unstyledSpan : unstyledSpans) {
        removeNodePreservingChildren(unstyledSpan, editingState);
        if (editingState->isAborted())
            return;
    }
}

static ContainerNode* dummySpanAncestorForNode(const Node* node)
{
    while (node && (!node->isElementNode() || !isStyleSpanOrSpanWithOnlyStyleAttribute(toElement(node))))
        node = node->parentNode();

    return node ? node->parentNode() : 0;
}

void ApplyStyleCommand::cleanupUnstyledAppleStyleSpans(ContainerNode* dummySpanAncestor, EditingState* editingState)
{
    if (!dummySpanAncestor)
        return;

    // Dummy spans are created when text node is split, so that style information
    // can be propagated, which can result in more splitting. If a dummy span gets
    // cloned/split, the new node is always a sibling of it. Therefore, we scan
    // all the children of the dummy's parent
    Node* next;
    for (Node* node = dummySpanAncestor->firstChild(); node; node = next) {
        next = node->nextSibling();
        if (isSpanWithoutAttributesOrUnstyledStyleSpan(node)) {
            removeNodePreservingChildren(node, editingState);
            if (editingState->isAborted())
                return;
        }
    }
}

HTMLElement* ApplyStyleCommand::splitAncestorsWithUnicodeBidi(Node* node, bool before, WritingDirection allowedDirection)
{
    // We are allowed to leave the highest ancestor with unicode-bidi unsplit if it is unicode-bidi: embed and direction: allowedDirection.
    // In that case, we return the unsplit ancestor. Otherwise, we return 0.
    Element* block = enclosingBlock(node);
    if (!block)
        return 0;

    ContainerNode* highestAncestorWithUnicodeBidi = nullptr;
    ContainerNode* nextHighestAncestorWithUnicodeBidi = nullptr;
    int highestAncestorUnicodeBidi = 0;
    for (Node& runner : NodeTraversal::ancestorsOf(*node)) {
        if (runner == block)
            break;
        int unicodeBidi = getIdentifierValue(CSSComputedStyleDeclaration::create(&runner), CSSPropertyUnicodeBidi);
        if (unicodeBidi && unicodeBidi != CSSValueNormal) {
            highestAncestorUnicodeBidi = unicodeBidi;
            nextHighestAncestorWithUnicodeBidi = highestAncestorWithUnicodeBidi;
            highestAncestorWithUnicodeBidi = static_cast<ContainerNode*>(&runner);
        }
    }

    if (!highestAncestorWithUnicodeBidi)
        return 0;

    HTMLElement* unsplitAncestor = 0;

    WritingDirection highestAncestorDirection;
    if (allowedDirection != NaturalWritingDirection
        && highestAncestorUnicodeBidi != CSSValueBidiOverride
        && highestAncestorWithUnicodeBidi->isHTMLElement()
        && EditingStyle::create(highestAncestorWithUnicodeBidi, EditingStyle::AllProperties)->textDirection(highestAncestorDirection)
        && highestAncestorDirection == allowedDirection) {
        if (!nextHighestAncestorWithUnicodeBidi)
            return toHTMLElement(highestAncestorWithUnicodeBidi);

        unsplitAncestor = toHTMLElement(highestAncestorWithUnicodeBidi);
        highestAncestorWithUnicodeBidi = nextHighestAncestorWithUnicodeBidi;
    }

    // Split every ancestor through highest ancestor with embedding.
    Node* currentNode = node;
    while (currentNode) {
        Element* parent = toElement(currentNode->parentNode());
        if (before ? currentNode->previousSibling() : currentNode->nextSibling())
            splitElement(parent, before ? currentNode : currentNode->nextSibling());
        if (parent == highestAncestorWithUnicodeBidi)
            break;
        currentNode = parent;
    }
    return unsplitAncestor;
}

void ApplyStyleCommand::removeEmbeddingUpToEnclosingBlock(Node* node, HTMLElement* unsplitAncestor, EditingState* editingState)
{
    Element* block = enclosingBlock(node);
    if (!block)
        return;

    for (Node& runner : NodeTraversal::ancestorsOf(*node)) {
        if (runner == block || runner == unsplitAncestor)
            break;
        if (!runner.isStyledElement())
            continue;

        Element* element = toElement(&runner);
        int unicodeBidi = getIdentifierValue(CSSComputedStyleDeclaration::create(element), CSSPropertyUnicodeBidi);
        if (!unicodeBidi || unicodeBidi == CSSValueNormal)
            continue;

        // FIXME: This code should really consider the mapped attribute 'dir', the inline style declaration,
        // and all matching style rules in order to determine how to best set the unicode-bidi property to 'normal'.
        // For now, it assumes that if the 'dir' attribute is present, then removing it will suffice, and
        // otherwise it sets the property in the inline style declaration.
        if (element->hasAttribute(dirAttr)) {
            // FIXME: If this is a BDO element, we should probably just remove it if it has no
            // other attributes, like we (should) do with B and I elements.
            removeElementAttribute(element, dirAttr);
        } else {
            MutableStylePropertySet* inlineStyle = copyStyleOrCreateEmpty(element->inlineStyle());
            inlineStyle->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
            inlineStyle->removeProperty(CSSPropertyDirection);
            setNodeAttribute(element, styleAttr, AtomicString(inlineStyle->asText()));
            if (isSpanWithoutAttributesOrUnstyledStyleSpan(element)) {
                removeNodePreservingChildren(element, editingState);
                if (editingState->isAborted())
                    return;
            }
        }
    }
}

static HTMLElement* highestEmbeddingAncestor(Node* startNode, Node* enclosingNode)
{
    for (Node* n = startNode; n && n != enclosingNode; n = n->parentNode()) {
        if (n->isHTMLElement()
            && EditingStyle::isEmbedOrIsolate(getIdentifierValue(CSSComputedStyleDeclaration::create(n), CSSPropertyUnicodeBidi))) {
            return toHTMLElement(n);
        }
    }

    return 0;
}

void ApplyStyleCommand::applyInlineStyle(EditingStyle* style, EditingState* editingState)
{
    ContainerNode* startDummySpanAncestor = nullptr;
    ContainerNode* endDummySpanAncestor = nullptr;

    // update document layout once before removing styles
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    // adjust to the positions we want to use for applying style
    Position start = startPosition();
    Position end = endPosition();

    if (start.isNull() || end.isNull())
        return;

    if (comparePositions(end, start) < 0) {
        Position swap = start;
        start = end;
        end = swap;
    }

    // split the start node and containing element if the selection starts inside of it
    bool splitStart = isValidCaretPositionInTextNode(start);
    if (splitStart) {
        if (shouldSplitTextElement(start.anchorNode()->parentElement(), style))
            splitTextElementAtStart(start, end);
        else
            splitTextAtStart(start, end);
        start = startPosition();
        end = endPosition();
        if (start.isNull() || end.isNull())
            return;
        startDummySpanAncestor = dummySpanAncestorForNode(start.anchorNode());
    }

    // split the end node and containing element if the selection ends inside of it
    bool splitEnd = isValidCaretPositionInTextNode(end);
    if (splitEnd) {
        if (shouldSplitTextElement(end.anchorNode()->parentElement(), style))
            splitTextElementAtEnd(start, end);
        else
            splitTextAtEnd(start, end);
        start = startPosition();
        end = endPosition();
        if (start.isNull() || end.isNull())
            return;
        endDummySpanAncestor = dummySpanAncestorForNode(end.anchorNode());
    }

    // Remove style from the selection.
    // Use the upstream position of the start for removing style.
    // This will ensure we remove all traces of the relevant styles from the selection
    // and prevent us from adding redundant ones, as described in:
    // <rdar://problem/3724344> Bolding and unbolding creates extraneous tags
    Position removeStart = mostBackwardCaretPosition(start);
    WritingDirection textDirection = NaturalWritingDirection;
    bool hasTextDirection = style->textDirection(textDirection);
    EditingStyle* styleWithoutEmbedding = nullptr;
    EditingStyle* embeddingStyle = nullptr;
    if (hasTextDirection) {
        // Leave alone an ancestor that provides the desired single level embedding, if there is one.
        HTMLElement* startUnsplitAncestor = splitAncestorsWithUnicodeBidi(start.anchorNode(), true, textDirection);
        HTMLElement* endUnsplitAncestor = splitAncestorsWithUnicodeBidi(end.anchorNode(), false, textDirection);
        removeEmbeddingUpToEnclosingBlock(start.anchorNode(), startUnsplitAncestor, editingState);
        if (editingState->isAborted())
            return;
        removeEmbeddingUpToEnclosingBlock(end.anchorNode(), endUnsplitAncestor, editingState);
        if (editingState->isAborted())
            return;

        // Avoid removing the dir attribute and the unicode-bidi and direction properties from the unsplit ancestors.
        Position embeddingRemoveStart = removeStart;
        if (startUnsplitAncestor && elementFullySelected(*startUnsplitAncestor, removeStart, end))
            embeddingRemoveStart = Position::inParentAfterNode(*startUnsplitAncestor);

        Position embeddingRemoveEnd = end;
        if (endUnsplitAncestor && elementFullySelected(*endUnsplitAncestor, removeStart, end))
            embeddingRemoveEnd = mostForwardCaretPosition(Position::inParentBeforeNode(*endUnsplitAncestor));

        if (embeddingRemoveEnd != removeStart || embeddingRemoveEnd != end) {
            styleWithoutEmbedding = style->copy();
            embeddingStyle = styleWithoutEmbedding->extractAndRemoveTextDirection();

            if (comparePositions(embeddingRemoveStart, embeddingRemoveEnd) <= 0) {
                removeInlineStyle(embeddingStyle, embeddingRemoveStart, embeddingRemoveEnd, editingState);
                if (editingState->isAborted())
                    return;
            }
        }
    }

    removeInlineStyle(styleWithoutEmbedding ? styleWithoutEmbedding : style, removeStart, end, editingState);
    if (editingState->isAborted())
        return;
    start = startPosition();
    end = endPosition();
    if (start.isNull() || start.isOrphan() || end.isNull() || end.isOrphan())
        return;

    if (splitStart) {
        bool mergeResult = mergeStartWithPreviousIfIdentical(start, end, editingState);
        if (editingState->isAborted())
            return;
        if (splitStart && mergeResult) {
            start = startPosition();
            end = endPosition();
        }
    }

    if (splitEnd) {
        mergeEndWithNextIfIdentical(start, end, editingState);
        if (editingState->isAborted())
            return;
        start = startPosition();
        end = endPosition();
    }

    // update document layout once before running the rest of the function
    // so that we avoid the expense of updating before each and every call
    // to check a computed style
    document().updateStyleAndLayoutIgnorePendingStylesheets();

    EditingStyle* styleToApply = style;
    if (hasTextDirection) {
        // Avoid applying the unicode-bidi and direction properties beneath ancestors that already have them.
        HTMLElement* embeddingStartElement = highestEmbeddingAncestor(start.anchorNode(), enclosingBlock(start.anchorNode()));
        HTMLElement* embeddingEndElement = highestEmbeddingAncestor(end.anchorNode(), enclosingBlock(end.anchorNode()));

        if (embeddingStartElement || embeddingEndElement) {
            Position embeddingApplyStart = embeddingStartElement ? Position::inParentAfterNode(*embeddingStartElement) : start;
            Position embeddingApplyEnd = embeddingEndElement ? Position::inParentBeforeNode(*embeddingEndElement) : end;
            DCHECK(embeddingApplyStart.isNotNull());
            DCHECK(embeddingApplyEnd.isNotNull());

            if (!embeddingStyle) {
                styleWithoutEmbedding = style->copy();
                embeddingStyle = styleWithoutEmbedding->extractAndRemoveTextDirection();
            }
            fixRangeAndApplyInlineStyle(embeddingStyle, embeddingApplyStart, embeddingApplyEnd, editingState);
            if (editingState->isAborted())
                return;

            styleToApply = styleWithoutEmbedding;
        }
    }

    fixRangeAndApplyInlineStyle(styleToApply, start, end, editingState);
    if (editingState->isAborted())
        return;

    // Remove dummy style spans created by splitting text elements.
    cleanupUnstyledAppleStyleSpans(startDummySpanAncestor, editingState);
    if (editingState->isAborted())
        return;
    if (endDummySpanAncestor != startDummySpanAncestor)
        cleanupUnstyledAppleStyleSpans(endDummySpanAncestor, editingState);
}

void ApplyStyleCommand::fixRangeAndApplyInlineStyle(EditingStyle* style, const Position& start, const Position& end, EditingState* editingState)
{
    Node* startNode = start.anchorNode();
    DCHECK(startNode);

    if (start.computeEditingOffset() >= caretMaxOffset(start.anchorNode())) {
        startNode = NodeTraversal::next(*startNode);
        if (!startNode || comparePositions(end, firstPositionInOrBeforeNode(startNode)) < 0)
            return;
    }

    Node* pastEndNode = end.anchorNode();
    if (end.computeEditingOffset() >= caretMaxOffset(end.anchorNode()))
        pastEndNode = NodeTraversal::nextSkippingChildren(*end.anchorNode());

    // FIXME: Callers should perform this operation on a Range that includes the br
    // if they want style applied to the empty line.
    if (start == end && isHTMLBRElement(*start.anchorNode()))
        pastEndNode = NodeTraversal::next(*start.anchorNode());

    // Start from the highest fully selected ancestor so that we can modify the fully selected node.
    // e.g. When applying font-size: large on <font color="blue">hello</font>, we need to include the font element in our run
    // to generate <font color="blue" size="4">hello</font> instead of <font color="blue"><font size="4">hello</font></font>
    Range* range = Range::create(startNode->document(), start, end);
    Element* editableRoot = startNode->rootEditableElement();
    if (startNode != editableRoot) {
        while (editableRoot && startNode->parentNode() != editableRoot && isNodeVisiblyContainedWithin(*startNode->parentNode(), *range))
            startNode = startNode->parentNode();
    }

    applyInlineStyleToNodeRange(style, startNode, pastEndNode, editingState);
}

static bool containsNonEditableRegion(Node& node)
{
    if (!node.hasEditableStyle())
        return true;

    Node* sibling = NodeTraversal::nextSkippingChildren(node);
    for (Node* descendent = node.firstChild(); descendent && descendent != sibling; descendent = NodeTraversal::next(*descendent)) {
        if (!descendent->hasEditableStyle())
            return true;
    }

    return false;
}

class InlineRunToApplyStyle {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    InlineRunToApplyStyle(Node* start, Node* end, Node* pastEndNode)
        : start(start)
        , end(end)
        , pastEndNode(pastEndNode)
    {
        DCHECK_EQ(start->parentNode(), end->parentNode());
    }

    bool startAndEndAreStillInDocument()
    {
        return start && end && start->inShadowIncludingDocument() && end->inShadowIncludingDocument();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(start);
        visitor->trace(end);
        visitor->trace(pastEndNode);
        visitor->trace(positionForStyleComputation);
        visitor->trace(dummyElement);
    }

    Member<Node> start;
    Member<Node> end;
    Member<Node> pastEndNode;
    Position positionForStyleComputation;
    Member<HTMLSpanElement> dummyElement;
    StyleChange change;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::InlineRunToApplyStyle);

namespace blink {

void ApplyStyleCommand::applyInlineStyleToNodeRange(EditingStyle* style, Node* startNode, Node* pastEndNode, EditingState* editingState)
{
    if (m_removeOnly)
        return;

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    HeapVector<InlineRunToApplyStyle> runs;
    Node* node = startNode;
    for (Node* next; node && node != pastEndNode; node = next) {
        next = NodeTraversal::next(*node);

        if (!node->layoutObject() || !node->hasEditableStyle())
            continue;

        if (!node->layoutObjectIsRichlyEditable() && node->isHTMLElement()) {
            HTMLElement* element = toHTMLElement(node);
            // This is a plaintext-only region. Only proceed if it's fully selected.
            // pastEndNode is the node after the last fully selected node, so if it's inside node then
            // node isn't fully selected.
            if (pastEndNode && pastEndNode->isDescendantOf(element))
                break;
            // Add to this element's inline style and skip over its contents.
            next = NodeTraversal::nextSkippingChildren(*node);
            if (!style->style())
                continue;
            MutableStylePropertySet* inlineStyle = copyStyleOrCreateEmpty(element->inlineStyle());
            inlineStyle->mergeAndOverrideOnConflict(style->style());
            setNodeAttribute(element, styleAttr, AtomicString(inlineStyle->asText()));
            continue;
        }

        if (isEnclosingBlock(node))
            continue;

        if (node->hasChildren()) {
            if (node->contains(pastEndNode) || containsNonEditableRegion(*node) || !node->parentNode()->hasEditableStyle())
                continue;
            if (editingIgnoresContent(node)) {
                next = NodeTraversal::nextSkippingChildren(*node);
                continue;
            }
        }

        Node* runStart = node;
        Node* runEnd = node;
        Node* sibling = node->nextSibling();
        while (sibling && sibling != pastEndNode && !sibling->contains(pastEndNode)
            && (!isEnclosingBlock(sibling) || isHTMLBRElement(*sibling))
            && !containsNonEditableRegion(*sibling)) {
            runEnd = sibling;
            sibling = runEnd->nextSibling();
        }
        DCHECK(runEnd);
        next = NodeTraversal::nextSkippingChildren(*runEnd);

        Node* pastEndNode = NodeTraversal::nextSkippingChildren(*runEnd);
        if (!shouldApplyInlineStyleToRun(style, runStart, pastEndNode))
            continue;

        runs.append(InlineRunToApplyStyle(runStart, runEnd, pastEndNode));
    }

    for (auto& run : runs) {
        removeConflictingInlineStyleFromRun(style, run.start, run.end, run.pastEndNode, editingState);
        if (editingState->isAborted())
            return;
        if (run.startAndEndAreStillInDocument()) {
            run.positionForStyleComputation = positionToComputeInlineStyleChange(run.start, run.dummyElement, editingState);
            if (editingState->isAborted())
                return;
        }
    }

    document().updateStyleAndLayoutIgnorePendingStylesheets();

    for (auto& run : runs) {
        if (run.positionForStyleComputation.isNotNull())
            run.change = StyleChange(style, run.positionForStyleComputation);
    }

    for (auto& run : runs) {
        if (run.dummyElement) {
            removeNode(run.dummyElement, editingState);
            if (editingState->isAborted())
                return;
        }
        if (run.startAndEndAreStillInDocument()) {
            applyInlineStyleChange(run.start.release(), run.end.release(), run.change, AddStyledElement, editingState);
            if (editingState->isAborted())
                return;
        }
    }
}

bool ApplyStyleCommand::isStyledInlineElementToRemove(Element* element) const
{
    return (m_styledInlineElement && element->hasTagName(m_styledInlineElement->tagQName()))
        || (m_isInlineElementToRemoveFunction && m_isInlineElementToRemoveFunction(element));
}

bool ApplyStyleCommand::shouldApplyInlineStyleToRun(EditingStyle* style, Node* runStart, Node* pastEndNode)
{
    DCHECK(style);
    DCHECK(runStart);

    for (Node* node = runStart; node && node != pastEndNode; node = NodeTraversal::next(*node)) {
        if (node->hasChildren())
            continue;
        // We don't consider m_isInlineElementToRemoveFunction here because we never apply style when m_isInlineElementToRemoveFunction is specified
        if (!style->styleIsPresentInComputedStyleOfNode(node))
            return true;
        if (m_styledInlineElement && !enclosingElementWithTag(Position::beforeNode(node), m_styledInlineElement->tagQName()))
            return true;
    }
    return false;
}

void ApplyStyleCommand::removeConflictingInlineStyleFromRun(EditingStyle* style, Member<Node>& runStart, Member<Node>& runEnd, Node* pastEndNode, EditingState* editingState)
{
    DCHECK(runStart);
    DCHECK(runEnd);
    Node* next = runStart;
    for (Node* node = next; node && node->inShadowIncludingDocument() && node != pastEndNode; node = next) {
        if (editingIgnoresContent(node)) {
            DCHECK(!node->contains(pastEndNode)) << node << " " << pastEndNode;
            next = NodeTraversal::nextSkippingChildren(*node);
        } else {
            next = NodeTraversal::next(*node);
        }
        if (!node->isHTMLElement())
            continue;

        HTMLElement& element = toHTMLElement(*node);
        Node* previousSibling = element.previousSibling();
        Node* nextSibling = element.nextSibling();
        ContainerNode* parent = element.parentNode();
        removeInlineStyleFromElement(style, &element, editingState, RemoveAlways);
        if (editingState->isAborted())
            return;
        if (!element.inShadowIncludingDocument()) {
            // FIXME: We might need to update the start and the end of current selection here but need a test.
            if (runStart == element)
                runStart = previousSibling ? previousSibling->nextSibling() : parent->firstChild();
            if (runEnd == element)
                runEnd = nextSibling ? nextSibling->previousSibling() : parent->lastChild();
        }
    }
}

bool ApplyStyleCommand::removeInlineStyleFromElement(EditingStyle* style, HTMLElement* element, EditingState* editingState, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    DCHECK(element);

    if (!element->parentNode() || !element->parentNode()->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable))
        return false;

    if (isStyledInlineElementToRemove(element)) {
        if (mode == RemoveNone)
            return true;
        if (extractedStyle)
            extractedStyle->mergeInlineStyleOfElement(element, EditingStyle::OverrideValues);
        removeNodePreservingChildren(element, editingState);
        if (editingState->isAborted())
            return false;
        return true;
    }

    bool removed = removeImplicitlyStyledElement(style, element, mode, extractedStyle, editingState);
    if (editingState->isAborted())
        return false;

    if (!element->inShadowIncludingDocument())
        return removed;

    // If the node was converted to a span, the span may still contain relevant
    // styles which must be removed (e.g. <b style='font-weight: bold'>)
    if (removeCSSStyle(style, element, editingState, mode, extractedStyle))
        removed = true;
    if (editingState->isAborted())
        return false;

    return removed;
}

void ApplyStyleCommand::replaceWithSpanOrRemoveIfWithoutAttributes(HTMLElement* elem, EditingState* editingState)
{
    if (hasNoAttributeOrOnlyStyleAttribute(elem, StyleAttributeShouldBeEmpty))
        removeNodePreservingChildren(elem, editingState);
    else
        replaceElementWithSpanPreservingChildrenAndAttributes(elem);
}

bool ApplyStyleCommand::removeImplicitlyStyledElement(EditingStyle* style, HTMLElement* element, InlineStyleRemovalMode mode, EditingStyle* extractedStyle, EditingState* editingState)
{
    DCHECK(style);
    if (mode == RemoveNone) {
        DCHECK(!extractedStyle);
        return style->conflictsWithImplicitStyleOfElement(element) || style->conflictsWithImplicitStyleOfAttributes(element);
    }

    DCHECK(mode == RemoveIfNeeded || mode == RemoveAlways);
    if (style->conflictsWithImplicitStyleOfElement(element, extractedStyle, mode == RemoveAlways ? EditingStyle::ExtractMatchingStyle : EditingStyle::DoNotExtractMatchingStyle)) {
        replaceWithSpanOrRemoveIfWithoutAttributes(element, editingState);
        if (editingState->isAborted())
            return false;
        return true;
    }

    // unicode-bidi and direction are pushed down separately so don't push down with other styles
    Vector<QualifiedName> attributes;
    if (!style->extractConflictingImplicitStyleOfAttributes(element, extractedStyle ? EditingStyle::PreserveWritingDirection : EditingStyle::DoNotPreserveWritingDirection,
        extractedStyle, attributes, mode == RemoveAlways ? EditingStyle::ExtractMatchingStyle : EditingStyle::DoNotExtractMatchingStyle))
        return false;

    for (const auto& attribute : attributes)
        removeElementAttribute(element, attribute);

    if (isEmptyFontTag(element) || isSpanWithoutAttributesOrUnstyledStyleSpan(element)) {
        removeNodePreservingChildren(element, editingState);
        if (editingState->isAborted())
            return false;
    }

    return true;
}

bool ApplyStyleCommand::removeCSSStyle(EditingStyle* style, HTMLElement* element, EditingState* editingState, InlineStyleRemovalMode mode, EditingStyle* extractedStyle)
{
    DCHECK(style);
    DCHECK(element);

    if (mode == RemoveNone)
        return style->conflictsWithInlineStyleOfElement(element);

    Vector<CSSPropertyID> properties;
    if (!style->conflictsWithInlineStyleOfElement(element, extractedStyle, properties))
        return false;

    // FIXME: We should use a mass-removal function here but we don't have an undoable one yet.
    for (const auto& property : properties)
        removeCSSProperty(element, property);

    if (isSpanWithoutAttributesOrUnstyledStyleSpan(element))
        removeNodePreservingChildren(element, editingState);

    return true;
}

HTMLElement* ApplyStyleCommand::highestAncestorWithConflictingInlineStyle(EditingStyle* style, Node* node)
{
    if (!node)
        return 0;

    HTMLElement* result = nullptr;
    Node* unsplittableElement = unsplittableElementForPosition(firstPositionInOrBeforeNode(node));

    for (Node *n = node; n; n = n->parentNode()) {
        if (n->isHTMLElement() && shouldRemoveInlineStyleFromElement(style, toHTMLElement(n)))
            result = toHTMLElement(n);
        // Should stop at the editable root (cannot cross editing boundary) and
        // also stop at the unsplittable element to be consistent with other UAs
        if (n == unsplittableElement)
            break;
    }

    return result;
}

void ApplyStyleCommand::applyInlineStyleToPushDown(Node* node, EditingStyle* style, EditingState* editingState)
{
    DCHECK(node);

    node->document().updateStyleAndLayoutTree();

    if (!style || style->isEmpty() || !node->layoutObject() || isHTMLIFrameElement(*node))
        return;

    EditingStyle* newInlineStyle = style;
    if (node->isHTMLElement() && toHTMLElement(node)->inlineStyle()) {
        newInlineStyle = style->copy();
        newInlineStyle->mergeInlineStyleOfElement(toHTMLElement(node), EditingStyle::OverrideValues);
    }

    // Since addInlineStyleIfNeeded can't add styles to block-flow layout objects, add style attribute instead.
    // FIXME: applyInlineStyleToRange should be used here instead.
    if ((node->layoutObject()->isLayoutBlockFlow() || node->hasChildren()) && node->isHTMLElement()) {
        setNodeAttribute(toHTMLElement(node), styleAttr, AtomicString(newInlineStyle->style()->asText()));
        return;
    }

    if (node->layoutObject()->isText() && toLayoutText(node->layoutObject())->isAllCollapsibleWhitespace())
        return;

    // We can't wrap node with the styled element here because new styled element will never be removed if we did.
    // If we modified the child pointer in pushDownInlineStyleAroundNode to point to new style element
    // then we fall into an infinite loop where we keep removing and adding styled element wrapping node.
    addInlineStyleIfNeeded(newInlineStyle, node, node, editingState);
}

void ApplyStyleCommand::pushDownInlineStyleAroundNode(EditingStyle* style, Node* targetNode, EditingState* editingState)
{
    HTMLElement* highestAncestor = highestAncestorWithConflictingInlineStyle(style, targetNode);
    if (!highestAncestor)
        return;

    // The outer loop is traversing the tree vertically from highestAncestor to targetNode
    Node* current = highestAncestor;
    // Along the way, styled elements that contain targetNode are removed and accumulated into elementsToPushDown.
    // Each child of the removed element, exclusing ancestors of targetNode, is then wrapped by clones of elements in elementsToPushDown.
    HeapVector<Member<Element>> elementsToPushDown;
    while (current && current != targetNode && current->contains(targetNode)) {
        NodeVector currentChildren;
        getChildNodes(toContainerNode(*current), currentChildren);
        Element* styledElement = nullptr;
        if (current->isStyledElement() && isStyledInlineElementToRemove(toElement(current))) {
            styledElement = toElement(current);
            elementsToPushDown.append(styledElement);
        }

        EditingStyle* styleToPushDown = EditingStyle::create();
        if (current->isHTMLElement()) {
            removeInlineStyleFromElement(style, toHTMLElement(current), editingState, RemoveIfNeeded, styleToPushDown);
            if (editingState->isAborted())
                return;
        }

        // The inner loop will go through children on each level
        // FIXME: we should aggregate inline child elements together so that we don't wrap each child separately.
        for (const auto& currentChild : currentChildren) {
            Node* child = currentChild;
            if (!child->parentNode())
                continue;
            if (!child->contains(targetNode) && elementsToPushDown.size()) {
                for (const auto& element : elementsToPushDown) {
                    Element* wrapper = element->cloneElementWithoutChildren();
                    wrapper->removeAttribute(styleAttr);
                    // Delete id attribute from the second element because the same id cannot be used for more than one element
                    element->removeAttribute(HTMLNames::idAttr);
                    if (isHTMLAnchorElement(element))
                        element->removeAttribute(HTMLNames::nameAttr);
                    surroundNodeRangeWithElement(child, child, wrapper, editingState);
                    if (editingState->isAborted())
                        return;
                }
            }

            // Apply style to all nodes containing targetNode and their siblings but NOT to targetNode
            // But if we've removed styledElement then go ahead and always apply the style.
            if (child != targetNode || styledElement) {
                applyInlineStyleToPushDown(child, styleToPushDown, editingState);
                if (editingState->isAborted())
                    return;
            }

            // We found the next node for the outer loop (contains targetNode)
            // When reached targetNode, stop the outer loop upon the completion of the current inner loop
            if (child == targetNode || child->contains(targetNode))
                current = child;
        }
    }
}

void ApplyStyleCommand::removeInlineStyle(EditingStyle* style, const Position &start, const Position &end, EditingState* editingState)
{
    DCHECK(start.isNotNull());
    DCHECK(end.isNotNull());
    DCHECK(start.inShadowIncludingDocument()) << start;
    DCHECK(end.inShadowIncludingDocument()) << end;
    DCHECK(Position::commonAncestorTreeScope(start, end)) << start << " " << end;
    DCHECK_LE(start, end);
    // FIXME: We should assert that start/end are not in the middle of a text node.

    Position pushDownStart = mostForwardCaretPosition(start);
    // If the pushDownStart is at the end of a text node, then this node is not fully selected.
    // Move it to the next deep quivalent position to avoid removing the style from this node.
    // e.g. if pushDownStart was at Position("hello", 5) in <b>hello<div>world</div></b>, we want Position("world", 0) instead.
    Node* pushDownStartContainer = pushDownStart.computeContainerNode();
    if (pushDownStartContainer && pushDownStartContainer->isTextNode()
        && pushDownStart.computeOffsetInContainerNode() == pushDownStartContainer->maxCharacterOffset())
        pushDownStart = nextVisuallyDistinctCandidate(pushDownStart);
    Position pushDownEnd = mostBackwardCaretPosition(end);
    // If pushDownEnd is at the start of a text node, then this node is not fully selected.
    // Move it to the previous deep equivalent position to avoid removing the style from this node.
    Node* pushDownEndContainer = pushDownEnd.computeContainerNode();
    if (pushDownEndContainer && pushDownEndContainer->isTextNode() && !pushDownEnd.computeOffsetInContainerNode())
        pushDownEnd = previousVisuallyDistinctCandidate(pushDownEnd);

    pushDownInlineStyleAroundNode(style, pushDownStart.anchorNode(), editingState);
    if (editingState->isAborted())
        return;
    pushDownInlineStyleAroundNode(style, pushDownEnd.anchorNode(), editingState);
    if (editingState->isAborted())
        return;

    // The s and e variables store the positions used to set the ending selection after style removal
    // takes place. This will help callers to recognize when either the start node or the end node
    // are removed from the document during the work of this function.
    // If pushDownInlineStyleAroundNode has pruned start.anchorNode() or end.anchorNode(),
    // use pushDownStart or pushDownEnd instead, which pushDownInlineStyleAroundNode won't prune.
    Position s = start.isNull() || start.isOrphan() ? pushDownStart : start;
    Position e = end.isNull() || end.isOrphan() ? pushDownEnd : end;

    // Current ending selection resetting algorithm assumes |start| and |end|
    // are in a same DOM tree even if they are not in document.
    if (!Position::commonAncestorTreeScope(start, end))
        return;

    Node* node = start.anchorNode();
    while (node) {
        Node* next = nullptr;
        if (editingIgnoresContent(node)) {
            DCHECK(node == end.anchorNode() || !node->contains(end.anchorNode())) << node << " " << end;
            next = NodeTraversal::nextSkippingChildren(*node);
        } else {
            next = NodeTraversal::next(*node);
        }
        if (node->isHTMLElement() && elementFullySelected(toHTMLElement(*node), start, end)) {
            HTMLElement* elem = toHTMLElement(node);
            Node* prev = NodeTraversal::previousPostOrder(*elem);
            Node* next = NodeTraversal::next(*elem);
            EditingStyle* styleToPushDown = nullptr;
            Node* childNode = nullptr;
            if (isStyledInlineElementToRemove(elem)) {
                styleToPushDown = EditingStyle::create();
                childNode = elem->firstChild();
            }

            removeInlineStyleFromElement(style, elem, editingState, RemoveIfNeeded, styleToPushDown);
            if (editingState->isAborted())
                return;
            if (!elem->inShadowIncludingDocument()) {
                if (s.anchorNode() == elem) {
                    // Since elem must have been fully selected, and it is at the start
                    // of the selection, it is clear we can set the new s offset to 0.
                    DCHECK(s.isBeforeAnchor() || s.isBeforeChildren() || s.offsetInContainerNode() <= 0) << s;
                    s = firstPositionInOrBeforeNode(next);
                }
                if (e.anchorNode() == elem) {
                    // Since elem must have been fully selected, and it is at the end
                    // of the selection, it is clear we can set the new e offset to
                    // the max range offset of prev.
                    DCHECK(s.isAfterAnchor() || !offsetIsBeforeLastNodeOffset(s.offsetInContainerNode(), s.computeContainerNode())) << s;
                    e = lastPositionInOrAfterNode(prev);
                }
            }

            if (styleToPushDown) {
                for (; childNode; childNode = childNode->nextSibling()) {
                    applyInlineStyleToPushDown(childNode, styleToPushDown, editingState);
                    if (editingState->isAborted())
                        return;
                }
            }
        }
        if (node == end.anchorNode())
            break;
        node = next;
    }

    updateStartEnd(s, e);
}

bool ApplyStyleCommand::elementFullySelected(HTMLElement& element, const Position& start, const Position& end) const
{
    // The tree may have changed and Position::upstream() relies on an up-to-date layout.
    element.document().updateStyleAndLayoutIgnorePendingStylesheets();

    return comparePositions(firstPositionInOrBeforeNode(&element), start) >= 0
        && comparePositions(mostBackwardCaretPosition(lastPositionInOrAfterNode(&element)), end) <= 0;
}

void ApplyStyleCommand::splitTextAtStart(const Position& start, const Position& end)
{
    DCHECK(start.computeContainerNode()->isTextNode()) << start;

    Position newEnd;
    if (end.isOffsetInAnchor() && start.computeContainerNode() == end.computeContainerNode())
        newEnd = Position(end.computeContainerNode(), end.offsetInContainerNode() - start.offsetInContainerNode());
    else
        newEnd = end;

    Text* text = toText(start.computeContainerNode());
    splitTextNode(text, start.offsetInContainerNode());
    updateStartEnd(Position::firstPositionInNode(text), newEnd);
}

void ApplyStyleCommand::splitTextAtEnd(const Position& start, const Position& end)
{
    DCHECK(end.computeContainerNode()->isTextNode()) << end;

    bool shouldUpdateStart = start.isOffsetInAnchor() && start.computeContainerNode() == end.computeContainerNode();
    Text* text = toText(end.anchorNode());
    splitTextNode(text, end.offsetInContainerNode());

    Node* prevNode = text->previousSibling();
    if (!prevNode || !prevNode->isTextNode())
        return;

    Position newStart = shouldUpdateStart ? Position(toText(prevNode), start.offsetInContainerNode()) : start;
    updateStartEnd(newStart, Position::lastPositionInNode(prevNode));
}

void ApplyStyleCommand::splitTextElementAtStart(const Position& start, const Position& end)
{
    DCHECK(start.computeContainerNode()->isTextNode()) << start;

    Position newEnd;
    if (start.computeContainerNode() == end.computeContainerNode())
        newEnd = Position(end.computeContainerNode(), end.offsetInContainerNode() - start.offsetInContainerNode());
    else
        newEnd = end;

    splitTextNodeContainingElement(toText(start.computeContainerNode()), start.offsetInContainerNode());
    updateStartEnd(Position::beforeNode(start.computeContainerNode()), newEnd);
}

void ApplyStyleCommand::splitTextElementAtEnd(const Position& start, const Position& end)
{
    DCHECK(end.computeContainerNode()->isTextNode()) << end;

    bool shouldUpdateStart = start.computeContainerNode() == end.computeContainerNode();
    splitTextNodeContainingElement(toText(end.computeContainerNode()), end.offsetInContainerNode());

    Node* parentElement = end.computeContainerNode()->parentNode();
    if (!parentElement || !parentElement->previousSibling())
        return;
    Node* firstTextNode = parentElement->previousSibling()->lastChild();
    if (!firstTextNode || !firstTextNode->isTextNode())
        return;

    Position newStart = shouldUpdateStart ? Position(toText(firstTextNode), start.offsetInContainerNode()) : start;
    updateStartEnd(newStart, Position::afterNode(firstTextNode));
}

bool ApplyStyleCommand::shouldSplitTextElement(Element* element, EditingStyle* style)
{
    if (!element || !element->isHTMLElement())
        return false;

    return shouldRemoveInlineStyleFromElement(style, toHTMLElement(element));
}

bool ApplyStyleCommand::isValidCaretPositionInTextNode(const Position& position)
{
    DCHECK(position.isNotNull());

    Node* node = position.computeContainerNode();
    if (!position.isOffsetInAnchor() || !node->isTextNode())
        return false;
    int offsetInText = position.offsetInContainerNode();
    return offsetInText > caretMinOffset(node) && offsetInText < caretMaxOffset(node);
}

bool ApplyStyleCommand::mergeStartWithPreviousIfIdentical(const Position& start, const Position& end, EditingState* editingState)
{
    Node* startNode = start.computeContainerNode();
    int startOffset = start.computeOffsetInContainerNode();
    if (startOffset)
        return false;

    if (isAtomicNode(startNode)) {
        // note: prior siblings could be unrendered elements. it's silly to miss the
        // merge opportunity just for that.
        if (startNode->previousSibling())
            return false;

        startNode = startNode->parentNode();
    }

    if (!startNode->isElementNode())
        return false;

    Node* previousSibling = startNode->previousSibling();

    if (previousSibling && areIdenticalElements(*startNode, *previousSibling)) {
        Element* previousElement = toElement(previousSibling);
        Element* element = toElement(startNode);
        Node* startChild = element->firstChild();
        DCHECK(startChild);
        mergeIdenticalElements(previousElement, element, editingState);
        if (editingState->isAborted())
            return false;

        int startOffsetAdjustment = startChild->nodeIndex();
        int endOffsetAdjustment = startNode == end.anchorNode() ? startOffsetAdjustment : 0;
        updateStartEnd(Position(startNode, startOffsetAdjustment),
            Position(end.anchorNode(), end.computeEditingOffset() + endOffsetAdjustment));
        return true;
    }

    return false;
}

bool ApplyStyleCommand::mergeEndWithNextIfIdentical(const Position& start, const Position& end, EditingState* editingState)
{
    Node* endNode = end.computeContainerNode();

    if (isAtomicNode(endNode)) {
        int endOffset = end.computeOffsetInContainerNode();
        if (offsetIsBeforeLastNodeOffset(endOffset, endNode))
            return false;

        if (end.anchorNode()->nextSibling())
            return false;

        endNode = end.anchorNode()->parentNode();
    }

    if (!endNode->isElementNode() || isHTMLBRElement(*endNode))
        return false;

    Node* nextSibling = endNode->nextSibling();
    if (nextSibling && areIdenticalElements(*endNode, *nextSibling)) {
        Element* nextElement = toElement(nextSibling);
        Element* element = toElement(endNode);
        Node* nextChild = nextElement->firstChild();

        mergeIdenticalElements(element, nextElement, editingState);
        if (editingState->isAborted())
            return false;

        bool shouldUpdateStart = start.computeContainerNode() == endNode;
        int endOffset = nextChild ? nextChild->nodeIndex() : nextElement->childNodes()->length();
        updateStartEnd(shouldUpdateStart ? Position(nextElement, start.offsetInContainerNode()) : start,
            Position(nextElement, endOffset));
        return true;
    }

    return false;
}

void ApplyStyleCommand::surroundNodeRangeWithElement(Node* passedStartNode, Node* endNode, Element* elementToInsert, EditingState* editingState)
{
    DCHECK(passedStartNode);
    DCHECK(endNode);
    DCHECK(elementToInsert);
    Node* node = passedStartNode;
    Element* element = elementToInsert;

    insertNodeBefore(element, node, editingState);
    if (editingState->isAborted())
        return;

    while (node) {
        Node* next = node->nextSibling();
        if (node->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable)) {
            removeNode(node, editingState);
            if (editingState->isAborted())
                return;
            appendNode(node, element, editingState);
            if (editingState->isAborted())
                return;
        }
        if (node == endNode)
            break;
        node = next;
    }

    Node* nextSibling = element->nextSibling();
    Node* previousSibling = element->previousSibling();
    if (nextSibling && nextSibling->isElementNode() && nextSibling->hasEditableStyle()
        && areIdenticalElements(*element, toElement(*nextSibling))) {
        mergeIdenticalElements(element, toElement(nextSibling), editingState);
        if (editingState->isAborted())
            return;
    }

    if (previousSibling && previousSibling->isElementNode() && previousSibling->hasEditableStyle()) {
        Node* mergedElement = previousSibling->nextSibling();
        if (mergedElement->isElementNode() && mergedElement->hasEditableStyle()
            && areIdenticalElements(toElement(*previousSibling), toElement(*mergedElement))) {
            mergeIdenticalElements(toElement(previousSibling), toElement(mergedElement), editingState);
            if (editingState->isAborted())
                return;
        }
    }

    // FIXME: We should probably call updateStartEnd if the start or end was in the node
    // range so that the endingSelection() is canonicalized.  See the comments at the end of
    // VisibleSelection::validate().
}

void ApplyStyleCommand::addBlockStyle(const StyleChange& styleChange, HTMLElement* block)
{
    // Do not check for legacy styles here. Those styles, like <B> and <I>, only apply for
    // inline content.
    if (!block)
        return;

    String cssStyle = styleChange.cssStyle();
    StringBuilder cssText;
    cssText.append(cssStyle);
    if (const StylePropertySet* decl = block->inlineStyle()) {
        if (!cssStyle.isEmpty())
            cssText.append(' ');
        cssText.append(decl->asText());
    }
    setNodeAttribute(block, styleAttr, cssText.toAtomicString());
}

void ApplyStyleCommand::addInlineStyleIfNeeded(EditingStyle* style, Node* passedStart, Node* passedEnd, EditingState* editingState)
{
    if (!passedStart || !passedEnd || !passedStart->inShadowIncludingDocument() || !passedEnd->inShadowIncludingDocument())
        return;

    Node* start = passedStart;
    Member<HTMLSpanElement> dummyElement = nullptr;
    StyleChange styleChange(style, positionToComputeInlineStyleChange(start, dummyElement, editingState));
    if (editingState->isAborted())
        return;

    if (dummyElement) {
        removeNode(dummyElement, editingState);
        if (editingState->isAborted())
            return;
    }

    applyInlineStyleChange(start, passedEnd, styleChange, DoNotAddStyledElement, editingState);
}

Position ApplyStyleCommand::positionToComputeInlineStyleChange(Node* startNode, Member<HTMLSpanElement>& dummyElement, EditingState* editingState)
{
    // It's okay to obtain the style at the startNode because we've removed all relevant styles from the current run.
    if (!startNode->isElementNode()) {
        dummyElement = HTMLSpanElement::create(document());
        insertNodeAt(dummyElement, Position::beforeNode(startNode), editingState);
        if (editingState->isAborted())
            return Position();
        return Position::beforeNode(dummyElement);
    }

    return firstPositionInOrBeforeNode(startNode);
}

void ApplyStyleCommand::applyInlineStyleChange(Node* passedStart, Node* passedEnd, StyleChange& styleChange, EAddStyledElement addStyledElement, EditingState* editingState)
{
    Node* startNode = passedStart;
    Node* endNode = passedEnd;
    DCHECK(startNode->inShadowIncludingDocument()) << startNode;
    DCHECK(endNode->inShadowIncludingDocument()) << endNode;

    // Find appropriate font and span elements top-down.
    HTMLFontElement* fontContainer = nullptr;
    HTMLElement* styleContainer = nullptr;
    for (Node* container = startNode; container && startNode == endNode; container = container->firstChild()) {
        if (isHTMLFontElement(*container))
            fontContainer = toHTMLFontElement(container);
        bool styleContainerIsNotSpan = !isHTMLSpanElement(styleContainer);
        if (container->isHTMLElement()) {
            HTMLElement* containerElement = toHTMLElement(container);
            if (isHTMLSpanElement(*containerElement) || (styleContainerIsNotSpan && containerElement->hasChildren()))
                styleContainer = toHTMLElement(container);
        }
        if (!container->hasChildren())
            break;
        startNode = container->firstChild();
        endNode = container->lastChild();
    }

    // Font tags need to go outside of CSS so that CSS font sizes override leagcy font sizes.
    if (styleChange.applyFontColor() || styleChange.applyFontFace() || styleChange.applyFontSize()) {
        if (fontContainer) {
            if (styleChange.applyFontColor())
                setNodeAttribute(fontContainer, colorAttr, AtomicString(styleChange.fontColor()));
            if (styleChange.applyFontFace())
                setNodeAttribute(fontContainer, faceAttr, AtomicString(styleChange.fontFace()));
            if (styleChange.applyFontSize())
                setNodeAttribute(fontContainer, sizeAttr, AtomicString(styleChange.fontSize()));
        } else {
            HTMLFontElement* fontElement = HTMLFontElement::create(document());
            if (styleChange.applyFontColor())
                fontElement->setAttribute(colorAttr, AtomicString(styleChange.fontColor()));
            if (styleChange.applyFontFace())
                fontElement->setAttribute(faceAttr, AtomicString(styleChange.fontFace()));
            if (styleChange.applyFontSize())
                fontElement->setAttribute(sizeAttr, AtomicString(styleChange.fontSize()));
            surroundNodeRangeWithElement(startNode, endNode, fontElement, editingState);
            if (editingState->isAborted())
                return;
        }
    }

    if (styleChange.cssStyle().length()) {
        if (styleContainer) {
            if (const StylePropertySet* existingStyle = styleContainer->inlineStyle()) {
                String existingText = existingStyle->asText();
                StringBuilder cssText;
                cssText.append(existingText);
                if (!existingText.isEmpty())
                    cssText.append(' ');
                cssText.append(styleChange.cssStyle());
                setNodeAttribute(styleContainer, styleAttr, cssText.toAtomicString());
            } else {
                setNodeAttribute(styleContainer, styleAttr, AtomicString(styleChange.cssStyle()));
            }
        } else {
            HTMLSpanElement* styleElement = HTMLSpanElement::create(document());
            styleElement->setAttribute(styleAttr, AtomicString(styleChange.cssStyle()));
            surroundNodeRangeWithElement(startNode, endNode, styleElement, editingState);
            if (editingState->isAborted())
                return;
        }
    }

    if (styleChange.applyBold()) {
        surroundNodeRangeWithElement(startNode, endNode, HTMLElement::create(bTag, document()), editingState);
        if (editingState->isAborted())
            return;
    }

    if (styleChange.applyItalic()) {
        surroundNodeRangeWithElement(startNode, endNode, HTMLElement::create(iTag, document()), editingState);
        if (editingState->isAborted())
            return;
    }

    if (styleChange.applyUnderline()) {
        surroundNodeRangeWithElement(startNode, endNode, HTMLElement::create(uTag, document()), editingState);
        if (editingState->isAborted())
            return;
    }

    if (styleChange.applyLineThrough()) {
        surroundNodeRangeWithElement(startNode, endNode, HTMLElement::create(strikeTag, document()), editingState);
        if (editingState->isAborted())
            return;
    }

    if (styleChange.applySubscript()) {
        surroundNodeRangeWithElement(startNode, endNode, HTMLElement::create(subTag, document()), editingState);
        if (editingState->isAborted())
            return;
    } else if (styleChange.applySuperscript()) {
        surroundNodeRangeWithElement(startNode, endNode, HTMLElement::create(supTag, document()), editingState);
        if (editingState->isAborted())
            return;
    }

    if (m_styledInlineElement && addStyledElement == AddStyledElement)
        surroundNodeRangeWithElement(startNode, endNode, m_styledInlineElement->cloneElementWithoutChildren(), editingState);
}

float ApplyStyleCommand::computedFontSize(Node* node)
{
    if (!node)
        return 0;

    CSSComputedStyleDeclaration* style = CSSComputedStyleDeclaration::create(node);
    if (!style)
        return 0;

    const CSSPrimitiveValue* value = toCSSPrimitiveValue(style->getPropertyCSSValue(CSSPropertyFontSize));
    if (!value)
        return 0;

    // TODO(yosin): We should have printer for |CSSPrimitiveValue::UnitType|.
    DCHECK(value->typeWithCalcResolved() == CSSPrimitiveValue::UnitType::Pixels);
    return value->getFloatValue();
}

void ApplyStyleCommand::joinChildTextNodes(ContainerNode* node, const Position& start, const Position& end)
{
    if (!node)
        return;

    Position newStart = start;
    Position newEnd = end;

    HeapVector<Member<Text>> textNodes;
    for (Node* curr = node->firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isTextNode())
            continue;

        textNodes.append(toText(curr));
    }

    for (const auto& textNode : textNodes) {
        Text* childText = textNode;
        Node* next = childText->nextSibling();
        if (!next || !next->isTextNode())
            continue;

        Text* nextText = toText(next);
        if (start.isOffsetInAnchor() && next == start.computeContainerNode())
            newStart = Position(childText, childText->length() + start.offsetInContainerNode());
        if (end.isOffsetInAnchor() && next == end.computeContainerNode())
            newEnd = Position(childText, childText->length() + end.offsetInContainerNode());
        String textToMove = nextText->data();
        insertTextIntoNode(childText, childText->length(), textToMove);
        // Removing a Text node doesn't dispatch synchronous events.
        removeNode(next, ASSERT_NO_EDITING_ABORT);
        // don't move child node pointer. it may want to merge with more text nodes.
    }

    updateStartEnd(newStart, newEnd);
}

DEFINE_TRACE(ApplyStyleCommand)
{
    visitor->trace(m_style);
    visitor->trace(m_start);
    visitor->trace(m_end);
    visitor->trace(m_styledInlineElement);
    CompositeEditCommand::trace(visitor);
}

} // namespace blink
