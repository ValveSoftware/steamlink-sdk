// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcAudioBrowserTest DISABLED_WebRtcAudioBrowserTest
#else
#define MAYBE_WebRtcAudioBrowserTest WebRtcAudioBrowserTest
#endif

// This class tests the scenario when permission to access mic or camera is
// granted.
class MAYBE_WebRtcAudioBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcAudioBrowserTest() {}
  ~MAYBE_WebRtcAudioBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  // Convenience method for making calls that detect if audio os playing (which
  // has some special prerequisites, such that there needs to be an audio output
  // device on the executing machine).
  void MakeAudioDetectingPeerConnectionCall(const std::string& javascript) {
    if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
      // Bots with no output devices will force the audio code into a state
      // where it doesn't manage to set either the low or high latency path.
      // This test will compute useless values in that case, so skip running on
      // such bots (see crbug.com/326338).
      LOG(INFO) << "Missing output devices: skipping test...";
      return;
    }

    ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream))
            << "Must run with fake devices since the test will explicitly look "
            << "for the fake device signal.";

    MakeTypicalCall(javascript, "/media/peerconnection-call-audio.html");
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       CanMakeVideoCallAndThenRenegotiateToAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndRenegotiateToAudio({audio: true, video:true}, {audio: true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioIsPlaying({audio:true, video:true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EstablishAudioOnlyCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioIsPlaying({audio:true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EstablishIsac16KCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callWithIsac16KAndEnsureAudioIsPlaying({audio:true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyRemoteMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteAudioTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyLocalMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalAudioTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EnsureLocalVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalVideoMutingDoesntMuteAudio();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EnsureRemoteVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteVideoMutingDoesntMuteAudio();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioBrowserTest,
                       EstablishAudioVideoCallAndVerifyUnmutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioTrackUnmutingWorks();");
}

}  // namespace content
