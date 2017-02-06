// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/shadow/SlotAssignment.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLSlotElement.h"

namespace blink {

void SlotAssignment::slotAdded(HTMLSlotElement& slot)
{
    // Relevant DOM Standard:
    // https://dom.spec.whatwg.org/#concept-node-insert
    // 6.4:  Run assign slotables for a tree with node's tree and a set containing each inclusive descendant of node that is a slot.

    ++m_slotCount;
    m_needsCollectSlots = true;

    if (!m_slotMap->contains(slot.name())) {
        m_slotMap->add(slot.name(), &slot);
        return;
    }

    HTMLSlotElement& oldActive = *findSlotByName(slot.name());
    DCHECK_NE(oldActive, slot);
    m_slotMap->add(slot.name(), &slot);
    if (findSlotByName(slot.name()) == oldActive)
        return;
    // |oldActive| is no longer an active slot.
    if (oldActive.findHostChildWithSameSlotName())
        oldActive.enqueueSlotChangeEvent();
    // TODO(hayato): We should not enqeueue a slotchange event for |oldActive|
    // if |oldActive| was inserted together with |slot|.
    // This could happen if |oldActive| and |slot| are descendants of the inserted node, and
    // |oldActive| is preceding |slot|.
}

void SlotAssignment::slotRemoved(HTMLSlotElement& slot)
{
    DCHECK_GT(m_slotCount, 0u);
    --m_slotCount;
    m_needsCollectSlots = true;

    DCHECK(m_slotMap->contains(slot.name()));
    HTMLSlotElement* oldActive = findSlotByName(slot.name());
    m_slotMap->remove(slot.name(), &slot);
    HTMLSlotElement* newActive = findSlotByName(slot.name());
    if (newActive && newActive != oldActive) {
        // |newActive| slot becomes an active slot.
        if (newActive->findHostChildWithSameSlotName())
            newActive->enqueueSlotChangeEvent();
        // TODO(hayato): Prevent a false-positive slotchange.
        // This could happen if more than one slots which have the same name are descendants of the removed node.
    }
}

bool SlotAssignment::findHostChildBySlotName(const AtomicString& slotName) const
{
    // TODO(hayato): Avoid traversing children every time.
    for (Node& child : NodeTraversal::childrenOf(m_owner->host())) {
        if (!child.isSlotable())
            continue;
        if (child.slotName() == slotName)
            return true;
    }
    return false;
}

void SlotAssignment::slotRenamed(const AtomicString& oldSlotName, HTMLSlotElement& slot)
{
    // |slot| has already new name. Thus, we can not use slot.hasAssignedNodesSynchronously.
    bool hasAssignedNodesBefore = (findSlotByName(oldSlotName) == &slot) && findHostChildBySlotName(oldSlotName);

    m_slotMap->remove(oldSlotName, &slot);
    m_slotMap->add(slot.name(), &slot);

    bool hasAssignedNodesAfter = slot.hasAssignedNodesSlow();

    if (hasAssignedNodesBefore || hasAssignedNodesAfter)
        slot.enqueueSlotChangeEvent();
}

void SlotAssignment::hostChildSlotNameChanged(const AtomicString& oldValue, const AtomicString& newValue)
{
    if (HTMLSlotElement* slot = findSlotByName(HTMLSlotElement::normalizeSlotName(oldValue))) {
        slot->enqueueSlotChangeEvent();
        m_owner->owner()->setNeedsDistributionRecalc();
    }
    if (HTMLSlotElement* slot = findSlotByName(HTMLSlotElement::normalizeSlotName(newValue))) {
        slot->enqueueSlotChangeEvent();
        m_owner->owner()->setNeedsDistributionRecalc();
    }
}

SlotAssignment::SlotAssignment(ShadowRoot& owner)
    : m_slotMap(DocumentOrderedMap::create())
    , m_owner(&owner)
    , m_needsCollectSlots(false)
    , m_slotCount(0)
{
}

static void detachNotAssignedNode(Node& node)
{
    if (node.layoutObject())
        node.lazyReattachIfAttached();
}

void SlotAssignment::resolveAssignment()
{
    for (Member<HTMLSlotElement> slot : slots())
        slot->saveAndClearDistribution();

    for (Node& child : NodeTraversal::childrenOf(m_owner->host())) {
        if (!child.isSlotable()) {
            detachNotAssignedNode(child);
            continue;
        }
        HTMLSlotElement* slot = findSlotByName(child.slotName());
        if (slot)
            slot->appendAssignedNode(child);
        else
            detachNotAssignedNode(child);
    }
}

void SlotAssignment::resolveDistribution()
{
    resolveAssignment();
    const HeapVector<Member<HTMLSlotElement>>& slots = this->slots();

    for (auto slot : slots)
        slot->resolveDistributedNodes();

    // Update each slot's distribution in reverse tree order so that a child slot is visited before its parent slot.
    for (auto slot = slots.rbegin(); slot != slots.rend(); ++slot) {
        (*slot)->updateDistributedNodesWithFallback();
        (*slot)->lazyReattachDistributedNodesIfNeeded();
    }
}

const HeapVector<Member<HTMLSlotElement>>& SlotAssignment::slots()
{
    if (m_needsCollectSlots)
        collectSlots();
    return m_slots;
}

HTMLSlotElement* SlotAssignment::findSlot(const Node& node)
{
    return node.isSlotable() ? findSlotByName(node.slotName()) : nullptr;
}

HTMLSlotElement* SlotAssignment::findSlotByName(const AtomicString& slotName)
{
    return m_slotMap->getSlotByName(slotName, m_owner.get());
}

void SlotAssignment::collectSlots()
{
    DCHECK(m_needsCollectSlots);
    m_slots.clear();

    m_slots.reserveCapacity(m_slotCount);
    for (HTMLSlotElement& slot : Traversal<HTMLSlotElement>::descendantsOf(*m_owner)) {
        m_slots.append(&slot);
    }
    m_needsCollectSlots = false;
    DCHECK_EQ(m_slots.size(), m_slotCount);
}

DEFINE_TRACE(SlotAssignment)
{
    visitor->trace(m_slots);
    visitor->trace(m_slotMap);
    visitor->trace(m_owner);
}

} // namespace blink
