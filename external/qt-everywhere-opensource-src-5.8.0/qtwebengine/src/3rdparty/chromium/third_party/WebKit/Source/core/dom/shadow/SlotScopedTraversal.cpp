// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/SlotScopedTraversal.h"

#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

HTMLSlotElement* SlotScopedTraversal::findScopeOwnerSlot(const Element& current)
{
    if (Element* nearestAncestorAssignedToSlot = SlotScopedTraversal::nearestAncestorAssignedToSlot(current))
        return nearestAncestorAssignedToSlot->assignedSlot();
    return nullptr;
}

Element* SlotScopedTraversal::nearestAncestorAssignedToSlot(const Element& current)
{
    // nearestAncestorAssignedToSlot returns an ancestor element of current which is directly assigned to a slot.
    Element* element = const_cast<Element*>(&current);
    for (; element; element = element->parentElement()) {
        if (element->assignedSlot())
            break;
    }
    return element;
}

Element* SlotScopedTraversal::next(const Element& current)
{
    // current.assignedSlot returns a slot only when current is assigned explicitly
    // If current is assigned to a slot, return a descendant of current, which is in the assigned scope of the same slot as current.
    HTMLSlotElement* slot = current.assignedSlot();
    Element* nearestAncestorAssignedToSlot = SlotScopedTraversal::nearestAncestorAssignedToSlot(current);
    if (slot) {
        if (Element* next = ElementTraversal::next(current, &current))
            return next;
    } else {
        // If current is in assigned scope, find an assigned ancestor.
        DCHECK(nearestAncestorAssignedToSlot);
        if (Element* next = ElementTraversal::next(current, nearestAncestorAssignedToSlot))
            return next;
        slot = nearestAncestorAssignedToSlot->assignedSlot();
        DCHECK(slot);
    }
    HeapVector<Member<Node>> assignedNodes = slot->assignedNodes();
    size_t currentIndex = assignedNodes.find(*nearestAncestorAssignedToSlot);
    DCHECK_NE(currentIndex, kNotFound);
    for (++currentIndex; currentIndex < assignedNodes.size(); ++currentIndex) {
        if (assignedNodes[currentIndex]->isElementNode())
            return toElement(assignedNodes[currentIndex]);
    }
    return nullptr;
}

Element* SlotScopedTraversal::previous(const Element& current)
{
    Element* nearestAncestorAssignedToSlot = SlotScopedTraversal::nearestAncestorAssignedToSlot(current);
    DCHECK(nearestAncestorAssignedToSlot);
    // NodeTraversal within nearestAncestorAssignedToSlot
    if (Element* previous = ElementTraversal::previous(current, nearestAncestorAssignedToSlot))
        return previous;
    // If null, jump to previous assigned node's descendant
    const HeapVector<Member<Node>> assignedNodes = nearestAncestorAssignedToSlot->assignedSlot()->assignedNodes();
    size_t currentIndex = assignedNodes.reverseFind(*nearestAncestorAssignedToSlot);
    DCHECK_NE(currentIndex, kNotFound);
    for (; currentIndex > 0; --currentIndex) {
        const Member<Node> assignedPrevious = assignedNodes[currentIndex - 1];
        if (assignedPrevious->isElementNode()) {
            if (Element* last = ElementTraversal::lastWithin(*toElement(assignedPrevious)))
                return last;
            return toElement(assignedPrevious);
        }
    }
    return nullptr;
}

bool SlotScopedTraversal::isSlotScoped(const Element& current)
{
    return SlotScopedTraversal::nearestAncestorAssignedToSlot(current);
}

} // namespace blink
