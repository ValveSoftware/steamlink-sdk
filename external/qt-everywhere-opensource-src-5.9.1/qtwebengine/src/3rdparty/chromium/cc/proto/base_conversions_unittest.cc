// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/proto/base_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(BaseProtoConversionsTest, SerializeTimeTicks) {
  base::TimeTicks ticks;
  base::TimeTicks new_ticks;

  ticks = base::TimeTicks::FromInternalValue(2);
  new_ticks = ProtoToTimeTicks(TimeTicksToProto(ticks));
  EXPECT_EQ(ticks, new_ticks);

  ticks = base::TimeTicks::Now();
  new_ticks = ProtoToTimeTicks(TimeTicksToProto(ticks));
  EXPECT_EQ(ticks, new_ticks);

  ticks = base::TimeTicks::FromInternalValue(0);
  new_ticks = ProtoToTimeTicks(TimeTicksToProto(ticks));
  EXPECT_EQ(ticks, new_ticks);
}

}  // namespace
}  // namespace cc
