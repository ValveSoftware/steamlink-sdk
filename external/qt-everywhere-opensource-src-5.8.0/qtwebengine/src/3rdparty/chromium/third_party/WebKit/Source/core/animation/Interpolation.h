// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Interpolation_h
#define Interpolation_h

#include "core/CoreExport.h"
#include "core/animation/InterpolableValue.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include <memory>

namespace blink {

class PropertyHandle;

// Represents an animation's effect between an adjacent pair of PropertySpecificKeyframes.
class CORE_EXPORT Interpolation : public RefCounted<Interpolation> {
    WTF_MAKE_NONCOPYABLE(Interpolation);
public:
    virtual ~Interpolation();

    virtual void interpolate(int iteration, double fraction);

    virtual bool isStyleInterpolation() const { return false; }
    virtual bool isInvalidatableInterpolation() const { return false; }
    virtual bool isLegacyStyleInterpolation() const { return false; }

    virtual PropertyHandle getProperty() const = 0;
    virtual bool dependsOnUnderlyingValue() const { return false; }

protected:
    const std::unique_ptr<InterpolableValue> m_start;
    const std::unique_ptr<InterpolableValue> m_end;

    mutable double m_cachedFraction;
    mutable int m_cachedIteration;
    mutable std::unique_ptr<InterpolableValue> m_cachedValue;

    Interpolation(std::unique_ptr<InterpolableValue> start, std::unique_ptr<InterpolableValue> end);

private:
    InterpolableValue* getCachedValueForTesting() const { return m_cachedValue.get(); }

    friend class AnimationInterpolableValueTest;
    friend class AnimationInterpolationEffectTest;
    friend class AnimationDoubleStyleInterpolationTest;
    friend class AnimationVisibilityStyleInterpolationTest;
};

using ActiveInterpolations = Vector<RefPtr<Interpolation>, 1>;

} // namespace blink

#endif // Interpolation_h
