// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/inter_process_time_ticks_converter.h"

#include <stdint.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;

namespace content {

namespace {

struct TestParams {
  int64_t local_lower_bound;
  int64_t remote_lower_bound;
  int64_t remote_upper_bound;
  int64_t local_upper_bound;
  int64_t test_time;
  int64_t test_delta;
};

struct TestResults {
  int64_t result_time;
  int32_t result_delta;
  bool is_skew_additive;
  int64_t skew;
};

TestResults RunTest(const TestParams& params) {
  TimeTicks local_lower_bound = TimeTicks::FromInternalValue(
      params.local_lower_bound);
  TimeTicks local_upper_bound = TimeTicks::FromInternalValue(
      params.local_upper_bound);
  TimeTicks remote_lower_bound = TimeTicks::FromInternalValue(
      params.remote_lower_bound);
  TimeTicks remote_upper_bound = TimeTicks::FromInternalValue(
      params.remote_upper_bound);
  TimeTicks test_time = TimeTicks::FromInternalValue(params.test_time);

  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(local_lower_bound),
      LocalTimeTicks::FromTimeTicks(local_upper_bound),
      RemoteTimeTicks::FromTimeTicks(remote_lower_bound),
      RemoteTimeTicks::FromTimeTicks(remote_upper_bound));

  TestResults results;
  results.result_time = converter.ToLocalTimeTicks(
      RemoteTimeTicks::FromTimeTicks(
          test_time)).ToTimeTicks().ToInternalValue();
  results.result_delta = converter.ToLocalTimeDelta(
      RemoteTimeDelta::FromRawDelta(params.test_delta)).ToInt32();
  results.is_skew_additive = converter.IsSkewAdditiveForMetrics();
  results.skew = converter.GetSkewForMetrics().ToInternalValue();
  return results;
}

TEST(InterProcessTimeTicksConverterTest, NullTime) {
  // Null / zero times should remain null.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 2;
  p.remote_upper_bound = 5;
  p.local_upper_bound = 6;
  p.test_time = 0;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(0, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, NoSkew) {
  // All times are monotonic and centered, so no adjustment should occur.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 2;
  p.remote_upper_bound = 5;
  p.local_upper_bound = 6;
  p.test_time = 3;
  p.test_delta = 1;
  TestResults results = RunTest(p);
  EXPECT_EQ(3, results.result_time);
  EXPECT_EQ(1, results.result_delta);
  EXPECT_TRUE(results.is_skew_additive);
  EXPECT_EQ(0, results.skew);
}

TEST(InterProcessTimeTicksConverterTest, OffsetMidpoints) {
  // All times are monotonic, but not centered. Adjust the |remote_*| times so
  // they are centered within the |local_*| times.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 3;
  p.remote_upper_bound = 6;
  p.local_upper_bound = 6;
  p.test_time = 4;
  p.test_delta = 1;
  TestResults results = RunTest(p);
  EXPECT_EQ(3, results.result_time);
  EXPECT_EQ(1, results.result_delta);
  EXPECT_TRUE(results.is_skew_additive);
  EXPECT_EQ(1, results.skew);
}

TEST(InterProcessTimeTicksConverterTest, DoubleEndedSkew) {
  // |remote_lower_bound| occurs before |local_lower_bound| and
  // |remote_upper_bound| occurs after |local_upper_bound|. We must adjust both
  // bounds and scale down the delta. |test_time| is on the midpoint, so it
  // doesn't change. The ratio of local time to network time is 1:2, so we scale
  // |test_delta| to half.
  TestParams p;
  p.local_lower_bound = 3;
  p.remote_lower_bound = 1;
  p.remote_upper_bound = 9;
  p.local_upper_bound = 7;
  p.test_time = 5;
  p.test_delta = 2;
  TestResults results = RunTest(p);
  EXPECT_EQ(5, results.result_time);
  EXPECT_EQ(1, results.result_delta);
  EXPECT_FALSE(results.is_skew_additive);
}

TEST(InterProcessTimeTicksConverterTest, FrontEndSkew) {
  // |remote_upper_bound| is coherent, but |remote_lower_bound| is not. So we
  // adjust the lower bound and move |test_time| out. The scale factor is 2:3,
  // but since we use integers, the numbers truncate from 3.33 to 3 and 1.33
  // to 1.
  TestParams p;
  p.local_lower_bound = 3;
  p.remote_lower_bound = 1;
  p.remote_upper_bound = 7;
  p.local_upper_bound = 7;
  p.test_time = 3;
  p.test_delta = 2;
  TestResults results = RunTest(p);
  EXPECT_EQ(4, results.result_time);
  EXPECT_EQ(1, results.result_delta);
  EXPECT_FALSE(results.is_skew_additive);
}

TEST(InterProcessTimeTicksConverterTest, BackEndSkew) {
  // Like the previous test, but |remote_lower_bound| is coherent and
  // |remote_upper_bound| is skewed.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 1;
  p.remote_upper_bound = 7;
  p.local_upper_bound = 5;
  p.test_time = 3;
  p.test_delta = 2;
  TestResults results = RunTest(p);
  EXPECT_EQ(2, results.result_time);
  EXPECT_EQ(1, results.result_delta);
  EXPECT_FALSE(results.is_skew_additive);
}

TEST(InterProcessTimeTicksConverterTest, Instantaneous) {
  // The bounds are all okay, but the |remote_lower_bound| and
  // |remote_upper_bound| have the same value. No adjustments should be made and
  // no divide-by-zero errors should occur.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 2;
  p.remote_upper_bound = 2;
  p.local_upper_bound = 3;
  p.test_time = 2;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(2, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, OffsetInstantaneous) {
  // The bounds are all okay, but the |remote_lower_bound| and
  // |remote_upper_bound| have the same value and are offset from the midpoint
  // of |local_lower_bound| and |local_upper_bound|. An offset should be applied
  // to make the midpoints line up.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 3;
  p.remote_upper_bound = 3;
  p.local_upper_bound = 3;
  p.test_time = 3;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(2, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, DisjointInstantaneous) {
  // |local_lower_bound| and |local_upper_bound| are the same. No matter what
  // the other values are, they must fit within [local_lower_bound,
  // local_upper_bound].  So, all of the values should be adjusted so they are
  // exactly that value.
  TestParams p;
  p.local_lower_bound = 1;
  p.remote_lower_bound = 2;
  p.remote_upper_bound = 2;
  p.local_upper_bound = 1;
  p.test_time = 2;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(1, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, RoundingNearEdges) {
  // Verify that rounding never causes a value to appear outside the given
  // |local_*| range.
  const int kMaxRange = 101;
  for (int i = 1; i < kMaxRange; ++i) {
    for (int j = 1; j < kMaxRange; ++j) {
      TestParams p;
      p.local_lower_bound = 1;
      p.remote_lower_bound = 1;
      p.remote_upper_bound = j;
      p.local_upper_bound = i;

      p.test_time = 1;
      p.test_delta = 0;
      TestResults results = RunTest(p);
      EXPECT_LE(1, results.result_time);
      EXPECT_EQ(0, results.result_delta);

      p.test_time = j;
      p.test_delta = j - 1;
      results = RunTest(p);
      EXPECT_GE(i, results.result_time);
      EXPECT_GE(i - 1, results.result_delta);
    }
  }
}

TEST(InterProcessTimeTicksConverterTest, DisjointRanges) {
  TestParams p;
  p.local_lower_bound = 10;
  p.remote_lower_bound = 30;
  p.remote_upper_bound = 41;
  p.local_upper_bound = 20;
  p.test_time = 41;
  p.test_delta = 0;
  TestResults results = RunTest(p);
  EXPECT_EQ(20, results.result_time);
  EXPECT_EQ(0, results.result_delta);
}

TEST(InterProcessTimeTicksConverterTest, ValuesOutsideOfRange) {
  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(TimeTicks::FromInternalValue(15)),
      LocalTimeTicks::FromTimeTicks(TimeTicks::FromInternalValue(20)),
      RemoteTimeTicks::FromTimeTicks(TimeTicks::FromInternalValue(10)),
      RemoteTimeTicks::FromTimeTicks(TimeTicks::FromInternalValue(25)));

  RemoteTimeTicks remote_ticks =
      RemoteTimeTicks::FromTimeTicks(TimeTicks::FromInternalValue(10));
  int64_t result =
      converter.ToLocalTimeTicks(remote_ticks).ToTimeTicks().ToInternalValue();
  EXPECT_EQ(15, result);

  remote_ticks =
      RemoteTimeTicks::FromTimeTicks(TimeTicks::FromInternalValue(25));
  result =
      converter.ToLocalTimeTicks(remote_ticks).ToTimeTicks().ToInternalValue();
  EXPECT_EQ(20, result);

  remote_ticks =
      RemoteTimeTicks::FromTimeTicks(TimeTicks::FromInternalValue(9));
  result =
      converter.ToLocalTimeTicks(remote_ticks).ToTimeTicks().ToInternalValue();
  EXPECT_EQ(14, result);
}

}  // anonymous namespace

}  // namespace content
