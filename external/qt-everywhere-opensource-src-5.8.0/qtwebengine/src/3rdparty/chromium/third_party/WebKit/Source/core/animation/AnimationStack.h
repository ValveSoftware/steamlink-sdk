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

#ifndef AnimationStack_h
#define AnimationStack_h

#include "core/CoreExport.h"
#include "core/animation/Animation.h"
#include "core/animation/EffectModel.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/SampledEffect.h"
#include "platform/geometry/FloatBox.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

using ActiveInterpolationsMap = HashMap<PropertyHandle, ActiveInterpolations>;

class InertEffect;

// Represents the order in which a sequence of SampledEffects should apply.
// This sequence is broken down to per PropertyHandle granularity.
class CORE_EXPORT AnimationStack {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(AnimationStack);
public:
    AnimationStack();

    void add(SampledEffect* sampledEffect) { m_sampledEffects.append(sampledEffect); }
    bool isEmpty() const { return m_sampledEffects.isEmpty(); }
    bool hasActiveAnimationsOnCompositor(CSSPropertyID) const;

    using PropertyHandleFilter = bool (*)(const PropertyHandle&);
    static ActiveInterpolationsMap activeInterpolations(AnimationStack*, const HeapVector<Member<const InertEffect>>* newAnimations, const HeapHashSet<Member<const Animation>>* suppressedAnimations, KeyframeEffect::Priority, PropertyHandleFilter = nullptr);

    bool getAnimatedBoundingBox(FloatBox&, CSSPropertyID) const;
    DECLARE_TRACE();

private:
    void removeRedundantSampledEffects();

    // Effects sorted by priority. Lower priority at the start of the list.
    HeapVector<Member<SampledEffect>> m_sampledEffects;

    friend class AnimationAnimationStackTest;
};

} // namespace blink

#endif // AnimationStack_h
