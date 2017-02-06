// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkew_h
#define CSSSkew_h

#include "core/css/cssom/CSSMatrixTransformComponent.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class CORE_EXPORT CSSSkew final : public CSSTransformComponent {
    WTF_MAKE_NONCOPYABLE(CSSSkew);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSSkew* create(double ax, double ay)
    {
        return new CSSSkew(ax, ay);
    }

    static CSSSkew* fromCSSValue(const CSSFunctionValue& value) { return nullptr; }

    double ax() const { return m_ax; }
    double ay() const { return m_ay; }

    TransformComponentType type() const override { return SkewType; }

    CSSMatrixTransformComponent* asMatrix() const override
    {
        return CSSMatrixTransformComponent::skew(m_ax, m_ay);
    }

    CSSFunctionValue* toCSSValue() const override;

private:
    CSSSkew(double ax, double ay) : CSSTransformComponent(), m_ax(ax), m_ay(ay) { }

    double m_ax;
    double m_ay;
};

} // namespace blink

#endif
