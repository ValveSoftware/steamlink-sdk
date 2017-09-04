// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/transforms/TransformationMatrix.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(TransformationMatrixTest, NonInvertableBlendTest) {
  TransformationMatrix from;
  TransformationMatrix to(2.7133590938, 0.0, 0.0, 0.0, 0.0, 2.4645137761, 0.0,
                          0.0, 0.0, 0.0, 0.00, 0.01, 0.02, 0.03, 0.04, 0.05);
  TransformationMatrix result;

  result = to;
  result.blend(from, 0.25);
  EXPECT_EQ(result, from);

  result = to;
  result.blend(from, 0.75);
  EXPECT_EQ(result, to);
}

TEST(TransformationMatrixTest, IsIdentityOr2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_TRUE(matrix.isIdentityOr2DTranslation());

  matrix.makeIdentity();
  matrix.translate(10, 0);
  EXPECT_TRUE(matrix.isIdentityOr2DTranslation());

  matrix.makeIdentity();
  matrix.translate(0, -20);
  EXPECT_TRUE(matrix.isIdentityOr2DTranslation());

  matrix.makeIdentity();
  matrix.translate3d(0, 0, 1);
  EXPECT_FALSE(matrix.isIdentityOr2DTranslation());

  matrix.makeIdentity();
  matrix.rotate(40 /* degrees */);
  EXPECT_FALSE(matrix.isIdentityOr2DTranslation());

  matrix.makeIdentity();
  matrix.skewX(30 /* degrees */);
  EXPECT_FALSE(matrix.isIdentityOr2DTranslation());
}

TEST(TransformationMatrixTest, To2DTranslation) {
  TransformationMatrix matrix;
  EXPECT_EQ(FloatSize(), matrix.to2DTranslation());
  matrix.translate(30, -40);
  EXPECT_EQ(FloatSize(30, -40), matrix.to2DTranslation());
}

TEST(TransformationMatrixTest, ApplyTransformOrigin) {
  TransformationMatrix matrix;

  // (0,0,0) is a fixed point of this scale.
  // (1,1,1) should be scaled appropriately.
  matrix.scale3d(2, 3, 4);
  EXPECT_EQ(FloatPoint3D(0, 0, 0), matrix.mapPoint(FloatPoint3D(0, 0, 0)));
  EXPECT_EQ(FloatPoint3D(2, 3, -4), matrix.mapPoint(FloatPoint3D(1, 1, -1)));

  // With the transform origin applied, (1,2,3) is the fixed point.
  // (0,0,0) should be scaled according to its distance from (1,2,3).
  matrix.applyTransformOrigin(1, 2, 3);
  EXPECT_EQ(FloatPoint3D(1, 2, 3), matrix.mapPoint(FloatPoint3D(1, 2, 3)));
  EXPECT_EQ(FloatPoint3D(-1, -4, -9), matrix.mapPoint(FloatPoint3D(0, 0, 0)));
}

TEST(TransformationMatrixTest, Multiplication) {
  TransformationMatrix a(1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4);
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]

  TransformationMatrix b(1, 2, 3, 5, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4);
  // [ 1 1 1 1 ]
  // [ 2 2 2 2 ]
  // [ 3 3 3 3 ]
  // [ 5 4 4 4 ]

  TransformationMatrix expectedAtimesB(34, 34, 34, 34, 30, 30, 30, 30, 30, 30,
                                       30, 30, 30, 30, 30, 30);

  EXPECT_EQ(expectedAtimesB, a * b);

  a.multiply(b);
  EXPECT_EQ(expectedAtimesB, a);
}

TEST(TransformationMatrixTest, ToString) {
  TransformationMatrix zeros(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("[0,0,0,0,\n0,0,0,0,\n0,0,0,0,\n0,0,0,0] (degenerate)",
            zeros.toString());
  EXPECT_EQ("[0,0,0,0,\n0,0,0,0,\n0,0,0,0,\n0,0,0,0]", zeros.toString(true));

  TransformationMatrix identity;
  EXPECT_EQ("identity", identity.toString());
  EXPECT_EQ("[1,0,0,0,\n0,1,0,0,\n0,0,1,0,\n0,0,0,1]", identity.toString(true));

  TransformationMatrix translation;
  translation.translate3d(3, 5, 7);
  EXPECT_EQ("translation(3,5,7)", translation.toString());
  EXPECT_EQ("[1,0,0,3,\n0,1,0,5,\n0,0,1,7,\n0,0,0,1]",
            translation.toString(true));

  TransformationMatrix rotation;
  rotation.rotate(180);
  EXPECT_EQ(
      "translation(0,0,0), scale(1,1,1), skew(0,0,0), "
      "quaternion(0,0,1,-6.12323e-17), perspective(0,0,0,1)",
      rotation.toString());
  EXPECT_EQ("[-1,-1.22465e-16,0,0,\n1.22465e-16,-1,0,0,\n0,0,1,0,\n0,0,0,1]",
            rotation.toString(true));

  TransformationMatrix columnMajorConstructor(1, 1, 1, 6, 2, 2, 0, 7, 3, 3, 3,
                                              8, 4, 4, 4, 9);
  // [ 1 2 3 4 ]
  // [ 1 2 3 4 ]
  // [ 1 0 3 4 ]
  // [ 6 7 8 9 ]
  EXPECT_EQ("[1,2,3,4,\n1,2,3,4,\n1,0,3,4,\n6,7,8,9]",
            columnMajorConstructor.toString(true));
}

}  // namespace blink
