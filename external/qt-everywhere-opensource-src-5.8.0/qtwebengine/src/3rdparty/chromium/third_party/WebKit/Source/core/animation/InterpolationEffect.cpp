// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/InterpolationEffect.h"

namespace blink {

void InterpolationEffect::getActiveInterpolations(double fraction, double iterationDuration, Vector<RefPtr<Interpolation>>& result) const
{
    size_t existingSize = result.size();
    size_t resultIndex = 0;

    for (const auto& record : m_interpolations) {
        if (fraction >= record.m_applyFrom && fraction < record.m_applyTo) {
            RefPtr<Interpolation> interpolation = record.m_interpolation;
            double recordLength = record.m_end - record.m_start;
            double localFraction = recordLength ? (fraction - record.m_start) / recordLength : 0.0;
            if (record.m_easing)
                localFraction = record.m_easing->evaluate(localFraction, accuracyForDuration(iterationDuration));
            interpolation->interpolate(0, localFraction);
            if (resultIndex < existingSize)
                result[resultIndex++] = interpolation;
            else
                result.append(interpolation);
        }
    }
    if (resultIndex < existingSize)
        result.shrink(resultIndex);
}

void InterpolationEffect::addInterpolationsFromKeyframes(PropertyHandle property, const Keyframe::PropertySpecificKeyframe& keyframeA, const Keyframe::PropertySpecificKeyframe& keyframeB, double applyFrom, double applyTo)
{
    addInterpolation(keyframeA.createInterpolation(property, keyframeB), &keyframeA.easing(), keyframeA.offset(), keyframeB.offset(), applyFrom, applyTo);
}

} // namespace blink
