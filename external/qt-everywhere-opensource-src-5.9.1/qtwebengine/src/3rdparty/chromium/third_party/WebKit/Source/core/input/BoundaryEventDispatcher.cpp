// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/BoundaryEventDispatcher.h"

#include "core/dom/Node.h"
#include "core/dom/shadow/FlatTreeTraversal.h"

namespace blink {

namespace {

bool isInDocument(EventTarget* target) {
  return target && target->toNode() && target->toNode()->isConnected();
}

void buildAncestorChain(EventTarget* target,
                        HeapVector<Member<Node>, 20>* ancestors) {
  if (!isInDocument(target))
    return;
  Node* targetNode = target->toNode();
  DCHECK(targetNode);
  targetNode->updateDistribution();
  // Index 0 element in the ancestors arrays will be the corresponding
  // target. So the root of their document will be their last element.
  for (Node* node = targetNode; node; node = FlatTreeTraversal::parent(*node))
    ancestors->append(node);
}

void buildAncestorChainsAndFindCommonAncestors(
    EventTarget* exitedTarget,
    EventTarget* enteredTarget,
    HeapVector<Member<Node>, 20>* exitedAncestorsOut,
    HeapVector<Member<Node>, 20>* enteredAncestorsOut,
    size_t* exitedAncestorsCommonParentIndexOut,
    size_t* enteredAncestorsCommonParentIndexOut) {
  DCHECK(exitedAncestorsOut);
  DCHECK(enteredAncestorsOut);
  DCHECK(exitedAncestorsCommonParentIndexOut);
  DCHECK(enteredAncestorsCommonParentIndexOut);

  buildAncestorChain(exitedTarget, exitedAncestorsOut);
  buildAncestorChain(enteredTarget, enteredAncestorsOut);

  *exitedAncestorsCommonParentIndexOut = exitedAncestorsOut->size();
  *enteredAncestorsCommonParentIndexOut = enteredAncestorsOut->size();
  while (*exitedAncestorsCommonParentIndexOut > 0 &&
         *enteredAncestorsCommonParentIndexOut > 0) {
    if ((*exitedAncestorsOut)[(*exitedAncestorsCommonParentIndexOut) - 1] !=
        (*enteredAncestorsOut)[(*enteredAncestorsCommonParentIndexOut) - 1])
      break;
    (*exitedAncestorsCommonParentIndexOut)--;
    (*enteredAncestorsCommonParentIndexOut)--;
  }
}

}  // namespace

void BoundaryEventDispatcher::sendBoundaryEvents(EventTarget* exitedTarget,
                                                 EventTarget* enteredTarget) {
  if (exitedTarget == enteredTarget)
    return;

  // Dispatch out event
  if (isInDocument(exitedTarget))
    dispatchOut(exitedTarget, enteredTarget);

  // Create lists of all exited/entered ancestors, locate the common ancestor
  // Based on httparchive, in more than 97% cases the depth of DOM is less
  // than 20.
  HeapVector<Member<Node>, 20> exitedAncestors;
  HeapVector<Member<Node>, 20> enteredAncestors;
  size_t exitedAncestorsCommonParentIndex = 0;
  size_t enteredAncestorsCommonParentIndex = 0;

  // A note on mouseenter and mouseleave: These are non-bubbling events, and
  // they are dispatched if there is a capturing event handler on an ancestor or
  // a normal event handler on the element itself. This special handling is
  // necessary to avoid O(n^2) capturing event handler checks.
  //
  // Note, however, that this optimization can possibly cause some
  // unanswered/missing/redundant mouseenter or mouseleave events in certain
  // contrived eventhandling scenarios, e.g., when:
  // - the mouseleave handler for a node sets the only
  //   capturing-mouseleave-listener in its ancestor, or
  // - DOM mods in any mouseenter/mouseleave handler changes the common ancestor
  //   of exited & entered nodes, etc.
  // We think the spec specifies a "frozen" state to avoid such corner cases
  // (check the discussion on "candidate event listeners" at
  // http://www.w3.org/TR/uievents), but our code below preserves one such
  // behavior from past only to match Firefox and IE behavior.
  //
  // TODO(mustaq): Confirm spec conformance, double-check with other browsers.

  buildAncestorChainsAndFindCommonAncestors(
      exitedTarget, enteredTarget, &exitedAncestors, &enteredAncestors,
      &exitedAncestorsCommonParentIndex, &enteredAncestorsCommonParentIndex);

  bool exitedNodeHasCapturingAncestor = false;
  const AtomicString& leaveEvent = getLeaveEvent();
  for (size_t j = 0; j < exitedAncestors.size(); j++) {
    if (exitedAncestors[j]->hasCapturingEventListeners(leaveEvent)) {
      exitedNodeHasCapturingAncestor = true;
      break;
    }
  }

  // Dispatch leave events, in child-to-parent order.
  for (size_t j = 0; j < exitedAncestorsCommonParentIndex; j++)
    dispatchLeave(exitedAncestors[j], enteredTarget,
                  !exitedNodeHasCapturingAncestor);

  // Dispatch over event
  if (isInDocument(enteredTarget))
    dispatchOver(enteredTarget, exitedTarget);

  // Defer locating capturing enter listener until /after/ dispatching the leave
  // events because the leave handlers might set a capturing enter handler.
  bool enteredNodeHasCapturingAncestor = false;
  const AtomicString& enterEvent = getEnterEvent();
  for (size_t i = 0; i < enteredAncestors.size(); i++) {
    if (enteredAncestors[i]->hasCapturingEventListeners(enterEvent)) {
      enteredNodeHasCapturingAncestor = true;
      break;
    }
  }

  // Dispatch enter events, in parent-to-child order.
  for (size_t i = enteredAncestorsCommonParentIndex; i > 0; i--)
    dispatchEnter(enteredAncestors[i - 1], exitedTarget,
                  !enteredNodeHasCapturingAncestor);
}

}  // namespace blink
