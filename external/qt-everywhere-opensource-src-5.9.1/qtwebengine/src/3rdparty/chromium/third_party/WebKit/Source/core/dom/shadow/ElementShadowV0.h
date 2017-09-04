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

#ifndef ElementShadowV0_h
#define ElementShadowV0_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/SelectRuleFeatureSet.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT ElementShadowV0 final
    : public GarbageCollectedFinalized<ElementShadowV0>,
      public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(ElementShadowV0);

 public:
  static ElementShadowV0* create(ElementShadow&);
  ~ElementShadowV0();

  void willAffectSelector();
  const SelectRuleFeatureSet& ensureSelectFeatureSet();

  const InsertionPoint* finalDestinationInsertionPointFor(const Node*) const;
  const DestinationInsertionPoints* destinationInsertionPointsFor(
      const Node*) const;

  void distribute();
  void didDistributeNode(const Node*, InsertionPoint*);
  void clearDistribution();

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  explicit ElementShadowV0(ElementShadow&);

  ShadowRoot& youngestShadowRoot() const;
  ShadowRoot& oldestShadowRoot() const;

  void distributeNodeChildrenTo(InsertionPoint*, ContainerNode*);

  void collectSelectFeatureSetFrom(const ShadowRoot&);
  bool needsSelectFeatureSet() const { return m_needsSelectFeatureSet; }
  void setNeedsSelectFeatureSet() { m_needsSelectFeatureSet = true; }

  Member<ElementShadow> m_elementShadow;
  using NodeToDestinationInsertionPoints =
      HeapHashMap<Member<const Node>, Member<DestinationInsertionPoints>>;
  NodeToDestinationInsertionPoints m_nodeToInsertionPoints;
  SelectRuleFeatureSet m_selectFeatures;
  bool m_needsSelectFeatureSet;
};

}  // namespace blink

#endif
