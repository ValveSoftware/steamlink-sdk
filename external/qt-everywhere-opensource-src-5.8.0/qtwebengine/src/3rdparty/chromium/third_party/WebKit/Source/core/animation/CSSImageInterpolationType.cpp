// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSImageInterpolationType.h"

#include "core/animation/ImagePropertyFunctions.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/StyleImage.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CSSImageNonInterpolableValue : public NonInterpolableValue {
public:
    ~CSSImageNonInterpolableValue() final { }

    static PassRefPtr<CSSImageNonInterpolableValue> create(CSSValue* start, CSSValue* end)
    {
        return adoptRef(new CSSImageNonInterpolableValue(start, end));
    }

    bool isSingle() const { return m_isSingle; }
    bool equals(const CSSImageNonInterpolableValue& other) const
    {
        return m_start->equals(*other.m_start) && m_end->equals(*other.m_end);
    }

    static PassRefPtr<CSSImageNonInterpolableValue> merge(PassRefPtr<NonInterpolableValue> start, PassRefPtr<NonInterpolableValue> end);

    CSSValue* crossfade(double progress) const
    {
        if (m_isSingle || progress <= 0)
            return m_start;
        if (progress >= 1)
            return m_end;
        return CSSCrossfadeValue::create(m_start, m_end, CSSPrimitiveValue::create(progress, CSSPrimitiveValue::UnitType::Number));
    }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSImageNonInterpolableValue(CSSValue* start, CSSValue* end)
        : m_start(start)
        , m_end(end)
        , m_isSingle(m_start == m_end)
    {
        ASSERT(m_start);
        ASSERT(m_end);
    }

    Persistent<CSSValue> m_start;
    Persistent<CSSValue> m_end;
    const bool m_isSingle;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSImageNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSImageNonInterpolableValue);

PassRefPtr<CSSImageNonInterpolableValue> CSSImageNonInterpolableValue::merge(PassRefPtr<NonInterpolableValue> start, PassRefPtr<NonInterpolableValue> end)
{
    const CSSImageNonInterpolableValue& startImagePair = toCSSImageNonInterpolableValue(*start);
    const CSSImageNonInterpolableValue& endImagePair = toCSSImageNonInterpolableValue(*end);
    ASSERT(startImagePair.m_isSingle);
    ASSERT(endImagePair.m_isSingle);
    return create(startImagePair.m_start, endImagePair.m_end);
}

InterpolationValue CSSImageInterpolationType::maybeConvertStyleImage(const StyleImage& styleImage, bool acceptGradients)
{
    return maybeConvertCSSValue(*styleImage.cssValue(), acceptGradients);
}

InterpolationValue CSSImageInterpolationType::maybeConvertCSSValue(const CSSValue& value, bool acceptGradients)
{
    if (value.isImageValue() || (value.isGradientValue() && acceptGradients)) {
        CSSValue* refableCSSValue = const_cast<CSSValue*>(&value);
        return InterpolationValue(InterpolableNumber::create(1), CSSImageNonInterpolableValue::create(refableCSSValue, refableCSSValue));
    }
    return nullptr;
}

PairwiseInterpolationValue CSSImageInterpolationType::staticMergeSingleConversions(InterpolationValue&& start, InterpolationValue&& end)
{
    if (!toCSSImageNonInterpolableValue(*start.nonInterpolableValue).isSingle()
        || !toCSSImageNonInterpolableValue(*end.nonInterpolableValue).isSingle()) {
        return nullptr;
    }
    return PairwiseInterpolationValue(
        InterpolableNumber::create(0),
        InterpolableNumber::create(1),
        CSSImageNonInterpolableValue::merge(start.nonInterpolableValue, end.nonInterpolableValue));
}

CSSValue* CSSImageInterpolationType::createCSSValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue)
{
    return toCSSImageNonInterpolableValue(nonInterpolableValue)->crossfade(toInterpolableNumber(interpolableValue).value());
}

StyleImage* CSSImageInterpolationType::resolveStyleImage(CSSPropertyID property, const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, StyleResolverState& state)
{
    CSSValue* image = createCSSValue(interpolableValue, nonInterpolableValue);
    return state.styleImage(property, *image);
}

bool CSSImageInterpolationType::equalNonInterpolableValues(const NonInterpolableValue* a, const NonInterpolableValue* b)
{
    return toCSSImageNonInterpolableValue(*a).equals(toCSSImageNonInterpolableValue(*b));
}

class UnderlyingImageChecker : public InterpolationType::ConversionChecker {
public:
    ~UnderlyingImageChecker() final {}

    static std::unique_ptr<UnderlyingImageChecker> create(const InterpolationValue& underlying)
    {
        return wrapUnique(new UnderlyingImageChecker(underlying));
    }

private:
    UnderlyingImageChecker(const InterpolationValue& underlying)
        : m_underlying(underlying.clone())
    { }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        if (!underlying && !m_underlying)
            return true;
        if (!underlying || !m_underlying)
            return false;
        return m_underlying.interpolableValue->equals(*underlying.interpolableValue)
            && CSSImageInterpolationType::equalNonInterpolableValues(m_underlying.nonInterpolableValue.get(), underlying.nonInterpolableValue.get());
    }

    const InterpolationValue m_underlying;
};

InterpolationValue CSSImageInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    conversionCheckers.append(UnderlyingImageChecker::create(underlying));
    return InterpolationValue(underlying.clone());
}

InterpolationValue CSSImageInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    return maybeConvertStyleImage(ImagePropertyFunctions::getInitialStyleImage(cssProperty()), true);
}

class ParentImageChecker : public InterpolationType::ConversionChecker {
public:
    ~ParentImageChecker() final {}

    static std::unique_ptr<ParentImageChecker> create(CSSPropertyID property, StyleImage* inheritedImage)
    {
        return wrapUnique(new ParentImageChecker(property, inheritedImage));
    }

private:
    ParentImageChecker(CSSPropertyID property, StyleImage* inheritedImage)
        : m_property(property)
        , m_inheritedImage(inheritedImage)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        const StyleImage* inheritedImage = ImagePropertyFunctions::getStyleImage(m_property, *environment.state().parentStyle());
        if (!m_inheritedImage && !inheritedImage)
            return true;
        if (!m_inheritedImage || !inheritedImage)
            return false;
        return *m_inheritedImage == *inheritedImage;
    }

    CSSPropertyID m_property;
    Persistent<StyleImage> m_inheritedImage;
};

InterpolationValue CSSImageInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;

    const StyleImage* inheritedImage = ImagePropertyFunctions::getStyleImage(cssProperty(), *state.parentStyle());
    StyleImage* refableImage = const_cast<StyleImage*>(inheritedImage);
    conversionCheckers.append(ParentImageChecker::create(cssProperty(), refableImage));
    return maybeConvertStyleImage(inheritedImage, true);
}

InterpolationValue CSSImageInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    return maybeConvertCSSValue(value, true);
}

InterpolationValue CSSImageInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return maybeConvertStyleImage(ImagePropertyFunctions::getStyleImage(cssProperty(), *environment.state().style()), true);
}

void CSSImageInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    underlyingValueOwner.set(*this, value);
}

void CSSImageInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    ImagePropertyFunctions::setStyleImage(cssProperty(), *environment.state().style(), resolveStyleImage(cssProperty(), interpolableValue, nonInterpolableValue, environment.state()));
}

} // namespace blink
