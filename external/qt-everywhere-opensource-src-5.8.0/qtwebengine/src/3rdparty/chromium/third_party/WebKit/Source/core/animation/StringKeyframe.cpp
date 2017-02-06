// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/StringKeyframe.h"

#include "core/StylePropertyShorthand.h"
#include "core/animation/DeferredLegacyStyleInterpolation.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/style/ComputedStyle.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

StringKeyframe::StringKeyframe(const StringKeyframe& copyFrom)
    : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing)
    , m_cssPropertyMap(copyFrom.m_cssPropertyMap->mutableCopy())
    , m_presentationAttributeMap(copyFrom.m_presentationAttributeMap->mutableCopy())
    , m_svgAttributeMap(copyFrom.m_svgAttributeMap)
{
}

void StringKeyframe::setCSSPropertyValue(CSSPropertyID property, const String& value, Element* element, StyleSheetContents* styleSheetContents)
{
    ASSERT(property != CSSPropertyInvalid);
    if (CSSAnimations::isAnimatableProperty(property))
        m_cssPropertyMap->setProperty(property, value, false, styleSheetContents);
}

void StringKeyframe::setCSSPropertyValue(CSSPropertyID property, CSSValue* value)
{
    ASSERT(property != CSSPropertyInvalid);
    ASSERT(CSSAnimations::isAnimatableProperty(property));
    m_cssPropertyMap->setProperty(property, value, false);
}

void StringKeyframe::setPresentationAttributeValue(CSSPropertyID property, const String& value, Element* element, StyleSheetContents* styleSheetContents)
{
    ASSERT(property != CSSPropertyInvalid);
    if (CSSAnimations::isAnimatableProperty(property))
        m_presentationAttributeMap->setProperty(property, value, false, styleSheetContents);
}

void StringKeyframe::setSVGAttributeValue(const QualifiedName& attributeName, const String& value)
{
    m_svgAttributeMap.set(&attributeName, value);
}

PropertyHandleSet StringKeyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertyHandleSet properties;
    for (unsigned i = 0; i < m_cssPropertyMap->propertyCount(); ++i) {
        StylePropertySet::PropertyReference propertyReference = m_cssPropertyMap->propertyAt(i);
        DCHECK(
            !isShorthandProperty(propertyReference.id()) || propertyReference.value()->isVariableReferenceValue())
            << "Web Animations: Encountered unexpanded shorthand CSS property (" << propertyReference.id() << ").";
        properties.add(PropertyHandle(propertyReference.id(), false));
    }

    for (unsigned i = 0; i < m_presentationAttributeMap->propertyCount(); ++i)
        properties.add(PropertyHandle(m_presentationAttributeMap->propertyAt(i).id(), true));

    for (const auto& key: m_svgAttributeMap.keys())
        properties.add(PropertyHandle(*key));

    return properties;
}

PassRefPtr<Keyframe> StringKeyframe::clone() const
{
    return adoptRef(new StringKeyframe(*this));
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::createPropertySpecificKeyframe(PropertyHandle property) const
{
    if (property.isCSSProperty())
        return CSSPropertySpecificKeyframe::create(offset(), &easing(), cssPropertyValue(property.cssProperty()), composite());

    if (property.isPresentationAttribute())
        return CSSPropertySpecificKeyframe::create(offset(), &easing(), presentationAttributeValue(property.presentationAttribute()), composite());

    ASSERT(property.isSVGAttribute());
    return SVGPropertySpecificKeyframe::create(offset(), &easing(), svgPropertyValue(property.svgAttribute()), composite());
}

bool StringKeyframe::CSSPropertySpecificKeyframe::populateAnimatableValue(CSSPropertyID property, Element& element, const ComputedStyle* baseStyle, bool force) const
{
    if (m_animatableValueCache && !force)
        return false;
    if (!baseStyle && (!m_value || DeferredLegacyStyleInterpolation::interpolationRequiresStyleResolve(*m_value)))
        return false;
    if (!element.document().frame())
        return false;
    m_animatableValueCache = StyleResolver::createAnimatableValueSnapshot(element, baseStyle, property, m_value.get());
    return true;
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::CSSPropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return create(offset, easing, nullptr, EffectModel::CompositeAdd);
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> StringKeyframe::CSSPropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    RefPtr<CSSPropertySpecificKeyframe> clone = create(offset, m_easing, m_value.get(), m_composite);
    clone->m_animatableValueCache = m_animatableValueCache;
    return clone.release();
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> SVGPropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    return create(offset, m_easing, m_value, m_composite);
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> SVGPropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return create(offset, easing, String(), EffectModel::CompositeAdd);
}

} // namespace blink
