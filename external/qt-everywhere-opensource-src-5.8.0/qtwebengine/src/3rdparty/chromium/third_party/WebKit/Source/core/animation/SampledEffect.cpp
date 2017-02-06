// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SampledEffect.h"

namespace blink {

SampledEffect::SampledEffect(KeyframeEffect* effect)
    : m_effect(effect)
    , m_sequenceNumber(effect->animation()->sequenceNumber())
    , m_priority(effect->getPriority())
{
}

void SampledEffect::clear()
{
    m_effect = nullptr;
    m_interpolations.clear();
}

bool SampledEffect::willNeverChange() const
{
    return !m_effect || !m_effect->animation();
}

void SampledEffect::removeReplacedInterpolations(const HashSet<PropertyHandle>& replacedProperties)
{
    size_t newSize = 0;
    for (auto& interpolation : m_interpolations) {
        if (!replacedProperties.contains(interpolation->getProperty()))
            m_interpolations[newSize++].swap(interpolation);
    }
    m_interpolations.shrink(newSize);
}

void SampledEffect::updateReplacedProperties(HashSet<PropertyHandle>& replacedProperties)
{
    for (const auto& interpolation : m_interpolations) {
        if (!interpolation->dependsOnUnderlyingValue())
            replacedProperties.add(interpolation->getProperty());
    }
}

DEFINE_TRACE(SampledEffect)
{
    visitor->trace(m_effect);
}

} // namespace blink
