/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
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

#include "core/dom/ContainerNode.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ChildFrameDisconnector.h"
#include "core/dom/ChildListMutationScope.h"
#include "core/dom/ClassCollection.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NameNodeList.h"
#include "core/dom/NodeChildRemovalTracker.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/NthIndexCache.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/MutationEvent.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLTagCollection.h"
#include "core/html/RadioNodeList.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/ScriptForbiddenScope.h"

namespace blink {

using namespace HTMLNames;

static void dispatchChildInsertionEvents(Node&);
static void dispatchChildRemovalEvents(Node&);

static void collectChildrenAndRemoveFromOldParent(Node& node, NodeVector& nodes, ExceptionState& exceptionState)
{
    if (node.isDocumentFragment()) {
        DocumentFragment& fragment = toDocumentFragment(node);
        getChildNodes(fragment, nodes);
        fragment.removeChildren();
        return;
    }
    nodes.append(&node);
    if (ContainerNode* oldParent = node.parentNode())
        oldParent->removeChild(&node, exceptionState);
}

void ContainerNode::parserTakeAllChildrenFrom(ContainerNode& oldParent)
{
    while (Node* child = oldParent.firstChild()) {
        // Explicitly remove since appending can fail, but this loop shouldn't be infinite.
        oldParent.parserRemoveChild(*child);
        parserAppendChild(child);
    }
}

ContainerNode::~ContainerNode()
{
    DCHECK(needsAttach());
}

bool ContainerNode::isChildTypeAllowed(const Node& child) const
{
    if (!child.isDocumentFragment())
        return childTypeAllowed(child.getNodeType());

    for (Node* node = toDocumentFragment(child).firstChild(); node; node = node->nextSibling()) {
        if (!childTypeAllowed(node->getNodeType()))
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
        DCHECK(isChildTypeAllowed(*newChild));
        if (containsConsideringHostElements(*newChild)) {
            exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
            return false;
        }
        return true;
    }

    // This should never happen, but also protect release builds from tree corruption.
    DCHECK(!newChild->isPseudoElement());
    if (newChild->isPseudoElement()) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element is a pseudo-element.");
        return false;
    }

    return checkAcceptChildGuaranteedNodeTypes(*newChild, oldChild, exceptionState);
}

bool ContainerNode::checkAcceptChildGuaranteedNodeTypes(const Node& newChild, const Node* oldChild, ExceptionState& exceptionState) const
{
    if (isDocumentNode())
        return toDocument(this)->canAcceptChild(newChild, oldChild, exceptionState);
    if (newChild.containsIncludingHostElements(*this)) {
        exceptionState.throwDOMException(HierarchyRequestError, "The new child element contains the parent.");
        return false;
    }
    if (!isChildTypeAllowed(newChild)) {
        exceptionState.throwDOMException(HierarchyRequestError, "Nodes of type '" + newChild.nodeName() + "' may not be inserted inside nodes of type '" + nodeName() + "'.");
        return false;
    }
    return true;
}

Node* ContainerNode::insertBefore(Node* newChild, Node* refChild, ExceptionState& exceptionState)
{
    // insertBefore(node, 0) is equivalent to appendChild(node)
    if (!refChild) {
        return appendChild(newChild, exceptionState);
    }

    // Make sure adding the new child is OK.
    if (!checkAcceptChild(newChild, 0, exceptionState)) {
        if (exceptionState.hadException())
            return nullptr;
        return newChild;
    }
    DCHECK(newChild);

    // NotFoundError: Raised if refChild is not a child of this node
    if (refChild->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node before which the new node is to be inserted is not a child of this node.");
        return nullptr;
    }

    // Nothing to do.
    if (refChild->previousSibling() == newChild || refChild == newChild)
        return newChild;

    Node* next = refChild;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    if (targets.isEmpty())
        return newChild;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*newChild, nullptr, exceptionState)) {
        if (exceptionState.hadException())
            return nullptr;
        return newChild;
    }

    InspectorInstrumentation::willInsertDOMNode(this);

    ChildListMutationScope mutation(*this);
    for (const auto& targetNode : targets) {
        DCHECK(targetNode);
        Node& child = *targetNode;

        // Due to arbitrary code running in response to a DOM mutation event it's
        // possible that "next" is no longer a child of "this".
        // It's also possible that "child" has been inserted elsewhere.
        // In either of those cases, we'll just stop.
        if (next->parentNode() != this)
            break;
        if (child.parentNode())
            break;

        {
            EventDispatchForbiddenScope assertNoEventDispatch;
            ScriptForbiddenScope forbidScript;

            treeScope().adoptIfNeeded(child);
            insertBeforeCommon(*next, child);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();

    return newChild;
}

void ContainerNode::insertBeforeCommon(Node& nextChild, Node& newChild)
{
    EventDispatchForbiddenScope assertNoEventDispatch;
    ScriptForbiddenScope forbidScript;

    DCHECK(!newChild.parentNode()); // Use insertBefore if you need to handle reparenting (and want DOM mutation events).
    DCHECK(!newChild.nextSibling());
    DCHECK(!newChild.previousSibling());
    DCHECK(!newChild.isShadowRoot());

    Node* prev = nextChild.previousSibling();
    DCHECK_NE(m_lastChild, prev);
    nextChild.setPreviousSibling(&newChild);
    if (prev) {
        DCHECK_NE(firstChild(), nextChild);
        DCHECK_EQ(prev->nextSibling(), nextChild);
        prev->setNextSibling(&newChild);
    } else {
        DCHECK(firstChild() == nextChild);
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

bool ContainerNode::checkParserAcceptChild(const Node& newChild) const
{
    if (!isDocumentNode())
        return true;
    // TODO(esprehn): Are there other conditions where the parser can create
    // invalid trees?
    return toDocument(*this).canAcceptChild(newChild, nullptr, IGNORE_EXCEPTION);
}

void ContainerNode::parserInsertBefore(Node* newChild, Node& nextChild)
{
    DCHECK(newChild);
    DCHECK_EQ(nextChild.parentNode(), this);
    DCHECK(!newChild->isDocumentFragment());
    DCHECK(!isHTMLTemplateElement(this));

    if (nextChild.previousSibling() == newChild || &nextChild == newChild) // nothing to do
        return;

    if (!checkParserAcceptChild(*newChild))
        return;

    // FIXME: parserRemoveChild can run script which could then insert the
    // newChild back into the page. Loop until the child is actually removed.
    // See: fast/parser/execute-script-during-adoption-agency-removal.html
    while (ContainerNode* parent = newChild->parentNode())
        parent->parserRemoveChild(*newChild);

    if (nextChild.parentNode() != this)
        return;

    if (document() != newChild->document())
        document().adoptNode(newChild, ASSERT_NO_EXCEPTION);

    {
        EventDispatchForbiddenScope assertNoEventDispatch;
        ScriptForbiddenScope forbidScript;

        treeScope().adoptIfNeeded(*newChild);
        insertBeforeCommon(nextChild, *newChild);
        DCHECK_EQ(newChild->connectedSubframeCount(), 0u);
        ChildListMutationScope(*this).childAdded(*newChild);
    }

    notifyNodeInserted(*newChild, ChildrenChangeSourceParser);
}

Node* ContainerNode::replaceChild(Node* newChild, Node* oldChild, ExceptionState& exceptionState)
{
    if (oldChild == newChild) // Nothing to do.
        return oldChild;

    if (!oldChild) {
        exceptionState.throwDOMException(NotFoundError, "The node to be replaced is null.");
        return nullptr;
    }

    Node* child = oldChild;

    // Make sure replacing the old child with the new is OK.
    if (!checkAcceptChild(newChild, child, exceptionState)) {
        if (exceptionState.hadException())
            return nullptr;
        return child;
    }

    // NotFoundError: Raised if oldChild is not a child of this node.
    if (child->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be replaced is not a child of this node.");
        return nullptr;
    }

    ChildListMutationScope mutation(*this);

    Node* next = child->nextSibling();

    // Remove the node we're replacing.
    removeChild(child, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    if (next && (next->previousSibling() == newChild || next == newChild)) // nothing to do
        return child;

    // Does this one more time because removeChild() fires a MutationEvent.
    if (!checkAcceptChild(newChild, child, exceptionState)) {
        if (exceptionState.hadException())
            return nullptr;
        return child;
    }

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    // Does this yet another check because collectChildrenAndRemoveFromOldParent() fires a MutationEvent.
    if (!checkAcceptChild(newChild, child, exceptionState)) {
        if (exceptionState.hadException())
            return nullptr;
        return child;
    }

    InspectorInstrumentation::willInsertDOMNode(this);

    // Add the new child(ren).
    for (const auto& targetNode : targets) {
        DCHECK(targetNode);
        Node& child = *targetNode;

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
            EventDispatchForbiddenScope assertNoEventDispatch;
            if (next)
                insertBeforeCommon(*next, child);
            else
                appendChildCommon(child);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
    return child;
}

void ContainerNode::willRemoveChild(Node& child)
{
    DCHECK_EQ(child.parentNode(), this);
    ChildListMutationScope(*this).willRemoveChild(child);
    child.notifyMutationObserversNodeWillDetach();
    dispatchChildRemovalEvents(child);
    ChildFrameDisconnector(child).disconnect();
    if (document() != child.document()) {
        // |child| was moved another document by DOM mutation event handler.
        return;
    }

    // |nodeWillBeRemoved()| must be run after |ChildFrameDisconnector|, because
    // |ChildFrameDisconnector| can run script which may cause state that is to
    // be invalidated by removing the node.
    ScriptForbiddenScope scriptForbiddenScope;
    EventDispatchForbiddenScope assertNoEventDispatch;
    // e.g. mutation event listener can create a new range.
    document().nodeWillBeRemoved(child);
}

void ContainerNode::willRemoveChildren()
{
    NodeVector children;
    getChildNodes(*this, children);

    ChildListMutationScope mutation(*this);
    for (const auto& node : children) {
        DCHECK(node);
        Node& child = *node;
        mutation.willRemoveChild(child);
        child.notifyMutationObserversNodeWillDetach();
        dispatchChildRemovalEvents(child);
    }

    ChildFrameDisconnector(*this).disconnect(ChildFrameDisconnector::DescendantsOnly);
}

DEFINE_TRACE(ContainerNode)
{
    visitor->trace(m_firstChild);
    visitor->trace(m_lastChild);
    Node::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ContainerNode)
{
    visitor->traceWrappers(m_firstChild);
    visitor->traceWrappers(m_lastChild);
    Node::traceWrappers(visitor);
}

Node* ContainerNode::removeChild(Node* oldChild, ExceptionState& exceptionState)
{
    // NotFoundError: Raised if oldChild is not a child of this node.
    // FIXME: We should never really get PseudoElements in here, but editing will sometimes
    // attempt to remove them still. We should fix that and enable this ASSERT.
    // DCHECK(!oldChild->isPseudoElement())
    if (!oldChild || oldChild->parentNode() != this || oldChild->isPseudoElement()) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is not a child of this node.");
        return nullptr;
    }

    Node* child = oldChild;

    document().removeFocusedElementOfSubtree(child);

    // Events fired when blurring currently focused node might have moved this
    // child into a different parent.
    if (child->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is no longer a child of this node. Perhaps it was moved in a 'blur' event handler?");
        return nullptr;
    }

    willRemoveChild(*child);

    // Mutation events might have moved this child into a different parent.
    if (child->parentNode() != this) {
        exceptionState.throwDOMException(NotFoundError, "The node to be removed is no longer a child of this node. Perhaps it was moved in response to a mutation?");
        return nullptr;
    }

    {
        HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
        DocumentOrderedMap::RemoveScope treeRemoveScope;

        Node* prev = child->previousSibling();
        Node* next = child->nextSibling();
        removeBetween(prev, next, *child);
        notifyNodeRemoved(*child);
        childrenChanged(ChildrenChange::forRemoval(*child, prev, next, ChildrenChangeSourceAPI));
    }
    dispatchSubtreeModifiedEvent();
    return child;
}

void ContainerNode::removeBetween(Node* previousChild, Node* nextChild, Node& oldChild)
{
    EventDispatchForbiddenScope assertNoEventDispatch;

    DCHECK_EQ(oldChild.parentNode(), this);

    AttachContext context;
    context.clearInvalidation = true;
    if (!oldChild.needsAttach())
        oldChild.detach(context);

    if (nextChild)
        nextChild->setPreviousSibling(previousChild);
    if (previousChild)
        previousChild->setNextSibling(nextChild);
    if (m_firstChild == &oldChild)
        m_firstChild = nextChild;
    if (m_lastChild == &oldChild)
        m_lastChild = previousChild;

    oldChild.setPreviousSibling(nullptr);
    oldChild.setNextSibling(nullptr);
    oldChild.setParentOrShadowHostNode(nullptr);

    document().adoptIfNeeded(oldChild);
}

void ContainerNode::parserRemoveChild(Node& oldChild)
{
    DCHECK_EQ(oldChild.parentNode(), this);
    DCHECK(!oldChild.isDocumentFragment());

    // This may cause arbitrary Javascript execution via onunload handlers.
    if (oldChild.connectedSubframeCount())
        ChildFrameDisconnector(oldChild).disconnect();

    if (oldChild.parentNode() != this)
        return;

    ChildListMutationScope(*this).willRemoveChild(oldChild);
    oldChild.notifyMutationObserversNodeWillDetach();

    HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
    DocumentOrderedMap::RemoveScope treeRemoveScope;

    Node* prev = oldChild.previousSibling();
    Node* next = oldChild.nextSibling();
    removeBetween(prev, next, oldChild);

    notifyNodeRemoved(oldChild);
    childrenChanged(ChildrenChange::forRemoval(oldChild, prev, next, ChildrenChangeSourceParser));
}

// This differs from other remove functions because it forcibly removes all the children,
// regardless of read-only status or event exceptions, e.g.
void ContainerNode::removeChildren(SubtreeModificationAction action)
{
    if (!m_firstChild)
        return;

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

    {
        HTMLFrameOwnerElement::UpdateSuspendScope suspendWidgetHierarchyUpdates;
        DocumentOrderedMap::RemoveScope treeRemoveScope;
        {
            EventDispatchForbiddenScope assertNoEventDispatch;
            ScriptForbiddenScope forbidScript;

            while (Node* child = m_firstChild) {
                removeBetween(0, child->nextSibling(), *child);
                notifyNodeRemoved(*child);
            }
        }

        ChildrenChange change = {AllChildrenRemoved, nullptr, nullptr, nullptr, ChildrenChangeSourceAPI};
        childrenChanged(change);
    }

    if (action == DispatchSubtreeModifiedEvent)
        dispatchSubtreeModifiedEvent();
}

Node* ContainerNode::appendChild(Node* newChild, ExceptionState& exceptionState)
{

    // Make sure adding the new child is ok
    if (!checkAcceptChild(newChild, 0, exceptionState)) {
        if (exceptionState.hadException())
            return nullptr;
        return newChild;
    }
    DCHECK(newChild);

    if (newChild == m_lastChild) // nothing to do
        return newChild;

    NodeVector targets;
    collectChildrenAndRemoveFromOldParent(*newChild, targets, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    if (targets.isEmpty())
        return newChild;

    // We need this extra check because collectChildrenAndRemoveFromOldParent() can fire mutation events.
    if (!checkAcceptChildGuaranteedNodeTypes(*newChild, nullptr, exceptionState)) {
        if (exceptionState.hadException())
            return nullptr;
        return newChild;
    }

    InspectorInstrumentation::willInsertDOMNode(this);

    // Now actually add the child(ren).
    ChildListMutationScope mutation(*this);
    for (const auto& targetNode : targets) {
        DCHECK(targetNode);
        Node& child = *targetNode;

        // If the child has a parent again, just stop what we're doing, because
        // that means someone is doing something with DOM mutation -- can't re-parent
        // a child that already has a parent.
        if (child.parentNode())
            break;

        {
            EventDispatchForbiddenScope assertNoEventDispatch;
            ScriptForbiddenScope forbidScript;

            treeScope().adoptIfNeeded(child);
            appendChildCommon(child);
        }

        updateTreeAfterInsertion(child);
    }

    dispatchSubtreeModifiedEvent();
    return newChild;
}

void ContainerNode::parserAppendChild(Node* newChild)
{
    DCHECK(newChild);
    DCHECK(!newChild->isDocumentFragment());
    DCHECK(!isHTMLTemplateElement(this));

    if (!checkParserAcceptChild(*newChild))
        return;

    // FIXME: parserRemoveChild can run script which could then insert the
    // newChild back into the page. Loop until the child is actually removed.
    // See: fast/parser/execute-script-during-adoption-agency-removal.html
    while (ContainerNode* parent = newChild->parentNode())
        parent->parserRemoveChild(*newChild);

    if (document() != newChild->document())
        document().adoptNode(newChild, ASSERT_NO_EXCEPTION);

    {
        EventDispatchForbiddenScope assertNoEventDispatch;
        ScriptForbiddenScope forbidScript;

        treeScope().adoptIfNeeded(*newChild);
        appendChildCommon(*newChild);
        DCHECK_EQ(newChild->connectedSubframeCount(), 0u);
        ChildListMutationScope(*this).childAdded(*newChild);
    }

    notifyNodeInserted(*newChild, ChildrenChangeSourceParser);
}

void ContainerNode::notifyNodeInserted(Node& root, ChildrenChangeSource source)
{
#if DCHECK_IS_ON()
    DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif
    DCHECK(!root.isShadowRoot());

    if (document().containsV1ShadowTree())
        root.checkSlotChangeAfterInserted();

    InspectorInstrumentation::didInsertDOMNode(&root);

    NodeVector postInsertionNotificationTargets;
    notifyNodeInsertedInternal(root, postInsertionNotificationTargets);

    childrenChanged(ChildrenChange::forInsertion(root, source));

    for (const auto& targetNode : postInsertionNotificationTargets) {
        if (targetNode->inShadowIncludingDocument())
            targetNode->didNotifySubtreeInsertionsToDocument();
    }
}

void ContainerNode::notifyNodeInsertedInternal(Node& root, NodeVector& postInsertionNotificationTargets)
{
    EventDispatchForbiddenScope assertNoEventDispatch;
    ScriptForbiddenScope forbidScript;

    for (Node& node : NodeTraversal::inclusiveDescendantsOf(root)) {
        // As an optimization we don't notify leaf nodes when when inserting
        // into detached subtrees that are not in a shadow tree.
        if (!inShadowIncludingDocument() && !isInShadowTree() && !node.isContainerNode())
            continue;
        if (Node::InsertionShouldCallDidNotifySubtreeInsertions == node.insertedInto(this))
            postInsertionNotificationTargets.append(&node);
        for (ShadowRoot* shadowRoot = node.youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
            notifyNodeInsertedInternal(*shadowRoot, postInsertionNotificationTargets);
    }
}

void ContainerNode::notifyNodeRemoved(Node& root)
{
    ScriptForbiddenScope forbidScript;
    EventDispatchForbiddenScope assertNoEventDispatch;

    for (Node& node : NodeTraversal::inclusiveDescendantsOf(root)) {
        // As an optimization we skip notifying Text nodes and other leaf nodes
        // of removal when they're not in the Document tree and not in a shadow root since the virtual
        // call to removedFrom is not needed.
        if (!node.isContainerNode() && !node.isInTreeScope())
            continue;
        node.removedFrom(this);
        for (ShadowRoot* shadowRoot = node.youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot())
            notifyNodeRemoved(*shadowRoot);
    }
}

void ContainerNode::attach(const AttachContext& context)
{
    AttachContext childrenContext(context);
    childrenContext.resolvedStyle = nullptr;

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
#if DCHECK_IS_ON()
        DCHECK(child->needsAttach() || childAttachedAllowedWhenAttachingChildren(this));
#endif
        if (child->needsAttach())
            child->attach(childrenContext);
    }

    clearChildNeedsStyleRecalc();
    Node::attach(context);
}

void ContainerNode::detach(const AttachContext& context)
{
    AttachContext childrenContext(context);
    childrenContext.resolvedStyle = nullptr;
    childrenContext.clearInvalidation = true;

    for (Node* child = firstChild(); child; child = child->nextSibling())
        child->detach(childrenContext);

    setChildNeedsStyleRecalc();
    Node::detach(context);
}

void ContainerNode::childrenChanged(const ChildrenChange& change)
{
    document().incDOMTreeVersion();
    invalidateNodeListCachesInAncestors();
    if (change.isChildInsertion() && !childNeedsStyleRecalc()) {
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
    if (!layoutObject())
        return false;

    // FIXME: What is this code really trying to do?
    LayoutObject* o = layoutObject();
    if (!o->isInline() || o->isAtomicInlineLevel()) {
        point = o->localToAbsolute(FloatPoint(), UseTransforms);
        return true;
    }

    // Find the next text/image child, to get a position.
    while (o) {
        LayoutObject* p = o;
        if (LayoutObject* oFirstChild = o->slowFirstChild()) {
            o = oFirstChild;
        } else if (o->nextSibling()) {
            o = o->nextSibling();
        } else {
            LayoutObject* next = nullptr;
            while (!next && o->parent()) {
                o = o->parent();
                next = o->nextSibling();
            }
            o = next;

            if (!o)
                break;
        }
        DCHECK(o);

        if (!o->isInline() || o->isAtomicInlineLevel()) {
            point = o->localToAbsolute(FloatPoint(), UseTransforms);
            return true;
        }

        if (p->node() && p->node() == this && o->isText() && !o->isBR() && !toLayoutText(o)->hasTextBoxes()) {
            // Do nothing - skip unrendered whitespace that is a child or next sibling of the anchor.
            // FIXME: This fails to skip a whitespace sibling when there was also a whitespace child (because p has moved).
        } else if ((o->isText() && !o->isBR()) || o->isAtomicInlineLevel()) {
            point = FloatPoint();
            if (o->isText()) {
                if (toLayoutText(o)->firstTextBox())
                    point.move(toLayoutText(o)->linesBoundingBox().x(), toLayoutText(o)->firstTextBox()->root().lineTop().toFloat());
                point = o->localToAbsolute(point, UseTransforms);
            } else {
                DCHECK(o->isBox());
                LayoutBox* box = toLayoutBox(o);
                point.moveBy(box->location());
                point = o->container()->localToAbsolute(point, UseTransforms);
            }
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

static inline LayoutObject* endOfContinuations(LayoutObject* layoutObject)
{
    LayoutObject* prev = nullptr;
    LayoutObject* cur = layoutObject;

    if (!cur->isLayoutInline() && !cur->isLayoutBlockFlow())
        return nullptr;

    while (cur) {
        prev = cur;
        if (cur->isLayoutInline())
            cur = toLayoutInline(cur)->continuation();
        else
            cur = toLayoutBlockFlow(cur)->continuation();
    }

    return prev;
}

bool ContainerNode::getLowerRightCorner(FloatPoint& point) const
{
    if (!layoutObject())
        return false;

    LayoutObject* o = layoutObject();
    if (!o->isInline() || o->isAtomicInlineLevel()) {
        LayoutBox* box = toLayoutBox(o);
        point = o->localToAbsolute(FloatPoint(box->size()), UseTransforms);
        return true;
    }

    LayoutObject* startContinuation = nullptr;
    // Find the last text/image child, to get a position.
    while (o) {
        if (LayoutObject* oLastChild = o->slowLastChild()) {
            o = oLastChild;
        } else if (o != layoutObject() && o->previousSibling()) {
            o = o->previousSibling();
        } else {
            LayoutObject* prev = nullptr;
            while (!prev) {
                // Check if the current layoutObject has contiunation and move the location for
                // finding the layoutObject to the end of continuations if there is the continuation.
                // Skip to check the contiunation on contiunations section
                if (startContinuation == o) {
                    startContinuation = nullptr;
                } else if (!startContinuation) {
                    if (LayoutObject* continuation = endOfContinuations(o)) {
                        startContinuation = o;
                        prev = continuation;
                        break;
                    }
                }
                // Prevent to overrun out of own layout tree
                if (o == layoutObject()) {
                    return false;
                }
                o = o->parent();
                if (!o)
                    return false;
                prev = o->previousSibling();
            }
            o = prev;
        }
        DCHECK(o);
        if (o->isText() || o->isAtomicInlineLevel()) {
            point = FloatPoint();
            if (o->isText()) {
                LayoutText* text = toLayoutText(o);
                IntRect linesBox = enclosingIntRect(text->linesBoundingBox());
                if (!linesBox.maxX() && !linesBox.maxY())
                    continue;
                point.moveBy(linesBox.maxXMaxYCorner());
                point = o->localToAbsolute(point, UseTransforms);
            } else {
                LayoutBox* box = toLayoutBox(o);
                point.moveBy(box->frameRect().maxXMaxYCorner());
                point = o->container()->localToAbsolute(point, UseTransforms);
            }
            return true;
        }
    }
    return true;
}

// FIXME: This override is only needed for inline anchors without an
// InlineBox and it does not belong in ContainerNode as it reaches into
// the layout and line box trees.
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

    FloatSize size = lowerRight.expandedTo(upperLeft) - upperLeft;
    if (std::isnan(size.width()) || std::isnan(size.height()))
        return LayoutRect();

    return enclosingLayoutRect(FloatRect(upperLeft, size));
}

// This is used by FrameSelection to denote when the active-state of the page has changed
// independent of the focused element changing.
void ContainerNode::focusStateChanged()
{
    // If we're just changing the window's active state and the focused node has no
    // layoutObject we can just ignore the state change.
    if (!layoutObject())
        return;

    if (computedStyle()->affectedByFocus()) {
        StyleChangeType changeType = computedStyle()->hasPseudoStyle(PseudoIdFirstLetter) ? SubtreeStyleChange : LocalStyleChange;
        setNeedsStyleRecalc(changeType, StyleChangeReasonForTracing::createWithExtraData(StyleChangeReason::PseudoClass, StyleChangeExtraData::Focus));
    }
    if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByFocus())
        toElement(this)->pseudoStateChanged(CSSSelector::PseudoFocus);

    LayoutTheme::theme().controlStateChanged(*layoutObject(), FocusControlState);
}

void ContainerNode::setFocus(bool received)
{
    // Recurse up author shadow trees to mark shadow hosts if it matches :focus.
    // TODO(kochi): Handle UA shadows which marks multiple nodes as focused such as
    // <input type="date"> the same way as author shadow.
    if (ShadowRoot* root = containingShadowRoot()) {
        if (root->type() != ShadowRootType::UserAgent)
            shadowHost()->setFocus(received);
    }

    // If this is an author shadow host and indirectly focused (has focused element within
    // its shadow root), update focus.
    if (isElementNode() && document().focusedElement() && document().focusedElement() != this) {
        if (toElement(this)->authorShadowRoot())
            received = received && toElement(this)->authorShadowRoot()->delegatesFocus();
    }

    if (focused() == received)
        return;

    Node::setFocus(received);

    focusStateChanged();

    if (layoutObject() || received)
        return;

    // If :focus sets display: none, we lose focus but still need to recalc our style.
    if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByFocus())
        toElement(this)->pseudoStateChanged(CSSSelector::PseudoFocus);
    else
        setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::createWithExtraData(StyleChangeReason::PseudoClass, StyleChangeExtraData::Focus));
}

void ContainerNode::setActive(bool down)
{
    if (down == active())
        return;

    Node::setActive(down);

    if (!layoutObject()) {
        if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByActive())
            toElement(this)->pseudoStateChanged(CSSSelector::PseudoActive);
        else
            setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::createWithExtraData(StyleChangeReason::PseudoClass, StyleChangeExtraData::Active));
        return;
    }

    if (computedStyle()->affectedByActive()) {
        StyleChangeType changeType = computedStyle()->hasPseudoStyle(PseudoIdFirstLetter) ? SubtreeStyleChange : LocalStyleChange;
        setNeedsStyleRecalc(changeType, StyleChangeReasonForTracing::createWithExtraData(StyleChangeReason::PseudoClass, StyleChangeExtraData::Active));
    }
    if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByActive())
        toElement(this)->pseudoStateChanged(CSSSelector::PseudoActive);

    LayoutTheme::theme().controlStateChanged(*layoutObject(), PressedControlState);
}

void ContainerNode::setHovered(bool over)
{
    if (over == hovered())
        return;

    Node::setHovered(over);

    // If :hover sets display: none we lose our hover but still need to recalc our style.
    if (!layoutObject()) {
        if (over)
            return;
        if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByHover())
            toElement(this)->pseudoStateChanged(CSSSelector::PseudoHover);
        else
            setNeedsStyleRecalc(LocalStyleChange, StyleChangeReasonForTracing::createWithExtraData(StyleChangeReason::PseudoClass, StyleChangeExtraData::Hover));
        return;
    }

    if (computedStyle()->affectedByHover()) {
        StyleChangeType changeType = computedStyle()->hasPseudoStyle(PseudoIdFirstLetter) ? SubtreeStyleChange : LocalStyleChange;
        setNeedsStyleRecalc(changeType, StyleChangeReasonForTracing::createWithExtraData(StyleChangeReason::PseudoClass, StyleChangeExtraData::Hover));
    }
    if (isElementNode() && toElement(this)->childrenOrSiblingsAffectedByHover())
        toElement(this)->pseudoStateChanged(CSSSelector::PseudoHover);

    LayoutTheme::theme().controlStateChanged(*layoutObject(), HoverControlState);
}

HTMLCollection* ContainerNode::children()
{
    return ensureCachedCollection<HTMLCollection>(NodeChildren);
}

unsigned ContainerNode::countChildren() const
{
    unsigned count = 0;
    Node* n;
    for (n = firstChild(); n; n = n->nextSibling())
        count++;
    return count;
}

Element* ContainerNode::querySelector(const AtomicString& selectors, ExceptionState& exceptionState)
{
    if (selectors.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The provided selector is empty.");
        return nullptr;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache().add(selectors, document(), exceptionState);
    if (!selectorQuery)
        return nullptr;

    NthIndexCache nthIndexCache(document());
    return selectorQuery->queryFirst(*this);
}

StaticElementList* ContainerNode::querySelectorAll(const AtomicString& selectors, ExceptionState& exceptionState)
{
    if (selectors.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The provided selector is empty.");
        return nullptr;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache().add(selectors, document(), exceptionState);
    if (!selectorQuery)
        return nullptr;

    NthIndexCache nthIndexCache(document());
    return selectorQuery->queryAll(*this);
}

static void dispatchChildInsertionEvents(Node& child)
{
    if (child.isInShadowTree())
        return;

#if DCHECK_IS_ON()
    DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif

    Node* c = &child;
    Document* document = &child.document();

    if (c->parentNode() && document->hasListenerType(Document::DOMNODEINSERTED_LISTENER))
        c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeInserted, true, c->parentNode()));

    // dispatch the DOMNodeInsertedIntoDocument event to all descendants
    if (c->inShadowIncludingDocument() && document->hasListenerType(Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER)) {
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

#if DCHECK_IS_ON()
    DCHECK(!EventDispatchForbiddenScope::isEventDispatchForbidden());
#endif

    InspectorInstrumentation::willRemoveDOMNode(&child);

    Node* c = &child;
    Document* document = &child.document();

    // Dispatch pre-removal mutation events.
    if (c->parentNode() && document->hasListenerType(Document::DOMNODEREMOVED_LISTENER)) {
        NodeChildRemovalTracker scope(child);
        c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeRemoved, true, c->parentNode()));
    }

    // Dispatch the DOMNodeRemovedFromDocument event to all descendants.
    if (c->inShadowIncludingDocument() && document->hasListenerType(Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER)) {
        NodeChildRemovalTracker scope(child);
        for (; c; c = NodeTraversal::next(*c, &child))
            c->dispatchScopedEvent(MutationEvent::create(EventTypeNames::DOMNodeRemovedFromDocument, false));
    }
}

void ContainerNode::updateTreeAfterInsertion(Node& child)
{
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
    DCHECK(isElementNode() || isShadowRoot());
    ensureRareData().setRestyleFlag(mask);
}

void ContainerNode::recalcChildStyle(StyleRecalcChange change)
{
    DCHECK(document().inStyleRecalc());
    DCHECK(change >= UpdatePseudoElements || childNeedsStyleRecalc());
    DCHECK(!needsStyleRecalc());

    // This loop is deliberately backwards because we use insertBefore in the layout tree, and want to avoid
    // a potentially n^2 loop to find the insertion point while resolving style. Having us start from the last
    // child and work our way back means in the common case, we'll find the insertion point in O(1) time.
    // See crbug.com/288225
    StyleResolver& styleResolver = document().ensureStyleResolver();
    Text* lastTextNode = nullptr;
    for (Node* child = lastChild(); child; child = child->previousSibling()) {
        if (child->isTextNode()) {
            toText(child)->recalcTextStyle(change, lastTextNode);
            lastTextNode = toText(child);
        } else if (child->isElementNode()) {
            Element* element = toElement(child);
            if (element->shouldCallRecalcStyle(change))
                element->recalcStyle(change, lastTextNode);
            else if (element->supportsStyleSharing())
                styleResolver.addToStyleSharingList(*element);
            if (element->layoutObject())
                lastTextNode = nullptr;
        }
    }
}

void ContainerNode::checkForSiblingStyleChanges(SiblingCheckType changeType, Node* changedNode, Node* nodeBeforeChange, Node* nodeAfterChange)
{
    if (!inActiveDocument() || document().hasPendingForcedStyleRecalc() || getStyleChangeType() >= SubtreeStyleChange)
        return;

    // Forward positional selectors include nth-child, nth-of-type, first-of-type and only-of-type.
    // The indirect adjacent selector is the ~ selector.
    // Backward positional selectors include nth-last-child, nth-last-of-type, last-of-type and only-of-type.
    // We have to invalidate everything following the insertion point in the forward and indirect adjacent case,
    // and everything before the insertion point in the backward case.
    // |afterChange| is 0 in the parser callback case, so we won't do any work for the forward case if we don't have to.
    // For performance reasons we just mark the parent node as changed, since we don't want to make childrenChanged O(n^2) by crawling all our kids
    // here. recalcStyle will then force a walk of the children when it sees that this has happened.
    if ((childrenAffectedByForwardPositionalRules() && nodeAfterChange)
        || (childrenAffectedByBackwardPositionalRules() && nodeBeforeChange)) {
        setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::SiblingSelector));
        return;
    }

    // :first-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (childrenAffectedByFirstChildRules() && nodeAfterChange) {
        DCHECK_NE(changeType, FinishedParsingChildren);
        // Find our new first child element.
        Element* firstChildElement = ElementTraversal::firstChild(*this);

        // Find the first element after the change.
        Element* elementAfterChange = nodeAfterChange->isElementNode() ? toElement(nodeAfterChange) : ElementTraversal::nextSibling(*nodeAfterChange);

        // This is the element insertion as first child element case.
        if (changeType == SiblingElementInserted && elementAfterChange && firstChildElement != elementAfterChange
            && (!nodeBeforeChange || !nodeBeforeChange->isElementNode()) && elementAfterChange->affectedByFirstChildRules()) {
            elementAfterChange->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::SiblingSelector));
        }

        // This is the first child element removal case.
        if (changeType == SiblingElementRemoved && firstChildElement == elementAfterChange && firstChildElement && firstChildElement->affectedByFirstChildRules())
            firstChildElement->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::SiblingSelector));
    }

    // :last-child. In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (childrenAffectedByLastChildRules() && nodeBeforeChange) {
        // Find our new last child element.
        Element* lastChildElement = ElementTraversal::lastChild(*this);

        // Find the last element before the change.
        Element* elementBeforeChange = nodeBeforeChange->isElementNode() ? toElement(nodeBeforeChange) : ElementTraversal::previousSibling(*nodeBeforeChange);

        // This is the element insertion as last child element case.
        if (changeType == SiblingElementInserted && elementBeforeChange && lastChildElement != elementBeforeChange
            && (!nodeAfterChange || !nodeAfterChange->isElementNode()) && elementBeforeChange->affectedByLastChildRules()) {
            elementBeforeChange->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::SiblingSelector));
        }

        // This is the last child element removal case. The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((changeType == SiblingElementRemoved || changeType == FinishedParsingChildren) && lastChildElement == elementBeforeChange && lastChildElement && lastChildElement->affectedByLastChildRules())
            lastChildElement->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::SiblingSelector));
    }

    // For ~ and + combinators, succeeding siblings may need style invalidation
    // after an element is inserted or removed.

    if (!nodeAfterChange)
        return;
    if (changeType != SiblingElementRemoved && changeType != SiblingElementInserted)
        return;
    if (!childrenAffectedByIndirectAdjacentRules() && !childrenAffectedByDirectAdjacentRules())
        return;

    Element* elementAfterChange = nodeAfterChange->isElementNode() ? toElement(nodeAfterChange) : ElementTraversal::nextSibling(*nodeAfterChange);
    if (!elementAfterChange)
        return;
    Element* elementBeforeChange = nullptr;
    if (nodeBeforeChange)
        elementBeforeChange = nodeBeforeChange->isElementNode() ? toElement(nodeBeforeChange) : ElementTraversal::previousSibling(*nodeBeforeChange);

    if (changeType == SiblingElementInserted)
        document().styleEngine().scheduleInvalidationsForInsertedSibling(elementBeforeChange, *toElement(changedNode));
    else
        document().styleEngine().scheduleInvalidationsForRemovedSibling(elementBeforeChange, *toElement(changedNode), *elementAfterChange);
}

void ContainerNode::invalidateNodeListCachesInAncestors(const QualifiedName* attrName, Element* attributeOwnerElement)
{
    if (hasRareData() && (!attrName || isAttributeNode())) {
        if (NodeListsNodeData* lists = rareData()->nodeLists()) {
            if (ChildNodeList* childNodeList = lists->childNodeList(*this))
                childNodeList->invalidateCache();
        }
    }

    // Modifications to attributes that are not associated with an Element can't invalidate NodeList caches.
    if (attrName && !attributeOwnerElement)
        return;

    if (!document().shouldInvalidateNodeListCaches(attrName))
        return;

    document().invalidateNodeListCaches(attrName);

    for (ContainerNode* node = this; node; node = node->parentNode()) {
        if (NodeListsNodeData* lists = node->nodeLists())
            lists->invalidateCaches(attrName);
    }
}

TagCollection* ContainerNode::getElementsByTagName(const AtomicString& localName)
{
    if (document().isHTMLDocument())
        return ensureCachedCollection<HTMLTagCollection>(HTMLTagCollectionType, localName);
    return ensureCachedCollection<TagCollection>(TagCollectionType, localName);
}

TagCollection* ContainerNode::getElementsByTagNameNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (namespaceURI == starAtom)
        return getElementsByTagName(localName);

    return ensureCachedCollection<TagCollection>(TagCollectionType, namespaceURI.isEmpty() ? nullAtom : namespaceURI, localName);
}

// Takes an AtomicString in argument because it is common for elements to share the same name attribute.
// Therefore, the NameNodeList factory function expects an AtomicString type.
NameNodeList* ContainerNode::getElementsByName(const AtomicString& elementName)
{
    return ensureCachedCollection<NameNodeList>(NameNodeListType, elementName);
}

// Takes an AtomicString in argument because it is common for elements to share the same set of class names.
// Therefore, the ClassNodeList factory function expects an AtomicString type.
ClassCollection* ContainerNode::getElementsByClassName(const AtomicString& classNames)
{
    return ensureCachedCollection<ClassCollection>(ClassCollectionType, classNames);
}

RadioNodeList* ContainerNode::radioNodeList(const AtomicString& name, bool onlyMatchImgElements)
{
    DCHECK(isHTMLFormElement(this) || isHTMLFieldSetElement(this));
    CollectionType type = onlyMatchImgElements ? RadioImgNodeListType : RadioNodeListType;
    return ensureCachedCollection<RadioNodeList>(type, name);
}

Element* ContainerNode::getElementById(const AtomicString& id) const
{
    if (isInTreeScope()) {
        // Fast path if we are in a tree scope: call getElementById() on tree scope
        // and check if the matching element is in our subtree.
        Element* element = containingTreeScope().getElementById(id);
        if (!element)
            return nullptr;
        if (element->isDescendantOf(this))
            return element;
    }

    // Fall back to traversing our subtree. In case of duplicate ids, the first element found will be returned.
    for (Element& element : ElementTraversal::descendantsOf(*this)) {
        if (element.getIdAttribute() == id)
            return &element;
    }
    return nullptr;
}

NodeListsNodeData& ContainerNode::ensureNodeLists()
{
    return ensureRareData().ensureNodeLists();
}

#if DCHECK_IS_ON()
bool childAttachedAllowedWhenAttachingChildren(ContainerNode* node)
{
    if (node->isShadowRoot())
        return true;

    if (node->isInsertionPoint())
        return true;

    if (isHTMLSlotElement(node))
        return true;

    if (node->isElementNode() && toElement(node)->shadow())
        return true;

    return false;
}
#endif

} // namespace blink
