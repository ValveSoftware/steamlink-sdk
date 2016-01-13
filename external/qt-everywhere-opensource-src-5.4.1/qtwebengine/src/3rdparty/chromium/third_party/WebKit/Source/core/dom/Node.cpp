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

#include "config.h"
#include "core/dom/Node.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/HTMLNames.h"
#include "core/XMLNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/Attr.h"
#include "core/dom/Attribute.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ChildNodeList.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/LiveNodeList.h"
#include "core/dom/NoEventDispatchAssertion.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeRenderingTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/Range.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/TemplateContentDocumentFragment.h"
#include "core/dom/Text.h"
#include "core/dom/TreeScopeAdopter.h"
#include "core/dom/UserActionElementSet.h"
#include "core/dom/WeakNodeMap.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/events/Event.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/EventDispatcher.h"
#include "core/events/EventListener.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/MutationEvent.h"
#include "core/events/TextEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/UIEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/page/ContextMenuController.h"
#include "core/page/EventHandler.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/RenderBox.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/Partitions.h"
#include "platform/TraceEvent.h"
#include "platform/TracedValue.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

using namespace HTMLNames;

#if !ENABLE(OILPAN)
void* Node::operator new(size_t size)
{
    ASSERT(isMainThread());
    return partitionAlloc(Partitions::getObjectModelPartition(), size);
}

void Node::operator delete(void* ptr)
{
    ASSERT(isMainThread());
    partitionFree(ptr);
}
#endif

#if DUMP_NODE_STATISTICS
static HashSet<Node*>& liveNodeSet()
{
    DEFINE_STATIC_LOCAL(HashSet<Node*>, s_liveNodeSet, ());
    return s_liveNodeSet;
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

    for (HashSet<Node*>::iterator it = liveNodeSet().begin(); it != liveNodeSet().end(); ++it) {
        Node* node = *it;

        if (node->hasRareData()) {
            ++nodesWithRareData;
            if (node->isElementNode()) {
                ++elementsWithRareData;
                if (toElement(node)->hasNamedNodeMap())
                    ++elementsWithNamedNodeMap;
            }
        }

        switch (node->nodeType()) {
            case ELEMENT_NODE: {
                ++elementNodes;

                // Tag stats
                Element* element = toElement(node);
                HashMap<String, size_t>::AddResult result = perTagCount.add(element->tagName(), 1);
                if (!result.isNewEntry)
                    result.storedValue->value++;

                if (const ElementData* elementData = element->elementData()) {
                    attributes += elementData->length();
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
    for (HashMap<String, size_t>::iterator it = perTagCount.begin(); it != perTagCount.end(); ++it)
        printf("  Number of <%s> tags: %zu\n", it->key.utf8().data(), it->value);

    printf("Attributes:\n");
    printf("  Number of Attributes (non-Node and Node): %zu [%zu]\n", attributes, sizeof(Attribute));
    printf("  Number of Elements with attribute storage: %zu [%zu]\n", elementsWithAttributeStorage, sizeof(ElementData));
    printf("  Number of Elements with RareData: %zu\n", elementsWithRareData);
    printf("  Number of Elements with NamedNodeMap: %zu [%zu]\n", elementsWithNamedNodeMap, sizeof(NamedNodeMap));
#endif
}

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, nodeCounter, ("WebCoreNode"));

void Node::trackForDebugging()
{
#ifndef NDEBUG
    nodeCounter.increment();
#endif

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
    ASSERT(m_treeScope || type == CreateDocument || type == CreateShadowRoot);
    ScriptWrappable::init(this);
#if !ENABLE(OILPAN)
    if (m_treeScope)
        m_treeScope->guardRef();
#endif

#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
    trackForDebugging();
#endif
    InspectorCounters::incrementCounter(InspectorCounters::NodeCounter);
}

Node::~Node()
{
#ifndef NDEBUG
    nodeCounter.decrement();
#endif

#if DUMP_NODE_STATISTICS
    liveNodeSet().remove(this);
#endif

#if !ENABLE(OILPAN)
    if (hasRareData())
        clearRareData();

    RELEASE_ASSERT(!renderer());

    if (!isContainerNode())
        willBeDeletedFromDocument();

    if (m_previous)
        m_previous->setNextSibling(0);
    if (m_next)
        m_next->setPreviousSibling(0);

    if (m_treeScope)
        m_treeScope->guardDeref();
#else
    // With Oilpan, the rare data finalizer also asserts for
    // this condition (we cannot directly access it here.)
    RELEASE_ASSERT(hasRareData() || !renderer());
#endif

    InspectorCounters::decrementCounter(InspectorCounters::NodeCounter);

    if (getFlag(HasWeakReferencesFlag))
        WeakNodeMap::notifyNodeDestroyed(this);
}

#if !ENABLE(OILPAN)
// With Oilpan all of this is handled with weak processing of the document.
void Node::willBeDeletedFromDocument()
{
    if (!isTreeScopeInitialized())
        return;

    Document& document = this->document();

    if (hasEventTargetData()) {
        clearEventTargetData();
        document.didClearTouchEventHandlers(this);
        if (document.frameHost())
            document.frameHost()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);
    }

    if (AXObjectCache* cache = document.existingAXObjectCache())
        cache->remove(this);

    document.markers().removeMarkers(this);
}
#endif

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
        m_data.m_rareData = ElementRareData::create(m_data.m_renderer);
    else
        m_data.m_rareData = NodeRareData::create(m_data.m_renderer);

    ASSERT(m_data.m_rareData);

    setFlag(HasRareDataFlag);
    return *rareData();
}

#if !ENABLE(OILPAN)
void Node::clearRareData()
{
    ASSERT(hasRareData());
    ASSERT(!transientMutationObserverRegistry() || transientMutationObserverRegistry()->isEmpty());

    RenderObject* renderer = m_data.m_rareData->renderer();
    if (isElementNode())
        delete static_cast<ElementRareData*>(m_data.m_rareData);
    else
        delete static_cast<NodeRareData*>(m_data.m_rareData);
    m_data.m_renderer = renderer;
    clearFlag(HasRareDataFlag);
}
#endif

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

PassRefPtrWillBeRawPtr<NodeList> Node::childNodes()
{
    if (isContainerNode())
        return ensureRareData().ensureNodeLists().ensureChildNodeList(toContainerNode(*this));
    return ensureRareData().ensureNodeLists().ensureEmptyChildNodeList(*this);
}

Node& Node::lastDescendantOrSelf() const
{
    Node* n = const_cast<Node*>(this);
    while (n && n->lastChild())
        n = n->lastChild();
    ASSERT(n);
    return *n;
}

Node* Node::pseudoAwarePreviousSibling() const
{
    if (parentElement() && !previousSibling()) {
        Element* parent = parentElement();
        if (isAfterPseudoElement() && parent->lastChild())
            return parent->lastChild();
        if (!isBeforePseudoElement())
            return parent->pseudoElement(BEFORE);
    }
    return previousSibling();
}

Node* Node::pseudoAwareNextSibling() const
{
    if (parentElement() && !nextSibling()) {
        Element* parent = parentElement();
        if (isBeforePseudoElement() && parent->firstChild())
            return parent->firstChild();
        if (!isAfterPseudoElement())
            return parent->pseudoElement(AFTER);
    }
    return nextSibling();
}

Node* Node::pseudoAwareFirstChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* first = currentElement->pseudoElement(BEFORE);
        if (first)
            return first;
        first = currentElement->firstChild();
        if (!first)
            first = currentElement->pseudoElement(AFTER);
        return first;
    }

    return firstChild();
}

Node* Node::pseudoAwareLastChild() const
{
    if (isElementNode()) {
        const Element* currentElement = toElement(this);
        Node* last = currentElement->pseudoElement(AFTER);
        if (last)
            return last;
        last = currentElement->lastChild();
        if (!last)
            last = currentElement->pseudoElement(BEFORE);
        return last;
    }

    return lastChild();
}

void Node::insertBefore(PassRefPtrWillBeRawPtr<Node> newChild, Node* refChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        toContainerNode(this)->insertBefore(newChild, refChild, exceptionState);
    else
        exceptionState.throwDOMException(HierarchyRequestError, "This node type does not support this method.");
}

void Node::replaceChild(PassRefPtrWillBeRawPtr<Node> newChild, Node* oldChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        toContainerNode(this)->replaceChild(newChild, oldChild, exceptionState);
    else
        exceptionState.throwDOMException(HierarchyRequestError,  "This node type does not support this method.");
}

void Node::removeChild(Node* oldChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        toContainerNode(this)->removeChild(oldChild, exceptionState);
    else
        exceptionState.throwDOMException(NotFoundError, "This node type does not support this method.");
}

void Node::appendChild(PassRefPtrWillBeRawPtr<Node> newChild, ExceptionState& exceptionState)
{
    if (isContainerNode())
        toContainerNode(this)->appendChild(newChild, exceptionState);
    else
        exceptionState.throwDOMException(HierarchyRequestError, "This node type does not support this method.");
}

void Node::remove(ExceptionState& exceptionState)
{
    if (ContainerNode* parent = parentNode())
        parent->removeChild(this, exceptionState);
}

void Node::normalize()
{
    // Go through the subtree beneath us, normalizing all nodes. This means that
    // any two adjacent text nodes are merged and any empty text nodes are removed.

    RefPtrWillBeRawPtr<Node> node = this;
    while (Node* firstChild = node->firstChild())
        node = firstChild;
    while (node) {
        if (node->isElementNode())
            toElement(node)->normalizeAttributes();

        if (node == this)
            break;

        if (node->nodeType() == TEXT_NODE)
            node = toText(node)->mergeNextSiblingNodesIfPossible();
        else
            node = NodeTraversal::nextPostOrder(*node);
    }
}

const AtomicString& Node::localName() const
{
    return nullAtom;
}

const AtomicString& Node::namespaceURI() const
{
    return nullAtom;
}

bool Node::isContentEditable(UserSelectAllTreatment treatment)
{
    document().updateRenderTreeIfNeeded();
    return rendererIsEditable(Editable, treatment);
}

bool Node::isContentRichlyEditable()
{
    document().updateRenderTreeIfNeeded();
    return rendererIsEditable(RichlyEditable, UserSelectAllIsAlwaysNonEditable);
}

bool Node::rendererIsEditable(EditableLevel editableLevel, UserSelectAllTreatment treatment) const
{
    if (isPseudoElement())
        return false;

    // Ideally we'd call ASSERT(!needsStyleRecalc()) here, but
    // ContainerNode::setFocus() calls setNeedsStyleRecalc(), so the assertion
    // would fire in the middle of Document::setFocusedNode().

    for (const Node* node = this; node; node = node->parentNode()) {
        if ((node->isHTMLElement() || node->isDocumentNode()) && node->renderer()) {
            // Elements with user-select: all style are considered atomic
            // therefore non editable.
            if (Position::nodeIsUserSelectAll(node) && treatment == UserSelectAllIsAlwaysNonEditable)
                return false;
            switch (node->renderer()->style()->userModify()) {
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
    if (rendererIsEditable(editableLevel))
        return true;

    // FIXME: Respect editableLevel for ARIA editable elements.
    if (editableLevel == RichlyEditable)
        return false;

    ASSERT(AXObjectCache::accessibilityEnabled());
    ASSERT(document().existingAXObjectCache());

    if (AXObjectCache* cache = document().existingAXObjectCache())
        return cache->rootAXEditableElement(this);

    return false;
}

bool Node::shouldUseInputMethod()
{
    return isContentEditable(UserSelectAllIsAlwaysNonEditable);
}

RenderBox* Node::renderBox() const
{
    RenderObject* renderer = this->renderer();
    return renderer && renderer->isBox() ? toRenderBox(renderer) : 0;
}

RenderBoxModelObject* Node::renderBoxModelObject() const
{
    RenderObject* renderer = this->renderer();
    return renderer && renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;
}

LayoutRect Node::boundingBox() const
{
    if (renderer())
        return renderer()->absoluteBoundingBoxRect();
    return LayoutRect();
}

bool Node::hasNonEmptyBoundingBox() const
{
    // Before calling absoluteRects, check for the common case where the renderer
    // is non-empty, since this is a faster check and almost always returns true.
    RenderBoxModelObject* box = renderBoxModelObject();
    if (!box)
        return false;
    if (!box->borderBoundingBox().isEmpty())
        return true;

    Vector<IntRect> rects;
    FloatPoint absPos = renderer()->localToAbsolute();
    renderer()->absoluteRects(rects, flooredLayoutPoint(absPos));
    size_t n = rects.size();
    for (size_t i = 0; i < n; ++i)
        if (!rects[i].isEmpty())
            return true;

    return false;
}

#ifndef NDEBUG
inline static ShadowRoot* oldestShadowRootFor(const Node* node)
{
    if (!node->isElementNode())
        return 0;
    if (ElementShadow* shadow = toElement(node)->shadow())
        return shadow->oldestShadowRoot();
    return 0;
}
#endif

void Node::recalcDistribution()
{
    if (isElementNode()) {
        if (ElementShadow* shadow = toElement(this)->shadow())
            shadow->distributeIfNeeded();
    }

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
    setFlag(NeedsStyleInvalidationFlag);
    markAncestorsWithChildNeedsStyleInvalidation();
}

void Node::markAncestorsWithChildNeedsStyleInvalidation()
{
    for (Node* node = parentOrShadowHostNode(); node && !node->childNeedsStyleInvalidation(); node = node->parentOrShadowHostNode())
        node->setChildNeedsStyleInvalidation();
    document().scheduleRenderTreeUpdateIfNeeded();
}

void Node::markAncestorsWithChildNeedsDistributionRecalc()
{
    for (Node* node = this; node && !node->childNeedsDistributionRecalc(); node = node->parentOrShadowHostNode())
        node->setChildNeedsDistributionRecalc();
    document().scheduleRenderTreeUpdateIfNeeded();
}

namespace {

PassRefPtr<JSONArray> jsStackAsJSONArray()
{
    RefPtr<JSONArray> jsonArray = JSONArray::create();
    RefPtrWillBeRawPtr<ScriptCallStack> stack = createScriptCallStack(10);
    if (!stack)
        return jsonArray.release();
    for (size_t i = 0; i < stack->size(); i++)
        jsonArray->pushString(stack->at(i).functionName());
    return jsonArray.release();
}

PassRefPtr<JSONObject> jsonObjectForStyleInvalidation(unsigned nodeCount, const Node* rootNode)
{
    RefPtr<JSONObject> jsonObject = JSONObject::create();
    jsonObject->setNumber("node_count", nodeCount);
    jsonObject->setString("root_node", rootNode->debugName());
    jsonObject->setArray("js_stack", jsStackAsJSONArray());
    return jsonObject.release();
}

} // anonymous namespace'd functions supporting traceStyleChange

unsigned Node::styledSubtreeSize() const
{
    unsigned nodeCount = 0;

    for (const Node* node = this; node; node = NodeTraversal::next(*node, this)) {
        if (node->isTextNode() || node->isElementNode())
            nodeCount++;
        for (ShadowRoot* root = node->youngestShadowRoot(); root; root = root->olderShadowRoot())
            nodeCount += root->styledSubtreeSize();
    }

    return nodeCount;
}

void Node::traceStyleChange(StyleChangeType changeType)
{
    static const unsigned kMinLoggedSize = 100;
    unsigned nodeCount = styledSubtreeSize();
    if (nodeCount < kMinLoggedSize)
        return;

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("style.debug"),
        "Node::setNeedsStyleRecalc",
        "data", TracedValue::fromJSONValue(jsonObjectForStyleInvalidation(nodeCount, this))
    );
}

void Node::traceStyleChangeIfNeeded(StyleChangeType changeType)
{
    // TRACE_EVENT_CATEGORY_GROUP_ENABLED macro loads a global static bool into our local bool.
    bool styleTracingEnabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("style.debug"), &styleTracingEnabled);
    if (UNLIKELY(styleTracingEnabled))
        traceStyleChange(changeType);
}

inline void Node::setStyleChange(StyleChangeType changeType)
{
    m_nodeFlags = (m_nodeFlags & ~StyleChangeMask) | changeType;
}

void Node::markAncestorsWithChildNeedsStyleRecalc()
{
    for (ContainerNode* p = parentOrShadowHostNode(); p && !p->childNeedsStyleRecalc(); p = p->parentOrShadowHostNode())
        p->setChildNeedsStyleRecalc();
    document().scheduleRenderTreeUpdateIfNeeded();
}

void Node::setNeedsStyleRecalc(StyleChangeType changeType)
{
    ASSERT(changeType != NoStyleChange);
    if (!inActiveDocument())
        return;

    StyleChangeType existingChangeType = styleChangeType();
    if (changeType > existingChangeType) {
        setStyleChange(changeType);
        if (changeType >= SubtreeStyleChange)
            traceStyleChangeIfNeeded(changeType);
    }

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
    return inDocument() && document().isActive();
}

Node* Node::focusDelegate()
{
    return this;
}

bool Node::shouldHaveFocusAppearance() const
{
    ASSERT(focused());
    return true;
}

bool Node::isInert() const
{
    const HTMLDialogElement* dialog = document().activeModalDialog();
    if (dialog && this != document() && !NodeRenderingTraversal::contains(dialog, this))
        return true;
    return document().ownerElement() && document().ownerElement()->isInert();
}

unsigned Node::nodeIndex() const
{
    Node *_tempNode = previousSibling();
    unsigned count=0;
    for ( count=0; _tempNode; count++ )
        _tempNode = _tempNode->previousSibling();
    return count;
}

void Node::invalidateNodeListCachesInAncestors(const QualifiedName* attrName, Element* attributeOwnerElement)
{
    if (hasRareData() && (!attrName || isAttributeNode())) {
        if (NodeListsNodeData* lists = rareData()->nodeLists())
            lists->clearChildNodeListCache();
    }

    // Modifications to attributes that are not associated with an Element can't invalidate NodeList caches.
    if (attrName && !attributeOwnerElement)
        return;

    if (!document().shouldInvalidateNodeListCaches(attrName))
        return;

    document().invalidateNodeListCaches(attrName);

    for (Node* node = this; node; node = node->parentNode()) {
        if (NodeListsNodeData* lists = node->nodeLists())
            lists->invalidateCaches(attrName);
    }
}

NodeListsNodeData* Node::nodeLists()
{
    return hasRareData() ? rareData()->nodeLists() : 0;
}

void Node::clearNodeLists()
{
    rareData()->clearNodeLists();
}

bool Node::isDescendantOf(const Node *other) const
{
    // Return true if other is an ancestor of this, otherwise false
    if (!other || !other->hasChildren() || inDocument() != other->inDocument())
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

bool Node::containsIncludingShadowDOM(const Node* node) const
{
    if (!node)
        return false;

    if (this == node)
        return true;

    if (document() != node->document())
        return false;

    if (inDocument() != node->inDocument())
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

Node* Node::commonAncestor(const Node& other, Node* (*parent)(const Node&))
{
    if (this == other)
        return this;
    if (document() != other.document())
        return 0;
    int thisDepth = 0;
    for (Node* node = this; node; node = parent(*node)) {
        if (node == &other)
            return node;
        thisDepth++;
    }
    int otherDepth = 0;
    for (const Node* node = &other; node; node = parent(*node)) {
        if (node == this)
            return this;
        otherDepth++;
    }
    Node* thisIterator = this;
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
            return thisIterator;
        thisIterator = parent(*thisIterator);
        otherIterator = parent(*otherIterator);
    }
    ASSERT(!otherIterator);
    return 0;
}

void Node::reattach(const AttachContext& context)
{
    AttachContext reattachContext(context);
    reattachContext.performingReattach = true;

    // We only need to detach if the node has already been through attach().
    if (styleChangeType() < NeedsReattachStyleChange)
        detach(reattachContext);
    attach(reattachContext);
}

void Node::attach(const AttachContext&)
{
    ASSERT(document().inStyleRecalc() || isDocumentNode());
    ASSERT(needsAttach());
    ASSERT(!renderer() || (renderer()->style() && (renderer()->parent() || renderer()->isRenderView())));

    clearNeedsStyleRecalc();

    if (AXObjectCache* cache = document().axObjectCache())
        cache->updateCacheAfterNodeIsAttached(this);
}

#ifndef NDEBUG
static Node* detachingNode;

bool Node::inDetach() const
{
    return detachingNode == this;
}
#endif

void Node::detach(const AttachContext& context)
{
    ASSERT(document().lifecycle().stateAllowsDetach());
    DocumentLifecycle::DetachScope willDetach(document().lifecycle());

#ifndef NDEBUG
    ASSERT(!detachingNode);
    detachingNode = this;
#endif

    if (renderer())
        renderer()->destroyAndCleanupAnonymousWrappers();
    setRenderer(0);

    // Do not remove the element's hovered and active status
    // if performing a reattach.
    if (!context.performingReattach) {
        Document& doc = document();
        if (isUserActionElement()) {
            if (hovered())
                doc.hoveredNodeDetached(this);
            if (inActiveChain())
                doc.activeChainNodeDetached(this);
            doc.userActionElements().didDetach(this);
        }
    }

    setStyleChange(NeedsReattachStyleChange);
    setChildNeedsStyleRecalc();

    if (StyleResolver* resolver = document().styleResolver())
        resolver->ruleFeatureSet().styleInvalidator().clearInvalidation(*this);
    clearChildNeedsStyleInvalidation();
    clearNeedsStyleInvalidation();

#ifndef NDEBUG
    detachingNode = 0;
#endif
}

void Node::reattachWhitespaceSiblings(Text* start)
{
    for (Node* sibling = start; sibling; sibling = sibling->nextSibling()) {
        if (sibling->isTextNode() && toText(sibling)->containsOnlyWhitespace()) {
            bool hadRenderer = !!sibling->renderer();
            sibling->reattach();
            // If the reattach didn't toggle the visibility of the whitespace we don't
            // need to continue reattaching siblings since they won't toggle visibility
            // either.
            if (hadRenderer == !!sibling->renderer())
                return;
        } else if (sibling->renderer()) {
            return;
        }
    }
}

// FIXME: This code is used by editing.  Seems like it could move over there and not pollute Node.
Node *Node::previousNodeConsideringAtomicNodes() const
{
    if (previousSibling()) {
        Node *n = previousSibling();
        while (!isAtomicNode(n) && n->lastChild())
            n = n->lastChild();
        return n;
    }
    else if (parentNode()) {
        return parentNode();
    }
    else {
        return 0;
    }
}

Node *Node::nextNodeConsideringAtomicNodes() const
{
    if (!isAtomicNode(this) && firstChild())
        return firstChild();
    if (nextSibling())
        return nextSibling();
    const Node *n = this;
    while (n && !n->nextSibling())
        n = n->parentNode();
    if (n)
        return n->nextSibling();
    return 0;
}

Node *Node::previousLeafNode() const
{
    Node *node = previousNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->previousNodeConsideringAtomicNodes();
    }
    return 0;
}

Node *Node::nextLeafNode() const
{
    Node *node = nextNodeConsideringAtomicNodes();
    while (node) {
        if (isAtomicNode(node))
            return node;
        node = node->nextNodeConsideringAtomicNodes();
    }
    return 0;
}

RenderStyle* Node::virtualComputedStyle(PseudoId pseudoElementSpecifier)
{
    return parentOrShadowHostNode() ? parentOrShadowHostNode()->computedStyle(pseudoElementSpecifier) : 0;
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
    if (rendererIsEditable())
        return true;

    if (renderer()) {
        RenderStyle* style = renderer()->style();
        // We allow selections to begin within an element that has -webkit-user-select: none set,
        // but if the element is draggable then dragging should take priority over selection.
        if (style->userDrag() == DRAG_ELEMENT && style->userSelect() == SELECT_NONE)
            return false;
    }
    return parentOrShadowHostNode() ? parentOrShadowHostNode()->canStartSelection() : true;
}

Element* Node::shadowHost() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->host();
    return 0;
}

Node* Node::deprecatedShadowAncestorNode() const
{
    if (ShadowRoot* root = containingShadowRoot())
        return root->host();

    return const_cast<Node*>(this);
}

ShadowRoot* Node::containingShadowRoot() const
{
    Node& root = treeScope().rootNode();
    return root.isShadowRoot() ? toShadowRoot(&root) : 0;
}

Node* Node::nonBoundaryShadowTreeRootNode()
{
    ASSERT(!isShadowRoot());
    Node* root = this;
    while (root) {
        if (root->isShadowRoot())
            return root;
        Node* parent = root->parentOrShadowHostNode();
        if (parent && parent->isShadowRoot())
            return root;
        root = parent;
    }
    return 0;
}

ContainerNode* Node::nonShadowBoundaryParentNode() const
{
    ContainerNode* parent = parentNode();
    return parent && !parent->isShadowRoot() ? parent : 0;
}

Element* Node::parentOrShadowHostElement() const
{
    ContainerNode* parent = parentOrShadowHostNode();
    if (!parent)
        return 0;

    if (parent->isShadowRoot())
        return toShadowRoot(parent)->host();

    if (!parent->isElementNode())
        return 0;

    return toElement(parent);
}

ContainerNode* Node::parentOrShadowHostOrTemplateHostNode() const
{
    if (isDocumentFragment() && toDocumentFragment(this)->isTemplateContent())
        return static_cast<const TemplateContentDocumentFragment*>(this)->host();
    return parentOrShadowHostNode();
}

bool Node::isBlockFlowElement() const
{
    return isElementNode() && renderer() && renderer()->isRenderBlockFlow();
}

Element *Node::enclosingBlockFlowElement() const
{
    Node *n = const_cast<Node *>(this);
    if (isBlockFlowElement())
        return toElement(n);

    while (1) {
        n = n->parentNode();
        if (!n)
            break;
        if (n->isBlockFlowElement() || isHTMLBodyElement(*n))
            return toElement(n);
    }
    return 0;
}

bool Node::isRootEditableElement() const
{
    return rendererIsEditable() && isElementNode() && (!parentNode() || !parentNode()->rendererIsEditable()
        || !parentNode()->isElementNode() || isHTMLBodyElement((*this)));
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
    Element* result = 0;
    for (Node* n = const_cast<Node*>(this); n && n->rendererIsEditable(); n = n->parentNode()) {
        if (n->isElementNode())
            result = toElement(n);
        if (isHTMLBodyElement(*n))
            break;
    }
    return result;
}

bool Node::inSameContainingBlockFlowElement(Node *n)
{
    return n ? enclosingBlockFlowElement() == n->enclosingBlockFlowElement() : false;
}

// FIXME: End of obviously misplaced HTML editing functions.  Try to move these out of Node.

Document* Node::ownerDocument() const
{
    Document* doc = &document();
    return doc == this ? 0 : doc;
}

KURL Node::baseURI() const
{
    return parentNode() ? parentNode()->baseURI() : KURL();
}

bool Node::isEqualNode(Node* other) const
{
    if (!other)
        return false;

    NodeType nodeType = this->nodeType();
    if (nodeType != other->nodeType())
        return false;

    if (nodeName() != other->nodeName())
        return false;

    if (localName() != other->localName())
        return false;

    if (namespaceURI() != other->namespaceURI())
        return false;

    if (nodeValue() != other->nodeValue())
        return false;

    if (isElementNode() && !toElement(this)->hasEquivalentAttributes(toElement(other)))
        return false;

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

    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element& element = toElement(*this);

            if (element.prefix().isNull())
                return element.namespaceURI() == namespaceURI;

            if (element.hasAttributes()) {
                AttributeCollection attributes = element.attributes();
                AttributeCollection::const_iterator end = attributes.end();
                for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
                    if (it->localName() == xmlnsAtom)
                        return it->value() == namespaceURI;
                }
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
    // http://dom.spec.whatwg.org/#dom-node-lookupprefix

    if (namespaceURI.isEmpty() || namespaceURI.isNull())
        return nullAtom;

    const Element* context;

    switch (nodeType()) {
        case ELEMENT_NODE:
            context = toElement(this);
            break;
        case DOCUMENT_NODE:
            context = toDocument(this)->documentElement();
            break;
        case DOCUMENT_FRAGMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            context = 0;
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

    switch (nodeType()) {
        case ELEMENT_NODE: {
            const Element& element = toElement(*this);

            if (!element.namespaceURI().isNull() && element.prefix() == prefix)
                return element.namespaceURI();

            if (element.hasAttributes()) {
                AttributeCollection attributes = element.attributes();
                AttributeCollection::const_iterator end = attributes.end();
                for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
                    if (it->prefix() == xmlnsAtom && it->localName() == prefix) {
                        if (!it->value().isEmpty())
                            return it->value();
                        return nullAtom;
                    }
                    if (it->localName() == xmlnsAtom && prefix.isNull()) {
                        if (!it->value().isEmpty())
                            return it->value();
                        return nullAtom;
                    }
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
            else
                return nullAtom;
        }
        default:
            if (Element* parent = parentElement())
                return parent->lookupNamespaceURI(prefix);
            return nullAtom;
    }
}

static void appendTextContent(const Node* node, bool convertBRsToNewlines, bool& isNullString, StringBuilder& content)
{
    switch (node->nodeType()) {
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
        isNullString = false;
        content.append(toCharacterData(node)->data());
        break;

    case Node::PROCESSING_INSTRUCTION_NODE:
        isNullString = false;
        content.append(toProcessingInstruction(node)->data());
        break;

    case Node::ELEMENT_NODE:
        if (isHTMLBRElement(*node) && convertBRsToNewlines) {
            isNullString = false;
            content.append('\n');
            break;
        }
    // Fall through.
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        isNullString = false;
        for (Node* child = toContainerNode(node)->firstChild(); child; child = child->nextSibling()) {
            Node::NodeType childNodeType = child->nodeType();
            if (childNodeType == Node::COMMENT_NODE || childNodeType == Node::PROCESSING_INSTRUCTION_NODE)
                continue;
            appendTextContent(child, convertBRsToNewlines, isNullString, content);
        }
        break;

    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_TYPE_NODE:
        break;
    }
}

String Node::textContent(bool convertBRsToNewlines) const
{
    StringBuilder content;
    bool isNullString = true;
    appendTextContent(this, convertBRsToNewlines, isNullString, content);
    return isNullString ? String() : content.toString();
}

void Node::setTextContent(const String& text)
{
    switch (nodeType()) {
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case COMMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
            setNodeValue(text);
            return;
        case ELEMENT_NODE:
        case ATTRIBUTE_NODE:
        case DOCUMENT_FRAGMENT_NODE: {
            // FIXME: Merge this logic into replaceChildrenWithText.
            RefPtrWillBeRawPtr<ContainerNode> container = toContainerNode(this);
            // No need to do anything if the text is identical.
            if (container->hasOneTextChild() && toText(container->firstChild())->data() == text)
                return;
            ChildListMutationScope mutation(*this);
            container->removeChildren();
            // Note: This API will not insert empty text nodes:
            // http://dom.spec.whatwg.org/#dom-node-textcontent
            if (!text.isEmpty())
                container->appendChild(document().createTextNode(text), ASSERT_NO_EXCEPTION);
            return;
        }
        case DOCUMENT_NODE:
        case DOCUMENT_TYPE_NODE:
            // Do nothing.
            return;
    }
    ASSERT_NOT_REACHED();
}

bool Node::offsetInCharacters() const
{
    return false;
}

unsigned short Node::compareDocumentPosition(const Node* otherNode) const
{
    return compareDocumentPositionInternal(otherNode, TreatShadowTreesAsDisconnected);
}

unsigned short Node::compareDocumentPositionInternal(const Node* otherNode, ShadowTreesTreatment treatment) const
{
    // It is not clear what should be done if |otherNode| is 0.
    if (!otherNode)
        return DOCUMENT_POSITION_DISCONNECTED;

    if (otherNode == this)
        return DOCUMENT_POSITION_EQUIVALENT;

    const Attr* attr1 = nodeType() == ATTRIBUTE_NODE ? toAttr(this) : 0;
    const Attr* attr2 = otherNode->nodeType() == ATTRIBUTE_NODE ? toAttr(otherNode) : 0;

    const Node* start1 = attr1 ? attr1->ownerElement() : this;
    const Node* start2 = attr2 ? attr2->ownerElement() : otherNode;

    // If either of start1 or start2 is null, then we are disconnected, since one of the nodes is
    // an orphaned attribute node.
    if (!start1 || !start2) {
        unsigned short direction = (this > otherNode) ? DOCUMENT_POSITION_PRECEDING : DOCUMENT_POSITION_FOLLOWING;
        return DOCUMENT_POSITION_DISCONNECTED | DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | direction;
    }

    Vector<const Node*, 16> chain1;
    Vector<const Node*, 16> chain2;
    if (attr1)
        chain1.append(attr1);
    if (attr2)
        chain2.append(attr2);

    if (attr1 && attr2 && start1 == start2 && start1) {
        // We are comparing two attributes on the same node. Crawl our attribute map and see which one we hit first.
        const Element* owner1 = attr1->ownerElement();
        owner1->synchronizeAllAttributes();
        AttributeCollection attributes = owner1->attributes();
        AttributeCollection::const_iterator end = attributes.end();
        for (AttributeCollection::const_iterator it = attributes.begin(); it != end; ++it) {
            // If neither of the two determining nodes is a child node and nodeType is the same for both determining nodes, then an
            // implementation-dependent order between the determining nodes is returned. This order is stable as long as no nodes of
            // the same nodeType are inserted into or removed from the direct container. This would be the case, for example,
            // when comparing two attributes of the same element, and inserting or removing additional attributes might change
            // the order between existing attributes.
            if (attr1->qualifiedName() == it->name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_FOLLOWING;
            if (attr2->qualifiedName() == it->name())
                return DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC | DOCUMENT_POSITION_PRECEDING;
        }

        ASSERT_NOT_REACHED();
        return DOCUMENT_POSITION_DISCONNECTED;
    }

    // If one node is in the document and the other is not, we must be disconnected.
    // If the nodes have different owning documents, they must be disconnected.  Note that we avoid
    // comparing Attr nodes here, since they return false from inDocument() all the time (which seems like a bug).
    if (start1->inDocument() != start2->inDocument() || (treatment == TreatShadowTreesAsDisconnected && start1->treeScope() != start2->treeScope())) {
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
            if (child1->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_FOLLOWING | connection;
            if (child2->nodeType() == ATTRIBUTE_NODE)
                return DOCUMENT_POSITION_PRECEDING | connection;

            // If one of the children is a shadow root,
            if (child1->isShadowRoot() || child2->isShadowRoot()) {
                if (!child2->isShadowRoot())
                    return Node::DOCUMENT_POSITION_FOLLOWING | connection;
                if (!child1->isShadowRoot())
                    return Node::DOCUMENT_POSITION_PRECEDING | connection;

                for (ShadowRoot* child = toShadowRoot(child2)->olderShadowRoot(); child; child = child->olderShadowRoot())
                    if (child == child1)
                        return Node::DOCUMENT_POSITION_FOLLOWING | connection;

                return Node::DOCUMENT_POSITION_PRECEDING | connection;
            }

            if (!child2->nextSibling())
                return DOCUMENT_POSITION_FOLLOWING | connection;
            if (!child1->nextSibling())
                return DOCUMENT_POSITION_PRECEDING | connection;

            // Otherwise we need to see which node occurs first.  Crawl backwards from child2 looking for child1.
            for (Node* child = child2->previousSibling(); child; child = child->previousSibling()) {
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

FloatPoint Node::convertToPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->localToAbsolute(p, UseTransforms);

    // Otherwise go up the tree looking for a renderer
    if (Element* parent = parentElement())
        return parent->convertToPage(p);

    // No parent - no conversion needed
    return p;
}

FloatPoint Node::convertFromPage(const FloatPoint& p) const
{
    // If there is a renderer, just ask it to do the conversion
    if (renderer())
        return renderer()->absoluteToLocal(p, UseTransforms);

    // Otherwise go up the tree looking for a renderer
    if (Element* parent = parentElement())
        return parent->convertFromPage(p);

    // No parent - no conversion needed
    return p;
}

String Node::debugName() const
{
    StringBuilder name;
    name.append(nodeName());

    if (hasID()) {
        name.appendLiteral(" id=\'");
        name.append(toElement(this)->getIdAttribute());
        name.append('\'');
    }

    if (hasClass()) {
        name.appendLiteral(" class=\'");
        for (size_t i = 0; i < toElement(this)->classNames().size(); ++i) {
            if (i > 0)
                name.append(' ');
            name.append(toElement(this)->classNames()[i]);
        }
        name.append('\'');
    }

    return name.toString();
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
        value.replaceWithLiteral('\\', "\\\\");
        value.replaceWithLiteral('\n', "\\n");
        fprintf(stderr, "%s%s\t%p \"%s\"\n", prefix, nodeName().utf8().data(), this, value.utf8().data());
    } else {
        StringBuilder attrs;
        appendAttributeDesc(this, attrs, idAttr, " ID");
        appendAttributeDesc(this, attrs, classAttr, " CLASS");
        appendAttributeDesc(this, attrs, styleAttr, " STYLE");
        fprintf(stderr, "%s%s\t%p%s\n", prefix, nodeName().utf8().data(), this, attrs.toString().utf8().data());
    }
}

void Node::showTreeForThis() const
{
    showTreeAndMark(this, "*");
}

void Node::showNodePathForThis() const
{
    Vector<const Node*, 16> chain;
    const Node* node = this;
    while (node->parentOrShadowHostNode()) {
        chain.append(node);
        node = node->parentOrShadowHostNode();
    }
    for (unsigned index = chain.size(); index > 0; --index) {
        const Node* node = chain[index - 1];
        if (node->isShadowRoot()) {
            int count = 0;
            for (ShadowRoot* shadowRoot = toShadowRoot(node)->olderShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
                ++count;
            fprintf(stderr, "/#shadow-root[%d]", count);
            continue;
        }

        switch (node->nodeType()) {
        case ELEMENT_NODE: {
            fprintf(stderr, "/%s", node->nodeName().utf8().data());

            const Element* element = toElement(node);
            const AtomicString& idattr = element->getIdAttribute();
            bool hasIdAttr = !idattr.isNull() && !idattr.isEmpty();
            if (node->previousSibling() || node->nextSibling()) {
                int count = 0;
                for (Node* previous = node->previousSibling(); previous; previous = previous->previousSibling())
                    if (previous->nodeName() == node->nodeName())
                        ++count;
                if (hasIdAttr)
                    fprintf(stderr, "[@id=\"%s\" and position()=%d]", idattr.utf8().data(), count);
                else
                    fprintf(stderr, "[%d]", count);
            } else if (hasIdAttr) {
                fprintf(stderr, "[@id=\"%s\"]", idattr.utf8().data());
            }
            break;
        }
        case TEXT_NODE:
            fprintf(stderr, "/text()");
            break;
        case ATTRIBUTE_NODE:
            fprintf(stderr, "/@%s", node->nodeName().utf8().data());
            break;
        default:
            break;
        }
    }
    fprintf(stderr, "\n");
}

static void traverseTreeAndMark(const String& baseIndent, const Node* rootNode, const Node* markedNode1, const char* markedLabel1, const Node* markedNode2, const char* markedLabel2)
{
    for (const Node* node = rootNode; node; node = NodeTraversal::next(*node)) {
        if (node == markedNode1)
            fprintf(stderr, "%s", markedLabel1);
        if (node == markedNode2)
            fprintf(stderr, "%s", markedLabel2);

        StringBuilder indent;
        indent.append(baseIndent);
        for (const Node* tmpNode = node; tmpNode && tmpNode != rootNode; tmpNode = tmpNode->parentOrShadowHostNode())
            indent.append('\t');
        fprintf(stderr, "%s", indent.toString().utf8().data());
        node->showNode();
        indent.append('\t');
        if (node->isShadowRoot()) {
            if (ShadowRoot* youngerShadowRoot = toShadowRoot(node)->youngerShadowRoot())
                traverseTreeAndMark(indent.toString(), youngerShadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
        } else if (ShadowRoot* oldestShadowRoot = oldestShadowRootFor(node))
            traverseTreeAndMark(indent.toString(), oldestShadowRoot, markedNode1, markedLabel1, markedNode2, markedLabel2);
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
    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        showSubTreeAcrossFrame(child, markedNode, indent + "\t");
}

void Node::showTreeForThisAcrossFrame() const
{
    Node* rootNode = const_cast<Node*>(this);
    while (parentOrShadowHostOrFrameOwner(rootNode))
        rootNode = parentOrShadowHostOrFrameOwner(rootNode);
    showSubTreeAcrossFrame(rootNode, this, "");
}

#endif

// --------

Node* Node::enclosingLinkEventParentOrSelf()
{
    for (Node* node = this; node; node = NodeRenderingTraversal::parent(node)) {
        // For imagemaps, the enclosing link node is the associated area element not the image itself.
        // So we don't let images be the enclosingLinkNode, even though isLink sometimes returns true
        // for them.
        if (node->isLink() && !isHTMLImageElement(*node))
            return node;
    }

    return 0;
}

const AtomicString& Node::interfaceName() const
{
    return EventTargetNames::Node;
}

ExecutionContext* Node::executionContext() const
{
    return document().contextDocument().get();
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

    if (AXObjectCache::accessibilityEnabled()) {
        if (AXObjectCache* cache = oldDocument.existingAXObjectCache())
            cache->remove(this);
    }

    oldDocument.markers().removeMarkers(this);
    oldDocument.updateRangesAfterNodeMovedToAnotherDocument(*this);

    if (const TouchEventTargetSet* touchHandlers = oldDocument.touchEventTargets()) {
        while (touchHandlers->contains(this)) {
            oldDocument.didRemoveTouchEventHandler(this);
            document().didAddTouchEventHandler(this);
        }
    }
    if (oldDocument.frameHost() != document().frameHost()) {
        if (oldDocument.frameHost())
            oldDocument.frameHost()->eventHandlerRegistry().didMoveOutOfFrameHost(*this);
        if (document().frameHost())
            document().frameHost()->eventHandlerRegistry().didMoveIntoFrameHost(*this);
    }

    if (WillBeHeapVector<OwnPtrWillBeMember<MutationObserverRegistration> >* registry = mutationObserverRegistry()) {
        for (size_t i = 0; i < registry->size(); ++i) {
            document().addMutationObserverTypes(registry->at(i)->mutationTypes());
        }
    }

    if (WillBeHeapHashSet<RawPtrWillBeMember<MutationObserverRegistration> >* transientRegistry = transientMutationObserverRegistry()) {
        for (WillBeHeapHashSet<RawPtrWillBeMember<MutationObserverRegistration> >::iterator iter = transientRegistry->begin(); iter != transientRegistry->end(); ++iter) {
            document().addMutationObserverTypes((*iter)->mutationTypes());
        }
    }
}

static inline bool tryAddEventListener(Node* targetNode, const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (!targetNode->EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    Document& document = targetNode->document();
    document.addListenerTypeIfNeeded(eventType);
    if (isTouchEventType(eventType))
        document.didAddTouchEventHandler(targetNode);
    if (document.frameHost())
        document.frameHost()->eventHandlerRegistry().didAddEventHandler(*targetNode, eventType);

    return true;
}

bool Node::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    return tryAddEventListener(this, eventType, listener, useCapture);
}

static inline bool tryRemoveEventListener(Node* targetNode, const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!targetNode->EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    // FIXME: Notify Document that the listener has vanished. We need to keep track of a number of
    // listeners for each type, not just a bool - see https://bugs.webkit.org/show_bug.cgi?id=33861
    Document& document = targetNode->document();
    if (isTouchEventType(eventType))
        document.didRemoveTouchEventHandler(targetNode);
    if (document.frameHost())
        document.frameHost()->eventHandlerRegistry().didRemoveEventHandler(*targetNode, eventType);

    return true;
}

bool Node::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    return tryRemoveEventListener(this, eventType, listener, useCapture);
}

void Node::removeAllEventListeners()
{
    if (hasEventListeners() && document().frameHost())
        document().frameHost()->eventHandlerRegistry().didRemoveAllEventHandlers(*this);
    EventTarget::removeAllEventListeners();
    document().didClearTouchEventHandlers(this);
}

void Node::removeAllEventListenersRecursively()
{
    for (Node* node = this; node; node = NodeTraversal::next(*node)) {
        node->removeAllEventListeners();
        for (ShadowRoot* root = node->youngestShadowRoot(); root; root = root->olderShadowRoot())
            root->removeAllEventListenersRecursively();
    }
}

typedef WillBeHeapHashMap<RawPtrWillBeWeakMember<Node>, OwnPtr<EventTargetData> > EventTargetDataMap;

static EventTargetDataMap& eventTargetDataMap()
{
#if ENABLE(OILPAN)
    DEFINE_STATIC_LOCAL(Persistent<EventTargetDataMap>, map, (new EventTargetDataMap()));
    return *map;
#else
    DEFINE_STATIC_LOCAL(EventTargetDataMap, map, ());
    return map;
#endif
}

EventTargetData* Node::eventTargetData()
{
    return hasEventTargetData() ? eventTargetDataMap().get(this) : 0;
}

EventTargetData& Node::ensureEventTargetData()
{
    if (hasEventTargetData())
        return *eventTargetDataMap().get(this);
    setHasEventTargetData(true);
    EventTargetData* data = new EventTargetData;
    eventTargetDataMap().set(this, adoptPtr(data));
    return *data;
}

#if !ENABLE(OILPAN)
void Node::clearEventTargetData()
{
    eventTargetDataMap().remove(this);
}
#endif

WillBeHeapVector<OwnPtrWillBeMember<MutationObserverRegistration> >* Node::mutationObserverRegistry()
{
    if (!hasRareData())
        return 0;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return 0;
    return &data->registry;
}

WillBeHeapHashSet<RawPtrWillBeMember<MutationObserverRegistration> >* Node::transientMutationObserverRegistry()
{
    if (!hasRareData())
        return 0;
    NodeMutationObserverData* data = rareData()->mutationObserverData();
    if (!data)
        return 0;
    return &data->transientRegistry;
}

template<typename Registry>
static inline void collectMatchingObserversForMutation(WillBeHeapHashMap<RawPtrWillBeMember<MutationObserver>, MutationRecordDeliveryOptions>& observers, Registry* registry, Node& target, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    if (!registry)
        return;
    for (typename Registry::iterator iter = registry->begin(); iter != registry->end(); ++iter) {
        const MutationObserverRegistration& registration = **iter;
        if (registration.shouldReceiveMutationFrom(target, type, attributeName)) {
            MutationRecordDeliveryOptions deliveryOptions = registration.deliveryOptions();
            WillBeHeapHashMap<RawPtrWillBeMember<MutationObserver>, MutationRecordDeliveryOptions>::AddResult result = observers.add(&registration.observer(), deliveryOptions);
            if (!result.isNewEntry)
                result.storedValue->value |= deliveryOptions;
        }
    }
}

void Node::getRegisteredMutationObserversOfType(WillBeHeapHashMap<RawPtrWillBeMember<MutationObserver>, MutationRecordDeliveryOptions>& observers, MutationObserver::MutationType type, const QualifiedName* attributeName)
{
    ASSERT((type == MutationObserver::Attributes && attributeName) || !attributeName);
    collectMatchingObserversForMutation(observers, mutationObserverRegistry(), *this, type, attributeName);
    collectMatchingObserversForMutation(observers, transientMutationObserverRegistry(), *this, type, attributeName);
    for (Node* node = parentNode(); node; node = node->parentNode()) {
        collectMatchingObserversForMutation(observers, node->mutationObserverRegistry(), *this, type, attributeName);
        collectMatchingObserversForMutation(observers, node->transientMutationObserverRegistry(), *this, type, attributeName);
    }
}

void Node::registerMutationObserver(MutationObserver& observer, MutationObserverOptions options, const HashSet<AtomicString>& attributeFilter)
{
    MutationObserverRegistration* registration = 0;
    WillBeHeapVector<OwnPtrWillBeMember<MutationObserverRegistration> >& registry = ensureRareData().ensureMutationObserverData().registry;
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
    WillBeHeapVector<OwnPtrWillBeMember<MutationObserverRegistration> >* registry = mutationObserverRegistry();
    ASSERT(registry);
    if (!registry)
        return;

    size_t index = registry->find(registration);
    ASSERT(index != kNotFound);
    if (index == kNotFound)
        return;

    // Deleting the registration may cause this node to be derefed, so we must make sure the Vector operation completes
    // before that, in case |this| is destroyed (see MutationObserverRegistration::m_registrationNodeKeepAlive).
    // FIXME: Simplify the registration/transient registration logic to make this understandable by humans.
    RefPtrWillBeRawPtr<Node> protect(this);
#if ENABLE(OILPAN)
    // The explicit dispose() is needed to have the registration
    // object unregister itself promptly.
    registration->dispose();
#endif
    registry->remove(index);
}

void Node::registerTransientMutationObserver(MutationObserverRegistration* registration)
{
    ensureRareData().ensureMutationObserverData().transientRegistry.add(registration);
}

void Node::unregisterTransientMutationObserver(MutationObserverRegistration* registration)
{
    WillBeHeapHashSet<RawPtrWillBeMember<MutationObserverRegistration> >* transientRegistry = transientMutationObserverRegistry();
    ASSERT(transientRegistry);
    if (!transientRegistry)
        return;

    ASSERT(transientRegistry->contains(registration));
    transientRegistry->remove(registration);
}

void Node::notifyMutationObserversNodeWillDetach()
{
    if (!document().hasMutationObservers())
        return;

    for (Node* node = parentNode(); node; node = node->parentNode()) {
        if (WillBeHeapVector<OwnPtrWillBeMember<MutationObserverRegistration> >* registry = node->mutationObserverRegistry()) {
            const size_t size = registry->size();
            for (size_t i = 0; i < size; ++i)
                registry->at(i)->observedSubtreeNodeWillDetach(*this);
        }

        if (WillBeHeapHashSet<RawPtrWillBeMember<MutationObserverRegistration> >* transientRegistry = node->transientMutationObserverRegistry()) {
            for (WillBeHeapHashSet<RawPtrWillBeMember<MutationObserverRegistration> >::iterator iter = transientRegistry->begin(); iter != transientRegistry->end(); ++iter)
                (*iter)->observedSubtreeNodeWillDetach(*this);
        }
    }
}

void Node::handleLocalEvents(Event* event)
{
    if (!hasEventTargetData())
        return;

    if (isDisabledFormControl(this) && event->isMouseEvent())
        return;

    fireEventListeners(event);
}

void Node::dispatchScopedEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    dispatchScopedEventDispatchMediator(EventDispatchMediator::create(event));
}

void Node::dispatchScopedEventDispatchMediator(PassRefPtrWillBeRawPtr<EventDispatchMediator> eventDispatchMediator)
{
    EventDispatcher::dispatchScopedEvent(this, eventDispatchMediator);
}

bool Node::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    if (event->isMouseEvent())
        return EventDispatcher::dispatchEvent(this, MouseEventDispatchMediator::create(static_pointer_cast<MouseEvent>(event), MouseEventDispatchMediator::SyntheticMouseEvent));
    if (event->isTouchEvent())
        return dispatchTouchEvent(static_pointer_cast<TouchEvent>(event));
    return EventDispatcher::dispatchEvent(this, EventDispatchMediator::create(event));
}

void Node::dispatchSubtreeModifiedEvent()
{
    if (isInShadowTree())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    if (!document().hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return;

    dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMSubtreeModified, true));
}

bool Node::dispatchDOMActivateEvent(int detail, PassRefPtrWillBeRawPtr<Event> underlyingEvent)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    RefPtrWillBeRawPtr<UIEvent> event = UIEvent::create(EventTypeNames::DOMActivate, true, true, document().domWindow(), detail);
    event->setUnderlyingEvent(underlyingEvent);
    dispatchScopedEvent(event);
    return event->defaultHandled();
}

bool Node::dispatchKeyEvent(const PlatformKeyboardEvent& event)
{
    return EventDispatcher::dispatchEvent(this, KeyboardEventDispatchMediator::create(KeyboardEvent::create(event, document().domWindow())));
}

bool Node::dispatchMouseEvent(const PlatformMouseEvent& event, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    return EventDispatcher::dispatchEvent(this, MouseEventDispatchMediator::create(MouseEvent::create(eventType, document().domWindow(), event, detail, relatedTarget)));
}

bool Node::dispatchGestureEvent(const PlatformGestureEvent& event)
{
    RefPtrWillBeRawPtr<GestureEvent> gestureEvent = GestureEvent::create(document().domWindow(), event);
    if (!gestureEvent.get())
        return false;
    return EventDispatcher::dispatchEvent(this, GestureEventDispatchMediator::create(gestureEvent));
}

bool Node::dispatchTouchEvent(PassRefPtrWillBeRawPtr<TouchEvent> event)
{
    return EventDispatcher::dispatchEvent(this, TouchEventDispatchMediator::create(event));
}

void Node::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions)
{
    EventDispatcher::dispatchSimulatedClick(this, underlyingEvent, eventOptions);
}

bool Node::dispatchWheelEvent(const PlatformWheelEvent& event)
{
    return EventDispatcher::dispatchEvent(this, WheelEventDispatchMediator::create(event, document().domWindow()));
}

void Node::dispatchInputEvent()
{
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
        if (dispatchDOMActivateEvent(detail, event))
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

            RenderObject* renderer = this->renderer();
            while (renderer && (!renderer->isBox() || !toRenderBox(renderer)->canBeScrolledAndHasScrollableArea()))
                renderer = renderer->parent();

            if (renderer) {
                if (LocalFrame* frame = document().frame())
                    frame->eventHandler().startPanScrolling(renderer);
            }
        }
#endif
    } else if ((eventType == EventTypeNames::wheel || eventType == EventTypeNames::mousewheel) && event->hasInterface(EventNames::WheelEvent)) {
        WheelEvent* wheelEvent = toWheelEvent(event);

        // If we don't have a renderer, send the wheel event to the first node we find with a renderer.
        // This is needed for <option> and <optgroup> elements so that <select>s get a wheel scroll.
        Node* startNode = this;
        while (startNode && !startNode->renderer())
            startNode = startNode->parentOrShadowHostNode();

        if (startNode && startNode->renderer()) {
            if (LocalFrame* frame = document().frame())
                frame->eventHandler().defaultWheelEventHandler(startNode, wheelEvent);
        }
    } else if (event->type() == EventTypeNames::webkitEditableContentChanged) {
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

#if !ENABLE(OILPAN)
// This is here for inlining
inline void TreeScope::removedLastRefToScope()
{
    ASSERT_WITH_SECURITY_IMPLICATION(!deletionHasBegun());
    if (m_guardRefCount) {
        // If removing a child removes the last self-only ref, we don't
        // want the scope to be destructed until after
        // removeDetachedChildren returns, so we guard ourselves with an
        // extra self-only ref.
        guardRef();
        dispose();
#if ASSERT_ENABLED
        // We need to do this right now since guardDeref() can delete this.
        rootNode().m_inRemovedLastRefFunction = false;
#endif
        guardDeref();
    } else {
#if ASSERT_ENABLED
        rootNode().m_inRemovedLastRefFunction = false;
#endif
#if SECURITY_ASSERT_ENABLED
        beginDeletion();
#endif
        delete this;
    }
}

// It's important not to inline removedLastRef, because we don't want to inline the code to
// delete a Node at each deref call site.
void Node::removedLastRef()
{
    // An explicit check for Document here is better than a virtual function since it is
    // faster for non-Document nodes, and because the call to removedLastRef that is inlined
    // at all deref call sites is smaller if it's a non-virtual function.
    if (isTreeScope()) {
        treeScope().removedLastRefToScope();
        return;
    }

#if SECURITY_ASSERT_ENABLED
    m_deletionHasBegun = true;
#endif
    delete this;
}
#endif

unsigned Node::connectedSubframeCount() const
{
    return hasRareData() ? rareData()->connectedSubframeCount() : 0;
}

void Node::incrementConnectedSubframeCount(unsigned amount)
{
    ASSERT(isContainerNode());
    ensureRareData().incrementConnectedSubframeCount(amount);
}

void Node::decrementConnectedSubframeCount(unsigned amount)
{
    rareData()->decrementConnectedSubframeCount(amount);
}

void Node::updateAncestorConnectedSubframeCountForRemoval() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount(count);
}

void Node::updateAncestorConnectedSubframeCountForInsertion() const
{
    unsigned count = connectedSubframeCount();

    if (!count)
        return;

    for (Node* node = parentOrShadowHostNode(); node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount(count);
}

PassRefPtrWillBeRawPtr<StaticNodeList> Node::getDestinationInsertionPoints()
{
    document().updateDistributionForNodeIfNeeded(this);
    WillBeHeapVector<RawPtrWillBeMember<InsertionPoint>, 8> insertionPoints;
    collectDestinationInsertionPoints(*this, insertionPoints);
    WillBeHeapVector<RefPtrWillBeMember<Node> > filteredInsertionPoints;
    for (size_t i = 0; i < insertionPoints.size(); ++i) {
        InsertionPoint* insertionPoint = insertionPoints[i];
        ASSERT(insertionPoint->containingShadowRoot());
        if (insertionPoint->containingShadowRoot()->type() != ShadowRoot::UserAgentShadowRoot)
            filteredInsertionPoints.append(insertionPoint);
    }
    return StaticNodeList::adopt(filteredInsertionPoints);
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
    ASSERT(isUserActionElement());
    return document().userActionElements().isActive(this);
}

bool Node::isUserActionElementInActiveChain() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isInActiveChain(this);
}

bool Node::isUserActionElementHovered() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isHovered(this);
}

bool Node::isUserActionElementFocused() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isFocused(this);
}

void Node::setCustomElementState(CustomElementState newState)
{
    CustomElementState oldState = customElementState();

    switch (newState) {
    case NotCustomElement:
        ASSERT_NOT_REACHED(); // Everything starts in this state
        return;

    case WaitingForUpgrade:
        ASSERT(NotCustomElement == oldState);
        break;

    case Upgraded:
        ASSERT(WaitingForUpgrade == oldState);
        break;
    }

    ASSERT(isHTMLElement() || isSVGElement());
    setFlag(CustomElementFlag);
    setFlag(newState == Upgraded, CustomElementUpgradedFlag);

    if (oldState == NotCustomElement || newState == Upgraded)
        setNeedsStyleRecalc(SubtreeStyleChange); // :unresolved has changed
}

void Node::trace(Visitor* visitor)
{
    visitor->trace(m_parentOrShadowHostNode);
    visitor->trace(m_previous);
    visitor->trace(m_next);
    if (hasRareData())
        visitor->trace(rareData());
    visitor->trace(m_treeScope);
    EventTarget::trace(visitor);
}

unsigned Node::lengthOfContents() const
{
    // This switch statement must be consistent with that of Range::processContentsBetweenOffsets.
    switch (nodeType()) {
    case Node::TEXT_NODE:
    case Node::CDATA_SECTION_NODE:
    case Node::COMMENT_NODE:
        return toCharacterData(this)->length();
    case Node::PROCESSING_INSTRUCTION_NODE:
        return toProcessingInstruction(this)->data().length();
    case Node::ELEMENT_NODE:
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        return toContainerNode(this)->countChildren();
    case Node::DOCUMENT_TYPE_NODE:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore

#ifndef NDEBUG

void showNode(const WebCore::Node* node)
{
    if (node)
        node->showNode("");
}

void showTree(const WebCore::Node* node)
{
    if (node)
        node->showTreeForThis();
}

void showNodePath(const WebCore::Node* node)
{
    if (node)
        node->showNodePathForThis();
}

#endif
