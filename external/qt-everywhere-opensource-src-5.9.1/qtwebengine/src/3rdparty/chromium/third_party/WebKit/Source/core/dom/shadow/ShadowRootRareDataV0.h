/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ShadowRootRareDataV0_h
#define ShadowRootRareDataV0_h

#include "core/dom/shadow/InsertionPoint.h"
#include "wtf/Vector.h"

namespace blink {

class ShadowRootRareDataV0 : public GarbageCollected<ShadowRootRareDataV0> {
 public:
  ShadowRootRareDataV0()
      : m_descendantShadowElementCount(0), m_descendantContentElementCount(0) {}

  HTMLShadowElement* shadowInsertionPointOfYoungerShadowRoot() const {
    return m_shadowInsertionPointOfYoungerShadowRoot.get();
  }
  void setShadowInsertionPointOfYoungerShadowRoot(
      HTMLShadowElement* shadowInsertionPoint) {
    m_shadowInsertionPointOfYoungerShadowRoot = shadowInsertionPoint;
  }

  void didAddInsertionPoint(InsertionPoint*);
  void didRemoveInsertionPoint(InsertionPoint*);

  bool containsShadowElements() const { return m_descendantShadowElementCount; }
  bool containsContentElements() const {
    return m_descendantContentElementCount;
  }

  unsigned descendantShadowElementCount() const {
    return m_descendantShadowElementCount;
  }

  const HeapVector<Member<InsertionPoint>>& descendantInsertionPoints() {
    return m_descendantInsertionPoints;
  }
  void setDescendantInsertionPoints(HeapVector<Member<InsertionPoint>>& list) {
    m_descendantInsertionPoints.swap(list);
  }
  void clearDescendantInsertionPoints() { m_descendantInsertionPoints.clear(); }

  void setYoungerShadowRoot(ShadowRoot& youngerShadowRoot) {
    m_youngerShadowRoot = &youngerShadowRoot;
  }
  void setOlderShadowRoot(ShadowRoot& olderShadowRoot) {
    m_olderShadowRoot = &olderShadowRoot;
  }

  ShadowRoot* youngerShadowRoot() const { return m_youngerShadowRoot; }
  ShadowRoot* olderShadowRoot() const { return m_olderShadowRoot; }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_youngerShadowRoot);
    visitor->trace(m_olderShadowRoot);
    visitor->trace(m_shadowInsertionPointOfYoungerShadowRoot);
    visitor->trace(m_descendantInsertionPoints);
  }

 private:
  Member<ShadowRoot> m_youngerShadowRoot;
  Member<ShadowRoot> m_olderShadowRoot;
  Member<HTMLShadowElement> m_shadowInsertionPointOfYoungerShadowRoot;
  unsigned m_descendantShadowElementCount;
  unsigned m_descendantContentElementCount;
  HeapVector<Member<InsertionPoint>> m_descendantInsertionPoints;
};

inline void ShadowRootRareDataV0::didAddInsertionPoint(InsertionPoint* point) {
  DCHECK(point);
  if (isHTMLShadowElement(*point))
    ++m_descendantShadowElementCount;
  else if (isHTMLContentElement(*point))
    ++m_descendantContentElementCount;
  else
    NOTREACHED();
}

inline void ShadowRootRareDataV0::didRemoveInsertionPoint(
    InsertionPoint* point) {
  DCHECK(point);
  if (isHTMLShadowElement(*point)) {
    DCHECK_GT(m_descendantShadowElementCount, 0u);
    --m_descendantShadowElementCount;
  } else if (isHTMLContentElement(*point)) {
    DCHECK_GT(m_descendantContentElementCount, 0u);
    --m_descendantContentElementCount;
  } else {
    NOTREACHED();
  }
}

}  // namespace blink

#endif
