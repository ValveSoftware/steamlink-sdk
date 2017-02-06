// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SampledEffect_h
#define SampledEffect_h

#include "core/animation/Animation.h"
#include "core/animation/Interpolation.h"
#include "core/animation/KeyframeEffect.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class SVGElement;

// Associates the results of sampling an EffectModel with metadata used for effect ordering and managing composited animations.
class SampledEffect : public GarbageCollectedFinalized<SampledEffect> {
    WTF_MAKE_NONCOPYABLE(SampledEffect);
public:
    static SampledEffect* create(KeyframeEffect* animation)
    {
        return new SampledEffect(animation);
    }

    void clear();

    const Vector<RefPtr<Interpolation>>& interpolations() const { return m_interpolations; }
    Vector<RefPtr<Interpolation>>& mutableInterpolations() { return m_interpolations; }

    KeyframeEffect* effect() const { return m_effect; }
    unsigned sequenceNumber() const { return m_sequenceNumber; }
    KeyframeEffect::Priority priority() const { return m_priority; }
    bool willNeverChange() const;
    void removeReplacedInterpolations(const HashSet<PropertyHandle>&);
    void updateReplacedProperties(HashSet<PropertyHandle>&);

    DECLARE_TRACE();

private:
    SampledEffect(KeyframeEffect*);

    WeakMember<KeyframeEffect> m_effect;
    Vector<RefPtr<Interpolation>> m_interpolations;
    const unsigned m_sequenceNumber;
    KeyframeEffect::Priority m_priority;
};

} // namespace blink

#endif // SampledEffect_h
