// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEffect_h
#define InterpolationEffect_h

#include "core/CoreExport.h"
#include "core/animation/Interpolation.h"
#include "core/animation/Keyframe.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/TimingFunction.h"

namespace blink {

// Stores all adjacent pairs of keyframes (represented by Interpolations) in a KeyframeEffectModel
// with keyframe offset data preprocessed for more efficient active keyframe pair sampling.
class CORE_EXPORT InterpolationEffect {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    InterpolationEffect()
        : m_isPopulated(false)
    { }

    bool isPopulated() const { return m_isPopulated; }
    void setPopulated() { m_isPopulated = true; }

    void clear()
    {
        m_isPopulated = false;
        m_interpolations.clear();
    }

    void getActiveInterpolations(double fraction, double iterationDuration, Vector<RefPtr<Interpolation>>&) const;

    void addInterpolation(PassRefPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
    {
        m_interpolations.append(InterpolationRecord(interpolation, easing, start, end, applyFrom, applyTo));
    }

    void addInterpolationsFromKeyframes(PropertyHandle, const Keyframe::PropertySpecificKeyframe& keyframeA, const Keyframe::PropertySpecificKeyframe& keyframeB, double applyFrom, double applyTo);

private:
    struct InterpolationRecord {
        InterpolationRecord(PassRefPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
            : m_interpolation(interpolation)
            , m_easing(easing)
            , m_start(start)
            , m_end(end)
            , m_applyFrom(applyFrom)
            , m_applyTo(applyTo)
        { }

        RefPtr<Interpolation> m_interpolation;
        RefPtr<TimingFunction> m_easing;
        double m_start;
        double m_end;
        double m_applyFrom;
        double m_applyTo;
    };

    bool m_isPopulated;
    Vector<InterpolationRecord> m_interpolations;
};

} // namespace blink

#endif // InterpolationEffect_h
