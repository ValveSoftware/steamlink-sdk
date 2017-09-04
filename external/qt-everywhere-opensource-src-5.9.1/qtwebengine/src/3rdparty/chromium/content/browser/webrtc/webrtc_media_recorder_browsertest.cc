// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/media_switches.h"

namespace {

static const char kMediaRecorderHtmlFile[] = "/media/mediarecorder_test.html";

static struct EncodingParameters {
  bool disable_accelerator;
  std::string video_codec;
} const kEncodingParameters[] = {
    {true, "VP8"},  {true, "VP9"},  {true, "H264"},
    {false, "VP8"}, {false, "VP9"}, {false, "H264"},
};

}  // namespace

namespace content {

// This class tests the recording of a media stream.
class WebRtcMediaRecorderTest
    : public WebRtcContentBrowserTestBase,
      public testing::WithParamInterface<struct EncodingParameters> {
 public:
  WebRtcMediaRecorderTest() {}
  ~WebRtcMediaRecorderTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);

    AppendUseFakeUIForMediaStreamFlag();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "GetUserMedia");
  }

  void MaybeForceDisableEncodeAccelerator(bool disable) {
    if (!disable)
      return;
    // This flag is also used for encoding, https://crbug.com/616640.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableAcceleratedVideoDecode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcMediaRecorderTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, Start) {
  MakeTypicalCall("testStartAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, StartAndStop) {
  MakeTypicalCall("testStartStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, StartAndDataAvailable) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testStartAndDataAvailable(\"%s\");",
                                     GetParam().video_codec.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, StartWithTimeSlice) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testStartWithTimeSlice(\"%s\");",
                                     GetParam().video_codec.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, Resume) {
  MakeTypicalCall("testResumeAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, NoResumeWhenRecorderInactive) {
  MakeTypicalCall("testIllegalResumeThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, ResumeAndDataAvailable) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testResumeAndDataAvailable(\"%s\");",
                                     GetParam().video_codec.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, Pause) {
  MakeTypicalCall("testPauseAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, PauseStop) {
  MakeTypicalCall("testPauseStopAndRecorderState();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       PausePreventsDataavailableFromBeingFired) {
  MakeTypicalCall("testPausePreventsDataavailableFromBeingFired();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, IllegalPauseThrowsDOMError) {
  MakeTypicalCall("testIllegalPauseThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, TwoChannelAudioRecording) {
  MakeTypicalCall("testTwoChannelAudio();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest, IllegalStopThrowsDOMError) {
  MakeTypicalCall("testIllegalStopThrowsDOMError();", kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       IllegalStartWhileRecordingThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInRecordingStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       IllegalStartWhilePausedThrowsDOMError) {
  MakeTypicalCall("testIllegalStartInPausedStateThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       IllegalRequestDataThrowsDOMError) {
  MakeTypicalCall("testIllegalRequestDataThrowsDOMError();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_P(WebRtcMediaRecorderTest, PeerConnection) {
  MaybeForceDisableEncodeAccelerator(GetParam().disable_accelerator);
  MakeTypicalCall(base::StringPrintf("testRecordRemotePeerConnection(\"%s\");",
                                     GetParam().video_codec.c_str()),
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       AddingTrackToMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testAddingTrackToMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

IN_PROC_BROWSER_TEST_F(WebRtcMediaRecorderTest,
                       RemovingTrackFromMediaStreamFiresErrorEvent) {
  MakeTypicalCall("testRemovingTrackFromMediaStreamFiresErrorEvent();",
                  kMediaRecorderHtmlFile);
}

INSTANTIATE_TEST_CASE_P(,
                        WebRtcMediaRecorderTest,
                        testing::ValuesIn(kEncodingParameters));

}  // namespace content
