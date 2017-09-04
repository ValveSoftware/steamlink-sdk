/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "platform/transforms/Rotation.h"

#include "platform/animation/AnimationUtilities.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

namespace {

const double kAngleEpsilon = 1e-4;

Rotation extractFromMatrix(const TransformationMatrix& matrix,
                           const Rotation& fallbackValue) {
  TransformationMatrix::DecomposedType decomp;
  if (!matrix.decompose(decomp))
    return fallbackValue;
  double x = -decomp.quaternionX;
  double y = -decomp.quaternionY;
  double z = -decomp.quaternionZ;
  double length = std::sqrt(x * x + y * y + z * z);
  double angle = 0;
  if (length > 0.00001) {
    x /= length;
    y /= length;
    z /= length;
    angle = rad2deg(std::acos(decomp.quaternionW) * 2);
  } else {
    x = 0;
    y = 0;
    z = 1;
  }
  return Rotation(FloatPoint3D(x, y, z), angle);
}

}  // namespace

bool Rotation::getCommonAxis(const Rotation& a,
                             const Rotation& b,
                             FloatPoint3D& resultAxis,
                             double& resultAngleA,
                             double& resultAngleB) {
  resultAxis = FloatPoint3D(0, 0, 1);
  resultAngleA = 0;
  resultAngleB = 0;

  bool isZeroA = a.axis.isZero() || fabs(a.angle) < kAngleEpsilon;
  bool isZeroB = b.axis.isZero() || fabs(b.angle) < kAngleEpsilon;

  if (isZeroA && isZeroB)
    return true;

  if (isZeroA) {
    resultAxis = b.axis;
    resultAngleB = b.angle;
    return true;
  }

  if (isZeroB) {
    resultAxis = a.axis;
    resultAngleA = a.angle;
    return true;
  }

  double dot = a.axis.dot(b.axis);
  if (dot < 0)
    return false;

  double aSquared = a.axis.lengthSquared();
  double bSquared = b.axis.lengthSquared();
  double error = std::abs(1 - (dot * dot) / (aSquared * bSquared));
  if (error > kAngleEpsilon)
    return false;

  resultAxis = a.axis;
  resultAngleA = a.angle;
  resultAngleB = b.angle;
  return true;
}

Rotation Rotation::slerp(const Rotation& from,
                         const Rotation& to,
                         double progress) {
  double fromAngle;
  double toAngle;
  FloatPoint3D axis;
  if (getCommonAxis(from, to, axis, fromAngle, toAngle))
    return Rotation(axis, blink::blend(fromAngle, toAngle, progress));

  TransformationMatrix fromMatrix;
  TransformationMatrix toMatrix;
  fromMatrix.rotate3d(from);
  toMatrix.rotate3d(to);
  toMatrix.blend(fromMatrix, progress);
  return extractFromMatrix(toMatrix, progress < 0.5 ? from : to);
}

Rotation Rotation::add(const Rotation& a, const Rotation& b) {
  double angleA;
  double angleB;
  FloatPoint3D axis;
  if (getCommonAxis(a, b, axis, angleA, angleB))
    return Rotation(axis, angleA + angleB);

  TransformationMatrix matrix;
  matrix.rotate3d(a);
  matrix.rotate3d(b);
  return extractFromMatrix(matrix, b);
}

}  // namespace blink
