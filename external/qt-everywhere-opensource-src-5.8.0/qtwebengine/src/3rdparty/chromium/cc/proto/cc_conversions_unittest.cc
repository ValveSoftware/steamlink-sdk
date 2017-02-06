// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/cc_conversions.h"

#include "cc/base/region.h"
#include "cc/proto/region.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
namespace {

void VerifySerializeAndDeserializeProto(const Region& region1) {
  proto::Region proto;
  RegionToProto(region1, &proto);
  Region region2 = RegionFromProto(proto);
  EXPECT_EQ(region1, region2);
}

TEST(RegionTest, SingleRectProtoConversion) {
  Region region(gfx::Rect(14, 15, 16, 17));
  VerifySerializeAndDeserializeProto(region);
}

TEST(RegionTest, MultipleRectProtoConversion) {
  Region region(gfx::Rect(0, 0, 1, 1));
  region.Union(gfx::Rect(9, 0, 1, 1));
  region.Union(gfx::Rect(0, 9, 1, 1));
  region.Union(gfx::Rect(9, 9, 1, 1));
  VerifySerializeAndDeserializeProto(region);
}

TEST(RegionTest, OverlappingRectIntersectProtoConversion) {
  Region region(gfx::Rect(0, 0, 10, 10));
  region.Intersect(gfx::Rect(5, 5, 10, 10));
  VerifySerializeAndDeserializeProto(region);
}

TEST(RegionTest, OverlappingRectUnionProtoConversion) {
  Region region(gfx::Rect(0, 0, 10, 10));
  region.Union(gfx::Rect(5, 5, 10, 10));
  VerifySerializeAndDeserializeProto(region);
}

TEST(RegionTest, OverlappingRectSubtractProtoConversion) {
  Region region(gfx::Rect(0, 0, 10, 10));
  region.Subtract(gfx::Rect(5, 5, 1, 1));
  VerifySerializeAndDeserializeProto(region);
}

TEST(ScrollbarOrientationTest, ScrollbarOrientationToProtoConversion) {
  ASSERT_NE(ScrollbarOrientation::HORIZONTAL, ScrollbarOrientation::VERTICAL);
  ScrollbarOrientation orientation = ScrollbarOrientation::HORIZONTAL;
  EXPECT_EQ(orientation, ScrollbarOrientationFromProto(
                             ScrollbarOrientationToProto(orientation)));

  orientation = ScrollbarOrientation::VERTICAL;
  EXPECT_EQ(orientation, ScrollbarOrientationFromProto(
                             ScrollbarOrientationToProto(orientation)));
}

}  // namespace
}  // namespace cc
