// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/math_util.h"

#include <cmath>

#include "cc/test/geometry_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

TEST(MathUtilTest, ProjectionOfPerpendicularPlane) {
  // In this case, the m33() element of the transform becomes zero, which could
  // cause a divide-by-zero when projecting points/quads.

  gfx::Transform transform;
  transform.MakeIdentity();
  transform.matrix().set(2, 2, 0);

  gfx::RectF rect = gfx::RectF(0, 0, 1, 1);
  gfx::RectF projected_rect = MathUtil::ProjectClippedRect(transform, rect);

  EXPECT_EQ(0, projected_rect.x());
  EXPECT_EQ(0, projected_rect.y());
  EXPECT_TRUE(projected_rect.IsEmpty());
}

TEST(MathUtilTest, EnclosingClippedRectUsesCorrectInitialBounds) {
  HomogeneousCoordinate h1(-100, -100, 0, 1);
  HomogeneousCoordinate h2(-10, -10, 0, 1);
  HomogeneousCoordinate h3(10, 10, 0, -1);
  HomogeneousCoordinate h4(100, 100, 0, -1);

  // The bounds of the enclosing clipped rect should be -100 to -10 for both x
  // and y. However, if there is a bug where the initial xmin/xmax/ymin/ymax are
  // initialized to numeric_limits<float>::min() (which is zero, not -flt_max)
  // then the enclosing clipped rect will be computed incorrectly.
  gfx::RectF result = MathUtil::ComputeEnclosingClippedRect(h1, h2, h3, h4);

  // Due to floating point math in ComputeClippedPointForEdge this result
  // is fairly imprecise.  0.15f was empirically determined.
  EXPECT_RECT_NEAR(
      gfx::RectF(gfx::PointF(-100, -100), gfx::SizeF(90, 90)), result, 0.15f);
}

TEST(MathUtilTest, EnclosingRectOfVerticesUsesCorrectInitialBounds) {
  gfx::PointF vertices[3];
  int num_vertices = 3;

  vertices[0] = gfx::PointF(-10, -100);
  vertices[1] = gfx::PointF(-100, -10);
  vertices[2] = gfx::PointF(-30, -30);

  // The bounds of the enclosing rect should be -100 to -10 for both x and y.
  // However, if there is a bug where the initial xmin/xmax/ymin/ymax are
  // initialized to numeric_limits<float>::min() (which is zero, not -flt_max)
  // then the enclosing clipped rect will be computed incorrectly.
  gfx::RectF result =
      MathUtil::ComputeEnclosingRectOfVertices(vertices, num_vertices);

  EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(-100, -100), gfx::SizeF(90, 90)),
                       result);
}

TEST(MathUtilTest, SmallestAngleBetweenVectors) {
  gfx::Vector2dF x(1, 0);
  gfx::Vector2dF y(0, 1);
  gfx::Vector2dF test_vector(0.5, 0.5);

  // Orthogonal vectors are at an angle of 90 degress.
  EXPECT_EQ(90, MathUtil::SmallestAngleBetweenVectors(x, y));

  // A vector makes a zero angle with itself.
  EXPECT_EQ(0, MathUtil::SmallestAngleBetweenVectors(x, x));
  EXPECT_EQ(0, MathUtil::SmallestAngleBetweenVectors(y, y));
  EXPECT_EQ(0, MathUtil::SmallestAngleBetweenVectors(test_vector, test_vector));

  // Parallel but reversed vectors are at 180 degrees.
  EXPECT_FLOAT_EQ(180, MathUtil::SmallestAngleBetweenVectors(x, -x));
  EXPECT_FLOAT_EQ(180, MathUtil::SmallestAngleBetweenVectors(y, -y));
  EXPECT_FLOAT_EQ(
      180, MathUtil::SmallestAngleBetweenVectors(test_vector, -test_vector));

  // The test vector is at a known angle.
  EXPECT_FLOAT_EQ(
      45, std::floor(MathUtil::SmallestAngleBetweenVectors(test_vector, x)));
  EXPECT_FLOAT_EQ(
      45, std::floor(MathUtil::SmallestAngleBetweenVectors(test_vector, y)));
}

TEST(MathUtilTest, VectorProjection) {
  gfx::Vector2dF x(1, 0);
  gfx::Vector2dF y(0, 1);
  gfx::Vector2dF test_vector(0.3f, 0.7f);

  // Orthogonal vectors project to a zero vector.
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), MathUtil::ProjectVector(x, y));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), MathUtil::ProjectVector(y, x));

  // Projecting a vector onto the orthonormal basis gives the corresponding
  // component of the vector.
  EXPECT_VECTOR_EQ(gfx::Vector2dF(test_vector.x(), 0),
                   MathUtil::ProjectVector(test_vector, x));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, test_vector.y()),
                   MathUtil::ProjectVector(test_vector, y));

  // Finally check than an arbitrary vector projected to another one gives a
  // vector parallel to the second vector.
  gfx::Vector2dF target_vector(0.5, 0.2f);
  gfx::Vector2dF projected_vector =
      MathUtil::ProjectVector(test_vector, target_vector);
  EXPECT_EQ(projected_vector.x() / target_vector.x(),
            projected_vector.y() / target_vector.y());
}

}  // namespace
}  // namespace cc
