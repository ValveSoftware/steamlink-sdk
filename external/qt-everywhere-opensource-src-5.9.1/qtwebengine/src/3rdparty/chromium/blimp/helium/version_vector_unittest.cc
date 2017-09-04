// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/macros.h"
#include "blimp/helium/version_vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace helium {
namespace {

class VersionVectorComparisonTest
    : public ::testing::TestWithParam<
          std::tuple<VersionVector, VersionVector, VersionVector::Comparison>> {
 public:
  VersionVectorComparisonTest() {}
  ~VersionVectorComparisonTest() override {}
};

TEST_P(VersionVectorComparisonTest, CompareTo) {
  auto param = GetParam();
  VersionVector v1 = std::get<0>(param);
  VersionVector v2 = std::get<1>(param);
  VersionVector::Comparison expected = std::get<2>(param);
  EXPECT_EQ(expected, v1.CompareTo(v2));
}

INSTANTIATE_TEST_CASE_P(
    LessThan,
    VersionVectorComparisonTest,
    ::testing::Values(std::make_tuple(VersionVector(1, 2),
                                      VersionVector(1, 3),
                                      VersionVector::Comparison::LessThan)));

INSTANTIATE_TEST_CASE_P(
    GreaterThan,
    VersionVectorComparisonTest,
    ::testing::Values(std::make_tuple(VersionVector(1, 3),
                                      VersionVector(1, 2),
                                      VersionVector::Comparison::GreaterThan),
                      std::make_tuple(VersionVector(2, 2),
                                      VersionVector(1, 2),
                                      VersionVector::Comparison::GreaterThan)));

INSTANTIATE_TEST_CASE_P(
    Conflict,
    VersionVectorComparisonTest,
    ::testing::Values(std::make_tuple(VersionVector(1, 2),
                                      VersionVector(0, 1),
                                      VersionVector::Comparison::Conflict),
                      std::make_tuple(VersionVector(1, 2),
                                      VersionVector(0, 3),
                                      VersionVector::Comparison::Conflict)));

INSTANTIATE_TEST_CASE_P(
    EqualTo,
    VersionVectorComparisonTest,
    ::testing::Values(std::make_tuple(VersionVector(1, 1),
                                      VersionVector(1, 1),
                                      VersionVector::Comparison::EqualTo),
                      std::make_tuple(VersionVector(2, 3),
                                      VersionVector(2, 3),
                                      VersionVector::Comparison::EqualTo),
                      std::make_tuple(VersionVector(3, 2),
                                      VersionVector(3, 2),
                                      VersionVector::Comparison::EqualTo)));

class VersionVectorTest : public testing::Test {
 public:
  VersionVectorTest() {}
  ~VersionVectorTest() override {}

 protected:
  void CheckCumulativeMerge(const VersionVector& v1,
                            const VersionVector& v2,
                            const VersionVector& expected) {
    // Compute the merge of v1 and v2
    VersionVector r1 = v1.MergeWith(v2);
    EXPECT_EQ(expected.local_revision(), r1.local_revision());
    EXPECT_EQ(expected.remote_revision(), r1.remote_revision());

    // Compute the merge of v2 and v1
    VersionVector r2 = v2.MergeWith(v1);
    EXPECT_EQ(expected.local_revision(), r2.local_revision());
    EXPECT_EQ(expected.remote_revision(), r2.remote_revision());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionVectorTest);
};

TEST_F(VersionVectorTest, IncrementLocal1) {
  VersionVector v(0, 0);
  v.IncrementLocal();
  EXPECT_EQ(1U, v.local_revision());
  EXPECT_EQ(0U, v.remote_revision());
}

TEST_F(VersionVectorTest, IncrementLocal2) {
  VersionVector v(4, 5);
  v.IncrementLocal();
  EXPECT_EQ(5U, v.local_revision());
  EXPECT_EQ(5U, v.remote_revision());
}

TEST_F(VersionVectorTest, MergeLocalEqualRemoteSmaller) {
  VersionVector v1(1, 2);
  VersionVector v2(1, 4);

  VersionVector expected(1, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

TEST_F(VersionVectorTest, MergeLocalSmallerRemoteEqual) {
  VersionVector v1(1, 4);
  VersionVector v2(2, 4);

  VersionVector expected(2, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

TEST_F(VersionVectorTest, MergeLocalSmallerRemoteSmaller) {
  VersionVector v1(1, 2);
  VersionVector v2(3, 4);

  VersionVector expected(3, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

TEST_F(VersionVectorTest, MergeLocalSmallerRemoteGreater) {
  VersionVector v1(1, 4);
  VersionVector v2(3, 2);

  VersionVector expected(3, 4);
  CheckCumulativeMerge(v1, v2, expected);
}

}  // namespace
}  // namespace helium
}  // namespace blimp
