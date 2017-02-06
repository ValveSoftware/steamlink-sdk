// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Keyframe_h
#define Keyframe_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffect.h"
#include "core/animation/EffectModel.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

using PropertyHandleSet = HashSet<PropertyHandle>;

class Element;
class ComputedStyle;

// Represents a user specificed keyframe in a KeyframeEffect.
// http://w3c.github.io/web-animations/#keyframe
// FIXME: Make Keyframe immutable
class CORE_EXPORT Keyframe : public RefCounted<Keyframe> {
    USING_FAST_MALLOC(Keyframe);
    WTF_MAKE_NONCOPYABLE(Keyframe);
public:
    virtual ~Keyframe() { }

    void setOffset(double offset) { m_offset = offset; }
    double offset() const { return m_offset; }

    void setComposite(EffectModel::CompositeOperation composite) { m_composite = composite; }
    EffectModel::CompositeOperation composite() const { return m_composite; }

    void setEasing(PassRefPtr<TimingFunction> easing) { m_easing = easing; }
    TimingFunction& easing() const { return *m_easing; }

    static bool compareOffsets(const RefPtr<Keyframe>& a, const RefPtr<Keyframe>& b)
    {
        return a->offset() < b->offset();
    }

    virtual PropertyHandleSet properties() const = 0;

    virtual PassRefPtr<Keyframe> clone() const = 0;
    PassRefPtr<Keyframe> cloneWithOffset(double offset) const
    {
        RefPtr<Keyframe> theClone = clone();
        theClone->setOffset(offset);
        return theClone.release();
    }

    virtual bool isAnimatableValueKeyframe() const { return false; }
    virtual bool isStringKeyframe() const { return false; }

    // Represents a property-value pair in a keyframe.
    class PropertySpecificKeyframe : public RefCounted<PropertySpecificKeyframe> {
        USING_FAST_MALLOC(PropertySpecificKeyframe);
        WTF_MAKE_NONCOPYABLE(PropertySpecificKeyframe);
    public:
        virtual ~PropertySpecificKeyframe() { }
        double offset() const { return m_offset; }
        TimingFunction& easing() const { return *m_easing; }
        EffectModel::CompositeOperation composite() const { return m_composite; }
        double underlyingFraction() const { return m_composite == EffectModel::CompositeReplace ? 0 : 1; }
        virtual bool isNeutral() const { NOTREACHED(); return false; }
        virtual PassRefPtr<PropertySpecificKeyframe> cloneWithOffset(double offset) const = 0;

        // FIXME: Remove this once CompositorAnimations no longer depends on AnimatableValues
        virtual bool populateAnimatableValue(CSSPropertyID, Element&, const ComputedStyle* baseStyle, bool force) const { return false; }
        virtual const PassRefPtr<AnimatableValue> getAnimatableValue() const = 0;

        virtual bool isAnimatableValuePropertySpecificKeyframe() const { return false; }
        virtual bool isCSSPropertySpecificKeyframe() const { return false; }
        virtual bool isSVGPropertySpecificKeyframe() const { return false; }

        virtual PassRefPtr<PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const = 0;
        virtual PassRefPtr<Interpolation> createInterpolation(PropertyHandle, const Keyframe::PropertySpecificKeyframe& end) const;

    protected:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, EffectModel::CompositeOperation);

        double m_offset;
        RefPtr<TimingFunction> m_easing;
        EffectModel::CompositeOperation m_composite;
    };

    virtual PassRefPtr<PropertySpecificKeyframe> createPropertySpecificKeyframe(PropertyHandle) const = 0;

protected:
    Keyframe()
        : m_offset(nullValue())
        , m_composite(EffectModel::CompositeReplace)
        , m_easing(LinearTimingFunction::shared())
    {
    }
    Keyframe(double offset, EffectModel::CompositeOperation composite, PassRefPtr<TimingFunction> easing)
        : m_offset(offset)
        , m_composite(composite)
        , m_easing(easing)
    {
    }

    double m_offset;
    EffectModel::CompositeOperation m_composite;
    RefPtr<TimingFunction> m_easing;
};

using PropertySpecificKeyframe = Keyframe::PropertySpecificKeyframe;

} // namespace blink

#endif // Keyframe_h
