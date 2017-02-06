// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/media/renderer_webmediaplayer_delegate.h"
#include "content/renderer/render_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MockWebMediaPlayerDelegateObserver
    : public WebMediaPlayerDelegate::Observer {
 public:
  MockWebMediaPlayerDelegateObserver() {}
  ~MockWebMediaPlayerDelegateObserver() {}

  // WebMediaPlayerDelegate::Observer implementation.
  MOCK_METHOD0(OnHidden, void());
  MOCK_METHOD0(OnShown, void());
  MOCK_METHOD1(OnSuspendRequested, void(bool));
  MOCK_METHOD0(OnPlay, void());
  MOCK_METHOD0(OnPause, void());
  MOCK_METHOD1(OnVolumeMultiplierUpdate, void(double));
};

class RendererWebMediaPlayerDelegateTest : public content::RenderViewTest {
 public:
  RendererWebMediaPlayerDelegateTest() {}
  ~RendererWebMediaPlayerDelegateTest() override {}

  void SetUp() override {
    RenderViewTest::SetUp();
    delegate_manager_.reset(
        new RendererWebMediaPlayerDelegate(view_->GetMainRenderFrame()));
  }

  void TearDown() override {
    // Destruct the controller prior to any other tear down to avoid out of
    // order destruction relative to the test render frame.
    delegate_manager_.reset();
    RenderViewTest::TearDown();
  }

 protected:
  IPC::TestSink& test_sink() { return render_thread_->sink(); }

  std::unique_ptr<RendererWebMediaPlayerDelegate> delegate_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererWebMediaPlayerDelegateTest);
};

TEST_F(RendererWebMediaPlayerDelegateTest, SendsMessagesCorrectly) {
  testing::StrictMock<MockWebMediaPlayerDelegateObserver> observer;
  const int delegate_id = delegate_manager_->AddObserver(&observer);

  // Verify the playing message.
  {
    const bool kHasVideo = true, kHasAudio = false, kIsRemote = false;
    const base::TimeDelta kDuration = base::TimeDelta::FromSeconds(5);
    delegate_manager_->DidPlay(delegate_id, kHasVideo, kHasAudio, kIsRemote,
                               kDuration);

    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaPlaying::ID);
    ASSERT_TRUE(msg);

    std::tuple<int, bool, bool, bool, base::TimeDelta> result;
    ASSERT_TRUE(MediaPlayerDelegateHostMsg_OnMediaPlaying::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
    EXPECT_EQ(kHasVideo, std::get<1>(result));
    EXPECT_EQ(kHasAudio, std::get<2>(result));
    EXPECT_EQ(kIsRemote, std::get<3>(result));
    EXPECT_EQ(kDuration, std::get<4>(result));
  }

  // Verify the paused message.
  {
    test_sink().ClearMessages();
    const bool kReachedEndOfStream = true;
    delegate_manager_->DidPause(delegate_id, kReachedEndOfStream);

    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaPaused::ID);
    ASSERT_TRUE(msg);

    std::tuple<int, bool> result;
    ASSERT_TRUE(MediaPlayerDelegateHostMsg_OnMediaPaused::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
    EXPECT_EQ(kReachedEndOfStream, std::get<1>(result));
  }

  // Verify the destruction message.
  {
    test_sink().ClearMessages();
    delegate_manager_->PlayerGone(delegate_id);
    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaDestroyed::ID);
    ASSERT_TRUE(msg);

    std::tuple<int> result;
    ASSERT_TRUE(
        MediaPlayerDelegateHostMsg_OnMediaDestroyed::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
  }

  delegate_manager_->RemoveObserver(delegate_id);
}

TEST_F(RendererWebMediaPlayerDelegateTest, DeliversObserverNotifications) {
  testing::StrictMock<MockWebMediaPlayerDelegateObserver> observer;
  const int delegate_id = delegate_manager_->AddObserver(&observer);

  EXPECT_CALL(observer, OnHidden());
  delegate_manager_->WasHidden();

  EXPECT_CALL(observer, OnShown());
  delegate_manager_->WasShown();

  EXPECT_CALL(observer, OnPause());
  MediaPlayerDelegateMsg_Pause pause_msg(0, delegate_id);
  delegate_manager_->OnMessageReceived(pause_msg);

  EXPECT_CALL(observer, OnPlay());
  MediaPlayerDelegateMsg_Play play_msg(0, delegate_id);
  delegate_manager_->OnMessageReceived(play_msg);

  const double kTestMultiplier = 0.5;
  EXPECT_CALL(observer, OnVolumeMultiplierUpdate(kTestMultiplier));
  MediaPlayerDelegateMsg_UpdateVolumeMultiplier volume_msg(0, delegate_id,
                                                           kTestMultiplier);
  delegate_manager_->OnMessageReceived(volume_msg);

  EXPECT_CALL(observer, OnSuspendRequested(true));
  MediaPlayerDelegateMsg_SuspendAllMediaPlayers suspend_msg(0);
  delegate_manager_->OnMessageReceived(suspend_msg);

  delegate_manager_->RemoveObserver(delegate_id);
}

TEST_F(RendererWebMediaPlayerDelegateTest, IdleDelegatesAreSuspended) {
  // Start the tick clock off at a non-null value.
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromSeconds(1234));

  const base::TimeDelta kIdleTimeout = base::TimeDelta::FromSeconds(2);
  delegate_manager_->SetIdleCleanupParamsForTesting(kIdleTimeout, &tick_clock);
  EXPECT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());

  // Just adding an observer should not start the idle timer.
  testing::StrictMock<MockWebMediaPlayerDelegateObserver> observer_1;
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1);
  EXPECT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());

  // Starting playback should not have an idle timer.
  delegate_manager_->DidPlay(delegate_id_1, true, true, false,
                             base::TimeDelta());
  EXPECT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());

  // Never calling DidPlay() but calling DidPause() should count as idle.
  testing::StrictMock<MockWebMediaPlayerDelegateObserver> observer_2;
  const int delegate_id_2 = delegate_manager_->AddObserver(&observer_2);
  delegate_manager_->DidPause(delegate_id_2, false);
  EXPECT_TRUE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());

  // Adding the observer should instantly queue the timeout task, once run the
  // second delegate should be expired while the first is kept alive.
  {
    EXPECT_CALL(observer_2, OnSuspendRequested(false))
        .WillOnce(RunClosure(base::Bind(
            &RendererWebMediaPlayerDelegate::PlayerGone,
            base::Unretained(delegate_manager_.get()), delegate_id_2)));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    tick_clock.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    run_loop.Run();
  }

  // Pausing should count as idle if playback didn't reach end of stream, but
  // in this case the player will not remove the MediaSession.
  delegate_manager_->DidPause(delegate_id_1, false /* reached_end_of_stream */);
  testing::StrictMock<MockWebMediaPlayerDelegateObserver> observer_3;
  const int delegate_id_3 = delegate_manager_->AddObserver(&observer_3);
  delegate_manager_->DidPlay(delegate_id_3, true, true, false,
                             base::TimeDelta());

  // Adding the observer should instantly queue the timeout task, once run no
  // delegates should have been expired.
  {
    EXPECT_CALL(observer_1, OnSuspendRequested(false))
        .Times(testing::AtLeast(1));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    tick_clock.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    run_loop.Run();
  }

  delegate_manager_->DidPlay(delegate_id_1, true, true, false,
                             base::TimeDelta());

  // Pausing after reaching end of stream should count as idle.
  delegate_manager_->DidPause(delegate_id_1, true /* reached_end_of_stream */);

  // Once the timeout task runs the first delegate should be expired while the
  // third is kept alive.
  {
    EXPECT_CALL(observer_1, OnSuspendRequested(false))
        .WillOnce(RunClosure(base::Bind(
            &RendererWebMediaPlayerDelegate::PlayerGone,
            base::Unretained(delegate_manager_.get()), delegate_id_1)));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    tick_clock.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    run_loop.Run();
  }

  delegate_manager_->RemoveObserver(delegate_id_1);
  delegate_manager_->RemoveObserver(delegate_id_2);
  delegate_manager_->RemoveObserver(delegate_id_3);
  EXPECT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, IdleDelegatesIgnoresSuspendRequest) {
  // Start the tick clock off at a non-null value.
  base::SimpleTestTickClock tick_clock;
  tick_clock.Advance(base::TimeDelta::FromSeconds(1234));

  const base::TimeDelta kIdleTimeout = base::TimeDelta::FromSeconds(2);
  delegate_manager_->SetIdleCleanupParamsForTesting(kIdleTimeout, &tick_clock);
  EXPECT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());

  testing::StrictMock<MockWebMediaPlayerDelegateObserver> observer_1;
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1);
  EXPECT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());

  // Calling DidPause() should instantly queue the timeout task.
  delegate_manager_->DidPause(delegate_id_1, false);
  EXPECT_TRUE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());

  // Wait for the suspend request, but don't call PlayerGone().
  EXPECT_CALL(observer_1, OnSuspendRequested(false));
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  tick_clock.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
  run_loop.Run();

  // Even though the player did not call PlayerGone() it should be removed from
  // future idle cleanup polls.
  EXPECT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
  delegate_manager_->RemoveObserver(delegate_id_1);
}

}  // namespace media
