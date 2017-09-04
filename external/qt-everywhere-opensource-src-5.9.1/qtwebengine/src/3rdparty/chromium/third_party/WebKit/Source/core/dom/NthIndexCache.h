// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NthIndexCache_h
#define NthIndexCache_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"

namespace blink {

class Document;

class CORE_EXPORT NthIndexData final : public GarbageCollected<NthIndexData> {
  WTF_MAKE_NONCOPYABLE(NthIndexData);

 public:
  NthIndexData(ContainerNode&);
  NthIndexData(ContainerNode&, const QualifiedName& type);

  unsigned nthIndex(Element&) const;
  unsigned nthLastIndex(Element&) const;
  unsigned nthOfTypeIndex(Element&) const;
  unsigned nthLastOfTypeIndex(Element&) const;

 private:
  HeapHashMap<Member<Element>, unsigned> m_elementIndexMap;
  unsigned m_count = 0;

  DECLARE_TRACE();
};

class CORE_EXPORT NthIndexCache final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(NthIndexCache);

 public:
  explicit NthIndexCache(Document&);
  ~NthIndexCache();

  static unsigned nthChildIndex(Element&);
  static unsigned nthLastChildIndex(Element&);
  static unsigned nthOfTypeIndex(Element&);
  static unsigned nthLastOfTypeIndex(Element&);

 private:
  using IndexByType = HeapHashMap<String, Member<NthIndexData>>;
  using ParentMap = HeapHashMap<Member<Node>, Member<NthIndexData>>;
  using ParentMapForType = HeapHashMap<Member<Node>, Member<IndexByType>>;

  void cacheNthIndexDataForParent(Element&);
  void cacheNthOfTypeIndexDataForParent(Element&);
  IndexByType& ensureTypeIndexMap(ContainerNode&);
  NthIndexData* nthTypeIndexDataForParent(Element&) const;

  Member<Document> m_document;
  Member<ParentMap> m_parentMap;
  Member<ParentMapForType> m_parentMapForType;

#if DCHECK_IS_ON()
  uint64_t m_domTreeVersion;
#endif
};

}  // namespace blink

#endif  // NthIndexCache_h
