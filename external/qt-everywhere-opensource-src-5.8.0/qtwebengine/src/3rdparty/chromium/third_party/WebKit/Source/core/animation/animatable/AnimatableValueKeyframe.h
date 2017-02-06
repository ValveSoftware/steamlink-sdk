// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimatableValueKeyframe_h
#define AnimatableValueKeyframe_h

#include "core/CoreExport.h"
#include "core/animation/Keyframe.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/css/CSSPropertyIDTemplates.h"

namespace blink {

class CORE_EXPORT AnimatableValueKeyframe : public Keyframe {
public:
    static PassRefPtr<AnimatableValueKeyframe> create()
    {
        return adoptRef(new AnimatableValueKeyframe);
    }
    void setPropertyValue(CSSPropertyID property, PassRefPtr<AnimatableValue> value)
    {
        m_propertyValues.set(property, value);
    }
    void clearPropertyValue(CSSPropertyID property) { m_propertyValues.remove(property); }
    AnimatableValue* propertyValue(CSSPropertyID property) const
    {
        ASSERT(m_propertyValues.contains(property));
        return m_propertyValues.get(property);
    }
    PropertyHandleSet properties() const override;

    class PropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        static PassRefPtr<PropertySpecificKeyframe> create(double offset, PassRefPtr<TimingFunction> easing, PassRefPtr<AnimatableValue> value, EffectModel::CompositeOperation composite)
        {
            return adoptRef(new PropertySpecificKeyframe(offset, easing, value, composite));
        }

        AnimatableValue* value() const { return m_value.get(); }
        const PassRefPtr<AnimatableValue> getAnimatableValue() const final { return m_value; }

        PassRefPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const final;
        PassRefPtr<Interpolation> createInterpolation(PropertyHandle, const Keyframe::PropertySpecificKeyframe& end) const final;

    private:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, PassRefPtr<AnimatableValue> value, EffectModel::CompositeOperation composite)
            : Keyframe::PropertySpecificKeyframe(offset, easing, composite)
            , m_value(value)
        { }

        PassRefPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(double offset) const override;
        bool isAnimatableValuePropertySpecificKeyframe() const override { return true; }

        RefPtr<AnimatableValue> m_value;
    };

private:
    AnimatableValueKeyframe() { }

    AnimatableValueKeyframe(const AnimatableValueKeyframe& copyFrom);

    PassRefPtr<Keyframe> clone() const override;
    PassRefPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(PropertyHandle) const override;

    bool isAnimatableValueKeyframe() const override { return true; }

    using PropertyValueMap = HashMap<CSSPropertyID, RefPtr<AnimatableValue>>;
    PropertyValueMap m_propertyValues;
};

using AnimatableValuePropertySpecificKeyframe = AnimatableValueKeyframe::PropertySpecificKeyframe;

DEFINE_TYPE_CASTS(AnimatableValueKeyframe, Keyframe, value, value->isAnimatableValueKeyframe(), value.isAnimatableValueKeyframe());
DEFINE_TYPE_CASTS(AnimatableValuePropertySpecificKeyframe, Keyframe::PropertySpecificKeyframe, value, value->isAnimatableValuePropertySpecificKeyframe(), value.isAnimatableValuePropertySpecificKeyframe());

} // namespace blink

#endif
