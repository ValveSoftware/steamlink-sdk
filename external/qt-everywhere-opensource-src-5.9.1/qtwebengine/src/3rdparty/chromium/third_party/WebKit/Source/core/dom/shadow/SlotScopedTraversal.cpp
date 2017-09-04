// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/SlotScopedTraversal.h"

#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

namespace {
Element* nextSkippingChildrenOfShadowHost(const Element& start,
                                          const Element& scope) {
  DCHECK(scope.assignedSlot());
  if (!start.authorShadowRoot()) {
    if (Element* first = ElementTraversal::firstChild(start))
      return first;
  }

  for (const Element* current = &start; current != scope;
       current = current->parentElement()) {
    if (Element* nextSibling = ElementTraversal::nextSibling(*current))
      return nextSibling;
  }
  return nullptr;
}

Element* lastWithinOrSelfSkippingChildrenOfShadowHost(const Element& scope) {
  Element* current = const_cast<Element*>(&scope);
  while (!current->authorShadowRoot()) {
    Element* lastChild = ElementTraversal::lastChild(*current);
    if (!lastChild)
      break;
    current = lastChild;
  }
  return current;
}

Element* previousSkippingChildrenOfShadowHost(const Element& start,
                                              const Element& scope) {
  DCHECK(scope.assignedSlot());
  DCHECK_NE(start, &scope);
  if (Element* previousSibling = ElementTraversal::previousSibling(start))
    return lastWithinOrSelfSkippingChildrenOfShadowHost(*previousSibling);
  return start.parentElement();
}
}  // namespace

HTMLSlotElement* SlotScopedTraversal::findScopeOwnerSlot(
    const Element& current) {
  if (Element* nearestInclusiveAncestorAssignedToSlot =
          SlotScopedTraversal::nearestInclusiveAncestorAssignedToSlot(current))
    return nearestInclusiveAncestorAssignedToSlot->assignedSlot();
  return nullptr;
}

Element* SlotScopedTraversal::nearestInclusiveAncestorAssignedToSlot(
    const Element& current) {
  Element* element = const_cast<Element*>(&current);
  for (; element; element = element->parentElement()) {
    if (element->assignedSlot())
      break;
  }
  return element;
}

Element* SlotScopedTraversal::next(const Element& current) {
  Element* nearestInclusiveAncestorAssignedToSlot =
      SlotScopedTraversal::nearestInclusiveAncestorAssignedToSlot(current);
  DCHECK(nearestInclusiveAncestorAssignedToSlot);
  // Search within children of an element which is assigned to a slot.
  if (Element* next = nextSkippingChildrenOfShadowHost(
          current, *nearestInclusiveAncestorAssignedToSlot))
    return next;

  // Seek to the next element assigned to the same slot.
  HTMLSlotElement* slot =
      nearestInclusiveAncestorAssignedToSlot->assignedSlot();
  DCHECK(slot);
  const HeapVector<Member<Node>>& assignedNodes = slot->assignedNodes();
  size_t currentIndex =
      assignedNodes.find(*nearestInclusiveAncestorAssignedToSlot);
  DCHECK_NE(currentIndex, kNotFound);
  for (++currentIndex; currentIndex < assignedNodes.size(); ++currentIndex) {
    if (assignedNodes[currentIndex]->isElementNode())
      return toElement(assignedNodes[currentIndex]);
  }
  return nullptr;
}

Element* SlotScopedTraversal::previous(const Element& current) {
  Element* nearestInclusiveAncestorAssignedToSlot =
      SlotScopedTraversal::nearestInclusiveAncestorAssignedToSlot(current);
  DCHECK(nearestInclusiveAncestorAssignedToSlot);

  if (current != nearestInclusiveAncestorAssignedToSlot) {
    // Search within children of an element which is assigned to a slot.
    Element* previous = previousSkippingChildrenOfShadowHost(
        current, *nearestInclusiveAncestorAssignedToSlot);
    DCHECK(previous);
    return previous;
  }

  // Seek to the previous element assigned to the same slot.
  const HeapVector<Member<Node>>& assignedNodes =
      nearestInclusiveAncestorAssignedToSlot->assignedSlot()->assignedNodes();
  size_t currentIndex =
      assignedNodes.reverseFind(*nearestInclusiveAncestorAssignedToSlot);
  DCHECK_NE(currentIndex, kNotFound);
  for (; currentIndex > 0; --currentIndex) {
    const Member<Node> assignedNode = assignedNodes[currentIndex - 1];
    if (!assignedNode->isElementNode())
      continue;
    return lastWithinOrSelfSkippingChildrenOfShadowHost(
        *toElement(assignedNode));
  }
  return nullptr;
}

Element* SlotScopedTraversal::firstAssignedToSlot(HTMLSlotElement& slot) {
  const HeapVector<Member<Node>>& assignedNodes = slot.assignedNodes();
  for (auto assignedNode : assignedNodes) {
    if (assignedNode->isElementNode())
      return toElement(assignedNode);
  }
  return nullptr;
}

Element* SlotScopedTraversal::lastAssignedToSlot(HTMLSlotElement& slot) {
  const HeapVector<Member<Node>>& assignedNodes = slot.assignedNodes();
  for (auto assignedNode = assignedNodes.rbegin();
       assignedNode != assignedNodes.rend(); ++assignedNode) {
    if (!(*assignedNode)->isElementNode())
      continue;
    return lastWithinOrSelfSkippingChildrenOfShadowHost(
        *toElement(*assignedNode));
  }
  return nullptr;
}

bool SlotScopedTraversal::isSlotScoped(const Element& current) {
  return SlotScopedTraversal::nearestInclusiveAncestorAssignedToSlot(current);
}

}  // namespace blink
