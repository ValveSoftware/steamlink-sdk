/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/dom/MutationRecord.h"

#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/StaticNodeList.h"
#include "wtf/StdLibExtras.h"

namespace blink {

namespace {

class ChildListRecord : public MutationRecord {
 public:
  ChildListRecord(Node* target,
                  StaticNodeList* added,
                  StaticNodeList* removed,
                  Node* previousSibling,
                  Node* nextSibling)
      : m_target(target),
        m_addedNodes(added),
        m_removedNodes(removed),
        m_previousSibling(previousSibling),
        m_nextSibling(nextSibling) {}

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_target);
    visitor->trace(m_addedNodes);
    visitor->trace(m_removedNodes);
    visitor->trace(m_previousSibling);
    visitor->trace(m_nextSibling);
    MutationRecord::trace(visitor);
  }

 private:
  const AtomicString& type() override;
  Node* target() override { return m_target.get(); }
  StaticNodeList* addedNodes() override { return m_addedNodes.get(); }
  StaticNodeList* removedNodes() override { return m_removedNodes.get(); }
  Node* previousSibling() override { return m_previousSibling.get(); }
  Node* nextSibling() override { return m_nextSibling.get(); }

  Member<Node> m_target;
  Member<StaticNodeList> m_addedNodes;
  Member<StaticNodeList> m_removedNodes;
  Member<Node> m_previousSibling;
  Member<Node> m_nextSibling;
};

class RecordWithEmptyNodeLists : public MutationRecord {
 public:
  RecordWithEmptyNodeLists(Node* target, const String& oldValue)
      : m_target(target), m_oldValue(oldValue) {}

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_target);
    visitor->trace(m_addedNodes);
    visitor->trace(m_removedNodes);
    MutationRecord::trace(visitor);
  }

 private:
  Node* target() override { return m_target.get(); }
  String oldValue() override { return m_oldValue; }
  StaticNodeList* addedNodes() override {
    return lazilyInitializeEmptyNodeList(m_addedNodes);
  }
  StaticNodeList* removedNodes() override {
    return lazilyInitializeEmptyNodeList(m_removedNodes);
  }

  static StaticNodeList* lazilyInitializeEmptyNodeList(
      Member<StaticNodeList>& nodeList) {
    if (!nodeList)
      nodeList = StaticNodeList::createEmpty();
    return nodeList.get();
  }

  Member<Node> m_target;
  String m_oldValue;
  Member<StaticNodeList> m_addedNodes;
  Member<StaticNodeList> m_removedNodes;
};

class AttributesRecord : public RecordWithEmptyNodeLists {
 public:
  AttributesRecord(Node* target,
                   const QualifiedName& name,
                   const AtomicString& oldValue)
      : RecordWithEmptyNodeLists(target, oldValue),
        m_attributeName(name.localName()),
        m_attributeNamespace(name.namespaceURI()) {}

 private:
  const AtomicString& type() override;
  const AtomicString& attributeName() override { return m_attributeName; }
  const AtomicString& attributeNamespace() override {
    return m_attributeNamespace;
  }

  AtomicString m_attributeName;
  AtomicString m_attributeNamespace;
};

class CharacterDataRecord : public RecordWithEmptyNodeLists {
 public:
  CharacterDataRecord(Node* target, const String& oldValue)
      : RecordWithEmptyNodeLists(target, oldValue) {}

 private:
  const AtomicString& type() override;
};

class MutationRecordWithNullOldValue : public MutationRecord {
 public:
  MutationRecordWithNullOldValue(MutationRecord* record) : m_record(record) {}

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_record);
    MutationRecord::trace(visitor);
  }

 private:
  const AtomicString& type() override { return m_record->type(); }
  Node* target() override { return m_record->target(); }
  StaticNodeList* addedNodes() override { return m_record->addedNodes(); }
  StaticNodeList* removedNodes() override { return m_record->removedNodes(); }
  Node* previousSibling() override { return m_record->previousSibling(); }
  Node* nextSibling() override { return m_record->nextSibling(); }
  const AtomicString& attributeName() override {
    return m_record->attributeName();
  }
  const AtomicString& attributeNamespace() override {
    return m_record->attributeNamespace();
  }

  String oldValue() override { return String(); }

  Member<MutationRecord> m_record;
};

const AtomicString& ChildListRecord::type() {
  DEFINE_STATIC_LOCAL(AtomicString, childList, ("childList"));
  return childList;
}

const AtomicString& AttributesRecord::type() {
  DEFINE_STATIC_LOCAL(AtomicString, attributes, ("attributes"));
  return attributes;
}

const AtomicString& CharacterDataRecord::type() {
  DEFINE_STATIC_LOCAL(AtomicString, characterData, ("characterData"));
  return characterData;
}

}  // namespace

MutationRecord* MutationRecord::createChildList(Node* target,
                                                StaticNodeList* added,
                                                StaticNodeList* removed,
                                                Node* previousSibling,
                                                Node* nextSibling) {
  return new ChildListRecord(target, added, removed, previousSibling,
                             nextSibling);
}

MutationRecord* MutationRecord::createAttributes(Node* target,
                                                 const QualifiedName& name,
                                                 const AtomicString& oldValue) {
  return new AttributesRecord(target, name, oldValue);
}

MutationRecord* MutationRecord::createCharacterData(Node* target,
                                                    const String& oldValue) {
  return new CharacterDataRecord(target, oldValue);
}

MutationRecord* MutationRecord::createWithNullOldValue(MutationRecord* record) {
  return new MutationRecordWithNullOldValue(record);
}

MutationRecord::~MutationRecord() {}

}  // namespace blink
