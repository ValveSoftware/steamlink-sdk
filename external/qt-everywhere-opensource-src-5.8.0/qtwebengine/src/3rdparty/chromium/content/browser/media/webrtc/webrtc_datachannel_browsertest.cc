// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "content/public/common/content_switches.h"
#include "content/test/webrtc_content_browsertest_base.h"

namespace {

const char kJavaScriptFeaturesNeeded[] = "--expose-gc";
const char kDataChannelHtmlFile[] = "/media/datachannel_test.html";

}  // namespace

namespace content {

#if defined(OS_WIN) || (defined(OS_ANDROID) && defined(ADDRESS_SANITIZER))
// GC does not work correctly in some cases under Android ASAN.
// See https://crbug.com/611620.
// This test has become flaky on Windows. See https://crbug.com/616388
#define MAYBE_WebRtcDataChannelTest DISABLED_WebRtcDataChannelTest
#else
#define MAYBE_WebRtcDataChannelTest WebRtcDataChannelTest
#endif

class MAYBE_WebRtcDataChannelTest : public WebRtcContentBrowserTest {
 public:
  MAYBE_WebRtcDataChannelTest() {}
  ~MAYBE_WebRtcDataChannelTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTest::SetUpCommandLine(command_line);
    AppendUseFakeUIForMediaStreamFlag();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kJavaScriptFlags, kJavaScriptFeaturesNeeded);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_WebRtcDataChannelTest);
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcDataChannelTest, DataChannelGC) {
  MakeTypicalCall("testDataChannelGC();", kDataChannelHtmlFile);
}

}  // namespace content
