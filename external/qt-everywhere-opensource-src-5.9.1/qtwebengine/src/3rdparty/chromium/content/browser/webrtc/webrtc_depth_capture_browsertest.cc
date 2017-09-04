// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

static const char kGetDepthStreamAndCallCreateImageBitmap[] =
    "getDepthStreamAndCallCreateImageBitmap";

void RemoveSwitchFromCommandLine(base::CommandLine* command_line,
                                 const std::string& switch_value) {
  base::CommandLine::StringVector argv = command_line->argv();
  const base::CommandLine::StringType switch_string =
#if defined(OS_WIN)
      base::ASCIIToUTF16(switch_value);
#else
      switch_value;
#endif
  argv.erase(std::remove_if(
                 argv.begin(), argv.end(),
                 [switch_string](const base::CommandLine::StringType& value) {
                   return value.find(switch_string) != std::string::npos;
                 }),
             argv.end());
  command_line->InitFromArgv(argv);
}

}  // namespace

namespace content {

class WebRtcDepthCaptureBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  WebRtcDepthCaptureBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }
  ~WebRtcDepthCaptureBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Test using two video capture devices - a color and a 16-bit depth device.
    // By default, command line argument is present with no value.  We need to
    // remove it and then add the value defining two video capture devices.
    const std::string fake_device_switch =
        switches::kUseFakeDeviceForMediaStream;
    ASSERT_TRUE(command_line->HasSwitch(fake_device_switch) &&
                command_line->GetSwitchValueASCII(fake_device_switch).empty());
    RemoveSwitchFromCommandLine(command_line, fake_device_switch);
    command_line->AppendSwitchASCII(fake_device_switch, "device-count=2");
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcDepthCaptureBrowserTest,
                       GetDepthStreamAndCallCreateImageBitmap) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/media/getusermedia-depth-capture.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(base::StringPrintf(
      "%s({video: true});", kGetDepthStreamAndCallCreateImageBitmap));
}

}  // namespace content
