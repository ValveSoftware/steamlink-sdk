// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/KeyframeEffect.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/EffectModelOrDictionarySequenceOrDictionary.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8KeyframeEffectOptions.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationEffectTiming.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/Timing.h"
#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>
#include <v8.h>

namespace blink {

class KeyframeEffectTest : public ::testing::Test {
protected:
    KeyframeEffectTest()
        : pageHolder(DummyPageHolder::create())
    {
        element = document().createElement("foo", ASSERT_NO_EXCEPTION);

        document().animationClock().resetTimeForTesting(document().timeline().zeroTime());
        document().documentElement()->appendChild(element.get());
        EXPECT_EQ(0, document().timeline().currentTime());
    }

    Document& document() const { return pageHolder->document(); }

    std::unique_ptr<DummyPageHolder> pageHolder;
    Persistent<Element> element;
    TrackExceptionState exceptionState;
};

class AnimationKeyframeEffectV8Test : public KeyframeEffectTest {
protected:
    template<typename T>
    static KeyframeEffect* createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, T timingInput, ExceptionState& exceptionState)
    {
        return KeyframeEffect::create(nullptr, element, EffectModelOrDictionarySequenceOrDictionary::fromDictionarySequence(keyframeDictionaryVector), timingInput, exceptionState);
    }
    static KeyframeEffect* createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, ExceptionState& exceptionState)
    {
        return KeyframeEffect::create(nullptr, element, EffectModelOrDictionarySequenceOrDictionary::fromDictionarySequence(keyframeDictionaryVector), exceptionState);
    }
};

TEST_F(AnimationKeyframeEffectV8Test, CanCreateAnAnimation)
{
    V8TestingScope scope;
    Vector<Dictionary> jsKeyframes;
    v8::Local<v8::Object> keyframe1 = v8::Object::New(scope.isolate());
    v8::Local<v8::Object> keyframe2 = v8::Object::New(scope.isolate());

    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe1, "easing", "ease-in-out");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "offset", "1");
    setV8ObjectPropertyAsString(scope.isolate(), keyframe2, "easing", "cubic-bezier(1, 1, 0.3, 0.3)");

    jsKeyframes.append(Dictionary(keyframe1, scope.isolate(), exceptionState));
    jsKeyframes.append(Dictionary(keyframe2, scope.isolate(), exceptionState));

    String value1;
    ASSERT_TRUE(DictionaryHelper::get(jsKeyframes[0], "width", value1));
    ASSERT_EQ("100px", value1);

    String value2;
    ASSERT_TRUE(DictionaryHelper::get(jsKeyframes[1], "width", value2));
    ASSERT_EQ("0px", value2);

    KeyframeEffect* animation = createAnimation(element.get(), jsKeyframes, 0, exceptionState);

    Element* target = animation->target();
    EXPECT_EQ(*element.get(), *target);

    const KeyframeVector keyframes = toKeyframeEffectModelBase(animation->model())->getFrames();

    EXPECT_EQ(0, keyframes[0]->offset());
    EXPECT_EQ(1, keyframes[1]->offset());

    const CSSValue* keyframe1Width = toStringKeyframe(keyframes[0].get())->cssPropertyValue(CSSPropertyWidth);
    const CSSValue* keyframe2Width = toStringKeyframe(keyframes[1].get())->cssPropertyValue(CSSPropertyWidth);
    ASSERT(keyframe1Width);
    ASSERT(keyframe2Width);

    EXPECT_EQ("100px", keyframe1Width->cssText());
    EXPECT_EQ("0px", keyframe2Width->cssText());

    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseType::EASE_IN_OUT)), keyframes[0]->easing());
    EXPECT_EQ(*(CubicBezierTimingFunction::create(1, 1, 0.3, 0.3).get()), keyframes[1]->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, CanSetDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    double duration = 2000;

    KeyframeEffect* animation = createAnimation(element.get(), jsKeyframes, duration, exceptionState);

    EXPECT_EQ(duration / 1000, animation->specifiedTiming().iterationDuration);
}

TEST_F(AnimationKeyframeEffectV8Test, CanOmitSpecifiedDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    KeyframeEffect* animation = createAnimation(element.get(), jsKeyframes, exceptionState);
    EXPECT_TRUE(std::isnan(animation->specifiedTiming().iterationDuration));
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedGetters)
{
    V8TestingScope scope;
    Vector<Dictionary, 0> jsKeyframes;

    v8::Local<v8::Object> timingInput = v8::Object::New(scope.isolate());
    setV8ObjectPropertyAsNumber(scope.isolate(), timingInput, "delay", 2);
    setV8ObjectPropertyAsNumber(scope.isolate(), timingInput, "endDelay", 0.5);
    setV8ObjectPropertyAsString(scope.isolate(), timingInput, "fill", "backwards");
    setV8ObjectPropertyAsNumber(scope.isolate(), timingInput, "iterationStart", 2);
    setV8ObjectPropertyAsNumber(scope.isolate(), timingInput, "iterations", 10);
    setV8ObjectPropertyAsNumber(scope.isolate(), timingInput, "playbackRate", 2);
    setV8ObjectPropertyAsString(scope.isolate(), timingInput, "direction", "reverse");
    setV8ObjectPropertyAsString(scope.isolate(), timingInput, "easing", "step-start");
    KeyframeEffectOptions timingInputDictionary;
    V8KeyframeEffectOptions::toImpl(scope.isolate(), timingInput, timingInputDictionary, exceptionState);

    KeyframeEffect* animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary, exceptionState);

    AnimationEffectTiming* specified = animation->timing();
    EXPECT_EQ(2, specified->delay());
    EXPECT_EQ(0.5, specified->endDelay());
    EXPECT_EQ("backwards", specified->fill());
    EXPECT_EQ(2, specified->iterationStart());
    EXPECT_EQ(10, specified->iterations());
    EXPECT_EQ(2, specified->playbackRate());
    EXPECT_EQ("reverse", specified->direction());
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedDurationGetter)
{
    V8TestingScope scope;
    Vector<Dictionary, 0> jsKeyframes;

    v8::Local<v8::Object> timingInputWithDuration = v8::Object::New(scope.isolate());
    setV8ObjectPropertyAsNumber(scope.isolate(), timingInputWithDuration, "duration", 2.5);
    KeyframeEffectOptions timingInputDictionaryWithDuration;
    V8KeyframeEffectOptions::toImpl(scope.isolate(), timingInputWithDuration, timingInputDictionaryWithDuration, exceptionState);

    KeyframeEffect* animationWithDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryWithDuration, exceptionState);

    AnimationEffectTiming* specifiedWithDuration = animationWithDuration->timing();
    UnrestrictedDoubleOrString duration;
    specifiedWithDuration->duration(duration);
    EXPECT_TRUE(duration.isUnrestrictedDouble());
    EXPECT_EQ(2.5, duration.getAsUnrestrictedDouble());
    EXPECT_FALSE(duration.isString());


    v8::Local<v8::Object> timingInputNoDuration = v8::Object::New(scope.isolate());
    KeyframeEffectOptions timingInputDictionaryNoDuration;
    V8KeyframeEffectOptions::toImpl(scope.isolate(), timingInputNoDuration, timingInputDictionaryNoDuration, exceptionState);

    KeyframeEffect* animationNoDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryNoDuration, exceptionState);

    AnimationEffectTiming* specifiedNoDuration = animationNoDuration->timing();
    UnrestrictedDoubleOrString duration2;
    specifiedNoDuration->duration(duration2);
    EXPECT_FALSE(duration2.isUnrestrictedDouble());
    EXPECT_TRUE(duration2.isString());
    EXPECT_EQ("auto", duration2.getAsString());
}

TEST_F(AnimationKeyframeEffectV8Test, SpecifiedSetters)
{
    V8TestingScope scope;
    Vector<Dictionary, 0> jsKeyframes;
    v8::Local<v8::Object> timingInput = v8::Object::New(scope.isolate());
    KeyframeEffectOptions timingInputDictionary;
    V8KeyframeEffectOptions::toImpl(scope.isolate(), timingInput, timingInputDictionary, exceptionState);
    KeyframeEffect* animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary, exceptionState);

    AnimationEffectTiming* specified = animation->timing();

    EXPECT_EQ(0, specified->delay());
    specified->setDelay(2);
    EXPECT_EQ(2, specified->delay());

    EXPECT_EQ(0, specified->endDelay());
    specified->setEndDelay(0.5);
    EXPECT_EQ(0.5, specified->endDelay());

    EXPECT_EQ("auto", specified->fill());
    specified->setFill("backwards");
    EXPECT_EQ("backwards", specified->fill());

    EXPECT_EQ(0, specified->iterationStart());
    specified->setIterationStart(2, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());
    EXPECT_EQ(2, specified->iterationStart());

    EXPECT_EQ(1, specified->iterations());
    specified->setIterations(10, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());
    EXPECT_EQ(10, specified->iterations());

    EXPECT_EQ(1, specified->playbackRate());
    specified->setPlaybackRate(2);
    EXPECT_EQ(2, specified->playbackRate());

    EXPECT_EQ("normal", specified->direction());
    specified->setDirection("reverse");
    EXPECT_EQ("reverse", specified->direction());

    EXPECT_EQ("linear", specified->easing());
    specified->setEasing("step-start", exceptionState);
    ASSERT_FALSE(exceptionState.hadException());
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationKeyframeEffectV8Test, SetSpecifiedDuration)
{
    V8TestingScope scope;
    Vector<Dictionary, 0> jsKeyframes;
    v8::Local<v8::Object> timingInput = v8::Object::New(scope.isolate());
    KeyframeEffectOptions timingInputDictionary;
    V8KeyframeEffectOptions::toImpl(scope.isolate(), timingInput, timingInputDictionary, exceptionState);
    KeyframeEffect* animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary, exceptionState);

    AnimationEffectTiming* specified = animation->timing();

    UnrestrictedDoubleOrString duration;
    specified->duration(duration);
    EXPECT_FALSE(duration.isUnrestrictedDouble());
    EXPECT_TRUE(duration.isString());
    EXPECT_EQ("auto", duration.getAsString());

    UnrestrictedDoubleOrString inDuration;
    inDuration.setUnrestrictedDouble(2.5);
    specified->setDuration(inDuration, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());
    UnrestrictedDoubleOrString duration2;
    specified->duration(duration2);
    EXPECT_TRUE(duration2.isUnrestrictedDouble());
    EXPECT_EQ(2.5, duration2.getAsUnrestrictedDouble());
    EXPECT_FALSE(duration2.isString());
}

TEST_F(KeyframeEffectTest, TimeToEffectChange)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.fillMode = Timing::FillModeNone;
    KeyframeEffect* animation = KeyframeEffect::create(0, nullptr, timing);
    Animation* player = document().timeline().play(animation);
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(100);
    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(199);
    EXPECT_EQ(1, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(200);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(300);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(100, animation->timeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, TimeToEffectChangeWithPlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = 2;
    timing.fillMode = Timing::FillModeNone;
    KeyframeEffect* animation = KeyframeEffect::create(0, nullptr, timing);
    Animation* player = document().timeline().play(animation);
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(100);
    EXPECT_EQ(50, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(149);
    EXPECT_EQ(1, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(150);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, TimeToEffectChangeWithNegativePlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = -2;
    timing.fillMode = Timing::FillModeNone;
    KeyframeEffect* animation = KeyframeEffect::create(0, nullptr, timing);
    Animation* player = document().timeline().play(animation);
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(100);
    EXPECT_EQ(50, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(149);
    EXPECT_EQ(1, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(150);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTimeInternal(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

TEST_F(KeyframeEffectTest, ElementDestructorClearsAnimationTarget)
{
    // This test expects incorrect behaviour should be removed once Element
    // and KeyframeEffect are moved to Oilpan. See crbug.com/362404 for context.
    Timing timing;
    timing.iterationDuration = 5;
    KeyframeEffect* animation = KeyframeEffect::create(element.get(), nullptr, timing);
    EXPECT_EQ(element.get(), animation->target());
    document().timeline().play(animation);
    pageHolder.reset();
    element.clear();
}

} // namespace blink
