// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcBrowserTest DISABLED_WebRtcBrowserTest
#else
#define MAYBE_WebRtcBrowserTest WebRtcBrowserTest
#endif

// This class tests the scenario when permission to access mic or camera is
// granted.
class MAYBE_WebRtcBrowserTest : public WebRtcContentBrowserTest {
 public:
  MAYBE_WebRtcBrowserTest() {}
  ~MAYBE_WebRtcBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTest::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  // Convenience function since most peerconnection-call.html tests just load
  // the page, kick off some javascript and wait for the title to change to OK.
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    MakeTypicalCall(javascript, "/media/peerconnection-call.html");
  }

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

    MakeTypicalPeerConnectionCall(javascript);
  }
};

// These tests will make a complete PeerConnection-based call and verify that
// video is playing for the call.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupDefaultVideoCall) {
  MakeTypicalPeerConnectionCall(
      "callAndExpectResolution({video: true}, 640, 480);");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallWith1To1AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 320,"
      " maxWidth: 320, minHeight: 320, maxHeight: 320}}}, 320, 320);";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallWith16To9AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: {minWidth: 640,"
      " maxWidth: 640, minAspectRatio: 1.777}}}, 640, 360);";
  MakeTypicalPeerConnectionCall(javascript);
}


IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallWith4To3AspectRatio) {
  const std::string javascript =
      "callAndExpectResolution({video: {mandatory: { minWidth: 320,"
      "maxWidth: 320, minAspectRatio: 1.333, maxAspectRatio: 1.333}}}, 320,"
      " 240);";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupVideoCallAndDisableLocalVideo) {
  const std::string javascript =
      "callAndDisableLocalVideo({video: true});";
  MakeTypicalPeerConnectionCall(javascript);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupAudioAndVideoCall) {
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
}


#if defined(OS_WIN) && !defined(NVALGRIND)
// Times out on Dr. Memory bots: https://crbug.com/545740
#define MAYBE_CanSetupCallAndSendDtmf DISABLED_CanSetupCallAndSendDtmf
#else
#define MAYBE_CanSetupCallAndSendDtmf CanSetupCallAndSendDtmf
#endif

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CanSetupCallAndSendDtmf) {
  MakeTypicalPeerConnectionCall("callAndSendDtmf(\'123,abc\');");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanMakeEmptyCallThenAddStreamsAndRenegotiate) {
  const char* kJavascript =
      "callEmptyThenAddOneStreamAndRenegotiate({video: true, audio: true});";
  MakeTypicalPeerConnectionCall(kJavascript);
}

// Causes asserts in libjingle: http://crbug.com/484826.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       DISABLED_CanMakeAudioCallAndThenRenegotiateToVideo) {
  const char* kJavascript =
      "callAndRenegotiateToVideo({audio: true}, {audio: true, video:true});";
  MakeTypicalPeerConnectionCall(kJavascript);
}

// Causes asserts in libjingle: http://crbug.com/484826.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       DISABLED_CanMakeVideoCallAndThenRenegotiateToAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndRenegotiateToAudio({audio: true, video:true}, {audio: true});");
}

// This test makes a call between pc1 and pc2 where a video only stream is sent
// from pc1 to pc2. The stream sent from pc1 to pc2 is cloned from the stream
// received on pc2 to test that cloning of remote video and audio tracks works
// as intended and is sent back to pc1.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CanForwardRemoteStream) {
#if defined (OS_ANDROID)
  // This test fails on Nexus 5 devices.
  // TODO(henrika): see http://crbug.com/362437 and http://crbug.com/359389
  // for details.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebRtcHWDecoding);
#endif
  MakeTypicalPeerConnectionCall(
      "callAndForwardRemoteStream({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       NoCrashWhenConnectChromiumSinkToRemoteTrack) {
  MakeTypicalPeerConnectionCall("ConnectChromiumSinkToRemoteAudioTrack();");
}

// This test will make a complete PeerConnection-based call but remove the
// MSID and bundle attribute from the initial offer to verify that
// video is playing for the call even if the initiating client don't support
// MSID. http://tools.ietf.org/html/draft-alvestrand-rtcweb-msid-02
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CanSetupAudioAndVideoCallWithoutMsidAndBundle) {
  MakeTypicalPeerConnectionCall("callWithoutMsidAndBundle();");
}

// This test will modify the SDP offer to an unsupported codec, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       NegotiateUnsupportedVideoCodec) {
  MakeTypicalPeerConnectionCall("negotiateUnsupportedVideoCodec();");
}

// This test will modify the SDP offer to use no encryption, which should
// cause SetLocalDescription to fail.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, NegotiateNonCryptoCall) {
  MakeTypicalPeerConnectionCall("negotiateNonCryptoCall();");
}

// This test can negotiate an SDP offer that includes a b=AS:xx to control
// the bandwidth for audio and video
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, NegotiateOfferWithBLine) {
  MakeTypicalPeerConnectionCall("negotiateOfferWithBLine();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CanSetupLegacyCall) {
  MakeTypicalPeerConnectionCall("callWithLegacySdp();");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallWithDataOnly) {
  MakeTypicalPeerConnectionCall("callWithDataOnly();");
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer: http://crbug.com/405951
#define MAYBE_CallWithSctpDataOnly DISABLED_CallWithSctpDataOnly
#else
#define MAYBE_CallWithSctpDataOnly CallWithSctpDataOnly
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, MAYBE_CallWithSctpDataOnly) {
  MakeTypicalPeerConnectionCall("callWithSctpDataOnly();");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and audio and video tracks.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallWithDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithDataAndMedia();");
}


#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer: http://crbug.com/405951
#define MAYBE_CallWithSctpDataAndMedia DISABLED_CallWithSctpDataAndMedia
#else
#define MAYBE_CallWithSctpDataAndMedia CallWithSctpDataAndMedia
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       MAYBE_CallWithSctpDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithSctpDataAndMedia();");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and later add an audio and video track.
// Doesn't work, therefore disabled: https://crbug.com/293252.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       DISABLED_CallWithDataAndLaterAddMedia) {
  MakeTypicalPeerConnectionCall("callWithDataAndLaterAddMedia();");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       CallWithNewVideoMediaStream) {
  MakeTypicalPeerConnectionCall("callWithNewVideoMediaStream();");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia. When video is flowing, the VideoTrack is removed and an
// AudioTrack is added instead.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallAndModifyStream) {
  MakeTypicalPeerConnectionCall(
      "callWithNewVideoMediaStreamLaterSwitchToAudio();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, AddTwoMediaStreamsToOnePC) {
  MakeTypicalPeerConnectionCall("addTwoMediaStreamsToOneConnection();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioVideoCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioIsPlaying({audio:true, video:true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioOnlyCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioIsPlaying({audio:true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishIsac16KCallAndEnsureAudioIsPlaying) {
  MakeAudioDetectingPeerConnectionCall(
      "callWithIsac16KAndEnsureAudioIsPlaying({audio:true});");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioVideoCallAndVerifyRemoteMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteAudioTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioVideoCallAndVerifyLocalMutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalAudioTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EnsureLocalVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureLocalVideoMutingDoesntMuteAudio();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EnsureRemoteVideoMuteDoesntMuteAudio) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureRemoteVideoMutingDoesntMuteAudio();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest,
                       EstablishAudioVideoCallAndVerifyUnmutingWorks) {
  MakeAudioDetectingPeerConnectionCall(
      "callAndEnsureAudioTrackUnmutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallAndVerifyVideoMutingWorks) {
  MakeTypicalPeerConnectionCall("callAndEnsureVideoTrackMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CreateOfferWithOfferOptions) {
  MakeTypicalPeerConnectionCall("testCreateOfferOptions();");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcBrowserTest, CallInsideIframe) {
  MakeTypicalPeerConnectionCall("callInsideIframe({video: true, audio:true});");
}

}  // namespace content
