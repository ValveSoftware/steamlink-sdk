// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationType_h
#define InterpolationType_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/Keyframe.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include "core/animation/PrimitiveInterpolation.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/UnderlyingValueOwner.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include <memory>

namespace blink {

class InterpolationEnvironment;

// Subclasses of InterpolationType implement the logic for a specific value type of a specific PropertyHandle to:
// - Convert PropertySpecificKeyframe values to (Pairwise)?InterpolationValues: maybeConvertPairwise() and maybeConvertSingle()
// - Convert the target Element's property value to an InterpolationValue: maybeConvertUnderlyingValue()
// - Apply an InterpolationValue to a target Element's property: apply().
class InterpolationType {
    USING_FAST_MALLOC(InterpolationType);
    WTF_MAKE_NONCOPYABLE(InterpolationType);
public:
    virtual ~InterpolationType() { NOTREACHED(); }

    PropertyHandle getProperty() const { return m_property; }

    // ConversionCheckers are returned from calls to maybeConvertPairwise() and maybeConvertSingle() to enable the caller to check
    // whether the result is still valid given changes in the InterpolationEnvironment and underlying InterpolationValue.
    class ConversionChecker {
        USING_FAST_MALLOC(ConversionChecker);
        WTF_MAKE_NONCOPYABLE(ConversionChecker);
    public:
        virtual ~ConversionChecker() { }
        void setType(const InterpolationType& type) { m_type = &type; }
        const InterpolationType& type() const { return *m_type; }
        virtual bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const = 0;
    protected:
        ConversionChecker()
            : m_type(nullptr)
        { }
        const InterpolationType* m_type;
    };
    using ConversionCheckers = Vector<std::unique_ptr<ConversionChecker>>;

    virtual PairwiseInterpolationValue maybeConvertPairwise(const PropertySpecificKeyframe& startKeyframe, const PropertySpecificKeyframe& endKeyframe, const InterpolationEnvironment& environment, const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
    {
        InterpolationValue start = maybeConvertSingle(startKeyframe, environment, underlying, conversionCheckers);
        if (!start)
            return nullptr;
        InterpolationValue end = maybeConvertSingle(endKeyframe, environment, underlying, conversionCheckers);
        if (!end)
            return nullptr;
        return maybeMergeSingles(std::move(start), std::move(end));
    }

    virtual InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const = 0;

    virtual InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const = 0;

    virtual void composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
    {
        ASSERT(!underlyingValueOwner.value().nonInterpolableValue);
        ASSERT(!value.nonInterpolableValue);
        underlyingValueOwner.mutableValue().interpolableValue->scaleAndAdd(underlyingFraction, *value.interpolableValue);
    }

    virtual void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const = 0;

    // Implement reference equality checking via pointer equality checking as these are singletons.
    bool operator==(const InterpolationType& other) const { return this == &other; }
    bool operator!=(const InterpolationType& other) const { return this != &other; }

protected:
    InterpolationType(PropertyHandle property)
        : m_property(property)
    { }

    virtual PairwiseInterpolationValue maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
    {
        ASSERT(!start.nonInterpolableValue);
        ASSERT(!end.nonInterpolableValue);
        return PairwiseInterpolationValue(
            std::move(start.interpolableValue),
            std::move(end.interpolableValue),
            nullptr);
    }

    const PropertyHandle m_property;
};

} // namespace blink

#endif // InterpolationType_h
