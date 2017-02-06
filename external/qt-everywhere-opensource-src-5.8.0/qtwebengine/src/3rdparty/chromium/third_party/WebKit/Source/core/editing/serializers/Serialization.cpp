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

#include "core/editing/serializers/Serialization.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/CDATASection.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/Comment.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/editing/EditingStrategy.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/MarkupAccumulator.h"
#include "core/editing/serializers/StyledMarkupSerializer.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLQuoteElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/LayoutObject.h"
#include "platform/weborigin/KURL.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

class AttributeChange {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    AttributeChange()
        : m_name(nullAtom, nullAtom, nullAtom)
    {
    }

    AttributeChange(Element* element, const QualifiedName& name, const String& value)
        : m_element(element), m_name(name), m_value(value)
    {
    }

    void apply()
    {
        m_element->setAttribute(m_name, AtomicString(m_value));
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_element);
    }

private:
    Member<Element> m_element;
    QualifiedName m_name;
    String m_value;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::AttributeChange);

namespace blink {

static void completeURLs(DocumentFragment& fragment, const String& baseURL)
{
    HeapVector<AttributeChange> changes;

    KURL parsedBaseURL(ParsedURLString, baseURL);

    for (Element& element : ElementTraversal::descendantsOf(fragment)) {
        AttributeCollection attributes = element.attributes();
        // AttributeCollection::iterator end = attributes.end();
        for (const auto& attribute : attributes) {
            if (element.isURLAttribute(attribute) && !attribute.value().isEmpty())
                changes.append(AttributeChange(&element, attribute.name(), KURL(parsedBaseURL, attribute.value()).getString()));
        }
    }

    for (auto& change : changes)
        change.apply();
}

static bool isHTMLBlockElement(const Node* node)
{
    DCHECK(node);
    return isHTMLTableCellElement(*node)
        || isNonTableCellHTMLBlockElement(node);
}

static HTMLElement* ancestorToRetainStructureAndAppearanceForBlock(Element* commonAncestorBlock)
{
    if (!commonAncestorBlock)
        return 0;

    if (commonAncestorBlock->hasTagName(tbodyTag) || isHTMLTableRowElement(*commonAncestorBlock))
        return Traversal<HTMLTableElement>::firstAncestor(*commonAncestorBlock);

    if (isNonTableCellHTMLBlockElement(commonAncestorBlock))
        return toHTMLElement(commonAncestorBlock);

    return 0;
}

static inline HTMLElement* ancestorToRetainStructureAndAppearance(Node* commonAncestor)
{
    return ancestorToRetainStructureAndAppearanceForBlock(enclosingBlock(commonAncestor));
}

static inline HTMLElement* ancestorToRetainStructureAndAppearanceWithNoLayoutObject(Node* commonAncestor)
{
    HTMLElement* commonAncestorBlock = toHTMLElement(enclosingNodeOfType(firstPositionInOrBeforeNode(commonAncestor), isHTMLBlockElement));
    return ancestorToRetainStructureAndAppearanceForBlock(commonAncestorBlock);
}

bool propertyMissingOrEqualToNone(StylePropertySet* style, CSSPropertyID propertyID)
{
    if (!style)
        return false;
    CSSValue* value = style->getPropertyCSSValue(propertyID);
    if (!value)
        return true;
    if (!value->isPrimitiveValue())
        return false;
    return toCSSPrimitiveValue(value)->getValueID() == CSSValueNone;
}

static bool isPresentationalHTMLElement(const Node* node)
{
    if (!node->isHTMLElement())
        return false;

    const HTMLElement& element = toHTMLElement(*node);
    return element.hasTagName(uTag) || element.hasTagName(sTag) || element.hasTagName(strikeTag)
        || element.hasTagName(iTag) || element.hasTagName(emTag) || element.hasTagName(bTag) || element.hasTagName(strongTag);
}

template<typename Strategy>
static HTMLElement* highestAncestorToWrapMarkup(const PositionTemplate<Strategy>& startPosition, const PositionTemplate<Strategy>& endPosition, EAnnotateForInterchange shouldAnnotate, Node* constrainingAncestor)
{
    Node* firstNode = startPosition.nodeAsRangeFirstNode();
    // For compatibility reason, we use container node of start and end
    // positions rather than first node and last node in selection.
    Node* commonAncestor = Strategy::commonAncestor(*startPosition.computeContainerNode(), *endPosition.computeContainerNode());
    DCHECK(commonAncestor);
    HTMLElement* specialCommonAncestor = nullptr;
    if (shouldAnnotate == AnnotateForInterchange) {
        // Include ancestors that aren't completely inside the range but are required to retain
        // the structure and appearance of the copied markup.
        specialCommonAncestor = ancestorToRetainStructureAndAppearance(commonAncestor);
        if (Node* parentListNode = enclosingNodeOfType(firstPositionInOrBeforeNode(firstNode), isListItem)) {
            EphemeralRangeTemplate<Strategy> markupRange = EphemeralRangeTemplate<Strategy>(startPosition, endPosition);
            EphemeralRangeTemplate<Strategy> nodeRange = normalizeRange(EphemeralRangeTemplate<Strategy>::rangeOfContents(*parentListNode));
            if (nodeRange == markupRange) {
                ContainerNode* ancestor = parentListNode->parentNode();
                while (ancestor && !isHTMLListElement(ancestor))
                    ancestor = ancestor->parentNode();
                specialCommonAncestor = toHTMLElement(ancestor);
            }
        }

        // Retain the Mail quote level by including all ancestor mail block quotes.
        if (HTMLQuoteElement* highestMailBlockquote = toHTMLQuoteElement(highestEnclosingNodeOfType(firstPositionInOrBeforeNode(firstNode), isMailHTMLBlockquoteElement, CanCrossEditingBoundary)))
            specialCommonAncestor = highestMailBlockquote;
    }

    Node* checkAncestor = specialCommonAncestor ? specialCommonAncestor : commonAncestor;
    if (checkAncestor->layoutObject()) {
        HTMLElement* newSpecialCommonAncestor = toHTMLElement(highestEnclosingNodeOfType(Position::firstPositionInNode(checkAncestor), &isPresentationalHTMLElement, CanCrossEditingBoundary, constrainingAncestor));
        if (newSpecialCommonAncestor)
            specialCommonAncestor = newSpecialCommonAncestor;
    }

    // If a single tab is selected, commonAncestor will be a text node inside a tab span.
    // If two or more tabs are selected, commonAncestor will be the tab span.
    // In either case, if there is a specialCommonAncestor already, it will necessarily be above
    // any tab span that needs to be included.
    if (!specialCommonAncestor && isTabHTMLSpanElementTextNode(commonAncestor))
        specialCommonAncestor = toHTMLSpanElement(Strategy::parent(*commonAncestor));
    if (!specialCommonAncestor && isTabHTMLSpanElement(commonAncestor))
        specialCommonAncestor = toHTMLSpanElement(commonAncestor);

    if (HTMLAnchorElement* enclosingAnchor = toHTMLAnchorElement(enclosingElementWithTag(Position::firstPositionInNode(specialCommonAncestor ? specialCommonAncestor : commonAncestor), aTag)))
        specialCommonAncestor = enclosingAnchor;

    return specialCommonAncestor;
}

template <typename Strategy>
class CreateMarkupAlgorithm {
public:
    static String createMarkup(const PositionTemplate<Strategy>& startPosition, const PositionTemplate<Strategy>& endPosition, EAnnotateForInterchange shouldAnnotate = DoNotAnnotateForInterchange, ConvertBlocksToInlines = ConvertBlocksToInlines::NotConvert, EAbsoluteURLs shouldResolveURLs = DoNotResolveURLs, Node* constrainingAncestor = nullptr);
};

// FIXME: Shouldn't we omit style info when annotate == DoNotAnnotateForInterchange?
// FIXME: At least, annotation and style info should probably not be included in range.markupString()
template <typename Strategy>
String CreateMarkupAlgorithm<Strategy>::createMarkup(const PositionTemplate<Strategy>& startPosition, const PositionTemplate<Strategy>& endPosition,
    EAnnotateForInterchange shouldAnnotate, ConvertBlocksToInlines convertBlocksToInlines, EAbsoluteURLs shouldResolveURLs, Node* constrainingAncestor)
{
    if (startPosition.isNull() || endPosition.isNull())
        return emptyString();

    RELEASE_ASSERT(startPosition.compareTo(endPosition) <= 0);

    bool collapsed = startPosition == endPosition;
    if (collapsed)
        return emptyString();
    Node* commonAncestor = Strategy::commonAncestor(*startPosition.computeContainerNode(), *endPosition.computeContainerNode());
    if (!commonAncestor)
        return emptyString();

    Document* document = startPosition.document();
    document->updateStyleAndLayoutIgnorePendingStylesheets();

    HTMLElement* specialCommonAncestor = highestAncestorToWrapMarkup<Strategy>(startPosition, endPosition, shouldAnnotate, constrainingAncestor);
    StyledMarkupSerializer<Strategy> serializer(shouldResolveURLs, shouldAnnotate, startPosition, endPosition, specialCommonAncestor, convertBlocksToInlines);
    return serializer.createMarkup();
}

String createMarkup(const Position& startPosition, const Position& endPosition, EAnnotateForInterchange shouldAnnotate, ConvertBlocksToInlines convertBlocksToInlines, EAbsoluteURLs shouldResolveURLs, Node* constrainingAncestor)
{
    return CreateMarkupAlgorithm<EditingStrategy>::createMarkup(startPosition, endPosition, shouldAnnotate, convertBlocksToInlines, shouldResolveURLs, constrainingAncestor);
}

String createMarkup(const PositionInFlatTree& startPosition, const PositionInFlatTree& endPosition, EAnnotateForInterchange shouldAnnotate, ConvertBlocksToInlines convertBlocksToInlines, EAbsoluteURLs shouldResolveURLs, Node* constrainingAncestor)
{
    return CreateMarkupAlgorithm<EditingInFlatTreeStrategy>::createMarkup(startPosition, endPosition, shouldAnnotate, convertBlocksToInlines, shouldResolveURLs, constrainingAncestor);
}

DocumentFragment* createFragmentFromMarkup(Document& document, const String& markup, const String& baseURL, ParserContentPolicy parserContentPolicy)
{
    // We use a fake body element here to trick the HTML parser to using the InBody insertion mode.
    HTMLBodyElement* fakeBody = HTMLBodyElement::create(document);
    DocumentFragment* fragment = DocumentFragment::create(document);

    fragment->parseHTML(markup, fakeBody, parserContentPolicy);

    if (!baseURL.isEmpty() && baseURL != blankURL() && baseURL != document.baseURL())
        completeURLs(*fragment, baseURL);

    return fragment;
}

static const char fragmentMarkerTag[] = "webkit-fragment-marker";

static bool findNodesSurroundingContext(DocumentFragment* fragment, Comment*& nodeBeforeContext, Comment*& nodeAfterContext)
{
    if (!fragment->firstChild())
        return false;
    for (Node& node : NodeTraversal::startsAt(*fragment->firstChild())) {
        if (node.getNodeType() == Node::COMMENT_NODE && toComment(node).data() == fragmentMarkerTag) {
            if (!nodeBeforeContext) {
                nodeBeforeContext = &toComment(node);
            } else {
                nodeAfterContext = &toComment(node);
                return true;
            }
        }
    }
    return false;
}

static void trimFragment(DocumentFragment* fragment, Comment* nodeBeforeContext, Comment* nodeAfterContext)
{
    Node* next = nullptr;
    for (Node* node = fragment->firstChild(); node; node = next) {
        if (nodeBeforeContext->isDescendantOf(node)) {
            next = NodeTraversal::next(*node);
            continue;
        }
        next = NodeTraversal::nextSkippingChildren(*node);
        DCHECK(!node->contains(nodeAfterContext)) << node << " " << nodeAfterContext;
        node->parentNode()->removeChild(node, ASSERT_NO_EXCEPTION);
        if (nodeBeforeContext == node)
            break;
    }

    DCHECK(nodeAfterContext->parentNode()) << nodeAfterContext;
    for (Node* node = nodeAfterContext; node; node = next) {
        next = NodeTraversal::nextSkippingChildren(*node);
        node->parentNode()->removeChild(node, ASSERT_NO_EXCEPTION);
    }
}

DocumentFragment* createFragmentFromMarkupWithContext(Document& document, const String& markup, unsigned fragmentStart, unsigned fragmentEnd,
    const String& baseURL, ParserContentPolicy parserContentPolicy)
{
    // FIXME: Need to handle the case where the markup already contains these markers.

    StringBuilder taggedMarkup;
    taggedMarkup.append(markup.left(fragmentStart));
    MarkupFormatter::appendComment(taggedMarkup, fragmentMarkerTag);
    taggedMarkup.append(markup.substring(fragmentStart, fragmentEnd - fragmentStart));
    MarkupFormatter::appendComment(taggedMarkup, fragmentMarkerTag);
    taggedMarkup.append(markup.substring(fragmentEnd));

    DocumentFragment* taggedFragment = createFragmentFromMarkup(document, taggedMarkup.toString(), baseURL, parserContentPolicy);

    Comment* nodeBeforeContext = nullptr;
    Comment* nodeAfterContext = nullptr;
    if (!findNodesSurroundingContext(taggedFragment, nodeBeforeContext, nodeAfterContext))
        return nullptr;

    Document* taggedDocument = Document::create();
    taggedDocument->setContextFeatures(document.contextFeatures());

    Element* root = Element::create(QualifiedName::null(), taggedDocument);
    root->appendChild(taggedFragment);
    taggedDocument->appendChild(root);

    Range* range = Range::create(*taggedDocument,
        Position::afterNode(nodeBeforeContext).parentAnchoredEquivalent(),
        Position::beforeNode(nodeAfterContext).parentAnchoredEquivalent());

    Node* commonAncestor = range->commonAncestorContainer();
    HTMLElement* specialCommonAncestor = ancestorToRetainStructureAndAppearanceWithNoLayoutObject(commonAncestor);

    // When there's a special common ancestor outside of the fragment, we must include it as well to
    // preserve the structure and appearance of the fragment. For example, if the fragment contains
    // TD, we need to include the enclosing TABLE tag as well.
    DocumentFragment* fragment = DocumentFragment::create(document);
    if (specialCommonAncestor)
        fragment->appendChild(specialCommonAncestor);
    else
        fragment->parserTakeAllChildrenFrom(toContainerNode(*commonAncestor));

    trimFragment(fragment, nodeBeforeContext, nodeAfterContext);

    return fragment;
}

String createMarkup(const Node* node, EChildrenOnly childrenOnly, EAbsoluteURLs shouldResolveURLs)
{
    if (!node)
        return "";

    MarkupAccumulator accumulator(shouldResolveURLs);
    return serializeNodes<EditingStrategy>(accumulator, const_cast<Node&>(*node), childrenOnly);
}

static void fillContainerFromString(ContainerNode* paragraph, const String& string)
{
    Document& document = paragraph->document();

    if (string.isEmpty()) {
        paragraph->appendChild(HTMLBRElement::create(document));
        return;
    }

    DCHECK_EQ(string.find('\n'), kNotFound) << string;

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
            Text* textNode = document.createTextNode(stringWithRebalancedWhitespace(s, first, i + 1 == numEntries));
            paragraph->appendChild(textNode);
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
    DCHECK(node);
    if (!isHTMLDivElement(*node))
        return false;

    HTMLDivElement& element = toHTMLDivElement(*node);
    if (!element.hasAttributes())
        return false;

    if (element.hasOneChild())
        return element.firstChild()->isTextNode() || element.firstChild()->hasChildren();

    return element.hasChildCount(2) && isTabHTMLSpanElementTextNode(element.firstChild()->firstChild()) && element.lastChild()->isTextNode();
}

static bool shouldPreserveNewline(const EphemeralRange& range)
{
    if (Node* node = range.startPosition().nodeAsRangeFirstNode()) {
        if (LayoutObject* layoutObject = node->layoutObject())
            return layoutObject->style()->preserveNewline();
    }

    if (Node* node = range.startPosition().anchorNode()) {
        if (LayoutObject* layoutObject = node->layoutObject())
            return layoutObject->style()->preserveNewline();
    }

    return false;
}

DocumentFragment* createFragmentFromText(const EphemeralRange& context, const String& text)
{
    if (context.isNull())
        return nullptr;

    Document& document = context.document();
    DocumentFragment* fragment = document.createDocumentFragment();

    if (text.isEmpty())
        return fragment;

    String string = text;
    string.replace("\r\n", "\n");
    string.replace('\r', '\n');

    if (!isRichlyEditablePosition(context.startPosition()) || shouldPreserveNewline(context)) {
        fragment->appendChild(document.createTextNode(string));
        if (string.endsWith('\n')) {
            HTMLBRElement* element = HTMLBRElement::create(document);
            element->setAttribute(classAttr, AppleInterchangeNewline);
            fragment->appendChild(element);
        }
        return fragment;
    }

    // A string with no newlines gets added inline, rather than being put into a paragraph.
    if (string.find('\n') == kNotFound) {
        fillContainerFromString(fragment, string);
        return fragment;
    }

    // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
    Element* block = enclosingBlock(context.startPosition().nodeAsRangeFirstNode());
    bool useClonesOfEnclosingBlock = block
        && !isHTMLBodyElement(*block)
        && !isHTMLHtmlElement(*block)
        && block != rootEditableElementOf(context.startPosition());

    Vector<String> list;
    string.split('\n', true, list); // true gets us empty strings in the list
    size_t numLines = list.size();
    for (size_t i = 0; i < numLines; ++i) {
        const String& s = list[i];

        Element* element = nullptr;
        if (s.isEmpty() && i + 1 == numLines) {
            // For last line, use the "magic BR" rather than a P.
            element = HTMLBRElement::create(document);
            element->setAttribute(classAttr, AppleInterchangeNewline);
        } else {
            if (useClonesOfEnclosingBlock)
                element = block->cloneElementWithoutChildren();
            else
                element = createDefaultParagraphElement(document);
            fillContainerFromString(element, s);
        }
        fragment->appendChild(element);
    }
    return fragment;
}

DocumentFragment* createFragmentForInnerOuterHTML(const String& markup, Element* contextElement, ParserContentPolicy parserContentPolicy, const char* method, ExceptionState& exceptionState)
{
    DCHECK(contextElement);
    Document& document = isHTMLTemplateElement(*contextElement) ? contextElement->document().ensureTemplateDocument() : contextElement->document();
    DocumentFragment* fragment = DocumentFragment::create(document);

    if (document.isHTMLDocument()) {
        fragment->parseHTML(markup, contextElement, parserContentPolicy);
        return fragment;
    }

    bool wasValid = fragment->parseXML(markup, contextElement, parserContentPolicy);
    if (!wasValid) {
        exceptionState.throwDOMException(SyntaxError, "The provided markup is invalid XML, and therefore cannot be inserted into an XML document.");
        return nullptr;
    }
    return fragment;
}

DocumentFragment* createFragmentForTransformToFragment(const String& sourceString, const String& sourceMIMEType, Document& outputDoc)
{
    DocumentFragment* fragment = outputDoc.createDocumentFragment();

    if (sourceMIMEType == "text/html") {
        // As far as I can tell, there isn't a spec for how transformToFragment is supposed to work.
        // Based on the documentation I can find, it looks like we want to start parsing the fragment in the InBody insertion mode.
        // Unfortunately, that's an implementation detail of the parser.
        // We achieve that effect here by passing in a fake body element as context for the fragment.
        HTMLBodyElement* fakeBody = HTMLBodyElement::create(outputDoc);
        fragment->parseHTML(sourceString, fakeBody);
    } else if (sourceMIMEType == "text/plain") {
        fragment->parserAppendChild(Text::create(outputDoc, sourceString));
    } else {
        bool successfulParse = fragment->parseXML(sourceString, 0);
        if (!successfulParse)
            return nullptr;
    }

    // FIXME: Do we need to mess with URLs here?

    return fragment;
}

static inline void removeElementPreservingChildren(DocumentFragment* fragment, HTMLElement* element)
{
    Node* nextChild = nullptr;
    for (Node* child = element->firstChild(); child; child = nextChild) {
        nextChild = child->nextSibling();
        element->removeChild(child);
        fragment->insertBefore(child, element);
    }
    fragment->removeChild(element);
}

static inline bool isSupportedContainer(Element* element)
{
    DCHECK(element);
    if (!element->isHTMLElement())
        return true;

    HTMLElement& htmlElement = toHTMLElement(*element);
    if (htmlElement.hasTagName(colTag) || htmlElement.hasTagName(colgroupTag) || htmlElement.hasTagName(framesetTag)
        || htmlElement.hasTagName(headTag) || htmlElement.hasTagName(styleTag) || htmlElement.hasTagName(titleTag)) {
        return false;
    }
    return !htmlElement.ieForbidsInsertHTML();
}

DocumentFragment* createContextualFragment(const String& markup, Element* element, ParserContentPolicy parserContentPolicy, ExceptionState& exceptionState)
{
    DCHECK(element);
    if (!isSupportedContainer(element)) {
        exceptionState.throwDOMException(NotSupportedError, "The range's container is '" + element->localName() + "', which is not supported.");
        return nullptr;
    }

    DocumentFragment* fragment = createFragmentForInnerOuterHTML(markup, element, parserContentPolicy, "createContextualFragment", exceptionState);
    if (!fragment)
        return nullptr;

    // We need to pop <html> and <body> elements and remove <head> to
    // accommodate folks passing complete HTML documents to make the
    // child of an element.

    Node* nextNode = nullptr;
    for (Node* node = fragment->firstChild(); node; node = nextNode) {
        nextNode = node->nextSibling();
        if (isHTMLHtmlElement(*node) || isHTMLHeadElement(*node) || isHTMLBodyElement(*node)) {
            HTMLElement* element = toHTMLElement(node);
            if (Node* firstChild = element->firstChild())
                nextNode = firstChild;
            removeElementPreservingChildren(fragment, element);
        }
    }
    return fragment;
}

void replaceChildrenWithFragment(ContainerNode* container, DocumentFragment* fragment, ExceptionState& exceptionState)
{
    DCHECK(container);
    ContainerNode* containerNode(container);

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
    DCHECK(container);
    ContainerNode* containerNode(container);

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
    Text* textNode = Text::create(containerNode->document(), text);

    // FIXME: No need to replace the child it is a text node and its contents are already == text.
    if (containerNode->hasOneChild()) {
        containerNode->replaceChild(textNode, containerNode->firstChild(), exceptionState);
        return;
    }

    containerNode->removeChildren();
    containerNode->appendChild(textNode, exceptionState);
}

void mergeWithNextTextNode(Text* textNode, ExceptionState& exceptionState)
{
    DCHECK(textNode);
    Node* next = textNode->nextSibling();
    if (!next || !next->isTextNode())
        return;

    Text* textNext = toText(next);
    textNode->appendData(textNext->data());
    if (textNext->parentNode()) // Might have been removed by mutation event.
        textNext->remove(exceptionState);
}

template class CORE_TEMPLATE_EXPORT CreateMarkupAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT CreateMarkupAlgorithm<EditingInFlatTreeStrategy>;

} // namespace blink
