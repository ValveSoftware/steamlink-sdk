/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "core/svg/SVGAnimateElement.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/StyleChangeReason.h"
#include "core/svg/properties/SVGProperty.h"

namespace blink {

namespace {

bool isTargetAttributeCSSProperty(const SVGElement& targetElement,
                                  const QualifiedName& attributeName) {
  return SVGElement::isAnimatableCSSProperty(attributeName) ||
         targetElement.isPresentationAttribute(attributeName);
}

String computeCSSPropertyValue(SVGElement* element, CSSPropertyID id) {
  DCHECK(element);
  // TODO(fs): StyleEngine doesn't support document without a frame.
  // Refer to comment in Element::computedStyle.
  DCHECK(element->inActiveDocument());

  // Don't include any properties resulting from CSS Transitions/Animations or
  // SMIL animations, as we want to retrieve the "base value".
  element->setUseOverrideComputedStyle(true);
  String value =
      CSSComputedStyleDeclaration::create(element)->getPropertyValue(id);
  element->setUseOverrideComputedStyle(false);
  return value;
}

AnimatedPropertyValueType propertyValueType(const QualifiedName& attributeName,
                                            const String& value) {
  DEFINE_STATIC_LOCAL(const AtomicString, inherit, ("inherit"));
  if (value.isEmpty() || value != inherit ||
      !SVGElement::isAnimatableCSSProperty(attributeName))
    return RegularPropertyValue;
  return InheritValue;
}

}  // unnamed namespace

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName,
                                     Document& document)
    : SVGAnimationElement(tagName, document),
      m_animator(this),
      m_fromPropertyValueType(RegularPropertyValue),
      m_toPropertyValueType(RegularPropertyValue),
      m_attributeType(AttributeTypeAuto),
      m_hasInvalidCSSAttributeType(false) {}

SVGAnimateElement* SVGAnimateElement::create(Document& document) {
  return new SVGAnimateElement(SVGNames::animateTag, document);
}

SVGAnimateElement::~SVGAnimateElement() {}

bool SVGAnimateElement::isSVGAnimationAttributeSettingJavaScriptURL(
    const Attribute& attribute) const {
  if ((attribute.name() == SVGNames::fromAttr ||
       attribute.name() == SVGNames::toAttr) &&
      attributeValueIsJavaScriptURL(attribute))
    return true;

  if (attribute.name() == SVGNames::valuesAttr) {
    Vector<String> parts;
    if (!parseValues(attribute.value(), parts)) {
      // Assume the worst.
      return true;
    }
    for (const auto& part : parts) {
      if (protocolIsJavaScript(part))
        return true;
    }
  }

  return SVGSMILElement::isSVGAnimationAttributeSettingJavaScriptURL(attribute);
}

void SVGAnimateElement::parseAttribute(const QualifiedName& name,
                                       const AtomicString& oldValue,
                                       const AtomicString& value) {
  if (name == SVGNames::attributeTypeAttr) {
    setAttributeType(value);
    return;
  }
  SVGAnimationElement::parseAttribute(name, oldValue, value);
}

void SVGAnimateElement::svgAttributeChanged(const QualifiedName& attrName) {
  if (attrName == SVGNames::attributeTypeAttr) {
    animationAttributeChanged();
    return;
  }
  SVGAnimationElement::svgAttributeChanged(attrName);
}

AnimatedPropertyType SVGAnimateElement::animatedPropertyType() {
  if (!targetElement())
    return AnimatedUnknown;

  m_animator.reset(*targetElement());
  return m_animator.type();
}

bool SVGAnimateElement::hasValidTarget() {
  return SVGAnimationElement::hasValidTarget() && hasValidAttributeName() &&
         hasValidAttributeType();
}

bool SVGAnimateElement::hasValidAttributeName() const {
  return attributeName() != anyQName();
}

bool SVGAnimateElement::hasValidAttributeType() {
  if (!targetElement())
    return false;
  return animatedPropertyType() != AnimatedUnknown &&
         !hasInvalidCSSAttributeType();
}

bool SVGAnimateElement::shouldApplyAnimation(
    const SVGElement& targetElement,
    const QualifiedName& attributeName) {
  if (!hasValidTarget() || !targetElement.parentNode())
    return false;

  // Always animate CSS properties using the ApplyCSSAnimation code path,
  // regardless of the attributeType value.
  if (isTargetAttributeCSSProperty(targetElement, attributeName))
    return true;

  // If attributeType="CSS" and attributeName doesn't point to a CSS property,
  // ignore the animation.
  return getAttributeType() != AttributeTypeCSS;
}

SVGPropertyBase* SVGAnimateElement::adjustForInheritance(
    SVGPropertyBase* propertyValue,
    AnimatedPropertyValueType valueType) const {
  if (valueType != InheritValue)
    return propertyValue;
  // TODO(fs): At the moment the computed style gets returned as a String and
  // needs to get parsed again. In the future we might want to work with the
  // value type directly to avoid the String parsing.
  DCHECK(targetElement());
  Element* parent = targetElement()->parentElement();
  if (!parent || !parent->isSVGElement())
    return propertyValue;
  SVGElement* svgParent = toSVGElement(parent);
  // Replace 'inherit' by its computed property value.
  String value = computeCSSPropertyValue(svgParent, m_animator.cssProperty());
  return m_animator.createPropertyForAnimation(value);
}

void SVGAnimateElement::calculateAnimatedValue(float percentage,
                                               unsigned repeatCount,
                                               SVGSMILElement* resultElement) {
  DCHECK(resultElement);
  DCHECK(targetElement());
  if (!isSVGAnimateElement(*resultElement))
    return;

  ASSERT(percentage >= 0 && percentage <= 1);
  ASSERT(animatedPropertyType() != AnimatedTransformList ||
         isSVGAnimateTransformElement(*this));
  ASSERT(animatedPropertyType() != AnimatedUnknown);
  ASSERT(m_fromProperty);
  ASSERT(m_fromProperty->type() == animatedPropertyType());
  ASSERT(m_toProperty);

  SVGAnimateElement* resultAnimationElement =
      toSVGAnimateElement(resultElement);
  ASSERT(resultAnimationElement->m_animatedProperty);
  ASSERT(resultAnimationElement->animatedPropertyType() ==
         animatedPropertyType());

  if (isSVGSetElement(*this))
    percentage = 1;

  if (getCalcMode() == CalcModeDiscrete)
    percentage = percentage < 0.5 ? 0 : 1;

  // Target element might have changed.
  SVGElement* targetElement = this->targetElement();

  // Values-animation accumulates using the last values entry corresponding to
  // the end of duration time.
  SVGPropertyBase* animatedValue = resultAnimationElement->m_animatedProperty;
  SVGPropertyBase* toAtEndOfDurationValue =
      m_toAtEndOfDurationProperty ? m_toAtEndOfDurationProperty : m_toProperty;
  SVGPropertyBase* fromValue =
      getAnimationMode() == ToAnimation ? animatedValue : m_fromProperty.get();
  SVGPropertyBase* toValue = m_toProperty;

  // Apply CSS inheritance rules.
  fromValue = adjustForInheritance(fromValue, m_fromPropertyValueType);
  toValue = adjustForInheritance(toValue, m_toPropertyValueType);

  animatedValue->calculateAnimatedValue(this, percentage, repeatCount,
                                        fromValue, toValue,
                                        toAtEndOfDurationValue, targetElement);
}

bool SVGAnimateElement::calculateToAtEndOfDurationValue(
    const String& toAtEndOfDurationString) {
  if (toAtEndOfDurationString.isEmpty())
    return false;
  m_toAtEndOfDurationProperty =
      m_animator.createPropertyForAnimation(toAtEndOfDurationString);
  return true;
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString,
                                                 const String& toString) {
  DCHECK(targetElement());
  m_fromProperty = m_animator.createPropertyForAnimation(fromString);
  m_fromPropertyValueType = propertyValueType(attributeName(), fromString);
  m_toProperty = m_animator.createPropertyForAnimation(toString);
  m_toPropertyValueType = propertyValueType(attributeName(), toString);
  return true;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString,
                                                 const String& byString) {
  DCHECK(targetElement());

  if (getAnimationMode() == ByAnimation && !isAdditive())
    return false;

  // from-by animation may only be used with attributes that support addition
  // (e.g. most numeric attributes).
  if (getAnimationMode() == FromByAnimation &&
      !animatedPropertyTypeSupportsAddition())
    return false;

  DCHECK(!isSVGSetElement(*this));

  m_fromProperty = m_animator.createPropertyForAnimation(fromString);
  m_fromPropertyValueType = propertyValueType(attributeName(), fromString);
  m_toProperty = m_animator.createPropertyForAnimation(byString);
  m_toPropertyValueType = propertyValueType(attributeName(), byString);
  m_toProperty->add(m_fromProperty, targetElement());
  return true;
}

void SVGAnimateElement::resetAnimatedType() {
  SVGElement* targetElement = this->targetElement();
  const QualifiedName& attributeName = this->attributeName();

  m_animator.reset(*targetElement);

  if (!shouldApplyAnimation(*targetElement, attributeName))
    return;
  if (m_animator.isAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    m_animatedProperty = m_animator.createAnimatedValue();
    targetElement->setAnimatedAttribute(attributeName, m_animatedProperty);
    return;
  }
  DCHECK(m_animator.isAnimatingCSSProperty());
  // Presentation attributes which has an SVG DOM representation should use the
  // "SVG DOM" code-path (above.)
  DCHECK(SVGElement::isAnimatableCSSProperty(attributeName));

  // CSS properties animation code-path.
  String baseValue =
      computeCSSPropertyValue(targetElement, m_animator.cssProperty());
  m_animatedProperty = m_animator.createPropertyForAnimation(baseValue);
}

void SVGAnimateElement::clearAnimatedType() {
  if (!m_animatedProperty)
    return;

  // The animated property lock is held for the "result animation" (see
  // SMILTimeContainer::updateAnimations()) while we're processing an animation
  // group. We will very likely crash later if we clear the animated type while
  // the lock is held. See crbug.com/581546.
  DCHECK(!animatedTypeIsLocked());

  SVGElement* targetElement = this->targetElement();
  if (!targetElement) {
    m_animatedProperty.clear();
    return;
  }

  bool shouldApply = shouldApplyAnimation(*targetElement, attributeName());
  if (m_animator.isAnimatingCSSProperty()) {
    // CSS properties animation code-path.
    if (shouldApply) {
      MutableStylePropertySet* propertySet =
          targetElement->ensureAnimatedSMILStyleProperties();
      if (propertySet->removeProperty(m_animator.cssProperty())) {
        targetElement->setNeedsStyleRecalc(
            LocalStyleChange,
            StyleChangeReasonForTracing::create(StyleChangeReason::Animation));
      }
    }
  }
  if (m_animator.isAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    targetElement->clearAnimatedAttribute(attributeName());
    if (shouldApply)
      targetElement->invalidateAnimatedAttribute(attributeName());
  }

  m_animatedProperty.clear();
  m_animator.clear();
}

void SVGAnimateElement::applyResultsToTarget() {
  ASSERT(animatedPropertyType() != AnimatedTransformList ||
         isSVGAnimateTransformElement(*this));
  ASSERT(animatedPropertyType() != AnimatedUnknown);

  // Early exit if our animated type got destructed by a previous
  // endedActiveInterval().
  if (!m_animatedProperty)
    return;

  if (!shouldApplyAnimation(*targetElement(), attributeName()))
    return;

  // We do update the style and the animation property independent of each
  // other.
  if (m_animator.isAnimatingCSSProperty()) {
    // CSS properties animation code-path.
    // Convert the result of the animation to a String and apply it as CSS
    // property on the target.
    MutableStylePropertySet* propertySet =
        targetElement()->ensureAnimatedSMILStyleProperties();
    if (propertySet->setProperty(m_animator.cssProperty(),
                                 m_animatedProperty->valueAsString(), false,
                                 0)) {
      targetElement()->setNeedsStyleRecalc(
          LocalStyleChange,
          StyleChangeReasonForTracing::create(StyleChangeReason::Animation));
    }
  }
  if (m_animator.isAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    // At this point the SVG DOM values are already changed, unlike for CSS.
    // We only have to trigger update notifications here.
    targetElement()->invalidateAnimatedAttribute(attributeName());
  }
}

bool SVGAnimateElement::animatedPropertyTypeSupportsAddition() {
  // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties.
  switch (animatedPropertyType()) {
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedPreserveAspectRatio:
    case AnimatedString:
    case AnimatedUnknown:
      return false;
    default:
      return true;
  }
}

bool SVGAnimateElement::isAdditive() {
  if (getAnimationMode() == ByAnimation ||
      getAnimationMode() == FromByAnimation) {
    if (!animatedPropertyTypeSupportsAddition())
      return false;
  }

  return SVGAnimationElement::isAdditive();
}

float SVGAnimateElement::calculateDistance(const String& fromString,
                                           const String& toString) {
  DCHECK(targetElement());
  // FIXME: A return value of float is not enough to support paced animations on
  // lists.
  SVGPropertyBase* fromValue =
      m_animator.createPropertyForAnimation(fromString);
  SVGPropertyBase* toValue = m_animator.createPropertyForAnimation(toString);
  return fromValue->calculateDistance(toValue, targetElement());
}

void SVGAnimateElement::setTargetElement(SVGElement* target) {
  SVGAnimationElement::setTargetElement(target);
  checkInvalidCSSAttributeType();
  resetAnimatedPropertyType();
}

void SVGAnimateElement::setAttributeName(const QualifiedName& attributeName) {
  SVGAnimationElement::setAttributeName(attributeName);
  checkInvalidCSSAttributeType();
  resetAnimatedPropertyType();
}

void SVGAnimateElement::setAttributeType(const AtomicString& attributeType) {
  if (attributeType == "CSS")
    m_attributeType = AttributeTypeCSS;
  else if (attributeType == "XML")
    m_attributeType = AttributeTypeXML;
  else
    m_attributeType = AttributeTypeAuto;
  checkInvalidCSSAttributeType();
}

void SVGAnimateElement::checkInvalidCSSAttributeType() {
  bool hasInvalidCSSAttributeType =
      targetElement() && hasValidAttributeName() &&
      getAttributeType() == AttributeTypeCSS &&
      !isTargetAttributeCSSProperty(*targetElement(), attributeName());

  if (hasInvalidCSSAttributeType != m_hasInvalidCSSAttributeType) {
    if (hasInvalidCSSAttributeType)
      unscheduleIfScheduled();

    m_hasInvalidCSSAttributeType = hasInvalidCSSAttributeType;

    if (!hasInvalidCSSAttributeType)
      schedule();
  }

  // Clear values that may depend on the previous target.
  if (targetElement())
    clearAnimatedType();
}

void SVGAnimateElement::resetAnimatedPropertyType() {
  ASSERT(!m_animatedProperty);
  m_fromProperty.clear();
  m_toProperty.clear();
  m_toAtEndOfDurationProperty.clear();
  m_animator.clear();
}

DEFINE_TRACE(SVGAnimateElement) {
  visitor->trace(m_fromProperty);
  visitor->trace(m_toProperty);
  visitor->trace(m_toAtEndOfDurationProperty);
  visitor->trace(m_animatedProperty);
  visitor->trace(m_animator);
  SVGAnimationElement::trace(visitor);
}

}  // namespace blink
