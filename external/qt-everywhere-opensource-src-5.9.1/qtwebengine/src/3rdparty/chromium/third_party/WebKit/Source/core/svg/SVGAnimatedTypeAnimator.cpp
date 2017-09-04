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

#include "core/svg/SVGAnimatedTypeAnimator.h"

#include "core/svg/SVGAnimateTransformElement.h"
#include "core/svg/SVGAnimatedColor.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthList.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGString.h"
#include "core/svg/SVGTransformList.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace blink {

SVGAnimatedTypeAnimator::SVGAnimatedTypeAnimator(
    SVGAnimationElement* animationElement)
    : m_animationElement(animationElement),
      m_type(AnimatedUnknown),
      m_cssProperty(CSSPropertyInvalid) {
  DCHECK(m_animationElement);
}

void SVGAnimatedTypeAnimator::clear() {
  m_animatedProperty = nullptr;
  m_type = AnimatedUnknown;
  m_cssProperty = CSSPropertyInvalid;
}

void SVGAnimatedTypeAnimator::reset(const SVGElement& contextElement) {
  const QualifiedName& attributeName = m_animationElement->attributeName();
  m_animatedProperty = contextElement.propertyFromAttribute(attributeName);
  if (m_animatedProperty) {
    m_type = m_animatedProperty->type();
    m_cssProperty = m_animatedProperty->cssPropertyId();
  } else {
    m_type = SVGElement::animatedPropertyTypeForCSSAttribute(attributeName);
    m_cssProperty = m_type != AnimatedUnknown
                        ? cssPropertyID(attributeName.localName())
                        : CSSPropertyInvalid;
  }

  // Only <animateTransform> is allowed to animate AnimatedTransformList.
  // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
  if (m_type == AnimatedTransformList &&
      !isSVGAnimateTransformElement(*m_animationElement)) {
    m_type = AnimatedUnknown;
    m_cssProperty = CSSPropertyInvalid;
  }

  DCHECK(m_type != AnimatedPoint && m_type != AnimatedStringList &&
         m_type != AnimatedTransform);
}

SVGPropertyBase* SVGAnimatedTypeAnimator::createPropertyForAttributeAnimation(
    const String& value) const {
  // SVG DOM animVal animation code-path.
  if (m_type == AnimatedTransformList) {
    // TransformList must be animated via <animateTransform>, and its
    // {from,by,to} attribute values needs to be parsed w.r.t. its "type"
    // attribute.  Spec:
    // http://www.w3.org/TR/SVG/single-page.html#animate-AnimateTransformElement
    DCHECK(m_animationElement);
    SVGTransformType transformType =
        toSVGAnimateTransformElement(m_animationElement)->transformType();
    return SVGTransformList::create(transformType, value);
  }
  DCHECK(m_animatedProperty);
  return m_animatedProperty->currentValueBase()->cloneForAnimation(value);
}

SVGPropertyBase* SVGAnimatedTypeAnimator::createPropertyForCSSAnimation(
    const String& value) const {
  // CSS properties animation code-path.
  // Create a basic instance of the corresponding SVG property.
  // The instance will not have full context info. (e.g. SVGLengthMode)
  switch (m_type) {
    case AnimatedColor:
      return SVGColorProperty::create(value);
    case AnimatedNumber: {
      SVGNumber* property = SVGNumber::create();
      property->setValueAsString(value);
      return property;
    }
    case AnimatedLength: {
      SVGLength* property = SVGLength::create();
      property->setValueAsString(value);
      return property;
    }
    case AnimatedLengthList: {
      SVGLengthList* property = SVGLengthList::create();
      property->setValueAsString(value);
      return property;
    }
    case AnimatedString: {
      SVGString* property = SVGString::create();
      property->setValueAsString(value);
      return property;
    }
    // These types don't appear in the table in
    // SVGElement::animatedPropertyTypeForCSSAttribute() and thus don't need
    // support.
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
    case AnimatedUnknown:
      break;
    default:
      break;
  }
  NOTREACHED();
  return nullptr;
}

SVGPropertyBase* SVGAnimatedTypeAnimator::createPropertyForAnimation(
    const String& value) const {
  if (isAnimatingSVGDom())
    return createPropertyForAttributeAnimation(value);
  DCHECK(isAnimatingCSSProperty());
  return createPropertyForCSSAnimation(value);
}

SVGPropertyBase* SVGAnimatedTypeAnimator::createAnimatedValue() const {
  DCHECK(isAnimatingSVGDom());
  SVGPropertyBase* animatedValue = m_animatedProperty->createAnimatedValue();
  DCHECK_EQ(animatedValue->type(), m_type);
  return animatedValue;
}

DEFINE_TRACE(SVGAnimatedTypeAnimator) {
  visitor->trace(m_animationElement);
  visitor->trace(m_animatedProperty);
}

}  // namespace blink
