// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "base/time/time.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "platform/testing/CompositorTest.h"

#include <memory>

namespace blink {

class CompositorAnimationDelegateForTesting
    : public CompositorAnimationDelegate {
 public:
  CompositorAnimationDelegateForTesting() { resetFlags(); }

  void resetFlags() {
    m_started = false;
    m_finished = false;
    m_aborted = false;
  }

  void notifyAnimationStarted(double, int) override { m_started = true; }
  void notifyAnimationFinished(double, int) override { m_finished = true; }
  void notifyAnimationAborted(double, int) override { m_aborted = true; }

  bool m_started;
  bool m_finished;
  bool m_aborted;
};

class CompositorAnimationPlayerTestClient
    : public CompositorAnimationPlayerClient {
 public:
  CompositorAnimationPlayerTestClient()
      : m_player(CompositorAnimationPlayer::create()) {}

  CompositorAnimationPlayer* compositorPlayer() const override {
    return m_player.get();
  }

  std::unique_ptr<CompositorAnimationPlayer> m_player;
};

class CompositorAnimationPlayerTest : public CompositorTest {};

// Test that when the animation delegate is null, the animation player
// doesn't forward the finish notification.
TEST_F(CompositorAnimationPlayerTest, NullDelegate) {
  std::unique_ptr<CompositorAnimationDelegateForTesting> delegate(
      new CompositorAnimationDelegateForTesting);

  std::unique_ptr<CompositorAnimationPlayer> player =
      CompositorAnimationPlayer::create();
  cc::AnimationPlayer* ccPlayer = player->ccAnimationPlayer();

  std::unique_ptr<CompositorAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(
      *curve, CompositorTargetProperty::TRANSFORM, 1, 0);
  player->addAnimation(std::move(animation));

  player->setAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->m_finished);

  ccPlayer->NotifyAnimationFinishedForTesting(
      CompositorTargetProperty::TRANSFORM, 1);
  EXPECT_TRUE(delegate->m_finished);

  delegate->resetFlags();

  player->setAnimationDelegate(nullptr);
  ccPlayer->NotifyAnimationFinishedForTesting(
      CompositorTargetProperty::TRANSFORM, 1);
  EXPECT_FALSE(delegate->m_finished);
}

TEST_F(CompositorAnimationPlayerTest,
       NotifyFromCCAfterCompositorPlayerDeletion) {
  std::unique_ptr<CompositorAnimationDelegateForTesting> delegate(
      new CompositorAnimationDelegateForTesting);

  std::unique_ptr<CompositorAnimationPlayer> player =
      CompositorAnimationPlayer::create();
  scoped_refptr<cc::AnimationPlayer> ccPlayer = player->ccAnimationPlayer();

  std::unique_ptr<CompositorAnimationCurve> curve =
      CompositorFloatAnimationCurve::create();
  std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(
      *curve, CompositorTargetProperty::OPACITY, 1, 0);
  player->addAnimation(std::move(animation));

  player->setAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->m_finished);

  ccPlayer->NotifyAnimationFinishedForTesting(CompositorTargetProperty::OPACITY,
                                              1);
  EXPECT_TRUE(delegate->m_finished);
  delegate->m_finished = false;

  // Delete CompositorAnimationPlayer. ccPlayer stays alive.
  player = nullptr;

  // No notifications. Doesn't crash.
  ccPlayer->NotifyAnimationFinishedForTesting(CompositorTargetProperty::OPACITY,
                                              1);
  EXPECT_FALSE(delegate->m_finished);
}

TEST_F(CompositorAnimationPlayerTest,
       CompositorPlayerDeletionDetachesFromCCTimeline) {
  std::unique_ptr<CompositorAnimationTimeline> timeline =
      CompositorAnimationTimeline::create();
  std::unique_ptr<CompositorAnimationPlayerTestClient> client(
      new CompositorAnimationPlayerTestClient);

  scoped_refptr<cc::AnimationTimeline> ccTimeline =
      timeline->animationTimeline();
  scoped_refptr<cc::AnimationPlayer> ccPlayer =
      client->m_player->ccAnimationPlayer();
  EXPECT_FALSE(ccPlayer->animation_timeline());

  timeline->playerAttached(*client);
  EXPECT_TRUE(ccPlayer->animation_timeline());
  EXPECT_TRUE(ccTimeline->GetPlayerById(ccPlayer->id()));

  // Delete client and CompositorAnimationPlayer while attached to timeline.
  client = nullptr;

  EXPECT_FALSE(ccPlayer->animation_timeline());
  EXPECT_FALSE(ccTimeline->GetPlayerById(ccPlayer->id()));
}

}  // namespace blink
