/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "core/HTMLNames.h"
#include "core/MathMLNames.h"
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
#include "core/dom/GetRootNodeOptions.h"
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
#include "core/dom/custom/CustomElement.h"
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
#include "core/events/PointerEventFactory.h"
#include "core/events/TextEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/UIEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutBox.h"
#include "core/page/ContextMenuController.h"
#include "core/page/Page.h"
#include "core/svg/SVGElement.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/InstanceCounters.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/tracing/TraceEvent.h"
#include "platform/tracing/TracedValue.h"
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
static WeakNodeSet& liveNodeSet() {
  DEFINE_STATIC_LOCAL(WeakNodeSet, set, (new WeakNodeSet));
  return set;
}
#endif

void Node::dumpStatistics() {
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
        case kElementNode: {
          ++elementNodes;

          // Tag stats
          Element* element = toElement(node);
          HashMap<String, size_t>::AddResult result =
              perTagCount.add(element->tagName(), 1);
          if (!result.isNewEntry)
            result.storedValue->value++;

          size_t attributeCount = element->attributesWithoutUpdate().size();
          if (attributeCount) {
            attributes += attributeCount;
            ++elementsWithAttributeStorage;
          }
          break;
        }
        case kAttributeNode: {
          ++attrNodes;
          break;
        }
        case kTextNode: {
          ++textNodes;
          break;
        }
        case kCdataSectionNode: {
          ++cdataNodes;
          break;
        }
        case kCommentNode: {
          ++commentNodes;
          break;
        }
        case kProcessingInstructionNode: {
          ++piNodes;
          break;
        }
        case kDocumentNode: {
          ++documentNodes;
          break;
        }
        case kDocumentTypeNode: {
          ++docTypeNodes;
          break;
        }
        case kDocumentFragmentNode: {
          if (node->isShadowRoot())
            ++shadowRootNodes;
          else
            ++fragmentNodes;
          break;
        }
      }
    }
  }

  std::stringstream perTagStream;
  for (const auto& entry : perTagCount)
    perTagStream << "  Number of <" << entry.key.utf8().data()
                 << "> tags: " << entry.value << "\n";

  LOG(INFO) << "\n"
            << "Number of Nodes: " << liveNodeSet().size() << "\n"
            << "Number of Nodes with RareData: " << nodesWithRareData << "\n\n"

            << "NodeType distribution:\n"
            << "  Number of Element nodes: " << elementNodes << "\n"
            << "  Number of Attribute nodes: " << attrNodes << "\n"
            << "  Number of Text nodes: " << textNodes << "\n"
            << "  Number of CDATASection nodes: " << cdataNodes << "\n"
            << "  Number of Comment nodes: " << commentNodes << "\n"
            << "  Number of ProcessingInstruction nodes: " << piNodes << "\n"
            << "  Number of Document nodes: " << documentNodes << "\n"
            << "  Number of DocumentType nodes: " << docTypeNodes << "\n"
            << "  Number of DocumentFragment nodes: " << fragmentNodes << "\n"
            << "  Number of ShadowRoot nodes: " << shadowRootNodes << "\n"

            << "Element tag name distibution:\n"
            << perTagStream.str()

            << "Attributes:\n"
            << "  Number of Attributes (non-Node and Node): " << attributes
            << " x " << sizeof(Attribute) << "Bytes\n"
            << "  Number of Elements with attribute storage: "
            << elementsWithAttributeStorage << " x " << sizeof(ElementData)
            << "Bytes\n"
            << "  Number of Elements with RareData: " << elementsWithRareData
            << "\n"
            << "  Number of Elements with NamedNodeMap: "
            << elementsWithNamedNodeMap << " x " << sizeof(NamedNodeMap)
            << "Bytes";
#endif
}

void Node::trackForDebugging() {
#if DUMP_NODE_STATISTICS
  liveNodeSet().add(this);
#endif
}

Node::Node(TreeScope* treeScope, ConstructionType type)
    : m_nodeFlags(type),
      m_parentOrShadowHostNode(nullptr),
      m_treeScope(treeScope),
      m_previous(nullptr),
      m_next(nullptr) {
  DCHECK(m_treeScope || type == CreateDocument || type == CreateShadowRoot);
#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
  trackForDebugging();
#endif
  InstanceCounters::incrementNodeCounter();
}

Node::~Node() {
  // With Oilpan, the rare data finalizer also asserts for
  // this condition (we cannot directly access it here.)
  RELEASE_ASSERT(hasRareData() || !layoutObject());
  InstanceCounters::decrementNodeCounter();
}

NodeRareData* Node::rareData() const {
  SECURITY_DCHECK(hasRareData());
  return static_cast<NodeRareData*>(m_data.m_rareData);
}

NodeRareData& Node::ensureRareData() {
  if (hasRareData())
    return *rareData();

  if (isElementNode())
    m_data.m_rareData = ElementRareData::create(m_data.m_layoutObject);
  else
    m_data.m_rareData = NodeRareData::create(m_data.m_layoutObject);

  DCHECK(m_data.m_rareData);
  setFlag(HasRareDataFlag);
  ScriptWrappableVisitor::writeBarrier(this, rareData());
  return *rareData();
}

Node* Node::toNode() {
  return this;
}

int Node::tabIndex() const {
  return 0;
}

String Node::nodeValue() const {
  return String();
}

void Node::setNodeValue(const String&) {
  // By default, setting nodeValue has no effect.
}

NodeList* Node::childNodes() {
  ThreadState::MainThreadGCForbiddenScope gcForbidden;
  if (isContainerNode())
    return ensureRareData().ensureNodeLists().ensureChildNodeList(
        toContainerNode(*this));
  return ensureRareData().ensureNodeLists().ensureEmptyChildNodeList(*this);
}

Node* Node::pseudoAwarePreviousSibling() const {
  if (parentElement() && !previousSibling()) {
    Element* parent = parentElement();
    if (isAfterPseudoElement() && parent->lastChild())
      return parent->lastChild();
    if (!isBeforePseudoElement())
      return parent->pseudoElement(PseudoIdBefore);
  }
  return previousSibling();
}

Node* Node::pseudoAwareNextSibling() const {
  if (parentElement() && !nextSibling()) {
    Element* parent = parentElement();
    if (isBeforePseudoElement() && parent->hasChildren())
      return parent->firstChild();
    if (!isAfterPseudoElement())
      return parent->pseudoElement(PseudoIdAfter);
  }
  return nextSibling();
}

Node* Node::pseudoAwareFirstChild() const {
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

Node* Node::pseudoAwareLastChild() const {
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

Node& Node::treeRoot() const {
  if (isInTreeScope())
    return containingTreeScope().rootNode();
  const Node* node = this;
  while (node->parentNode())
    node = node->parentNode();
  return const_cast<Node&>(*node);
}

Node* Node::getRootNode(const GetRootNodeOptions& options) const {
  return (options.hasComposed() && options.composed()) ? &shadowIncludingRoot()
                                                       : &treeRoot();
}

Node* Node::insertBefore(Node* newChild,
                         Node* refChild,
                         ExceptionState& exceptionState) {
  if (isContainerNode())
    return toContainerNode(this)->insertBefore(newChild, refChild,
                                               exceptionState);

  exceptionState.throwDOMException(
      HierarchyRequestError, "This node type does not support this method.");
  return nullptr;
}

Node* Node::replaceChild(Node* newChild,
                         Node* oldChild,
                         ExceptionState& exceptionState) {
  if (isContainerNode())
    return toContainerNode(this)->replaceChild(newChild, oldChild,
                                               exceptionState);

  exceptionState.throwDOMException(
      HierarchyRequestError, "This node type does not support this method.");
  return nullptr;
}

Node* Node::removeChild(Node* oldChild, ExceptionState& exceptionState) {
  if (isContainerNode())
    return toContainerNode(this)->removeChild(oldChild, exceptionState);

  exceptionState.throwDOMException(
      NotFoundError, "This node type does not support this method.");
  return nullptr;
}

Node* Node::appendChild(Node* newChild, ExceptionState& exceptionState) {
  if (isContainerNode())
    return toContainerNode(this)->appendChild(newChild, exceptionState);

  exceptionState.throwDOMException(
      HierarchyRequestError, "This node type does not support this method.");
  return nullptr;
}

static bool isNodeInNodes(const Node* const node,
                          const HeapVector<NodeOrString>& nodes) {
  for (const NodeOrString& nodeOrString : nodes) {
    if (nodeOrString.isNode() && nodeOrString.getAsNode() == node)
      return true;
  }
  return false;
}

static Node* findViablePreviousSibling(const Node& node,
                                       const HeapVector<NodeOrString>& nodes) {
  for (Node* sibling = node.previousSibling(); sibling;
       sibling = sibling->previousSibling()) {
    if (!isNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* findViableNextSibling(const Node& node,
                                   const HeapVector<NodeOrString>& nodes) {
  for (Node* sibling = node.nextSibling(); sibling;
       sibling = sibling->nextSibling()) {
    if (!isNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* nodeOrStringToNode(const NodeOrString& nodeOrString,
                                Document& document) {
  if (nodeOrString.isNode())
    return nodeOrString.getAsNode();
  return Text::create(document, nodeOrString.getAsString());
}

// Returns nullptr if an exception was thrown.
static Node* convertNodesIntoNode(const HeapVector<NodeOrString>& nodes,
                                  Document& document,
                                  ExceptionState& exceptionState) {
  if (nodes.size() == 1)
    return nodeOrStringToNode(nodes[0], document);

  Node* fragment = DocumentFragment::create(document);
  for (const NodeOrString& nodeOrString : nodes) {
    fragment->appendChild(nodeOrStringToNode(nodeOrString, document),
                          exceptionState);
    if (exceptionState.hadException())
      return nullptr;
  }
  return fragment;
}

void Node::prepend(const HeapVector<NodeOrString>& nodes,
                   ExceptionState& exceptionState) {
  if (Node* node = convertNodesIntoNode(nodes, document(), exceptionState))
    insertBefore(node, firstChild(), exceptionState);
}

void Node::append(const HeapVector<NodeOrString>& nodes,
                  ExceptionState& exceptionState) {
  if (Node* node = convertNodesIntoNode(nodes, document(), exceptionState))
    appendChild(node, exceptionState);
}

void Node::before(const HeapVector<NodeOrString>& nodes,
                  ExceptionState& exceptionState) {
  Node* parent = parentNode();
  if (!parent)
    return;
  Node* viablePreviousSibling = findViablePreviousSibling(*this, nodes);
  if (Node* node = convertNodesIntoNode(nodes, document(), exceptionState))
    parent->insertBefore(node, viablePreviousSibling
                                   ? viablePreviousSibling->nextSibling()
                                   : parent->firstChild(),
                         exceptionState);
}

void Node::after(const HeapVector<NodeOrString>& nodes,
                 ExceptionState& exceptionState) {
  Node* parent = parentNode();
  if (!parent)
    return;
  Node* viableNextSibling = findViableNextSibling(*this, nodes);
  if (Node* node = convertNodesIntoNode(nodes, document(), exceptionState))
    parent->insertBefore(node, viableNextSibling, exceptionState);
}

void Node::replaceWith(const HeapVector<NodeOrString>& nodes,
                       ExceptionState& exceptionState) {
  Node* parent = parentNode();
  if (!parent)
    return;
  Node* viableNextSibling = findViableNextSibling(*this, nodes);
  Node* node = convertNodesIntoNode(nodes, document(), exceptionState);
  if (exceptionState.hadException())
    return;
  if (parent == parentNode())
    parent->replaceChild(node, this, exceptionState);
  else
    parent->insertBefore(node, viableNextSibling, exceptionState);
}

void Node::remove(ExceptionState& exceptionState) {
  if (ContainerNode* parent = parentNode())
    parent->removeChild(this, exceptionState);
}

void Node::normalize() {
  updateDistribution();

  // Go through the subtree beneath us, normalizing all nodes. This means that
  // any two adjacent text nodes are merged and any empty text nodes are
  // removed.

  Node* node = this;
  while (Node* firstChild = node->firstChild())
    node = firstChild;
  while (node) {
    if (node == this)
      break;

    if (node->getNodeType() == kTextNode)
      node = toText(node)->mergeNextSiblingNodesIfPossible();
    else
      node = NodeTraversal::nextPostOrder(*node);
  }
}

LayoutBox* Node::layoutBox() const {
  LayoutObject* layoutObject = this->layoutObject();
  return layoutObject && layoutObject->isBox() ? toLayoutBox(layoutObject)
                                               : nullptr;
}

LayoutBoxModelObject* Node::layoutBoxModelObject() const {
  LayoutObject* layoutObject = this->layoutObject();
  return layoutObject && layoutObject->isBoxModelObject()
             ? toLayoutBoxModelObject(layoutObject)
             : nullptr;
}

LayoutRect Node::boundingBox() const {
  if (layoutObject())
    return LayoutRect(layoutObject()->absoluteBoundingBoxRect());
  return LayoutRect();
}

#ifndef NDEBUG
inline static ShadowRoot* oldestShadowRootFor(const Node* node) {
  if (!node->isElementNode())
    return nullptr;
  if (ElementShadow* shadow = toElement(node)->shadow())
    return &shadow->oldestShadowRoot();
  return nullptr;
}
#endif

Node& Node::shadowIncludingRoot() const {
  if (isConnected())
    return document();
  Node* root = const_cast<Node*>(this);
  while (Node* host = root->ownerShadowHost())
    root = host;
  while (Node* ancestor = root->parentNode())
    root = ancestor;
  DCHECK(!root->ownerShadowHost());
  return *root;
}

bool Node::isClosedShadowHiddenFrom(const Node& other) const {
  if (!isInShadowTree() || treeScope() == other.treeScope())
    return false;

  const TreeScope* scope = &treeScope();
  for (; scope->parentTreeScope(); scope = scope->parentTreeScope()) {
    const ContainerNode& root = scope->rootNode();
    if (root.isShadowRoot() && !toShadowRoot(root).isOpenOrV0())
      break;
  }

  for (TreeScope* otherScope = &other.treeScope(); otherScope;
       otherScope = otherScope->parentTreeScope()) {
    if (otherScope == scope)
      return false;
  }
  return true;
}

bool Node::needsDistributionRecalc() const {
  return shadowIncludingRoot().childNeedsDistributionRecalc();
}

void Node::updateDistribution() {
  // Extra early out to avoid spamming traces.
  if (isConnected() && !document().childNeedsDistributionRecalc())
    return;
  TRACE_EVENT0("blink", "Node::updateDistribution");
  ScriptForbiddenScope forbidScript;
  Node& root = shadowIncludingRoot();
  if (root.childNeedsDistributionRecalc())
    root.recalcDistribution();
}

void Node::recalcDistribution() {
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

  for (ShadowRoot* root = youngestShadowRoot(); root;
       root = root->olderShadowRoot()) {
    if (root->childNeedsDistributionRecalc())
      root->recalcDistribution();
  }

  clearChildNeedsDistributionRecalc();
}

void Node::setIsLink(bool isLink) {
  setFlag(isLink && !SVGImage::isInSVGImage(toElement(this)), IsLinkFlag);
}

void Node::setNeedsStyleInvalidation() {
  DCHECK(isElementNode() || isShadowRoot());
  setFlag(NeedsStyleInvalidationFlag);
  markAncestorsWithChildNeedsStyleInvalidation();
}

void Node::markAncestorsWithChildNeedsStyleInvalidation() {
  ScriptForbiddenScope forbidScriptDuringRawIteration;
  for (Node* node = parentOrShadowHostNode();
       node && !node->childNeedsStyleInvalidation();
       node = node->parentOrShadowHostNode())
    node->setChildNeedsStyleInvalidation();
  document().scheduleLayoutTreeUpdateIfNeeded();
}

void Node::markAncestorsWithChildNeedsDistributionRecalc() {
  ScriptForbiddenScope forbidScriptDuringRawIteration;
  for (Node* node = this; node && !node->childNeedsDistributionRecalc();
       node = node->parentOrShadowHostNode())
    node->setChildNeedsDistributionRecalc();
  document().scheduleLayoutTreeUpdateIfNeeded();
}

inline void Node::setStyleChange(StyleChangeType changeType) {
  m_nodeFlags = (m_nodeFlags & ~StyleChangeMask) | changeType;
}

void Node::markAncestorsWithChildNeedsStyleRecalc() {
  for (ContainerNode* p = parentOrShadowHostNode();
       p && !p->childNeedsStyleRecalc(); p = p->parentOrShadowHostNode())
    p->setChildNeedsStyleRecalc();
  document().scheduleLayoutTreeUpdateIfNeeded();
}

void Node::markAncestorsWithChildNeedsReattachLayoutTree() {
  for (ContainerNode* p = parentOrShadowHostNode();
       p && !p->childNeedsReattachLayoutTree(); p = p->parentOrShadowHostNode())
    p->setChildNeedsReattachLayoutTree();
}

void Node::setNeedsReattachLayoutTree() {
  setFlag(NeedsReattachLayoutTree);
  markAncestorsWithChildNeedsReattachLayoutTree();
}

void Node::setNeedsStyleRecalc(StyleChangeType changeType,
                               const StyleChangeReasonForTracing& reason) {
  DCHECK(changeType != NoStyleChange);
  if (!inActiveDocument())
    return;

  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
      "StyleRecalcInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorStyleRecalcInvalidationTrackingEvent::data(this, reason));

  StyleChangeType existingChangeType = getStyleChangeType();
  if (changeType > existingChangeType)
    setStyleChange(changeType);

  if (existingChangeType == NoStyleChange)
    markAncestorsWithChildNeedsStyleRecalc();

  if (isElementNode() && hasRareData())
    toElement(*this).setAnimationStyleChange(false);

  if (isSVGElement())
    toSVGElement(this)->setNeedsStyleRecalcForInstances(changeType, reason);
}

void Node::clearNeedsStyleRecalc() {
  m_nodeFlags &= ~StyleChangeMask;

  if (isElementNode() && hasRareData())
    toElement(*this).setAnimationStyleChange(false);
}

bool Node::inActiveDocument() const {
  return isConnected() && document().isActive();
}

Node* Node::focusDelegate() {
  return this;
}

bool Node::shouldHaveFocusAppearance() const {
  DCHECK(isFocused());
  return true;
}

bool Node::isInert() const {
  const HTMLDialogElement* dialog = document().activeModalDialog();
  if (dialog && this != document() &&
      (!canParticipateInFlatTree() ||
       !FlatTreeTraversal::containsIncludingPseudoElement(*dialog, *this)))
    return true;
  return document().localOwner() && document().localOwner()->isInert();
}

unsigned Node::nodeIndex() const {
  const Node* tempNode = previousSibling();
  unsigned count = 0;
  for (count = 0; tempNode; count++)
    tempNode = tempNode->previousSibling();
  return count;
}

NodeListsNodeData* Node::nodeLists() {
  return hasRareData() ? rareData()->nodeLists() : nullptr;
}

void Node::clearNodeLists() {
  rareData()->clearNodeLists();
}

bool Node::isDescendantOf(const Node* other) const {
  // Return true if other is an ancestor of this, otherwise false
  if (!other || !other->hasChildren() || isConnected() != other->isConnected())
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

bool Node::contains(const Node* node) const {
  if (!node)
    return false;
  return this == node || node->isDescendantOf(this);
}

bool Node::isShadowIncludingInclusiveAncestorOf(const Node* node) const {
  if (!node)
    return false;

  if (this == node)
    return true;

  if (document() != node->document())
    return false;

  if (isConnected() != node->isConnected())
    return false;

  bool hasChildren = isContainerNode() && toContainerNode(this)->hasChildren();
  bool hasShadow = isElementNode() && toElement(this)->shadow();
  if (!hasChildren && !hasShadow)
    return false;

  for (; node; node = node->ownerShadowHost()) {
    if (treeScope() == node->treeScope())
      return contains(node);
  }

  return false;
}

bool Node::containsIncludingHostElements(const Node& node) const {
  const Node* current = &node;
  do {
    if (current == this)
      return true;
    if (current->isDocumentFragment() &&
        toDocumentFragment(current)->isTemplateContent())
      current =
          static_cast<const TemplateContentDocumentFragment*>(current)->host();
    else
      current = current->parentOrShadowHostNode();
  } while (current);
  return false;
}

Node* Node::commonAncestor(const Node& other,
                           ContainerNode* (*parent)(const Node&)) const {
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

void Node::reattachLayoutTree(const AttachContext& context) {
  AttachContext reattachContext(context);
  reattachContext.performingReattach = true;

  // We only need to detach if the node has already been through
  // attachLayoutTree().
  if (getStyleChangeType() < NeedsReattachStyleChange)
    detachLayoutTree(reattachContext);
  attachLayoutTree(reattachContext);
}

void Node::attachLayoutTree(const AttachContext&) {
  DCHECK(document().inStyleRecalc() || isDocumentNode());
  DCHECK(!document().lifecycle().inDetach());
  DCHECK(needsAttach());
  DCHECK(!layoutObject() ||
         (layoutObject()->style() &&
          (layoutObject()->parent() || layoutObject()->isLayoutView())));

  clearNeedsStyleRecalc();
  clearNeedsReattachLayoutTree();

  if (AXObjectCache* cache = document().axObjectCache())
    cache->updateCacheAfterNodeIsAttached(this);
}

void Node::detachLayoutTree(const AttachContext& context) {
  DCHECK(document().lifecycle().stateAllowsDetach());
  DocumentLifecycle::DetachScope willDetach(document().lifecycle());

  if (layoutObject())
    layoutObject()->destroyAndCleanupAnonymousWrappers();
  setLayoutObject(nullptr);
  setStyleChange(NeedsReattachStyleChange);
  clearChildNeedsStyleInvalidation();
}

void Node::reattachWhitespaceSiblingsIfNeeded(Text* start) {
  ScriptForbiddenScope forbidScriptDuringRawIteration;
  for (Node* sibling = start; sibling; sibling = sibling->nextSibling()) {
    if (sibling->isTextNode() && toText(sibling)->containsOnlyWhitespace()) {
      bool hadLayoutObject = !!sibling->layoutObject();
      toText(sibling)->reattachLayoutTreeIfNeeded();
      // If sibling's layout object status didn't change we don't need to
      // continue checking other siblings since their layout object status won't
      // change either.
      if (!!sibling->layoutObject() == hadLayoutObject)
        return;
    } else if (sibling->layoutObject()) {
      return;
    }
  }
}

const ComputedStyle* Node::virtualEnsureComputedStyle(
    PseudoId pseudoElementSpecifier) {
  return parentOrShadowHostNode()
             ? parentOrShadowHostNode()->ensureComputedStyle(
                   pseudoElementSpecifier)
             : nullptr;
}

int Node::maxCharacterOffset() const {
  NOTREACHED();
  return 0;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks
// questions about HTML in the core DOM class is obviously misplaced.
bool Node::canStartSelection() const {
  if (hasEditableStyle(*this))
    return true;

  if (layoutObject()) {
    const ComputedStyle& style = layoutObject()->styleRef();
    // We allow selections to begin within an element that has
    // -webkit-user-select: none set, but if the element is draggable then
    // dragging should take priority over selection.
    if (style.userDrag() == DRAG_ELEMENT && style.userSelect() == SELECT_NONE)
      return false;
  }
  ContainerNode* parent = FlatTreeTraversal::parent(*this);
  return parent ? parent->canStartSelection() : true;
}

// StyledElements allow inline style (style="border: 1px"), presentational
// attributes (ex. color), class names (ex. class="foo bar") and other non-basic
// styling features. They also control if this element can participate in style
// sharing.
//
// FIXME: The only things that ever go through StyleResolver that aren't
// StyledElements are PseudoElements and VTTElements. It's possible we can just
// eliminate all the checks since those elements will never have class names,
// inline style, or other things that this apparently guards against.
bool Node::isStyledElement() const {
  return isHTMLElement() || isSVGElement() ||
         (isElementNode() &&
          toElement(this)->namespaceURI() == MathMLNames::mathmlNamespaceURI);
}

bool Node::canParticipateInFlatTree() const {
  // TODO(hayato): Return false for pseudo elements.
  return !isShadowRoot() && !isActiveSlotOrActiveInsertionPoint();
}

bool Node::isActiveSlotOrActiveInsertionPoint() const {
  return (isHTMLSlotElement(*this) &&
          toHTMLSlotElement(*this).supportsDistribution()) ||
         isActiveInsertionPoint(*this);
}

AtomicString Node::slotName() const {
  DCHECK(isSlotable());
  if (isElementNode())
    return HTMLSlotElement::normalizeSlotName(
        toElement(*this).fastGetAttribute(HTMLNames::slotAttr));
  DCHECK(isTextNode());
  return emptyAtom;
}

bool Node::isInV1ShadowTree() const {
  ShadowRoot* shadowRoot = containingShadowRoot();
  return shadowRoot && shadowRoot->isV1();
}

bool Node::isInV0ShadowTree() const {
  ShadowRoot* shadowRoot = containingShadowRoot();
  return shadowRoot && !shadowRoot->isV1();
}

ElementShadow* Node::parentElementShadow() const {
  Element* parent = parentElement();
  return parent ? parent->shadow() : nullptr;
}

bool Node::isChildOfV1ShadowHost() const {
  ElementShadow* parentShadow = parentElementShadow();
  return parentShadow && parentShadow->isV1();
}

bool Node::isChildOfV0ShadowHost() const {
  ElementShadow* parentShadow = parentElementShadow();
  return parentShadow && !parentShadow->isV1();
}

ShadowRoot* Node::v1ShadowRootOfParent() const {
  if (Element* parent = parentElement())
    return parent->shadowRootIfV1();
  return nullptr;
}

Element* Node::ownerShadowHost() const {
  if (ShadowRoot* root = containingShadowRoot())
    return &root->host();
  return nullptr;
}

ShadowRoot* Node::containingShadowRoot() const {
  Node& root = treeScope().rootNode();
  return root.isShadowRoot() ? toShadowRoot(&root) : nullptr;
}

Node* Node::nonBoundaryShadowTreeRootNode() {
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

ContainerNode* Node::nonShadowBoundaryParentNode() const {
  ContainerNode* parent = parentNode();
  return parent && !parent->isShadowRoot() ? parent : nullptr;
}

Element* Node::parentOrShadowHostElement() const {
  ContainerNode* parent = parentOrShadowHostNode();
  if (!parent)
    return nullptr;

  if (parent->isShadowRoot())
    return &toShadowRoot(parent)->host();

  if (!parent->isElementNode())
    return nullptr;

  return toElement(parent);
}

ContainerNode* Node::parentOrShadowHostOrTemplateHostNode() const {
  if (isDocumentFragment() && toDocumentFragment(this)->isTemplateContent())
    return static_cast<const TemplateContentDocumentFragment*>(this)->host();
  return parentOrShadowHostNode();
}

Document* Node::ownerDocument() const {
  Document* doc = &document();
  return doc == this ? nullptr : doc;
}

const KURL& Node::baseURI() const {
  return document().baseURL();
}

bool Node::isEqualNode(Node* other) const {
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

bool Node::isDefaultNamespace(
    const AtomicString& namespaceURIMaybeEmpty) const {
  const AtomicString& namespaceURI =
      namespaceURIMaybeEmpty.isEmpty() ? nullAtom : namespaceURIMaybeEmpty;

  switch (getNodeType()) {
    case kElementNode: {
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
    case kDocumentNode:
      if (Element* de = toDocument(this)->documentElement())
        return de->isDefaultNamespace(namespaceURI);
      return false;
    case kDocumentTypeNode:
    case kDocumentFragmentNode:
      return false;
    case kAttributeNode: {
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

const AtomicString& Node::lookupPrefix(const AtomicString& namespaceURI) const {
  // Implemented according to
  // https://dom.spec.whatwg.org/#dom-node-lookupprefix

  if (namespaceURI.isEmpty() || namespaceURI.isNull())
    return nullAtom;

  const Element* context;

  switch (getNodeType()) {
    case kElementNode:
      context = toElement(this);
      break;
    case kDocumentNode:
      context = toDocument(this)->documentElement();
      break;
    case kDocumentFragmentNode:
    case kDocumentTypeNode:
      context = nullptr;
      break;
    case kAttributeNode:
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

const AtomicString& Node::lookupNamespaceURI(const String& prefix) const {
  // Implemented according to
  // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/namespaces-algorithms.html#lookupNamespaceURIAlgo

  if (!prefix.isNull() && prefix.isEmpty())
    return nullAtom;

  switch (getNodeType()) {
    case kElementNode: {
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
    case kDocumentNode:
      if (Element* de = toDocument(this)->documentElement())
        return de->lookupNamespaceURI(prefix);
      return nullAtom;
    case kDocumentTypeNode:
    case kDocumentFragmentNode:
      return nullAtom;
    case kAttributeNode: {
      const Attr* attr = toAttr(this);
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

String Node::textContent(bool convertBRsToNewlines) const {
  // This covers ProcessingInstruction and Comment that should return their
  // value when .textContent is accessed on them, but should be ignored when
  // iterated over as a descendant of a ContainerNode.
  if (isCharacterDataNode())
    return toCharacterData(this)->data();

  // Attribute nodes have their attribute values as textContent.
  if (isAttributeNode())
    return toAttr(this)->value();

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

void Node::setTextContent(const String& text) {
  switch (getNodeType()) {
    case kAttributeNode:
    case kTextNode:
    case kCdataSectionNode:
    case kCommentNode:
    case kProcessingInstructionNode:
      setNodeValue(text);
      return;
    case kElementNode:
    case kDocumentFragmentNode: {
      // FIXME: Merge this logic into replaceChildrenWithText.
      ContainerNode* container = toContainerNode(this);

      // Note: This is an intentional optimization.
      // See crbug.com/352836 also.
      // No need to do anything if the text is identical.
      if (container->hasOneTextChild() &&
          toText(container->firstChild())->data() == text && !text.isEmpty())
        return;

      ChildListMutationScope mutation(*this);
      // Note: This API will not insert empty text nodes:
      // https://dom.spec.whatwg.org/#dom-node-textcontent
      if (text.isEmpty()) {
        container->removeChildren(DispatchSubtreeModifiedEvent);
      } else {
        container->removeChildren(OmitSubtreeModifiedEvent);
        container->appendChild(document().createTextNode(text),
                               ASSERT_NO_EXCEPTION);
      }
      return;
    }
    case kDocumentNode:
    case kDocumentTypeNode:
      // Do nothing.
      return;
  }
  NOTREACHED();
}

unsigned short Node::compareDocumentPosition(
    const Node* otherNode,
    ShadowTreesTreatment treatment) const {
  if (otherNode == this)
    return kDocumentPositionEquivalent;

  const Attr* attr1 = getNodeType() == kAttributeNode ? toAttr(this) : nullptr;
  const Attr* attr2 =
      otherNode->getNodeType() == kAttributeNode ? toAttr(otherNode) : nullptr;

  const Node* start1 = attr1 ? attr1->ownerElement() : this;
  const Node* start2 = attr2 ? attr2->ownerElement() : otherNode;

  // If either of start1 or start2 is null, then we are disconnected, since one
  // of the nodes is an orphaned attribute node.
  if (!start1 || !start2) {
    unsigned short direction = (this > otherNode) ? kDocumentPositionPreceding
                                                  : kDocumentPositionFollowing;
    return kDocumentPositionDisconnected |
           kDocumentPositionImplementationSpecific | direction;
  }

  HeapVector<Member<const Node>, 16> chain1;
  HeapVector<Member<const Node>, 16> chain2;
  if (attr1)
    chain1.append(attr1);
  if (attr2)
    chain2.append(attr2);

  if (attr1 && attr2 && start1 == start2 && start1) {
    // We are comparing two attributes on the same node. Crawl our attribute map
    // and see which one we hit first.
    const Element* owner1 = attr1->ownerElement();
    AttributeCollection attributes = owner1->attributes();
    for (const Attribute& attr : attributes) {
      // If neither of the two determining nodes is a child node and nodeType is
      // the same for both determining nodes, then an implementation-dependent
      // order between the determining nodes is returned. This order is stable
      // as long as no nodes of the same nodeType are inserted into or removed
      // from the direct container. This would be the case, for example, when
      // comparing two attributes of the same element, and inserting or removing
      // additional attributes might change the order between existing
      // attributes.
      if (attr1->getQualifiedName() == attr.name())
        return kDocumentPositionImplementationSpecific |
               kDocumentPositionFollowing;
      if (attr2->getQualifiedName() == attr.name())
        return kDocumentPositionImplementationSpecific |
               kDocumentPositionPreceding;
    }

    NOTREACHED();
    return kDocumentPositionDisconnected;
  }

  // If one node is in the document and the other is not, we must be
  // disconnected.  If the nodes have different owning documents, they must be
  // disconnected.  Note that we avoid comparing Attr nodes here, since they
  // return false from isConnected() all the time (which seems like a bug).
  if (start1->isConnected() != start2->isConnected() ||
      (treatment == TreatShadowTreesAsDisconnected &&
       start1->treeScope() != start2->treeScope())) {
    unsigned short direction = (this > otherNode) ? kDocumentPositionPreceding
                                                  : kDocumentPositionFollowing;
    return kDocumentPositionDisconnected |
           kDocumentPositionImplementationSpecific | direction;
  }

  // We need to find a common ancestor container, and then compare the indices
  // of the two immediate children.
  const Node* current;
  for (current = start1; current; current = current->parentOrShadowHostNode())
    chain1.append(current);
  for (current = start2; current; current = current->parentOrShadowHostNode())
    chain2.append(current);

  unsigned index1 = chain1.size();
  unsigned index2 = chain2.size();

  // If the two elements don't have a common root, they're not in the same tree.
  if (chain1[index1 - 1] != chain2[index2 - 1]) {
    unsigned short direction = (this > otherNode) ? kDocumentPositionPreceding
                                                  : kDocumentPositionFollowing;
    return kDocumentPositionDisconnected |
           kDocumentPositionImplementationSpecific | direction;
  }

  unsigned connection = start1->treeScope() != start2->treeScope()
                            ? kDocumentPositionDisconnected |
                                  kDocumentPositionImplementationSpecific
                            : 0;

  // Walk the two chains backwards and look for the first difference.
  for (unsigned i = std::min(index1, index2); i; --i) {
    const Node* child1 = chain1[--index1];
    const Node* child2 = chain2[--index2];
    if (child1 != child2) {
      // If one of the children is an attribute, it wins.
      if (child1->getNodeType() == kAttributeNode)
        return kDocumentPositionFollowing | connection;
      if (child2->getNodeType() == kAttributeNode)
        return kDocumentPositionPreceding | connection;

      // If one of the children is a shadow root,
      if (child1->isShadowRoot() || child2->isShadowRoot()) {
        if (!child2->isShadowRoot())
          return Node::kDocumentPositionFollowing | connection;
        if (!child1->isShadowRoot())
          return Node::kDocumentPositionPreceding | connection;

        for (const ShadowRoot* child = toShadowRoot(child2)->olderShadowRoot();
             child; child = child->olderShadowRoot()) {
          if (child == child1) {
            return Node::kDocumentPositionFollowing | connection;
          }
        }

        return Node::kDocumentPositionPreceding | connection;
      }

      if (!child2->nextSibling())
        return kDocumentPositionFollowing | connection;
      if (!child1->nextSibling())
        return kDocumentPositionPreceding | connection;

      // Otherwise we need to see which node occurs first.  Crawl backwards from
      // child2 looking for child1.
      for (const Node* child = child2->previousSibling(); child;
           child = child->previousSibling()) {
        if (child == child1)
          return kDocumentPositionFollowing | connection;
      }
      return kDocumentPositionPreceding | connection;
    }
  }

  // There was no difference between the two parent chains, i.e., one was a
  // subset of the other.  The shorter chain is the ancestor.
  return index1 < index2
             ? kDocumentPositionFollowing | kDocumentPositionContainedBy |
                   connection
             : kDocumentPositionPreceding | kDocumentPositionContains |
                   connection;
}

String Node::debugName() const {
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

String Node::debugNodeName() const {
  return nodeName();
}

static void dumpAttributeDesc(const Node& node,
                              const QualifiedName& name,
                              std::ostream& ostream) {
  if (!node.isElementNode())
    return;
  const AtomicString& value = toElement(node).getAttribute(name);
  if (value.isEmpty())
    return;
  ostream << ' ' << name.toString().utf8().data() << '=' << value;
}

std::ostream& operator<<(std::ostream& ostream, const Node& node) {
  if (node.getNodeType() == Node::kProcessingInstructionNode)
    return ostream << "?" << node.nodeName().utf8().data();
  if (node.isShadowRoot()) {
    // nodeName of ShadowRoot is #document-fragment.  It's confused with
    // DocumentFragment.
    return ostream << "#shadow-root";
  }
  if (node.isDocumentTypeNode())
    return ostream << "DOCTYPE " << node.nodeName().utf8().data();

  // We avoid to print "" by utf8().data().
  ostream << node.nodeName().utf8().data();
  if (node.isTextNode())
    return ostream << " " << node.nodeValue();
  dumpAttributeDesc(node, HTMLNames::idAttr, ostream);
  dumpAttributeDesc(node, HTMLNames::classAttr, ostream);
  dumpAttributeDesc(node, HTMLNames::styleAttr, ostream);
  if (hasEditableStyle(node))
    ostream << " (editable)";
  if (node.document().focusedElement() == &node)
    ostream << " (focused)";
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const Node* node) {
  if (!node)
    return ostream << "null";
  return ostream << *node;
}

#ifndef NDEBUG

String Node::toString() const {
  // TODO(tkent): We implemented toString() with operator<<.  We should
  // implement operator<< with toString() instead.
  std::stringstream stream;
  stream << *this;
  return String(stream.str().c_str());
}

String Node::toTreeStringForThis() const {
  return toMarkedTreeString(this, "*");
}

String Node::toFlatTreeStringForThis() const {
  return toMarkedFlatTreeString(this, "*");
}

void Node::printNodePathTo(std::ostream& stream) const {
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
      for (const ShadowRoot* shadowRoot = toShadowRoot(node)->olderShadowRoot();
           shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
        ++count;
      stream << "/#shadow-root[" << count << "]";
      continue;
    }

    switch (node->getNodeType()) {
      case kElementNode: {
        stream << "/" << node->nodeName().utf8().data();

        const Element* element = toElement(node);
        const AtomicString& idattr = element->getIdAttribute();
        bool hasIdAttr = !idattr.isNull() && !idattr.isEmpty();
        if (node->previousSibling() || node->nextSibling()) {
          int count = 0;
          for (const Node* previous = node->previousSibling(); previous;
               previous = previous->previousSibling()) {
            if (previous->nodeName() == node->nodeName()) {
              ++count;
            }
          }
          if (hasIdAttr)
            stream << "[@id=\"" << idattr.utf8().data()
                   << "\" and position()=" << count << "]";
          else
            stream << "[" << count << "]";
        } else if (hasIdAttr) {
          stream << "[@id=\"" << idattr.utf8().data() << "\"]";
        }
        break;
      }
      case kTextNode:
        stream << "/text()";
        break;
      case kAttributeNode:
        stream << "/@" << node->nodeName().utf8().data();
        break;
      default:
        break;
    }
  }
}

static void appendMarkedTree(const String& baseIndent,
                             const Node* rootNode,
                             const Node* markedNode1,
                             const char* markedLabel1,
                             const Node* markedNode2,
                             const char* markedLabel2,
                             StringBuilder& builder) {
  for (const Node& node : NodeTraversal::inclusiveDescendantsOf(*rootNode)) {
    StringBuilder indent;
    if (node == markedNode1)
      indent.append(markedLabel1);
    if (node == markedNode2)
      indent.append(markedLabel2);
    indent.append(baseIndent);
    for (const Node* tmpNode = &node; tmpNode && tmpNode != rootNode;
         tmpNode = tmpNode->parentOrShadowHostNode())
      indent.append('\t');
    builder.append(indent);
    builder.append(node.toString());
    builder.append("\n");
    indent.append('\t');

    if (node.isElementNode()) {
      const Element& element = toElement(node);
      if (Element* pseudo = element.pseudoElement(PseudoIdBefore))
        appendMarkedTree(indent.toString(), pseudo, markedNode1, markedLabel1,
                         markedNode2, markedLabel2, builder);
      if (Element* pseudo = element.pseudoElement(PseudoIdAfter))
        appendMarkedTree(indent.toString(), pseudo, markedNode1, markedLabel1,
                         markedNode2, markedLabel2, builder);
      if (Element* pseudo = element.pseudoElement(PseudoIdFirstLetter))
        appendMarkedTree(indent.toString(), pseudo, markedNode1, markedLabel1,
                         markedNode2, markedLabel2, builder);
      if (Element* pseudo = element.pseudoElement(PseudoIdBackdrop))
        appendMarkedTree(indent.toString(), pseudo, markedNode1, markedLabel1,
                         markedNode2, markedLabel2, builder);
    }

    if (node.isShadowRoot()) {
      if (ShadowRoot* youngerShadowRoot =
              toShadowRoot(node).youngerShadowRoot())
        appendMarkedTree(indent.toString(), youngerShadowRoot, markedNode1,
                         markedLabel1, markedNode2, markedLabel2, builder);
    } else if (ShadowRoot* oldestShadowRoot = oldestShadowRootFor(&node)) {
      appendMarkedTree(indent.toString(), oldestShadowRoot, markedNode1,
                       markedLabel1, markedNode2, markedLabel2, builder);
    }
  }
}

static void appendMarkedFlatTree(const String& baseIndent,
                                 const Node* rootNode,
                                 const Node* markedNode1,
                                 const char* markedLabel1,
                                 const Node* markedNode2,
                                 const char* markedLabel2,
                                 StringBuilder& builder) {
  for (const Node* node = rootNode; node;
       node = FlatTreeTraversal::nextSibling(*node)) {
    StringBuilder indent;
    if (node == markedNode1)
      indent.append(markedLabel1);
    if (node == markedNode2)
      indent.append(markedLabel2);
    indent.append(baseIndent);
    builder.append(indent);
    builder.append(node->toString());
    builder.append("\n");
    indent.append('\t');

    if (Node* child = FlatTreeTraversal::firstChild(*node))
      appendMarkedFlatTree(indent.toString(), child, markedNode1, markedLabel1,
                           markedNode2, markedLabel2, builder);
  }
}

String Node::toMarkedTreeString(const Node* markedNode1,
                                const char* markedLabel1,
                                const Node* markedNode2,
                                const char* markedLabel2) const {
  const Node* rootNode;
  const Node* node = this;
  while (node->parentOrShadowHostNode() && !isHTMLBodyElement(*node))
    node = node->parentOrShadowHostNode();
  rootNode = node;

  StringBuilder builder;
  String startingIndent;
  appendMarkedTree(startingIndent, rootNode, markedNode1, markedLabel1,
                   markedNode2, markedLabel2, builder);
  return builder.toString();
}

String Node::toMarkedFlatTreeString(const Node* markedNode1,
                                    const char* markedLabel1,
                                    const Node* markedNode2,
                                    const char* markedLabel2) const {
  const Node* rootNode;
  const Node* node = this;
  while (node->parentOrShadowHostNode() && !isHTMLBodyElement(*node))
    node = node->parentOrShadowHostNode();
  rootNode = node;

  StringBuilder builder;
  String startingIndent;
  appendMarkedFlatTree(startingIndent, rootNode, markedNode1, markedLabel1,
                       markedNode2, markedLabel2, builder);
  return builder.toString();
}

static ContainerNode* parentOrShadowHostOrFrameOwner(const Node* node) {
  ContainerNode* parent = node->parentOrShadowHostNode();
  if (!parent && node->document().frame())
    parent = node->document().frame()->deprecatedLocalOwner();
  return parent;
}

static void printSubTreeAcrossFrame(const Node* node,
                                    const Node* markedNode,
                                    const String& indent,
                                    std::ostream& stream) {
  if (node == markedNode)
    stream << "*";
  stream << indent.utf8().data() << *node << "\n";
  if (node->isShadowRoot()) {
    if (ShadowRoot* youngerShadowRoot = toShadowRoot(node)->youngerShadowRoot())
      printSubTreeAcrossFrame(youngerShadowRoot, markedNode, indent + "\t",
                              stream);
  } else {
    if (node->isFrameOwnerElement())
      printSubTreeAcrossFrame(toHTMLFrameOwnerElement(node)->contentDocument(),
                              markedNode, indent + "\t", stream);
    if (ShadowRoot* oldestShadowRoot = oldestShadowRootFor(node))
      printSubTreeAcrossFrame(oldestShadowRoot, markedNode, indent + "\t",
                              stream);
  }
  for (const Node* child = node->firstChild(); child;
       child = child->nextSibling())
    printSubTreeAcrossFrame(child, markedNode, indent + "\t", stream);
}

void Node::showTreeForThisAcrossFrame() const {
  const Node* rootNode = this;
  while (parentOrShadowHostOrFrameOwner(rootNode))
    rootNode = parentOrShadowHostOrFrameOwner(rootNode);
  std::stringstream stream;
  printSubTreeAcrossFrame(rootNode, this, "", stream);
  LOG(INFO) << "\n" << stream.str();
}

#endif

// --------

Element* Node::enclosingLinkEventParentOrSelf() const {
  const Node* result = nullptr;
  for (const Node* node = this; node; node = FlatTreeTraversal::parent(*node)) {
    // For imagemaps, the enclosing link node is the associated area element not
    // the image itself.  So we don't let images be the enclosingLinkNode, even
    // though isLink sometimes returns true for them.
    if (node->isLink() && !isHTMLImageElement(*node)) {
      // Casting to Element is safe because only HTMLAnchorElement,
      // HTMLImageElement and SVGAElement can return true for isLink().
      result = node;
      break;
    }
  }

  return toElement(const_cast<Node*>(result));
}

const AtomicString& Node::interfaceName() const {
  return EventTargetNames::Node;
}

ExecutionContext* Node::getExecutionContext() const {
  return document().contextDocument();
}

void Node::didMoveToNewDocument(Document& oldDocument) {
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
  if (oldDocument.frameHost() && !document().frameHost())
    oldDocument.frameHost()->eventHandlerRegistry().didMoveOutOfFrameHost(
        *this);
  else if (document().frameHost() && !oldDocument.frameHost())
    document().frameHost()->eventHandlerRegistry().didMoveIntoFrameHost(*this);
  else if (oldDocument.frameHost() != document().frameHost())
    EventHandlerRegistry::didMoveBetweenFrameHosts(
        *this, oldDocument.frameHost(), document().frameHost());

  if (const HeapVector<TraceWrapperMember<MutationObserverRegistration>>*
          registry = mutationObserverRegistry()) {
    for (size_t i = 0; i < registry->size(); ++i) {
      document().addMutationObserverTypes(registry->at(i)->mutationTypes());
    }
  }

  if (transientMutationObserverRegistry()) {
    for (MutationObserverRegistration* registration :
         *transientMutationObserverRegistry())
      document().addMutationObserverTypes(registration->mutationTypes());
  }
}

void Node::addedEventListener(const AtomicString& eventType,
                              RegisteredEventListener& registeredListener) {
  EventTarget::addedEventListener(eventType, registeredListener);
  document().addListenerTypeIfNeeded(eventType);
  if (FrameHost* frameHost = document().frameHost())
    frameHost->eventHandlerRegistry().didAddEventHandler(
        *this, eventType, registeredListener.options());
}

void Node::removedEventListener(
    const AtomicString& eventType,
    const RegisteredEventListener& registeredListener) {
  EventTarget::removedEventListener(eventType, registeredListener);
  // FIXME: Notify Document that the listener has vanished. We need to keep
  // track of a number of listeners for each type, not just a bool - see
  // https://bugs.webkit.org/show_bug.cgi?id=33861
  if (FrameHost* frameHost = document().frameHost())
    frameHost->eventHandlerRegistry().didRemoveEventHandler(
        *this, eventType, registeredListener.options());
}

void Node::removeAllEventListeners() {
  if (hasEventListeners() && document().frameHost())
    document().frameHost()->eventHandlerRegistry().didRemoveAllEventHandlers(
        *this);
  EventTarget::removeAllEventListeners();
}

void Node::removeAllEventListenersRecursively() {
  ScriptForbiddenScope forbidScriptDuringRawIteration;
  for (Node& node : NodeTraversal::startsAt(*this)) {
    node.removeAllEventListeners();
    for (ShadowRoot* root = node.youngestShadowRoot(); root;
         root = root->olderShadowRoot())
      root->removeAllEventListenersRecursively();
  }
}

using EventTargetDataMap =
    PersistentHeapHashMap<WeakMember<Node>, Member<EventTargetData>>;
static EventTargetDataMap& eventTargetDataMap() {
  DEFINE_STATIC_LOCAL(EventTargetDataMap, map, ());
  return map;
}

EventTargetData* Node::eventTargetData() {
  return hasEventTargetData() ? eventTargetDataMap().get(this) : nullptr;
}

EventTargetData& Node::ensureEventTargetData() {
  if (hasEventTargetData())
    return *eventTargetDataMap().get(this);
  DCHECK(!eventTargetDataMap().contains(this));
  setHasEventTargetData(true);
  EventTargetData* data = new EventTargetData;
  eventTargetDataMap().set(this, data);
  return *data;
}

const HeapVector<TraceWrapperMember<MutationObserverRegistration>>*
Node::mutationObserverRegistry() {
  if (!hasRareData())
    return nullptr;
  NodeMutationObserverData* data = rareData()->mutationObserverData();
  if (!data)
    return nullptr;
  return &data->registry();
}

const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>*
Node::transientMutationObserverRegistry() {
  if (!hasRareData())
    return nullptr;
  NodeMutationObserverData* data = rareData()->mutationObserverData();
  if (!data)
    return nullptr;
  return &data->transientRegistry();
}

template <typename Registry>
static inline void collectMatchingObserversForMutation(
    HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&
        observers,
    Registry* registry,
    Node& target,
    MutationObserver::MutationType type,
    const QualifiedName* attributeName) {
  if (!registry)
    return;

  for (const auto& registration : *registry) {
    if (registration->shouldReceiveMutationFrom(target, type, attributeName)) {
      MutationRecordDeliveryOptions deliveryOptions =
          registration->deliveryOptions();
      HeapHashMap<Member<MutationObserver>,
                  MutationRecordDeliveryOptions>::AddResult result =
          observers.add(&registration->observer(), deliveryOptions);
      if (!result.isNewEntry)
        result.storedValue->value |= deliveryOptions;
    }
  }
}

void Node::getRegisteredMutationObserversOfType(
    HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&
        observers,
    MutationObserver::MutationType type,
    const QualifiedName* attributeName) {
  DCHECK((type == MutationObserver::Attributes && attributeName) ||
         !attributeName);
  collectMatchingObserversForMutation(observers, mutationObserverRegistry(),
                                      *this, type, attributeName);
  collectMatchingObserversForMutation(observers,
                                      transientMutationObserverRegistry(),
                                      *this, type, attributeName);
  ScriptForbiddenScope forbidScriptDuringRawIteration;
  for (Node* node = parentNode(); node; node = node->parentNode()) {
    collectMatchingObserversForMutation(observers,
                                        node->mutationObserverRegistry(), *this,
                                        type, attributeName);
    collectMatchingObserversForMutation(
        observers, node->transientMutationObserverRegistry(), *this, type,
        attributeName);
  }
}

void Node::registerMutationObserver(
    MutationObserver& observer,
    MutationObserverOptions options,
    const HashSet<AtomicString>& attributeFilter) {
  MutationObserverRegistration* registration = nullptr;
  const HeapVector<TraceWrapperMember<MutationObserverRegistration>>& registry =
      ensureRareData().ensureMutationObserverData().registry();
  for (size_t i = 0; i < registry.size(); ++i) {
    if (&registry[i]->observer() == &observer) {
      registration = registry[i].get();
      registration->resetObservation(options, attributeFilter);
    }
  }

  if (!registration) {
    registration = MutationObserverRegistration::create(observer, this, options,
                                                        attributeFilter);
    ensureRareData().ensureMutationObserverData().addRegistration(registration);
  }

  document().addMutationObserverTypes(registration->mutationTypes());
}

void Node::unregisterMutationObserver(
    MutationObserverRegistration* registration) {
  const HeapVector<TraceWrapperMember<MutationObserverRegistration>>* registry =
      mutationObserverRegistry();
  DCHECK(registry);
  if (!registry)
    return;

  // FIXME: Simplify the registration/transient registration logic to make this
  // understandable by humans.  The explicit dispose() is needed to have the
  // registration object unregister itself promptly.
  registration->dispose();
  ensureRareData().ensureMutationObserverData().removeRegistration(
      registration);
}

void Node::registerTransientMutationObserver(
    MutationObserverRegistration* registration) {
  ensureRareData().ensureMutationObserverData().addTransientRegistration(
      registration);
}

void Node::unregisterTransientMutationObserver(
    MutationObserverRegistration* registration) {
  const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>*
      transientRegistry = transientMutationObserverRegistry();
  DCHECK(transientRegistry);
  if (!transientRegistry)
    return;

  ensureRareData().ensureMutationObserverData().removeTransientRegistration(
      registration);
}

void Node::notifyMutationObserversNodeWillDetach() {
  if (!document().hasMutationObservers())
    return;

  ScriptForbiddenScope forbidScriptDuringRawIteration;
  for (Node* node = parentNode(); node; node = node->parentNode()) {
    if (const HeapVector<TraceWrapperMember<MutationObserverRegistration>>*
            registry = node->mutationObserverRegistry()) {
      const size_t size = registry->size();
      for (size_t i = 0; i < size; ++i)
        registry->at(i)->observedSubtreeNodeWillDetach(*this);
    }

    if (const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>*
            transientRegistry = node->transientMutationObserverRegistry()) {
      for (auto& registration : *transientRegistry)
        registration->observedSubtreeNodeWillDetach(*this);
    }
  }
}

void Node::handleLocalEvents(Event& event) {
  if (!hasEventTargetData())
    return;

  if (isDisabledFormControl(this) && event.isMouseEvent())
    return;

  fireEventListeners(&event);
}

void Node::dispatchScopedEvent(Event* event) {
  event->setTrusted(true);
  EventDispatcher::dispatchScopedEvent(*this, event->createMediator());
}

DispatchEventResult Node::dispatchEventInternal(Event* event) {
  return EventDispatcher::dispatchEvent(*this, event->createMediator());
}

void Node::dispatchSubtreeModifiedEvent() {
  if (isInShadowTree())
    return;

#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif

  if (!document().hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
    return;

  dispatchScopedEvent(
      MutationEvent::create(EventTypeNames::DOMSubtreeModified, true));
}

DispatchEventResult Node::dispatchDOMActivateEvent(int detail,
                                                   Event& underlyingEvent) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif
  UIEvent* event = UIEvent::create();
  event->initUIEvent(EventTypeNames::DOMActivate, true, true,
                     document().domWindow(), detail);
  event->setUnderlyingEvent(&underlyingEvent);
  event->setComposed(underlyingEvent.composed());
  dispatchScopedEvent(event);

  // TODO(dtapuska): Dispatching scoped events shouldn't check the return
  // type because the scoped event could get put off in the delayed queue.
  return EventTarget::dispatchEventResult(*event);
}

void Node::createAndDispatchPointerEvent(const AtomicString& mouseEventName,
                                         const PlatformMouseEvent& mouseEvent,
                                         LocalDOMWindow* view) {
  AtomicString pointerEventName;
  if (mouseEventName == EventTypeNames::mousemove)
    pointerEventName = EventTypeNames::pointermove;
  else if (mouseEventName == EventTypeNames::mousedown)
    pointerEventName = EventTypeNames::pointerdown;
  else if (mouseEventName == EventTypeNames::mouseup)
    pointerEventName = EventTypeNames::pointerup;
  else
    return;

  PointerEventInit pointerEventInit;

  pointerEventInit.setPointerId(PointerEventFactory::s_mouseId);
  pointerEventInit.setPointerType("mouse");
  pointerEventInit.setIsPrimary(true);
  pointerEventInit.setButtons(
      MouseEvent::platformModifiersToButtons(mouseEvent.getModifiers()));

  pointerEventInit.setBubbles(true);
  pointerEventInit.setCancelable(true);
  pointerEventInit.setComposed(true);
  pointerEventInit.setDetail(0);

  pointerEventInit.setScreenX(mouseEvent.globalPosition().x());
  pointerEventInit.setScreenY(mouseEvent.globalPosition().y());

  IntPoint locationInFrameZoomed;
  if (view && view->frame() && view->frame()->view()) {
    LocalFrame* frame = view->frame();
    FrameView* frameView = frame->view();
    IntPoint locationInContents =
        frameView->rootFrameToContents(mouseEvent.position());
    locationInFrameZoomed = frameView->contentsToFrame(locationInContents);
    float scaleFactor = 1 / frame->pageZoomFactor();
    locationInFrameZoomed.scale(scaleFactor, scaleFactor);
  }

  // Set up initial values for coordinates.
  pointerEventInit.setClientX(locationInFrameZoomed.x());
  pointerEventInit.setClientY(locationInFrameZoomed.y());

  if (pointerEventName == EventTypeNames::pointerdown ||
      pointerEventName == EventTypeNames::pointerup) {
    pointerEventInit.setButton(
        static_cast<int>(mouseEvent.pointerProperties().button));
  } else {
    pointerEventInit.setButton(
        static_cast<int>(WebPointerProperties::Button::NoButton));
  }

  UIEventWithKeyState::setFromPlatformModifiers(pointerEventInit,
                                                mouseEvent.getModifiers());
  pointerEventInit.setView(view);

  dispatchEvent(PointerEvent::create(pointerEventName, pointerEventInit));
}

void Node::dispatchMouseEvent(const PlatformMouseEvent& nativeEvent,
                              const AtomicString& mouseEventType,
                              int detail,
                              Node* relatedTarget) {
  createAndDispatchPointerEvent(mouseEventType, nativeEvent,
                                document().domWindow());
  dispatchEvent(MouseEvent::create(mouseEventType, document().domWindow(),
                                   nativeEvent, detail, relatedTarget));
}

void Node::dispatchSimulatedClick(Event* underlyingEvent,
                                  SimulatedClickMouseEventOptions eventOptions,
                                  SimulatedClickCreationScope scope) {
  EventDispatcher::dispatchSimulatedClick(*this, underlyingEvent, eventOptions,
                                          scope);
}

void Node::dispatchInputEvent() {
  // Legacy 'input' event for forms set value and checked.
  dispatchScopedEvent(Event::createBubble(EventTypeNames::input));
}

void Node::defaultEventHandler(Event* event) {
  if (event->target() != this)
    return;
  const AtomicString& eventType = event->type();
  if (eventType == EventTypeNames::keydown ||
      eventType == EventTypeNames::keypress) {
    if (event->isKeyboardEvent()) {
      if (LocalFrame* frame = document().frame())
        frame->eventHandler().defaultKeyboardEventHandler(
            toKeyboardEvent(event));
    }
  } else if (eventType == EventTypeNames::click) {
    int detail =
        event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
    if (dispatchDOMActivateEvent(detail, *event) !=
        DispatchEventResult::NotCanceled)
      event->setDefaultHandled();
  } else if (eventType == EventTypeNames::contextmenu) {
    if (Page* page = document().page())
      page->contextMenuController().handleContextMenuEvent(event);
  } else if (eventType == EventTypeNames::textInput) {
    if (event->hasInterface(EventNames::TextEvent)) {
      if (LocalFrame* frame = document().frame())
        frame->eventHandler().defaultTextInputEventHandler(toTextEvent(event));
    }
  } else if (RuntimeEnabledFeatures::middleClickAutoscrollEnabled() &&
             eventType == EventTypeNames::mousedown && event->isMouseEvent()) {
    MouseEvent* mouseEvent = toMouseEvent(event);
    if (mouseEvent->button() ==
        static_cast<short>(WebPointerProperties::Button::Middle)) {
      if (enclosingLinkEventParentOrSelf())
        return;

      // Avoid that canBeScrolledAndHasScrollableArea changes layout tree
      // structure.
      // FIXME: We should avoid synchronous layout if possible. We can
      // remove this synchronous layout if we avoid synchronous layout in
      // LayoutTextControlSingleLine::scrollHeight
      document().updateStyleAndLayoutIgnorePendingStylesheets();
      LayoutObject* layoutObject = this->layoutObject();
      while (
          layoutObject &&
          (!layoutObject->isBox() ||
           !toLayoutBox(layoutObject)->canBeScrolledAndHasScrollableArea())) {
        if (layoutObject->node() && layoutObject->node()->isDocumentNode()) {
          Element* owner = toDocument(layoutObject->node())->localOwner();
          layoutObject = owner ? owner->layoutObject() : nullptr;
        } else {
          layoutObject = layoutObject->parent();
        }
      }
      if (layoutObject) {
        if (LocalFrame* frame = document().frame())
          frame->eventHandler().startMiddleClickAutoscroll(layoutObject);
      }
    }
  } else if (event->type() == EventTypeNames::webkitEditableContentChanged) {
    // TODO(chongz): Remove after shipped.
    // New InputEvent are dispatched in Editor::appliedEditing, etc.
    if (!RuntimeEnabledFeatures::inputEventEnabled())
      dispatchInputEvent();
  }
}

void Node::willCallDefaultEventHandler(const Event&) {}

bool Node::willRespondToMouseMoveEvents() {
  if (isDisabledFormControl(this))
    return false;
  return hasEventListeners(EventTypeNames::mousemove) ||
         hasEventListeners(EventTypeNames::mouseover) ||
         hasEventListeners(EventTypeNames::mouseout);
}

bool Node::willRespondToMouseClickEvents() {
  if (isDisabledFormControl(this))
    return false;
  document().updateStyleAndLayoutTree();
  return hasEditableStyle(*this) ||
         hasEventListeners(EventTypeNames::mouseup) ||
         hasEventListeners(EventTypeNames::mousedown) ||
         hasEventListeners(EventTypeNames::click) ||
         hasEventListeners(EventTypeNames::DOMActivate);
}

bool Node::willRespondToTouchEvents() {
  if (isDisabledFormControl(this))
    return false;
  return hasEventListeners(EventTypeNames::touchstart) ||
         hasEventListeners(EventTypeNames::touchmove) ||
         hasEventListeners(EventTypeNames::touchcancel) ||
         hasEventListeners(EventTypeNames::touchend);
}

unsigned Node::connectedSubframeCount() const {
  return hasRareData() ? rareData()->connectedSubframeCount() : 0;
}

void Node::incrementConnectedSubframeCount() {
  DCHECK(isContainerNode());
  ensureRareData().incrementConnectedSubframeCount();
}

void Node::decrementConnectedSubframeCount() {
  rareData()->decrementConnectedSubframeCount();
}

StaticNodeList* Node::getDestinationInsertionPoints() {
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

HTMLSlotElement* Node::assignedSlot() const {
  DCHECK(!isPseudoElement());
  if (ShadowRoot* root = v1ShadowRootOfParent())
    return root->ensureSlotAssignment().findSlot(*this);
  return nullptr;
}

HTMLSlotElement* Node::assignedSlotForBinding() {
  updateDistribution();
  if (ShadowRoot* root = v1ShadowRootOfParent()) {
    if (root->type() == ShadowRootType::Open)
      return root->ensureSlotAssignment().findSlot(*this);
  }
  return nullptr;
}

void Node::setFocused(bool flag) {
  document().userActionElements().setFocused(this, flag);
}

void Node::setActive(bool flag) {
  document().userActionElements().setActive(this, flag);
}

void Node::setDragged(bool flag) {
  document().userActionElements().setDragged(this, flag);
}

void Node::setHovered(bool flag) {
  document().userActionElements().setHovered(this, flag);
}

bool Node::isUserActionElementActive() const {
  DCHECK(isUserActionElement());
  return document().userActionElements().isActive(this);
}

bool Node::isUserActionElementInActiveChain() const {
  DCHECK(isUserActionElement());
  return document().userActionElements().isInActiveChain(this);
}

bool Node::isUserActionElementDragged() const {
  DCHECK(isUserActionElement());
  return document().userActionElements().isDragged(this);
}

bool Node::isUserActionElementHovered() const {
  DCHECK(isUserActionElement());
  return document().userActionElements().isHovered(this);
}

bool Node::isUserActionElementFocused() const {
  DCHECK(isUserActionElement());
  return document().userActionElements().isFocused(this);
}

void Node::setCustomElementState(CustomElementState newState) {
  CustomElementState oldState = getCustomElementState();

  switch (newState) {
    case CustomElementState::Uncustomized:
      NOTREACHED();  // Everything starts in this state
      return;

    case CustomElementState::Undefined:
      DCHECK_EQ(CustomElementState::Uncustomized, oldState);
      break;

    case CustomElementState::Custom:
      DCHECK_EQ(CustomElementState::Undefined, oldState);
      break;

    case CustomElementState::Failed:
      DCHECK_NE(CustomElementState::Failed, oldState);
      break;
  }

  DCHECK(isHTMLElement());
  DCHECK_NE(V0Upgraded, getV0CustomElementState());

  Element* element = toElement(this);
  bool wasDefined = element->isDefined();

  m_nodeFlags = (m_nodeFlags & ~CustomElementStateMask) |
                static_cast<NodeFlags>(newState);
  DCHECK(newState == getCustomElementState());

  if (element->isDefined() != wasDefined)
    element->pseudoStateChanged(CSSSelector::PseudoDefined);
}

void Node::setV0CustomElementState(V0CustomElementState newState) {
  V0CustomElementState oldState = getV0CustomElementState();

  switch (newState) {
    case V0NotCustomElement:
      NOTREACHED();  // Everything starts in this state
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

void Node::checkSlotChange(SlotChangeType slotChangeType) {
  // Common check logic is used in both cases, "after inserted" and "before
  // removed".
  if (!isSlotable())
    return;
  if (ShadowRoot* root = v1ShadowRootOfParent()) {
    // Relevant DOM Standard:
    // https://dom.spec.whatwg.org/#concept-node-insert
    // - 6.1.2: If parent is a shadow host and node is a slotable, then assign a
    //   slot for node.
    // https://dom.spec.whatwg.org/#concept-node-remove
    // - 10. If node is assigned, then run assign slotables for node’s assigned
    //   slot.

    // Although DOM Standard requires "assign a slot for node / run assign
    // slotables" at this timing, we skip it as an optimization.
    if (HTMLSlotElement* slot = root->ensureSlotAssignment().findSlot(*this))
      slot->didSlotChange(slotChangeType);
  } else {
    // Relevant DOM Standard:
    // https://dom.spec.whatwg.org/#concept-node-insert
    // - 6.1.3: If parent is a slot whose assigned nodes is the empty list, then
    //   run signal a slot change for parent.
    // https://dom.spec.whatwg.org/#concept-node-remove
    // - 11. If parent is a slot whose assigned nodes is the empty list, then
    //   run signal a slot change for parent.
    Element* parent = parentElement();
    if (parent && isHTMLSlotElement(parent)) {
      HTMLSlotElement& parentSlot = toHTMLSlotElement(*parent);
      // TODO(hayato): Support slotchange for slots in non-shadow trees.
      if (ShadowRoot* root = containingShadowRoot()) {
        if (root && root->isV1() && !parentSlot.hasAssignedNodesSlow())
          parentSlot.didSlotChange(slotChangeType);
      }
    }
  }
}

DEFINE_TRACE(Node) {
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

DEFINE_TRACE_WRAPPERS(Node) {
  visitor->traceWrappersWithManualWriteBarrier(m_parentOrShadowHostNode);
  visitor->traceWrappersWithManualWriteBarrier(m_previous);
  visitor->traceWrappersWithManualWriteBarrier(m_next);
  if (hasRareData())
    visitor->traceWrappers(rareData());
  EventTarget::traceWrappers(visitor);
}

unsigned Node::lengthOfContents() const {
  // This switch statement must be consistent with that of
  // Range::processContentsBetweenOffsets.
  switch (getNodeType()) {
    case Node::kTextNode:
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kProcessingInstructionNode:
      return toCharacterData(this)->length();
    case Node::kElementNode:
    case Node::kDocumentNode:
    case Node::kDocumentFragmentNode:
      return toContainerNode(this)->countChildren();
    case Node::kAttributeNode:
    case Node::kDocumentTypeNode:
      return 0;
  }
  NOTREACHED();
  return 0;
}

DISABLE_CFI_PERF
v8::Local<v8::Object> Node::wrap(v8::Isolate* isolate,
                                 v8::Local<v8::Object> creationContext) {
  DCHECK(!DOMDataStore::containsWrapper(this, isolate));

  const WrapperTypeInfo* wrapperType = wrapperTypeInfo();

  v8::Local<v8::Object> wrapper =
      V8DOMWrapper::createWrapper(isolate, creationContext, wrapperType);
  DCHECK(!wrapper.IsEmpty());
  return associateWithWrapper(isolate, wrapperType, wrapper);
}

v8::Local<v8::Object> Node::associateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapperType,
    v8::Local<v8::Object> wrapper) {
  return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperType,
                                                  wrapper);
}

}  // namespace blink

#ifndef NDEBUG

void showNode(const blink::Node* node) {
  if (node)
    LOG(INFO) << *node;
  else
    LOG(INFO) << "Cannot showNode for <null>";
}

void showTree(const blink::Node* node) {
  if (node)
    LOG(INFO) << "\n" << node->toTreeStringForThis().utf8().data();
  else
    LOG(INFO) << "Cannot showTree for <null>";
}

void showNodePath(const blink::Node* node) {
  if (node) {
    std::stringstream stream;
    node->printNodePathTo(stream);
    LOG(INFO) << stream.str();
  } else {
    LOG(INFO) << "Cannot showNodePath for <null>";
  }
}

#endif
