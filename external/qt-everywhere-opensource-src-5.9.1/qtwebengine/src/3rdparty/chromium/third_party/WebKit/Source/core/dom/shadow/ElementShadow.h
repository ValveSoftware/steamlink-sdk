/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef ElementShadow_h
#define ElementShadow_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/CoreExport.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ElementShadowV0;

class CORE_EXPORT ElementShadow final
    : public GarbageCollectedFinalized<ElementShadow>,
      public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(ElementShadow);

 public:
  static ElementShadow* create();
  ~ElementShadow();

  Element& host() const {
    DCHECK(m_shadowRoot);
    return m_shadowRoot->host();
  }

  // TODO(hayato): Remove youngestShadowRoot() and oldestShadowRoot() from
  // ElementShadow
  ShadowRoot& youngestShadowRoot() const;
  ShadowRoot& oldestShadowRoot() const {
    DCHECK(m_shadowRoot);
    return *m_shadowRoot;
  }

  ElementShadow* containingShadow() const;

  ShadowRoot& addShadowRoot(Element& shadowHost, ShadowRootType);

  bool hasSameStyles(const ElementShadow&) const;

  void attach(const Node::AttachContext&);
  void detach(const Node::AttachContext&);

  void distributeIfNeeded();

  void setNeedsDistributionRecalc();
  bool needsDistributionRecalc() const { return m_needsDistributionRecalc; }

  bool isV1() const { return youngestShadowRoot().isV1(); }
  bool isOpenOrV0() const { return youngestShadowRoot().isOpenOrV0(); }

  ElementShadowV0& v0() const {
    DCHECK(m_elementShadowV0);
    return *m_elementShadowV0;
  }

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  ElementShadow();

  void appendShadowRoot(ShadowRoot&);
  void distribute();

  TraceWrapperMember<ElementShadowV0> m_elementShadowV0;
  TraceWrapperMember<ShadowRoot> m_shadowRoot;
  bool m_needsDistributionRecalc;
};

inline ShadowRoot* Node::youngestShadowRoot() const {
  if (!isElementNode())
    return nullptr;
  return toElement(this)->youngestShadowRoot();
}

inline ShadowRoot* Element::youngestShadowRoot() const {
  if (ElementShadow* shadow = this->shadow())
    return &shadow->youngestShadowRoot();
  return nullptr;
}

inline ElementShadow* ElementShadow::containingShadow() const {
  if (ShadowRoot* parentRoot = host().containingShadowRoot())
    return parentRoot->owner();
  return nullptr;
}

inline void ElementShadow::distributeIfNeeded() {
  if (m_needsDistributionRecalc)
    distribute();
  m_needsDistributionRecalc = false;
}

}  // namespace blink

#endif
