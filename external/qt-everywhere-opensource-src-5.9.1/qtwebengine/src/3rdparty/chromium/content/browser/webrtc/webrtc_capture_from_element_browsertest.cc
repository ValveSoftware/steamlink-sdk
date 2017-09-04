// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "media/base/test_data_util.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/sys_info.h"
#endif

#if defined(ENABLE_MOJO_RENDERER)
// Remote mojo renderer does not send audio/video frames back to the renderer
// process and hence does not support capture: https://crbug.com/641559.
#define MAYBE_CaptureFromMediaElement DISABLED_CaptureFromMediaElement
#else
#define MAYBE_CaptureFromMediaElement CaptureFromMediaElement
#endif

namespace {

static const char kCanvasTestHtmlPage[] = "/media/canvas_capture_color.html";

static const char kVideoAudioHtmlFile[] =
    "/media/video_audio_element_capture_test.html";

static struct FileAndTypeParameters {
  bool has_video;
  bool has_audio;
  bool use_audio_tag;
  std::string filename;
} const kFileAndTypeParameters[] = {
    {true, false, false, "bear-320x240-video-only.webm"},
    {false, true, false, "bear-320x240-audio-only.webm"},
    {true, true, false, "bear-320x240.webm"},
    {false, true, true, "bear-320x240-audio-only.webm"},
};

}  // namespace

namespace content {

// This class is the browser tests for Media Capture from DOM Elements API,
// which allows for creation of a MediaStream out of a <canvas>, <video> or
// <audio> element.
class WebRtcCaptureFromElementBrowserTest
    : public WebRtcContentBrowserTestBase,
      public testing::WithParamInterface<struct FileAndTypeParameters> {
 public:
  WebRtcCaptureFromElementBrowserTest() {}
  ~WebRtcCaptureFromElementBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "MediaCaptureFromVideo");

    // Allow <video>/<audio>.play() when not initiated by user gesture.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcCaptureFromElementBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebRtcCaptureFromElementBrowserTest,
                       VerifyCanvasCaptureColor) {
  MakeTypicalCall("testCanvasCaptureColors();", kCanvasTestHtmlPage);
}

IN_PROC_BROWSER_TEST_P(WebRtcCaptureFromElementBrowserTest,
                       MAYBE_CaptureFromMediaElement) {
#if defined(OS_ANDROID)
  // TODO(mcasas): flaky on Lollipop Low-End devices, investigate and reconnect
  // https://crbug.com/626299
  if (base::SysInfo::IsLowEndDevice() &&
      base::android::BuildInfo::GetInstance()->sdk_int() <
          base::android::SDK_VERSION_MARSHMALLOW) {
    return;
  }
#endif

  MakeTypicalCall(
      base::StringPrintf("testCaptureFromMediaElement(\"%s\", %d, %d, %d);",
                         GetParam().filename.c_str(),
                         GetParam().has_video,
                         GetParam().has_audio,
                         GetParam().use_audio_tag),
      kVideoAudioHtmlFile);
}

INSTANTIATE_TEST_CASE_P(,
                        WebRtcCaptureFromElementBrowserTest,
                        testing::ValuesIn(kFileAndTypeParameters));
}  // namespace content
