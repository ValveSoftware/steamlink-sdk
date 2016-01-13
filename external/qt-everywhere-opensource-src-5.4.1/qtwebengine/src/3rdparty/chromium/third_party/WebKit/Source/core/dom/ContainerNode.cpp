/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "core/dom/ContainerNode.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ChildFrameDisconnector.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ClassCollection.h"
#include "core/dom/ContainerNodeAlgorithms.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/NameNodeList.h"
#include "core/dom/NoEventDispatchAssertion.h"
#include "core/dom/NodeChildRemovalTracker.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ScriptForbiddenScope.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/MutationEvent.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/RadioNodeList.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

using namespace HTMLNames;

static void dispatchChildInsertionEvents(Node&);
static void dispatchChildRemovalEvents(Node&);

#ifndef NDEBUG
unsigned NoEventDispatchAssertion::s_count = 0;
#endif

static void collectChildrenAndRemoveFromOldParent(Node& node, NodeVector& nodes, ExceptionState& exceptionState)
{
    if (!node.isDocumentFragment()) {
        nodes.append(&node);
        if (ContainerNode* oldParent = node.parentNode())
            oldParent->removeChild(&node, exceptionState);
        return;
    }
    getChildNodes(node, nodes);
    toContainerNode(node).removeChildren();
}

#if !ENABLE(OILPAN)
void ContainerNode::removeDetachedChildren()
{
    ASSERT(!connectedSubframeCount());
    ASSERT(needsAttach());
    removeDetachedChildrenInContainer<Node, ContainerNode>(*this);
}
#endif

void ContainerNode::parserTakeAllChildrenFrom(ContainerNode& oldParent)
{
    while (RefPtrWillBeRawPtr<Node> child = oldParent.firstChild()) {
        oldParent.parserRemoveChild(*child);
        treeScope().adoptIfNeeded(*child);
        parserAppendChild(child.get());
    }
}

ContainerNode::~ContainerNode()
{
    ASSERT(needsAttach());
#if !ENABLE(OILPAN)
    willBeDeletedFromDocument();
    removeDetachedChildren();
#endif
}

bool ContainerNode::isChildTypeAllowed(const Node& child) const
{
    if (!child.isDocumentFragment())
        return childTypeAllowed(child.nodeType());

    for (Node* node = toDocumentFragment(child).firstChild(); node; node = node->nextSibling()) {
        if (!childTypeAllowed(node->nodeType()))
            return false;
    }
    return true;
}

bool ContainerNode::containsConsideringHostElements(const Node& newChild) const
{
    if (isInShadowTree() || document().isTemplateDocument())
        return newChild.containsIncludingHostElements(*this);
    return newChild.contains(this);
}

bool ContainerNode::checkAcceptChild(const Node* newChild, const Node* oldChild, ExceptionState& exceptionState) const
{
    // Not mentioned in spec: throw NotFoundError if newChild is null
    if (!newChild) {
        exceptionState.throwDOMException(NotFoundError, "The new child element is null.");
        return false;
    }

    // Use common case fast path if possible.
    if ((newChild->isElementNode() || newChild->isTextNode()) && isElementNode()) {
        ASSERT(isChildTypeAllowed(*newChild));
        if (containsConsideringHostElements(*newChild)) {
            exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
            return false;
        }
        return true;
    }

    // This should never happen, but also protect release builds from tree corruption.
    ASSERT(!newChild->isPseudoElement());
    if (newChild->isPseudoElement()) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element is a pseudo-element.");
        return false;
    }

    if (containsConsideringHostElements(*newChild)) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
        return false;
    }

    if (oldChild && isDocumentNode()) {
        if (!toDocument(this)->canReplaceChild(*newChild, *oldChild)) {
            // FIXME: Adjust 'Document::canReplaceChild' to return some additional detail (or an error message).
            exceptionState.throwDOMException(HierarchyRequestError, "Failed to replace child.");
            return false;
        }
    } else if (!isChildTypeAllowed(*newChild)) {
        exceptionState.throwDOMException(HierarchyRequestError, "Nodes of type '" + newChild->nodeName() + "' may not be inserted inside nodes of type '" + nodeName() + "'.");
        return false;
    }

    return true;
}

bool ContainerNode::checkAcceptChildGuaranteedNodeTypes(const Node& newChild, ExceptionState& exceptionState) const
{
    ASSERT(isChildTypeAllowed(newChild));
    if (newChild.contains(this)) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
        return false;
    }
    return true;
}

void ContainerNode::insertBefore(PassRefPtrWillBeRawPtr<Node> newChild, Node* refChild, ExceptionState& exceptionState)
{
#if !ENABLE(OILPAN)
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());
#endif

    RefPtrWillBeRawPtr<Node> protect(this);

    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild) {
        appendChild(newChild, exceptionState);
        return;
    }

    // Make sure adding the new child is OK.
    if (!checkAcceptChild(newChild.get(), 0, exceptionState))
        return;
    ASSERT(newChild);

    // NotFoundError: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node before which the new node is to be inserted is not a child of this node.");
        return;
    }

    if (refChild->previousSibling() == newChild || refChild == newChild) // nothing to do
        return;

    RefPtrWillBeRawPtr<Node> next = refChild;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return;
    if (targets.isEmpty())
        return;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*newChild, exceptionState))
        return;

    InspectorInstrumentation::willInsertDOMNode(this);

    ChildListMutationScope mutation(*this);
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next->parentNode() != this)
            break;
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        insertBeforeCommon(*next, child);

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::insertBeforeCommon(Node& nextChild, Node& newChild)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ScriptForbiddenScope forbidScript;

    ASSERT(!newChild.parentNode()); // Use insertBefore if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild.nextSibling());
    ASSERT(!newChild.previousSibling());
    ASSERT(!newChild.isShadowRoot());

    Node* prev = nextChild.previousSibling();
    ASSERT(m_lastChild != prev);
    nextChild.setPreviousSibling(&newChild);
    if (prev) {
        ASSERT(firstChild() != nextChild);
        ASSERT(prev->nextSibling() == nextChild);
        prev->setNextSibling(&newChild);
    } else {
        ASSERT(firstChild() == nextChild);
        m_firstChild = &newChild;
    }
    newChild.setParentOrShadowHostNode(this);
    newChild.setPreviousSibling(prev);
    newChild.setNextSibling(&nextChild);
}

void ContainerNode::appendChildCommon(Node& child)
{
    child.setParentOrShadowHostNode(this);

    if (m_lastChild) {
        child.setPreviousSibling(m_lastChild);
        m_lastChild->setNextSibling(&child);
    } else {
        setFirstChild(&child);
    }

    setLastChild(&child);
}

void ContainerNode::parserInsertBefore(PassRefPtrWillBeRawPtr<Node> newChild, Node& nextChild)
{
    ASSERT(newChild);
    ASSERT(nextChild.parentNode() == this);
    ASSERT(!newChild->isDocumentFragment());
    ASSERT(!isHTMLTemplateElement(this));

    if (nextChild.previousSibling() == newChild || &nextChild == newChild) // nothing to do
        return;

    RefPtrWillBeRawPtr<Node> protect(this);

    if (document() != newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    insertBeforeCommon(nextChild, *newChild);

    newChild->updateAncestorConnectedSubframeCountForInsertion();

    ChildListMutationScope(*this).childAdded(*newChild);

    notifyNodeInserted(*newChild, ChildrenChangeSourceParser);
}

void ContainerNode::replaceChild(PassRefPtrWillBeRawPtr<Node> newChild, Node* oldChild, ExceptionState& exceptionState)
{
#if !ENABLE(OILPAN)
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());
#endif

    RefPtrWillBeRawPtr<Node> protect(this);

    if (oldChild == newChild) // nothing to do
        return;

    if (!oldChild) {
        exceptionState.throwDOMException(NotFoundError, "The node to be replaced is null.");
        return;
    }

    // Make sure replacing the old child with the new is ok
    if (!checkAcceptChild(newChild.get(), oldChild, exceptionState))
        return;

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (oldChild->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be replaced is not a child of this node.");
        return;
    }

    ChildListMutationScope mutation(*this);

    RefPtrWillBeRawPtr<Node> next = oldChild->nextSibling();

    // Remove the node we're replacing
    RefPtrWillBeRawPtr<Node> protectRemovedChild = oldChild;
    ASSERT_UNUSED(protectRemovedChild, protectRemovedChild);
    removeChild(oldChild, exceptionState);
    if (exceptionState.hadException())
        return;

    if (next && (next->previousSibling() == newChild || next == newChild)) // nothing to do
        return;

    // Does this one more time because removeChild() fires a MutationEvent.
    if (!checkAcceptChild(newChild.get(), oldChild, exceptionState))
        return;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return;

    // Does this yet another check because collectChildrenAndRemoveFromOldParent() fires a MutationEvent.
    if (!checkAcceptChild(newChild.get(), oldChild, exceptionState))
        return;

    InspectorInstrumentation::willInsertDOMNode(this);

    // Add the new child(ren)
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next && next->parentNode() != this)
            break;
        if (child.parentNode())
            break;

        treeScope().adoptIfNeeded(child);

        // Add child before "next".
        {
            NoEventDispatchAssertion assertNoEventDispatch;
            if (next)
                insertBeforeCommon(*next, child);
            else
                appendChildCommon(child);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::willRemoveChild(Node& child)
{
    ASSERT(child.parentNode() == this);
    ChildListMutationScope(*this).willRemoveChild(child);
    child.notifyMutationObserversNodeWillDetach();
    dispatchChildRemovalEvents(child);
    document().nodeWillBeRemoved(child); // e.g. mutation event listener can create a new range.
    ChildFrameDisconnector(child).disconnect();
}

void ContainerNode::willRemoveChildren()
{
    NodeVector children;
    getChildNodes(*this, children);

    ChildListMutationScope mutation(*this);
    for (NodeVector::const_iterator it = children.begin(); it != children.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;
        mutation.willRemoveChild(child);
        child.notifyMutationObserversNodeWillDetach();
        dispatchChildRemovalEvents(child);
    }

    ChildFrameDisconnector(*this).disconnect(ChildFrameDisconnector::DescendantsOnly);
}

void ContainerNode::disconnectDescendantFrames()
{
    ChildFrameDisconnector(*this).disconnect();
}

void ContainerNode::trace(Visitor* visitor)
{
    visitor->trace(m_firstChild);
    visitor->trace(m_lastChild);
    Node::trace(visitor);
}

void ContainerNode::removeChild(Node* oldChild, ExceptionState& exceptionState)
{
#if !ENABLE(OILPAN)
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());
#endif

    RefPtrWillBeRawPtr<Node> protect(this);

    // NotFoundError: Raised if oldChild is not a child of this node.
    // FIXME: We should never really get PseudoElements in here, but editing will sometimes
    // attempt to remove them still. We should fix that and enable this ASSERT.
    // ASSERT(!oldChild->isPseudoElement())
    if (!oldChild || oldChild->parentNode() != this || oldChild->isPseudoElement()) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is not a child of this node.");
        return;
    }

    RefPtrWillBeRawPtr<Node> child = oldChild;

    document().removeFocusedElementOfSubtree(child.get());

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document()))
        fullscreen->removeFullScreenElementOfSubtree(child.get());

    // Events fired when blurring currently focused node might have moved this
    // child into a different parent.
    if (child->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is no longer a child of this node. Perhaps it was moved in a 'blur' event handler?");
        return;
    }

    willRemoveChild(*child);

    // Mutation events might have moved this child into a different parent.
    if (child->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is no longer a child of this node. Perhaps it was moved in response to a mutation?");
        return;
    }

    {
        HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;

        Node* prev = child->previousSibling();
        Node* next = child->nextSibling();
        removeBetween(prev, next, *child);
        notifyNodeRemoved(*child);
        childrenChanged(false, prev, next, -1);
    }
    dispatchSubtreeModifiedEvent();
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node& oldChild)
{
    NoEventDispatchAssertion assertNoEventDispatch;

    ASSERT(oldChild.parentNode() == this);

    if (!oldChild.needsAttach())
        oldChild.detach();

    if (nextChild)
        nextChild->setPreviousSibling(previousChild);
    if (previousChild)
        previousChild->setNextSibling(nextChild);
    if (m_firstChild == &oldChild)
        m_firstChild = nextChild;
    if (m_lastChild == &oldChild)
        m_lastChild = previousChild;

    oldChild.setPreviousSibling(0);
    oldChild.setNextSibling(0);
    oldChild.setParentOrShadowHostNode(0);

    document().adoptIfNeeded(oldChild);
}

void ContainerNode::parserRemoveChild(Node& oldChild)
{
    ASSERT(oldChild.parentNode() == this);
    ASSERT(!oldChild.isDocumentFragment());

    Node* prev = oldChild.previousSibling();
    Node* next = oldChild.nextSibling();

    oldChild.updateAncestorConnectedSubframeCountForRemoval();

    ChildListMutationScope(*this).willRemoveChild(oldChild);
    oldChild.notifyMutationObserversNodeWillDetach();

    removeBetween(prev, next, oldChild);

    childrenChanged(true, prev, next, -1);
    notifyNodeRemoved(oldChild);
}

// this differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren()
{
    if (!m_firstChild)
        return;

    // The container node can be removed from event handlers.
    RefPtrWillBeRawPtr<ContainerNode> protect(this);

    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(document()))
        fullscreen->removeFullScreenElementOfSubtree(this, true);

    // Do any prep work needed before actually starting to detach
    // and remove... e.g. stop loading frames, fire unload events.
    willRemoveChildren();

    {
        // Removing focus can cause frames to load, either via events (focusout, blur)
        // or widget updates (e.g., for <embed>).
        SubframeLoadingDisabler disabler(*this);

        // Exclude this node when looking for removed focusedElement since only
        // children will be removed.
        // This must be later than willRemoveChildren, which might change focus
        // state of a child.
        document().removeFocusedElementOfSubtree(this, true);

        // Removing a node from a selection can cause widget updates.
        document().nodeChildrenWillBeRemoved(*this);
    }

    // FIXME: Remove this NodeVector. Right now WebPluginContainerImpl::m_element is a
    // raw ptr which means the code below can drop the last ref to a plugin element and
    // then the code in UpdateSuspendScope::performDeferredWidgetTreeOperations will
    // try to destroy the plugin which will be a use-after-free. We should use a RefPtr
    // in the WebPluginContainerImpl instead.
    NodeVector removedChildren;
    {
        HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;

        {
            NoEventDispatchAssertion assertNoEventDispatch;
            ScriptForbiddenScope forbidScript;

            removedChildren.reserveInitialCapacity(countChildren());

            while (RefPtrWillBeRawPtr<Node> child = m_firstChild) {
                removeBetween(0, child->nextSibling(), *child);
                removedChildren.append(child.get());
                notifyNodeRemoved(*child);
            }
        }

        childrenChanged(false, 0, 0, -static_cast<int>(removedChildren.size()));
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::appendChild(PassRefPtrWillBeRawPtr<Node> newChild, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<ContainerNode> protect(this);

#if !ENABLE(OILPAN)
    // Check that this node is not "floating".
    // If it is, it can be deleted as a side effect of sending mutation events.
    ASSERT(refCount() || parentOrShadowHostNode());
#endif

    // Make sure adding the new child is ok
    if (!checkAcceptChild(newChild.get(), 0, exceptionState))
        return;
    ASSERT(newChild);

    if (newChild == m_lastChild) // nothing to do
        return;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return;

    if (targets.isEmpty())
        return;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*newChild, exceptionState))
        return;

    InspectorInstrumentation::willInsertDOMNode(this);

    // Now actually add the child(ren)
    ChildListMutationScope mutation(*this);
    for (NodeVector::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        ASSERT(*it);
        Node& child = **it;

        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child.parentNode())
            break;

        {
            NoEventDispatchAssertion assertNoEventDispatch;
            ScriptForbiddenScope forbidScript;

            treeScope().adoptIfNeeded(child);
            appendChildCommon(child);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
}

void ContainerNode::parserAppendChild(PassRefPtrWillBeRawPtr<Node> newChild)
{
    ASSERT(newChild);
    ASSERT(!newChild->parentNode()); // Use appendChild if you need to handle reparenting (and want DOM mutation events).
    ASSERT(!newChild->isDocumentFragment());
    ASSERT(!isHTMLTemplateElement(this));

    RefPtrWillBeRawPtr<Node> protect(this);

    if (document() != newChild->document())
        document().adoptNode(newChild.get(), ASSERT_NO_EXCEPTION);

    {
        NoEventDispatchAssertion assertNoEventDispatch;
        ScriptForbiddenScope forbidScript;

        treeScope().adoptIfNeeded(*newChild);
        appendChildCommon(*newChild);
        newChild->updateAncestorConnectedSubframeCountForInsertion();
        ChildListMutationScope(*this).childAdded(*newChild);
    }

    notifyNodeInserted(*newChild, ChildrenChangeSourceParser);
}

void ContainerNode::notifyNodeInserted(Node& root, ChildrenChangeSource source)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    InspectorInstrumentation::didInsertDOMNode(&root);

    RefPtrWillBeRawPtr<Node> protect(this);
    RefPtrWillBeRawPtr<Node> protectNode(root);

    NodeVector postInsertionNotificationTargets;
    notifyNodeInsertedInternal(root, postInsertionNotificationTargets);

    // ShadowRoots are not real children, we don't need to tell host that it's
    // children changed when one is added.
    // FIXME: We should have a separate code path for ShadowRoot since it only
    // needs to call insertedInto and the rest of this logic is not needed.
    if (!root.isShadowRoot()) {
        childrenChanged(source == ChildrenChangeSourceParser, root.previousSibling(), root.nextSibling(), 1);
    }

    for (size_t i = 0; i < postInsertionNotificationTargets.size(); ++i) {
        Node* targetNode = postInsertionNotificationTargets[i].get();
        if (targetNode->inDocument())
            targetNode->didNotifySubtreeInsertionsToDocument();
    }
}

void ContainerNode::notifyNodeInsertedInternal(Node& root, NodeVector& postInsertionNotificationTargets)
{
    NoEventDispatchAssertion assertNoEventDispatch;
    ScriptForbiddenScope forbidScript;

    for (Node* node = &root; node; node = NodeTraversal::next(*node, &root)) {
        // As an optimization we don't notify leaf nodes when when inserting
        // into detached subtrees.
        if (!inDocument() && !node->isContainerNode())
            continue;
        if (Node::InsertionShouldCallDidNotifySubtreeInsertions == node->insertedInto(this))
            postInsertionNotificationTargets.append(node);
        for (ShadowRoot* shadowRoot = node->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
            notifyNodeInsertedInternal(*shadowRoot, postInsertionNotificationTargets);
    }
}

void ContainerNode::notifyNodeRemoved(Node& root)
{
    ScriptForbiddenScope forbidScript;
    NoEventDispatchAssertion assertNoEventDispatch;

    Document& document = root.document();
    for (Node* node = &root; node; node = NodeTraversal::next(*node, &root)) {
        // As an optimization we skip notifying Text nodes and other leaf nodes
        // of removal when they're not in the Document tree since the virtual
        // call to removedFrom is not needed.
        if (!node->inDocument() && !node->isContainerNode())
            continue;
        if (document.cssTarget() == node)
            document.setCSSTarget(nullptr);
        node->removedFrom(this);
        for (ShadowRoot* shadowRoot = node->youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
            notifyNodeRemoved(*shadowRoot);
    }
}

void ContainerNode::attach(const AttachContext& context)
{
    attachChildren(context);
    clearChildNeedsStyleRecalc();
    Node::attach(context);
}

void ContainerNode::detach(const AttachContext& context)
{
    detachChildren(context);
    clearChildNeedsStyleRecalc();
    Node::detach(context);
}

void ContainerNode::childrenChanged(bool changedByParser, Node*, Node*, int childCountDelta)
{
    document().incDOMTreeVersion();
    if (!changedByParser && childCountDelta)
        document().updateRangesAfterChildrenChanged(this);
    invalidateNodeListCachesInAncestors();
    if (childCountDelta > 0 && !childNeedsStyleRecalc()) {
        setChildNeedsStyleRecalc();
        markAncestorsWithChildNeedsStyleRecalc();
    }
}

void ContainerNode::cloneChildNodes(ContainerNode *clone)
{
    TrackExceptionState exceptionState;
    for (Node* n = firstChild(); n && !exceptionState.hadException(); n = n->nextSibling())
        clone->appendChild(n->cloneNode(true), exceptionState);
}


bool ContainerNode::getUpperLeftCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;
    // What is this code really trying to do?
    RenderObject* o = renderer();

    if (!o->isInline() || o->isReplaced()) {
        point = o->localToAbsolute(FloatPoint(), UseTransforms);
        return true;
    }

    // find the next text/image child, to get a position
    while (o) {
        RenderObject* p = o;
        if (RenderObject* oFirstChild = o->slowFirstChild()) {
            o = oFirstChild;
        } else if (o->nextSibling()) {
            o = o->nextSibling();
        } else {
            RenderObject* next = 0;
            while (!next && o->parent()) {
                o = o->parent();
                next = o->nextSibling();
            }
            o = next;

            if (!o)
                break;
        }
        ASSERT(o);

        if (!o->isInline() || o->isReplaced()) {
            point = o->localToAbsolute(FloatPoint(), UseTransforms);
            return true;
        }

        if (p->node() && p->node() == this && o->isText() && !o->isBR() && !toRenderText(o)->firstTextBox()) {
            // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        } else if ((o->isText() && !o->isBR()) || o->isReplaced()) {
            point = FloatPoint();
            if (o->isText() && toRenderText(o)->firstTextBox()) {
                point.move(toRenderText(o)->linesBoundingBox().x(), toRenderText(o)->firstTextBox()->root().lineTop().toFloat());
            } else if (o->isBox()) {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->location());
            }
            point = o->container()->localToAbsolute(point, UseTransforms);
            return true;
        }
    }

    // If the target doesn't have any children or siblings that could be used to calculate the scroll position, we must be
    // at the end of the document. Scroll to the bottom. FIXME: who said anything about scrolling?
    if (!o && document().view()) {
        point = FloatPoint(0, document().view()->contentsHeight());
        return true;
    }
    return false;
}

bool ContainerNode::getLowerRightCorner(FloatPoint& point) const
{
    if (!renderer())
        return false;

    RenderObject* o = renderer();
    if (!o->isInline() || o->isReplaced()) {
        RenderBox* box = toRenderBox(o);
        point = o->localToAbsolute(LayoutPoint(box->size()), UseTransforms);
        return true;
    }

    // find the last text/image child, to get a position
    while (o) {
        if (RenderObject* oLastChild = o->slowLastChild()) {
            o = oLastChild;
        } else if (o->previousSibling()) {
            o = o->previousSibling();
        } else {
            RenderObject* prev = 0;
        while (!prev) {
            o = o->parent();
            if (!o)
                return false;
            prev = o->previousSibling();
        }
        o = prev;
        }
        ASSERT(o);
        if (o->isText() || o->isReplaced()) {
            point = FloatPoint();
            if (o->isText()) {
                RenderText* text = toRenderText(o);
                IntRect linesBox = text->linesBoundingBox();
                if (!linesBox.maxX() && !linesBox.maxY())
                    continue;
                point.moveBy(linesBox.maxXMaxYCorner());
            } else {
                RenderBox* box = toRenderBox(o);
                point.moveBy(box->frameRect().maxXMaxYCorner());
            }
            point = o->container()->localToAbsolute(point, UseTransforms);
            return true;
        }
    }
    return true;
}

// FIXME: This override is only needed for inline anchors without an
// InlineBox and it does not belong in ContainerNode as it reaches into
// the render and line box trees.
// https://code.google.com/p/chromium/issues/detail?id=248354
LayoutRect ContainerNode::boundingBox() const
{
    FloatPoint upperLeft, lowerRight;
    bool foundUpperLeft = getUpperLeftCorner(upperLeft);
    bool foundLowerRight = getLowerRightCorner(lowerRight);

    // If we've found one corner, but not the other,
    // then we should just return a point at the corner that we did find.
    if (foundUpperLeft != foundLowerRight) {
        if (foundUpperLeft)
            lowerRight = upperLeft;
        else
            upperLeft = lowerRight;
    }

    return enclosingLayoutRect(FloatRect(upperLeft, lowerRight.expandedTo(upperLeft) - upperLeft));
}

// This is used by FrameSelection to denote when the active-state of the page has changed
// independent of the focused element changing.
void ContainerNode::focusStateChanged()
{
    // If we're just changing the window's active state and the focused node has no
    // renderer we can just ignore the state change.
    if (!renderer())
        return;

    if (styleChangeType() < SubtreeStyleChange) {
        if (renderStyle()->affectedByFocus() && renderStyle()->hasPseudoStyle(FIRST_LETTER))
            setNeedsStyleRecalc(SubtreeStyleChange);
        else if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByFocus())
            document().ensureStyleResolver().ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForPseudoChange(CSSSelector::PseudoFocus, *toElement(this));
        else if (renderStyle()->affectedByFocus())
            setNeedsStyleRecalc(LocalStyleChange);
    }

    if (renderer() && renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), FocusControlState);
}

void ContainerNode::setFocus(bool received)
{
    if (focused() == received)
        return;

    Node::setFocus(received);

    focusStateChanged();

    if (renderer() || received)
        return;

    // If :focus sets display: none, we lose focus but still need to recalc our style.
    if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByFocus() && styleChangeType() < SubtreeStyleChange)
        document().ensureStyleResolver().ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForPseudoChange(CSSSelector::PseudoFocus, *toElement(this));
    else
        setNeedsStyleRecalc(LocalStyleChange);
}

void ContainerNode::setActive(bool down)
{
    if (down == active())
        return;

    Node::setActive(down);

    // FIXME: Why does this not need to handle the display: none transition like :hover does?
    if (renderer()) {
        if (styleChangeType() < SubtreeStyleChange) {
            if (renderStyle()->affectedByActive() && renderStyle()->hasPseudoStyle(FIRST_LETTER))
                setNeedsStyleRecalc(SubtreeStyleChange);
            else if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByActive())
                document().ensureStyleResolver().ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForPseudoChange(CSSSelector::PseudoActive, *toElement(this));
            else if (renderStyle()->affectedByActive())
                setNeedsStyleRecalc(LocalStyleChange);
        }

        if (renderStyle()->hasAppearance())
            RenderTheme::theme().stateChanged(renderer(), PressedControlState);
    }
}

void ContainerNode::setHovered(bool over)
{
    if (over == hovered())
        return;

    Node::setHovered(over);

    // If :hover sets display: none we lose our hover but still need to recalc our style.
    if (!renderer()) {
        if (over)
            return;
        if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByHover() && styleChangeType() < SubtreeStyleChange)
            document().ensureStyleResolver().ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForPseudoChange(CSSSelector::PseudoHover, *toElement(this));
        else
            setNeedsStyleRecalc(LocalStyleChange);
        return;
    }

    if (styleChangeType() < SubtreeStyleChange) {
        if (renderStyle()->affectedByHover() && renderStyle()->hasPseudoStyle(FIRST_LETTER))
            setNeedsStyleRecalc(SubtreeStyleChange);
        else if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByHover())
            document().ensureStyleResolver().ensureUpdatedRuleFeatureSet().scheduleStyleInvalidationForPseudoChange(CSSSelector::PseudoHover, *toElement(this));
        else if (renderStyle()->affectedByHover())
            setNeedsStyleRecalc(LocalStyleChange);
    }

    if (renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), HoverControlState);
}

PassRefPtrWillBeRawPtr<HTMLCollection> ContainerNode::children()
{
    return ensureRareData().ensureNodeLists().addCache<HTMLCollection>(*this, NodeChildren);
}

unsigned ContainerNode::countChildren() const
{
    unsigned count = 0;
    Node *n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

Node* ContainerNode::traverseToChildAt(unsigned index) const
{
    unsigned i;
    Node *n = firstChild();
    for (i = 0; n != 0 && i < index; i++)
        n = n->nextSibling();
    return n;
}

PassRefPtrWillBeRawPtr<Element> ContainerNode::querySelector(const AtomicString& selectors, ExceptionState& exceptionState)
{
    if (selectors.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The provided selector is empty.");
        return nullptr;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache().add(selectors, document(), exceptionState);
    if (!selectorQuery)
        return nullptr;
    return selectorQuery->queryFirst(*this);
}

PassRefPtrWillBeRawPtr<StaticNodeList> ContainerNode::querySelectorAll(const AtomicString& selectors, ExceptionState& exceptionState)
{
    if (selectors.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The provided selector is empty.");
        return nullptr;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache().add(selectors, document(), exceptionState);
    if (!selectorQuery)
        return nullptr;

    return selectorQuery->queryAll(*this);
}

static void dispatchChildInsertionEvents(Node& child)
{
    if (child.isInShadowTree())
        return;

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    RefPtrWillBeRawPtr<Node> c(child);
    RefPtrWillBeRawPtr<Document> document(child.document());

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeInserted, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeInsertedIntoDocument, false));
    }
}

static void dispatchChildRemovalEvents(Node& child)
{
    if (child.isInShadowTree()) {
        InspectorInstrumentation::willRemoveDOMNode(&child);
        return;
    }

    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    InspectorInstrumentation::willRemoveDOMNode(&child);

    RefPtrWillBeRawPtr<Node> c(child);
    RefPtrWillBeRawPtr<Document> document(child.document());

    // dispatch pre-removal mutation events
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER)) {
        NodeChildRemovalTracker scope(child);
        c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeRemoved, true, c->parentNode()));
    }

    // dispatch the DOMNodeRemovedFromDocument event to all descendants
    if (c->inDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        NodeChildRemovalTracker scope(child);
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeRemovedFromDocument, false));
    }
}

void ContainerNode::updateTreeAfterInsertion(Node& child)
{
#if !ENABLE(OILPAN)
    ASSERT(refCount());
    ASSERT(child.refCount());
#endif

    ChildListMutationScope(*this).childAdded(child);

    notifyNodeInserted(child);

    dispatchChildInsertionEvents(child);
}

bool ContainerNode::hasRestyleFlagInternal(DynamicRestyleFlags mask) const
{
    return rareData()->hasRestyleFlag(mask);
}

bool ContainerNode::hasRestyleFlagsInternal() const
{
    return rareData()->hasRestyleFlags();
}

void ContainerNode::setRestyleFlag(DynamicRestyleFlags mask)
{
    ASSERT(isElementNode() || isShadowRoot());
    ensureRareData().setRestyleFlag(mask);
}

void ContainerNode::checkForChildrenAdjacentRuleChanges()
{
    bool hasDirectAdjacentRules = childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = childrenAffectedByIndirectAdjacentRules();

    if (!hasDirectAdjacentRules && !hasIndirectAdjacentRules)
        return;

    unsigned forceCheckOfNextElementCount = 0;
    bool forceCheckOfAnyElementSibling = false;
    Document& document = this->document();

    for (Element* child = ElementTraversal::firstChild(*this); child; child = ElementTraversal::nextSibling(*child)) {
        bool childRulesChanged = child->needsStyleRecalc() && child->styleChangeType() >= SubtreeStyleChange;

        if (forceCheckOfNextElementCount || forceCheckOfAnyElementSibling)
            child->setNeedsStyleRecalc(SubtreeStyleChange);

        if (childRulesChanged && hasDirectAdjacentRules)
            forceCheckOfNextElementCount = document.styleEngine()->maxDirectAdjacentSelectors();
        else if (forceCheckOfNextElementCount)
            --forceCheckOfNextElementCount;

        forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);
    }
}

void ContainerNode::checkForSiblingStyleChanges(bool finishedParsingCallback, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    if (!inActiveDocument() || document().hasPendingForcedStyleRecalc() || styleChangeType() >= SubtreeStyleChange)
        return;

    if (needsStyleRecalc() && childrenAffectedByPositionalRules())
        return;

    // Forward positional selectors include nth-child, nth-of-type, first-of-type and only-of-type.
    // The indirect adjacent selector is the ~ selector.
    // Backward positional selectors include nth-last-child, nth-last-of-type, last-of-type and only-of-type.
    // We have to invalidate everything following the insertion point in the forward and indirect adjacent case,
    // and everything before the insertion point in the backward case.
    // |afterChange| is 0 in the parser callback case, so we won't do any work for the forward case if we don't have to.
    // For performance reasons we just mark the parent node as changed, since we don't want to make childrenChanged O(n^2) by crawling all our kids
    // here. recalcStyle will then force a walk of the children when it sees that this has happened.
    if (((childrenAffectedByForwardPositionalRules() || childrenAffectedByIndirectAdjacentRules()) && afterChange)
        || (childrenAffectedByBackwardPositionalRules() && beforeChange)) {
        setNeedsStyleRecalc(SubtreeStyleChange);
        return;
    }

    // :first-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (childrenAffectedByFirstChildRules() && afterChange) {
        // Find our new first child.
        Element* newFirstChild = ElementTraversal::firstWithin(*this);
        RenderStyle* newFirstChildStyle = newFirstChild ? newFirstChild->renderStyle() : 0;

        // Find the first element node following |afterChange|
        Node* firstElementAfterInsertion = afterChange->isElementNode() ? afterChange : ElementTraversal::nextSibling(*afterChange);
        RenderStyle* firstElementAfterInsertionStyle = firstElementAfterInsertion ? firstElementAfterInsertion->renderStyle() : 0;

        // This is the insert/append case.
        if (newFirstChild != firstElementAfterInsertion && firstElementAfterInsertionStyle && firstElementAfterInsertionStyle->firstChildState())
            firstElementAfterInsertion->setNeedsStyleRecalc(SubtreeStyleChange);

        // We also have to handle node removal.
        if (childCountDelta < 0 && newFirstChild == firstElementAfterInsertion && newFirstChild && (!newFirstChildStyle || !newFirstChildStyle->firstChildState()))
            newFirstChild->setNeedsStyleRecalc(SubtreeStyleChange);
    }

    // :last-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (childrenAffectedByLastChildRules() && beforeChange) {
        // Find our new last child.
        Node* newLastChild = ElementTraversal::lastChild(*this);
        RenderStyle* newLastChildStyle = newLastChild ? newLastChild->renderStyle() : 0;

        // Find the last element node going backwards from |beforeChange|
        Node* lastElementBeforeInsertion = beforeChange->isElementNode() ? beforeChange : ElementTraversal::previousSibling(*beforeChange);
        RenderStyle* lastElementBeforeInsertionStyle = lastElementBeforeInsertion ? lastElementBeforeInsertion->renderStyle() : 0;

        if (newLastChild != lastElementBeforeInsertion && lastElementBeforeInsertionStyle && lastElementBeforeInsertionStyle->lastChildState())
            lastElementBeforeInsertion->setNeedsStyleRecalc(SubtreeStyleChange);

        // We also have to handle node removal. The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((childCountDelta < 0 || finishedParsingCallback) && newLastChild == lastElementBeforeInsertion && newLastChild && (!newLastChildStyle || !newLastChildStyle->lastChildState()))
            newLastChild->setNeedsStyleRecalc(SubtreeStyleChange);
    }

    // The + selector. We need to invalidate the first element following the insertion point. It is the only possible element
    // that could be affected by this DOM change.
    if (childrenAffectedByDirectAdjacentRules() && afterChange) {
        if (Node* firstElementAfterInsertion = afterChange->isElementNode() ? afterChange : ElementTraversal::nextSibling(*afterChange))
            firstElementAfterInsertion->setNeedsStyleRecalc(SubtreeStyleChange);
    }
}

PassRefPtrWillBeRawPtr<TagCollection> ContainerNode::getElementsByTagName(const AtomicString& localName)
{
    if (localName.isNull())
        return nullptr;

    if (document().isHTMLDocument())
        return ensureRareData().ensureNodeLists().addCache<HTMLTagCollection>(*this, HTMLTagCollectionType, localName);
    return ensureRareData().ensureNodeLists().addCache<TagCollection>(*this, TagCollectionType, localName);
}

PassRefPtrWillBeRawPtr<TagCollection> ContainerNode::getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (localName.isNull())
        return nullptr;

    if (namespaceURI == starAtom)
        return getElementsByTagName(localName);

    return ensureRareData().ensureNodeLists().addCache(*this, namespaceURI.isEmpty() ? nullAtom : namespaceURI, localName);
}

// Takes an AtomicString in argument because it is common for elements to share the same name attribute.
// Therefore, the NameNodeList factory function expects an AtomicString type.
PassRefPtrWillBeRawPtr<NameNodeList> ContainerNode::getElementsByName(const AtomicString& elementName)
{
    return ensureRareData().ensureNodeLists().addCache<NameNodeList>(*this, NameNodeListType, elementName);
}

// Takes an AtomicString in argument because it is common for elements to share the same set of class names.
// Therefore, the ClassNodeList factory function expects an AtomicString type.
PassRefPtrWillBeRawPtr<ClassCollection> ContainerNode::getElementsByClassName(const AtomicString& classNames)
{
    return ensureRareData().ensureNodeLists().addCache<ClassCollection>(*this, ClassCollectionType, classNames);
}

PassRefPtrWillBeRawPtr<RadioNodeList> ContainerNode::radioNodeList(const AtomicString& name, bool onlyMatchImgElements)
{
    ASSERT(isHTMLFormElement(this) || isHTMLFieldSetElement(this));
    CollectionType type = onlyMatchImgElements ? RadioImgNodeListType : RadioNodeListType;
    return ensureRareData().ensureNodeLists().addCache<RadioNodeList>(*this, type, name);
}

Element* ContainerNode::getElementById(const AtomicString& id) const
{
    if (isInTreeScope()) {
        // Fast path if we are in a tree scope: call getElementById() on tree scope
        // and check if the matching element is in our subtree.
        Element* element = treeScope().getElementById(id);
        if (!element)
            return 0;
        if (element->isDescendantOf(this))
            return element;
    }

    // Fall back to traversing our subtree. In case of duplicate ids, the first element found will be returned.
    for (Element* element = ElementTraversal::firstWithin(*this); element; element = ElementTraversal::next(*element, this)) {
        if (element->getIdAttribute() == id)
            return element;
    }
    return 0;
}

#ifndef NDEBUG
bool childAttachedAllowedWhenAttachingChildren(ContainerNode* node)
{
    if (node->isShadowRoot())
        return true;

    if (node->isInsertionPoint())
        return true;

    if (node->isElementNode() && toElement(node)->shadow())
        return true;

    return false;
}
#endif

} // namespace WebCore
