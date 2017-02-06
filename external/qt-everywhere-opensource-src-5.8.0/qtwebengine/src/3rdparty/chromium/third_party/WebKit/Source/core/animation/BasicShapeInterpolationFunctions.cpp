// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/BasicShapeInterpolationFunctions.h"

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/CSSPositionAxisListInterpolationType.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/BasicShapes.h"
#include <memory>

namespace blink {

class BasicShapeNonInterpolableValue : public NonInterpolableValue {
public:
    static PassRefPtr<NonInterpolableValue> create(BasicShape::ShapeType type)
    {
        return adoptRef(new BasicShapeNonInterpolableValue(type));
    }
    static PassRefPtr<NonInterpolableValue> createPolygon(WindRule windRule, size_t size)
    {
        return adoptRef(new BasicShapeNonInterpolableValue(windRule, size));
    }

    BasicShape::ShapeType type() const { return m_type; }

    WindRule windRule() const { ASSERT(type() == BasicShape::BasicShapePolygonType); return m_windRule; }
    size_t size() const { ASSERT(type() == BasicShape::BasicShapePolygonType); return m_size; }

    bool isCompatibleWith(const BasicShapeNonInterpolableValue& other) const
    {
        if (type() != other.type())
            return false;
        switch (type()) {
        case BasicShape::BasicShapeCircleType:
        case BasicShape::BasicShapeEllipseType:
        case BasicShape::BasicShapeInsetType:
            return true;
        case BasicShape::BasicShapePolygonType:
            return windRule() == other.windRule()
                && size() == other.size();
        default:
            NOTREACHED();
            return false;
        }
    }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    BasicShapeNonInterpolableValue(BasicShape::ShapeType type)
        : m_type(type)
        , m_windRule(RULE_NONZERO)
        , m_size(0)
    {
        ASSERT(type != BasicShape::BasicShapePolygonType);
    }
    BasicShapeNonInterpolableValue(WindRule windRule, size_t size)
        : m_type(BasicShape::BasicShapePolygonType)
        , m_windRule(windRule)
        , m_size(size)
    { }

    const BasicShape::ShapeType m_type;
    const WindRule m_windRule;
    const size_t m_size;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(BasicShapeNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(BasicShapeNonInterpolableValue);

namespace {

std::unique_ptr<InterpolableValue> unwrap(InterpolationValue&& value)
{
    ASSERT(value.interpolableValue);
    return std::move(value.interpolableValue);
}

std::unique_ptr<InterpolableValue> convertCSSCoordinate(const CSSValue* coordinate)
{
    if (coordinate)
        return unwrap(CSSPositionAxisListInterpolationType::convertPositionAxisCSSValue(*coordinate));
    return unwrap(CSSLengthInterpolationType::maybeConvertLength(Length(50, Percent), 1));
}

std::unique_ptr<InterpolableValue> convertCoordinate(const BasicShapeCenterCoordinate& coordinate, double zoom)
{
    return unwrap(CSSLengthInterpolationType::maybeConvertLength(coordinate.computedLength(), zoom));
}

std::unique_ptr<InterpolableValue> createNeutralInterpolableCoordinate()
{
    return CSSLengthInterpolationType::createNeutralInterpolableValue();
}

BasicShapeCenterCoordinate createCoordinate(const InterpolableValue& interpolableValue, const CSSToLengthConversionData& conversionData)
{
    return BasicShapeCenterCoordinate(
        BasicShapeCenterCoordinate::TopLeft,
        CSSLengthInterpolationType::resolveInterpolableLength(interpolableValue, nullptr, conversionData));
}

std::unique_ptr<InterpolableValue> convertCSSRadius(const CSSPrimitiveValue* radius)
{
    if (!radius || radius->isValueID())
        return nullptr;
    return unwrap(CSSLengthInterpolationType::maybeConvertCSSValue(*radius));
}

std::unique_ptr<InterpolableValue> convertRadius(const BasicShapeRadius& radius, double zoom)
{
    if (radius.type() != BasicShapeRadius::Value)
        return nullptr;
    return unwrap(CSSLengthInterpolationType::maybeConvertLength(radius.value(), zoom));
}

std::unique_ptr<InterpolableValue> createNeutralInterpolableRadius()
{
    return CSSLengthInterpolationType::createNeutralInterpolableValue();
}

BasicShapeRadius createRadius(const InterpolableValue& interpolableValue, const CSSToLengthConversionData& conversionData)
{
    return BasicShapeRadius(CSSLengthInterpolationType::resolveInterpolableLength(interpolableValue, nullptr, conversionData, ValueRangeNonNegative));
}

std::unique_ptr<InterpolableValue> convertCSSLength(const CSSValue* length)
{
    if (!length)
        return CSSLengthInterpolationType::createNeutralInterpolableValue();
    return unwrap(CSSLengthInterpolationType::maybeConvertCSSValue(*length));
}

std::unique_ptr<InterpolableValue> convertLength(const Length& length, double zoom)
{
    return unwrap(CSSLengthInterpolationType::maybeConvertLength(length, zoom));
}

std::unique_ptr<InterpolableValue> convertCSSBorderRadiusWidth(const CSSValuePair* pair)
{
    return convertCSSLength(pair ? &pair->first() : nullptr);
}

std::unique_ptr<InterpolableValue> convertCSSBorderRadiusHeight(const CSSValuePair* pair)
{
    return convertCSSLength(pair ? &pair->second() : nullptr);
}

LengthSize createBorderRadius(const InterpolableValue& width, const InterpolableValue& height, const CSSToLengthConversionData& conversionData)
{
    return LengthSize(
        CSSLengthInterpolationType::resolveInterpolableLength(width, nullptr, conversionData, ValueRangeNonNegative),
        CSSLengthInterpolationType::resolveInterpolableLength(height, nullptr, conversionData, ValueRangeNonNegative));
}

namespace CircleFunctions {

enum CircleComponentIndex {
    CircleCenterXIndex,
    CircleCenterYIndex,
    CircleRadiusIndex,
    CircleComponentIndexCount,
};

InterpolationValue convertCSSValue(const CSSBasicShapeCircleValue& circle)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(CircleComponentIndexCount);
    list->set(CircleCenterXIndex, convertCSSCoordinate(circle.centerX()));
    list->set(CircleCenterYIndex, convertCSSCoordinate(circle.centerY()));

    std::unique_ptr<InterpolableValue> radius;
    if (!(radius = convertCSSRadius(circle.radius())))
        return nullptr;
    list->set(CircleRadiusIndex, std::move(radius));

    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::create(BasicShape::BasicShapeCircleType));
}

InterpolationValue convertBasicShape(const BasicShapeCircle& circle, double zoom)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(CircleComponentIndexCount);
    list->set(CircleCenterXIndex, convertCoordinate(circle.centerX(), zoom));
    list->set(CircleCenterYIndex, convertCoordinate(circle.centerY(), zoom));

    std::unique_ptr<InterpolableValue> radius;
    if (!(radius = convertRadius(circle.radius(), zoom)))
        return nullptr;
    list->set(CircleRadiusIndex, std::move(radius));

    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::create(BasicShape::BasicShapeCircleType));
}

std::unique_ptr<InterpolableValue> createNeutralValue()
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(CircleComponentIndexCount);
    list->set(CircleCenterXIndex, createNeutralInterpolableCoordinate());
    list->set(CircleCenterYIndex, createNeutralInterpolableCoordinate());
    list->set(CircleRadiusIndex, createNeutralInterpolableRadius());
    return std::move(list);
}

PassRefPtr<BasicShape> createBasicShape(const InterpolableValue& interpolableValue, const CSSToLengthConversionData& conversionData)
{
    RefPtr<BasicShapeCircle> circle = BasicShapeCircle::create();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    circle->setCenterX(createCoordinate(*list.get(CircleCenterXIndex), conversionData));
    circle->setCenterY(createCoordinate(*list.get(CircleCenterYIndex), conversionData));
    circle->setRadius(createRadius(*list.get(CircleRadiusIndex), conversionData));
    return circle.release();
}

} // namespace CircleFunctions

namespace EllipseFunctions {

enum EllipseComponentIndex {
    EllipseCenterXIndex,
    EllipseCenterYIndex,
    EllipseRadiusXIndex,
    EllipseRadiusYIndex,
    EllipseComponentIndexCount,
};

InterpolationValue convertCSSValue(const CSSBasicShapeEllipseValue& ellipse)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(EllipseComponentIndexCount);
    list->set(EllipseCenterXIndex, convertCSSCoordinate(ellipse.centerX()));
    list->set(EllipseCenterYIndex, convertCSSCoordinate(ellipse.centerY()));

    std::unique_ptr<InterpolableValue> radius;
    if (!(radius = convertCSSRadius(ellipse.radiusX())))
        return nullptr;
    list->set(EllipseRadiusXIndex, std::move(radius));
    if (!(radius = convertCSSRadius(ellipse.radiusY())))
        return nullptr;
    list->set(EllipseRadiusYIndex, std::move(radius));

    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::create(BasicShape::BasicShapeEllipseType));
}

InterpolationValue convertBasicShape(const BasicShapeEllipse& ellipse, double zoom)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(EllipseComponentIndexCount);
    list->set(EllipseCenterXIndex, convertCoordinate(ellipse.centerX(), zoom));
    list->set(EllipseCenterYIndex, convertCoordinate(ellipse.centerY(), zoom));

    std::unique_ptr<InterpolableValue> radius;
    if (!(radius = convertRadius(ellipse.radiusX(), zoom)))
        return nullptr;
    list->set(EllipseRadiusXIndex, std::move(radius));
    if (!(radius = convertRadius(ellipse.radiusY(), zoom)))
        return nullptr;
    list->set(EllipseRadiusYIndex, std::move(radius));

    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::create(BasicShape::BasicShapeEllipseType));
}

std::unique_ptr<InterpolableValue> createNeutralValue()
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(EllipseComponentIndexCount);
    list->set(EllipseCenterXIndex, createNeutralInterpolableCoordinate());
    list->set(EllipseCenterYIndex, createNeutralInterpolableCoordinate());
    list->set(EllipseRadiusXIndex, createNeutralInterpolableRadius());
    list->set(EllipseRadiusYIndex, createNeutralInterpolableRadius());
    return std::move(list);
}

PassRefPtr<BasicShape> createBasicShape(const InterpolableValue& interpolableValue, const CSSToLengthConversionData& conversionData)
{
    RefPtr<BasicShapeEllipse> ellipse = BasicShapeEllipse::create();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    ellipse->setCenterX(createCoordinate(*list.get(EllipseCenterXIndex), conversionData));
    ellipse->setCenterY(createCoordinate(*list.get(EllipseCenterYIndex), conversionData));
    ellipse->setRadiusX(createRadius(*list.get(EllipseRadiusXIndex), conversionData));
    ellipse->setRadiusY(createRadius(*list.get(EllipseRadiusYIndex), conversionData));
    return ellipse.release();
}

} // namespace EllipseFunctions

namespace InsetFunctions {

enum InsetComponentIndex {
    InsetTopIndex,
    InsetRightIndex,
    InsetBottomIndex,
    InsetLeftIndex,
    InsetBorderTopLeftWidthIndex,
    InsetBorderTopLeftHeightIndex,
    InsetBorderTopRightWidthIndex,
    InsetBorderTopRightHeightIndex,
    InsetBorderBottomRightWidthIndex,
    InsetBorderBottomRightHeightIndex,
    InsetBorderBottomLeftWidthIndex,
    InsetBorderBottomLeftHeightIndex,
    InsetComponentIndexCount,
};

InterpolationValue convertCSSValue(const CSSBasicShapeInsetValue& inset)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(InsetComponentIndexCount);
    list->set(InsetTopIndex, convertCSSLength(inset.top()));
    list->set(InsetRightIndex, convertCSSLength(inset.right()));
    list->set(InsetBottomIndex, convertCSSLength(inset.bottom()));
    list->set(InsetLeftIndex, convertCSSLength(inset.left()));

    list->set(InsetBorderTopLeftWidthIndex, convertCSSBorderRadiusWidth(inset.topLeftRadius()));
    list->set(InsetBorderTopLeftHeightIndex, convertCSSBorderRadiusHeight(inset.topLeftRadius()));
    list->set(InsetBorderTopRightWidthIndex, convertCSSBorderRadiusWidth(inset.topRightRadius()));
    list->set(InsetBorderTopRightHeightIndex, convertCSSBorderRadiusHeight(inset.topRightRadius()));
    list->set(InsetBorderBottomRightWidthIndex, convertCSSBorderRadiusWidth(inset.bottomRightRadius()));
    list->set(InsetBorderBottomRightHeightIndex, convertCSSBorderRadiusHeight(inset.bottomRightRadius()));
    list->set(InsetBorderBottomLeftWidthIndex, convertCSSBorderRadiusWidth(inset.bottomLeftRadius()));
    list->set(InsetBorderBottomLeftHeightIndex, convertCSSBorderRadiusHeight(inset.bottomLeftRadius()));
    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::create(BasicShape::BasicShapeInsetType));
}

InterpolationValue convertBasicShape(const BasicShapeInset& inset, double zoom)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(InsetComponentIndexCount);
    list->set(InsetTopIndex, convertLength(inset.top(), zoom));
    list->set(InsetRightIndex, convertLength(inset.right(), zoom));
    list->set(InsetBottomIndex, convertLength(inset.bottom(), zoom));
    list->set(InsetLeftIndex, convertLength(inset.left(), zoom));

    list->set(InsetBorderTopLeftWidthIndex, convertLength(inset.topLeftRadius().width(), zoom));
    list->set(InsetBorderTopLeftHeightIndex, convertLength(inset.topLeftRadius().height(), zoom));
    list->set(InsetBorderTopRightWidthIndex, convertLength(inset.topRightRadius().width(), zoom));
    list->set(InsetBorderTopRightHeightIndex, convertLength(inset.topRightRadius().height(), zoom));
    list->set(InsetBorderBottomRightWidthIndex, convertLength(inset.bottomRightRadius().width(), zoom));
    list->set(InsetBorderBottomRightHeightIndex, convertLength(inset.bottomRightRadius().height(), zoom));
    list->set(InsetBorderBottomLeftWidthIndex, convertLength(inset.bottomLeftRadius().width(), zoom));
    list->set(InsetBorderBottomLeftHeightIndex, convertLength(inset.bottomLeftRadius().height(), zoom));
    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::create(BasicShape::BasicShapeInsetType));
}

std::unique_ptr<InterpolableValue> createNeutralValue()
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(InsetComponentIndexCount);
    list->set(InsetTopIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetRightIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBottomIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetLeftIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());

    list->set(InsetBorderTopLeftWidthIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBorderTopLeftHeightIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBorderTopRightWidthIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBorderTopRightHeightIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBorderBottomRightWidthIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBorderBottomRightHeightIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBorderBottomLeftWidthIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    list->set(InsetBorderBottomLeftHeightIndex, CSSLengthInterpolationType::createNeutralInterpolableValue());
    return std::move(list);
}

PassRefPtr<BasicShape> createBasicShape(const InterpolableValue& interpolableValue, const CSSToLengthConversionData& conversionData)
{
    RefPtr<BasicShapeInset> inset = BasicShapeInset::create();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    inset->setTop(CSSLengthInterpolationType::resolveInterpolableLength(*list.get(InsetTopIndex), nullptr, conversionData));
    inset->setRight(CSSLengthInterpolationType::resolveInterpolableLength(*list.get(InsetRightIndex), nullptr, conversionData));
    inset->setBottom(CSSLengthInterpolationType::resolveInterpolableLength(*list.get(InsetBottomIndex), nullptr, conversionData));
    inset->setLeft(CSSLengthInterpolationType::resolveInterpolableLength(*list.get(InsetLeftIndex), nullptr, conversionData));

    inset->setTopLeftRadius(createBorderRadius(*list.get(InsetBorderTopLeftWidthIndex), *list.get(InsetBorderTopLeftHeightIndex), conversionData));
    inset->setTopRightRadius(createBorderRadius(*list.get(InsetBorderTopRightWidthIndex), *list.get(InsetBorderTopRightHeightIndex), conversionData));
    inset->setBottomRightRadius(createBorderRadius(*list.get(InsetBorderBottomRightWidthIndex), *list.get(InsetBorderBottomRightHeightIndex), conversionData));
    inset->setBottomLeftRadius(createBorderRadius(*list.get(InsetBorderBottomLeftWidthIndex), *list.get(InsetBorderBottomLeftHeightIndex), conversionData));
    return inset.release();
}

} // namespace InsetFunctions

namespace PolygonFunctions {

InterpolationValue convertCSSValue(const CSSBasicShapePolygonValue& polygon)
{
    size_t size = polygon.values().size();
    std::unique_ptr<InterpolableList> list = InterpolableList::create(size);
    for (size_t i = 0; i < size; i++)
        list->set(i, convertCSSLength(polygon.values()[i].get()));
    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::createPolygon(polygon.getWindRule(), size));
}

InterpolationValue convertBasicShape(const BasicShapePolygon& polygon, double zoom)
{
    size_t size = polygon.values().size();
    std::unique_ptr<InterpolableList> list = InterpolableList::create(size);
    for (size_t i = 0; i < size; i++)
        list->set(i, convertLength(polygon.values()[i], zoom));
    return InterpolationValue(std::move(list), BasicShapeNonInterpolableValue::createPolygon(polygon.getWindRule(), size));
}

std::unique_ptr<InterpolableValue> createNeutralValue(const BasicShapeNonInterpolableValue& nonInterpolableValue)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(nonInterpolableValue.size());
    for (size_t i = 0; i < nonInterpolableValue.size(); i++)
        list->set(i, CSSLengthInterpolationType::createNeutralInterpolableValue());
    return std::move(list);
}

PassRefPtr<BasicShape> createBasicShape(const InterpolableValue& interpolableValue, const BasicShapeNonInterpolableValue& nonInterpolableValue, const CSSToLengthConversionData& conversionData)
{
    RefPtr<BasicShapePolygon> polygon = BasicShapePolygon::create();
    polygon->setWindRule(nonInterpolableValue.windRule());
    const InterpolableList& list = toInterpolableList(interpolableValue);
    size_t size = nonInterpolableValue.size();
    ASSERT(list.length() == size);
    ASSERT(size % 2 == 0);
    for (size_t i = 0; i < size; i += 2) {
        polygon->appendPoint(
            CSSLengthInterpolationType::resolveInterpolableLength(*list.get(i), nullptr, conversionData),
            CSSLengthInterpolationType::resolveInterpolableLength(*list.get(i + 1), nullptr, conversionData));
    }
    return polygon.release();
}

} // namespace PolygonFunctions

} // namespace

InterpolationValue BasicShapeInterpolationFunctions::maybeConvertCSSValue(const CSSValue& value)
{
    if (value.isBasicShapeCircleValue())
        return CircleFunctions::convertCSSValue(toCSSBasicShapeCircleValue(value));
    if (value.isBasicShapeEllipseValue())
        return EllipseFunctions::convertCSSValue(toCSSBasicShapeEllipseValue(value));
    if (value.isBasicShapeInsetValue())
        return InsetFunctions::convertCSSValue(toCSSBasicShapeInsetValue(value));
    if (value.isBasicShapePolygonValue())
        return PolygonFunctions::convertCSSValue(toCSSBasicShapePolygonValue(value));
    return nullptr;
}

InterpolationValue BasicShapeInterpolationFunctions::maybeConvertBasicShape(const BasicShape* shape, double zoom)
{
    if (!shape)
        return nullptr;
    switch (shape->type()) {
    case BasicShape::BasicShapeCircleType:
        return CircleFunctions::convertBasicShape(toBasicShapeCircle(*shape), zoom);
    case BasicShape::BasicShapeEllipseType:
        return EllipseFunctions::convertBasicShape(toBasicShapeEllipse(*shape), zoom);
    case BasicShape::BasicShapeInsetType:
        return InsetFunctions::convertBasicShape(toBasicShapeInset(*shape), zoom);
    case BasicShape::BasicShapePolygonType:
        return PolygonFunctions::convertBasicShape(toBasicShapePolygon(*shape), zoom);
    default:
        NOTREACHED();
        return nullptr;
    }
}

std::unique_ptr<InterpolableValue> BasicShapeInterpolationFunctions::createNeutralValue(const NonInterpolableValue& untypedNonInterpolableValue)
{
    const BasicShapeNonInterpolableValue& nonInterpolableValue = toBasicShapeNonInterpolableValue(untypedNonInterpolableValue);
    switch (nonInterpolableValue.type()) {
    case BasicShape::BasicShapeCircleType:
        return CircleFunctions::createNeutralValue();
    case BasicShape::BasicShapeEllipseType:
        return EllipseFunctions::createNeutralValue();
    case BasicShape::BasicShapeInsetType:
        return InsetFunctions::createNeutralValue();
    case BasicShape::BasicShapePolygonType:
        return PolygonFunctions::createNeutralValue(nonInterpolableValue);
    default:
        NOTREACHED();
        return nullptr;
    }
}

bool BasicShapeInterpolationFunctions::shapesAreCompatible(const NonInterpolableValue& a, const NonInterpolableValue& b)
{
    return toBasicShapeNonInterpolableValue(a).isCompatibleWith(toBasicShapeNonInterpolableValue(b));
}

PassRefPtr<BasicShape> BasicShapeInterpolationFunctions::createBasicShape(const InterpolableValue& interpolableValue, const NonInterpolableValue& untypedNonInterpolableValue, const CSSToLengthConversionData& conversionData)
{
    const BasicShapeNonInterpolableValue& nonInterpolableValue = toBasicShapeNonInterpolableValue(untypedNonInterpolableValue);
    switch (nonInterpolableValue.type()) {
    case BasicShape::BasicShapeCircleType:
        return CircleFunctions::createBasicShape(interpolableValue, conversionData);
    case BasicShape::BasicShapeEllipseType:
        return EllipseFunctions::createBasicShape(interpolableValue, conversionData);
    case BasicShape::BasicShapeInsetType:
        return InsetFunctions::createBasicShape(interpolableValue, conversionData);
    case BasicShape::BasicShapePolygonType:
        return PolygonFunctions::createBasicShape(interpolableValue, nonInterpolableValue, conversionData);
    default:
        NOTREACHED();
        return nullptr;
    }
}

} // namespace blink
