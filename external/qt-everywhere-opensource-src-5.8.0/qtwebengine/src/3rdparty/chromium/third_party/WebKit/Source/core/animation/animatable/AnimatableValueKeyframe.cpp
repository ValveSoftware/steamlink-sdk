// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/animatable/AnimatableValueKeyframe.h"

#include "core/animation/LegacyStyleInterpolation.h"

namespace blink {

AnimatableValueKeyframe::AnimatableValueKeyframe(const AnimatableValueKeyframe& copyFrom)
    : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing)
{
    for (PropertyValueMap::const_iterator iter = copyFrom.m_propertyValues.begin(); iter != copyFrom.m_propertyValues.end(); ++iter)
        setPropertyValue(iter->key, iter->value.get());
}

PropertyHandleSet AnimatableValueKeyframe::properties() const
{
    // This is not used in time-critical code, so we probably don't need to
    // worry about caching this result.
    PropertyHandleSet properties;
    for (PropertyValueMap::const_iterator iter = m_propertyValues.begin(); iter != m_propertyValues.end(); ++iter)
        properties.add(PropertyHandle(*iter.keys()));
    return properties;
}

PassRefPtr<Keyframe> AnimatableValueKeyframe::clone() const
{
    return adoptRef(new AnimatableValueKeyframe(*this));
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> AnimatableValueKeyframe::createPropertySpecificKeyframe(PropertyHandle property) const
{
    return PropertySpecificKeyframe::create(offset(), &easing(), propertyValue(property.cssProperty()), composite());
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> AnimatableValueKeyframe::PropertySpecificKeyframe::cloneWithOffset(double offset) const
{
    return create(offset, m_easing, m_value, m_composite);
}

PassRefPtr<Interpolation> AnimatableValueKeyframe::PropertySpecificKeyframe::createInterpolation(PropertyHandle property, const Keyframe::PropertySpecificKeyframe& end) const
{
    return LegacyStyleInterpolation::create(value(), toAnimatableValuePropertySpecificKeyframe(end).value(), property.cssProperty());
}

PassRefPtr<Keyframe::PropertySpecificKeyframe> AnimatableValueKeyframe::PropertySpecificKeyframe::neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const
{
    return create(offset, easing, AnimatableValue::neutralValue(), EffectModel::CompositeAdd);
}

} // namespace blink
