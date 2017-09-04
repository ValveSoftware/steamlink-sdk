// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/media/renderer_webmediaplayer_delegate.h"
#include "content/renderer/render_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace media {

namespace {
constexpr base::TimeDelta kIdleTimeout = base::TimeDelta::FromSeconds(1);
}

ACTION_P(RunClosure, closure) {
  closure.Run();
  return true;
}

class MockWebMediaPlayerDelegateObserver
    : public WebMediaPlayerDelegate::Observer {
 public:
  MockWebMediaPlayerDelegateObserver() {}
  ~MockWebMediaPlayerDelegateObserver() {}

  // WebMediaPlayerDelegate::Observer implementation.
  MOCK_METHOD0(OnHidden, void());
  MOCK_METHOD0(OnShown, void());
  MOCK_METHOD1(OnSuspendRequested, bool(bool));
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
    // Start the tick clock off at a non-null value.
    tick_clock_.Advance(base::TimeDelta::FromSeconds(1234));
    delegate_manager_.reset(
        new RendererWebMediaPlayerDelegate(view_->GetMainRenderFrame()));
    delegate_manager_->SetIdleCleanupParamsForTesting(kIdleTimeout,
                                                      &tick_clock_, false);
  }

  void TearDown() override {
    delegate_manager_.reset();
    RenderViewTest::TearDown();
  }

 protected:
  IPC::TestSink& test_sink() { return render_thread_->sink(); }

  bool HasPlayingVideo(int delegate_id) {
    return delegate_manager_->playing_videos_.count(delegate_id);
  }

  void SetPlayingBackgroundVideo(bool is_playing) {
    delegate_manager_->is_playing_background_video_ = is_playing;
  }

  void CallOnMediaDelegatePlay(int delegate_id) {
    delegate_manager_->OnMediaDelegatePlay(delegate_id);
  }

  void CallOnMediaDelegatePause(int delegate_id) {
    delegate_manager_->OnMediaDelegatePause(delegate_id);
  }

  void SetIsLowEndDeviceForTesting() {
    delegate_manager_->SetIdleCleanupParamsForTesting(kIdleTimeout,
                                                      &tick_clock_, true);
  }

  std::unique_ptr<RendererWebMediaPlayerDelegate> delegate_manager_;
  StrictMock<MockWebMediaPlayerDelegateObserver> observer_1_, observer_2_,
      observer_3_;
  base::SimpleTestTickClock tick_clock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererWebMediaPlayerDelegateTest);
};

TEST_F(RendererWebMediaPlayerDelegateTest, SendsMessagesCorrectly) {
  StrictMock<MockWebMediaPlayerDelegateObserver> observer;
  const int delegate_id = delegate_manager_->AddObserver(&observer);

  // Verify the playing message.
  {
    const bool kHasVideo = true, kHasAudio = false, kIsRemote = false;
    const media::MediaContentType kMediaContentType =
        media::MediaContentType::Transient;
    delegate_manager_->DidPlay(delegate_id, kHasVideo, kHasAudio, kIsRemote,
                               kMediaContentType);

    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaPlaying::ID);
    ASSERT_TRUE(msg);

    std::tuple<int, bool, bool, bool, media::MediaContentType> result;
    ASSERT_TRUE(MediaPlayerDelegateHostMsg_OnMediaPlaying::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
    EXPECT_EQ(kHasVideo, std::get<1>(result));
    EXPECT_EQ(kHasAudio, std::get<2>(result));
    EXPECT_EQ(kIsRemote, std::get<3>(result));
    EXPECT_EQ(kMediaContentType, std::get<4>(result));
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
}

TEST_F(RendererWebMediaPlayerDelegateTest, DeliversObserverNotifications) {
  const int delegate_id = delegate_manager_->AddObserver(&observer_1_);

  EXPECT_CALL(observer_1_, OnHidden());
  delegate_manager_->WasHidden();

  EXPECT_CALL(observer_1_, OnShown());
  delegate_manager_->WasShown();

  EXPECT_CALL(observer_1_, OnPause());
  MediaPlayerDelegateMsg_Pause pause_msg(0, delegate_id);
  delegate_manager_->OnMessageReceived(pause_msg);

  EXPECT_CALL(observer_1_, OnPlay());
  MediaPlayerDelegateMsg_Play play_msg(0, delegate_id);
  delegate_manager_->OnMessageReceived(play_msg);

  const double kTestMultiplier = 0.5;
  EXPECT_CALL(observer_1_, OnVolumeMultiplierUpdate(kTestMultiplier));
  MediaPlayerDelegateMsg_UpdateVolumeMultiplier volume_msg(0, delegate_id,
                                                           kTestMultiplier);
  delegate_manager_->OnMessageReceived(volume_msg);

  EXPECT_CALL(observer_1_, OnSuspendRequested(true));
  MediaPlayerDelegateMsg_SuspendAllMediaPlayers suspend_msg(0);
  delegate_manager_->OnMessageReceived(suspend_msg);
}

TEST_F(RendererWebMediaPlayerDelegateTest, TheTimerIsInitiallyStopped) {
  ASSERT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, AddingAnObserverStartsTheTimer) {
  delegate_manager_->AddObserver(&observer_1_);
  ASSERT_TRUE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, RemovingAllObserversStopsTheTimer) {
  delegate_manager_->RemoveObserver(
      delegate_manager_->AddObserver(&observer_1_));
  ASSERT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, PlayingDelegatesAreNotIdle) {
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->DidPlay(delegate_id_1, true, true, false,
                             media::MediaContentType::Persistent);
  ASSERT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, PlaySuspendsLowEndIdleDelegates) {
  SetIsLowEndDeviceForTesting();

  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->AddObserver(&observer_2_);

  // Calling play on the first player should suspend the other idle player.
  EXPECT_CALL(observer_2_, OnSuspendRequested(false));
  tick_clock_.Advance(base::TimeDelta::FromMicroseconds(1));
  delegate_manager_->DidPlay(delegate_id_1, true, true, false,
                             media::MediaContentType::Persistent);
}

TEST_F(RendererWebMediaPlayerDelegateTest, MaxLowEndIdleDelegates) {
  SetIsLowEndDeviceForTesting();

  delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->AddObserver(&observer_2_);

  // Just adding a third idle observer should suspend the others.
  EXPECT_CALL(observer_1_, OnSuspendRequested(false));
  EXPECT_CALL(observer_2_, OnSuspendRequested(false));
  tick_clock_.Advance(base::TimeDelta::FromMicroseconds(1));
  delegate_manager_->AddObserver(&observer_3_);
}

// Make sure it's safe to call DidPause(), which modifies the idle delegate
// list, from OnSuspendRequested(), which iterates over the idle delegate list.
TEST_F(RendererWebMediaPlayerDelegateTest, ReentrantDelegateCallsAreSafe) {
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  EXPECT_CALL(observer_1_, OnSuspendRequested(false))
      .WillOnce(RunClosure(base::Bind(&RendererWebMediaPlayerDelegate::DidPause,
                                      base::Unretained(delegate_manager_.get()),
                                      delegate_id_1, false)));
  // Run an idle cleanup.
  tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererWebMediaPlayerDelegateTest,
       SuspendRequestsAreOnlySentOnceIfHandled) {
  delegate_manager_->AddObserver(&observer_1_);
  // Return true from OnSuspendRequested() to indicate that it was handled. So
  // even though the player did not call PlayerGone() it should be removed from
  // future idle cleanup polls.
  EXPECT_CALL(observer_1_, OnSuspendRequested(false)).WillOnce(Return(true));
  tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererWebMediaPlayerDelegateTest,
       SuspendRequestsAreSentAgainIfNotHandled) {
  delegate_manager_->AddObserver(&observer_1_);
  // Return false from OnSuspendRequested() to indicate that it was not handled.
  // The observer should get another OnSuspendRequested.
  EXPECT_CALL(observer_1_, OnSuspendRequested(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererWebMediaPlayerDelegateTest, IdleDelegatesAreSuspended) {
  // Add one non-idle observer and one idle observer.
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->DidPlay(delegate_id_1, true, true, false,
                             media::MediaContentType::Persistent);
  delegate_manager_->AddObserver(&observer_2_);

  // The idle cleanup task should suspend the second delegate while the first is
  // kept alive.
  {
    EXPECT_CALL(observer_2_, OnSuspendRequested(false)).WillOnce(Return(true));
    tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    base::RunLoop().RunUntilIdle();
  }

  // Pausing should count as idle if playback didn't reach end of stream, but
  // in this case the player will not remove the MediaSession.
  delegate_manager_->DidPause(delegate_id_1, false /* reached_end_of_stream */);
  const int delegate_id_3 = delegate_manager_->AddObserver(&observer_3_);
  delegate_manager_->DidPlay(delegate_id_3, true, true, false,
                             media::MediaContentType::Persistent);

  // Adding the observer should instantly queue the timeout task. Once run only
  // the first player should be suspended.
  {
    EXPECT_CALL(observer_1_, OnSuspendRequested(false)).WillOnce(Return(true));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    run_loop.Run();
  }

  delegate_manager_->DidPlay(delegate_id_1, true, true, false,
                             media::MediaContentType::Persistent);

  // Pausing after reaching end of stream should count as idle.
  delegate_manager_->DidPause(delegate_id_1, true /* reached_end_of_stream */);

  // Once the timeout task runs the first delegate should be suspended while the
  // third is kept alive.
  {
    EXPECT_CALL(observer_1_, OnSuspendRequested(false)).WillOnce(Return(true));
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    run_loop.Run();
  }
}

TEST_F(RendererWebMediaPlayerDelegateTest, PlayingVideosSet) {
  int delegate_id = delegate_manager_->AddObserver(&observer_1_);
  EXPECT_FALSE(HasPlayingVideo(delegate_id));

  // Playing a local video adds it to the set.
  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  EXPECT_TRUE(HasPlayingVideo(delegate_id));

  // Pause doesn't remove the video from the set.
  delegate_manager_->DidPause(delegate_id, false);
  EXPECT_TRUE(HasPlayingVideo(delegate_id));

  // Reaching the end removes the video from the set.
  delegate_manager_->DidPause(delegate_id, true);
  EXPECT_FALSE(HasPlayingVideo(delegate_id));

  // Removing the player removes the video from the set.
  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  delegate_manager_->PlayerGone(delegate_id);
  EXPECT_FALSE(HasPlayingVideo(delegate_id));

  // Playing a remote video removes it from the set.
  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  delegate_manager_->DidPlay(delegate_id, true, true, true,
                             MediaContentType::Persistent);
  EXPECT_FALSE(HasPlayingVideo(delegate_id));

  // Playing a local video without audio adds it to the set (because of WMPA).
  delegate_manager_->DidPlay(delegate_id, true, false, false,
                             MediaContentType::Persistent);
  EXPECT_TRUE(HasPlayingVideo(delegate_id));

  // Playing a local audio removes it from the set.
  delegate_manager_->DidPlay(delegate_id, false, true, false,
                             MediaContentType::Persistent);
  EXPECT_FALSE(HasPlayingVideo(delegate_id));

  // Removing the observer also removes the video from the set.
  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  delegate_manager_->RemoveObserver(delegate_id);
  EXPECT_FALSE(HasPlayingVideo(delegate_id));
}

TEST_F(RendererWebMediaPlayerDelegateTest, IsPlayingBackgroundVideo) {
  NiceMock<MockWebMediaPlayerDelegateObserver> observer;
  int delegate_id = delegate_manager_->AddObserver(&observer);
  EXPECT_FALSE(delegate_manager_->IsPlayingBackgroundVideo());

  // Showing the frame always clears the flag.
  SetPlayingBackgroundVideo(true);
  delegate_manager_->WasShown();
  EXPECT_FALSE(delegate_manager_->IsPlayingBackgroundVideo());

  // Pausing anything other than a local playing video doesn't affect the flag.
  SetPlayingBackgroundVideo(true);
  CallOnMediaDelegatePause(delegate_id);
  EXPECT_TRUE(delegate_manager_->IsPlayingBackgroundVideo());

  // Pausing a currently playing video clears the flag.
  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  CallOnMediaDelegatePause(delegate_id);
  EXPECT_FALSE(delegate_manager_->IsPlayingBackgroundVideo());

  // TODO(avayvod): this test can't mock the IsHidden() method.
  // Just test that the value changes or doesn't depending on whether the video
  // is currently playing.
  bool old_value = !delegate_manager_->IsHidden();
  SetPlayingBackgroundVideo(old_value);
  delegate_manager_->DidPause(delegate_id, true);
  CallOnMediaDelegatePlay(delegate_id);
  EXPECT_EQ(old_value, delegate_manager_->IsPlayingBackgroundVideo());

  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  CallOnMediaDelegatePlay(delegate_id);
  EXPECT_NE(old_value, delegate_manager_->IsPlayingBackgroundVideo());
}

#if defined(OS_ANDROID)

TEST_F(RendererWebMediaPlayerDelegateTest, Histograms) {
  NiceMock<MockWebMediaPlayerDelegateObserver> observer;
  int delegate_id = delegate_manager_->AddObserver(&observer);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 0);

  // Pausing or showing doesn't record anything as background playback
  // hasn't started yet.
  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  CallOnMediaDelegatePause(delegate_id);
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 0);

  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  delegate_manager_->WasShown();
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 0);

  // Doing this things after the background playback has started should record
  // the time.
  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  SetPlayingBackgroundVideo(true);
  CallOnMediaDelegatePause(delegate_id);
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 1);

  delegate_manager_->DidPlay(delegate_id, true, true, false,
                             MediaContentType::Persistent);
  SetPlayingBackgroundVideo(true);
  delegate_manager_->WasShown();
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 2);
}

#endif  // OS_ANDROID

}  // namespace media
