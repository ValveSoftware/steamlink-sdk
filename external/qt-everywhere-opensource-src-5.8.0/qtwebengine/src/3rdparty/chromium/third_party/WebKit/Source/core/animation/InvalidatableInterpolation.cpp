// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/InvalidatableInterpolation.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/resolver/StyleResolverState.h"
#include <memory>

namespace blink {

void InvalidatableInterpolation::interpolate(int, double fraction)
{
    if (fraction == m_currentFraction)
        return;

    if (m_currentFraction == 0 || m_currentFraction == 1 || fraction == 0 || fraction == 1)
        clearCache();

    m_currentFraction = fraction;
    if (m_isCached && m_cachedPairConversion)
        m_cachedPairConversion->interpolateValue(fraction, m_cachedValue);
    // We defer the interpolation to ensureValidInterpolation() if m_cachedPairConversion is null.
}

std::unique_ptr<PairwisePrimitiveInterpolation> InvalidatableInterpolation::maybeConvertPairwise(const InterpolationEnvironment& environment, const UnderlyingValueOwner& underlyingValueOwner) const
{
    ASSERT(m_currentFraction != 0 && m_currentFraction != 1);
    for (const auto& interpolationType : m_interpolationTypes) {
        if ((m_startKeyframe->isNeutral() || m_endKeyframe->isNeutral()) && (!underlyingValueOwner || underlyingValueOwner.type() != *interpolationType))
            continue;
        ConversionCheckers conversionCheckers;
        PairwiseInterpolationValue result = interpolationType->maybeConvertPairwise(*m_startKeyframe, *m_endKeyframe, environment, underlyingValueOwner.value(), conversionCheckers);
        addConversionCheckers(*interpolationType, conversionCheckers);
        if (result) {
            return PairwisePrimitiveInterpolation::create(*interpolationType,
                std::move(result.startInterpolableValue),
                std::move(result.endInterpolableValue),
                result.nonInterpolableValue.release());
        }
    }
    return nullptr;
}

std::unique_ptr<TypedInterpolationValue> InvalidatableInterpolation::convertSingleKeyframe(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const UnderlyingValueOwner& underlyingValueOwner) const
{
    if (keyframe.isNeutral() && !underlyingValueOwner)
        return nullptr;
    for (const auto& interpolationType : m_interpolationTypes) {
        if (keyframe.isNeutral() && underlyingValueOwner.type() != *interpolationType)
            continue;
        ConversionCheckers conversionCheckers;
        InterpolationValue result = interpolationType->maybeConvertSingle(keyframe, environment, underlyingValueOwner.value(), conversionCheckers);
        addConversionCheckers(*interpolationType, conversionCheckers);
        if (result)
            return TypedInterpolationValue::create(*interpolationType, std::move(result.interpolableValue), result.nonInterpolableValue.release());
    }
    ASSERT(keyframe.isNeutral());
    return nullptr;
}

void InvalidatableInterpolation::addConversionCheckers(const InterpolationType& type, ConversionCheckers& conversionCheckers) const
{
    for (size_t i = 0; i < conversionCheckers.size(); i++) {
        conversionCheckers[i]->setType(type);
        m_conversionCheckers.append(std::move(conversionCheckers[i]));
    }
}

std::unique_ptr<TypedInterpolationValue> InvalidatableInterpolation::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    for (const auto& interpolationType : m_interpolationTypes) {
        InterpolationValue result = interpolationType->maybeConvertUnderlyingValue(environment);
        if (result)
            return TypedInterpolationValue::create(*interpolationType, std::move(result.interpolableValue), result.nonInterpolableValue.release());
    }
    return nullptr;
}

bool InvalidatableInterpolation::dependsOnUnderlyingValue() const
{
    return (m_startKeyframe->underlyingFraction() != 0 && m_currentFraction != 1) || (m_endKeyframe->underlyingFraction() != 0 && m_currentFraction != 0);
}

bool InvalidatableInterpolation::isNeutralKeyframeActive() const
{
    return (m_startKeyframe->isNeutral() && m_currentFraction != 1) || (m_endKeyframe->isNeutral() && m_currentFraction != 0);
}

void InvalidatableInterpolation::clearCache() const
{
    m_isCached = false;
    m_cachedPairConversion.reset();
    m_conversionCheckers.clear();
    m_cachedValue.reset();
}

bool InvalidatableInterpolation::isCacheValid(const InterpolationEnvironment& environment, const UnderlyingValueOwner& underlyingValueOwner) const
{
    if (!m_isCached)
        return false;
    if (isNeutralKeyframeActive()) {
        if (m_cachedPairConversion && m_cachedPairConversion->isFlip())
            return false;
        // Pairwise interpolation can never happen between different InterpolationTypes, neutral values always represent the underlying value.
        if (!underlyingValueOwner || !m_cachedValue || m_cachedValue->type() != underlyingValueOwner.type())
            return false;
    }
    for (const auto& checker : m_conversionCheckers) {
        if (!checker->isValid(environment, underlyingValueOwner.value()))
            return false;
    }
    return true;
}

const TypedInterpolationValue* InvalidatableInterpolation::ensureValidInterpolation(const InterpolationEnvironment& environment, const UnderlyingValueOwner& underlyingValueOwner) const
{
    ASSERT(!std::isnan(m_currentFraction));
    if (isCacheValid(environment, underlyingValueOwner))
        return m_cachedValue.get();
    clearCache();
    if (m_currentFraction == 0) {
        m_cachedValue = convertSingleKeyframe(*m_startKeyframe, environment, underlyingValueOwner);
    } else if (m_currentFraction == 1) {
        m_cachedValue = convertSingleKeyframe(*m_endKeyframe, environment, underlyingValueOwner);
    } else {
        std::unique_ptr<PairwisePrimitiveInterpolation> pairwiseConversion = maybeConvertPairwise(environment, underlyingValueOwner);
        if (pairwiseConversion) {
            m_cachedValue = pairwiseConversion->initialValue();
            m_cachedPairConversion = std::move(pairwiseConversion);
        } else {
            m_cachedPairConversion = FlipPrimitiveInterpolation::create(
                convertSingleKeyframe(*m_startKeyframe, environment, underlyingValueOwner),
                convertSingleKeyframe(*m_endKeyframe, environment, underlyingValueOwner));
        }
        m_cachedPairConversion->interpolateValue(m_currentFraction, m_cachedValue);
    }
    m_isCached = true;
    return m_cachedValue.get();
}

void InvalidatableInterpolation::setFlagIfInheritUsed(InterpolationEnvironment& environment) const
{
    if (!m_property.isCSSProperty() && !m_property.isPresentationAttribute())
        return;
    if (!environment.state().parentStyle())
        return;
    const CSSValue* startValue = toCSSPropertySpecificKeyframe(*m_startKeyframe).value();
    const CSSValue* endValue = toCSSPropertySpecificKeyframe(*m_endKeyframe).value();
    if ((startValue && startValue->isInheritedValue())
        || (endValue && endValue->isInheritedValue())) {
        environment.state().parentStyle()->setHasExplicitlyInheritedProperties();
    }
}

double InvalidatableInterpolation::underlyingFraction() const
{
    if (m_currentFraction == 0)
        return m_startKeyframe->underlyingFraction();
    if (m_currentFraction == 1)
        return m_endKeyframe->underlyingFraction();
    return m_cachedPairConversion->interpolateUnderlyingFraction(m_startKeyframe->underlyingFraction(), m_endKeyframe->underlyingFraction(), m_currentFraction);
}

void InvalidatableInterpolation::applyStack(const ActiveInterpolations& interpolations, InterpolationEnvironment& environment)
{
    ASSERT(!interpolations.isEmpty());
    size_t startingIndex = 0;

    // Compute the underlying value to composite onto.
    UnderlyingValueOwner underlyingValueOwner;
    const InvalidatableInterpolation& firstInterpolation = toInvalidatableInterpolation(*interpolations.at(startingIndex));
    if (firstInterpolation.dependsOnUnderlyingValue()) {
        underlyingValueOwner.set(firstInterpolation.maybeConvertUnderlyingValue(environment));
    } else {
        const TypedInterpolationValue* firstValue = firstInterpolation.ensureValidInterpolation(environment, underlyingValueOwner);
        // Fast path for replace interpolations that are the only one to apply.
        if (interpolations.size() == 1) {
            if (firstValue) {
                firstInterpolation.setFlagIfInheritUsed(environment);
                firstValue->type().apply(firstValue->interpolableValue(), firstValue->getNonInterpolableValue(), environment);
            }
            return;
        }
        underlyingValueOwner.set(firstValue);
        startingIndex++;
    }

    // Composite interpolations onto the underlying value.
    bool shouldApply = false;
    for (size_t i = startingIndex; i < interpolations.size(); i++) {
        const InvalidatableInterpolation& currentInterpolation = toInvalidatableInterpolation(*interpolations.at(i));
        ASSERT(currentInterpolation.dependsOnUnderlyingValue());
        const TypedInterpolationValue* currentValue = currentInterpolation.ensureValidInterpolation(environment, underlyingValueOwner);
        if (!currentValue)
            continue;
        shouldApply = true;
        currentInterpolation.setFlagIfInheritUsed(environment);
        double underlyingFraction = currentInterpolation.underlyingFraction();
        if (underlyingFraction == 0 || !underlyingValueOwner || underlyingValueOwner.type() != currentValue->type())
            underlyingValueOwner.set(currentValue);
        else
            currentValue->type().composite(underlyingValueOwner, underlyingFraction, currentValue->value(), currentInterpolation.m_currentFraction);
    }

    if (shouldApply && underlyingValueOwner)
        underlyingValueOwner.type().apply(*underlyingValueOwner.value().interpolableValue, underlyingValueOwner.value().nonInterpolableValue.get(), environment);
}

} // namespace blink
