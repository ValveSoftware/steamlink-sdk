/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef HTMLSlotElement_h
#define HTMLSlotElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLElement.h"

namespace blink {

class AssignedNodesOptions;

class CORE_EXPORT HTMLSlotElement final : public HTMLElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    DECLARE_NODE_FACTORY(HTMLSlotElement);

    const HeapVector<Member<Node>>& assignedNodes();
    const HeapVector<Member<Node>>& getDistributedNodes();
    const HeapVector<Member<Node>> assignedNodesForBinding(const AssignedNodesOptions&);

    Node* firstDistributedNode() const { return m_distributedNodes.isEmpty() ? nullptr : m_distributedNodes.first().get(); }
    Node* lastDistributedNode() const { return m_distributedNodes.isEmpty() ? nullptr : m_distributedNodes.last().get(); }

    Node* distributedNodeNextTo(const Node&) const;
    Node* distributedNodePreviousTo(const Node&) const;

    void appendAssignedNode(Node&);

    void resolveDistributedNodes();
    void appendDistributedNode(Node&);
    void appendDistributedNodesFrom(const HTMLSlotElement& other);

    void updateDistributedNodesWithFallback();

    void lazyReattachDistributedNodesIfNeeded();

    void attach(const AttachContext& = AttachContext()) final;
    void detach(const AttachContext& = AttachContext()) final;

    void attributeChanged(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason = ModifiedDirectly) final;

    short tabIndex() const override;
    AtomicString name() const;

    // This method can be slow because this has to traverse the children of a shadow host.
    // This method should be used only when m_assignedNodes is dirty.
    // e.g. To detect a slotchange event in DOM mutations.
    bool hasAssignedNodesSlow() const;
    bool findHostChildWithSameSlotName() const;

    void enqueueSlotChangeEvent();

    void clearDistribution();
    void saveAndClearDistribution();

    static AtomicString normalizeSlotName(const AtomicString&);

    DECLARE_VIRTUAL_TRACE();

private:
    HTMLSlotElement(Document&);

    InsertionNotificationRequest insertedInto(ContainerNode*) final;
    void removedFrom(ContainerNode*) final;
    void willRecalcStyle(StyleRecalcChange) final;

    void dispatchSlotChangeEvent();

    HeapVector<Member<Node>> m_assignedNodes;
    HeapVector<Member<Node>> m_distributedNodes;
    HeapVector<Member<Node>> m_oldDistributedNodes;
    HeapHashMap<Member<const Node>, size_t> m_distributedIndices;
    bool m_slotchangeEventEnqueued = false;
};

} // namespace blink

#endif // HTMLSlotElement_h
