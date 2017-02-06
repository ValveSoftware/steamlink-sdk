/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CSSBasicShapeValues_h
#define CSSBasicShapeValues_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValuePair.h"
#include "platform/graphics/GraphicsTypes.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSBasicShapeCircleValue final : public CSSValue {
public:
    static CSSBasicShapeCircleValue* create() { return new CSSBasicShapeCircleValue; }

    String customCSSText() const;
    bool equals(const CSSBasicShapeCircleValue&) const;

    CSSValue* centerX() const { return m_centerX.get(); }
    CSSValue* centerY() const { return m_centerY.get(); }
    CSSPrimitiveValue* radius() const { return m_radius.get(); }

    // TODO(sashab): Remove these and pass them as arguments in the constructor.
    void setCenterX(CSSValue* centerX) { m_centerX = centerX; }
    void setCenterY(CSSValue* centerY) { m_centerY = centerY; }
    void setRadius(CSSPrimitiveValue* radius) { m_radius = radius; }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSBasicShapeCircleValue()
        : CSSValue(BasicShapeCircleClass)
        { }

    Member<CSSValue> m_centerX;
    Member<CSSValue> m_centerY;
    Member<CSSPrimitiveValue> m_radius;
};

class CSSBasicShapeEllipseValue final : public CSSValue {
public:
    static CSSBasicShapeEllipseValue* create() { return new CSSBasicShapeEllipseValue; }

    String customCSSText() const;
    bool equals(const CSSBasicShapeEllipseValue&) const;

    CSSValue* centerX() const { return m_centerX.get(); }
    CSSValue* centerY() const { return m_centerY.get(); }
    CSSPrimitiveValue* radiusX() const { return m_radiusX.get(); }
    CSSPrimitiveValue* radiusY() const { return m_radiusY.get(); }

    // TODO(sashab): Remove these and pass them as arguments in the constructor.
    void setCenterX(CSSValue* centerX) { m_centerX = centerX; }
    void setCenterY(CSSValue* centerY) { m_centerY = centerY; }
    void setRadiusX(CSSPrimitiveValue* radiusX) { m_radiusX = radiusX; }
    void setRadiusY(CSSPrimitiveValue* radiusY) { m_radiusY = radiusY; }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSBasicShapeEllipseValue()
        : CSSValue(BasicShapeEllipseClass)
        { }

    Member<CSSValue> m_centerX;
    Member<CSSValue> m_centerY;
    Member<CSSPrimitiveValue> m_radiusX;
    Member<CSSPrimitiveValue> m_radiusY;
};

class CSSBasicShapePolygonValue final : public CSSValue {
public:
    static CSSBasicShapePolygonValue* create() { return new CSSBasicShapePolygonValue; }

    void appendPoint(CSSPrimitiveValue* x, CSSPrimitiveValue* y)
    {
        m_values.append(x);
        m_values.append(y);
    }

    CSSPrimitiveValue* getXAt(unsigned i) const { return m_values.at(i * 2); }
    CSSPrimitiveValue* getYAt(unsigned i) const { return m_values.at(i * 2 + 1); }
    const HeapVector<Member<CSSPrimitiveValue>>& values() const { return m_values; }

    // TODO(sashab): Remove this and pass it as an argument in the constructor.
    void setWindRule(WindRule w) { m_windRule = w; }
    WindRule getWindRule() const { return m_windRule; }

    String customCSSText() const;
    bool equals(const CSSBasicShapePolygonValue&) const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSBasicShapePolygonValue()
        : CSSValue(BasicShapePolygonClass),
        m_windRule(RULE_NONZERO)
    { }

    HeapVector<Member<CSSPrimitiveValue>> m_values;
    WindRule m_windRule;
};

class CSSBasicShapeInsetValue final : public CSSValue {
public:
    static CSSBasicShapeInsetValue* create() { return new CSSBasicShapeInsetValue; }

    CSSPrimitiveValue* top() const { return m_top.get(); }
    CSSPrimitiveValue* right() const { return m_right.get(); }
    CSSPrimitiveValue* bottom() const { return m_bottom.get(); }
    CSSPrimitiveValue* left() const { return m_left.get(); }

    CSSValuePair* topLeftRadius() const { return m_topLeftRadius.get(); }
    CSSValuePair* topRightRadius() const { return m_topRightRadius.get(); }
    CSSValuePair* bottomRightRadius() const { return m_bottomRightRadius.get(); }
    CSSValuePair* bottomLeftRadius() const { return m_bottomLeftRadius.get(); }

    // TODO(sashab): Remove these and pass them as arguments in the constructor.
    void setTop(CSSPrimitiveValue* top) { m_top = top; }
    void setRight(CSSPrimitiveValue* right) { m_right = right; }
    void setBottom(CSSPrimitiveValue* bottom) { m_bottom = bottom; }
    void setLeft(CSSPrimitiveValue* left) { m_left = left; }

    void updateShapeSize4Values(CSSPrimitiveValue* top, CSSPrimitiveValue* right, CSSPrimitiveValue* bottom, CSSPrimitiveValue* left)
    {
        setTop(top);
        setRight(right);
        setBottom(bottom);
        setLeft(left);
    }

    void updateShapeSize1Value(CSSPrimitiveValue* value1)
    {
        updateShapeSize4Values(value1, value1, value1, value1);
    }

    void updateShapeSize2Values(CSSPrimitiveValue* value1,  CSSPrimitiveValue* value2)
    {
        updateShapeSize4Values(value1, value2, value1, value2);
    }

    void updateShapeSize3Values(CSSPrimitiveValue* value1, CSSPrimitiveValue* value2,  CSSPrimitiveValue* value3)
    {
        updateShapeSize4Values(value1, value2, value3, value2);
    }

    void setTopLeftRadius(CSSValuePair* radius) { m_topLeftRadius = radius; }
    void setTopRightRadius(CSSValuePair* radius) { m_topRightRadius = radius; }
    void setBottomRightRadius(CSSValuePair* radius) { m_bottomRightRadius = radius; }
    void setBottomLeftRadius(CSSValuePair* radius) { m_bottomLeftRadius = radius; }

    String customCSSText() const;
    bool equals(const CSSBasicShapeInsetValue&) const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSBasicShapeInsetValue()
        : CSSValue(BasicShapeInsetClass)
        { }

    Member<CSSPrimitiveValue> m_top;
    Member<CSSPrimitiveValue> m_right;
    Member<CSSPrimitiveValue> m_bottom;
    Member<CSSPrimitiveValue> m_left;

    Member<CSSValuePair> m_topLeftRadius;
    Member<CSSValuePair> m_topRightRadius;
    Member<CSSValuePair> m_bottomRightRadius;
    Member<CSSValuePair> m_bottomLeftRadius;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSBasicShapeCircleValue, isBasicShapeCircleValue());
DEFINE_CSS_VALUE_TYPE_CASTS(CSSBasicShapeEllipseValue, isBasicShapeEllipseValue());
DEFINE_CSS_VALUE_TYPE_CASTS(CSSBasicShapePolygonValue, isBasicShapePolygonValue());
DEFINE_CSS_VALUE_TYPE_CASTS(CSSBasicShapeInsetValue, isBasicShapeInsetValue());

} // namespace blink

#endif // CSSBasicShapeValues_h
