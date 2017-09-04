// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_webcam_browsertest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

#if defined(OS_WIN)
// These tests are flaky on WebRTC Windows bots: https://crbug.com/633242.
#define MAYBE_GetCapabilities DISABLED_GetCapabilities
#define MAYBE_TakePhoto DISABLED_TakePhoto
#define MAYBE_GrabFrame DISABLED_GrabFrame
#else
#define MAYBE_GetCapabilities GetCapabilities
#define MAYBE_TakePhoto TakePhoto
#define MAYBE_GrabFrame GrabFrame
#endif

namespace {

static const char kImageCaptureHtmlFile[] = "/media/image_capture_test.html";

// TODO(mcasas): enable real-camera tests by disabling the Fake Device for
// platforms where the ImageCaptureCode is landed, https://crbug.com/656810
static struct TargetCamera {
  bool use_fake;
} const kTestParameters[] = {
    {true},
#if defined(OS_LINUX)
    {false}
#endif
};

}  // namespace

// This class is the content_browsertests for Image Capture API, which allows
// for capturing still images out of a MediaStreamTrack. Is a
// WebRtcWebcamBrowserTest to be able to use a physical camera.
class WebRtcImageCaptureBrowserTest
    : public WebRtcWebcamBrowserTest,
      public testing::WithParamInterface<struct TargetCamera> {
 public:
  WebRtcImageCaptureBrowserTest() = default;
  ~WebRtcImageCaptureBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcWebcamBrowserTest::SetUpCommandLine(command_line);

    ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    if (GetParam().use_fake) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kUseFakeDeviceForMediaStream);
      ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream));
    }

    // "GetUserMedia": enables navigator.mediaDevices.getUserMedia();
    // TODO(mcasas): remove GetUserMedia after https://crbug.com/503227.
    // "ImageCapture": enables the ImageCapture API.
    // TODO(mcasas): remove ImageCapture after https://crbug.com/603328.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "GetUserMedia,ImageCapture");
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    WebRtcWebcamBrowserTest::SetUp();
  }

  // Tries to run a |command| JS test, returning true if the test can be safely
  // skipped or it works as intended, or false otherwise.
  bool RunImageCaptureTestCase(const std::string& command) {
#if defined(OS_ANDROID)
    // TODO(mcasas): fails on Lollipop devices: https://crbug.com/634811
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_MARSHMALLOW) {
      return true;
    }
#endif

    GURL url(embedded_test_server()->GetURL(kImageCaptureHtmlFile));
    NavigateToURL(shell(), url);

    if (!IsWebcamAvailableOnSystem(shell()->web_contents())) {
      DVLOG(1) << "No video device; skipping test...";
      return true;
    }

    std::string result;
    if (!ExecuteScriptAndExtractString(shell(), command, &result))
      return false;
    DLOG_IF(ERROR, result != "OK") << result;
    return result == "OK";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcImageCaptureBrowserTest);
};

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureBrowserTest, MAYBE_GetCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGetCapabilities()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureBrowserTest, MAYBE_TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhoto()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureBrowserTest, MAYBE_GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrame()"));
}

INSTANTIATE_TEST_CASE_P(,
                        WebRtcImageCaptureBrowserTest,
                        testing::ValuesIn(kTestParameters));

}  // namespace content
