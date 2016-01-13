/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/svg/SVGAnimatedTypeAnimator.h"

#include "core/svg/SVGAnimateTransformElement.h"
#include "core/svg/SVGAnimatedColor.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthList.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGPaint.h"
#include "core/svg/SVGPointList.h"
#include "core/svg/SVGString.h"
#include "core/svg/SVGTransformList.h"

namespace WebCore {

SVGAnimatedTypeAnimator::SVGAnimatedTypeAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : m_animationElement(animationElement)
    , m_contextElement(contextElement)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    const QualifiedName& attributeName = m_animationElement->attributeName();
    m_animatedProperty = m_contextElement->propertyFromAttribute(attributeName);
    m_type = m_animatedProperty ? m_animatedProperty->type()
        : SVGElement::animatedPropertyTypeForCSSAttribute(attributeName);

    // Only <animateTransform> is allowed to animate AnimatedTransformList.
    // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
    if (m_type == AnimatedTransformList && !isSVGAnimateTransformElement(*animationElement))
        m_type = AnimatedUnknown;

    ASSERT(m_type != AnimatedPoint
        && m_type != AnimatedStringList
        && m_type != AnimatedTransform);
}

SVGAnimatedTypeAnimator::~SVGAnimatedTypeAnimator()
{
}

PassRefPtr<SVGPropertyBase> SVGAnimatedTypeAnimator::createPropertyForAnimation(const String& value)
{
    if (isAnimatingSVGDom()) {
        // SVG DOM animVal animation code-path.

        if (m_type == AnimatedTransformList) {
            // TransformList must be animated via <animateTransform>,
            // and its {from,by,to} attribute values needs to be parsed w.r.t. its "type" attribute.
            // Spec: http://www.w3.org/TR/SVG/single-page.html#animate-AnimateTransformElement
            ASSERT(m_animationElement);
            SVGTransformType transformType = toSVGAnimateTransformElement(m_animationElement)->transformType();
            return SVGTransformList::create(transformType, value);
        }

        ASSERT(m_animatedProperty);
        return m_animatedProperty->currentValueBase()->cloneForAnimation(value);
    }

    ASSERT(isAnimatingCSSProperty());

    // CSS properties animation code-path.
    // Create a basic instance of the corresponding SVG property.
    // The instance will not have full context info. (e.g. SVGLengthMode)

    switch (m_type) {
    case AnimatedColor:
        return SVGColorProperty::create(value.isEmpty() ? StyleColor::currentColor() : SVGPaint::colorFromRGBColorString(value));
    case AnimatedNumber: {
        RefPtr<SVGNumber> property = SVGNumber::create();
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedLength: {
        RefPtr<SVGLength> property = SVGLength::create(LengthModeOther);
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedLengthList: {
        RefPtr<SVGLengthList> property = SVGLengthList::create(LengthModeOther);
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedString: {
        RefPtr<SVGString> property = SVGString::create();
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }

    // These types don't appear in the table in SVGElement::animatedPropertyTypeForCSSAttribute() and thus don't need support.
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPath:
    case AnimatedPoint:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedRect:
    case AnimatedStringList:
    case AnimatedTransform:
    case AnimatedTransformList:
        ASSERT_NOT_REACHED();

    case AnimatedUnknown:
        ASSERT_NOT_REACHED();
    };

    ASSERT_NOT_REACHED();
    return nullptr;
}

PassRefPtr<SVGPropertyBase> SVGAnimatedTypeAnimator::constructFromString(const String& value)
{
    return createPropertyForAnimation(value);
}

void SVGAnimatedTypeAnimator::calculateFromAndToValues(RefPtr<SVGPropertyBase>& from, RefPtr<SVGPropertyBase>& to, const String& fromString, const String& toString)
{
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedTypeAnimator::calculateFromAndByValues(RefPtr<SVGPropertyBase>& from, RefPtr<SVGPropertyBase>& to, const String& fromString, const String& byString)
{
    from = constructFromString(fromString);
    to = constructFromString(byString);
    // FIXME(oilpan): Below .get() should be removed after transition types are gone.
    to->add(from.get(), m_contextElement);
}

namespace {

void setAnimatedValueOnAllTargetProperties(const WillBeHeapVector<RawPtrWillBeMember<SVGElement> >& list, const QualifiedName& attributeName, PassRefPtr<SVGPropertyBase> passValue)
{
    RefPtr<SVGPropertyBase> value = passValue;

    WillBeHeapVector<RawPtrWillBeMember<SVGElement> >::const_iterator it = list.begin();
    WillBeHeapVector<RawPtrWillBeMember<SVGElement> >::const_iterator itEnd = list.end();
    for (; it != itEnd; ++it) {
        RefPtr<SVGAnimatedPropertyBase> animatedProperty = (*it)->propertyFromAttribute(attributeName);
        if (animatedProperty)
            animatedProperty->setAnimatedValue(value);
    }
}

}

PassRefPtr<SVGPropertyBase> SVGAnimatedTypeAnimator::resetAnimation(const WillBeHeapVector<RawPtrWillBeMember<SVGElement> >& list)
{
    ASSERT(isAnimatingSVGDom());
    RefPtr<SVGPropertyBase> animatedValue = m_animatedProperty->createAnimatedValue();
    ASSERT(animatedValue->type() == m_type);
    setAnimatedValueOnAllTargetProperties(list, m_animatedProperty->attributeName(), animatedValue);

    return animatedValue.release();
}

PassRefPtr<SVGPropertyBase> SVGAnimatedTypeAnimator::startAnimValAnimation(const WillBeHeapVector<RawPtrWillBeMember<SVGElement> >& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElement::InstanceUpdateBlocker blocker(m_contextElement);

    return resetAnimation(list);
}

void SVGAnimatedTypeAnimator::stopAnimValAnimation(const WillBeHeapVector<RawPtrWillBeMember<SVGElement> >& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElement::InstanceUpdateBlocker blocker(m_contextElement);

    WillBeHeapVector<RawPtrWillBeMember<SVGElement> >::const_iterator it = list.begin();
    WillBeHeapVector<RawPtrWillBeMember<SVGElement> >::const_iterator itEnd = list.end();
    for (; it != itEnd; ++it) {
        RefPtr<SVGAnimatedPropertyBase> animatedProperty = (*it)->propertyFromAttribute(m_animatedProperty->attributeName());
        if (animatedProperty)
            animatedProperty->animationEnded();
    }
}

PassRefPtr<SVGPropertyBase> SVGAnimatedTypeAnimator::resetAnimValToBaseVal(const WillBeHeapVector<RawPtrWillBeMember<SVGElement> >& list)
{
    SVGElement::InstanceUpdateBlocker blocker(m_contextElement);

    return resetAnimation(list);
}

class ParsePropertyFromString {
public:
    explicit ParsePropertyFromString(SVGAnimatedTypeAnimator* animator)
        : m_animator(animator)
    {
    }

    PassRefPtr<SVGPropertyBase> operator()(SVGAnimationElement*, const String& value)
    {
        return m_animator->createPropertyForAnimation(value);
    }

private:
    SVGAnimatedTypeAnimator* m_animator;
};

void SVGAnimatedTypeAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGPropertyBase* from, SVGPropertyBase* to, SVGPropertyBase* toAtEndOfDuration, SVGPropertyBase* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    RefPtr<SVGPropertyBase> fromValue = m_animationElement->animationMode() == ToAnimation ? animated : from;
    RefPtr<SVGPropertyBase> toValue = to;
    RefPtr<SVGPropertyBase> toAtEndOfDurationValue = toAtEndOfDuration;
    RefPtr<SVGPropertyBase> animatedValue = animated;

    // Apply CSS inheritance rules.
    ParsePropertyFromString parsePropertyFromString(this);
    m_animationElement->adjustForInheritance<RefPtr<SVGPropertyBase>, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->fromPropertyValueType(), fromValue, m_contextElement);
    m_animationElement->adjustForInheritance<RefPtr<SVGPropertyBase>, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->toPropertyValueType(), toValue, m_contextElement);

    animatedValue->calculateAnimatedValue(m_animationElement, percentage, repeatCount, fromValue, toValue, toAtEndOfDurationValue, m_contextElement);
}

float SVGAnimatedTypeAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    RefPtr<SVGPropertyBase> fromValue = createPropertyForAnimation(fromString);
    RefPtr<SVGPropertyBase> toValue = createPropertyForAnimation(toString);
    return fromValue->calculateDistance(toValue, m_contextElement);
}

void SVGAnimatedTypeAnimator::trace(Visitor* visitor)
{
    visitor->trace(m_animationElement);
    visitor->trace(m_contextElement);
}

}
