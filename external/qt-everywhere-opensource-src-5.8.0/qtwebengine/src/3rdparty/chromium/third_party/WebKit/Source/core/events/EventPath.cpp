/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/events/EventPath.h"

#include "core/EventNames.h"
#include "core/dom/Document.h"
#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/TouchEvent.h"
#include "core/events/TouchEventContext.h"
#include "core/html/HTMLSlotElement.h"
#include "core/svg/SVGUseElement.h"

namespace blink {

EventTarget* EventPath::eventTargetRespectingTargetRules(Node& referenceNode)
{
    if (referenceNode.isPseudoElement()) {
        ASSERT(referenceNode.parentNode());
        return referenceNode.parentNode();
    }

    return &referenceNode;
}

static inline bool shouldStopAtShadowRoot(Event& event, ShadowRoot& shadowRoot, EventTarget& target)
{
    if (shadowRoot.isV1()) {
        // In v1, an event is scoped by default unless event.composed flag is set.
        return !event.composed() && target.toNode() && target.toNode()->shadowHost() == shadowRoot.host();
    }
    // Ignores event.composed() for v0.
    // Instead, use event.isScopedInV0() for backward compatibility.
    return event.isScopedInV0() && target.toNode() && target.toNode()->shadowHost() == shadowRoot.host();
}

EventPath::EventPath(Node& node, Event* event)
    : m_node(node)
    , m_event(event)
{
    initialize();
}

void EventPath::initializeWith(Node& node, Event* event)
{
    m_node = &node;
    m_event = event;
    m_windowEventContext = nullptr;
    m_nodeEventContexts.clear();
    m_treeScopeEventContexts.clear();
    initialize();
}

static inline bool eventPathShouldBeEmptyFor(Node& node, Event* event)
{
    if (node.isPseudoElement() && !node.parentElement())
        return true;

    // Do not dispatch non-composed events in SVG use trees.
    if (node.isSVGElement()) {
        if (toSVGElement(node).inUseShadowTree() && event && !event->composed())
            return true;
    }

    return false;
}

void EventPath::initialize()
{
    if (eventPathShouldBeEmptyFor(*m_node, m_event))
        return;

    calculatePath();
    calculateAdjustedTargets();
    calculateTreeOrderAndSetNearestAncestorClosedTree();
}

void EventPath::calculatePath()
{
    ASSERT(m_node);
    ASSERT(m_nodeEventContexts.isEmpty());
    m_node->updateDistribution();

    // For performance and memory usage reasons we want to store the
    // path using as few bytes as possible and with as few allocations
    // as possible which is why we gather the data on the stack before
    // storing it in a perfectly sized m_nodeEventContexts Vector.
    HeapVector<Member<Node>, 64> nodesInPath;
    Node* current = m_node;

    // Exclude nodes in SVG <use>'s shadow tree from event path.
    // See crbug.com/630870
    while (current->isSVGElement()) {
        SVGUseElement* correspondingUseElement = toSVGElement(current)->correspondingUseElement();
        if (!correspondingUseElement)
            break;
        current = correspondingUseElement;
    }

    nodesInPath.append(current);
    while (current) {
        if (m_event && current->keepEventInNode(m_event))
            break;
        HeapVector<Member<InsertionPoint>, 8> insertionPoints;
        collectDestinationInsertionPoints(*current, insertionPoints);
        if (!insertionPoints.isEmpty()) {
            for (const auto& insertionPoint : insertionPoints) {
                if (insertionPoint->isShadowInsertionPoint()) {
                    ShadowRoot* containingShadowRoot = insertionPoint->containingShadowRoot();
                    ASSERT(containingShadowRoot);
                    if (!containingShadowRoot->isOldest())
                        nodesInPath.append(containingShadowRoot->olderShadowRoot());
                }
                nodesInPath.append(insertionPoint);
            }
            current = insertionPoints.last();
            continue;
        }
        if (current->isChildOfV1ShadowHost()) {
            if (HTMLSlotElement* slot = current->assignedSlot()) {
                current = slot;
                nodesInPath.append(current);
                continue;
            }
        }
        if (current->isShadowRoot()) {
            if (m_event && shouldStopAtShadowRoot(*m_event, *toShadowRoot(current), *m_node))
                break;
            current = current->shadowHost();
            nodesInPath.append(current);
        } else {
            current = current->parentNode();
            if (current)
                nodesInPath.append(current);
        }
    }

    m_nodeEventContexts.reserveCapacity(nodesInPath.size());
    for (Node* nodeInPath : nodesInPath) {
        m_nodeEventContexts.append(NodeEventContext(nodeInPath, eventTargetRespectingTargetRules(*nodeInPath)));
    }
}

void EventPath::calculateTreeOrderAndSetNearestAncestorClosedTree()
{
    // Precondition:
    //   - TreeScopes in m_treeScopeEventContexts must be *connected* in the same composed tree.
    //   - The root tree must be included.
    HeapHashMap<Member<const TreeScope>, Member<TreeScopeEventContext>> treeScopeEventContextMap;
    for (const auto& treeScopeEventContext : m_treeScopeEventContexts)
        treeScopeEventContextMap.add(&treeScopeEventContext->treeScope(), treeScopeEventContext.get());
    TreeScopeEventContext* rootTree = nullptr;
    for (const auto& treeScopeEventContext : m_treeScopeEventContexts) {
        // Use olderShadowRootOrParentTreeScope here for parent-child relationships.
        // See the definition of trees of trees in the Shadow DOM spec:
        // http://w3c.github.io/webcomponents/spec/shadow/
        TreeScope* parent = treeScopeEventContext.get()->treeScope().olderShadowRootOrParentTreeScope();
        if (!parent) {
            ASSERT(!rootTree);
            rootTree = treeScopeEventContext.get();
            continue;
        }
        ASSERT(treeScopeEventContextMap.find(parent) != treeScopeEventContextMap.end());
        treeScopeEventContextMap.find(parent)->value->addChild(*treeScopeEventContext.get());
    }
    ASSERT(rootTree);
    rootTree->calculateTreeOrderAndSetNearestAncestorClosedTree(0, nullptr);
}

TreeScopeEventContext* EventPath::ensureTreeScopeEventContext(Node* currentTarget, TreeScope* treeScope, TreeScopeEventContextMap& treeScopeEventContextMap)
{
    if (!treeScope)
        return nullptr;
    TreeScopeEventContext* treeScopeEventContext;
    bool isNewEntry;
    {
        TreeScopeEventContextMap::AddResult addResult = treeScopeEventContextMap.add(treeScope, nullptr);
        isNewEntry = addResult.isNewEntry;
        if (isNewEntry)
            addResult.storedValue->value = TreeScopeEventContext::create(*treeScope);
        treeScopeEventContext = addResult.storedValue->value.get();
    }
    if (isNewEntry) {
        TreeScopeEventContext* parentTreeScopeEventContext = ensureTreeScopeEventContext(0, treeScope->olderShadowRootOrParentTreeScope(), treeScopeEventContextMap);
        if (parentTreeScopeEventContext && parentTreeScopeEventContext->target()) {
            treeScopeEventContext->setTarget(parentTreeScopeEventContext->target());
        } else if (currentTarget) {
            treeScopeEventContext->setTarget(eventTargetRespectingTargetRules(*currentTarget));
        }
    } else if (!treeScopeEventContext->target() && currentTarget) {
        treeScopeEventContext->setTarget(eventTargetRespectingTargetRules(*currentTarget));
    }
    return treeScopeEventContext;
}

void EventPath::calculateAdjustedTargets()
{
    const TreeScope* lastTreeScope = nullptr;

    TreeScopeEventContextMap treeScopeEventContextMap;
    TreeScopeEventContext* lastTreeScopeEventContext = nullptr;

    for (size_t i = 0; i < size(); ++i) {
        Node* currentNode = at(i).node();
        TreeScope& currentTreeScope = currentNode->treeScope();
        if (lastTreeScope != &currentTreeScope) {
            lastTreeScopeEventContext = ensureTreeScopeEventContext(currentNode, &currentTreeScope, treeScopeEventContextMap);
        }
        ASSERT(lastTreeScopeEventContext);
        at(i).setTreeScopeEventContext(lastTreeScopeEventContext);
        lastTreeScope = &currentTreeScope;
    }
    m_treeScopeEventContexts.appendRange(treeScopeEventContextMap.values().begin(), treeScopeEventContextMap.values().end());
}

void EventPath::buildRelatedNodeMap(const Node& relatedNode, RelatedTargetMap& relatedTargetMap)
{
    EventPath* relatedTargetEventPath = new EventPath(const_cast<Node&>(relatedNode));
    for (size_t i = 0; i < relatedTargetEventPath->m_treeScopeEventContexts.size(); ++i) {
        TreeScopeEventContext* treeScopeEventContext = relatedTargetEventPath->m_treeScopeEventContexts[i].get();
        relatedTargetMap.add(&treeScopeEventContext->treeScope(), treeScopeEventContext->target());
    }
    // Oilpan: It is important to explicitly clear the vectors to reuse
    // the memory in subsequent event dispatchings.
    relatedTargetEventPath->clear();
}

EventTarget* EventPath::findRelatedNode(TreeScope& scope, RelatedTargetMap& relatedTargetMap)
{
    HeapVector<Member<TreeScope>, 32> parentTreeScopes;
    EventTarget* relatedNode = nullptr;
    for (TreeScope* current = &scope; current; current = current->olderShadowRootOrParentTreeScope()) {
        parentTreeScopes.append(current);
        RelatedTargetMap::const_iterator iter = relatedTargetMap.find(current);
        if (iter != relatedTargetMap.end() && iter->value) {
            relatedNode = iter->value;
            break;
        }
    }
    ASSERT(relatedNode);
    for (const auto& entry : parentTreeScopes)
        relatedTargetMap.add(entry, relatedNode);

    return relatedNode;
}

void EventPath::adjustForRelatedTarget(Node& target, EventTarget* relatedTarget)
{
    if (!relatedTarget)
        return;
    Node* relatedNode = relatedTarget->toNode();
    if (!relatedNode)
        return;
    if (target.document() != relatedNode->document())
        return;
    retargetRelatedTarget(*relatedNode);
    shrinkForRelatedTarget(target, *relatedNode);
}

void EventPath::retargetRelatedTarget(const Node& relatedTargetNode)
{
    RelatedTargetMap relatedNodeMap;
    buildRelatedNodeMap(relatedTargetNode, relatedNodeMap);

    for (const auto& treeScopeEventContext : m_treeScopeEventContexts) {
        EventTarget* adjustedRelatedTarget = findRelatedNode(treeScopeEventContext->treeScope(), relatedNodeMap);
        ASSERT(adjustedRelatedTarget);
        treeScopeEventContext.get()->setRelatedTarget(adjustedRelatedTarget);
    }
}

void EventPath::shrinkForRelatedTarget(const Node& target, const Node& relatedTarget)
{
    if (!target.isInShadowTree() && !relatedTarget.isInShadowTree())
        return;
    for (size_t i = 0; i < size(); ++i) {
        if (at(i).target() == at(i).relatedTarget()) {
            // Event dispatching should be stopped here.
            shrink(i);
            break;
        }
    }
}

void EventPath::adjustForTouchEvent(TouchEvent& touchEvent)
{
    HeapVector<Member<TouchList>> adjustedTouches;
    HeapVector<Member<TouchList>> adjustedTargetTouches;
    HeapVector<Member<TouchList>> adjustedChangedTouches;
    HeapVector<Member<TreeScope>> treeScopes;

    for (const auto& treeScopeEventContext : m_treeScopeEventContexts) {
        TouchEventContext* touchEventContext = treeScopeEventContext->ensureTouchEventContext();
        adjustedTouches.append(&touchEventContext->touches());
        adjustedTargetTouches.append(&touchEventContext->targetTouches());
        adjustedChangedTouches.append(&touchEventContext->changedTouches());
        treeScopes.append(&treeScopeEventContext->treeScope());
    }

    adjustTouchList(touchEvent.touches(), adjustedTouches, treeScopes);
    adjustTouchList(touchEvent.targetTouches(), adjustedTargetTouches, treeScopes);
    adjustTouchList(touchEvent.changedTouches(), adjustedChangedTouches, treeScopes);

#if ENABLE(ASSERT)
    for (const auto& treeScopeEventContext : m_treeScopeEventContexts) {
        TreeScope& treeScope = treeScopeEventContext->treeScope();
        TouchEventContext* touchEventContext = treeScopeEventContext->touchEventContext();
        checkReachability(treeScope, touchEventContext->touches());
        checkReachability(treeScope, touchEventContext->targetTouches());
        checkReachability(treeScope, touchEventContext->changedTouches());
    }
#endif
}

void EventPath::adjustTouchList(const TouchList* touchList, HeapVector<Member<TouchList>> adjustedTouchList, const HeapVector<Member<TreeScope>>& treeScopes)
{
    if (!touchList)
        return;
    for (size_t i = 0; i < touchList->length(); ++i) {
        const Touch& touch = *touchList->item(i);
        if (!touch.target())
            continue;

        Node* targetNode = touch.target()->toNode();
        if (!targetNode)
            continue;

        RelatedTargetMap relatedNodeMap;
        buildRelatedNodeMap(*targetNode, relatedNodeMap);
        for (size_t j = 0; j < treeScopes.size(); ++j) {
            adjustedTouchList[j]->append(touch.cloneWithNewTarget(findRelatedNode(*treeScopes[j], relatedNodeMap)));
        }
    }
}

const NodeEventContext& EventPath::topNodeEventContext()
{
    ASSERT(!isEmpty());
    return last();
}

void EventPath::ensureWindowEventContext()
{
    ASSERT(m_event);
    if (!m_windowEventContext)
        m_windowEventContext = new WindowEventContext(*m_event, topNodeEventContext());
}

#if ENABLE(ASSERT)
void EventPath::checkReachability(TreeScope& treeScope, TouchList& touchList)
{
    for (size_t i = 0; i < touchList.length(); ++i)
        ASSERT(touchList.item(i)->target()->toNode()->treeScope().isInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(treeScope));
}
#endif

DEFINE_TRACE(EventPath)
{
    visitor->trace(m_nodeEventContexts);
    visitor->trace(m_node);
    visitor->trace(m_event);
    visitor->trace(m_treeScopeEventContexts);
    visitor->trace(m_windowEventContext);
}

} // namespace blink
