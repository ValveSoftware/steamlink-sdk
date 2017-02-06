// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimatableDoubleAndBool_h
#define AnimatableDoubleAndBool_h

#include "core/CoreExport.h"
#include "core/animation/animatable/AnimatableValue.h"

namespace blink {

class CORE_EXPORT AnimatableDoubleAndBool final : public AnimatableValue {
public:
    ~AnimatableDoubleAndBool() override { }
    static PassRefPtr<AnimatableDoubleAndBool> create(double number, bool flag)
    {
        return adoptRef(new AnimatableDoubleAndBool(number, flag));
    }

    double toDouble() const { return m_number; }
    bool flag() const { return m_flag; }

protected:
    PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const override;
    bool usesDefaultInterpolationWith(const AnimatableValue*) const override;

private:
    AnimatableDoubleAndBool(double number, bool flag)
        : m_number(number)
        , m_flag(flag)
    {
    }
    AnimatableType type() const override { return TypeDoubleAndBool; }
    bool equalTo(const AnimatableValue*) const override;

    double m_number;
    bool m_flag;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableDoubleAndBool, isDoubleAndBool());

} // namespace blink

#endif // AnimatableDoubleAndBool_h
