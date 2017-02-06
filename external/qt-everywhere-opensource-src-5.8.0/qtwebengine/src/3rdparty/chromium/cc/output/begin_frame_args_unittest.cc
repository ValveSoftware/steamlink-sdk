// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "cc/output/begin_frame_args.h"
#include "cc/proto/begin_main_frame_and_commit_state.pb.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(BeginFrameArgsTest, Helpers) {
  // Quick create methods work
  BeginFrameArgs args0 = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE);
  EXPECT_TRUE(args0.IsValid()) << args0;

  BeginFrameArgs args1 =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 0, -1);
  EXPECT_FALSE(args1.IsValid()) << args1;

  BeginFrameArgs args2 =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 1, 2, 3);
  EXPECT_TRUE(args2.IsValid()) << args2;
  EXPECT_EQ(1, args2.frame_time.ToInternalValue());
  EXPECT_EQ(2, args2.deadline.ToInternalValue());
  EXPECT_EQ(3, args2.interval.ToInternalValue());
  EXPECT_EQ(BeginFrameArgs::NORMAL, args2.type);

  BeginFrameArgs args4 = CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 1, 2, 3, BeginFrameArgs::MISSED);
  EXPECT_TRUE(args4.IsValid()) << args4;
  EXPECT_EQ(1, args4.frame_time.ToInternalValue());
  EXPECT_EQ(2, args4.deadline.ToInternalValue());
  EXPECT_EQ(3, args4.interval.ToInternalValue());
  EXPECT_EQ(BeginFrameArgs::MISSED, args4.type);

  // operator==
  EXPECT_EQ(CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 4, 5, 6),
            CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 4, 5, 6));

  EXPECT_NONFATAL_FAILURE(
      EXPECT_EQ(CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 7, 8, 9,
                                               BeginFrameArgs::MISSED),
                CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 7, 8, 9)),
      "");

  EXPECT_NONFATAL_FAILURE(
      EXPECT_EQ(CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 4, 5, 6),
                CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 7, 8, 9)),
      "");

  // operator<<
  std::stringstream out1;
  out1 << args1;
  EXPECT_EQ("BeginFrameArgs(NORMAL, 0, 0, -1us)", out1.str());
  std::stringstream out2;
  out2 << args2;
  EXPECT_EQ("BeginFrameArgs(NORMAL, 1, 2, 3us)", out2.str());

  // PrintTo
  EXPECT_EQ(std::string("BeginFrameArgs(NORMAL, 0, 0, -1us)"),
            ::testing::PrintToString(args1));
  EXPECT_EQ(std::string("BeginFrameArgs(NORMAL, 1, 2, 3us)"),
            ::testing::PrintToString(args2));
}

TEST(BeginFrameArgsTest, Create) {
  // BeginFrames are not valid by default
  BeginFrameArgs args1;
  EXPECT_FALSE(args1.IsValid()) << args1;
  EXPECT_TRUE(args1.on_critical_path);

  BeginFrameArgs args2 = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, base::TimeTicks::FromInternalValue(1),
      base::TimeTicks::FromInternalValue(2),
      base::TimeDelta::FromInternalValue(3), BeginFrameArgs::NORMAL);
  EXPECT_TRUE(args2.IsValid()) << args2;
  EXPECT_EQ(1, args2.frame_time.ToInternalValue()) << args2;
  EXPECT_EQ(2, args2.deadline.ToInternalValue()) << args2;
  EXPECT_EQ(3, args2.interval.ToInternalValue()) << args2;
  EXPECT_EQ(BeginFrameArgs::NORMAL, args2.type) << args2;
}

TEST(BeginFrameArgsSerializationTest, BeginFrameArgsType) {
  for (size_t i = 0;
       i < BeginFrameArgs::BeginFrameArgsType::BEGIN_FRAME_ARGS_TYPE_MAX; ++i) {
    BeginFrameArgs::BeginFrameArgsType type =
        static_cast<BeginFrameArgs::BeginFrameArgsType>(i);
    BeginFrameArgs args;
    args.type = type;

    proto::BeginFrameArgs proto;
    args.BeginFrameArgsTypeToProtobuf(&proto);

    BeginFrameArgs new_args;
    new_args.BeginFrameArgsTypeFromProtobuf(proto);
    EXPECT_EQ(args.type, new_args.type);
  }
}

TEST(BeginFrameArgsSerializationTest, BeginFrameArgs) {
  BeginFrameArgs args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, base::TimeTicks::FromInternalValue(1),
      base::TimeTicks::FromInternalValue(2),
      base::TimeDelta::FromInternalValue(3), BeginFrameArgs::NORMAL);
  proto::BeginFrameArgs proto;
  args.ToProtobuf(&proto);

  BeginFrameArgs new_args;
  new_args.FromProtobuf(proto);

  EXPECT_EQ(args, new_args);
}

#ifndef NDEBUG
TEST(BeginFrameArgsTest, Location) {
  tracked_objects::Location expected_location = BEGINFRAME_FROM_HERE;

  BeginFrameArgs args = CreateBeginFrameArgsForTesting(expected_location);
  EXPECT_EQ(expected_location.ToString(), args.created_from.ToString());
}
#endif

}  // namespace
}  // namespace cc
