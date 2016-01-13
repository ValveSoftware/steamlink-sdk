// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MediaStreamConstraintsUtilTest : public testing::Test {
};

TEST_F(MediaStreamConstraintsUtilTest, BooleanConstraints) {
  static const std::string kValueTrue = "true";
  static const std::string kValueFalse = "false";

  MockMediaConstraintFactory constraint_factory;
  // Mandatory constraints.
  constraint_factory.AddMandatory(MediaAudioConstraints::kEchoCancellation,
                                  kValueTrue);
  constraint_factory.AddMandatory(MediaAudioConstraints::kGoogEchoCancellation,
                                  kValueFalse);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();
  bool value_true = false;
  bool value_false = false;
  EXPECT_TRUE(GetMandatoryConstraintValueAsBoolean(
      constraints, MediaAudioConstraints::kEchoCancellation, &value_true));
  EXPECT_TRUE(GetMandatoryConstraintValueAsBoolean(
      constraints, MediaAudioConstraints::kGoogEchoCancellation, &value_false));
  EXPECT_TRUE(value_true);
  EXPECT_FALSE(value_false);

  // Optional constraints.
  constraint_factory.AddOptional(MediaAudioConstraints::kEchoCancellation,
                                 kValueFalse);
  constraint_factory.AddOptional(MediaAudioConstraints::kGoogEchoCancellation,
                                 kValueTrue);
  constraints = constraint_factory.CreateWebMediaConstraints();
  EXPECT_TRUE(GetOptionalConstraintValueAsBoolean(
      constraints, MediaAudioConstraints::kEchoCancellation, &value_false));
  EXPECT_TRUE(GetOptionalConstraintValueAsBoolean(
      constraints, MediaAudioConstraints::kGoogEchoCancellation,
      &value_true));
  EXPECT_TRUE(value_true);
  EXPECT_FALSE(value_false);
}

TEST_F(MediaStreamConstraintsUtilTest, IntConstraints) {
  MockMediaConstraintFactory constraint_factory;
  int width = 600;
  int height = 480;
  constraint_factory.AddMandatory(MediaStreamVideoSource::kMaxWidth, width);
  constraint_factory.AddMandatory(MediaStreamVideoSource::kMaxHeight, height);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();
  int value_width = 0;
  int value_height = 0;
  EXPECT_TRUE(GetMandatoryConstraintValueAsInteger(
      constraints, MediaStreamVideoSource::kMaxWidth, &value_width));
  EXPECT_TRUE(GetMandatoryConstraintValueAsInteger(
      constraints, MediaStreamVideoSource::kMaxHeight, &value_height));
  EXPECT_EQ(width, value_width);
  EXPECT_EQ(height, value_height);

  width = 720;
  height = 600;
  constraint_factory.AddOptional(MediaStreamVideoSource::kMaxWidth, width);
  constraint_factory.AddOptional(MediaStreamVideoSource::kMaxHeight, height);
  constraints = constraint_factory.CreateWebMediaConstraints();
  EXPECT_TRUE(GetOptionalConstraintValueAsInteger(
      constraints, MediaStreamVideoSource::kMaxWidth, &value_width));
  EXPECT_TRUE(GetOptionalConstraintValueAsInteger(
      constraints, MediaStreamVideoSource::kMaxHeight, &value_height));
  EXPECT_EQ(width, value_width);
  EXPECT_EQ(height, value_height);
}

TEST_F(MediaStreamConstraintsUtilTest, WrongBooleanConstraints) {
  static const std::string kWrongValueTrue = "True";
  static const std::string kWrongValueFalse = "False";
  MockMediaConstraintFactory constraint_factory;
  constraint_factory.AddMandatory(MediaAudioConstraints::kEchoCancellation,
                                  kWrongValueTrue);
  constraint_factory.AddMandatory(MediaAudioConstraints::kGoogEchoCancellation,
                                  kWrongValueFalse);
  blink::WebMediaConstraints constraints =
      constraint_factory.CreateWebMediaConstraints();
  bool value_false = false;
  EXPECT_FALSE(GetMandatoryConstraintValueAsBoolean(
      constraints, MediaAudioConstraints::kEchoCancellation, &value_false));
  EXPECT_FALSE(value_false);
  EXPECT_FALSE(GetMandatoryConstraintValueAsBoolean(
      constraints, MediaAudioConstraints::kGoogEchoCancellation, &value_false));
  EXPECT_FALSE(value_false);
}

}  // namespace content
