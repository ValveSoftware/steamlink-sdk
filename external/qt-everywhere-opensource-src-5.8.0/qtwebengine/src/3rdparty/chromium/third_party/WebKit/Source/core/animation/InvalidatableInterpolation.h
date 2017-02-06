// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InvalidatableInterpolation_h
#define InvalidatableInterpolation_h

#include "core/animation/InterpolationType.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/PropertyInterpolationTypesMapping.h"
#include "core/animation/StyleInterpolation.h"
#include "core/animation/TypedInterpolationValue.h"
#include <memory>

namespace blink {

// TODO(alancutter): This class will replace *StyleInterpolation and Interpolation.
// For now it needs to distinguish itself during the refactor and temporarily has an ugly name.
class CORE_EXPORT InvalidatableInterpolation : public Interpolation {
public:
    static PassRefPtr<InvalidatableInterpolation> create(
        PropertyHandle property,
        const InterpolationTypes& interpolationTypes,
        PassRefPtr<PropertySpecificKeyframe> startKeyframe,
        PassRefPtr<PropertySpecificKeyframe> endKeyframe)
    {
        return adoptRef(new InvalidatableInterpolation(property, interpolationTypes, startKeyframe, endKeyframe));
    }

    PropertyHandle getProperty() const final { return m_property; }
    virtual void interpolate(int iteration, double fraction);
    bool dependsOnUnderlyingValue() const final;
    virtual void apply(InterpolationEnvironment&) const { NOTREACHED(); }
    static void applyStack(const ActiveInterpolations&, InterpolationEnvironment&);

    virtual bool isInvalidatableInterpolation() const { return true; }

private:
    InvalidatableInterpolation(
        PropertyHandle property,
        const InterpolationTypes& interpolationTypes,
        PassRefPtr<PropertySpecificKeyframe> startKeyframe,
        PassRefPtr<PropertySpecificKeyframe> endKeyframe)
        : Interpolation(nullptr, nullptr)
        , m_property(property)
        , m_interpolationTypes(interpolationTypes)
        , m_startKeyframe(startKeyframe)
        , m_endKeyframe(endKeyframe)
        , m_currentFraction(std::numeric_limits<double>::quiet_NaN())
        , m_isCached(false)
    { }

    using ConversionCheckers = InterpolationType::ConversionCheckers;

    std::unique_ptr<TypedInterpolationValue> maybeConvertUnderlyingValue(const InterpolationEnvironment&) const;
    const TypedInterpolationValue* ensureValidInterpolation(const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    void clearCache() const;
    bool isCacheValid(const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    bool isNeutralKeyframeActive() const;
    std::unique_ptr<PairwisePrimitiveInterpolation> maybeConvertPairwise(const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    std::unique_ptr<TypedInterpolationValue> convertSingleKeyframe(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const UnderlyingValueOwner&) const;
    void addConversionCheckers(const InterpolationType&, ConversionCheckers&) const;
    void setFlagIfInheritUsed(InterpolationEnvironment&) const;
    double underlyingFraction() const;

    const PropertyHandle m_property;
    const InterpolationTypes& m_interpolationTypes;
    RefPtr<PropertySpecificKeyframe> m_startKeyframe;
    RefPtr<PropertySpecificKeyframe> m_endKeyframe;
    double m_currentFraction;
    mutable bool m_isCached;
    mutable std::unique_ptr<PrimitiveInterpolation> m_cachedPairConversion;
    mutable ConversionCheckers m_conversionCheckers;
    mutable std::unique_ptr<TypedInterpolationValue> m_cachedValue;
};

DEFINE_TYPE_CASTS(InvalidatableInterpolation, Interpolation, value, value->isInvalidatableInterpolation(), value.isInvalidatableInterpolation());

} // namespace blink

#endif // InvalidatableInterpolation_h
