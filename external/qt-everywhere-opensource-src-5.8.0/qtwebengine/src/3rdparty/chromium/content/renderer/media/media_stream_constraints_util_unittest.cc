// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MediaStreamConstraintsUtilTest : public testing::Test {
};

TEST_F(MediaStreamConstraintsUtilTest, BooleanConstraints) {
  static const std::string kValueTrue = "true";
  static const std::string kValueFalse = "false";

  MockConstraintFactory constraint_factory;
  // Mandatory constraints.
  constraint_factory.basic().echoCancellation.setExact(true);
  constraint_factory.basic().googEchoCancellation.setExact(false);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();
  bool value_true = false;
  bool value_false = false;
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::echoCancellation,
      &value_true));
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::googEchoCancellation,
      &value_false));
  EXPECT_TRUE(value_true);
  EXPECT_FALSE(value_false);

  // Optional constraints, represented as "advanced"
  constraint_factory.Reset();
  constraint_factory.AddAdvanced().echoCancellation.setExact(false);
  constraint_factory.AddAdvanced().googEchoCancellation.setExact(true);
  constraints = constraint_factory.CreateWebMediaConstraints();
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::echoCancellation,
      &value_false));
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::googEchoCancellation,
      &value_true));
  EXPECT_TRUE(value_true);
  EXPECT_FALSE(value_false);

  // A mandatory constraint should override an optional one.
  constraint_factory.Reset();
  constraint_factory.AddAdvanced().echoCancellation.setExact(false);
  constraint_factory.basic().echoCancellation.setExact(true);
  constraints = constraint_factory.CreateWebMediaConstraints();
  EXPECT_TRUE(GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::echoCancellation,
      &value_true));
  EXPECT_TRUE(value_true);
}

TEST_F(MediaStreamConstraintsUtilTest, DoubleConstraints) {
  MockConstraintFactory constraint_factory;
  const double test_value= 0.01f;

  constraint_factory.basic().aspectRatio.setExact(test_value);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();

  double value;
  EXPECT_FALSE(GetConstraintValueAsDouble(
      constraints, &blink::WebMediaTrackConstraintSet::frameRate, &value));
  EXPECT_TRUE(GetConstraintValueAsDouble(
      constraints, &blink::WebMediaTrackConstraintSet::aspectRatio, &value));
  EXPECT_EQ(test_value, value);
}

TEST_F(MediaStreamConstraintsUtilTest, IntConstraints) {
  MockConstraintFactory constraint_factory;
  const int test_value = 327;

  constraint_factory.basic().width.setExact(test_value);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();

  int value;
  EXPECT_TRUE(GetConstraintValueAsInteger(
      constraints, &blink::WebMediaTrackConstraintSet::width, &value));
  EXPECT_EQ(test_value, value);
}

}  // namespace content
