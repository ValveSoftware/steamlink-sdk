// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/test_helpers.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/renderers/default_renderer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace media {

int64_t OnAdjustAllocatedMemory(int64_t delta) {
  return 0;
}

class DummyWebMediaPlayerClient : public blink::WebMediaPlayerClient {
 public:
  DummyWebMediaPlayerClient() {}

  // blink::WebMediaPlayerClient implementation.
  void networkStateChanged() override {}
  void readyStateChanged() override {}
  void timeChanged() override {}
  void repaint() override {}
  void durationChanged() override {}
  void sizeChanged() override {}
  void playbackStateChanged() override {}
  void setWebLayer(blink::WebLayer*) override {}
  blink::WebMediaPlayer::TrackId addAudioTrack(
      const blink::WebString& id,
      blink::WebMediaPlayerClient::AudioTrackKind,
      const blink::WebString& label,
      const blink::WebString& language,
      bool enabled) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void removeAudioTrack(blink::WebMediaPlayer::TrackId) override {}
  blink::WebMediaPlayer::TrackId addVideoTrack(
      const blink::WebString& id,
      blink::WebMediaPlayerClient::VideoTrackKind,
      const blink::WebString& label,
      const blink::WebString& language,
      bool selected) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void removeVideoTrack(blink::WebMediaPlayer::TrackId) override {}
  void addTextTrack(blink::WebInbandTextTrack*) override {}
  void removeTextTrack(blink::WebInbandTextTrack*) override {}
  void mediaSourceOpened(blink::WebMediaSource*) override {}
  void requestSeek(double) override {}
  void remoteRouteAvailabilityChanged(
      blink::WebRemotePlaybackAvailability) override {}
  void connectedToRemoteDevice() override {}
  void disconnectedFromRemoteDevice() override {}
  void cancelledRemotePlaybackRequest() override {}
  void remotePlaybackStarted() override {}
  bool isAutoplayingMuted() override { return is_autoplaying_muted_; }
  void requestReload(const blink::WebURL& newUrl) override {}

  void set_is_autoplaying_muted(bool value) { is_autoplaying_muted_ = value; }

 private:
  bool is_autoplaying_muted_ = false;

  DISALLOW_COPY_AND_ASSIGN(DummyWebMediaPlayerClient);
};

class MockWebMediaPlayerDelegate
    : public WebMediaPlayerDelegate,
      public base::SupportsWeakPtr<MockWebMediaPlayerDelegate> {
 public:
  MockWebMediaPlayerDelegate() = default;
  ~MockWebMediaPlayerDelegate() = default;

  // WebMediaPlayerDelegate implementation.
  MOCK_METHOD1(AddObserver, int(Observer*));
  MOCK_METHOD1(RemoveObserver, void(int));
  MOCK_METHOD5(DidPlay, void(int, bool, bool, bool, MediaContentType));
  MOCK_METHOD2(DidPause, void(int, bool));
  MOCK_METHOD1(PlayerGone, void(int));
  MOCK_METHOD0(IsHidden, bool());
  MOCK_METHOD0(IsPlayingBackgroundVideo, bool());
};

class WebMediaPlayerImplTest : public testing::Test {
 public:
  WebMediaPlayerImplTest()
      : media_thread_("MediaThreadForTest"),
        web_view_(blink::WebView::create(nullptr,
                                         blink::WebPageVisibilityStateVisible)),
        web_local_frame_(
            blink::WebLocalFrame::create(blink::WebTreeScopeType::Document,
                                         &web_frame_client_)),
        media_log_(new MediaLog()),
        audio_parameters_(TestAudioParameters::Normal()) {
    web_view_->setMainFrame(web_local_frame_);
    media_thread_.StartAndWaitForTesting();
  }

  void InitializeWebMediaPlayerImpl() {
    wmpi_.reset(new WebMediaPlayerImpl(
        web_local_frame_, &client_, nullptr, delegate_.AsWeakPtr(),
        base::MakeUnique<DefaultRendererFactory>(
            media_log_, nullptr, DefaultRendererFactory::GetGpuFactoriesCB()),
        url_index_,
        WebMediaPlayerParams(
            WebMediaPlayerParams::DeferLoadCB(),
            scoped_refptr<SwitchableAudioRendererSink>(), media_log_,
            media_thread_.task_runner(), message_loop_.task_runner(),
            message_loop_.task_runner(), WebMediaPlayerParams::Context3DCB(),
            base::Bind(&OnAdjustAllocatedMemory), nullptr, nullptr, nullptr)));
  }

  ~WebMediaPlayerImplTest() override {
    // Destruct WebMediaPlayerImpl and pump the message loop to ensure that
    // objects passed to the message loop for destruction are released.
    //
    // NOTE: This should be done before any other member variables are
    // destructed since WMPI may reference them during destruction.
    wmpi_.reset();
    base::RunLoop().RunUntilIdle();

    web_view_->close();
  }

 protected:
  void SetReadyState(blink::WebMediaPlayer::ReadyState state) {
    wmpi_->SetReadyState(state);
  }

  void SetPaused(bool is_paused) { wmpi_->paused_ = is_paused; }
  void SetSeeking(bool is_seeking) { wmpi_->seeking_ = is_seeking; }
  void SetEnded(bool is_ended) { wmpi_->ended_ = is_ended; }
  void SetTickClock(base::TickClock* clock) { wmpi_->tick_clock_.reset(clock); }

  void SetFullscreen(bool is_fullscreen) {
    wmpi_->overlay_enabled_ = is_fullscreen;
  }

  void SetMetadata(bool has_audio, bool has_video) {
    wmpi_->SetNetworkState(blink::WebMediaPlayer::NetworkStateLoaded);
    wmpi_->SetReadyState(blink::WebMediaPlayer::ReadyStateHaveMetadata);
    wmpi_->pipeline_metadata_.has_audio = has_audio;
    wmpi_->pipeline_metadata_.has_video = has_video;
  }

  void OnMetadata(PipelineMetadata metadata) { wmpi_->OnMetadata(metadata); }

  void OnVideoNaturalSizeChange(const gfx::Size& size) {
    wmpi_->OnVideoNaturalSizeChange(size);
  }

  WebMediaPlayerImpl::PlayState ComputePlayState() {
    wmpi_->is_idle_ = false;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputePlayStateSuspended() {
    wmpi_->is_idle_ = false;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, true, false);
  }

  WebMediaPlayerImpl::PlayState ComputeBackgroundedPlayState() {
    wmpi_->is_idle_ = false;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, true);
  }

  WebMediaPlayerImpl::PlayState ComputeIdlePlayState() {
    wmpi_->is_idle_ = true;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputeIdleSuspendedPlayState() {
    wmpi_->is_idle_ = true;
    wmpi_->must_suspend_ = false;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, true, false);
  }

  WebMediaPlayerImpl::PlayState ComputeMustSuspendPlayState() {
    wmpi_->is_idle_ = false;
    wmpi_->must_suspend_ = true;
    return wmpi_->UpdatePlayState_ComputePlayState(false, false, false, false);
  }

  WebMediaPlayerImpl::PlayState ComputeStreamingPlayState(bool must_suspend) {
    wmpi_->is_idle_ = true;
    wmpi_->must_suspend_ = must_suspend;
    return wmpi_->UpdatePlayState_ComputePlayState(false, true, false, true);
  }

  void SetDelegateState(WebMediaPlayerImpl::DelegateState state) {
    wmpi_->SetDelegateState(state);
  }

  bool IsSuspended() { return wmpi_->pipeline_controller_.IsSuspended(); }

  void AddBufferedRanges() {
    wmpi_->buffered_data_source_host_.AddBufferedByteRange(0, 1);
  }

  void SetupForResumingBackgroundVideo() {
#if !defined(OS_ANDROID)
    // Need to enable media suspend to test resuming background videos.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableMediaSuspend);
#endif  // !defined(OS_ANDROID)
    scoped_feature_list_.InitAndEnableFeature(kResumeBackgroundVideo);
  }

  // "Renderer" thread.
  base::MessageLoop message_loop_;

  // "Media" thread. This is necessary because WMPI destruction waits on a
  // WaitableEvent.
  base::Thread media_thread_;

  // Blink state.
  blink::WebFrameClient web_frame_client_;
  blink::WebView* web_view_;
  blink::WebLocalFrame* web_local_frame_;

  scoped_refptr<MediaLog> media_log_;
  linked_ptr<media::UrlIndex> url_index_;

  // Audio hardware configuration.
  AudioParameters audio_parameters_;

  // The client interface used by |wmpi_|. Just a dummy for now, but later we
  // may want a mock or intelligent fake.
  DummyWebMediaPlayerClient client_;

  testing::NiceMock<MockWebMediaPlayerDelegate> delegate_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerImplTest);
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  InitializeWebMediaPlayerImpl();
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendIsEnabledBeforeLoadingBegins) {
  InitializeWebMediaPlayerImpl();
  wmpi_->OnSuspendRequested(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest,
       IdleSuspendIsDisabledIfLoadingProgressedRecently) {
  InitializeWebMediaPlayerImpl();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(clock);
  AddBufferedRanges();
  wmpi_->didLoadingProgress();
  // Advance less than the loading timeout.
  clock->Advance(base::TimeDelta::FromSeconds(1));
  wmpi_->OnSuspendRequested(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, IdleSuspendIsEnabledIfLoadingHasStalled) {
  InitializeWebMediaPlayerImpl();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  SetTickClock(clock);
  AddBufferedRanges();
  wmpi_->didLoadingProgress();
  // Advance more than the loading timeout.
  clock->Advance(base::TimeDelta::FromSeconds(4));
  wmpi_->OnSuspendRequested(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, DidLoadingProgressTriggersResume) {
  InitializeWebMediaPlayerImpl();
  EXPECT_FALSE(IsSuspended());
  wmpi_->OnSuspendRequested(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSuspended());
  AddBufferedRanges();
  wmpi_->didLoadingProgress();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSuspended());
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterConstruction) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeIdlePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterMetadata) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeIdlePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterMetadata_AudioOnly) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, false);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeIdlePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  SetPaused(false);
  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_AfterFutureData) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeBackgroundedPlayState();

  if (base::FeatureList::IsEnabled(kResumeBackgroundVideo))
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  else
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  // Idle suspension is possible after HaveFutureData.
  state = ComputeIdlePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Playing) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeBackgroundedPlayState();
  if (base::FeatureList::IsEnabled(kResumeBackgroundVideo))
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  else
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_PlayingThenUnderflow) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveCurrentData);

  // Underflow should not trigger idle suspend. The user is still playing the
  // the video, just waiting on the network.
  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // Background suspend should still be possible during underflow.
  state = ComputeBackgroundedPlayState();
  if (base::FeatureList::IsEnabled(kResumeBackgroundVideo))
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  else
    EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  // Forced suspend should still be possible during underflow.
  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Playing_AudioOnly) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, false);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // Audio-only stays playing in the background.
  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // Backgrounding a paused audio only player should suspend, but keep the
  // session alive for user interactions.
  SetPaused(true);
  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  state = ComputeMustSuspendPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Paused_Seek) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetSeeking(true);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Paused_Fullscreen) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetFullscreen(true);

  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Ended) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetEnded(true);

  // The pipeline is not suspended immediately on ended.
  state = ComputePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::ENDED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  state = ComputeIdlePlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::ENDED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Streaming) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);

  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(true);

  // Streaming media should not suspend, even if paused, idle, and backgrounded.
  state = ComputeStreamingPlayState(false);
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // Streaming media should suspend when the tab is closed, regardless.
  state = ComputeStreamingPlayState(true);
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, ComputePlayState_Suspended) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);

  // Suspended players should be resumed unless we have reached the appropriate
  // ready state and are not seeking.
  SetPaused(true);
  state = ComputePlayStateSuspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // Paused players in the idle state are allowed to remain suspended.
  state = ComputeIdleSuspendedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  SetPaused(false);
  state = ComputePlayStateSuspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::GONE, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);

  // Paused players should stay suspended.
  SetPaused(true);
  state = ComputePlayStateSuspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);

  // Playing players should resume into the playing state.
  SetPaused(false);
  state = ComputePlayStateSuspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  // If seeking, the previously suspended state does not matter; the player
  // should always be resumed.
  SetSeeking(true);

  SetPaused(true);
  state = ComputePlayStateSuspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);

  SetPaused(false);
  state = ComputePlayStateSuspended();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.natural_size = gfx::Size(320, 240);

  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(320, 240), wmpi_->naturalSize());

  // TODO(sandersd): Verify that the client is notified of the size change?
  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1920, 1080), wmpi_->naturalSize());
}

TEST_F(WebMediaPlayerImplTest, NaturalSizeChange_Rotated) {
  InitializeWebMediaPlayerImpl();
  PipelineMetadata metadata;
  metadata.has_video = true;
  metadata.natural_size = gfx::Size(320, 240);
  metadata.video_rotation = VIDEO_ROTATION_90;

  // For 90/270deg rotations, the natural size should be transposed.
  OnMetadata(metadata);
  ASSERT_EQ(blink::WebSize(240, 320), wmpi_->naturalSize());

  OnVideoNaturalSizeChange(gfx::Size(1920, 1080));
  ASSERT_EQ(blink::WebSize(1080, 1920), wmpi_->naturalSize());
}

// Audible backgrounded videos are not suspended if delegate_ allows it.
TEST_F(WebMediaPlayerImplTest, ComputePlayState_BackgroundedVideoPlaying) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);

  SetupForResumingBackgroundVideo();

  EXPECT_CALL(delegate_, IsPlayingBackgroundVideo())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, IsHidden()).WillRepeatedly(Return(true));

  SetPaused(false);
  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PLAYING, state.delegate_state);
  EXPECT_TRUE(state.is_memory_reporting_enabled);
  EXPECT_FALSE(state.is_suspended);
}

// Backgrounding audible videos should suspend them and report as paused, not
// gone.
TEST_F(WebMediaPlayerImplTest, ComputePlayState_BackgroundedVideoPaused) {
  InitializeWebMediaPlayerImpl();
  WebMediaPlayerImpl::PlayState state;
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);

  SetupForResumingBackgroundVideo();

  EXPECT_CALL(delegate_, IsPlayingBackgroundVideo()).WillOnce(Return(false));
  EXPECT_CALL(delegate_, IsHidden()).WillRepeatedly(Return(true));

  state = ComputeBackgroundedPlayState();
  EXPECT_EQ(WebMediaPlayerImpl::DelegateState::PAUSED, state.delegate_state);
  EXPECT_FALSE(state.is_memory_reporting_enabled);
  EXPECT_TRUE(state.is_suspended);
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted_StartsAndStops) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  EXPECT_CALL(delegate_, DidPlay(_, true, false, false, _));
  client_.set_is_autoplaying_muted(true);
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);

  EXPECT_CALL(delegate_, DidPlay(_, true, true, false, _));
  client_.set_is_autoplaying_muted(false);
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);
}

TEST_F(WebMediaPlayerImplTest, AutoplayMuted_SetVolume) {
  InitializeWebMediaPlayerImpl();
  SetMetadata(true, true);
  SetReadyState(blink::WebMediaPlayer::ReadyStateHaveFutureData);
  SetPaused(false);

  EXPECT_CALL(delegate_, DidPlay(_, true, false, false, _));
  client_.set_is_autoplaying_muted(true);
  SetDelegateState(WebMediaPlayerImpl::DelegateState::PLAYING);

  EXPECT_CALL(delegate_, DidPlay(_, true, true, false, _));
  client_.set_is_autoplaying_muted(false);
  wmpi_->setVolume(1.0);
}

}  // namespace media
