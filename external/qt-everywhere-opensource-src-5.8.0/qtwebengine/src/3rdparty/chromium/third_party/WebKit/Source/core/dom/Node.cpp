/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/Node.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "core/HTMLNames.h"
#include "core/css/CSSSelector.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Attr.h"
#include "core/dom/Attribute.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ChildNodeList.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/Range.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TemplateContentDocumentFragment.h"
#include "core/dom/Text.h"
#include "core/dom/TreeScopeAdopter.h"
#include "core/dom/UserActionElementSet.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/dom/shadow/SlotAssignment.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/Event.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/EventDispatcher.h"
#include "core/events/EventListener.h"
#include "core/events/GestureEvent.h"
#include "core/events/InputEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/MutationEvent.h"
#include "core/events/PointerEvent.h"
#include "core/events/TextEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/UIEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InstanceCounters.h"
#include "core/layout/LayoutBox.h"
#include "core/page/ContextMenuController.h"
#include "core/page/Page.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

struct SameSizeAsNode : EventTarget {
    uint32_t m_nodeFlags;
    Member<void*> m_willbeMember[4];
    void* m_pointer;
};

static_assert(sizeof(Node) <= sizeof(SameSizeAsNode), "Node should stay small");

#if DUMP_NODE_STATISTICS
using WeakNodeSet = HeapHashSet<WeakMember<Node>>;
static WeakNodeSet& liveNodeSet()
{
    DEFINE_STATIC_LOCAL(WeakNodeSet, set, (new WeakNodeSet));
    return set;
}
#endif

void Node::dumpStatistics()
{
#if DUMP_NODE_STATISTICS
    size_t nodesWithRareData = 0;

    size_t elementNodes = 0;
    size_t attrNodes = 0;
    size_t textNodes = 0;
    size_t cdataNodes = 0;
    size_t commentNodes = 0;
    size_t piNodes = 0;
    size_t documentNodes = 0;
    size_t docTypeNodes = 0;
    size_t fragmentNodes = 0;
    size_t shadowRootNodes = 0;

    HashMap<String, size_t> perTagCount;

    size_t attributes = 0;
    size_t elementsWithAttributeStorage = 0;
    size_t elementsWithRareData = 0;
    size_t elementsWithNamedNodeMap = 0;

    {
        ScriptForbiddenScope forbidScriptDuringRawIteration;
        for (Node* node : liveNodeSet()) {
            if (node->hasRareData()) {
                ++nodesWithRareData;
                if (node->isElementNode()) {
                    ++elementsWithRareData;
                    if (toElement(node)->hasNamedNodeMap())
                        ++elementsWithNamedNodeMap;
                }
            }

            switch (node->getNodeType()) {
            case ELEMENT_NODE: {
                ++elementNodes;

                // Tag stats
                Element* element = toElement(node);
                HashMap<String, size_t>::AddResult result = perTagCount.add(element->tagName(), 1);
                if (!result.isNewEntry)
                    result.storedValue->value++;

                size_t attributeCount = element->attributesWithoutUpdate().size();
                if (attributeCount) {
                    attributes += attributeCount;
                    ++elementsWithAttributeStorage;
                }
                break;
            }
            case ATTRIBUTE_NODE: {
                ++attrNodes;
                break;
            }
            case TEXT_NODE: {
                ++textNodes;
                break;
            }
            case CDATA_SECTION_NODE: {
                ++cdataNodes;
                break;
            }
            case COMMENT_NODE: {
                ++commentNodes;
                break;
            }
            case PROCESSING_INSTRUCTION_NODE: {
                ++piNodes;
                break;
            }
            case DOCUMENT_NODE: {
                ++documentNodes;
                break;
            }
            case DOCUMENT_TYPE_NODE: {
                ++docTypeNodes;
                break;
            }
            case DOCUMENT_FRAGMENT_NODE: {
                if (node->isShadowRoot())
                    ++shadowRootNodes;
                else
                    ++fragmentNodes;
                break;
            }
            }
        }
    }

    printf("Number of Nodes: %d\n\n", liveNodeSet().size());
    printf("Number of Nodes with RareData: %zu\n\n", nodesWithRareData);

    printf("NodeType distribution:\n");
    printf("  Number of Element nodes: %zu\n", elementNodes);
    printf("  Number of Attribute nodes: %zu\n", attrNodes);
    printf("  Number of Text nodes: %zu\n", textNodes);
    printf("  Number of CDATASection nodes: %zu\n", cdataNodes);
    printf("  Number of Comment nodes: %zu\n", commentNodes);
    printf("  Number of ProcessingInstruction nodes: %zu\n", piNodes);
    printf("  Number of Document nodes: %zu\n", documentNodes);
    printf("  Number of DocumentType nodes: %zu\n", docTypeNodes);
    printf("  Number of DocumentFragment nodes: %zu\n", fragmentNodes);
    printf("  Number of ShadowRoot nodes: %zu\n", shadowRootNodes);

    printf("Element tag name distibution:\n");
    for (const auto& entry : perTagCount)
        printf("  Number of <%s> tags: %zu\n", entry.key.utf8().data(), entry.value);

    printf("Attributes:\n");
    printf("  Number of Attributes (non-Node and Node): %zu [%zu]\n", attributes, sizeof(Attribute));
    printf("  Number of Elements with attribute storage: %zu [%zu]\n", elementsWithAttributeStorage, sizeof(ElementData));
    printf("  Number of Elements with RareData: %zu\n", elementsWithRareData);
    printf("  Number of Elements with NamedNodeMap: %zu [%zu]\n", elementsWithNamedNodeMap, sizeof(NamedNodeMap));
#endif
}

void Node::trackForDebugging()
{
#if DUMP_NODE_STATISTICS
    liveNodeSet().add(this);
#endif
}

Node::Node(TreeScope* treeScope, ConstructionType type)
    : m_nodeFlags(type)
    , m_parentOrShadowHostNode(nullptr)
    , m_treeScope(treeScope)
    , m_previous(nullptr)
    , m_next(nullptr)
{
    DCHECK(m_treeScope || type == CreateDocument || type == CreateShadowRoot);
#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
    trackForDebugging();
#endif
    InstanceCounters::incrementCounter(InstanceCounters::NodeCounter);
}

Node::~Node()
{
    // With Oilpan, the rare data finalizer also asserts for
    // this condition (we cannot directly access it here.)
    RELEASE_ASSERT(hasRareData() || !layoutObject());
    InstanceCounters::decrementCounter(InstanceCounters::NodeCounter);
}

NodeRareData* Node::rareData() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(hasRareData());
    return static_cast<NodeRareData*>(m_data.m_rareData);
}

NodeRareData& Node::ensureRareData()
{
    if (hasRareData())
        return *rareData();

    if (isElementNode())
        m_data.m_rareData = ElementRareData::create(m_data.m_layoutObject);
    else
        m_data.m_rareData = NodeRareData::create(m_data.m_layoutObject);

    DCHECK(m_data.m_rareData);

    setFlag(HasRareDataFlag);
    return *rareData();
}

Node* Node::toNode()
{
    return this;
}

short Node::tabIndex() const
{
    return 0;
}

String Node::nodeValue() const
{
    return String();
}

void Node::setNodeValue(const String&)
{
    // By default, setting nodeValue has no effect.
}

NodeList* Node::childNodes()
{
    if (isContainerNode())
        return ensureRareData().ensureNodeLists().ensureChildNodeList(toContainerNode(*this));
    return ensureRareData().ensureNodeLists().ensureEmptyChildNodeList(*this);
}

Node* Node::pseudoAwarePreviousSibling() const
{
    if (parentElement() && !previousSibling()) {
        Element* parent = parentElement();
        if (isAfterPseudoElement() && parent->lastChild())
            return parent->lastChild();
        if (!isBeforePseudoElement())
            return parent->pseudoElement(PseudoIdBefore);
    }
    return previousSibling();
}

Node* Node::pseudoAwareNextSibling() const
{
    if (parentElement() && !nextSibling()) {
        Element* parent = parentElement();
        if (isBeforePseudoElement() && parent->hasChildren())
            return parent->firstChild();
        if (!isAfterPseudoElement())
            return parent->pseudoElement(PseudoIdAfter);
    }
    return nextSibling();
}

Node* Node::pseudoAwareFirstChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* first = currentElement->pseudoElement(PseudoIdBefore);
        if (first)
            return first;
        first = currentElement->firstChild();
        if (!first)
            first = currentElement->pseudoElement(PseudoIdAfter);
        return first;
    }

    return firstChild();
}

Node* Node::pseudoAwareLastChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* last = currentElement->pseudoElement(PseudoIdAfter);
        if (last)
            return last;
        last = currentElement->lastChild();
        if (!last)
            last = currentElement->pseudoElement(PseudoIdBefore);
        return last;
    }

    return lastChild();
}

Node& Node::treeRoot() const
{
    if (isInTreeScope())
        return containingTreeScope().rootNode();
    const Node* node = this;
    while (node->parentNode())
        node = node->parentNode();
    return const_cast<Node&>(*node);
}

Node* Node::insertBefore(Node* newChild, Node* refChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        return toContainerNode(this)->insertBefore(newChild, refChild, exceptionState);

    exceptionState.throwDOMException(HierarchyRequestError, "This node type does not support this method.");
    return nullptr;
}

Node* Node::replaceChild(Node* newChild, Node* oldChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        return toContainerNode(this)->replaceChild(newChild, oldChild, exceptionState);

    exceptionState.throwDOMException(HierarchyRequestError,  "This node type does not support this method.");
    return nullptr;
}

Node* Node::removeChild(Node* oldChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        return toContainerNode(this)->removeChild(oldChild, exceptionState);

    exceptionState.throwDOMException(NotFoundError, "This node type does not support this method.");
    return nullptr;
}

Node* Node::appendChild(Node* newChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        return toContainerNode(this)->appendChild(newChild, exceptionState);

    exceptionState.throwDOMException(HierarchyRequestError, "This node type does not support this method.");
    return nullptr;
}

static bool isNodeInNodes(const Node* const node, const HeapVector<NodeOrString>& nodes)
{
    for (const NodeOrString& nodeOrString : nodes) {
        if (nodeOrString.isNode() && nodeOrString.getAsNode() == node)
            return true;
    }
    return false;
}

static Node* findViablePreviousSibling(const Node& node, const HeapVector<NodeOrString>& nodes)
{
    for (Node* sibling = node.previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (!isNodeInNodes(sibling, nodes))
            return sibling;
    }
    return nullptr;
}

static Node* findViableNextSibling(const Node& node, const HeapVector<NodeOrString>& nodes)
{
    for (Node* sibling = node.nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (!isNodeInNodes(sibling, nodes))
            return sibling;
    }
    return nullptr;
}

static Node* nodeOrStringToNode(const NodeOrString& nodeOrString, Document& document)
{
    if (nodeOrString.isNode())
        return nodeOrString.getAsNode();
    return Text::create(document, nodeOrString.getAsString());
}

static Node* convertNodesIntoNode(const HeapVector<NodeOrString>& nodes, Document& document)
{
    if (nodes.size() == 1)
        return nodeOrStringToNode(nodes[0], document);

    Node* fragment = DocumentFragment::create(document);
    for (const NodeOrString& nodeOrString : nodes)
        fragment->appendChild(nodeOrStringToNode(nodeOrString, document), ASSERT_NO_EXCEPTION);
    return fragment;
}

void Node::prepend(const HeapVector<NodeOrString>& nodes, ExceptionState& exceptionState)
{
    Node* node = convertNodesIntoNode(nodes, document());
    insertBefore(node, firstChild(), exceptionState);
}

void Node::append(const HeapVector<NodeOrString>& nodes, ExceptionState& exceptionState)
{
    Node* node = convertNodesIntoNode(nodes, document());
    appendChild(node, exceptionState);
}

void Node::before(const HeapVector<NodeOrString>& nodes, ExceptionState& exceptionState)
{
    Node* parent = parentNode();
    if (!parent)
        return;
    Node* viablePreviousSibling = findViablePreviousSibling(*this, nodes);
    Node* node = convertNodesIntoNode(nodes, document());
    parent->insertBefore(node, viablePreviousSibling ? viablePreviousSibling->nextSibling() : parent->firstChild(), exceptionState);
}

void Node::after(const HeapVector<NodeOrString>& nodes, ExceptionState& exceptionState)
{
    Node* parent = parentNode();
    if (!parent)
        return;
    Node* viableNextSibling = findViableNextSibling(*this, nodes);
    Node* node = convertNodesIntoNode(nodes, document());
    parent->insertBefore(node, viableNextSibling, exceptionState);
}

void Node::replaceWith(const HeapVector<NodeOrString>& nodes, ExceptionState& exceptionState)
{
    Node* parent = parentNode();
    if (!parent)
        return;
    Node* viableNextSibling = findViableNextSibling(*this, nodes);
    Node* node = convertNodesIntoNode(nodes, document());
    if (parent == parentNode())
        parent->replaceChild(node, this, exceptionState);
    else
        parent->insertBefore(node, viableNextSibling, exceptionState);
}

void Node::remove(ExceptionState& exceptionState)
{
    if (ContainerNode* parent = parentNode())
        parent->removeChild(this, exceptionState);
}

void Node::normalize()
{
    updateDistribution();

    // Go through the subtree beneath us, normalizing all nodes. This means that
    // any two adjacent text nodes are merged and any empty text nodes are removed.

    Node* node = this;
    while (Node* firstChild = node->firstChild())
        node = firstChild;
    while (node) {
        if (node == this)
            break;

        if (node->getNodeType() == TEXT_NODE)
            node = toText(node)->mergeNextSiblingNodesIfPossible();
        else
            node = NodeTraversal::nextPostOrder(*node);
    }
}

bool Node::isContentEditable(UserSelectAllTreatment treatment) const
{
    document().updateStyleAndLayoutTree();
    return hasEditableStyle(Editable, treatment);
}

bool Node::isContentRichlyEditable() const
{
    document().updateStyleAndLayoutTree();
    return hasEditableStyle(RichlyEditable, UserSelectAllIsAlwaysNonEditable);
}

bool Node::hasEditableStyle(EditableLevel editableLevel, UserSelectAllTreatment treatment) const
{
    if (isPseudoElement())
        return false;

    // Ideally we'd call DCHECK(!needsStyleRecalc()) here, but
    // ContainerNode::setFocus() calls setNeedsStyleRecalc(), so the assertion
    // would fire in the middle of Document::setFocusedNode().

    for (const Node& node : NodeTraversal::inclusiveAncestorsOf(*this)) {
        if ((node.isHTMLElement() || node.isDocumentNode()) && node.layoutObject()) {
            switch (node.layoutObject()->style()->userModify()) {
            case READ_ONLY:
                return false;
            case READ_WRITE:
                return true;
            case READ_WRITE_PLAINTEXT_ONLY:
                return editableLevel != RichlyEditable;
            }
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    return false;
}

bool Node::isEditableToAccessibility(EditableLevel editableLevel) const
{
    if (hasEditableStyle(editableLevel))
        return true;

    // FIXME: Respect editableLevel for ARIA editable elements.
    if (editableLevel == RichlyEditable)
        return false;

    // FIXME(dmazzoni): support ScopedAXObjectCache (crbug/489851).
    if (AXObjectCache* cache = document().existingAXObjectCache())
        return cache->rootAXEditableElement(this);

    return false;
}

LayoutBox* Node::layoutBox() const
{
    LayoutObject* layoutObject = this->layoutObject();
    return layoutObject && layoutObject->isBox() ? toLayoutBox(layoutObject) : nullptr;
}

LayoutBoxModelObject* Node::layoutBoxModelObject() const
{
    LayoutObject* layoutObject = this->layoutObject();
    return layoutObject && layoutObject->isBoxModelObject() ? toLayoutBoxModelObject(layoutObject) : nullptr;
}

LayoutRect Node::boundingBox() const
{
    if (layoutObject())
        return LayoutRect(layoutObject()->absoluteBoundingBoxRect());
    return LayoutRect();
}

#ifndef NDEBUG
inline static ShadowRoot* oldestShadowRootFor(const Node* node)
{
    if (!node->isElementNode())
        return nullptr;
    if (ElementShadow* shadow = toElement(node)->shadow())
        return shadow->oldestShadowRoot();
    return nullptr;
}
#endif

Node& Node::shadowIncludingRoot() const
{
    if (inShadowIncludingDocument())
        return document();
    Node* root = const_cast<Node*>(this);
    while (Node* host = root->shadowHost())
        root = host;
    while (Node* ancestor = root->parentNode())
        root = ancestor;
    DCHECK(!root->shadowHost());
    return *root;
}

bool Node::isUnclosedNodeOf(const Node& other) const
{
    if (!isInShadowTree() || treeScope() == other.treeScope())
        return true;

    const TreeScope* scope = &treeScope();
    for (; scope->parentTreeScope(); scope = scope->parentTreeScope()) {
        const ContainerNode& root = scope->rootNode();
        if (root.isShadowRoot() && !toShadowRoot(root).isOpenOrV0())
            break;
    }

    for (TreeScope* otherScope = &other.treeScope(); otherScope; otherScope = otherScope->parentTreeScope()) {
        if (otherScope == scope)
            return true;
    }
    return false;
}

bool Node::needsDistributionRecalc() const
{
    return shadowIncludingRoot().childNeedsDistributionRecalc();
}

void Node::updateDistribution()
{
    // Extra early out to avoid spamming traces.
    if (inShadowIncludingDocument() && !document().childNeedsDistributionRecalc())
        return;
    TRACE_EVENT0("blink", "Node::updateDistribution");
    ScriptForbiddenScope forbidScript;
    Node& root = shadowIncludingRoot();
    if (root.childNeedsDistributionRecalc())
        root.recalcDistribution();
}

void Node::recalcDistribution()
{
    DCHECK(childNeedsDistributionRecalc());

    if (isElementNode()) {
        if (ElementShadow* shadow = toElement(this)->shadow())
            shadow->distributeIfNeeded();
    }

    DCHECK(ScriptForbiddenScope::isScriptForbidden());
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->childNeedsDistributionRecalc())
            child->recalcDistribution();
    }

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->childNeedsDistributionRecalc())
            root->recalcDistribution();
    }

    clearChildNeedsDistributionRecalc();
}

void Node::setIsLink(bool isLink)
{
    setFlag(isLink && !SVGImage::isInSVGImage(toElement(this)), IsLinkFlag);
}

void Node::setNeedsStyleInvalidation()
{
    DCHECK(isElementNode() || isShadowRoot());
    setFlag(NeedsStyleInvalidationFlag);
    markAncestorsWithChildNeedsStyleInvalidation();
}

void Node::markAncestorsWithChildNeedsStyleInvalidation()
{
    ScriptForbiddenScope forbidScriptDuringRawIteration;
    for (Node* node = parentOrShadowHostNode(); node && !node->childNeedsStyleInvalidation(); node = node->parentOrShadowHostNode())
        node->setChildNeedsStyleInvalidation();
    document().scheduleLayoutTreeUpdateIfNeeded();
}

void Node::markAncestorsWithChildNeedsDistributionRecalc()
{
    ScriptForbiddenScope forbidScriptDuringRawIteration;
    for (Node* node = this; node && !node->childNeedsDistributionRecalc(); node = node->parentOrShadowHostNode())
        node->setChildNeedsDistributionRecalc();
    document().scheduleLayoutTreeUpdateIfNeeded();
}

inline void Node::setStyleChange(StyleChangeType changeType)
{
    m_nodeFlags = (m_nodeFlags & ~StyleChangeMask) | changeType;
}

void Node::markAncestorsWithChildNeedsStyleRecalc()
{
    for (ContainerNode* p = parentOrShadowHostNode(); p && !p->childNeedsStyleRecalc(); p = p->parentOrShadowHostNode())
        p->setChildNeedsStyleRecalc();
    document().scheduleLayoutTreeUpdateIfNeeded();
}

void Node::setNeedsStyleRecalc(StyleChangeType changeType, const StyleChangeReasonForTracing& reason)
{
    DCHECK(changeType != NoStyleChange);
    if (!inActiveDocument())
        return;

    TRACE_EVENT_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
        "StyleRecalcInvalidationTracking",
        TRACE_EVENT_SCOPE_THREAD,
        "data",
        InspectorStyleRecalcInvalidationTrackingEvent::data(this, reason));

    StyleChangeType existingChangeType = getStyleChangeType();
    if (changeType > existingChangeType)
        setStyleChange(changeType);

    if (existingChangeType == NoStyleChange)
        markAncestorsWithChildNeedsStyleRecalc();

    if (isElementNode() && hasRareData())
        toElement(*this).setAnimationStyleChange(false);
}

void Node::clearNeedsStyleRecalc()
{
    m_nodeFlags &= ~StyleChangeMask;

    clearSVGFilterNeedsLayerUpdate();

    if (isElementNode() && hasRareData())
        toElement(*this).setAnimationStyleChange(false);
}

bool Node::inActiveDocument() const
{
    return inShadowIncludingDocument() && document().isActive();
}

Node* Node::focusDelegate()
{
    return this;
}

bool Node::shouldHaveFocusAppearance() const
{
    DCHECK(focused());
    return true;
}

bool Node::isInert() const
{
    const HTMLDialogElement* dialog = document().activeModalDialog();
    if (dialog && this != document() && (!canParticipateInFlatTree() || !FlatTreeTraversal::containsIncludingPseudoElement(*dialog, *this)))
        return true;
    return document().localOwner() && document().localOwner()->isInert();
}

unsigned Node::nodeIndex() const
{
    const Node* tempNode = previousSibling();
    unsigned count = 0;
    for (count = 0; tempNode; count++)
        tempNode = tempNode->previousSibling();
    return count;
}

NodeListsNodeData* Node::nodeLists()
{
    return hasRareData() ? rareData()->nodeLists() : nullptr;
}

void Node::clearNodeLists()
{
    rareData()->clearNodeLists();
}

bool Node::isDescendantOf(const Node *other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other || !other->hasChildren() || inShadowIncludingDocument() != other->inShadowIncludingDocument())
        return false;
    if (other->treeScope() != treeScope())
        return false;
    if (other->isTreeScope())
        return !isTreeScope();
    for (const ContainerNode* n = parentNode(); n; n = n->parentNode()) {
        if (n == other)
            return true;
    }
    return false;
}

bool Node::contains(const Node* node) const
{
    if (!node)
        return false;
    return this == node || node->isDescendantOf(this);
}

bool Node::isShadowIncludingInclusiveAncestorOf(const Node* node) const
{
    if (!node)
        return false;

    if (this == node)
        return true;

    if (document() != node->document())
        return false;

    if (inShadowIncludingDocument() != node->inShadowIncludingDocument())
        return false;

    bool hasChildren = isContainerNode() && toContainerNode(this)->hasChildren();
    bool hasShadow = isElementNode() && toElement(this)->shadow();
    if (!hasChildren && !hasShadow)
        return false;

    for (; node; node = node->shadowHost()) {
        if (treeScope() == node->treeScope())
            return contains(node);
    }

    return false;
}

bool Node::containsIncludingHostElements(const Node& node) const
{
    const Node* current = &node;
    do {
        if (current == this)
            return true;
        if (current->isDocumentFragment() && toDocumentFragment(current)->isTemplateContent())
            current = static_cast<const TemplateContentDocumentFragment*>(current)->host();
        else
            current = current->parentOrShadowHostNode();
    } while (current);
    return false;
}

Node* Node::commonAncestor(const Node& other, ContainerNode* (*parent)(const Node&)) const
{
    if (this == other)
        return const_cast<Node*>(this);
    if (document() != other.document())
        return nullptr;
    int thisDepth = 0;
    for (const Node* node = this; node; node = parent(*node)) {
        if (node == &other)
            return const_cast<Node*>(node);
        thisDepth++;
    }
    int otherDepth = 0;
    for (const Node* node = &other; node; node = parent(*node)) {
        if (node == this)
            return const_cast<Node*>(this);
        otherDepth++;
    }
    const Node* thisIterator = this;
    const Node* otherIterator = &other;
    if (thisDepth > otherDepth) {
        for (int i = thisDepth; i > otherDepth; --i)
            thisIterator = parent(*thisIterator);
    } else if (otherDepth > thisDepth) {
        for (int i = otherDepth; i > thisDepth; --i)
            otherIterator = parent(*otherIterator);
    }
    while (thisIterator) {
        if (thisIterator == otherIterator)
            return const_cast<Node*>(thisIterator);
        thisIterator = parent(*thisIterator);
        otherIterator = parent(*otherIterator);
    }
    DCHECK(!otherIterator);
    return nullptr;
}

void Node::reattach(const AttachContext& context)
{
    AttachContext reattachContext(context);
    reattachContext.performingReattach = true;

    // We only need to detach if the node has already been through attach().
    if (getStyleChangeType() < NeedsReattachStyleChange)
        detach(reattachContext);
    attach(reattachContext);
}

void Node::attach(const AttachContext&)
{
    DCHECK(document().inStyleRecalc() || isDocumentNode());
    DCHECK(!document().lifecycle().inDetach());
    DCHECK(needsAttach());
    DCHECK(!layoutObject() || (layoutObject()->style() && (layoutObject()->parent() || layoutObject()->isLayoutView())));

    clearNeedsStyleRecalc();

    if (AXObjectCache* cache = document().axObjectCache())
        cache->updateCacheAfterNodeIsAttached(this);
}

void Node::detach(const AttachContext& context)
{
    DCHECK(document().lifecycle().stateAllowsDetach());
    DocumentLifecycle::DetachScope willDetach(document().lifecycle());

    if (layoutObject())
        layoutObject()->destroyAndCleanupAnonymousWrappers();
    setLayoutObject(nullptr);
    setStyleChange(NeedsReattachStyleChange);
    clearChildNeedsStyleInvalidation();
}

void Node::reattachWhitespaceSiblingsIfNeeded(Text* start)
{
    ScriptForbiddenScope forbidScriptDuringRawIteration;
    for (Node* sibling = start; sibling; sibling = sibling->nextSibling()) {
        if (sibling->isTextNode() && toText(sibling)->containsOnlyWhitespace()) {
            bool hadLayoutObject = !!sibling->layoutObject();
            toText(sibling)->reattachIfNeeded();
            // If sibling's layout object status didn't change we don't need to continue checking
            // other siblings since their layout object status won't change either.
            if (!!sibling->layoutObject() == hadLayoutObject)
                return;
        } else if (sibling->layoutObject()) {
            return;
        }
    }
}

const ComputedStyle* Node::virtualEnsureComputedStyle(PseudoId pseudoElementSpecifier)
{
    return parentOrShadowHostNode() ? parentOrShadowHostNode()->ensureComputedStyle(pseudoElementSpecifier) : nullptr;
}

int Node::maxCharacterOffset() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks questions about HTML in the core DOM class
// is obviously misplaced.
bool Node::canStartSelection() const
{
    if (hasEditableStyle())
        return true;

    if (layoutObject()) {
        const ComputedStyle& style = layoutObject()->styleRef();
        // We allow selections to begin within an element that has -webkit-user-select: none set,
        // but if the element is draggable then dragging should take priority over selection.
        if (style.userDrag() == DRAG_ELEMENT && style.userSelect() == SELECT_NONE)
            return false;
    }
    ContainerNode* parent = FlatTreeTraversal::parent(*this);
    return parent ? parent->canStartSelection() : true;
}

bool Node::canParticipateInFlatTree() const
{
    return !isShadowRoot() && !isSlotOrActiveInsertionPoint();
}

bool Node::isSlotOrActiveInsertionPoint() const
{
    return isHTMLSlotElement(*this) || isActiveInsertionPoint(*this);
}

AtomicString Node::slotName() const
{
    DCHECK(isSlotable());
    if (isElementNode())
        return HTMLSlotElement::normalizeSlotName(toElement(*this).fastGetAttribute(HTMLNames::slotAttr));
    DCHECK(isTextNode());
    return emptyAtom;
}

bool Node::isInV1ShadowTree() const
{
    ShadowRoot* shadowRoot = containingShadowRoot();
    return shadowRoot && shadowRoot->isV1();
}

bool Node::isInV0ShadowTree() const
{
    ShadowRoot* shadowRoot = containingShadowRoot();
    return shadowRoot && !shadowRoot->isV1();
}

ElementShadow* Node::parentElementShadow() const
{
    Element* parent = parentElement();
    return parent ? parent->shadow() : nullptr;
}

bool Node::isChildOfV1ShadowHost() const
{
    ElementShadow* parentShadow = parentElementShadow();
    return parentShadow && parentShadow->isV1();
}

bool Node::isChildOfV0ShadowHost() const
{
    ElementShadow* parentShadow = parentElementShadow();
    return parentShadow && !parentShadow->isV1();
}

ShadowRoot* Node::v1ShadowRootOfParent() const
{
    if (Element* parent = parentElement())
        return parent->shadowRootIfV1();
    return nullptr;
}

Element* Node::shadowHost() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return &root->host();
    return nullptr;
}

ShadowRoot* Node::containingShadowRoot() const
{
    Node& root = treeScope().rootNode();
    return root.isShadowRoot() ? toShadowRoot(&root) : nullptr;
}

Node* Node::nonBoundaryShadowTreeRootNode()
{
    DCHECK(!isShadowRoot());
    Node* root = this;
    while (root) {
        if (root->isShadowRoot())
            return root;
        Node* parent = root->parentOrShadowHostNode();
        if (parent && parent->isShadowRoot())
            return root;
        root = parent;
    }
    return nullptr;
}

ContainerNode* Node::nonShadowBoundaryParentNode() const
{
    ContainerNode* parent = parentNode();
    return parent && !parent->isShadowRoot() ? parent : nullptr;
}

Element* Node::parentOrShadowHostElement() const
{
    ContainerNode* parent = parentOrShadowHostNode();
    if (!parent)
        return nullptr;

    if (parent->isShadowRoot())
        return &toShadowRoot(parent)->host();

    if (!parent->isElementNode())
        return nullptr;

    return toElement(parent);
}

ContainerNode* Node::parentOrShadowHostOrTemplateHostNode() const
{
    if (isDocumentFragment() && toDocumentFragment(this)->isTemplateContent())
        return static_cast<const TemplateContentDocumentFragment*>(this)->host();
    return parentOrShadowHostNode();
}

bool Node::isRootEditableElement() const
{
    return hasEditableStyle() && isElementNode() && (!parentNode() || !parentNode()->hasEditableStyle()
        || !parentNode()->isElementNode() || this == document().body());
}

Element* Node::rootEditableElement(EditableType editableType) const
{
    if (editableType == HasEditableAXRole) {
        if (AXObjectCache* cache = document().existingAXObjectCache())
            return const_cast<Element*>(cache->rootAXEditableElement(this));
    }

    return rootEditableElement();
}

Element* Node::rootEditableElement() const
{
    const Node* result = nullptr;
    for (const Node* n = this; n && n->hasEditableStyle(); n = n->parentNode()) {
        if (n->isElementNode())
            result = n;
        if (document().body() == n)
            break;
    }
    return toElement(const_cast<Node*>(result));
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of Node.

Document* Node::ownerDocument() const
{
    Document* doc = &document();
    return doc == this ? nullptr : doc;
}

const KURL& Node::baseURI() const
{
    return document().baseURL();
}

bool Node::isEqualNode(Node* other) const
{
    if (!other)
        return false;

    NodeType nodeType = this->getNodeType();
    if (nodeType != other->getNodeType())
        return false;

    if (nodeValue() != other->nodeValue())
        return false;

    if (isAttributeNode()) {
        if (toAttr(this)->localName() != toAttr(other)->localName())
            return false;

        if (toAttr(this)->namespaceURI() != toAttr(other)->namespaceURI())
            return false;
    } else if (isElementNode()) {
        if (toElement(this)->tagQName() != toElement(other)->tagQName())
            return false;

        if (!toElement(this)->hasEquivalentAttributes(toElement(other)))
            return false;
    } else if (nodeName() != other->nodeName()) {
        return false;
    }

    Node* child = firstChild();
    Node* otherChild = other->firstChild();

    while (child) {
        if (!child->isEqualNode(otherChild))
            return false;

        child = child->nextSibling();
        otherChild = otherChild->nextSibling();
    }

    if (otherChild)
        return false;

    if (isDocumentTypeNode()) {
        const DocumentType* documentTypeThis = toDocumentType(this);
        const DocumentType* documentTypeOther = toDocumentType(other);

        if (documentTypeThis->publicId() != documentTypeOther->publicId())
            return false;

        if (documentTypeThis->systemId() != documentTypeOther->systemId())
            return false;
    }

    return true;
}

bool Node::isDefaultNamespace(const AtomicString& namespaceURIMaybeEmpty) const
{
    const AtomicString& namespaceURI = namespaceURIMaybeEmpty.isEmpty() ? nullAtom : namespaceURIMaybeEmpty;

    switch (getNodeType()) {
    case ELEMENT_NODE: {
        const Element& element = toElement(*this);

        if (element.prefix().isNull())
            return element.namespaceURI() == namespaceURI;

        AttributeCollection attributes = element.attributes();
        for (const Attribute& attr : attributes) {
            if (attr.localName() == xmlnsAtom)
                return attr.value() == namespaceURI;
        }

        if (Element* parent = parentElement())
            return parent->isDefaultNamespace(namespaceURI);

        return false;
    }
    case DOCUMENT_NODE:
        if (Element* de = toDocument(this)->documentElement())
            return de->isDefaultNamespace(namespaceURI);
        return false;
    case DOCUMENT_TYPE_NODE:
    case DOCUMENT_FRAGMENT_NODE:
        return false;
    case ATTRIBUTE_NODE: {
        const Attr* attr = toAttr(this);
        if (attr->ownerElement())
            return attr->ownerElement()->isDefaultNamespace(namespaceURI);
        return false;
    }
    default:
        if (Element* parent = parentElement())
            return parent->isDefaultNamespace(namespaceURI);
        return false;
    }
}

const AtomicString& Node::lookupPrefix(const AtomicString& namespaceURI) const
{
    // Implemented according to
    // https://dom.spec.whatwg.org/#dom-node-lookupprefix

    if (namespaceURI.isEmpty() || namespaceURI.isNull())
        return nullAtom;

    const Element* context;

    switch (getNodeType()) {
    case ELEMENT_NODE:
        context = toElement(this);
        break;
    case DOCUMENT_NODE:
        context = toDocument(this)->documentElement();
        break;
    case DOCUMENT_FRAGMENT_NODE:
    case DOCUMENT_TYPE_NODE:
        context = nullptr;
        break;
    // FIXME: Remove this when Attr no longer extends Node (CR305105)
    case ATTRIBUTE_NODE:
        context = toAttr(this)->ownerElement();
        break;
    default:
        context = parentElement();
        break;
    }

    if (!context)
        return nullAtom;

    return context->locateNamespacePrefix(namespaceURI);
}

const AtomicString& Node::lookupNamespaceURI(const String& prefix) const
{
    // Implemented according to
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespaceURIAlgo

    if (!prefix.isNull() && prefix.isEmpty())
        return nullAtom;

    switch (getNodeType()) {
    case ELEMENT_NODE: {
        const Element& element = toElement(*this);

        if (!element.namespaceURI().isNull() && element.prefix() == prefix)
            return element.namespaceURI();

        AttributeCollection attributes = element.attributes();
        for (const Attribute& attr : attributes) {
            if (attr.prefix() == xmlnsAtom && attr.localName() == prefix) {
                if (!attr.value().isEmpty())
                    return attr.value();
                return nullAtom;
            }
            if (attr.localName() == xmlnsAtom && prefix.isNull()) {
                if (!attr.value().isEmpty())
                    return attr.value();
                return nullAtom;
            }
        }

        if (Element* parent = parentElement())
            return parent->lookupNamespaceURI(prefix);
        return nullAtom;
    }
    case DOCUMENT_NODE:
        if (Element* de = toDocument(this)->documentElement())
            return de->lookupNamespaceURI(prefix);
        return nullAtom;
    case DOCUMENT_TYPE_NODE:
    case DOCUMENT_FRAGMENT_NODE:
        return nullAtom;
    case ATTRIBUTE_NODE: {
        const Attr *attr = toAttr(this);
        if (attr->ownerElement())
            return attr->ownerElement()->lookupNamespaceURI(prefix);
        return nullAtom;
    }
    default:
        if (Element* parent = parentElement())
            return parent->lookupNamespaceURI(prefix);
        return nullAtom;
    }
}

String Node::textContent(bool convertBRsToNewlines) const
{
    // This covers ProcessingInstruction and Comment that should return their
    // value when .textContent is accessed on them, but should be ignored when
    // iterated over as a descendant of a ContainerNode.
    if (isCharacterDataNode())
        return toCharacterData(this)->data();

    // Documents and non-container nodes (that are not CharacterData)
    // have null textContent.
    if (isDocumentNode() || !isContainerNode())
        return String();

    StringBuilder content;
    for (const Node& node : NodeTraversal::inclusiveDescendantsOf(*this)) {
        if (isHTMLBRElement(node) && convertBRsToNewlines) {
            content.append('\n');
        } else if (node.isTextNode()) {
            content.append(toText(node).data());
        }
    }
    return content.toString();
}

void Node::setTextContent(const String& text)
{
    switch (getNodeType()) {
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
        setNodeValue(text);
        return;
    case ELEMENT_NODE:
    case DOCUMENT_FRAGMENT_NODE: {
        // FIXME: Merge this logic into replaceChildrenWithText.
        ContainerNode* container = toContainerNode(this);

        // Note: This is an intentional optimization.
        // See crbug.com/352836 also.
        // No need to do anything if the text is identical.
        if (container->hasOneTextChild() && toText(container->firstChild())->data() == text)
            return;

        ChildListMutationScope mutation(*this);
        // Note: This API will not insert empty text nodes:
        // https://dom.spec.whatwg.org/#dom-node-textcontent
        if (text.isEmpty()) {
            container->removeChildren(DispatchSubtreeModifiedEvent);
        } else {
            container->removeChildren(OmitSubtreeModifiedEvent);
            container->appendChild(document().createTextNode(text), ASSERT_NO_EXCEPTION);
        }
        return;
    }
    case ATTRIBUTE_NODE:
    case DOCUMENT_NODE:
    case DOCUMENT_TYPE_NODE:
        // Do nothing.
        return;
    }
    ASSERT_NOT_REACHED();
}

bool Node::offsetInCharacters() const
{
    return isCharacterDataNode();
}

unsigned short Node::compareDocumentPosition(const Node* otherNode, ShadowTreesTreatment treatment) const
{
    if (otherNode == this)
        return DOCUMENT_POSITION_EQUIVALENT;

    const Attr* attr1 = getNodeType() == ATTRIBUTE_NODE ? toAttr(this) : nullptr;
    const Attr* attr2 = otherNode->getNodeType() == ATTRIBUTE_NODE ? toAttr(otherNode) : nullptr;

    const Node* start1 = attr1 ? attr1->ownerElement() : this;
    const Node* start2 = attr2 ? attr2->ownerElement() : otherNode;

    // If either of start1 or start2 is null, then we are disconnected, since one of the nodes is
    // an orphaned attribute node.
    if (!start1 || !start2) {
        unsigned short direction = (this > otherNode) ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
    }

    HeapVector<Member<const Node>, 16> chain1;
    HeapVector<Member<const Node>, 16> chain2;
    if (attr1)
        chain1.append(attr1);
    if (attr2)
        chain2.append(attr2);

    if (attr1 && attr2 && start1 == start2 && start1) {
        // We are comparing two attributes on the same node. Crawl our attribute map and see which one we hit first.
        const Element* owner1 = attr1->ownerElement();
        AttributeCollection attributes = owner1->attributes();
        for (const Attribute& attr : attributes) {
            // If neither of the two determining nodes is a child node and nodeType is the same for both determining nodes, then an
            // implementation-dependent order between the determining nodes is returned. This order is stable as long as no nodes of
            // the same nodeType are inserted into or removed from the direct container. This would be the case, for example,
            // when comparing two attributes of the same element, and inserting or removing additional attributes might change
            // the order between existing attributes.
            if (attr1->getQualifiedName() == attr.name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;
            if (attr2->getQualifiedName() == attr.name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
        }

        ASSERT_NOT_REACHED();
        return DOCUMENT_POSITION_DISCONNECTED;
    }

    // If one node is in the document and the other is not, we must be disconnected.
    // If the nodes have different owning documents, they must be disconnected.  Note that we avoid
    // comparing Attr nodes here, since they return false from inShadowIncludingDocument() all the time (which seems like a bug).
    if (start1->inShadowIncludingDocument() != start2->inShadowIncludingDocument() || (treatment == TreatShadowTreesAsDisconnected && start1->treeScope() != start2->treeScope())) {
        unsigned short direction = (this > otherNode) ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
    }

    // We need to find a common ancestor container, and then compare the indices of the two immediate children.
    const Node* current;
    for (current = start1; current; current = current->parentOrShadowHostNode())
        chain1.append(current);
    for (current = start2; current; current = current->parentOrShadowHostNode())
        chain2.append(current);

    unsigned index1 = chain1.size();
    unsigned index2 = chain2.size();

    // If the two elements don't have a common root, they're not in the same tree.
    if (chain1[index1 - 1] != chain2[index2 - 1]) {
        unsigned short direction = (this > otherNode) ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
    }

    unsigned connection = start1->treeScope() != start2->treeScope() ? DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC : 0;

    // Walk the two chains backwards and look for the first difference.
    for (unsigned i = std::min(index1, index2); i; --i) {
        const Node* child1 = chain1[--index1];
        const Node* child2 = chain2[--index2];
        if (child1 != child2) {
            // If one of the children is an attribute, it wins.
            if (child1->getNodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_FOLLOWING | connection;
            if (child2->getNodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_PRECEDING | connection;

            // If one of the children is a shadow root,
            if (child1->isShadowRoot() || child2->isShadowRoot()) {
                if (!child2->isShadowRoot())
                    return Node::DOCUMENT_POSITION_FOLLOWING | connection;
                if (!child1->isShadowRoot())
                    return Node::DOCUMENT_POSITION_PRECEDING | connection;

                for (const ShadowRoot* child = toShadowRoot(child2)->olderShadowRoot(); child; child = child->olderShadowRoot()) {
                    if (child == child1) {
                        return Node::DOCUMENT_POSITION_FOLLOWING | connection;
                    }
                }

                return Node::DOCUMENT_POSITION_PRECEDING | connection;
            }

            if (!child2->nextSibling())
                return DOCUMENT_POSITION_FOLLOWING | connection;
            if (!child1->nextSibling())
                return DOCUMENT_POSITION_PRECEDING | connection;

            // Otherwise we need to see which node occurs first.  Crawl backwards from child2 looking for child1.
            for (const Node* child = child2->previousSibling(); child; child = child->previousSibling()) {
                if (child == child1)
                    return DOCUMENT_POSITION_FOLLOWING | connection;
            }
            return DOCUMENT_POSITION_PRECEDING | connection;
        }
    }

    // There was no difference between the two parent chains, i.e., one was a subset of the other.  The shorter
    // chain is the ancestor.
    return index1 < index2 ?
        DOCUMENT_POSITION_FOLLOWING | DOCUMENT_POSITION_CONTAINED_BY | connection :
        DOCUMENT_POSITION_PRECEDING | DOCUMENT_POSITION_CONTAINS | connection;
}

String Node::debugName() const
{
    StringBuilder name;
    name.append(debugNodeName());
    if (isElementNode()) {
        const Element& thisElement = toElement(*this);
        if (thisElement.hasID()) {
            name.append(" id=\'");
            name.append(thisElement.getIdAttribute());
            name.append('\'');
        }

        if (thisElement.hasClass()) {
            name.append(" class=\'");
            for (size_t i = 0; i < thisElement.classNames().size(); ++i) {
                if (i > 0)
                    name.append(' ');
                name.append(thisElement.classNames()[i]);
            }
            name.append('\'');
        }
    }
    return name.toString();
}

String Node::debugNodeName() const
{
    return nodeName();
}

static void dumpAttributeDesc(const Node& node, const QualifiedName& name, std::ostream& ostream)
{
    if (!node.isElementNode())
        return;
    const AtomicString& value = toElement(node).getAttribute(name);
    if (value.isEmpty())
        return;
    ostream << ' ' << name.toString().utf8().data() << '=' << value;
}

// |std::ostream| version of |Node::showNode|
std::ostream& operator<<(std::ostream& ostream, const Node& node)
{
    // We avoid to print "" by utf8().data().
    ostream << node.nodeName().utf8().data();
    if (node.isTextNode())
        return ostream << " " << node.nodeValue();
    dumpAttributeDesc(node, HTMLNames::idAttr, ostream);
    dumpAttributeDesc(node, HTMLNames::classAttr, ostream);
    dumpAttributeDesc(node, HTMLNames::styleAttr, ostream);
    return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const Node* node)
{
    if (!node)
        return ostream << "null";
    return ostream << *node;
}

#ifndef NDEBUG

static void appendAttributeDesc(const Node* node, StringBuilder& stringBuilder, const QualifiedName& name, const char* attrDesc)
{
    if (!node->isElementNode())
        return;

    String attr = toElement(node)->getAttribute(name);
    if (attr.isEmpty())
        return;

    stringBuilder.append(attrDesc);
    stringBuilder.append("=\"");
    stringBuilder.append(attr);
    stringBuilder.append("\"");
}

void Node::showNode(const char* prefix) const
{
    if (!prefix)
        prefix = "";
    if (isTextNode()) {
        String value = nodeValue();
        value.replace('\\', "\\\\");
        value.replace('\n', "\\n");
        WTFLogAlways("%s%s\t%p \"%s\"\n", prefix, nodeName().utf8().data(), this, value.utf8().data());
    } else if (isDocumentTypeNode()) {
        WTFLogAlways("%sDOCTYPE %s\t%p\n", prefix, nodeName().utf8().data(), this);
    } else if (getNodeType() == PROCESSING_INSTRUCTION_NODE) {
        WTFLogAlways("%s?%s\t%p\n", prefix, nodeName().utf8().data(), this);
    } else if (isShadowRoot()) {
        // nodeName of ShadowRoot is #document-fragment.  It's confused with
        // DocumentFragment.
        WTFLogAlways("%s#shadow-root\t%p\n", prefix, this);
    } else {
        StringBuilder attrs;
        appendAttributeDesc(this, attrs, idAttr, " ID");
        appendAttributeDesc(this, attrs, classAttr, " CLASS");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE");
        if (hasEditableStyle())
            attrs.append(" (editable)");
        if (document().focusedElement() == this)
            attrs.append(" (focused)");
        WTFLogAlways("%s%s\t%p%s\n", prefix, nodeName().utf8().data(), this, attrs.toString().utf8().data());
    }
}

void Node::showTreeForThis() const
{
    showTreeAndMark(this, "*");
}

void Node::showTreeForThisInFlatTree() const
{
    showTreeAndMarkInFlatTree(this, "*");
}

void Node::showNodePathForThis() const
{
    HeapVector<Member<const Node>, 16> chain;
    const Node* node = this;
    while (node->parentOrShadowHostNode()) {
        chain.append(node);
        node = node->parentOrShadowHostNode();
    }
    for (unsigned index = chain.size(); index > 0; --index) {
        const Node* node = chain[index - 1];
        if (node->isShadowRoot()) {
            int count = 0;
            for (const ShadowRoot* shadowRoot = toShadowRoot(node)->olderShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
                ++count;
            WTFLogAlways("/#shadow-root[%d]", count);
            continue;
        }

        switch (node->getNodeType()) {
        case ELEMENT_NODE: {
            WTFLogAlways("/%s", node->nodeName().utf8().data());

            const Element* element = toElement(node);
            const AtomicString& idattr = element->getIdAttribute();
            bool hasIdAttr = !idattr.isNull() && !idattr.isEmpty();
            if (node->previousSibling() || node->nextSibling()) {
                int count = 0;
                for (const Node* previous = node->previousSibling(); previous; previous = previous->previousSibling()) {
                    if (previous->nodeName() == node->nodeName()) {
                        ++count;
                    }
                }
                if (hasIdAttr)
                    WTFLogAlways("[@id=\"%s\" and position()=%d]", idattr.utf8().data(), count);
                else
                    WTFLogAlways("[%d]", count);
            } else if (hasIdAttr) {
                WTFLogAlways("[@id=\"%s\"]", idattr.utf8().data());
            }
            break;
        }
        case TEXT_NODE:
            WTFLogAlways("/text()");
            break;
        case ATTRIBUTE_NODE:
            WTFLogAlways("/@%s", node->nodeName().utf8().data());
            break;
        default:
            break;
        }
    }
    WTFLogAlways("\n");
}

static void traverseTreeAndMark(const String& baseIndent, const Node* rootNode, const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2)
{
    for (const Node& node : NodeTraversal::inclusiveDescendantsOf(*rootNode)) {
        StringBuilder indent;
        if (node == markedNode1)
            indent.append(markedLabel1);
        if (node == markedNode2)
            indent.append(markedLabel2);
        indent.append(baseIndent);
        for (const Node* tmpNode = &node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentOrShadowHostNode())
            indent.append('\t');
        node.showNode(indent.toString().utf8().data());
        indent.append('\t');

        if (node.isElementNode()) {
            const Element& element = toElement(node);
            if (Element* pseudo = element.pseudoElement(PseudoIdBefore))
                traverseTreeAndMark(indent.toString(), pseudo, markedNode1, markedLabel1, markedNode2, markedLabel2);
            if (Element* pseudo = element.pseudoElement(PseudoIdAfter))
                traverseTreeAndMark(indent.toString(), pseudo, markedNode1, markedLabel1, markedNode2, markedLabel2);
            if (Element* pseudo = element.pseudoElement(PseudoIdFirstLetter))
                traverseTreeAndMark(indent.toString(), pseudo, markedNode1, markedLabel1, markedNode2, markedLabel2);
            if (Element* pseudo = element.pseudoElement(PseudoIdBackdrop))
                traverseTreeAndMark(indent.toString(), pseudo, markedNode1, markedLabel1, markedNode2, markedLabel2);
        }

        if (node.isShadowRoot()) {
            if (ShadowRoot* youngerShadowRoot = toShadowRoot(node).youngerShadowRoot())
                traverseTreeAndMark(indent.toString(), youngerShadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
        } else if (ShadowRoot* oldestShadowRoot = oldestShadowRootFor(&node)) {
            traverseTreeAndMark(indent.toString(), oldestShadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
        }
    }
}

static void traverseTreeAndMarkInFlatTree(const String& baseIndent, const Node* rootNode, const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2)
{
    for (const Node* node = rootNode; node; node = FlatTreeTraversal::nextSibling(*node)) {
        StringBuilder indent;
        if (node == markedNode1)
            indent.append(markedLabel1);
        if (node == markedNode2)
            indent.append(markedLabel2);
        indent.append(baseIndent);
        node->showNode(indent.toString().utf8().data());
        indent.append('\t');

        Node* child = FlatTreeTraversal::firstChild(*node);
        if (child)
            traverseTreeAndMarkInFlatTree(indent.toString(), child, markedNode1, markedLabel1, markedNode2, markedLabel2);
    }
}

void Node::showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2) const
{
    const Node* rootNode;
    const Node* node = this;
    while (node->parentOrShadowHostNode() && !isHTMLBodyElement(*node))
        node = node->parentOrShadowHostNode();
    rootNode = node;

    String startingIndent;
    traverseTreeAndMark(startingIndent, rootNode, markedNode1, markedLabel1, markedNode2, markedLabel2);
}

void Node::showTreeAndMarkInFlatTree(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2) const
{
    const Node* rootNode;
    const Node* node = this;
    while (node->parentOrShadowHostNode() && !isHTMLBodyElement(*node))
        node = node->parentOrShadowHostNode();
    rootNode = node;

    String startingIndent;
    traverseTreeAndMarkInFlatTree(startingIndent, rootNode, markedNode1, markedLabel1, markedNode2, markedLabel2);
}

void Node::formatForDebugger(char* buffer, unsigned length) const
{
    String result;
    String s;

    s = nodeName();
    if (s.isEmpty())
        result = "<none>";
    else
        result = s;

    strncpy(buffer, result.utf8().data(), length - 1);
}

static ContainerNode* parentOrShadowHostOrFrameOwner(const Node* node)
{
    ContainerNode* parent = node->parentOrShadowHostNode();
    if (!parent && node->document().frame())
        parent = node->document().frame()->deprecatedLocalOwner();
    return parent;
}

static void showSubTreeAcrossFrame(const Node* node, const Node* markedNode, const String& indent)
{
    if (node == markedNode)
        fputs("*", stderr);
    fputs(indent.utf8().data(), stderr);
    node->showNode();
    if (node->isShadowRoot()) {
        if (ShadowRoot* youngerShadowRoot = toShadowRoot(node)->youngerShadowRoot())
            showSubTreeAcrossFrame(youngerShadowRoot, markedNode, indent + "\t");
    } else {
        if (node->isFrameOwnerElement())
            showSubTreeAcrossFrame(toHTMLFrameOwnerElement(node)->contentDocument(), markedNode, indent + "\t");
        if (ShadowRoot* oldestShadowRoot = oldestShadowRootFor(node))
            showSubTreeAcrossFrame(oldestShadowRoot, markedNode, indent + "\t");
    }
    for (const Node* child = node->firstChild(); child; child = child->nextSibling())
        showSubTreeAcrossFrame(child, markedNode, indent + "\t");
}

void Node::showTreeForThisAcrossFrame() const
{
    const Node* rootNode = this;
    while (parentOrShadowHostOrFrameOwner(rootNode))
        rootNode = parentOrShadowHostOrFrameOwner(rootNode);
    showSubTreeAcrossFrame(rootNode, this, "");
}

#endif

// --------

Element* Node::enclosingLinkEventParentOrSelf() const
{
    const Node* result = nullptr;
    for (const Node* node = this; node; node = FlatTreeTraversal::parent(*node)) {
        // For imagemaps, the enclosing link node is the associated area element not the image itself.
        // So we don't let images be the enclosingLinkNode, even though isLink sometimes returns true
        // for them.
        if (node->isLink() && !isHTMLImageElement(*node)) {
            // Casting to Element is safe because only HTMLAnchorElement, HTMLImageElement and
            // SVGAElement can return true for isLink().
            result = node;
            break;
        }
    }

    return toElement(const_cast<Node*>(result));
}

const AtomicString& Node::interfaceName() const
{
    return EventTargetNames::Node;
}

ExecutionContext* Node::getExecutionContext() const
{
    return document().contextDocument();
}

void Node::didMoveToNewDocument(Document& oldDocument)
{
    TreeScopeAdopter::ensureDidMoveToNewDocumentWasCalled(oldDocument);

    if (const EventTargetData* eventTargetData = this->eventTargetData()) {
        const EventListenerMap& listenerMap = eventTargetData->eventListenerMap;
        if (!listenerMap.isEmpty()) {
            Vector<AtomicString> types = listenerMap.eventTypes();
            for (unsigned i = 0; i < types.size(); ++i)
                document().addListenerTypeIfNeeded(types[i]);
        }
    }

    oldDocument.markers().removeMarkers(this);
    oldDocument.updateRangesAfterNodeMovedToAnotherDocument(*this);
    if (oldDocument.frameHost() && !document().frameHost())
        oldDocument.frameHost()->eventHandlerRegistry().didMoveOutOfFrameHost(*this);
    else if (document().frameHost() && !oldDocument.frameHost())
        document().frameHost()->eventHandlerRegistry().didMoveIntoFrameHost(*this);
    else if (oldDocument.frameHost() != document().frameHost())
        EventHandlerRegistry::didMoveBetweenFrameHosts(*this, oldDocument.frameHost(), document().frameHost());

    if (HeapVector<Member<MutationObserverRegistration>>* registry = mutationObserverRegistry()) {
        for (size_t i = 0; i < registry->size(); ++i) {
            document().addMutationObserverTypes(registry->at(i)->mutationTypes());
        }
    }

    if (transientMutationObserverRegistry()) {
        for (MutationObserverRegistration* registration : *transientMutationObserverRegistry())
            document().addMutationObserverTypes(registration->mutationTypes());
    }
}

void Node::addedEventListener(const AtomicString& eventType, RegisteredEventListener& registeredListener)
{
    EventTarget::addedEventListener(eventType, registeredListener);
    document().addListenerTypeIfNeeded(eventType);
    if (FrameHost* frameHost = document().frameHost())
        frameHost->eventHandlerRegistry().didAddEventHandler(*this, eventType, registeredListener.options());
}

void Node::removedEventListener(const AtomicString& eventType, const RegisteredEventListener& registeredListener)
{
    EventTarget::removedEventListener(eventType, registeredListener);
    // FIXME: Notify Document that the listener has vanished. We need to keep track of a number of
    // listeners for each type, not just a bool - see https://bugs.webkit.org/show_bug.cgi?id=33861
    if (FrameHost* frameHost = document().frameHost())
        frameHost->eventHandlerRegistry().didRemoveEventHandler(*this, eventType, registeredListener.options());
}

void Node::removeAllEventListeners()
{
    if (hasEventListeners() && document().frameHost())
        document().frameHost()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);
    EventTarget::removeAllEventListeners();
}

void Node::removeAllEventListenersRecursively()
{
    ScriptForbiddenScope forbidScriptDuringRawIteration;
    for (Node& node : NodeTraversal::startsAt(*this)) {
        node.removeAllEventListeners();
        for (ShadowRoot* root = node.youngestShadowRoot(); root; root = root->olderShadowRoot())
            root->removeAllEventListenersRecursively();
    }
}

using EventTargetDataMap = HeapHashMap<WeakMember<Node>, Member<EventTargetData>>;
static EventTargetDataMap& eventTargetDataMap()
{
    DEFINE_STATIC_LOCAL(EventTargetDataMap, map, (new EventTargetDataMap));
    return map;
}

EventTargetData* Node::eventTargetData()
{
    return hasEventTargetData() ? eventTargetDataMap().get(this) : nullptr;
}

EventTargetData& Node::ensureEventTargetData()
{
    if (hasEventTargetData())
        return *eventTargetDataMap().get(this);
    DCHECK(!eventTargetDataMap().contains(this));
    setHasEventTargetData(true);
    EventTargetData* data = new EventTargetData;
    eventTargetDataMap().set(this, data);
    return *data;
}

HeapVector<Member<MutationObserverRegistration>>* Node::mutationObserverRegistry()
{
    if (!hasRareData())
        return nullptr;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return nullptr;
    return &data->registry;
}

HeapHashSet<Member<MutationObserverRegistration>>* Node::transientMutationObserverRegistry()
{
    if (!hasRareData())
        return nullptr;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return nullptr;
    return &data->transientRegistry;
}

template<typename Registry>
static inline void collectMatchingObserversForMutation(HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>& observers, Registry* registry, Node& target, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    if (!registry)
        return;

    for (const auto& registration : *registry) {
        if (registration->shouldReceiveMutationFrom(target, type, attributeName)) {
            MutationRecordDeliveryOptions deliveryOptions = registration->deliveryOptions();
            HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>::AddResult result = observers.add(&registration->observer(), deliveryOptions);
            if (!result.isNewEntry)
                result.storedValue->value |= deliveryOptions;
        }
    }
}

void Node::getRegisteredMutationObserversOfType(HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>& observers, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    DCHECK((type == MutationObserver::Attributes && attributeName) || !attributeName);
    collectMatchingObserversForMutation(observers, mutationObserverRegistry(), *this, type, attributeName);
    collectMatchingObserversForMutation(observers, transientMutationObserverRegistry(), *this, type, attributeName);
    ScriptForbiddenScope forbidScriptDuringRawIteration;
    for (Node* node = parentNode(); node; node = node->parentNode()) {
        collectMatchingObserversForMutation(observers, node->mutationObserverRegistry(), *this, type, attributeName);
        collectMatchingObserversForMutation(observers, node->transientMutationObserverRegistry(), *this, type, attributeName);
    }
}

void Node::registerMutationObserver(MutationObserver& observer, MutationObserverOptions options, const HashSet<AtomicString>& attributeFilter)
{
    MutationObserverRegistration* registration = nullptr;
    HeapVector<Member<MutationObserverRegistration>>& registry = ensureRareData().ensureMutationObserverData().registry;
    for (size_t i = 0; i < registry.size(); ++i) {
        if (&registry[i]->observer() == &observer) {
            registration = registry[i].get();
            registration->resetObservation(options, attributeFilter);
        }
    }

    if (!registration) {
        registry.append(MutationObserverRegistration::create(observer, this, options, attributeFilter));
        registration = registry.last().get();
    }

    document().addMutationObserverTypes(registration->mutationTypes());
}

void Node::unregisterMutationObserver(MutationObserverRegistration* registration)
{
    HeapVector<Member<MutationObserverRegistration>>* registry = mutationObserverRegistry();
    DCHECK(registry);
    if (!registry)
        return;

    size_t index = registry->find(registration);
    DCHECK_NE(index, kNotFound);
    if (index == kNotFound)
        return;

    // FIXME: Simplify the registration/transient registration logic to make this understandable by humans.
    // The explicit dispose() is needed to have the registration
    // object unregister itself promptly.
    registration->dispose();
    registry->remove(index);
}

void Node::registerTransientMutationObserver(MutationObserverRegistration* registration)
{
    ensureRareData().ensureMutationObserverData().transientRegistry.add(registration);
}

void Node::unregisterTransientMutationObserver(MutationObserverRegistration* registration)
{
    HeapHashSet<Member<MutationObserverRegistration>>* transientRegistry = transientMutationObserverRegistry();
    DCHECK(transientRegistry);
    if (!transientRegistry)
        return;

    DCHECK(transientRegistry->contains(registration));
    transientRegistry->remove(registration);
}

void Node::notifyMutationObserversNodeWillDetach()
{
    if (!document().hasMutationObservers())
        return;

    ScriptForbiddenScope forbidScriptDuringRawIteration;
    for (Node* node = parentNode(); node; node = node->parentNode()) {
        if (HeapVector<Member<MutationObserverRegistration>>* registry = node->mutationObserverRegistry()) {
            const size_t size = registry->size();
            for (size_t i = 0; i < size; ++i)
                registry->at(i)->observedSubtreeNodeWillDetach(*this);
        }

        if (HeapHashSet<Member<MutationObserverRegistration>>* transientRegistry = node->transientMutationObserverRegistry()) {
            for (auto& registration : *transientRegistry)
                registration->observedSubtreeNodeWillDetach(*this);
        }
    }
}

void Node::handleLocalEvents(Event& event)
{
    if (!hasEventTargetData())
        return;

    if (isDisabledFormControl(this) && event.isMouseEvent())
        return;

    fireEventListeners(&event);
}

void Node::dispatchScopedEvent(Event* event)
{
    event->setTrusted(true);
    EventDispatcher::dispatchScopedEvent(*this, event->createMediator());
}

DispatchEventResult Node::dispatchEventInternal(Event* event)
{
    return EventDispatcher::dispatchEvent(*this, event->createMediator());
}

void Node::dispatchSubtreeModifiedEvent()
{
    if (isInShadowTree())
        return;

#if DCHECK_IS_ON()
    DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif

    if (!document().hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return;

    dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMSubtreeModified, true));
}

DispatchEventResult Node::dispatchDOMActivateEvent(int detail, Event* underlyingEvent)
{
#if DCHECK_IS_ON()
    DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif
    UIEvent* event = UIEvent::create(EventTypeNames::DOMActivate, true, true, document().domWindow(), detail);
    event->setUnderlyingEvent(underlyingEvent);
    dispatchScopedEvent(event);

    // TODO(dtapuska): Dispatching scoped events shouldn't check the return
    // type because the scoped event could get put off in the delayed queue.
    return EventTarget::dispatchEventResult(*event);
}

DispatchEventResult Node::dispatchMouseEvent(const PlatformMouseEvent& nativeEvent, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    MouseEvent* event = MouseEvent::create(eventType, document().domWindow(), nativeEvent, detail, relatedTarget);
    return dispatchEvent(event);
}

void Node::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions, SimulatedClickCreationScope scope)
{
    EventDispatcher::dispatchSimulatedClick(*this, underlyingEvent, eventOptions, scope);
}

void Node::dispatchInputEvent()
{
    // Legacy 'input' event for forms set value and checked.
    dispatchScopedEvent(Event::createBubble(EventTypeNames::input));
}

void Node::defaultEventHandler(Event* event)
{
    if (event->target() != this)
        return;
    const AtomicString& eventType = event->type();
    if (eventType == EventTypeNames::keydown || eventType == EventTypeNames::keypress) {
        if (event->isKeyboardEvent()) {
            if (LocalFrame* frame = document().frame())
                frame->eventHandler().defaultKeyboardEventHandler(toKeyboardEvent(event));
        }
    } else if (eventType == EventTypeNames::click) {
        int detail = event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
        if (dispatchDOMActivateEvent(detail, event) != DispatchEventResult::NotCanceled)
            event->setDefaultHandled();
    } else if (eventType == EventTypeNames::contextmenu) {
        if (Page* page = document().page())
            page->contextMenuController().handleContextMenuEvent(event);
    } else if (eventType == EventTypeNames::textInput) {
        if (event->hasInterface(EventNames::TextEvent)) {
            if (LocalFrame* frame = document().frame())
                frame->eventHandler().defaultTextInputEventHandler(toTextEvent(event));
        }
#if OS(WIN)
    } else if (eventType == EventTypeNames::mousedown && event->isMouseEvent()) {
        MouseEvent* mouseEvent = toMouseEvent(event);
        if (mouseEvent->button() == MiddleButton) {
            if (enclosingLinkEventParentOrSelf())
                return;

            // Avoid that canBeScrolledAndHasScrollableArea changes layout tree
            // structure.
            // FIXME: We should avoid synchronous layout if possible. We can
            // remove this synchronous layout if we avoid synchronous layout in
            // LayoutTextControlSingleLine::scrollHeight
            document().updateStyleAndLayoutIgnorePendingStylesheets();
            LayoutObject* layoutObject = this->layoutObject();
            while (layoutObject && (!layoutObject->isBox() || !toLayoutBox(layoutObject)->canBeScrolledAndHasScrollableArea()))
                layoutObject = layoutObject->parent();

            if (layoutObject) {
                if (LocalFrame* frame = document().frame())
                    frame->eventHandler().startPanScrolling(layoutObject);
            }
        }
#endif
    } else if (event->type() == EventTypeNames::webkitEditableContentChanged) {
        // TODO(chongz): Remove after shipped.
        // New InputEvent are dispatched in Editor::appliedEditing, etc.
        if (!RuntimeEnabledFeatures::inputEventEnabled())
            dispatchInputEvent();
    }
}

void Node::willCallDefaultEventHandler(const Event&)
{
}

bool Node::willRespondToMouseMoveEvents()
{
    if (isDisabledFormControl(this))
        return false;
    return hasEventListeners(EventTypeNames::mousemove) || hasEventListeners(EventTypeNames::mouseover) || hasEventListeners(EventTypeNames::mouseout);
}

bool Node::willRespondToMouseClickEvents()
{
    if (isDisabledFormControl(this))
        return false;
    return isContentEditable(UserSelectAllIsAlwaysNonEditable) || hasEventListeners(EventTypeNames::mouseup) || hasEventListeners(EventTypeNames::mousedown) || hasEventListeners(EventTypeNames::click) || hasEventListeners(EventTypeNames::DOMActivate);
}

bool Node::willRespondToTouchEvents()
{
    if (isDisabledFormControl(this))
        return false;
    return hasEventListeners(EventTypeNames::touchstart) || hasEventListeners(EventTypeNames::touchmove) || hasEventListeners(EventTypeNames::touchcancel) || hasEventListeners(EventTypeNames::touchend);
}

unsigned Node::connectedSubframeCount() const
{
    return hasRareData() ? rareData()->connectedSubframeCount() : 0;
}

void Node::incrementConnectedSubframeCount()
{
    DCHECK(isContainerNode());
    ensureRareData().incrementConnectedSubframeCount();
}

void Node::decrementConnectedSubframeCount()
{
    rareData()->decrementConnectedSubframeCount();
}

StaticNodeList* Node::getDestinationInsertionPoints()
{
    updateDistribution();
    HeapVector<Member<InsertionPoint>, 8> insertionPoints;
    collectDestinationInsertionPoints(*this, insertionPoints);
    HeapVector<Member<Node>> filteredInsertionPoints;
    for (size_t i = 0; i < insertionPoints.size(); ++i) {
        InsertionPoint* insertionPoint = insertionPoints[i];
        DCHECK(insertionPoint->containingShadowRoot());
        if (!insertionPoint->containingShadowRoot()->isOpenOrV0())
            break;
        filteredInsertionPoints.append(insertionPoint);
    }
    return StaticNodeList::adopt(filteredInsertionPoints);
}

HTMLSlotElement* Node::assignedSlot() const
{
    if (ShadowRoot* root = v1ShadowRootOfParent())
        return root->ensureSlotAssignment().findSlot(*this);
    return nullptr;
}

HTMLSlotElement* Node::assignedSlotForBinding()
{
    updateDistribution();
    if (ShadowRoot* root = v1ShadowRootOfParent()) {
        if (root->type() == ShadowRootType::Open)
            return root->ensureSlotAssignment().findSlot(*this);
    }
    return nullptr;
}

void Node::setFocus(bool flag)
{
    document().userActionElements().setFocused(this, flag);
}

void Node::setActive(bool flag)
{
    document().userActionElements().setActive(this, flag);
}

void Node::setHovered(bool flag)
{
    document().userActionElements().setHovered(this, flag);
}

bool Node::isUserActionElementActive() const
{
    DCHECK(isUserActionElement());
    return document().userActionElements().isActive(this);
}

bool Node::isUserActionElementInActiveChain() const
{
    DCHECK(isUserActionElement());
    return document().userActionElements().isInActiveChain(this);
}

bool Node::isUserActionElementHovered() const
{
    DCHECK(isUserActionElement());
    return document().userActionElements().isHovered(this);
}

bool Node::isUserActionElementFocused() const
{
    DCHECK(isUserActionElement());
    return document().userActionElements().isFocused(this);
}

std::ostream& operator<<(std::ostream& os, CustomElementState state)
{
    switch (state) {
    case CustomElementState::Uncustomized: return os << "Uncustomized";
    case CustomElementState::Undefined: return os << "Undefined";
    case CustomElementState::Custom: return os << "Custom";
    default: NOTREACHED();
    }
    return os;
}

CustomElementState Node::getCustomElementState() const
{
    return !isCustomElement()
        ? CustomElementState::Uncustomized
        : (getFlag(CustomElementCustomFlag) ? CustomElementState::Custom : CustomElementState::Undefined);
}

void Node::setCustomElementState(CustomElementState newState)
{
    CustomElementState oldState = getCustomElementState();

    switch (newState) {
    case CustomElementState::Uncustomized:
        NOTREACHED(); // Everything starts in this state
        return;

    case CustomElementState::Undefined:
        DCHECK_EQ(CustomElementState::Uncustomized, oldState);
        break;

    case CustomElementState::Custom:
        DCHECK_EQ(CustomElementState::Undefined, oldState);
        break;
    }

    DCHECK(isHTMLElement());
    DCHECK_NE(V0Upgraded, getV0CustomElementState());
#if DCHECK_IS_ON()
    bool wasDefined = toElement(this)->isDefined();
#endif

    setFlag(CustomElementFlag);
    if (newState == CustomElementState::Custom)
        setFlag(CustomElementCustomFlag);
    DCHECK(newState == getCustomElementState());

    // When the state goes from Uncustomized to Undefined, and then to Custom,
    // isDefined is always flipped.
#if DCHECK_IS_ON()
    DCHECK_NE(wasDefined, toElement(this)->isDefined());
#endif
    toElement(this)->pseudoStateChanged(CSSSelector::PseudoDefined);
}

void Node::setV0CustomElementState(V0CustomElementState newState)
{
    V0CustomElementState oldState = getV0CustomElementState();

    switch (newState) {
    case V0NotCustomElement:
        ASSERT_NOT_REACHED(); // Everything starts in this state
        return;

    case V0WaitingForUpgrade:
        DCHECK_EQ(V0NotCustomElement, oldState);
        break;

    case V0Upgraded:
        DCHECK_EQ(V0WaitingForUpgrade, oldState);
        break;
    }

    DCHECK(isHTMLElement() || isSVGElement());
    DCHECK(CustomElementState::Custom != getCustomElementState());
    setFlag(V0CustomElementFlag);
    setFlag(newState == V0Upgraded, V0CustomElementUpgradedFlag);

    if (oldState == V0NotCustomElement || newState == V0Upgraded)
        toElement(this)->pseudoStateChanged(CSSSelector::PseudoUnresolved);
}

void Node::checkSlotChange()
{
    // Common check logic is used in both cases, "after inserted" and "before removed".
    if (!isSlotable())
        return;
    if (ShadowRoot* root = v1ShadowRootOfParent()) {
        // Relevant DOM Standard:
        // https://dom.spec.whatwg.org/#concept-node-insert
        // - 6.1.2: If parent is a shadow host and node is a slotable, then assign a slot for node.
        // https://dom.spec.whatwg.org/#concept-node-remove
        // - 10. If node is assigned, then run assign slotables for node’s assigned slot.

        // Although DOM Standard requires "assign a slot for node / run assign slotables" at this timing,
        // we skip it as an optimization.
        if (HTMLSlotElement* slot = root->ensureSlotAssignment().findSlot(*this))
            slot->enqueueSlotChangeEvent();
    } else {
        // Relevant DOM Standard:
        // https://dom.spec.whatwg.org/#concept-node-insert
        // - 6.1.3: If parent is a slot whose assigned nodes is the empty list, then run signal a slot change for parent.
        // https://dom.spec.whatwg.org/#concept-node-remove
        // - 11. If parent is a slot whose assigned nodes is the empty list, then run signal a slot change for parent.
        Element* parent = parentElement();
        if (parent && isHTMLSlotElement(parent)) {
            HTMLSlotElement& parentSlot = toHTMLSlotElement(*parent);
            if (ShadowRoot* root = containingShadowRoot()) {
                if (root && root->isV1() && !parentSlot.hasAssignedNodesSlow())
                    parentSlot.enqueueSlotChangeEvent();
            }
        }
    }
}

DEFINE_TRACE(Node)
{
    visitor->trace(m_parentOrShadowHostNode);
    visitor->trace(m_previous);
    visitor->trace(m_next);
    // rareData() and m_data.m_layoutObject share their storage. We have to trace
    // only one of them.
    if (hasRareData())
        visitor->trace(rareData());

    visitor->trace(m_treeScope);
    EventTarget::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(Node)
{
    visitor->traceWrappers(parentOrShadowHostOrTemplateHostNode());
    visitor->traceWrappers(m_previous);
    visitor->traceWrappers(m_next);
    if (hasRareData())
        visitor->traceWrappers(rareData());
    EventTarget::traceWrappers(visitor);
}

unsigned Node::lengthOfContents() const
{
    // This switch statement must be consistent with that of Range::processContentsBetweenOffsets.
    switch (getNodeType()) {
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
    case Node::PROCESSING_INSTRUCTION_NODE:
        return toCharacterData(this)->length();
    case Node::ELEMENT_NODE:
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        return toContainerNode(this)->countChildren();
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_TYPE_NODE:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

v8::Local<v8::Object> Node::wrap(v8::Isolate* isolate, v8::Local<v8::Object> creationContext)
{
    DCHECK(!DOMDataStore::containsWrapper(this, isolate));

    const WrapperTypeInfo* wrapperType = wrapperTypeInfo();

    v8::Local<v8::Object> wrapper = V8DOMWrapper::createWrapper(isolate, creationContext, wrapperType);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    wrapperType->installConditionallyEnabledProperties(wrapper, isolate);
    return associateWithWrapper(isolate, wrapperType, wrapper);
}

v8::Local<v8::Object> Node::associateWithWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperType, v8::Local<v8::Object> wrapper)
{
    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperType, wrapper);
}

} // namespace blink

#ifndef NDEBUG

void showNode(const blink::Node* node)
{
    if (node)
        node->showNode("");
    else
        fprintf(stderr, "Cannot showNode for (nil)\n");
}

void showTree(const blink::Node* node)
{
    if (node)
        node->showTreeForThis();
    else
        fprintf(stderr, "Cannot showTree for (nil)\n");
}

void showNodePath(const blink::Node* node)
{
    if (node)
        node->showNodePathForThis();
    else
        fprintf(stderr, "Cannot showNodePath for (nil)\n");
}

#endif
