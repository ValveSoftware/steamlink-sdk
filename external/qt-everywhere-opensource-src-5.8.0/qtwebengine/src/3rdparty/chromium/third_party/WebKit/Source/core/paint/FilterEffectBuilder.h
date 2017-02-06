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
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/heap/Handle.h"

class SkPaint;

namespace blink {

class Element;
class FilterOperations;
class ReferenceFilterOperation;
class SVGFilterElement;
class SVGFilterGraphNodeMap;

class CORE_EXPORT FilterEffectBuilder final : public GarbageCollectedFinalized<FilterEffectBuilder> {
public:
    static FilterEffectBuilder* create()
    {
        return new FilterEffectBuilder();
    }

    virtual ~FilterEffectBuilder();
    DECLARE_TRACE();

    static Filter* buildReferenceFilter(const ReferenceFilterOperation&, const FloatSize* zoomedReferenceBoxSize, const SkPaint* fillPaint, const SkPaint* strokePaint, Element&, FilterEffect* previousEffect, float zoom);
    static Filter* buildReferenceFilter(SVGFilterElement&, const FloatRect& referenceBox, const SkPaint* fillPaint, const SkPaint* strokePaint, FilterEffect* previousEffect, float zoom, SVGFilterGraphNodeMap* = nullptr);

    bool build(Element*, const FilterOperations&, float zoom, const FloatSize* zoomedReferenceBoxSize = nullptr, const SkPaint* fillPaint = nullptr, const SkPaint* strokePaint = nullptr);

    FilterEffect* lastEffect() const
    {
        return m_lastEffect;
    }

private:
    FilterEffectBuilder();

    Member<FilterEffect> m_lastEffect;
};

} // namespace blink

#endif // FilterEffectBuilder_h
