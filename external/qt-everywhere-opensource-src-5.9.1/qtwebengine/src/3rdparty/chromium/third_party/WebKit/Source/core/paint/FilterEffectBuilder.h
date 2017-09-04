/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FilterEffectBuilder_h
#define FilterEffectBuilder_h

#include "core/CoreExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

class SkPaint;

namespace blink {

class CompositorFilterOperations;
class Filter;
class FilterEffect;
class FilterOperations;
class FloatRect;
class Node;
class ReferenceFilterOperation;
class SVGFilterElement;
class SVGFilterGraphNodeMap;

class CORE_EXPORT FilterEffectBuilder final {
  STACK_ALLOCATED();

 public:
  FilterEffectBuilder(Node*,
                      const FloatRect& zoomedReferenceBox,
                      float zoom,
                      const SkPaint* fillPaint = nullptr,
                      const SkPaint* strokePaint = nullptr);

  Filter* buildReferenceFilter(SVGFilterElement&,
                               FilterEffect* previousEffect,
                               SVGFilterGraphNodeMap* = nullptr) const;

  FilterEffect* buildFilterEffect(const FilterOperations&) const;
  CompositorFilterOperations buildFilterOperations(
      const FilterOperations&) const;

 private:
  Filter* buildReferenceFilter(const ReferenceFilterOperation&,
                               FilterEffect* previousEffect) const;

  Member<Node> m_targetContext;
  FloatRect m_referenceBox;
  float m_zoom;
  const SkPaint* m_fillPaint;
  const SkPaint* m_strokePaint;
};

}  // namespace blink

#endif  // FilterEffectBuilder_h
