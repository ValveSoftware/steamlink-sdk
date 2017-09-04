/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/TimingCalculations.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AnimationTimingCalculationsTest, ActiveTime) {
  Timing timing;

  // calculateActiveTime(
  //     activeDuration, fillMode, localTime, parentPhase, phase, timing)

  // Before Phase
  timing.startDelay = 10;
  EXPECT_TRUE(isNull(calculateActiveTime(
      20, Timing::FillMode::FORWARDS, 0, AnimationEffectReadOnly::PhaseActive,
      AnimationEffectReadOnly::PhaseBefore, timing)));
  EXPECT_TRUE(isNull(calculateActiveTime(
      20, Timing::FillMode::NONE, 0, AnimationEffectReadOnly::PhaseActive,
      AnimationEffectReadOnly::PhaseBefore, timing)));
  EXPECT_EQ(0,
            calculateActiveTime(20, Timing::FillMode::BACKWARDS, 0,
                                AnimationEffectReadOnly::PhaseActive,
                                AnimationEffectReadOnly::PhaseBefore, timing));
  EXPECT_EQ(0,
            calculateActiveTime(20, Timing::FillMode::BOTH, 0,
                                AnimationEffectReadOnly::PhaseActive,
                                AnimationEffectReadOnly::PhaseBefore, timing));

  // Active Phase
  timing.startDelay = 10;
  // Active, and parent Before
  EXPECT_TRUE(isNull(calculateActiveTime(
      20, Timing::FillMode::NONE, 15, AnimationEffectReadOnly::PhaseBefore,
      AnimationEffectReadOnly::PhaseActive, timing)));
  EXPECT_TRUE(isNull(calculateActiveTime(
      20, Timing::FillMode::FORWARDS, 15, AnimationEffectReadOnly::PhaseBefore,
      AnimationEffectReadOnly::PhaseActive, timing)));
  // Active, and parent After
  EXPECT_TRUE(isNull(calculateActiveTime(
      20, Timing::FillMode::NONE, 15, AnimationEffectReadOnly::PhaseAfter,
      AnimationEffectReadOnly::PhaseActive, timing)));
  EXPECT_TRUE(isNull(calculateActiveTime(
      20, Timing::FillMode::BACKWARDS, 15, AnimationEffectReadOnly::PhaseAfter,
      AnimationEffectReadOnly::PhaseActive, timing)));
  // Active, and parent Active
  EXPECT_EQ(5,
            calculateActiveTime(20, Timing::FillMode::FORWARDS, 15,
                                AnimationEffectReadOnly::PhaseActive,
                                AnimationEffectReadOnly::PhaseActive, timing));

  // After Phase
  timing.startDelay = 10;
  EXPECT_EQ(21,
            calculateActiveTime(21, Timing::FillMode::FORWARDS, 45,
                                AnimationEffectReadOnly::PhaseActive,
                                AnimationEffectReadOnly::PhaseAfter, timing));
  EXPECT_EQ(21,
            calculateActiveTime(21, Timing::FillMode::BOTH, 45,
                                AnimationEffectReadOnly::PhaseActive,
                                AnimationEffectReadOnly::PhaseAfter, timing));
  EXPECT_TRUE(isNull(calculateActiveTime(
      21, Timing::FillMode::BACKWARDS, 45, AnimationEffectReadOnly::PhaseActive,
      AnimationEffectReadOnly::PhaseAfter, timing)));
  EXPECT_TRUE(isNull(calculateActiveTime(
      21, Timing::FillMode::NONE, 45, AnimationEffectReadOnly::PhaseActive,
      AnimationEffectReadOnly::PhaseAfter, timing)));

  // None
  EXPECT_TRUE(
      isNull(calculateActiveTime(32, Timing::FillMode::NONE, nullValue(),
                                 AnimationEffectReadOnly::PhaseNone,
                                 AnimationEffectReadOnly::PhaseNone, timing)));
}

TEST(AnimationTimingCalculationsTest, ScaledActiveTime) {
  Timing timing;

  // calculateScaledActiveTime(activeDuration, activeTime, startOffset, timing)

  // if the active time is null
  EXPECT_TRUE(isNull(calculateScaledActiveTime(4, nullValue(), 5, timing)));

  // if the playback rate is negative
  timing.playbackRate = -1;
  EXPECT_EQ(35, calculateScaledActiveTime(40, 10, 5, timing));

  // otherwise
  timing.playbackRate = 0;
  EXPECT_EQ(5, calculateScaledActiveTime(40, 10, 5, timing));
  timing.playbackRate = 1;
  EXPECT_EQ(15, calculateScaledActiveTime(40, 10, 5, timing));

  // infinte activeTime
  timing.playbackRate = 0;
  EXPECT_EQ(0, calculateScaledActiveTime(
                   std::numeric_limits<double>::infinity(),
                   std::numeric_limits<double>::infinity(), 0, timing));
  timing.playbackRate = 1;
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            calculateScaledActiveTime(std::numeric_limits<double>::infinity(),
                                      std::numeric_limits<double>::infinity(),
                                      0, timing));
  timing.playbackRate = -1;
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            calculateScaledActiveTime(std::numeric_limits<double>::infinity(),
                                      std::numeric_limits<double>::infinity(),
                                      0, timing));
}

TEST(AnimationTimingCalculationsTest, IterationTime) {
  Timing timing;

  // calculateIterationTime(
  //     iterationDuration, repeatedDuration, scaledActiveTime, startOffset,
  //     phase, timing)

  // if the scaled active time is null
  EXPECT_TRUE(isNull(calculateIterationTime(
      1, 1, nullValue(), 1, AnimationEffectReadOnly::PhaseActive, timing)));

  // if (complex-conditions)...
  EXPECT_EQ(
      12, calculateIterationTime(12, 12, 12, 0,
                                 AnimationEffectReadOnly::PhaseActive, timing));

  // otherwise
  timing.iterationCount = 10;
  EXPECT_EQ(
      5, calculateIterationTime(10, 100, 25, 4,
                                AnimationEffectReadOnly::PhaseActive, timing));
  EXPECT_EQ(
      7, calculateIterationTime(11, 110, 29, 1,
                                AnimationEffectReadOnly::PhaseActive, timing));
  timing.iterationStart = 1.1;
  EXPECT_EQ(
      8, calculateIterationTime(12, 120, 20, 7,
                                AnimationEffectReadOnly::PhaseActive, timing));
}

TEST(AnimationTimingCalculationsTest, CurrentIteration) {
  Timing timing;

  // calculateCurrentIteration(
  //     iterationDuration, iterationTime, scaledActiveTime, timing)

  // if the scaled active time is null
  EXPECT_TRUE(isNull(calculateCurrentIteration(1, 1, nullValue(), timing)));

  // if the scaled active time is zero
  EXPECT_EQ(0, calculateCurrentIteration(1, 1, 0, timing));

  // if the iteration time equals the iteration duration
  timing.iterationStart = 4;
  timing.iterationCount = 7;
  EXPECT_EQ(10, calculateCurrentIteration(5, 5, 9, timing));

  // otherwise
  EXPECT_EQ(3, calculateCurrentIteration(3.2, 3.1, 10, timing));
}

TEST(AnimationTimingCalculationsTest, DirectedTime) {
  Timing timing;

  // calculateDirectedTime(
  //     currentIteration, iterationDuration, iterationTime, timing)

  // if the iteration time is null
  EXPECT_TRUE(isNull(calculateDirectedTime(1, 2, nullValue(), timing)));

  // forwards
  EXPECT_EQ(17, calculateDirectedTime(0, 20, 17, timing));
  EXPECT_EQ(17, calculateDirectedTime(1, 20, 17, timing));
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  EXPECT_EQ(17, calculateDirectedTime(0, 20, 17, timing));
  EXPECT_EQ(17, calculateDirectedTime(2, 20, 17, timing));
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  EXPECT_EQ(17, calculateDirectedTime(1, 20, 17, timing));
  EXPECT_EQ(17, calculateDirectedTime(3, 20, 17, timing));

  // reverse
  timing.direction = Timing::PlaybackDirection::REVERSE;
  EXPECT_EQ(3, calculateDirectedTime(0, 20, 17, timing));
  EXPECT_EQ(3, calculateDirectedTime(1, 20, 17, timing));
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  EXPECT_EQ(3, calculateDirectedTime(1, 20, 17, timing));
  EXPECT_EQ(3, calculateDirectedTime(3, 20, 17, timing));
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  EXPECT_EQ(3, calculateDirectedTime(0, 20, 17, timing));
  EXPECT_EQ(3, calculateDirectedTime(2, 20, 17, timing));
}

TEST(AnimationTimingCalculationsTest, TransformedTime) {
  Timing timing;

  // calculateTransformedTime(
  //     currentIteration, iterationDuration, iterationTime, timing)

  // Iteration time is null
  EXPECT_TRUE(isNull(calculateTransformedTime(1, 2, nullValue(), timing)));

  // PlaybackDirectionForwards
  EXPECT_EQ(12, calculateTransformedTime(0, 20, 12, timing));
  EXPECT_EQ(12, calculateTransformedTime(1, 20, 12, timing));

  // PlaybackDirectionForwards with timing function
  timing.timingFunction =
      StepsTimingFunction::create(4, StepsTimingFunction::StepPosition::END);
  EXPECT_EQ(10, calculateTransformedTime(0, 20, 12, timing));
  EXPECT_EQ(10, calculateTransformedTime(1, 20, 12, timing));

  // PlaybackDirectionReverse
  timing.timingFunction = Timing::defaults().timingFunction;
  timing.direction = Timing::PlaybackDirection::REVERSE;
  EXPECT_EQ(8, calculateTransformedTime(0, 20, 12, timing));
  EXPECT_EQ(8, calculateTransformedTime(1, 20, 12, timing));

  // PlaybackDirectionReverse with timing function
  timing.timingFunction =
      StepsTimingFunction::create(4, StepsTimingFunction::StepPosition::END);
  EXPECT_EQ(5, calculateTransformedTime(0, 20, 12, timing));
  EXPECT_EQ(5, calculateTransformedTime(1, 20, 12, timing));

  // Timing function when directed time is null.
  EXPECT_TRUE(isNull(calculateTransformedTime(1, 2, nullValue(), timing)));

  // Timing function when iterationDuration is infinity
  timing.direction = Timing::PlaybackDirection::NORMAL;
  EXPECT_EQ(0, calculateTransformedTime(
                   0, std::numeric_limits<double>::infinity(), 0, timing));
  EXPECT_EQ(1, calculateTransformedTime(
                   0, std::numeric_limits<double>::infinity(), 1, timing));
  timing.direction = Timing::PlaybackDirection::REVERSE;
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            calculateTransformedTime(0, std::numeric_limits<double>::infinity(),
                                     0, timing));
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            calculateTransformedTime(0, std::numeric_limits<double>::infinity(),
                                     1, timing));
}

}  // namespace blink
