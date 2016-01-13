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

#include "config.h"
#include "core/animation/AnimationPlayer.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationTimeline.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "platform/weborigin/KURL.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class AnimationAnimationPlayerTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        setUpWithoutStartingTimeline();
        startTimeline();
    }

    void setUpWithoutStartingTimeline()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        timeline = AnimationTimeline::create(document.get());
        player = timeline->createAnimationPlayer(0);
        player->setStartTimeInternal(0);
        player->setSource(makeAnimation().get());
    }

    void startTimeline()
    {
        updateTimeline(0);
    }

    PassRefPtrWillBeRawPtr<Animation> makeAnimation(double duration = 30, double playbackRate = 1)
    {
        Timing timing;
        timing.iterationDuration = duration;
        timing.playbackRate = playbackRate;
        return Animation::create(0, nullptr, timing);
    }

    bool updateTimeline(double time)
    {
        document->animationClock().updateTime(time);
        // The timeline does not know about our player, so we have to explicitly call update().
        return player->update(TimingUpdateOnDemand);
    }

    RefPtrWillBePersistent<Document> document;
    RefPtrWillBePersistent<AnimationTimeline> timeline;
    RefPtrWillBePersistent<AnimationPlayer> player;
    TrackExceptionState exceptionState;
};

TEST_F(AnimationAnimationPlayerTest, InitialState)
{
    setUpWithoutStartingTimeline();
    player = timeline->createAnimationPlayer(0);
    EXPECT_EQ(0, player->currentTimeInternal());
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(1, player->playbackRate());
    EXPECT_EQ(0, player->timeLagInternal());
    EXPECT_FALSE(player->hasStartTime());
    EXPECT_TRUE(isNull(player->startTimeInternal()));

    startTimeline();
    player->setStartTimeInternal(0);
    EXPECT_EQ(0, timeline->currentTimeInternal());
    EXPECT_EQ(0, player->currentTimeInternal());
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(1, player->playbackRate());
    EXPECT_EQ(0, player->startTimeInternal());
    EXPECT_EQ(0, player->timeLagInternal());
    EXPECT_TRUE(player->hasStartTime());
}


TEST_F(AnimationAnimationPlayerTest, CurrentTimeDoesNotSetOutdated)
{
    EXPECT_FALSE(player->outdated());
    EXPECT_EQ(0, player->currentTimeInternal());
    EXPECT_FALSE(player->outdated());
    // FIXME: We should split updateTimeline into a version that doesn't update
    // the player and one that does, as most of the tests don't require update()
    // to be called.
    document->animationClock().updateTime(10);
    EXPECT_EQ(10, player->currentTimeInternal());
    EXPECT_FALSE(player->outdated());
}

TEST_F(AnimationAnimationPlayerTest, SetCurrentTime)
{
    player->setCurrentTimeInternal(10);
    EXPECT_EQ(10, player->currentTimeInternal());
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetCurrentTimeNegative)
{
    player->setCurrentTimeInternal(-10);
    EXPECT_EQ(-10, player->currentTimeInternal());
    updateTimeline(20);
    EXPECT_EQ(10, player->currentTimeInternal());

    player->setPlaybackRate(-2);
    player->setCurrentTimeInternal(-10);
    EXPECT_EQ(-10, player->currentTimeInternal());
    updateTimeline(40);
    EXPECT_EQ(-10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetCurrentTimePastContentEnd)
{
    player->setCurrentTimeInternal(50);
    EXPECT_EQ(50, player->currentTimeInternal());
    updateTimeline(20);
    EXPECT_EQ(50, player->currentTimeInternal());

    player->setPlaybackRate(-2);
    player->setCurrentTimeInternal(50);
    EXPECT_EQ(50, player->currentTimeInternal());
    updateTimeline(40);
    EXPECT_EQ(10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetCurrentTimeBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->setCurrentTimeInternal(5);
    EXPECT_EQ(5, player->currentTimeInternal());
    startTimeline();
    updateTimeline(10);
    EXPECT_EQ(15, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetCurrentTimePastContentEndBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->setCurrentTimeInternal(250);
    EXPECT_EQ(250, player->currentTimeInternal());
    startTimeline();
    updateTimeline(10);
    EXPECT_EQ(250, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetCurrentTimeMax)
{
    player->setCurrentTimeInternal(std::numeric_limits<double>::max());
    EXPECT_EQ(std::numeric_limits<double>::max(), player->currentTimeInternal());
    updateTimeline(100);
    EXPECT_EQ(std::numeric_limits<double>::max(), player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetCurrentTimeUnrestrictedDouble)
{
    updateTimeline(10);
    player->setCurrentTimeInternal(nullValue());
    EXPECT_EQ(10, player->currentTimeInternal());
    player->setCurrentTimeInternal(std::numeric_limits<double>::infinity());
    EXPECT_EQ(10, player->currentTimeInternal());
    player->setCurrentTimeInternal(-std::numeric_limits<double>::infinity());
    EXPECT_EQ(10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, TimeLag)
{
    player->setCurrentTimeInternal(10);
    EXPECT_EQ(-10, player->timeLagInternal());
    updateTimeline(10);
    EXPECT_EQ(-10, player->timeLagInternal());
    player->setCurrentTimeInternal(40);
    EXPECT_EQ(-30, player->timeLagInternal());
    updateTimeline(20);
    EXPECT_EQ(-20, player->timeLagInternal());
}


TEST_F(AnimationAnimationPlayerTest, SetStartTime)
{
    updateTimeline(20);
    EXPECT_EQ(0, player->startTimeInternal());
    EXPECT_EQ(20, player->currentTimeInternal());
    player->setStartTimeInternal(10);
    EXPECT_EQ(10, player->startTimeInternal());
    EXPECT_EQ(10, player->currentTimeInternal());
    updateTimeline(30);
    EXPECT_EQ(10, player->startTimeInternal());
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetStartTimeLimitsAnimationPlayer)
{
    player->setStartTimeInternal(-50);
    EXPECT_EQ(30, player->currentTimeInternal());
    player->setPlaybackRate(-1);
    player->setStartTimeInternal(-100);
    EXPECT_EQ(0, player->currentTimeInternal());
    EXPECT_TRUE(player->finished());
}

TEST_F(AnimationAnimationPlayerTest, SetStartTimeOnLimitedAnimationPlayer)
{
    updateTimeline(30);
    player->setStartTimeInternal(-10);
    EXPECT_EQ(30, player->currentTimeInternal());
    player->setCurrentTimeInternal(50);
    player->setStartTimeInternal(-40);
    EXPECT_EQ(50, player->currentTimeInternal());
    EXPECT_TRUE(player->finished());
}

TEST_F(AnimationAnimationPlayerTest, SetStartTimeWhilePaused)
{
    updateTimeline(10);
    player->pause();
    player->setStartTimeInternal(-40);
    EXPECT_EQ(10, player->currentTimeInternal());
    updateTimeline(50);
    player->setStartTimeInternal(60);
    EXPECT_EQ(10, player->currentTimeInternal());
}


TEST_F(AnimationAnimationPlayerTest, PausePlay)
{
    updateTimeline(10);
    player->pause();
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(10, player->currentTimeInternal());
    updateTimeline(20);
    player->play();
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(10, player->currentTimeInternal());
    updateTimeline(30);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, PauseBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->pause();
    EXPECT_TRUE(player->paused());
    player->play();
    EXPECT_FALSE(player->paused());

    player->pause();
    startTimeline();
    updateTimeline(100);
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(0, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, PauseBeforeStartTimeSet)
{
    player = timeline->createAnimationPlayer(makeAnimation().get());
    updateTimeline(100);
    player->pause();
    updateTimeline(200);
    EXPECT_EQ(0, player->currentTimeInternal());

    player->setStartTimeInternal(150);
    player->play();
    EXPECT_EQ(0, player->currentTimeInternal());
    updateTimeline(220);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, PlayRewindsToStart)
{
    player->setCurrentTimeInternal(30);
    player->play();
    EXPECT_EQ(0, player->currentTimeInternal());

    player->setCurrentTimeInternal(40);
    player->play();
    EXPECT_EQ(0, player->currentTimeInternal());

    player->setCurrentTimeInternal(-10);
    player->play();
    EXPECT_EQ(0, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, PlayRewindsToEnd)
{
    player->setPlaybackRate(-1);
    player->play();
    EXPECT_EQ(30, player->currentTimeInternal());

    player->setCurrentTimeInternal(40);
    player->play();
    EXPECT_EQ(30, player->currentTimeInternal());

    player->setCurrentTimeInternal(-10);
    player->play();
    EXPECT_EQ(30, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, PlayWithPlaybackRateZeroDoesNotSeek)
{
    player->setPlaybackRate(0);
    player->play();
    EXPECT_EQ(0, player->currentTimeInternal());

    player->setCurrentTimeInternal(40);
    player->play();
    EXPECT_EQ(40, player->currentTimeInternal());

    player->setCurrentTimeInternal(-10);
    player->play();
    EXPECT_EQ(-10, player->currentTimeInternal());
}


TEST_F(AnimationAnimationPlayerTest, Reverse)
{
    player->setCurrentTimeInternal(10);
    player->pause();
    player->reverse();
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(-1, player->playbackRate());
    EXPECT_EQ(10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, ReverseDoesNothingWithPlaybackRateZero)
{
    player->setCurrentTimeInternal(10);
    player->setPlaybackRate(0);
    player->pause();
    player->reverse();
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(0, player->playbackRate());
    EXPECT_EQ(10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, ReverseDoesNotSeekWithNoSource)
{
    player->setSource(0);
    player->setCurrentTimeInternal(10);
    player->reverse();
    EXPECT_EQ(10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, ReverseSeeksToStart)
{
    player->setCurrentTimeInternal(-10);
    player->setPlaybackRate(-1);
    player->reverse();
    EXPECT_EQ(0, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, ReverseSeeksToEnd)
{
    player->setCurrentTimeInternal(40);
    player->reverse();
    EXPECT_EQ(30, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, ReverseLimitsAnimationPlayer)
{
    player->setCurrentTimeInternal(40);
    player->setPlaybackRate(-1);
    player->reverse();
    EXPECT_TRUE(player->finished());
    EXPECT_EQ(40, player->currentTimeInternal());

    player->setCurrentTimeInternal(-10);
    player->reverse();
    EXPECT_TRUE(player->finished());
    EXPECT_EQ(-10, player->currentTimeInternal());
}


TEST_F(AnimationAnimationPlayerTest, Finish)
{
    player->finish(exceptionState);
    EXPECT_EQ(30, player->currentTimeInternal());
    EXPECT_TRUE(player->finished());

    player->setPlaybackRate(-1);
    player->finish(exceptionState);
    EXPECT_EQ(0, player->currentTimeInternal());
    EXPECT_TRUE(player->finished());

    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(AnimationAnimationPlayerTest, FinishAfterSourceEnd)
{
    player->setCurrentTimeInternal(40);
    player->finish(exceptionState);
    EXPECT_EQ(30, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, FinishBeforeStart)
{
    player->setCurrentTimeInternal(-10);
    player->setPlaybackRate(-1);
    player->finish(exceptionState);
    EXPECT_EQ(0, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, FinishDoesNothingWithPlaybackRateZero)
{
    player->setCurrentTimeInternal(10);
    player->setPlaybackRate(0);
    player->finish(exceptionState);
    EXPECT_EQ(10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, FinishRaisesException)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    player->setSource(Animation::create(0, nullptr, timing).get());
    player->setCurrentTimeInternal(10);

    player->finish(exceptionState);
    EXPECT_EQ(10, player->currentTimeInternal());
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(InvalidStateError, exceptionState.code());
}


TEST_F(AnimationAnimationPlayerTest, LimitingAtSourceEnd)
{
    updateTimeline(30);
    EXPECT_EQ(30, player->currentTimeInternal());
    EXPECT_TRUE(player->finished());
    updateTimeline(40);
    EXPECT_EQ(30, player->currentTimeInternal());
    EXPECT_FALSE(player->paused());
}

TEST_F(AnimationAnimationPlayerTest, LimitingAtStart)
{
    updateTimeline(30);
    player->setPlaybackRate(-2);
    updateTimeline(45);
    EXPECT_EQ(0, player->currentTimeInternal());
    EXPECT_TRUE(player->finished());
    updateTimeline(60);
    EXPECT_EQ(0, player->currentTimeInternal());
    EXPECT_FALSE(player->paused());
}

TEST_F(AnimationAnimationPlayerTest, LimitingWithNoSource)
{
    player->setSource(0);
    EXPECT_TRUE(player->finished());
    updateTimeline(30);
    EXPECT_EQ(0, player->currentTimeInternal());
}


TEST_F(AnimationAnimationPlayerTest, SetPlaybackRate)
{
    player->setPlaybackRate(2);
    EXPECT_EQ(2, player->playbackRate());
    EXPECT_EQ(0, player->currentTimeInternal());
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetPlaybackRateBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->setPlaybackRate(2);
    EXPECT_EQ(2, player->playbackRate());
    EXPECT_EQ(0, player->currentTimeInternal());
    startTimeline();
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetPlaybackRateWhilePaused)
{
    updateTimeline(10);
    player->pause();
    player->setPlaybackRate(2);
    updateTimeline(20);
    player->play();
    EXPECT_EQ(10, player->currentTimeInternal());
    updateTimeline(25);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetPlaybackRateWhileLimited)
{
    updateTimeline(40);
    EXPECT_EQ(30, player->currentTimeInternal());
    player->setPlaybackRate(2);
    updateTimeline(50);
    EXPECT_EQ(30, player->currentTimeInternal());
    player->setPlaybackRate(-2);
    EXPECT_FALSE(player->finished());
    updateTimeline(60);
    EXPECT_EQ(10, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetPlaybackRateZero)
{
    updateTimeline(10);
    player->setPlaybackRate(0);
    EXPECT_EQ(10, player->currentTimeInternal());
    updateTimeline(20);
    EXPECT_EQ(10, player->currentTimeInternal());
    player->setCurrentTimeInternal(20);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetPlaybackRateMax)
{
    player->setPlaybackRate(std::numeric_limits<double>::max());
    EXPECT_EQ(std::numeric_limits<double>::max(), player->playbackRate());
    EXPECT_EQ(0, player->currentTimeInternal());
    updateTimeline(1);
    EXPECT_EQ(30, player->currentTimeInternal());
}


TEST_F(AnimationAnimationPlayerTest, SetSource)
{
    player = timeline->createAnimationPlayer(0);
    player->setStartTimeInternal(0);
    RefPtrWillBeRawPtr<AnimationNode> source1 = makeAnimation();
    RefPtrWillBeRawPtr<AnimationNode> source2 = makeAnimation();
    player->setSource(source1.get());
    EXPECT_EQ(source1, player->source());
    EXPECT_EQ(0, player->currentTimeInternal());
    player->setCurrentTimeInternal(15);
    player->setSource(source2.get());
    EXPECT_EQ(15, player->currentTimeInternal());
    EXPECT_EQ(0, source1->player());
    EXPECT_EQ(player.get(), source2->player());
    EXPECT_EQ(source2, player->source());
}

TEST_F(AnimationAnimationPlayerTest, SetSourceLimitsAnimationPlayer)
{
    player->setCurrentTimeInternal(20);
    player->setSource(makeAnimation(10).get());
    EXPECT_EQ(20, player->currentTimeInternal());
    EXPECT_TRUE(player->finished());
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTimeInternal());
}

TEST_F(AnimationAnimationPlayerTest, SetSourceUnlimitsAnimationPlayer)
{
    player->setCurrentTimeInternal(40);
    player->setSource(makeAnimation(60).get());
    EXPECT_FALSE(player->finished());
    EXPECT_EQ(40, player->currentTimeInternal());
    updateTimeline(10);
    EXPECT_EQ(50, player->currentTimeInternal());
}


TEST_F(AnimationAnimationPlayerTest, EmptyAnimationPlayersDontUpdateEffects)
{
    player = timeline->createAnimationPlayer(0);
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    updateTimeline(1234);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());
}

TEST_F(AnimationAnimationPlayerTest, AnimationPlayersDisassociateFromSource)
{
    AnimationNode* animationNode = player->source();
    AnimationPlayer* player2 = timeline->createAnimationPlayer(animationNode);
    EXPECT_EQ(0, player->source());
    player->setSource(animationNode);
    EXPECT_EQ(0, player2->source());
}

TEST_F(AnimationAnimationPlayerTest, AnimationPlayersReturnTimeToNextEffect)
{
    Timing timing;
    timing.startDelay = 1;
    timing.iterationDuration = 1;
    timing.endDelay = 1;
    RefPtrWillBeRawPtr<Animation> animation = Animation::create(0, nullptr, timing);
    player = timeline->createAnimationPlayer(animation.get());
    player->setStartTimeInternal(0);

    updateTimeline(0);
    EXPECT_EQ(1, player->timeToEffectChange());

    updateTimeline(0.5);
    EXPECT_EQ(0.5, player->timeToEffectChange());

    updateTimeline(1);
    EXPECT_EQ(0, player->timeToEffectChange());

    updateTimeline(1.5);
    EXPECT_EQ(0, player->timeToEffectChange());

    updateTimeline(2);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    updateTimeline(3);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    player->setCurrentTimeInternal(0);
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(1, player->timeToEffectChange());

    player->setPlaybackRate(2);
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(0.5, player->timeToEffectChange());

    player->setPlaybackRate(0);
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    player->setCurrentTimeInternal(3);
    player->setPlaybackRate(-1);
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(1, player->timeToEffectChange());

    player->setPlaybackRate(-2);
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(0.5, player->timeToEffectChange());
}

TEST_F(AnimationAnimationPlayerTest, TimeToNextEffectWhenPaused)
{
    EXPECT_EQ(0, player->timeToEffectChange());
    player->pause();
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());
}

TEST_F(AnimationAnimationPlayerTest, TimeToNextEffectWhenCancelledBeforeStart)
{
    EXPECT_EQ(0, player->timeToEffectChange());
    player->setCurrentTimeInternal(-8);
    player->setPlaybackRate(2);
    player->cancel();
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(4, player->timeToEffectChange());
}

TEST_F(AnimationAnimationPlayerTest, TimeToNextEffectWhenCancelledBeforeStartReverse)
{
    EXPECT_EQ(0, player->timeToEffectChange());
    player->setCurrentTimeInternal(9);
    player->setPlaybackRate(-3);
    player->cancel();
    player->update(TimingUpdateOnDemand);
    EXPECT_EQ(3, player->timeToEffectChange());
}

TEST_F(AnimationAnimationPlayerTest, AttachedAnimationPlayers)
{
    RefPtrWillBePersistent<Element> element = document->createElement("foo", ASSERT_NO_EXCEPTION);

    Timing timing;
    RefPtrWillBeRawPtr<Animation> animation = Animation::create(element.get(), nullptr, timing);
    RefPtrWillBeRawPtr<AnimationPlayer> player = timeline->createAnimationPlayer(animation.get());
    player->setStartTime(0);
    timeline->serviceAnimations(TimingUpdateForAnimationFrame);
    EXPECT_EQ(1, element->activeAnimations()->players().find(player.get())->value);

    player.release();
    Heap::collectAllGarbage();
    EXPECT_TRUE(element->activeAnimations()->players().isEmpty());
}

TEST_F(AnimationAnimationPlayerTest, HasLowerPriority)
{
    // Sort time defaults to timeline current time
    updateTimeline(15);
    RefPtrWillBeRawPtr<AnimationPlayer> player1 = timeline->createAnimationPlayer(0);
    RefPtrWillBeRawPtr<AnimationPlayer> player2 = timeline->createAnimationPlayer(0);
    player2->setStartTimeInternal(10);
    RefPtrWillBeRawPtr<AnimationPlayer> player3 = timeline->createAnimationPlayer(0);
    RefPtrWillBeRawPtr<AnimationPlayer> player4 = timeline->createAnimationPlayer(0);
    player4->setStartTimeInternal(20);
    RefPtrWillBeRawPtr<AnimationPlayer> player5 = timeline->createAnimationPlayer(0);
    player5->setStartTimeInternal(10);
    RefPtrWillBeRawPtr<AnimationPlayer> player6 = timeline->createAnimationPlayer(0);
    player6->setStartTimeInternal(-10);
    Vector<RefPtrWillBeMember<AnimationPlayer> > players;
    players.append(player6);
    players.append(player2);
    players.append(player5);
    players.append(player1);
    players.append(player3);
    players.append(player4);
    for (size_t i = 0; i < players.size(); i++) {
        for (size_t j = 0; j < players.size(); j++)
            EXPECT_EQ(i < j, AnimationPlayer::hasLowerPriority(players[i].get(), players[j].get()));
    }
}

}
