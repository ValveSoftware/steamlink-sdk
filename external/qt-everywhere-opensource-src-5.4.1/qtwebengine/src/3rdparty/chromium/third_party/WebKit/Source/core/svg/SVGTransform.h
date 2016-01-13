/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGTransform_h
#define SVGTransform_h

#include "core/svg/properties/SVGProperty.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class FloatSize;
class SVGTransformTearOff;

enum SVGTransformType {
    SVG_TRANSFORM_UNKNOWN = 0,
    SVG_TRANSFORM_MATRIX = 1,
    SVG_TRANSFORM_TRANSLATE = 2,
    SVG_TRANSFORM_SCALE = 3,
    SVG_TRANSFORM_ROTATE = 4,
    SVG_TRANSFORM_SKEWX = 5,
    SVG_TRANSFORM_SKEWY = 6
};

class SVGTransform : public SVGPropertyBase {
public:
    typedef SVGTransformTearOff TearOffType;

    enum ConstructionMode {
        ConstructIdentityTransform,
        ConstructZeroTransform
    };

    static PassRefPtr<SVGTransform> create()
    {
        return adoptRef(new SVGTransform());
    }

    static PassRefPtr<SVGTransform> create(SVGTransformType type, ConstructionMode mode = ConstructIdentityTransform)
    {
        return adoptRef(new SVGTransform(type, mode));
    }

    static PassRefPtr<SVGTransform> create(const AffineTransform& affineTransform)
    {
        return adoptRef(new SVGTransform(affineTransform));
    }

    virtual ~SVGTransform();

    PassRefPtr<SVGTransform> clone() const;
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;

    SVGTransformType transformType() const { return m_transformType; }

    const AffineTransform& matrix() const { return m_matrix; }

    // |onMatrixChange| must be called after modifications via |mutableMatrix|.
    AffineTransform* mutableMatrix() { return &m_matrix; }
    void onMatrixChange();

    float angle() const { return m_angle; }
    FloatPoint rotationCenter() const { return m_center; }

    void setMatrix(const AffineTransform&);
    void setTranslate(float tx, float ty);
    void setScale(float sx, float sy);
    void setRotate(float angle, float cx, float cy);
    void setSkewX(float angle);
    void setSkewY(float angle);

    // Internal use only (animation system)
    FloatPoint translate() const;
    FloatSize scale() const;

    virtual String valueAsString() const OVERRIDE;

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> from, PassRefPtr<SVGPropertyBase> to, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement* contextElement) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedTransform; }

private:
    SVGTransform();
    SVGTransform(SVGTransformType, ConstructionMode);
    explicit SVGTransform(const AffineTransform&);
    SVGTransform(SVGTransformType, float, const FloatPoint&, const AffineTransform&);

    friend bool operator==(const SVGTransform& a, const SVGTransform& b);

    SVGTransformType m_transformType;
    float m_angle;
    FloatPoint m_center;
    AffineTransform m_matrix;
};

inline bool operator==(const SVGTransform& a, const SVGTransform& b)
{
    return a.m_transformType == b.m_transformType && a.m_angle == b.m_angle && a.m_matrix == b.m_matrix;
}

inline bool operator!=(const SVGTransform& a, const SVGTransform& b)
{
    return !(a == b);
}

} // namespace WebCore

#endif
