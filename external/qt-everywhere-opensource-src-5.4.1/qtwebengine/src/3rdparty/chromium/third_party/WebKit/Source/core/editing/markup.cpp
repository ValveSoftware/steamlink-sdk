/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009, 2010, 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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

#include "config.h"
#include "core/editing/markup.h"

#include "bindings/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/CDATASection.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/MarkupAccumulator.h"
#include "core/editing/TextIterator.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/rendering/RenderObject.h"
#include "platform/weborigin/KURL.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

using namespace HTMLNames;

static bool propertyMissingOrEqualToNone(StylePropertySet*, CSSPropertyID);

class AttributeChange {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    AttributeChange()
        : m_name(nullAtom, nullAtom, nullAtom)
    {
    }

    AttributeChange(PassRefPtrWillBeRawPtr<Element> element, const QualifiedName& name, const String& value)
        : m_element(element), m_name(name), m_value(value)
    {
    }

    void apply()
    {
        m_element->setAttribute(m_name, AtomicString(m_value));
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_element);
    }

private:
    RefPtrWillBeMember<Element> m_element;
    QualifiedName m_name;
    String m_value;
};

} // namespace WebCore

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(WebCore::AttributeChange);

namespace WebCore {

static void completeURLs(DocumentFragment& fragment, const String& baseURL)
{
    WillBeHeapVector<AttributeChange> changes;

    KURL parsedBaseURL(ParsedURLString, baseURL);

    for (Element* element = ElementTraversal::firstWithin(fragment); element; element = ElementTraversal::next(*element, &fragment)) {
        if (!element->hasAttributes())
            continue;
        AttributeCollection attributes = element->attributes();
        AttributeCollection::const_iterator end = attributes.end();
        for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
            if (element->isURLAttribute(*it) && !it->value().isEmpty())
                changes.append(AttributeChange(element, it->name(), KURL(parsedBaseURL, it->value()).string()));
        }
    }

    size_t numChanges = changes.size();
    for (size_t i = 0; i < numChanges; ++i)
        changes[i].apply();
}

class StyledMarkupAccumulator FINAL : public MarkupAccumulator {
public:
    enum RangeFullySelectsNode { DoesFullySelectNode, DoesNotFullySelectNode };

    StyledMarkupAccumulator(WillBeHeapVector<RawPtrWillBeMember<Node> >* nodes, EAbsoluteURLs, EAnnotateForInterchange, RawPtr<const Range>, Node* highestNodeToBeSerialized = 0);
    Node* serializeNodes(Node* startNode, Node* pastEnd);
    void appendString(const String& s) { return MarkupAccumulator::appendString(s); }
    void wrapWithNode(Node&, bool convertBlocksToInlines = false, RangeFullySelectsNode = DoesFullySelectNode);
    void wrapWithStyleNode(StylePropertySet*, const Document&, bool isBlock = false);
    String takeResults();

private:
    void appendStyleNodeOpenTag(StringBuilder&, StylePropertySet*, const Document&, bool isBlock = false);
    const String& styleNodeCloseTag(bool isBlock = false);
    virtual void appendText(StringBuilder& out, Text&) OVERRIDE;
    String renderedText(Node&, const Range*);
    String stringValueForRange(const Node&, const Range*);
    void appendElement(StringBuilder& out, Element&, bool addDisplayInline, RangeFullySelectsNode);
    virtual void appendElement(StringBuilder& out, Element& element, Namespaces*) OVERRIDE { appendElement(out, element, false, DoesFullySelectNode); }

    enum NodeTraversalMode { EmitString, DoNotEmitString };
    Node* traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode);

    bool shouldAnnotate() { return m_shouldAnnotate == AnnotateForInterchange; }
    bool shouldApplyWrappingStyle(const Node& node) const
    {
        return m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode() == node.parentNode()
            && m_wrappingStyle && m_wrappingStyle->style();
    }

    Vector<String> m_reversedPrecedingMarkup;
    const EAnnotateForInterchange m_shouldAnnotate;
    RawPtrWillBeMember<Node> m_highestNodeToBeSerialized;
    RefPtrWillBeMember<EditingStyle> m_wrappingStyle;
};

inline StyledMarkupAccumulator::StyledMarkupAccumulator(WillBeHeapVector<RawPtrWillBeMember<Node> >* nodes, EAbsoluteURLs shouldResolveURLs, EAnnotateForInterchange shouldAnnotate, RawPtr<const Range> range, Node* highestNodeToBeSerialized)
    : MarkupAccumulator(nodes, shouldResolveURLs, range)
    , m_shouldAnnotate(shouldAnnotate)
    , m_highestNodeToBeSerialized(highestNodeToBeSerialized)
{
}

void StyledMarkupAccumulator::wrapWithNode(Node& node, bool convertBlocksToInlines, RangeFullySelectsNode rangeFullySelectsNode)
{
    StringBuilder markup;
    if (node.isElementNode())
        appendElement(markup, toElement(node), convertBlocksToInlines && isBlock(&node), rangeFullySelectsNode);
    else
        appendStartMarkup(markup, node, 0);
    m_reversedPrecedingMarkup.append(markup.toString());
    appendEndTag(node);
    if (m_nodes)
        m_nodes->append(&node);
}

void StyledMarkupAccumulator::wrapWithStyleNode(StylePropertySet* style, const Document& document, bool isBlock)
{
    StringBuilder openTag;
    appendStyleNodeOpenTag(openTag, style, document, isBlock);
    m_reversedPrecedingMarkup.append(openTag.toString());
    appendString(styleNodeCloseTag(isBlock));
}

void StyledMarkupAccumulator::appendStyleNodeOpenTag(StringBuilder& out, StylePropertySet* style, const Document& document, bool isBlock)
{
    // wrappingStyleForSerialization should have removed -webkit-text-decorations-in-effect
    ASSERT(propertyMissingOrEqualToNone(style, CSSPropertyWebkitTextDecorationsInEffect));
    if (isBlock)
        out.appendLiteral("<div style=\"");
    else
        out.appendLiteral("<span style=\"");
    appendAttributeValue(out, style->asText(), document.isHTMLDocument());
    out.appendLiteral("\">");
}

const String& StyledMarkupAccumulator::styleNodeCloseTag(bool isBlock)
{
    DEFINE_STATIC_LOCAL(const String, divClose, ("</div>"));
    DEFINE_STATIC_LOCAL(const String, styleSpanClose, ("</span>"));
    return isBlock ? divClose : styleSpanClose;
}

String StyledMarkupAccumulator::takeResults()
{
    StringBuilder result;
    result.reserveCapacity(totalLength(m_reversedPrecedingMarkup) + length());

    for (size_t i = m_reversedPrecedingMarkup.size(); i > 0; --i)
        result.append(m_reversedPrecedingMarkup[i - 1]);

    concatenateMarkup(result);

    // We remove '\0' characters because they are not visibly rendered to the user.
    return result.toString().replace(0, "");
}

void StyledMarkupAccumulator::appendText(StringBuilder& out, Text& text)
{
    const bool parentIsTextarea = text.parentElement() && text.parentElement()->tagQName() == textareaTag;
    const bool wrappingSpan = shouldApplyWrappingStyle(text) && !parentIsTextarea;
    if (wrappingSpan) {
        RefPtrWillBeRawPtr<EditingStyle> wrappingStyle = m_wrappingStyle->copy();
        // FIXME: <rdar://problem/5371536> Style rules that match pasted content can change it's appearance
        // Make sure spans are inline style in paste side e.g. span { display: block }.
        wrappingStyle->forceInline();
        // FIXME: Should this be included in forceInline?
        wrappingStyle->style()->setProperty(CSSPropertyFloat, CSSValueNone);

        appendStyleNodeOpenTag(out, wrappingStyle->style(), text.document());
    }

    if (!shouldAnnotate() || parentIsTextarea)
        MarkupAccumulator::appendText(out, text);
    else {
        const bool useRenderedText = !enclosingNodeWithTag(firstPositionInNode(&text), selectTag);
        String content = useRenderedText ? renderedText(text, m_range) : stringValueForRange(text, m_range);
        StringBuilder buffer;
        appendCharactersReplacingEntities(buffer, content, 0, content.length(), EntityMaskInPCDATA);
        out.append(convertHTMLTextToInterchangeFormat(buffer.toString(), text));
    }

    if (wrappingSpan)
        out.append(styleNodeCloseTag());
}

String StyledMarkupAccumulator::renderedText(Node& node, const Range* range)
{
    if (!node.isTextNode())
        return String();

    const Text& textNode = toText(node);
    unsigned startOffset = 0;
    unsigned endOffset = textNode.length();

    if (range && node == range->startContainer())
        startOffset = range->startOffset();
    if (range && node == range->endContainer())
        endOffset = range->endOffset();

    Position start = createLegacyEditingPosition(&node, startOffset);
    Position end = createLegacyEditingPosition(&node, endOffset);
    return plainText(Range::create(node.document(), start, end).get());
}

String StyledMarkupAccumulator::stringValueForRange(const Node& node, const Range* range)
{
    if (!range)
        return node.nodeValue();

    String str = node.nodeValue();
    if (node == range->endContainer())
        str.truncate(range->endOffset());
    if (node == range->startContainer())
        str.remove(0, range->startOffset());
    return str;
}

void StyledMarkupAccumulator::appendElement(StringBuilder& out, Element& element, bool addDisplayInline, RangeFullySelectsNode rangeFullySelectsNode)
{
    const bool documentIsHTML = element.document().isHTMLDocument();
    appendOpenTag(out, element, 0);

    const bool shouldAnnotateOrForceInline = element.isHTMLElement() && (shouldAnnotate() || addDisplayInline);
    const bool shouldOverrideStyleAttr = shouldAnnotateOrForceInline || shouldApplyWrappingStyle(element);

    if (element.hasAttributes()) {
        AttributeCollection attributes = element.attributes();
        AttributeCollection::const_iterator end = attributes.end();
        for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
            // We'll handle the style attribute separately, below.
            if (it->name() == styleAttr && shouldOverrideStyleAttr)
                continue;
            appendAttribute(out, element, *it, 0);
        }
    }

    if (shouldOverrideStyleAttr) {
        RefPtrWillBeRawPtr<EditingStyle> newInlineStyle = nullptr;

        if (shouldApplyWrappingStyle(element)) {
            newInlineStyle = m_wrappingStyle->copy();
            newInlineStyle->removePropertiesInElementDefaultStyle(&element);
            newInlineStyle->removeStyleConflictingWithStyleOfNode(&element);
        } else
            newInlineStyle = EditingStyle::create();

        if (element.isStyledElement() && element.inlineStyle())
            newInlineStyle->overrideWithStyle(element.inlineStyle());

        if (shouldAnnotateOrForceInline) {
            if (shouldAnnotate())
                newInlineStyle->mergeStyleFromRulesForSerialization(&toHTMLElement(element));

            if (addDisplayInline)
                newInlineStyle->forceInline();

            // If the node is not fully selected by the range, then we don't want to keep styles that affect its relationship to the nodes around it
            // only the ones that affect it and the nodes within it.
            if (rangeFullySelectsNode == DoesNotFullySelectNode && newInlineStyle->style())
                newInlineStyle->style()->removeProperty(CSSPropertyFloat);
        }

        if (!newInlineStyle->isEmpty()) {
            out.appendLiteral(" style=\"");
            appendAttributeValue(out, newInlineStyle->style()->asText(), documentIsHTML);
            out.append('\"');
        }
    }

    appendCloseTag(out, element);
}

Node* StyledMarkupAccumulator::serializeNodes(Node* startNode, Node* pastEnd)
{
    if (!m_highestNodeToBeSerialized) {
        Node* lastClosed = traverseNodesForSerialization(startNode, pastEnd, DoNotEmitString);
        m_highestNodeToBeSerialized = lastClosed;
    }

    if (m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode())
        m_wrappingStyle = EditingStyle::wrappingStyleForSerialization(m_highestNodeToBeSerialized->parentNode(), shouldAnnotate());

    return traverseNodesForSerialization(startNode, pastEnd, EmitString);
}

Node* StyledMarkupAccumulator::traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode traversalMode)
{
    const bool shouldEmit = traversalMode == EmitString;
    WillBeHeapVector<RawPtrWillBeMember<Node> > ancestorsToClose;
    Node* next;
    Node* lastClosed = 0;
    for (Node* n = startNode; n != pastEnd; n = next) {
        // According to <rdar://problem/5730668>, it is possible for n to blow
        // past pastEnd and become null here. This shouldn't be possible.
        // This null check will prevent crashes (but create too much markup)
        // and the ASSERT will hopefully lead us to understanding the problem.
        ASSERT(n);
        if (!n)
            break;

        next = NodeTraversal::next(*n);
        bool openedTag = false;

        if (isBlock(n) && canHaveChildrenForEditing(n) && next == pastEnd)
            // Don't write out empty block containers that aren't fully selected.
            continue;

        if (!n->renderer() && !enclosingNodeWithTag(firstPositionInOrBeforeNode(n), selectTag)) {
            next = NodeTraversal::nextSkippingChildren(*n);
            // Don't skip over pastEnd.
            if (pastEnd && pastEnd->isDescendantOf(n))
                next = pastEnd;
        } else {
            // Add the node to the markup if we're not skipping the descendants
            if (shouldEmit)
                appendStartTag(*n);

            // If node has no children, close the tag now.
            if (!n->hasChildren()) {
                if (shouldEmit)
                    appendEndTag(*n);
                lastClosed = n;
            } else {
                openedTag = true;
                ancestorsToClose.append(n);
            }
        }

        // If we didn't insert open tag and there's no more siblings or we're at the end of the traversal, take care of ancestors.
        // FIXME: What happens if we just inserted open tag and reached the end?
        if (!openedTag && (!n->nextSibling() || next == pastEnd)) {
            // Close up the ancestors.
            while (!ancestorsToClose.isEmpty()) {
                Node* ancestor = ancestorsToClose.last();
                ASSERT(ancestor);
                if (next != pastEnd && next->isDescendantOf(ancestor))
                    break;
                // Not at the end of the range, close ancestors up to sibling of next node.
                if (shouldEmit)
                    appendEndTag(*ancestor);
                lastClosed = ancestor;
                ancestorsToClose.removeLast();
            }

            // Surround the currently accumulated markup with markup for ancestors we never opened as we leave the subtree(s) rooted at those ancestors.
            ContainerNode* nextParent = next ? next->parentNode() : 0;
            if (next != pastEnd && n != nextParent) {
                Node* lastAncestorClosedOrSelf = n->isDescendantOf(lastClosed) ? lastClosed : n;
                for (ContainerNode* parent = lastAncestorClosedOrSelf->parentNode(); parent && parent != nextParent; parent = parent->parentNode()) {
                    // All ancestors that aren't in the ancestorsToClose list should either be a) unrendered:
                    if (!parent->renderer())
                        continue;
                    // or b) ancestors that we never encountered during a pre-order traversal starting at startNode:
                    ASSERT(startNode->isDescendantOf(parent));
                    if (shouldEmit)
                        wrapWithNode(*parent);
                    lastClosed = parent;
                }
            }
        }
    }

    return lastClosed;
}

static bool isHTMLBlockElement(const Node* node)
{
    ASSERT(node);
    return isHTMLTableCellElement(*node)
        || isNonTableCellHTMLBlockElement(node);
}

static Node* ancestorToRetainStructureAndAppearanceForBlock(Node* commonAncestorBlock)
{
    if (!commonAncestorBlock)
        return 0;

    if (commonAncestorBlock->hasTagName(tbodyTag) || isHTMLTableRowElement(*commonAncestorBlock)) {
        ContainerNode* table = commonAncestorBlock->parentNode();
        while (table && !isHTMLTableElement(*table))
            table = table->parentNode();

        return table;
    }

    if (isNonTableCellHTMLBlockElement(commonAncestorBlock))
        return commonAncestorBlock;

    return 0;
}

static inline Node* ancestorToRetainStructureAndAppearance(Node* commonAncestor)
{
    return ancestorToRetainStructureAndAppearanceForBlock(enclosingBlock(commonAncestor));
}

static inline Node* ancestorToRetainStructureAndAppearanceWithNoRenderer(Node* commonAncestor)
{
    Node* commonAncestorBlock = enclosingNodeOfType(firstPositionInOrBeforeNode(commonAncestor), isHTMLBlockElement);
    return ancestorToRetainStructureAndAppearanceForBlock(commonAncestorBlock);
}

static bool propertyMissingOrEqualToNone(StylePropertySet* style, CSSPropertyID propertyID)
{
    if (!style)
        return false;
    RefPtrWillBeRawPtr<CSSValue> value = style->getPropertyCSSValue(propertyID);
    if (!value)
        return true;
    if (!value->isPrimitiveValue())
        return false;
    return toCSSPrimitiveValue(value.get())->getValueID() == CSSValueNone;
}

static bool needInterchangeNewlineAfter(const VisiblePosition& v)
{
    VisiblePosition next = v.next();
    Node* upstreamNode = next.deepEquivalent().upstream().deprecatedNode();
    Node* downstreamNode = v.deepEquivalent().downstream().deprecatedNode();
    // Add an interchange newline if a paragraph break is selected and a br won't already be added to the markup to represent it.
    return isEndOfParagraph(v) && isStartOfParagraph(next) && !(isHTMLBRElement(*upstreamNode) && upstreamNode == downstreamNode);
}

static PassRefPtrWillBeRawPtr<EditingStyle> styleFromMatchedRulesAndInlineDecl(const Node* node)
{
    if (!node->isHTMLElement())
        return nullptr;

    // FIXME: Having to const_cast here is ugly, but it is quite a bit of work to untangle
    // the non-const-ness of styleFromMatchedRulesForElement.
    HTMLElement* element = const_cast<HTMLElement*>(toHTMLElement(node));
    RefPtrWillBeRawPtr<EditingStyle> style = EditingStyle::create(element->inlineStyle());
    style->mergeStyleFromRules(element);
    return style.release();
}

static bool isElementPresentational(const Node* node)
{
    return node->hasTagName(uTag) || node->hasTagName(sTag) || node->hasTagName(strikeTag)
        || node->hasTagName(iTag) || node->hasTagName(emTag) || node->hasTagName(bTag) || node->hasTagName(strongTag);
}

static Node* highestAncestorToWrapMarkup(const Range* range, EAnnotateForInterchange shouldAnnotate, Node* constrainingAncestor)
{
    Node* commonAncestor = range->commonAncestorContainer();
    ASSERT(commonAncestor);
    Node* specialCommonAncestor = 0;
    if (shouldAnnotate == AnnotateForInterchange) {
        // Include ancestors that aren't completely inside the range but are required to retain
        // the structure and appearance of the copied markup.
        specialCommonAncestor = ancestorToRetainStructureAndAppearance(commonAncestor);

        if (Node* parentListNode = enclosingNodeOfType(firstPositionInOrBeforeNode(range->firstNode()), isListItem)) {
            if (WebCore::areRangesEqual(VisibleSelection::selectionFromContentsOfNode(parentListNode).toNormalizedRange().get(), range)) {
                specialCommonAncestor = parentListNode->parentNode();
                while (specialCommonAncestor && !isListElement(specialCommonAncestor))
                    specialCommonAncestor = specialCommonAncestor->parentNode();
            }
        }

        // Retain the Mail quote level by including all ancestor mail block quotes.
        if (Node* highestMailBlockquote = highestEnclosingNodeOfType(firstPositionInOrBeforeNode(range->firstNode()), isMailBlockquote, CanCrossEditingBoundary))
            specialCommonAncestor = highestMailBlockquote;
    }

    Node* checkAncestor = specialCommonAncestor ? specialCommonAncestor : commonAncestor;
    if (checkAncestor->renderer()) {
        Node* newSpecialCommonAncestor = highestEnclosingNodeOfType(firstPositionInNode(checkAncestor), &isElementPresentational, CanCrossEditingBoundary, constrainingAncestor);
        if (newSpecialCommonAncestor)
            specialCommonAncestor = newSpecialCommonAncestor;
    }

    // If a single tab is selected, commonAncestor will be a text node inside a tab span.
    // If two or more tabs are selected, commonAncestor will be the tab span.
    // In either case, if there is a specialCommonAncestor already, it will necessarily be above
    // any tab span that needs to be included.
    if (!specialCommonAncestor && isTabSpanTextNode(commonAncestor))
        specialCommonAncestor = commonAncestor->parentNode();
    if (!specialCommonAncestor && isTabSpanNode(commonAncestor))
        specialCommonAncestor = commonAncestor;

    if (Node *enclosingAnchor = enclosingNodeWithTag(firstPositionInNode(specialCommonAncestor ? specialCommonAncestor : commonAncestor), aTag))
        specialCommonAncestor = enclosingAnchor;

    return specialCommonAncestor;
}

// FIXME: Shouldn't we omit style info when annotate == DoNotAnnotateForInterchange?
// FIXME: At least, annotation and style info should probably not be included in range.markupString()
static String createMarkupInternal(Document& document, const Range* range, const Range* updatedRange, WillBeHeapVector<RawPtrWillBeMember<Node> >* nodes,
    EAnnotateForInterchange shouldAnnotate, bool convertBlocksToInlines, EAbsoluteURLs shouldResolveURLs, Node* constrainingAncestor)
{
    ASSERT(range);
    ASSERT(updatedRange);
    DEFINE_STATIC_LOCAL(const String, interchangeNewlineString, ("<br class=\"" AppleInterchangeNewline "\">"));

    bool collapsed = updatedRange->collapsed();
    if (collapsed)
        return emptyString();
    Node* commonAncestor = updatedRange->commonAncestorContainer();
    if (!commonAncestor)
        return emptyString();

    document.updateLayoutIgnorePendingStylesheets();

    Node* body = enclosingNodeWithTag(firstPositionInNode(commonAncestor), bodyTag);
    Node* fullySelectedRoot = 0;
    // FIXME: Do this for all fully selected blocks, not just the body.
    if (body && areRangesEqual(VisibleSelection::selectionFromContentsOfNode(body).toNormalizedRange().get(), range))
        fullySelectedRoot = body;
    Node* specialCommonAncestor = highestAncestorToWrapMarkup(updatedRange, shouldAnnotate, constrainingAncestor);
    StyledMarkupAccumulator accumulator(nodes, shouldResolveURLs, shouldAnnotate, updatedRange, specialCommonAncestor);
    Node* pastEnd = updatedRange->pastLastNode();

    Node* startNode = updatedRange->firstNode();
    VisiblePosition visibleStart(updatedRange->startPosition(), VP_DEFAULT_AFFINITY);
    VisiblePosition visibleEnd(updatedRange->endPosition(), VP_DEFAULT_AFFINITY);
    if (shouldAnnotate == AnnotateForInterchange && needInterchangeNewlineAfter(visibleStart)) {
        if (visibleStart == visibleEnd.previous())
            return interchangeNewlineString;

        accumulator.appendString(interchangeNewlineString);
        startNode = visibleStart.next().deepEquivalent().deprecatedNode();

        if (pastEnd && Range::compareBoundaryPoints(startNode, 0, pastEnd, 0, ASSERT_NO_EXCEPTION) >= 0)
            return interchangeNewlineString;
    }

    Node* lastClosed = accumulator.serializeNodes(startNode, pastEnd);

    if (specialCommonAncestor && lastClosed) {
        // Also include all of the ancestors of lastClosed up to this special ancestor.
        for (ContainerNode* ancestor = lastClosed->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (ancestor == fullySelectedRoot && !convertBlocksToInlines) {
                RefPtrWillBeRawPtr<EditingStyle> fullySelectedRootStyle = styleFromMatchedRulesAndInlineDecl(fullySelectedRoot);

                // Bring the background attribute over, but not as an attribute because a background attribute on a div
                // appears to have no effect.
                if ((!fullySelectedRootStyle || !fullySelectedRootStyle->style() || !fullySelectedRootStyle->style()->getPropertyCSSValue(CSSPropertyBackgroundImage))
                    && toElement(fullySelectedRoot)->hasAttribute(backgroundAttr))
                    fullySelectedRootStyle->style()->setProperty(CSSPropertyBackgroundImage, "url('" + toElement(fullySelectedRoot)->getAttribute(backgroundAttr) + "')");

                if (fullySelectedRootStyle->style()) {
                    // Reset the CSS properties to avoid an assertion error in addStyleMarkup().
                    // This assertion is caused at least when we select all text of a <body> element whose
                    // 'text-decoration' property is "inherit", and copy it.
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle->style(), CSSPropertyTextDecoration))
                        fullySelectedRootStyle->style()->setProperty(CSSPropertyTextDecoration, CSSValueNone);
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle->style(), CSSPropertyWebkitTextDecorationsInEffect))
                        fullySelectedRootStyle->style()->setProperty(CSSPropertyWebkitTextDecorationsInEffect, CSSValueNone);
                    accumulator.wrapWithStyleNode(fullySelectedRootStyle->style(), document, true);
                }
            } else {
                // Since this node and all the other ancestors are not in the selection we want to set RangeFullySelectsNode to DoesNotFullySelectNode
                // so that styles that affect the exterior of the node are not included.
                accumulator.wrapWithNode(*ancestor, convertBlocksToInlines, StyledMarkupAccumulator::DoesNotFullySelectNode);
            }
            if (nodes)
                nodes->append(ancestor);

            if (ancestor == specialCommonAncestor)
                break;
        }
    }

    // FIXME: The interchange newline should be placed in the block that it's in, not after all of the content, unconditionally.
    if (shouldAnnotate == AnnotateForInterchange && needInterchangeNewlineAfter(visibleEnd.previous()))
        accumulator.appendString(interchangeNewlineString);

    return accumulator.takeResults();
}

String createMarkup(const Range* range, WillBeHeapVector<RawPtrWillBeMember<Node> >* nodes, EAnnotateForInterchange shouldAnnotate, bool convertBlocksToInlines, EAbsoluteURLs shouldResolveURLs, Node* constrainingAncestor)
{
    if (!range)
        return emptyString();

    Document& document = range->ownerDocument();
    const Range* updatedRange = range;

    return createMarkupInternal(document, range, updatedRange, nodes, shouldAnnotate, convertBlocksToInlines, shouldResolveURLs, constrainingAncestor);
}

PassRefPtrWillBeRawPtr<DocumentFragment> createFragmentFromMarkup(Document& document, const String& markup, const String& baseURL, ParserContentPolicy parserContentPolicy)
{
    // We use a fake body element here to trick the HTML parser to using the InBody insertion mode.
    RefPtrWillBeRawPtr<HTMLBodyElement> fakeBody = HTMLBodyElement::create(document);
    RefPtrWillBeRawPtr<DocumentFragment> fragment = DocumentFragment::create(document);

    fragment->parseHTML(markup, fakeBody.get(), parserContentPolicy);

    if (!baseURL.isEmpty() && baseURL != blankURL() && baseURL != document.baseURL())
        completeURLs(*fragment, baseURL);

    return fragment.release();
}

static const char fragmentMarkerTag[] = "webkit-fragment-marker";

static bool findNodesSurroundingContext(Document* document, RefPtrWillBeRawPtr<Node>& nodeBeforeContext, RefPtrWillBeRawPtr<Node>& nodeAfterContext)
{
    for (Node* node = document->firstChild(); node; node = NodeTraversal::next(*node)) {
        if (node->nodeType() == Node::COMMENT_NODE && toCharacterData(node)->data() == fragmentMarkerTag) {
            if (!nodeBeforeContext)
                nodeBeforeContext = node;
            else {
                nodeAfterContext = node;
                return true;
            }
        }
    }
    return false;
}

static void trimFragment(DocumentFragment* fragment, Node* nodeBeforeContext, Node* nodeAfterContext)
{
    RefPtrWillBeRawPtr<Node> next = nullptr;
    for (RefPtrWillBeRawPtr<Node> node = fragment->firstChild(); node; node = next) {
        if (nodeBeforeContext->isDescendantOf(node.get())) {
            next = NodeTraversal::next(*node);
            continue;
        }
        next = NodeTraversal::nextSkippingChildren(*node);
        ASSERT(!node->contains(nodeAfterContext));
        node->parentNode()->removeChild(node.get(), ASSERT_NO_EXCEPTION);
        if (nodeBeforeContext == node)
            break;
    }

    ASSERT(nodeAfterContext->parentNode());
    for (RefPtrWillBeRawPtr<Node> node = nodeAfterContext; node; node = next) {
        next = NodeTraversal::nextSkippingChildren(*node);
        node->parentNode()->removeChild(node.get(), ASSERT_NO_EXCEPTION);
    }
}

PassRefPtrWillBeRawPtr<DocumentFragment> createFragmentFromMarkupWithContext(Document& document, const String& markup, unsigned fragmentStart, unsigned fragmentEnd,
    const String& baseURL, ParserContentPolicy parserContentPolicy)
{
    // FIXME: Need to handle the case where the markup already contains these markers.

    StringBuilder taggedMarkup;
    taggedMarkup.append(markup.left(fragmentStart));
    MarkupAccumulator::appendComment(taggedMarkup, fragmentMarkerTag);
    taggedMarkup.append(markup.substring(fragmentStart, fragmentEnd - fragmentStart));
    MarkupAccumulator::appendComment(taggedMarkup, fragmentMarkerTag);
    taggedMarkup.append(markup.substring(fragmentEnd));

    RefPtrWillBeRawPtr<DocumentFragment> taggedFragment = createFragmentFromMarkup(document, taggedMarkup.toString(), baseURL, parserContentPolicy);
    RefPtrWillBeRawPtr<Document> taggedDocument = Document::create();
    taggedDocument->setContextFeatures(document.contextFeatures());

    // FIXME: It's not clear what this code is trying to do. It puts nodes as direct children of a
    // Document that are not normally allowed by using the parser machinery.
    taggedDocument->parserTakeAllChildrenFrom(*taggedFragment);

    RefPtrWillBeRawPtr<Node> nodeBeforeContext = nullptr;
    RefPtrWillBeRawPtr<Node> nodeAfterContext = nullptr;
    if (!findNodesSurroundingContext(taggedDocument.get(), nodeBeforeContext, nodeAfterContext))
        return nullptr;

    RefPtrWillBeRawPtr<Range> range = Range::create(*taggedDocument.get(),
        positionAfterNode(nodeBeforeContext.get()).parentAnchoredEquivalent(),
        positionBeforeNode(nodeAfterContext.get()).parentAnchoredEquivalent());

    Node* commonAncestor = range->commonAncestorContainer();
    Node* specialCommonAncestor = ancestorToRetainStructureAndAppearanceWithNoRenderer(commonAncestor);

    // When there's a special common ancestor outside of the fragment, we must include it as well to
    // preserve the structure and appearance of the fragment. For example, if the fragment contains
    // TD, we need to include the enclosing TABLE tag as well.
    RefPtrWillBeRawPtr<DocumentFragment> fragment = DocumentFragment::create(document);
    if (specialCommonAncestor)
        fragment->appendChild(specialCommonAncestor);
    else
        fragment->parserTakeAllChildrenFrom(toContainerNode(*commonAncestor));

    trimFragment(fragment.get(), nodeBeforeContext.get(), nodeAfterContext.get());

    return fragment;
}

String createMarkup(const Node* node, EChildrenOnly childrenOnly, WillBeHeapVector<RawPtrWillBeMember<Node> >* nodes, EAbsoluteURLs shouldResolveURLs, Vector<QualifiedName>* tagNamesToSkip)
{
    if (!node)
        return "";

    MarkupAccumulator accumulator(nodes, shouldResolveURLs);
    return accumulator.serializeNodes(const_cast<Node&>(*node), childrenOnly, tagNamesToSkip);
}

static void fillContainerFromString(ContainerNode* paragraph, const String& string)
{
    Document& document = paragraph->document();

    if (string.isEmpty()) {
        paragraph->appendChild(createBlockPlaceholderElement(document));
        return;
    }

    ASSERT(string.find('\n') == kNotFound);

    Vector<String> tabList;
    string.split('\t', true, tabList);
    StringBuilder tabText;
    bool first = true;
    size_t numEntries = tabList.size();
    for (size_t i = 0; i < numEntries; ++i) {
        const String& s = tabList[i];

        // append the non-tab textual part
        if (!s.isEmpty()) {
            if (!tabText.isEmpty()) {
                paragraph->appendChild(createTabSpanElement(document, tabText.toString()));
                tabText.clear();
            }
            RefPtrWillBeRawPtr<Node> textNode = document.createTextNode(stringWithRebalancedWhitespace(s, first, i + 1 == numEntries));
            paragraph->appendChild(textNode.release());
        }

        // there is a tab after every entry, except the last entry
        // (if the last character is a tab, the list gets an extra empty entry)
        if (i + 1 != numEntries)
            tabText.append('\t');
        else if (!tabText.isEmpty())
            paragraph->appendChild(createTabSpanElement(document, tabText.toString()));

        first = false;
    }
}

bool isPlainTextMarkup(Node* node)
{
    ASSERT(node);
    if (!node->isElementNode())
        return false;

    Element* element = toElement(node);
    if (!isHTMLDivElement(*element) || !element->hasAttributes())
        return false;

    if (element->hasOneChild() && (element->firstChild()->isTextNode() || (element->firstChild()->firstChild())))
        return true;

    return element->hasChildCount(2) && isTabSpanTextNode(element->firstChild()->firstChild()) && element->lastChild()->isTextNode();
}

static bool shouldPreserveNewline(const Range& range)
{
    if (Node* node = range.firstNode()) {
        if (RenderObject* renderer = node->renderer())
            return renderer->style()->preserveNewline();
    }

    if (Node* node = range.startPosition().anchorNode()) {
        if (RenderObject* renderer = node->renderer())
            return renderer->style()->preserveNewline();
    }

    return false;
}

PassRefPtrWillBeRawPtr<DocumentFragment> createFragmentFromText(Range* context, const String& text)
{
    if (!context)
        return nullptr;

    Document& document = context->ownerDocument();
    RefPtrWillBeRawPtr<DocumentFragment> fragment = document.createDocumentFragment();

    if (text.isEmpty())
        return fragment.release();

    String string = text;
    string.replace("\r\n", "\n");
    string.replace('\r', '\n');

    if (shouldPreserveNewline(*context)) {
        fragment->appendChild(document.createTextNode(string));
        if (string.endsWith('\n')) {
            RefPtrWillBeRawPtr<Element> element = createBreakElement(document);
            element->setAttribute(classAttr, AppleInterchangeNewline);
            fragment->appendChild(element.release());
        }
        return fragment.release();
    }

    // A string with no newlines gets added inline, rather than being put into a paragraph.
    if (string.find('\n') == kNotFound) {
        fillContainerFromString(fragment.get(), string);
        return fragment.release();
    }

    // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
    Node* blockNode = enclosingBlock(context->firstNode());
    Element* block = toElement(blockNode);
    bool useClonesOfEnclosingBlock = blockNode
        && blockNode->isElementNode()
        && !isHTMLBodyElement(*block)
        && !isHTMLHtmlElement(*block)
        && block != editableRootForPosition(context->startPosition());
    bool useLineBreak = enclosingTextFormControl(context->startPosition());

    Vector<String> list;
    string.split('\n', true, list); // true gets us empty strings in the list
    size_t numLines = list.size();
    for (size_t i = 0; i < numLines; ++i) {
        const String& s = list[i];

        RefPtrWillBeRawPtr<Element> element = nullptr;
        if (s.isEmpty() && i + 1 == numLines) {
            // For last line, use the "magic BR" rather than a P.
            element = createBreakElement(document);
            element->setAttribute(classAttr, AppleInterchangeNewline);
        } else if (useLineBreak) {
            element = createBreakElement(document);
            fillContainerFromString(fragment.get(), s);
        } else {
            if (useClonesOfEnclosingBlock)
                element = block->cloneElementWithoutChildren();
            else
                element = createDefaultParagraphElement(document);
            fillContainerFromString(element.get(), s);
        }
        fragment->appendChild(element.release());
    }
    return fragment.release();
}

String createFullMarkup(const Node* node)
{
    if (!node)
        return String();

    LocalFrame* frame = node->document().frame();
    if (!frame)
        return String();

    // FIXME: This is never "for interchange". Is that right?
    String markupString = createMarkup(node, IncludeNode, 0);
    Node::NodeType nodeType = node->nodeType();
    if (nodeType != Node::DOCUMENT_NODE && !node->isDocumentTypeNode())
        markupString = frame->documentTypeString() + markupString;

    return markupString;
}

String urlToMarkup(const KURL& url, const String& title)
{
    StringBuilder markup;
    markup.appendLiteral("<a href=\"");
    markup.append(url.string());
    markup.appendLiteral("\">");
    MarkupAccumulator::appendCharactersReplacingEntities(markup, title, 0, title.length(), EntityMaskInPCDATA);
    markup.appendLiteral("</a>");
    return markup.toString();
}

PassRefPtrWillBeRawPtr<DocumentFragment> createFragmentForInnerOuterHTML(const String& markup, Element* contextElement, ParserContentPolicy parserContentPolicy, const char* method, ExceptionState& exceptionState)
{
    ASSERT(contextElement);
    Document& document = isHTMLTemplateElement(*contextElement) ? contextElement->document().ensureTemplateDocument() : contextElement->document();
    RefPtrWillBeRawPtr<DocumentFragment> fragment = DocumentFragment::create(document);

    if (document.isHTMLDocument()) {
        fragment->parseHTML(markup, contextElement, parserContentPolicy);
        return fragment;
    }

    bool wasValid = fragment->parseXML(markup, contextElement, parserContentPolicy);
    if (!wasValid) {
        exceptionState.throwDOMException(SyntaxError, "The provided markup is invalid XML, and therefore cannot be inserted into an XML document.");
        return nullptr;
    }
    return fragment.release();
}

PassRefPtrWillBeRawPtr<DocumentFragment> createFragmentForTransformToFragment(const String& sourceString, const String& sourceMIMEType, Document& outputDoc)
{
    RefPtrWillBeRawPtr<DocumentFragment> fragment = outputDoc.createDocumentFragment();

    if (sourceMIMEType == "text/html") {
        // As far as I can tell, there isn't a spec for how transformToFragment is supposed to work.
        // Based on the documentation I can find, it looks like we want to start parsing the fragment in the InBody insertion mode.
        // Unfortunately, that's an implementation detail of the parser.
        // We achieve that effect here by passing in a fake body element as context for the fragment.
        RefPtrWillBeRawPtr<HTMLBodyElement> fakeBody = HTMLBodyElement::create(outputDoc);
        fragment->parseHTML(sourceString, fakeBody.get());
    } else if (sourceMIMEType == "text/plain") {
        fragment->parserAppendChild(Text::create(outputDoc, sourceString));
    } else {
        bool successfulParse = fragment->parseXML(sourceString, 0);
        if (!successfulParse)
            return nullptr;
    }

    // FIXME: Do we need to mess with URLs here?

    return fragment.release();
}

static inline void removeElementPreservingChildren(PassRefPtrWillBeRawPtr<DocumentFragment> fragment, HTMLElement* element)
{
    RefPtrWillBeRawPtr<Node> nextChild = nullptr;
    for (RefPtrWillBeRawPtr<Node> child = element->firstChild(); child; child = nextChild) {
        nextChild = child->nextSibling();
        element->removeChild(child.get());
        fragment->insertBefore(child, element);
    }
    fragment->removeChild(element);
}

PassRefPtrWillBeRawPtr<DocumentFragment> createContextualFragment(const String& markup, HTMLElement* element, ParserContentPolicy parserContentPolicy, ExceptionState& exceptionState)
{
    ASSERT(element);
    if (element->ieForbidsInsertHTML() || element->hasLocalName(colTag) || element->hasLocalName(colgroupTag) || element->hasLocalName(framesetTag)
        || element->hasLocalName(headTag) || element->hasLocalName(styleTag) || element->hasLocalName(titleTag)) {
        exceptionState.throwDOMException(NotSupportedError, "The range's container is '" + element->localName() + "', which is not supported.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, element, parserContentPolicy, "createContextualFragment", exceptionState);
    if (!fragment)
        return nullptr;

    // We need to pop <html> and <body> elements and remove <head> to
    // accommodate folks passing complete HTML documents to make the
    // child of an element.

    RefPtrWillBeRawPtr<Node> nextNode = nullptr;
    for (RefPtrWillBeRawPtr<Node> node = fragment->firstChild(); node; node = nextNode) {
        nextNode = node->nextSibling();
        if (isHTMLHtmlElement(*node) || isHTMLHeadElement(*node) || isHTMLBodyElement(*node)) {
            HTMLElement* element = toHTMLElement(node);
            if (Node* firstChild = element->firstChild())
                nextNode = firstChild;
            removeElementPreservingChildren(fragment, element);
        }
    }
    return fragment.release();
}

void replaceChildrenWithFragment(ContainerNode* container, PassRefPtrWillBeRawPtr<DocumentFragment> fragment, ExceptionState& exceptionState)
{
    ASSERT(container);
    RefPtrWillBeRawPtr<ContainerNode> containerNode(container);

    ChildListMutationScope mutation(*containerNode);

    if (!fragment->firstChild()) {
        containerNode->removeChildren();
        return;
    }

    // FIXME: This is wrong if containerNode->firstChild() has more than one ref!
    if (containerNode->hasOneTextChild() && fragment->hasOneTextChild()) {
        toText(containerNode->firstChild())->setData(toText(fragment->firstChild())->data());
        return;
    }

    // FIXME: No need to replace the child it is a text node and its contents are already == text.
    if (containerNode->hasOneChild()) {
        containerNode->replaceChild(fragment, containerNode->firstChild(), exceptionState);
        return;
    }

    containerNode->removeChildren();
    containerNode->appendChild(fragment, exceptionState);
}

void replaceChildrenWithText(ContainerNode* container, const String& text, ExceptionState& exceptionState)
{
    ASSERT(container);
    RefPtrWillBeRawPtr<ContainerNode> containerNode(container);

    ChildListMutationScope mutation(*containerNode);

    // FIXME: This is wrong if containerNode->firstChild() has more than one ref! Example:
    // <div>foo</div>
    // <script>
    // var oldText = div.firstChild;
    // console.log(oldText.data); // foo
    // div.innerText = "bar";
    // console.log(oldText.data); // bar!?!
    // </script>
    // I believe this is an intentional benchmark cheat from years ago.
    // We should re-visit if we actually want this still.
    if (containerNode->hasOneTextChild()) {
        toText(containerNode->firstChild())->setData(text);
        return;
    }

    // NOTE: This method currently always creates a text node, even if that text node will be empty.
    RefPtrWillBeRawPtr<Text> textNode = Text::create(containerNode->document(), text);

    // FIXME: No need to replace the child it is a text node and its contents are already == text.
    if (containerNode->hasOneChild()) {
        containerNode->replaceChild(textNode.release(), containerNode->firstChild(), exceptionState);
        return;
    }

    containerNode->removeChildren();
    containerNode->appendChild(textNode.release(), exceptionState);
}

void mergeWithNextTextNode(PassRefPtrWillBeRawPtr<Node> node, ExceptionState& exceptionState)
{
    ASSERT(node && node->isTextNode());
    Node* next = node->nextSibling();
    if (!next || !next->isTextNode())
        return;

    RefPtrWillBeRawPtr<Text> textNode = toText(node.get());
    RefPtrWillBeRawPtr<Text> textNext = toText(next);
    textNode->appendData(textNext->data());
    if (textNext->parentNode()) // Might have been removed by mutation event.
        textNext->remove(exceptionState);
}

}
