// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_sequence_generator.h"

#include "cc/surfaces/surface_sequence.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(SurfaceSequenceGeneratorTest, Basic) {
  SurfaceSequenceGenerator generator;
  generator.set_frame_sink_id(kArbitraryFrameSinkId);
  SurfaceSequence sequence1 = generator.CreateSurfaceSequence();
  SurfaceSequence sequence2 = generator.CreateSurfaceSequence();
  EXPECT_NE(sequence1, sequence2);
}

}  // namespace
}  // namespace cc
