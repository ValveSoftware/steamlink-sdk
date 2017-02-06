// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationStack.h"

#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorPendingAnimations.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class AnimationAnimationStackTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        pageHolder = DummyPageHolder::create();
        document = &pageHolder->document();
        document->animationClock().resetTimeForTesting();
        timeline = AnimationTimeline::create(document.get());
        element = document->createElement("foo", ASSERT_NO_EXCEPTION);
    }

    Animation* play(KeyframeEffect* effect, double startTime)
    {
        Animation* animation = timeline->play(effect);
        animation->setStartTime(startTime * 1000);
        animation->update(TimingUpdateOnDemand);
        return animation;
    }

    void updateTimeline(double time)
    {
        document->animationClock().updateTime(document->timeline().zeroTime() + time);
        timeline->serviceAnimations(TimingUpdateForAnimationFrame);
    }

    size_t sampledEffectCount()
    {
        return element->ensureElementAnimations().animationStack().m_sampledEffects.size();
    }

    EffectModel* makeEffectModel(CSSPropertyID id, PassRefPtr<AnimatableValue> value)
    {
        AnimatableValueKeyframeVector keyframes(2);
        keyframes[0] = AnimatableValueKeyframe::create();
        keyframes[0]->setOffset(0.0);
        keyframes[0]->setPropertyValue(id, value.get());
        keyframes[1] = AnimatableValueKeyframe::create();
        keyframes[1]->setOffset(1.0);
        keyframes[1]->setPropertyValue(id, value.get());
        return AnimatableValueKeyframeEffectModel::create(keyframes);
    }

    InertEffect* makeInertEffect(EffectModel* effect)
    {
        Timing timing;
        timing.fillMode = Timing::FillModeBoth;
        return InertEffect::create(effect, timing, false, 0);
    }

    KeyframeEffect* makeKeyframeEffect(EffectModel* effect, double duration = 10)
    {
        Timing timing;
        timing.fillMode = Timing::FillModeBoth;
        timing.iterationDuration = duration;
        return KeyframeEffect::create(element.get(), effect, timing);
    }

    AnimatableValue* interpolationValue(const ActiveInterpolationsMap& activeInterpolations, CSSPropertyID id)
    {
        Interpolation& interpolation = *activeInterpolations.get(PropertyHandle(id)).at(0);
        return toLegacyStyleInterpolation(interpolation).currentValue().get();
    }

    std::unique_ptr<DummyPageHolder> pageHolder;
    Persistent<Document> document;
    Persistent<AnimationTimeline> timeline;
    Persistent<Element> element;
};

TEST_F(AnimationAnimationStackTest, ElementAnimationsSorted)
{
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))), 10);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(2))), 15);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(3))), 5);
    ActiveInterpolationsMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), 0, 0, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
}

TEST_F(AnimationAnimationStackTest, NewAnimations)
{
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))), 15);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyZIndex, AnimatableDouble::create(2))), 10);
    HeapVector<Member<const InertEffect>> newAnimations;
    InertEffect* inert1 = makeInertEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(3)));
    InertEffect* inert2 = makeInertEffect(makeEffectModel(CSSPropertyZIndex, AnimatableDouble::create(4)));
    newAnimations.append(inert1);
    newAnimations.append(inert2);
    ActiveInterpolationsMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), &newAnimations, 0, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(2u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
    EXPECT_TRUE(interpolationValue(result, CSSPropertyZIndex)->equals(AnimatableDouble::create(4).get()));
}

TEST_F(AnimationAnimationStackTest, CancelledAnimations)
{
    HeapHashSet<Member<const Animation>> cancelledAnimations;
    Animation* animation = play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))), 0);
    cancelledAnimations.add(animation);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyZIndex, AnimatableDouble::create(2))), 0);
    ActiveInterpolationsMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), 0, &cancelledAnimations, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyZIndex)->equals(AnimatableDouble::create(2).get()));
}

TEST_F(AnimationAnimationStackTest, ClearedEffectsRemoved)
{
    Animation* animation = play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))), 10);
    ActiveInterpolationsMap result = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), 0, 0, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(1u, result.size());
    EXPECT_TRUE(interpolationValue(result, CSSPropertyFontSize)->equals(AnimatableDouble::create(1).get()));

    animation->setEffect(0);
    result = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), 0, 0, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(0u, result.size());
}

TEST_F(AnimationAnimationStackTest, ForwardsFillDiscarding)
{
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(1))), 2);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(2))), 6);
    play(makeKeyframeEffect(makeEffectModel(CSSPropertyFontSize, AnimatableDouble::create(3))), 4);
    document->compositorPendingAnimations().update();
    ActiveInterpolationsMap interpolations;

    updateTimeline(11);
    ThreadHeap::collectAllGarbage();
    interpolations = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), nullptr, nullptr, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(1u, interpolations.size());
    EXPECT_TRUE(interpolationValue(interpolations, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
    EXPECT_EQ(3u, sampledEffectCount());

    updateTimeline(13);
    ThreadHeap::collectAllGarbage();
    interpolations = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), nullptr, nullptr, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(1u, interpolations.size());
    EXPECT_TRUE(interpolationValue(interpolations, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
    EXPECT_EQ(3u, sampledEffectCount());

    updateTimeline(15);
    ThreadHeap::collectAllGarbage();
    interpolations = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), nullptr, nullptr, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(1u, interpolations.size());
    EXPECT_TRUE(interpolationValue(interpolations, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
    EXPECT_EQ(2u, sampledEffectCount());

    updateTimeline(17);
    ThreadHeap::collectAllGarbage();
    interpolations = AnimationStack::activeInterpolations(&element->elementAnimations()->animationStack(), nullptr, nullptr, KeyframeEffect::DefaultPriority);
    EXPECT_EQ(1u, interpolations.size());
    EXPECT_TRUE(interpolationValue(interpolations, CSSPropertyFontSize)->equals(AnimatableDouble::create(3).get()));
    EXPECT_EQ(1u, sampledEffectCount());
}

} // namespace blink
