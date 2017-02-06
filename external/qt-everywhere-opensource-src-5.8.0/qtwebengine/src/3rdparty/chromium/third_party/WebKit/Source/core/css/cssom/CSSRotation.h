// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSRotation_h
#define CSSRotation_h

#include "core/css/cssom/CSSAngleValue.h"
#include "core/css/cssom/CSSMatrixTransformComponent.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CORE_EXPORT CSSRotation final : public CSSTransformComponent {
    WTF_MAKE_NONCOPYABLE(CSSRotation);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSRotation* create(double angle)
    {
        return new CSSRotation(angle);
    }

    static CSSRotation* create(const CSSAngleValue* angleValue)
    {
        return new CSSRotation(angleValue->degrees());
    }

    static CSSRotation* create(double x, double y, double z, double angle)
    {
        return new CSSRotation(x, y, z, angle);
    }

    static CSSRotation* create(double x, double y, double z, const CSSAngleValue* angleValue)
    {
        return new CSSRotation(x, y, z, angleValue->degrees());
    }

    static CSSRotation* fromCSSValue(const CSSFunctionValue&);

    double angle() const { return m_angle; }
    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

    TransformComponentType type() const override { return m_is2D ? RotationType : Rotation3DType; }

    CSSMatrixTransformComponent* asMatrix() const override
    {
        return m_is2D ? CSSMatrixTransformComponent::rotate(m_angle)
            : CSSMatrixTransformComponent::rotate3d(m_angle, m_x, m_y, m_z);
    }

    CSSFunctionValue* toCSSValue() const override;

private:
    CSSRotation(double angle)
        : m_x(0)
        , m_y(0)
        , m_z(1)
        , m_angle(angle)
        , m_is2D(true)
    { }

    CSSRotation(double x, double y, double z, double angle)
        : m_x(x)
        , m_y(y)
        , m_z(z)
        , m_angle(angle)
        , m_is2D(false)
    { }

    double m_x;
    double m_y;
    double m_z;
    double m_angle;
    bool m_is2D;
};

} // namespace blink

#endif
