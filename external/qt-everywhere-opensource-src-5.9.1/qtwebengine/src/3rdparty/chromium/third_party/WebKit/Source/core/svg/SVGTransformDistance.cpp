/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "core/svg/SVGTransformDistance.h"

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include <math.h>

namespace blink {

SVGTransformDistance::SVGTransformDistance()
    : m_transformType(kSvgTransformUnknown), m_angle(0), m_cx(0), m_cy(0) {}

SVGTransformDistance::SVGTransformDistance(SVGTransformType transformType,
                                           float angle,
                                           float cx,
                                           float cy,
                                           const AffineTransform& transform)
    : m_transformType(transformType),
      m_angle(angle),
      m_cx(cx),
      m_cy(cy),
      m_transform(transform) {}

SVGTransformDistance::SVGTransformDistance(SVGTransform* fromSVGTransform,
                                           SVGTransform* toSVGTransform)
    : m_angle(0), m_cx(0), m_cy(0) {
  m_transformType = fromSVGTransform->transformType();
  ASSERT(m_transformType == toSVGTransform->transformType());

  switch (m_transformType) {
    case kSvgTransformMatrix:
      ASSERT_NOT_REACHED();
    case kSvgTransformUnknown:
      break;
    case kSvgTransformRotate: {
      FloatSize centerDistance =
          toSVGTransform->rotationCenter() - fromSVGTransform->rotationCenter();
      m_angle = toSVGTransform->angle() - fromSVGTransform->angle();
      m_cx = centerDistance.width();
      m_cy = centerDistance.height();
      break;
    }
    case kSvgTransformTranslate: {
      FloatSize translationDistance =
          toSVGTransform->translate() - fromSVGTransform->translate();
      m_transform.translate(translationDistance.width(),
                            translationDistance.height());
      break;
    }
    case kSvgTransformScale: {
      float scaleX =
          toSVGTransform->scale().width() - fromSVGTransform->scale().width();
      float scaleY =
          toSVGTransform->scale().height() - fromSVGTransform->scale().height();
      m_transform.scaleNonUniform(scaleX, scaleY);
      break;
    }
    case kSvgTransformSkewx:
    case kSvgTransformSkewy:
      m_angle = toSVGTransform->angle() - fromSVGTransform->angle();
      break;
  }
}

SVGTransformDistance SVGTransformDistance::scaledDistance(
    float scaleFactor) const {
  switch (m_transformType) {
    case kSvgTransformMatrix:
      ASSERT_NOT_REACHED();
    case kSvgTransformUnknown:
      return SVGTransformDistance();
    case kSvgTransformRotate:
      return SVGTransformDistance(m_transformType, m_angle * scaleFactor,
                                  m_cx * scaleFactor, m_cy * scaleFactor,
                                  AffineTransform());
    case kSvgTransformScale:
      return SVGTransformDistance(
          m_transformType, m_angle * scaleFactor, m_cx * scaleFactor,
          m_cy * scaleFactor, AffineTransform(m_transform).scale(scaleFactor));
    case kSvgTransformTranslate: {
      AffineTransform newTransform(m_transform);
      newTransform.setE(m_transform.e() * scaleFactor);
      newTransform.setF(m_transform.f() * scaleFactor);
      return SVGTransformDistance(m_transformType, 0, 0, 0, newTransform);
    }
    case kSvgTransformSkewx:
    case kSvgTransformSkewy:
      return SVGTransformDistance(m_transformType, m_angle * scaleFactor,
                                  m_cx * scaleFactor, m_cy * scaleFactor,
                                  AffineTransform());
  }

  ASSERT_NOT_REACHED();
  return SVGTransformDistance();
}

SVGTransform* SVGTransformDistance::addSVGTransforms(SVGTransform* first,
                                                     SVGTransform* second,
                                                     unsigned repeatCount) {
  ASSERT(first->transformType() == second->transformType());

  SVGTransform* transform = SVGTransform::create();

  switch (first->transformType()) {
    case kSvgTransformMatrix:
      ASSERT_NOT_REACHED();
    case kSvgTransformUnknown:
      return transform;
    case kSvgTransformRotate: {
      transform->setRotate(first->angle() + second->angle() * repeatCount,
                           first->rotationCenter().x() +
                               second->rotationCenter().x() * repeatCount,
                           first->rotationCenter().y() +
                               second->rotationCenter().y() * repeatCount);
      return transform;
    }
    case kSvgTransformTranslate: {
      float dx = first->translate().x() + second->translate().x() * repeatCount;
      float dy = first->translate().y() + second->translate().y() * repeatCount;
      transform->setTranslate(dx, dy);
      return transform;
    }
    case kSvgTransformScale: {
      FloatSize scale = second->scale();
      scale.scale(repeatCount);
      scale += first->scale();
      transform->setScale(scale.width(), scale.height());
      return transform;
    }
    case kSvgTransformSkewx:
      transform->setSkewX(first->angle() + second->angle() * repeatCount);
      return transform;
    case kSvgTransformSkewy:
      transform->setSkewY(first->angle() + second->angle() * repeatCount);
      return transform;
  }
  ASSERT_NOT_REACHED();
  return transform;
}

SVGTransform* SVGTransformDistance::addToSVGTransform(
    SVGTransform* transform) const {
  DCHECK(m_transformType == transform->transformType() ||
         m_transformType == kSvgTransformUnknown);

  SVGTransform* newTransform = transform->clone();

  switch (m_transformType) {
    case kSvgTransformMatrix:
      ASSERT_NOT_REACHED();
    case kSvgTransformUnknown:
      return SVGTransform::create();
    case kSvgTransformTranslate: {
      FloatPoint translation = transform->translate();
      translation +=
          FloatSize::narrowPrecision(m_transform.e(), m_transform.f());
      newTransform->setTranslate(translation.x(), translation.y());
      return newTransform;
    }
    case kSvgTransformScale: {
      FloatSize scale = transform->scale();
      scale += FloatSize::narrowPrecision(m_transform.a(), m_transform.d());
      newTransform->setScale(scale.width(), scale.height());
      return newTransform;
    }
    case kSvgTransformRotate: {
      FloatPoint center = transform->rotationCenter();
      newTransform->setRotate(transform->angle() + m_angle, center.x() + m_cx,
                              center.y() + m_cy);
      return newTransform;
    }
    case kSvgTransformSkewx:
      newTransform->setSkewX(transform->angle() + m_angle);
      return newTransform;
    case kSvgTransformSkewy:
      newTransform->setSkewY(transform->angle() + m_angle);
      return newTransform;
  }

  ASSERT_NOT_REACHED();
  return newTransform;
}

float SVGTransformDistance::distance() const {
  switch (m_transformType) {
    case kSvgTransformMatrix:
      ASSERT_NOT_REACHED();
    case kSvgTransformUnknown:
      return 0;
    case kSvgTransformRotate:
      return sqrtf(m_angle * m_angle + m_cx * m_cx + m_cy * m_cy);
    case kSvgTransformScale:
      return static_cast<float>(sqrt(m_transform.a() * m_transform.a() +
                                     m_transform.d() * m_transform.d()));
    case kSvgTransformTranslate:
      return static_cast<float>(sqrt(m_transform.e() * m_transform.e() +
                                     m_transform.f() * m_transform.f()));
    case kSvgTransformSkewx:
    case kSvgTransformSkewy:
      return m_angle;
  }
  ASSERT_NOT_REACHED();
  return 0;
}

}  // namespace blink
