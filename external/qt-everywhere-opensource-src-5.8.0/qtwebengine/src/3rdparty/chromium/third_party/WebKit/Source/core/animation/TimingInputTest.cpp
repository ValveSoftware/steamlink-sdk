// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/TimingInput.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8KeyframeEffectOptions.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

namespace blink {

Timing applyTimingInputNumber(v8::Isolate* isolate, String timingProperty, double timingPropertyValue, bool& timingConversionSuccess)
{
    v8::Local<v8::Object> timingInput = v8::Object::New(isolate);
    setV8ObjectPropertyAsNumber(isolate, timingInput, timingProperty, timingPropertyValue);
    KeyframeEffectOptions timingInputDictionary;
    TrackExceptionState exceptionState;
    V8KeyframeEffectOptions::toImpl(isolate, timingInput, timingInputDictionary, exceptionState);
    Timing result;
    timingConversionSuccess = TimingInput::convert(timingInputDictionary, result, nullptr, exceptionState) && !exceptionState.hadException();
    return result;
}

Timing applyTimingInputString(v8::Isolate* isolate, String timingProperty, String timingPropertyValue, bool& timingConversionSuccess)
{
    v8::Local<v8::Object> timingInput = v8::Object::New(isolate);
    setV8ObjectPropertyAsString(isolate, timingInput, timingProperty, timingPropertyValue);
    KeyframeEffectOptions timingInputDictionary;
    TrackExceptionState exceptionState;
    V8KeyframeEffectOptions::toImpl(isolate, timingInput, timingInputDictionary, exceptionState);
    Timing result;
    timingConversionSuccess = TimingInput::convert(timingInputDictionary, result, nullptr, exceptionState) && !exceptionState.hadException();
    return result;
}

TEST(AnimationTimingInputTest, TimingInputStartDelay)
{
    V8TestingScope scope;
    bool ignoredSuccess;
    EXPECT_EQ(1.1, applyTimingInputNumber(scope.isolate(), "delay", 1100, ignoredSuccess).startDelay);
    EXPECT_EQ(-1, applyTimingInputNumber(scope.isolate(), "delay", -1000, ignoredSuccess).startDelay);
    EXPECT_EQ(1, applyTimingInputString(scope.isolate(), "delay", "1000", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString(scope.isolate(), "delay", "1s", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString(scope.isolate(), "delay", "Infinity", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString(scope.isolate(), "delay", "-Infinity", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString(scope.isolate(), "delay", "NaN", ignoredSuccess).startDelay);
    EXPECT_EQ(0, applyTimingInputString(scope.isolate(), "delay", "rubbish", ignoredSuccess).startDelay);
}

TEST(AnimationTimingInputTest, TimingInputEndDelay)
{
    V8TestingScope scope;
    bool ignoredSuccess;
    EXPECT_EQ(10, applyTimingInputNumber(scope.isolate(), "endDelay", 10000, ignoredSuccess).endDelay);
    EXPECT_EQ(-2.5, applyTimingInputNumber(scope.isolate(), "endDelay", -2500, ignoredSuccess).endDelay);
}

TEST(AnimationTimingInputTest, TimingInputFillMode)
{
    V8TestingScope scope;
    Timing::FillMode defaultFillMode = Timing::FillModeAuto;
    bool ignoredSuccess;

    EXPECT_EQ(Timing::FillModeAuto, applyTimingInputString(scope.isolate(), "fill", "auto", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeForwards, applyTimingInputString(scope.isolate(), "fill", "forwards", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeNone, applyTimingInputString(scope.isolate(), "fill", "none", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeBackwards, applyTimingInputString(scope.isolate(), "fill", "backwards", ignoredSuccess).fillMode);
    EXPECT_EQ(Timing::FillModeBoth, applyTimingInputString(scope.isolate(), "fill", "both", ignoredSuccess).fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputString(scope.isolate(), "fill", "everything!", ignoredSuccess).fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputString(scope.isolate(), "fill", "backwardsandforwards", ignoredSuccess).fillMode);
    EXPECT_EQ(defaultFillMode, applyTimingInputNumber(scope.isolate(), "fill", 2, ignoredSuccess).fillMode);
}

TEST(AnimationTimingInputTest, TimingInputIterationStart)
{
    V8TestingScope scope;
    bool success;
    EXPECT_EQ(1.1, applyTimingInputNumber(scope.isolate(), "iterationStart", 1.1, success).iterationStart);
    EXPECT_TRUE(success);

    applyTimingInputNumber(scope.isolate(), "iterationStart", -1, success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "iterationStart", "Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "iterationStart", "-Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "iterationStart", "NaN", success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "iterationStart", "rubbish", success);
    EXPECT_FALSE(success);
}

TEST(AnimationTimingInputTest, TimingInputIterationCount)
{
    V8TestingScope scope;
    bool success;
    EXPECT_EQ(2.1, applyTimingInputNumber(scope.isolate(), "iterations", 2.1, success).iterationCount);
    EXPECT_TRUE(success);

    Timing timing = applyTimingInputString(scope.isolate(), "iterations", "Infinity", success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isinf(timing.iterationCount));
    EXPECT_GT(timing.iterationCount, 0);

    applyTimingInputNumber(scope.isolate(), "iterations", -1, success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "iterations", "-Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "iterations", "NaN", success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "iterations", "rubbish", success);
    EXPECT_FALSE(success);
}

TEST(AnimationTimingInputTest, TimingInputIterationDuration)
{
    V8TestingScope scope;
    bool success;
    EXPECT_EQ(1.1, applyTimingInputNumber(scope.isolate(), "duration", 1100, success).iterationDuration);
    EXPECT_TRUE(success);

    Timing timing = applyTimingInputNumber(scope.isolate(), "duration", std::numeric_limits<double>::infinity(), success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isinf(timing.iterationDuration));
    EXPECT_GT(timing.iterationDuration, 0);

    EXPECT_TRUE(std::isnan(applyTimingInputString(scope.isolate(), "duration", "auto", success).iterationDuration));
    EXPECT_TRUE(success);

    applyTimingInputString(scope.isolate(), "duration", "1000", success);
    EXPECT_FALSE(success);

    applyTimingInputNumber(scope.isolate(), "duration", -1000, success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "duration", "-Infinity", success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "duration", "NaN", success);
    EXPECT_FALSE(success);

    applyTimingInputString(scope.isolate(), "duration", "rubbish", success);
    EXPECT_FALSE(success);
}

TEST(AnimationTimingInputTest, TimingInputPlaybackRate)
{
    V8TestingScope scope;
    bool ignoredSuccess;
    EXPECT_EQ(2.1, applyTimingInputNumber(scope.isolate(), "playbackRate", 2.1, ignoredSuccess).playbackRate);
    EXPECT_EQ(-1, applyTimingInputNumber(scope.isolate(), "playbackRate", -1, ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString(scope.isolate(), "playbackRate", "Infinity", ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString(scope.isolate(), "playbackRate", "-Infinity", ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString(scope.isolate(), "playbackRate", "NaN", ignoredSuccess).playbackRate);
    EXPECT_EQ(1, applyTimingInputString(scope.isolate(), "playbackRate", "rubbish", ignoredSuccess).playbackRate);
}

TEST(AnimationTimingInputTest, TimingInputDirection)
{
    V8TestingScope scope;
    Timing::PlaybackDirection defaultPlaybackDirection = Timing::PlaybackDirectionNormal;
    bool ignoredSuccess;

    EXPECT_EQ(Timing::PlaybackDirectionNormal, applyTimingInputString(scope.isolate(), "direction", "normal", ignoredSuccess).direction);
    EXPECT_EQ(Timing::PlaybackDirectionReverse, applyTimingInputString(scope.isolate(), "direction", "reverse", ignoredSuccess).direction);
    EXPECT_EQ(Timing::PlaybackDirectionAlternate, applyTimingInputString(scope.isolate(), "direction", "alternate", ignoredSuccess).direction);
    EXPECT_EQ(Timing::PlaybackDirectionAlternateReverse, applyTimingInputString(scope.isolate(), "direction", "alternate-reverse", ignoredSuccess).direction);
    EXPECT_EQ(defaultPlaybackDirection, applyTimingInputString(scope.isolate(), "direction", "rubbish", ignoredSuccess).direction);
    EXPECT_EQ(defaultPlaybackDirection, applyTimingInputNumber(scope.isolate(), "direction", 2, ignoredSuccess).direction);
}

TEST(AnimationTimingInputTest, TimingInputTimingFunction)
{
    V8TestingScope scope;
    const RefPtr<TimingFunction> defaultTimingFunction = LinearTimingFunction::shared();
    bool success;

    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE), *applyTimingInputString(scope.isolate(), "easing", "ease", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE_IN), *applyTimingInputString(scope.isolate(), "easing", "ease-in", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE_OUT), *applyTimingInputString(scope.isolate(), "easing", "ease-out", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE_IN_OUT), *applyTimingInputString(scope.isolate(), "easing", "ease-in-out", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*LinearTimingFunction::shared(), *applyTimingInputString(scope.isolate(), "easing", "linear", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::StepPosition::START), *applyTimingInputString(scope.isolate(), "easing", "step-start", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::StepPosition::MIDDLE), *applyTimingInputString(scope.isolate(), "easing", "step-middle", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::preset(StepsTimingFunction::StepPosition::END), *applyTimingInputString(scope.isolate(), "easing", "step-end", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*CubicBezierTimingFunction::create(1, 1, 0.3, 0.3), *applyTimingInputString(scope.isolate(), "easing", "cubic-bezier(1, 1, 0.3, 0.3)", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::create(3, StepsTimingFunction::StepPosition::START), *applyTimingInputString(scope.isolate(), "easing", "steps(3, start)", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::create(5, StepsTimingFunction::StepPosition::MIDDLE), *applyTimingInputString(scope.isolate(), "easing", "steps(5, middle)", success).timingFunction);
    EXPECT_TRUE(success);
    EXPECT_EQ(*StepsTimingFunction::create(5, StepsTimingFunction::StepPosition::END), *applyTimingInputString(scope.isolate(), "easing", "steps(5, end)", success).timingFunction);
    EXPECT_TRUE(success);

    applyTimingInputString(scope.isolate(), "easing", "", success);
    EXPECT_FALSE(success);
    applyTimingInputString(scope.isolate(), "easing", "steps(5.6, end)", success);
    EXPECT_FALSE(success);
    applyTimingInputString(scope.isolate(), "easing", "cubic-bezier(2, 2, 0.3, 0.3)", success);
    EXPECT_FALSE(success);
    applyTimingInputString(scope.isolate(), "easing", "rubbish", success);
    EXPECT_FALSE(success);
    applyTimingInputNumber(scope.isolate(), "easing", 2, success);
    EXPECT_FALSE(success);
    applyTimingInputString(scope.isolate(), "easing", "initial", success);
    EXPECT_FALSE(success);
}

TEST(AnimationTimingInputTest, TimingInputEmpty)
{
    TrackExceptionState exceptionState;
    Timing controlTiming;
    Timing updatedTiming;
    bool success = TimingInput::convert(KeyframeEffectOptions(), updatedTiming, nullptr, exceptionState);
    EXPECT_TRUE(success);
    EXPECT_FALSE(exceptionState.hadException());

    EXPECT_EQ(controlTiming.startDelay, updatedTiming.startDelay);
    EXPECT_EQ(controlTiming.fillMode, updatedTiming.fillMode);
    EXPECT_EQ(controlTiming.iterationStart, updatedTiming.iterationStart);
    EXPECT_EQ(controlTiming.iterationCount, updatedTiming.iterationCount);
    EXPECT_TRUE(std::isnan(updatedTiming.iterationDuration));
    EXPECT_EQ(controlTiming.playbackRate, updatedTiming.playbackRate);
    EXPECT_EQ(controlTiming.direction, updatedTiming.direction);
    EXPECT_EQ(*controlTiming.timingFunction, *updatedTiming.timingFunction);
}

} // namespace blink
