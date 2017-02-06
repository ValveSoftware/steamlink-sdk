// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSScale_h
#define CSSScale_h

#include "core/css/cssom/CSSMatrixTransformComponent.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CORE_EXPORT CSSScale final : public CSSTransformComponent {
    WTF_MAKE_NONCOPYABLE(CSSScale);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSScale* create(double x, double y)
    {
        return new CSSScale(x, y);
    }

    static CSSScale* create(double x, double y, double z)
    {
        return new CSSScale(x, y, z);
    }

    static CSSScale* fromCSSValue(const CSSFunctionValue& value) { return nullptr; }

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

    TransformComponentType type() const override { return m_is2D ? ScaleType : Scale3DType; }

    CSSMatrixTransformComponent* asMatrix() const override
    {
        return m_is2D ? CSSMatrixTransformComponent::scale(m_x, m_y)
            : CSSMatrixTransformComponent::scale3d(m_x, m_y, m_z);
    }

    CSSFunctionValue* toCSSValue() const override;

private:
    CSSScale(double x, double y) : m_x(x), m_y(y), m_z(1), m_is2D(true) { }
    CSSScale(double x, double y, double z) : m_x(x), m_y(y), m_z(z), m_is2D(false) { }

    double m_x;
    double m_y;
    double m_z;
    bool m_is2D;
};

} // namespace blink

#endif
