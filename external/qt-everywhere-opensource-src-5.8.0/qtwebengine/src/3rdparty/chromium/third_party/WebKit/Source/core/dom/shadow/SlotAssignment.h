// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SlotAssignment_h
#define SlotAssignment_h

// #include "core/dom/DocumentOrderedList.h"
#include "core/dom/DocumentOrderedMap.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class HTMLSlotElement;
class Node;
class ShadowRoot;

// TODO(hayato): Support SlotAssignment for non-shadow trees, e.g. a document tree.
class SlotAssignment final : public GarbageCollected<SlotAssignment> {
public:
    static SlotAssignment* create(ShadowRoot& owner)
    {
        return new SlotAssignment(owner);
    }

    // Relevant DOM Standard: https://dom.spec.whatwg.org/#find-a-slot
    HTMLSlotElement* findSlot(const Node&);
    HTMLSlotElement* findSlotByName(const AtomicString& slotName);

    // DOM Standaard defines these two procedures:
    // 1. https://dom.spec.whatwg.org/#assign-a-slot
    //    void assignSlot(const Node& slottable);
    // 2. https://dom.spec.whatwg.org/#assign-slotables
    //    void assignSlotables(HTMLSlotElement&);
    // As an optimization, Blink does not implement these literally.
    // Instead, provide alternative, HTMLSlotElement::hasAssignedNodesSlow()
    // so that slotchange can be detected.

    void resolveDistribution();
    const HeapVector<Member<HTMLSlotElement>>& slots();

    void slotAdded(HTMLSlotElement&);
    void slotRemoved(HTMLSlotElement&);
    void slotRenamed(const AtomicString& oldName, HTMLSlotElement&);
    void hostChildSlotNameChanged(const AtomicString& oldValue, const AtomicString& newValue);

    bool findHostChildBySlotName(const AtomicString& slotName) const;

    DECLARE_TRACE();

private:
    explicit SlotAssignment(ShadowRoot& owner);

    void collectSlots();

    void resolveAssignment();
    void distributeTo(Node&, HTMLSlotElement&);

    HeapVector<Member<HTMLSlotElement>> m_slots;
    Member<DocumentOrderedMap> m_slotMap;
    WeakMember<ShadowRoot> m_owner;
    unsigned m_needsCollectSlots : 1;
    unsigned m_slotCount : 31;
};

} // namespace blink

#endif // HTMLSlotAssignment_h
