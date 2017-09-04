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

namespace blink {

class FloatSize;
class SVGTransformTearOff;

enum SVGTransformType {
  kSvgTransformUnknown = 0,
  kSvgTransformMatrix = 1,
  kSvgTransformTranslate = 2,
  kSvgTransformScale = 3,
  kSvgTransformRotate = 4,
  kSvgTransformSkewx = 5,
  kSvgTransformSkewy = 6
};

class SVGTransform final : public SVGPropertyBase {
 public:
  typedef SVGTransformTearOff TearOffType;

  enum ConstructionMode { ConstructIdentityTransform, ConstructZeroTransform };

  static SVGTransform* create() { return new SVGTransform(); }

  static SVGTransform* create(
      SVGTransformType type,
      ConstructionMode mode = ConstructIdentityTransform) {
    return new SVGTransform(type, mode);
  }

  static SVGTransform* create(const AffineTransform& affineTransform) {
    return new SVGTransform(affineTransform);
  }

  ~SVGTransform() override;

  SVGTransform* clone() const;
  SVGPropertyBase* cloneForAnimation(const String&) const override;

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

  String valueAsString() const override;

  void add(SVGPropertyBase*, SVGElement*) override;
  void calculateAnimatedValue(SVGAnimationElement*,
                              float percentage,
                              unsigned repeatCount,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* toAtEndOfDurationValue,
                              SVGElement* contextElement) override;
  float calculateDistance(SVGPropertyBase* to,
                          SVGElement* contextElement) override;

  static AnimatedPropertyType classType() { return AnimatedTransform; }
  AnimatedPropertyType type() const override { return classType(); }

 private:
  SVGTransform();
  SVGTransform(SVGTransformType, ConstructionMode);
  explicit SVGTransform(const AffineTransform&);
  SVGTransform(SVGTransformType,
               float,
               const FloatPoint&,
               const AffineTransform&);

  SVGTransformType m_transformType;
  float m_angle;
  FloatPoint m_center;
  AffineTransform m_matrix;
};

}  // namespace blink

#endif  // SVGTransform_h
