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

#include "core/animation/Animation.h"

#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationTimeline.h"
#include "core/animation/CompositorPendingAnimations.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffect.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/weborigin/KURL.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class AnimationAnimationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        setUpWithoutStartingTimeline();
        startTimeline();
    }

    void setUpWithoutStartingTimeline()
    {
        pageHolder = DummyPageHolder::create();
        document = &pageHolder->document();
        document->animationClock().resetTimeForTesting();
        timeline = AnimationTimeline::create(document.get());
        timeline->resetForTesting();
        animation = timeline->play(0);
        animation->setStartTime(0);
        animation->setEffect(makeAnimation());
    }

    void startTimeline()
    {
        simulateFrame(0);
    }

    KeyframeEffect* makeAnimation(double duration = 30, double playbackRate = 1)
    {
        Timing timing;
        timing.iterationDuration = duration;
        timing.playbackRate = playbackRate;
        return KeyframeEffect::create(0, nullptr, timing);
    }

    bool simulateFrame(double time)
    {
        document->animationClock().updateTime(time);
        document->compositorPendingAnimations().update(false);
        // The timeline does not know about our animation, so we have to explicitly call update().
        return animation->update(TimingUpdateForAnimationFrame);
    }

    Persistent<Document> document;
    Persistent<AnimationTimeline> timeline;
    Persistent<Animation> animation;
    TrackExceptionState exceptionState;
    std::unique_ptr<DummyPageHolder> pageHolder;
};

TEST_F(AnimationAnimationTest, InitialState)
{
    setUpWithoutStartingTimeline();
    animation = timeline->play(0);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_FALSE(animation->paused());
    EXPECT_EQ(1, animation->playbackRate());
    EXPECT_FALSE(animation->hasStartTime());
    EXPECT_TRUE(isNull(animation->startTimeInternal()));

    startTimeline();
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(0, timeline->currentTimeInternal());
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_FALSE(animation->paused());
    EXPECT_EQ(1, animation->playbackRate());
    EXPECT_EQ(0, animation->startTimeInternal());
    EXPECT_TRUE(animation->hasStartTime());
}

TEST_F(AnimationAnimationTest, CurrentTimeDoesNotSetOutdated)
{
    EXPECT_FALSE(animation->outdated());
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_FALSE(animation->outdated());
    // FIXME: We should split simulateFrame into a version that doesn't update
    // the animation and one that does, as most of the tests don't require update()
    // to be called.
    document->animationClock().updateTime(10);
    EXPECT_EQ(10, animation->currentTimeInternal());
    EXPECT_FALSE(animation->outdated());
}

TEST_F(AnimationAnimationTest, SetCurrentTime)
{
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    animation->setCurrentTimeInternal(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(10, animation->currentTimeInternal());
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(20, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetCurrentTimeNegative)
{
    animation->setCurrentTimeInternal(-10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(-10, animation->currentTimeInternal());
    simulateFrame(20);
    EXPECT_EQ(10, animation->currentTimeInternal());

    animation->setPlaybackRate(-2);
    animation->setCurrentTimeInternal(-10);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(-10, animation->currentTimeInternal());
    simulateFrame(40);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(-10, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetCurrentTimeNegativeWithoutSimultaneousPlaybackRateChange)
{
    simulateFrame(20);
    EXPECT_EQ(20, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    animation->setPlaybackRate(-1);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    simulateFrame(30);
    EXPECT_EQ(20, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    animation->setCurrentTime(-10 * 1000);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, SetCurrentTimePastContentEnd)
{
    animation->setCurrentTime(50 * 1000);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(50, animation->currentTimeInternal());
    simulateFrame(20);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(50, animation->currentTimeInternal());

    animation->setPlaybackRate(-2);
    animation->setCurrentTime(50 * 1000);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(50, animation->currentTimeInternal());
    simulateFrame(20);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    simulateFrame(40);
    EXPECT_EQ(10, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetCurrentTimeMax)
{
    animation->setCurrentTimeInternal(std::numeric_limits<double>::max());
    EXPECT_EQ(std::numeric_limits<double>::max(), animation->currentTimeInternal());
    simulateFrame(100);
    EXPECT_EQ(std::numeric_limits<double>::max(), animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetCurrentTimeSetsStartTime)
{
    EXPECT_EQ(0, animation->startTime());
    animation->setCurrentTime(1000);
    EXPECT_EQ(-1000, animation->startTime());
    simulateFrame(1);
    EXPECT_EQ(-1000, animation->startTime());
    EXPECT_EQ(2000, animation->currentTime());
}

TEST_F(AnimationAnimationTest, SetStartTime)
{
    simulateFrame(20);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(0, animation->startTimeInternal());
    EXPECT_EQ(20 * 1000, animation->currentTime());
    animation->setStartTime(10 * 1000);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(10, animation->startTimeInternal());
    EXPECT_EQ(10 * 1000, animation->currentTime());
    simulateFrame(30);
    EXPECT_EQ(10, animation->startTimeInternal());
    EXPECT_EQ(20 * 1000, animation->currentTime());
    animation->setStartTime(-20 * 1000);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, SetStartTimeLimitsAnimation)
{
    animation->setStartTime(-50 * 1000);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(30, animation->currentTimeInternal());
    animation->setPlaybackRate(-1);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    animation->setStartTime(-100 * 1000);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_TRUE(animation->limited());
}

TEST_F(AnimationAnimationTest, SetStartTimeOnLimitedAnimation)
{
    simulateFrame(30);
    animation->setStartTime(-10 * 1000);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(30, animation->currentTimeInternal());
    animation->setCurrentTimeInternal(50);
    animation->setStartTime(-40 * 1000);
    EXPECT_EQ(30, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_TRUE(animation->limited());
}

TEST_F(AnimationAnimationTest, StartTimePauseFinish)
{
    animation->pause();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_TRUE(std::isnan(animation->startTime()));
    animation->finish(exceptionState);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
    EXPECT_EQ(-30000, animation->startTime());
}

TEST_F(AnimationAnimationTest, FinishWhenPaused)
{
    animation->pause();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    simulateFrame(10);
    EXPECT_EQ(Animation::Paused, animation->playStateInternal());
    animation->finish(exceptionState);
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, StartTimeFinishPause)
{
    animation->finish(exceptionState);
    EXPECT_EQ(-30 * 1000, animation->startTime());
    animation->pause();
    EXPECT_TRUE(std::isnan(animation->startTime()));
}

TEST_F(AnimationAnimationTest, StartTimeWithZeroPlaybackRate)
{
    animation->setPlaybackRate(0);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_TRUE(std::isnan(animation->startTime()));
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, PausePlay)
{
    simulateFrame(10);
    animation->pause();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_TRUE(animation->paused());
    EXPECT_EQ(10, animation->currentTimeInternal());
    simulateFrame(20);
    EXPECT_EQ(Animation::Paused, animation->playStateInternal());
    animation->play();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    simulateFrame(20);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_FALSE(animation->paused());
    EXPECT_EQ(10, animation->currentTimeInternal());
    simulateFrame(30);
    EXPECT_EQ(20, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, PlayRewindsToStart)
{
    animation->setCurrentTimeInternal(30);
    animation->play();
    EXPECT_EQ(0, animation->currentTimeInternal());

    animation->setCurrentTimeInternal(40);
    animation->play();
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());

    animation->setCurrentTimeInternal(-10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    animation->play();
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, PlayRewindsToEnd)
{
    animation->setPlaybackRate(-1);
    animation->play();
    EXPECT_EQ(30, animation->currentTimeInternal());

    animation->setCurrentTimeInternal(40);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    animation->play();
    EXPECT_EQ(30, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());

    animation->setCurrentTimeInternal(-10);
    animation->play();
    EXPECT_EQ(30, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    simulateFrame(20);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, PlayWithPlaybackRateZeroDoesNotSeek)
{
    animation->setPlaybackRate(0);
    animation->play();
    EXPECT_EQ(0, animation->currentTimeInternal());

    animation->setCurrentTimeInternal(40);
    animation->play();
    EXPECT_EQ(40, animation->currentTimeInternal());

    animation->setCurrentTimeInternal(-10);
    animation->play();
    EXPECT_EQ(-10, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, PlayAfterPauseWithPlaybackRateZeroUpdatesPlayState)
{
    animation->pause();
    animation->setPlaybackRate(0);
    simulateFrame(1);
    EXPECT_EQ(Animation::Paused, animation->playStateInternal());
    animation->play();
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, Reverse)
{
    animation->setCurrentTimeInternal(10);
    animation->pause();
    animation->reverse();
    EXPECT_FALSE(animation->paused());
    EXPECT_EQ(-1, animation->playbackRate());
    EXPECT_EQ(10, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, ReverseDoesNothingWithPlaybackRateZero)
{
    animation->setCurrentTimeInternal(10);
    animation->setPlaybackRate(0);
    animation->pause();
    animation->reverse();
    EXPECT_TRUE(animation->paused());
    EXPECT_EQ(0, animation->playbackRate());
    EXPECT_EQ(10, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, ReverseSeeksToStart)
{
    animation->setCurrentTimeInternal(-10);
    animation->setPlaybackRate(-1);
    animation->reverse();
    EXPECT_EQ(0, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, ReverseSeeksToEnd)
{
    animation->setCurrentTime(40 * 1000);
    animation->reverse();
    EXPECT_EQ(30, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, ReverseBeyondLimit)
{
    animation->setCurrentTimeInternal(40);
    animation->setPlaybackRate(-1);
    animation->reverse();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(0, animation->currentTimeInternal());

    animation->setCurrentTimeInternal(-10);
    animation->reverse();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(30, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, Finish)
{
    animation->finish(exceptionState);
    EXPECT_EQ(30, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());

    animation->setPlaybackRate(-1);
    animation->finish(exceptionState);
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());

    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(AnimationAnimationTest, FinishAfterEffectEnd)
{
    animation->setCurrentTime(40 * 1000);
    animation->finish(exceptionState);
    EXPECT_EQ(40, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, FinishBeforeStart)
{
    animation->setCurrentTimeInternal(-10);
    animation->setPlaybackRate(-1);
    animation->finish(exceptionState);
    EXPECT_EQ(0, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, FinishDoesNothingWithPlaybackRateZero)
{
    animation->setCurrentTimeInternal(10);
    animation->setPlaybackRate(0);
    animation->finish(exceptionState);
    EXPECT_EQ(10, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, FinishRaisesException)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    animation->setEffect(KeyframeEffect::create(0, nullptr, timing));
    animation->setCurrentTimeInternal(10);

    animation->finish(exceptionState);
    EXPECT_EQ(10, animation->currentTimeInternal());
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(InvalidStateError, exceptionState.code());
}

TEST_F(AnimationAnimationTest, LimitingAtEffectEnd)
{
    simulateFrame(30);
    EXPECT_EQ(30, animation->currentTimeInternal());
    EXPECT_TRUE(animation->limited());
    simulateFrame(40);
    EXPECT_EQ(30, animation->currentTimeInternal());
    EXPECT_FALSE(animation->paused());
}

TEST_F(AnimationAnimationTest, LimitingAtStart)
{
    simulateFrame(30);
    animation->setPlaybackRate(-2);
    simulateFrame(30);
    simulateFrame(45);
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_TRUE(animation->limited());
    simulateFrame(60);
    EXPECT_EQ(0, animation->currentTimeInternal());
    EXPECT_FALSE(animation->paused());
}

TEST_F(AnimationAnimationTest, LimitingWithNoEffect)
{
    animation->setEffect(0);
    EXPECT_TRUE(animation->limited());
    simulateFrame(30);
    EXPECT_EQ(0, animation->currentTimeInternal());
}


TEST_F(AnimationAnimationTest, SetPlaybackRate)
{
    animation->setPlaybackRate(2);
    simulateFrame(0);
    EXPECT_EQ(2, animation->playbackRate());
    EXPECT_EQ(0, animation->currentTimeInternal());
    simulateFrame(10);
    EXPECT_EQ(20, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetPlaybackRateWhilePaused)
{
    simulateFrame(10);
    animation->pause();
    animation->setPlaybackRate(2);
    simulateFrame(20);
    animation->play();
    EXPECT_EQ(10, animation->currentTimeInternal());
    simulateFrame(20);
    simulateFrame(25);
    EXPECT_EQ(20, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetPlaybackRateWhileLimited)
{
    simulateFrame(40);
    EXPECT_EQ(30, animation->currentTimeInternal());
    animation->setPlaybackRate(2);
    simulateFrame(50);
    EXPECT_EQ(30, animation->currentTimeInternal());
    animation->setPlaybackRate(-2);
    simulateFrame(50);
    simulateFrame(60);
    EXPECT_FALSE(animation->limited());
    EXPECT_EQ(10, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetPlaybackRateZero)
{
    simulateFrame(0);
    simulateFrame(10);
    animation->setPlaybackRate(0);
    simulateFrame(10);
    EXPECT_EQ(10, animation->currentTimeInternal());
    simulateFrame(20);
    EXPECT_EQ(10, animation->currentTimeInternal());
    animation->setCurrentTimeInternal(20);
    EXPECT_EQ(20, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetPlaybackRateMax)
{
    animation->setPlaybackRate(std::numeric_limits<double>::max());
    simulateFrame(0);
    EXPECT_EQ(std::numeric_limits<double>::max(), animation->playbackRate());
    EXPECT_EQ(0, animation->currentTimeInternal());
    simulateFrame(1);
    EXPECT_EQ(30, animation->currentTimeInternal());
}


TEST_F(AnimationAnimationTest, SetEffect)
{
    animation = timeline->play(0);
    animation->setStartTime(0);
    AnimationEffect* effect1 = makeAnimation();
    AnimationEffect* effect2 = makeAnimation();
    animation->setEffect(effect1);
    EXPECT_EQ(effect1, animation->effect());
    EXPECT_EQ(0, animation->currentTimeInternal());
    animation->setCurrentTimeInternal(15);
    animation->setEffect(effect2);
    EXPECT_EQ(15, animation->currentTimeInternal());
    EXPECT_EQ(0, effect1->animation());
    EXPECT_EQ(animation, effect2->animation());
    EXPECT_EQ(effect2, animation->effect());
}

TEST_F(AnimationAnimationTest, SetEffectLimitsAnimation)
{
    animation->setCurrentTimeInternal(20);
    animation->setEffect(makeAnimation(10));
    EXPECT_EQ(20, animation->currentTimeInternal());
    EXPECT_TRUE(animation->limited());
    simulateFrame(10);
    EXPECT_EQ(20, animation->currentTimeInternal());
}

TEST_F(AnimationAnimationTest, SetEffectUnlimitsAnimation)
{
    animation->setCurrentTimeInternal(40);
    animation->setEffect(makeAnimation(60));
    EXPECT_FALSE(animation->limited());
    EXPECT_EQ(40, animation->currentTimeInternal());
    simulateFrame(10);
    EXPECT_EQ(50, animation->currentTimeInternal());
}


TEST_F(AnimationAnimationTest, EmptyAnimationsDontUpdateEffects)
{
    animation = timeline->play(0);
    animation->update(TimingUpdateOnDemand);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());

    simulateFrame(1234);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());
}

TEST_F(AnimationAnimationTest, AnimationsDisassociateFromEffect)
{
    AnimationEffect* animationNode = animation->effect();
    Animation* animation2 = timeline->play(animationNode);
    EXPECT_EQ(0, animation->effect());
    animation->setEffect(animationNode);
    EXPECT_EQ(0, animation2->effect());
}

TEST_F(AnimationAnimationTest, AnimationsReturnTimeToNextEffect)
{
    Timing timing;
    timing.startDelay = 1;
    timing.iterationDuration = 1;
    timing.endDelay = 1;
    KeyframeEffect* keyframeEffect = KeyframeEffect::create(0, nullptr, timing);
    animation = timeline->play(keyframeEffect);
    animation->setStartTime(0);

    simulateFrame(0);
    EXPECT_EQ(1, animation->timeToEffectChange());

    simulateFrame(0.5);
    EXPECT_EQ(0.5, animation->timeToEffectChange());

    simulateFrame(1);
    EXPECT_EQ(0, animation->timeToEffectChange());

    simulateFrame(1.5);
    EXPECT_EQ(0, animation->timeToEffectChange());

    simulateFrame(2);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());

    simulateFrame(3);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());

    animation->setCurrentTimeInternal(0);
    simulateFrame(3);
    EXPECT_EQ(1, animation->timeToEffectChange());

    animation->setPlaybackRate(2);
    simulateFrame(3);
    EXPECT_EQ(0.5, animation->timeToEffectChange());

    animation->setPlaybackRate(0);
    animation->update(TimingUpdateOnDemand);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());

    animation->setCurrentTimeInternal(3);
    animation->setPlaybackRate(-1);
    animation->update(TimingUpdateOnDemand);
    simulateFrame(3);
    EXPECT_EQ(1, animation->timeToEffectChange());

    animation->setPlaybackRate(-2);
    animation->update(TimingUpdateOnDemand);
    simulateFrame(3);
    EXPECT_EQ(0.5, animation->timeToEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToNextEffectWhenPaused)
{
    EXPECT_EQ(0, animation->timeToEffectChange());
    animation->pause();
    animation->update(TimingUpdateOnDemand);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToNextEffectWhenCancelledBeforeStart)
{
    EXPECT_EQ(0, animation->timeToEffectChange());
    animation->setCurrentTimeInternal(-8);
    animation->setPlaybackRate(2);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    animation->cancel();
    EXPECT_EQ(Animation::Idle, animation->playStateInternal());
    animation->update(TimingUpdateOnDemand);
    // This frame will fire the finish event event though no start time has been
    // received from the compositor yet, as cancel() nukes start times.
    simulateFrame(0);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToNextEffectWhenCancelledBeforeStartReverse)
{
    EXPECT_EQ(0, animation->timeToEffectChange());
    animation->setCurrentTimeInternal(9);
    animation->setPlaybackRate(-3);
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    animation->cancel();
    EXPECT_EQ(Animation::Idle, animation->playStateInternal());
    animation->update(TimingUpdateOnDemand);
    // This frame will fire the finish event event though no start time has been
    // received from the compositor yet, as cancel() nukes start times.
    simulateFrame(0);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToNextEffectSimpleCancelledBeforeStart)
{
    EXPECT_EQ(0, animation->timeToEffectChange());
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    animation->cancel();
    animation->update(TimingUpdateOnDemand);
    // This frame will fire the finish event event though no start time has been
    // received from the compositor yet, as cancel() nukes start times.
    simulateFrame(0);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), animation->timeToEffectChange());
}

TEST_F(AnimationAnimationTest, AttachedAnimations)
{
    Persistent<Element> element = document->createElement("foo", ASSERT_NO_EXCEPTION);

    Timing timing;
    KeyframeEffect* keyframeEffect = KeyframeEffect::create(element.get(), nullptr, timing);
    Animation* animation = timeline->play(keyframeEffect);
    simulateFrame(0);
    timeline->serviceAnimations(TimingUpdateForAnimationFrame);
    EXPECT_EQ(1U, element->elementAnimations()->animations().find(animation)->value);

    ThreadHeap::collectAllGarbage();
    EXPECT_TRUE(element->elementAnimations()->animations().isEmpty());
}

TEST_F(AnimationAnimationTest, HasLowerPriority)
{
    Animation* animation1 = timeline->play(0);
    Animation* animation2 = timeline->play(0);
    EXPECT_TRUE(Animation::hasLowerPriority(animation1, animation2));
}

TEST_F(AnimationAnimationTest, PlayAfterCancel)
{
    animation->cancel();
    EXPECT_EQ(Animation::Idle, animation->playStateInternal());
    EXPECT_TRUE(std::isnan(animation->currentTime()));
    EXPECT_TRUE(std::isnan(animation->startTime()));
    animation->play();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(0, animation->currentTime());
    EXPECT_TRUE(std::isnan(animation->startTime()));
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(0, animation->currentTime());
    EXPECT_EQ(10 * 1000, animation->startTime());
}

TEST_F(AnimationAnimationTest, PlayBackwardsAfterCancel)
{
    animation->setPlaybackRate(-1);
    animation->setCurrentTime(15 * 1000);
    simulateFrame(0);
    animation->cancel();
    EXPECT_EQ(Animation::Idle, animation->playStateInternal());
    EXPECT_TRUE(std::isnan(animation->currentTime()));
    EXPECT_TRUE(std::isnan(animation->startTime()));
    animation->play();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(30 * 1000, animation->currentTime());
    EXPECT_TRUE(std::isnan(animation->startTime()));
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(30 * 1000, animation->currentTime());
    EXPECT_EQ(40 * 1000, animation->startTime());
}

TEST_F(AnimationAnimationTest, ReverseAfterCancel)
{
    animation->cancel();
    EXPECT_EQ(Animation::Idle, animation->playStateInternal());
    EXPECT_TRUE(std::isnan(animation->currentTime()));
    EXPECT_TRUE(std::isnan(animation->startTime()));
    animation->reverse();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(30 * 1000, animation->currentTime());
    EXPECT_TRUE(std::isnan(animation->startTime()));
    simulateFrame(10);
    EXPECT_EQ(Animation::Running, animation->playStateInternal());
    EXPECT_EQ(30 * 1000, animation->currentTime());
    EXPECT_EQ(40 * 1000, animation->startTime());
}

TEST_F(AnimationAnimationTest, FinishAfterCancel)
{
    animation->cancel();
    EXPECT_EQ(Animation::Idle, animation->playStateInternal());
    EXPECT_TRUE(std::isnan(animation->currentTime()));
    EXPECT_TRUE(std::isnan(animation->startTime()));
    animation->finish(exceptionState);
    EXPECT_EQ(30000, animation->currentTime());
    EXPECT_EQ(-30000, animation->startTime());
    EXPECT_EQ(Animation::Finished, animation->playStateInternal());
}

TEST_F(AnimationAnimationTest, PauseAfterCancel)
{
    animation->cancel();
    EXPECT_EQ(Animation::Idle, animation->playStateInternal());
    EXPECT_TRUE(std::isnan(animation->currentTime()));
    EXPECT_TRUE(std::isnan(animation->startTime()));
    animation->pause();
    EXPECT_EQ(Animation::Pending, animation->playStateInternal());
    EXPECT_EQ(0, animation->currentTime());
    EXPECT_TRUE(std::isnan(animation->startTime()));
}

} // namespace blink
