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

#include "config.h"

#include "core/svg/SVGAnimateElement.h"

#include "core/CSSPropertyNames.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/svg/SVGAnimatedTypeAnimator.h"
#include "core/svg/SVGDocumentExtensions.h"

namespace WebCore {

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName, Document& document)
    : SVGAnimationElement(tagName, document)
{
    ASSERT(isSVGAnimateElement(*this));
    ScriptWrappable::init(this);
}

PassRefPtrWillBeRawPtr<SVGAnimateElement> SVGAnimateElement::create(Document& document)
{
    return adoptRefWillBeNoop(new SVGAnimateElement(SVGNames::animateTag, document));
}

SVGAnimateElement::~SVGAnimateElement()
{
}

AnimatedPropertyType SVGAnimateElement::animatedPropertyType()
{
    return ensureAnimator()->type();
}

bool SVGAnimateElement::hasValidAttributeType()
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    return animatedPropertyType() != AnimatedUnknown && !hasInvalidCSSAttributeType();
}

void SVGAnimateElement::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement)
{
    ASSERT(resultElement);
    SVGElement* targetElement = this->targetElement();
    if (!targetElement || !isSVGAnimateElement(*resultElement))
        return;

    ASSERT(percentage >= 0 && percentage <= 1);
    ASSERT(m_animator);
    ASSERT(animatedPropertyType() != AnimatedTransformList || isSVGAnimateTransformElement(*this));
    ASSERT(animatedPropertyType() != AnimatedUnknown);
    ASSERT(m_fromProperty);
    ASSERT(m_fromProperty->type() == animatedPropertyType());
    ASSERT(m_toProperty);

    SVGAnimateElement* resultAnimationElement = toSVGAnimateElement(resultElement);
    ASSERT(resultAnimationElement->m_animatedProperty);
    ASSERT(resultAnimationElement->animatedPropertyType() == animatedPropertyType());

    if (isSVGSetElement(*this))
        percentage = 1;

    if (calcMode() == CalcModeDiscrete)
        percentage = percentage < 0.5 ? 0 : 1;

    // Target element might have changed.
    m_animator->setContextElement(targetElement);

    // Values-animation accumulates using the last values entry corresponding to the end of duration time.
    SVGPropertyBase* toAtEndOfDurationProperty = m_toAtEndOfDurationProperty ? m_toAtEndOfDurationProperty.get() : m_toProperty.get();
    m_animator->calculateAnimatedValue(percentage, repeatCount, m_fromProperty.get(), m_toProperty.get(), toAtEndOfDurationProperty, resultAnimationElement->m_animatedProperty.get());
}

bool SVGAnimateElement::calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString)
{
    if (toAtEndOfDurationString.isEmpty())
        return false;
    m_toAtEndOfDurationProperty = ensureAnimator()->constructFromString(toAtEndOfDurationString);
    return true;
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    determinePropertyValueTypes(fromString, toString);
    ensureAnimator()->calculateFromAndToValues(m_fromProperty, m_toProperty, fromString, toString);
    return true;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    if (animationMode() == ByAnimation && !isAdditive())
        return false;

    // from-by animation may only be used with attributes that support addition (e.g. most numeric attributes).
    if (animationMode() == FromByAnimation && !animatedPropertyTypeSupportsAddition())
        return false;

    ASSERT(!isSVGSetElement(*this));

    determinePropertyValueTypes(fromString, byString);
    ensureAnimator()->calculateFromAndByValues(m_fromProperty, m_toProperty, fromString, byString);
    return true;
}

namespace {

WillBeHeapVector<RawPtrWillBeMember<SVGElement> > findElementInstances(SVGElement* targetElement)
{
    ASSERT(targetElement);
    WillBeHeapVector<RawPtrWillBeMember<SVGElement> > animatedElements;

    animatedElements.append(targetElement);

    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& instances = targetElement->instancesForElement();
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator end = instances.end();
    for (WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator it = instances.begin(); it != end; ++it) {
        if (SVGElement* shadowTreeElement = *it)
            animatedElements.append(shadowTreeElement);
    }

    return animatedElements;
}

}

void SVGAnimateElement::resetAnimatedType()
{
    SVGAnimatedTypeAnimator* animator = ensureAnimator();

    SVGElement* targetElement = this->targetElement();
    const QualifiedName& attributeName = this->attributeName();
    ShouldApplyAnimation shouldApply = shouldApplyAnimation(targetElement, attributeName);

    if (shouldApply == DontApplyAnimation)
        return;

    if (shouldApply == ApplyXMLAnimation) {
        // SVG DOM animVal animation code-path.
        WillBeHeapVector<RawPtrWillBeMember<SVGElement> > animatedElements = findElementInstances(targetElement);
        ASSERT(!animatedElements.isEmpty());

        WillBeHeapVector<RawPtrWillBeMember<SVGElement> >::const_iterator end = animatedElements.end();
        for (WillBeHeapVector<RawPtrWillBeMember<SVGElement> >::const_iterator it = animatedElements.begin(); it != end; ++it)
            document().accessSVGExtensions().addElementReferencingTarget(this, *it);

        if (!m_animatedProperty)
            m_animatedProperty = animator->startAnimValAnimation(animatedElements);
        else
            m_animatedProperty = animator->resetAnimValToBaseVal(animatedElements);

        return;
    }

    // CSS properties animation code-path.
    String baseValue;

    if (shouldApply == ApplyCSSAnimation) {
        ASSERT(SVGAnimationElement::isTargetAttributeCSSProperty(targetElement, attributeName));
        computeCSSPropertyValue(targetElement, cssPropertyID(attributeName.localName()), baseValue);
    }

    m_animatedProperty = animator->constructFromString(baseValue);
}

static inline void applyCSSPropertyToTarget(SVGElement* targetElement, CSSPropertyID id, const String& value)
{
#if !ENABLE(OILPAN)
    ASSERT_WITH_SECURITY_IMPLICATION(!targetElement->m_deletionHasBegun);
#endif

    MutableStylePropertySet* propertySet = targetElement->ensureAnimatedSMILStyleProperties();
    if (!propertySet->setProperty(id, value, false, 0))
        return;

    targetElement->setNeedsStyleRecalc(LocalStyleChange);
}

static inline void removeCSSPropertyFromTarget(SVGElement* targetElement, CSSPropertyID id)
{
#if !ENABLE(OILPAN)
    ASSERT_WITH_SECURITY_IMPLICATION(!targetElement->m_deletionHasBegun);
#endif
    targetElement->ensureAnimatedSMILStyleProperties()->removeProperty(id);
    targetElement->setNeedsStyleRecalc(LocalStyleChange);
}

static inline void applyCSSPropertyToTargetAndInstances(SVGElement* targetElement, const QualifiedName& attributeName, const String& valueAsString)
{
    ASSERT(targetElement);
    if (attributeName == anyQName() || !targetElement->inDocument() || !targetElement->parentNode())
        return;

    CSSPropertyID id = cssPropertyID(attributeName.localName());

    SVGElement::InstanceUpdateBlocker blocker(targetElement);
    applyCSSPropertyToTarget(targetElement, id, valueAsString);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& instances = targetElement->instancesForElement();
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator end = instances.end();
    for (WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator it = instances.begin(); it != end; ++it) {
        if (SVGElement* shadowTreeElement = *it)
            applyCSSPropertyToTarget(shadowTreeElement, id, valueAsString);
    }
}

static inline void removeCSSPropertyFromTargetAndInstances(SVGElement* targetElement, const QualifiedName& attributeName)
{
    ASSERT(targetElement);
    if (attributeName == anyQName() || !targetElement->inDocument() || !targetElement->parentNode())
        return;

    CSSPropertyID id = cssPropertyID(attributeName.localName());

    SVGElement::InstanceUpdateBlocker blocker(targetElement);
    removeCSSPropertyFromTarget(targetElement, id);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& instances = targetElement->instancesForElement();
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator end = instances.end();
    for (WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator it = instances.begin(); it != end; ++it) {
        if (SVGElement* shadowTreeElement = *it)
            removeCSSPropertyFromTarget(shadowTreeElement, id);
    }
}

static inline void notifyTargetAboutAnimValChange(SVGElement* targetElement, const QualifiedName& attributeName)
{
#if !ENABLE(OILPAN)
    ASSERT_WITH_SECURITY_IMPLICATION(!targetElement->m_deletionHasBegun);
#endif
    targetElement->invalidateSVGAttributes();
    targetElement->svgAttributeChanged(attributeName);
}

static inline void notifyTargetAndInstancesAboutAnimValChange(SVGElement* targetElement, const QualifiedName& attributeName)
{
    ASSERT(targetElement);
    if (attributeName == anyQName() || !targetElement->inDocument() || !targetElement->parentNode())
        return;

    SVGElement::InstanceUpdateBlocker blocker(targetElement);
    notifyTargetAboutAnimValChange(targetElement, attributeName);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& instances = targetElement->instancesForElement();
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator end = instances.end();
    for (WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator it = instances.begin(); it != end; ++it) {
        notifyTargetAboutAnimValChange(*it, attributeName);
    }
}

void SVGAnimateElement::clearAnimatedType(SVGElement* targetElement)
{
    if (!m_animatedProperty)
        return;

    if (!targetElement) {
        m_animatedProperty.clear();
        return;
    }

    if (ensureAnimator()->isAnimatingCSSProperty()) {
        // CSS properties animation code-path.
        removeCSSPropertyFromTargetAndInstances(targetElement, attributeName());
        m_animatedProperty.clear();
        return;
    }

    // SVG DOM animVal animation code-path.
    if (m_animator) {
        WillBeHeapVector<RawPtrWillBeMember<SVGElement> > animatedElements = findElementInstances(targetElement);
        m_animator->stopAnimValAnimation(animatedElements);
        notifyTargetAndInstancesAboutAnimValChange(targetElement, attributeName());
    }

    m_animatedProperty.clear();
}

void SVGAnimateElement::applyResultsToTarget()
{
    ASSERT(m_animator);
    ASSERT(animatedPropertyType() != AnimatedTransformList || isSVGAnimateTransformElement(*this));
    ASSERT(animatedPropertyType() != AnimatedUnknown);

    // Early exit if our animated type got destructed by a previous endedActiveInterval().
    if (!m_animatedProperty)
        return;

    if (m_animator->isAnimatingCSSProperty()) {
        // CSS properties animation code-path.
        // Convert the result of the animation to a String and apply it as CSS property on the target & all instances.
        applyCSSPropertyToTargetAndInstances(targetElement(), attributeName(), m_animatedProperty->valueAsString());
        return;
    }

    // SVG DOM animVal animation code-path.
    // At this point the SVG DOM values are already changed, unlike for CSS.
    // We only have to trigger update notifications here.
    notifyTargetAndInstancesAboutAnimValChange(targetElement(), attributeName());
}

bool SVGAnimateElement::animatedPropertyTypeSupportsAddition()
{
    // Spec: http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties.
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

bool SVGAnimateElement::isAdditive()
{
    if (animationMode() == ByAnimation || animationMode() == FromByAnimation)
        if (!animatedPropertyTypeSupportsAddition())
            return false;

    return SVGAnimationElement::isAdditive();
}

float SVGAnimateElement::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return -1;

    return ensureAnimator()->calculateDistance(fromString, toString);
}

void SVGAnimateElement::setTargetElement(SVGElement* target)
{
    SVGAnimationElement::setTargetElement(target);
    resetAnimatedPropertyType();
}

void SVGAnimateElement::setAttributeName(const QualifiedName& attributeName)
{
    SVGAnimationElement::setAttributeName(attributeName);
    resetAnimatedPropertyType();
}

void SVGAnimateElement::resetAnimatedPropertyType()
{
    ASSERT(!m_animatedProperty);
    m_fromProperty.clear();
    m_toProperty.clear();
    m_toAtEndOfDurationProperty.clear();
    m_animator.clear();
}

SVGAnimatedTypeAnimator* SVGAnimateElement::ensureAnimator()
{
    if (!m_animator)
        m_animator = SVGAnimatedTypeAnimator::create(this, targetElement());
    return m_animator.get();
}

void SVGAnimateElement::trace(Visitor* visitor)
{
    visitor->trace(m_animator);
    SVGAnimationElement::trace(visitor);
}

}
