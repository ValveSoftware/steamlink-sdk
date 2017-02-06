// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/media/session/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {
static const char kStartPlayerScript[] =
    "document.getElementById('long-video').play()";
static const char kPausePlayerScript[] =
    "document.getElementById('long-video').pause()";
}


// Base class of MediaSession visibility tests. The class is intended
// to be used to run tests under different configurations. Tests
// should inheret from this class, set up their own command line per
// their configuration, and use macro INCLUDE_TEST_FROM_BASE_CLASS to
// include required tests. See
// media_session_visibility_browsertest_instances.cc for examples.
class MediaSessionVisibilityBrowserTest
    : public ContentBrowserTest {
 public:
  MediaSessionVisibilityBrowserTest() = default;
  ~MediaSessionVisibilityBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    web_contents_ = shell()->web_contents();
    media_session_ = MediaSession::Get(web_contents_);

    media_session_state_loop_runners_[MediaSession::State::ACTIVE] =
        new MessageLoopRunner();
    media_session_state_loop_runners_[MediaSession::State::SUSPENDED] =
        new MessageLoopRunner();
    media_session_state_loop_runners_[MediaSession::State::INACTIVE] =
        new MessageLoopRunner();
    media_session_state_callback_subscription_ =
        media_session_->RegisterMediaSessionStateChangedCallbackForTest(
            base::Bind(&MediaSessionVisibilityBrowserTest::
                       OnMediaSessionStateChanged,
                       base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    // Unsubscribe the callback subscription before tearing down, so that the
    // CallbackList in MediaSession will be empty when it is destroyed.
    media_session_state_callback_subscription_.reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
#if !defined(OS_ANDROID)
    command_line->AppendSwitch(
        switches::kEnableDefaultMediaSession);
#endif  // !defined(OS_ANDROID)
  }

  void LoadTestPage() {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(GetTestUrl("media/session", "media-session.html"));
    navigation_observer.Wait();
  }

  void RunScript(const std::string& script) {
    ASSERT_TRUE(ExecuteScript(web_contents_->GetMainFrame(), script));
  }

  void ClearMediaSessionStateLoopRunners() {
    for (auto& state_loop_runner : media_session_state_loop_runners_)
      state_loop_runner.second = new MessageLoopRunner();
  }

  void OnMediaSessionStateChanged(MediaSession::State state) {
    ASSERT_TRUE(media_session_state_loop_runners_.count(state));
    media_session_state_loop_runners_[state]->Quit();
  }

  // TODO(zqzhang): This method is shared with
  // MediaRouterIntegrationTests. Move it into a general place.
  void Wait(base::TimeDelta timeout) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), timeout);
    run_loop.Run();
  }

  void WaitForMediaSessionState(MediaSession::State state) {
    ASSERT_TRUE(media_session_state_loop_runners_.count(state));
    media_session_state_loop_runners_[state]->Run();
  }

 protected:
  void TestSessionInactiveWhenHiddenAfterContentPause() {
    LoadTestPage();

    LOG(INFO) << "Starting player";
    ClearMediaSessionStateLoopRunners();
    RunScript(kStartPlayerScript);
    LOG(INFO) << "Waiting for Session to be active";
    WaitForMediaSessionState(MediaSession::State::ACTIVE);

    LOG(INFO) << "Pausing player";
    ClearMediaSessionStateLoopRunners();
    RunScript(kPausePlayerScript);
    LOG(INFO) << "Waiting for Session to be suspended";
    WaitForMediaSessionState(MediaSession::State::SUSPENDED);

    LOG(INFO) << "Hiding the tab";
    ClearMediaSessionStateLoopRunners();
    web_contents_->WasHidden();
    LOG(INFO) << "Waiting for Session to be inactive";
    WaitForMediaSessionState(MediaSession::State::INACTIVE);

    LOG(INFO) << "Test succeeded";
  }

  void TestSessionInactiveWhenHiddenWhilePlaying() {
    LoadTestPage();

    LOG(INFO) << "Starting player";
    ClearMediaSessionStateLoopRunners();
    RunScript(kStartPlayerScript);
    LOG(INFO) << "Waiting for Session to be active";
    WaitForMediaSessionState(MediaSession::State::ACTIVE);

    LOG(INFO) << "Hiding the tab";
    ClearMediaSessionStateLoopRunners();
    web_contents_->WasHidden();
    LOG(INFO) << "Waiting for Session to be inactive";
    WaitForMediaSessionState(MediaSession::State::INACTIVE);

    LOG(INFO) << "Test succeeded";
  }

  void TestSessionSuspendedWhenHiddenAfterContentPause() {
    LoadTestPage();

    LOG(INFO) << "Starting player";
    ClearMediaSessionStateLoopRunners();
    RunScript(kStartPlayerScript);
    LOG(INFO) << "Waiting for Session to be active";
    WaitForMediaSessionState(MediaSession::State::ACTIVE);

    LOG(INFO) << "Pausing player";
    ClearMediaSessionStateLoopRunners();
    RunScript(kPausePlayerScript);
    LOG(INFO) << "Waiting for Session to be suspended";
    WaitForMediaSessionState(MediaSession::State::SUSPENDED);

    LOG(INFO) << "Hiding the tab";
    // Wait for 1 second and check the MediaSession state.
    // No better solution till now.
    web_contents_->WasHidden();
    Wait(base::TimeDelta::FromSeconds(1));
    ASSERT_EQ(media_session_->audio_focus_state_,
              MediaSession::State::SUSPENDED);

    LOG(INFO) << "Test succeeded";
  }

  void TestSessionActiveWhenHiddenWhilePlaying() {
    LoadTestPage();

    LOG(INFO) << "Starting player";
    ClearMediaSessionStateLoopRunners();
    RunScript(kStartPlayerScript);
    LOG(INFO) << "Waiting for Session to be active";
    WaitForMediaSessionState(MediaSession::State::ACTIVE);

    LOG(INFO) << "Hiding the tab";
    // Wait for 1 second and check the MediaSession state.
    // No better solution till now.
    web_contents_->WasHidden();
    Wait(base::TimeDelta::FromSeconds(1));
    ASSERT_EQ(media_session_->audio_focus_state_,
              MediaSession::State::ACTIVE);

    LOG(INFO) << "Test succeeded";
  }

  WebContents* web_contents_;
  MediaSession* media_session_;
  // MessageLoopRunners for waiting MediaSession state to change. Note that the
  // MessageLoopRunners can accept Quit() before calling Run(), thus the state
  // change can still be captured before waiting. For example, the MediaSession
  // might go active immediately after calling HTMLMediaElement.play(). A test
  // can listen to the state change before calling play(), and then wait for the
  // state change after play().
  std::map<MediaSession::State, scoped_refptr<MessageLoopRunner> >
  media_session_state_loop_runners_;
  std::unique_ptr<base::CallbackList<void(MediaSession::State)>::Subscription>
      media_session_state_callback_subscription_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaSessionVisibilityBrowserTest);
};

// Helper macro to include tests from the base class.
#define INCLUDE_TEST_FROM_BASE_CLASS(test_fixture, test_name)   \
  IN_PROC_BROWSER_TEST_F(test_fixture, test_name) {             \
    test_name();                                                \
  }

///////////////////////////////////////////////////////////////////////////////
// Configuration instances.

// UnifiedPipeline + SuspendOnHide
class MediaSessionVisibilityBrowserTest_UnifiedPipeline_SuspendOnHide :
      public MediaSessionVisibilityBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaSessionVisibilityBrowserTest::SetUpCommandLine(command_line);
#if !defined(OS_ANDROID)
    command_line->AppendSwitch(switches::kEnableMediaSuspend);
#endif  // defined(OS_ANDROID)
  }
};

INCLUDE_TEST_FROM_BASE_CLASS(
    MediaSessionVisibilityBrowserTest_UnifiedPipeline_SuspendOnHide,
    TestSessionInactiveWhenHiddenAfterContentPause)
INCLUDE_TEST_FROM_BASE_CLASS(
    MediaSessionVisibilityBrowserTest_UnifiedPipeline_SuspendOnHide,
    TestSessionInactiveWhenHiddenWhilePlaying)

// UnifiedPipeline + NosuspendOnHide
class MediaSessionVisibilityBrowserTest_UnifiedPipeline_NosuspendOnHide :
      public MediaSessionVisibilityBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaSessionVisibilityBrowserTest::SetUpCommandLine(command_line);
#if defined(OS_ANDROID)
    command_line->AppendSwitch(switches::kDisableMediaSuspend);
#endif  // defined(OS_ANDROID)
  }
};

INCLUDE_TEST_FROM_BASE_CLASS(
    MediaSessionVisibilityBrowserTest_UnifiedPipeline_NosuspendOnHide,
    TestSessionSuspendedWhenHiddenAfterContentPause)
INCLUDE_TEST_FROM_BASE_CLASS(
    MediaSessionVisibilityBrowserTest_UnifiedPipeline_NosuspendOnHide,
    TestSessionActiveWhenHiddenWhilePlaying)

#if defined(OS_ANDROID)
// AndroidPipeline + SuspendOnHide
class MediaSessionVisibilityBrowserTest_AndroidPipeline_SuspendOnHide :
      public MediaSessionVisibilityBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaSessionVisibilityBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableUnifiedMediaPipeline);
  }
};

// The following tests are flaky. Re-enabling with logging to see what's
// happening on the bots. See crbug.com/619096.
INCLUDE_TEST_FROM_BASE_CLASS(
    MediaSessionVisibilityBrowserTest_AndroidPipeline_SuspendOnHide,
    TestSessionInactiveWhenHiddenAfterContentPause)
INCLUDE_TEST_FROM_BASE_CLASS(
    MediaSessionVisibilityBrowserTest_AndroidPipeline_SuspendOnHide,
    TestSessionInactiveWhenHiddenWhilePlaying)

// AndroidPipeline + NosuspendOnHide
class MediaSessionVisibilityBrowserTest_AndroidPipeline_NosuspendOnHide :
      public MediaSessionVisibilityBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaSessionVisibilityBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableUnifiedMediaPipeline);
    command_line->AppendSwitch(switches::kDisableMediaSuspend);
  }
};

// The following tests are flaky. Re-enabling with logging to see what's
// happening on the bots. See crbug.com/619096.
INCLUDE_TEST_FROM_BASE_CLASS(
    MediaSessionVisibilityBrowserTest_AndroidPipeline_NosuspendOnHide,
    TestSessionSuspendedWhenHiddenAfterContentPause)
INCLUDE_TEST_FROM_BASE_CLASS(
   MediaSessionVisibilityBrowserTest_AndroidPipeline_NosuspendOnHide,
   TestSessionActiveWhenHiddenWhilePlaying)

#endif  // defined(OS_ANDROID)

}  // namespace content
