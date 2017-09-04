/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#ifndef SVGPathData_h
#define SVGPathData_h

#include "platform/geometry/FloatPoint.h"
#include "wtf/Allocator.h"

namespace blink {

enum SVGPathSegType {
  PathSegUnknown = 0,
  PathSegClosePath = 1,
  PathSegMoveToAbs = 2,
  PathSegMoveToRel = 3,
  PathSegLineToAbs = 4,
  PathSegLineToRel = 5,
  PathSegCurveToCubicAbs = 6,
  PathSegCurveToCubicRel = 7,
  PathSegCurveToQuadraticAbs = 8,
  PathSegCurveToQuadraticRel = 9,
  PathSegArcAbs = 10,
  PathSegArcRel = 11,
  PathSegLineToHorizontalAbs = 12,
  PathSegLineToHorizontalRel = 13,
  PathSegLineToVerticalAbs = 14,
  PathSegLineToVerticalRel = 15,
  PathSegCurveToCubicSmoothAbs = 16,
  PathSegCurveToCubicSmoothRel = 17,
  PathSegCurveToQuadraticSmoothAbs = 18,
  PathSegCurveToQuadraticSmoothRel = 19
};

static inline SVGPathSegType toAbsolutePathSegType(const SVGPathSegType type) {
  // Clear the LSB to get the absolute command.
  return type >= PathSegMoveToAbs ? static_cast<SVGPathSegType>(type & ~1u)
                                  : type;
}

static inline bool isAbsolutePathSegType(const SVGPathSegType type) {
  // For commands with an ordinal >= PathSegMoveToAbs, and odd number =>
  // relative command.
  return type < PathSegMoveToAbs || type % 2 == 0;
}

struct PathSegmentData {
  STACK_ALLOCATED();
  PathSegmentData()
      : command(PathSegUnknown), arcSweep(false), arcLarge(false) {}

  const FloatPoint& arcRadii() const { return point1; }
  FloatPoint& arcRadii() { return point1; }

  float arcAngle() const { return point2.x(); }
  void setArcAngle(float angle) { point2.setX(angle); }

  float r1() const { return arcRadii().x(); }
  float r2() const { return arcRadii().y(); }

  bool largeArcFlag() const { return arcLarge; }
  bool sweepFlag() const { return arcSweep; }

  float x() const { return targetPoint.x(); }
  float y() const { return targetPoint.y(); }

  float x1() const { return point1.x(); }
  float y1() const { return point1.y(); }

  float x2() const { return point2.x(); }
  float y2() const { return point2.y(); }

  SVGPathSegType command;
  FloatPoint targetPoint;
  FloatPoint point1;
  FloatPoint point2;
  bool arcSweep;
  bool arcLarge;
};

}  // namespace blink

#endif  // SVGPathData_h
