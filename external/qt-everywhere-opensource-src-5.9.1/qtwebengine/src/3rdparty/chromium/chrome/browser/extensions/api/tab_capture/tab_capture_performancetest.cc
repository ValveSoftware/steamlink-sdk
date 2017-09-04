// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/win/windows_version.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/tracing.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/complex_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

namespace {

const char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

enum TestFlags {
  kUseGpu              = 1 << 0, // Only execute test if --enable-gpu was given
                                 // on the command line.  This is required for
                                 // tests that run on GPU.
  kForceGpuComposited  = 1 << 1, // Force the test to use the compositor.
  kDisableVsync        = 1 << 2, // Do not limit framerate to vertical refresh.
                                 // when on GPU, nor to 60hz when not on GPU.
  kTestThroughWebRTC   = 1 << 3, // Send captured frames through webrtc
  kSmallWindow         = 1 << 4, // 1 = 800x600, 0 = 2000x1000
};

class TabCapturePerformanceTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<int> {
 public:
  TabCapturePerformanceTest() {}

  bool HasFlag(TestFlags flag) const {
    return (GetParam() & flag) == flag;
  }

  bool IsGpuAvailable() const {
    return base::CommandLine::ForCurrentProcess()->HasSwitch("enable-gpu");
  }

  std::string GetSuffixForTestFlags() {
    std::string suffix;
    if (HasFlag(kForceGpuComposited))
      suffix += "_comp";
    if (HasFlag(kUseGpu))
      suffix += "_gpu";
    if (HasFlag(kDisableVsync))
      suffix += "_novsync";
    if (HasFlag(kTestThroughWebRTC))
      suffix += "_webrtc";
    if (HasFlag(kSmallWindow))
      suffix += "_small";
    return suffix;
  }

  void SetUp() override {
    EnablePixelOutput();
    ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Some of the tests may launch http requests through JSON or AJAX
    // which causes a security error (cross domain request) when the page
    // is loaded from the local file system ( file:// ). The following switch
    // fixes that error.
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

    if (HasFlag(kSmallWindow)) {
      command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
    } else {
      command_line->AppendSwitchASCII(switches::kWindowSize, "2000,1500");
    }

    if (!HasFlag(kUseGpu))
      command_line->AppendSwitch(switches::kDisableGpu);

    if (HasFlag(kDisableVsync))
      command_line->AppendSwitch(switches::kDisableGpuVsync);

    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        kExtensionId);
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  bool PrintResults(trace_analyzer::TraceAnalyzer *analyzer,
                    const std::string& test_name,
                    const std::string& event_name,
                    const std::string& unit) {
    trace_analyzer::TraceEventVector events;
    trace_analyzer::Query query =
        trace_analyzer::Query::EventNameIs(event_name) &&
        (trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_COMPLETE) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_FLOW_BEGIN) ||
         trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_INSTANT));
    analyzer->FindEvents(query, &events);
    if (events.size() < 20) {
      LOG(ERROR) << "Not enough events of type " << event_name << " found ("
                 << events.size() << ").";
      return false;
    }

    // Ignore some events for startup/setup/caching.
    trace_analyzer::TraceEventVector rate_events(events.begin() + 3,
                                                 events.end() - 3);
    trace_analyzer::RateStats stats;
    if (!GetRateStats(rate_events, &stats, NULL)) {
      LOG(ERROR) << "GetRateStats failed";
      return false;
    }
    double mean_ms = stats.mean_us / 1000.0;
    double std_dev_ms = stats.standard_deviation_us / 1000.0;
    std::string mean_and_error = base::StringPrintf("%f,%f", mean_ms,
                                                    std_dev_ms);
    perf_test::PrintResultMeanAndError(test_name,
                                       GetSuffixForTestFlags(),
                                       event_name,
                                       mean_and_error,
                                       unit,
                                       true);
    return true;
  }

  void RunTest(const std::string& test_name) {
    if (HasFlag(kUseGpu) && !IsGpuAvailable()) {
      LOG(WARNING) <<
          "Test skipped: requires gpu. Pass --enable-gpu on the command "
          "line if use of GPU is desired.";
      return;
    }

    std::string json_events;
    ASSERT_TRUE(tracing::BeginTracing("gpu,gpu.capture"));
    std::string page = "performance.html";
    page += HasFlag(kTestThroughWebRTC) ? "?WebRTC=1" : "?WebRTC=0";
    // Ideally we'd like to run a higher capture rate when vsync is disabled,
    // but libjingle currently doesn't allow that.
    // page += HasFlag(kDisableVsync) ? "&fps=300" : "&fps=30";
    page += "&fps=60";
    ASSERT_TRUE(RunExtensionSubtest("tab_capture", page)) << message_;
    ASSERT_TRUE(tracing::EndTracing(&json_events));
    std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer;
    analyzer.reset(trace_analyzer::TraceAnalyzer::Create(json_events));

    // The printed result will be the average time between frames in the
    // browser window.
    bool gpu_frames = PrintResults(
        analyzer.get(),
        test_name,
        "RenderWidget::DidCommitAndDrawCompositorFrame",
        "ms");
    EXPECT_TRUE(gpu_frames);

    // This prints out the average time between capture events.
    // As the capture frame rate is capped at 30fps, this score
    // cannot get any better than (lower) 33.33 ms.
    // TODO(ericrk): Remove the "Capture" result once we are confident that
    // "CaptureSucceeded" is giving the coverage we want. crbug.com/489817
    EXPECT_TRUE(PrintResults(analyzer.get(),
                             test_name,
                             "Capture",
                             "ms"));

    // Also track the CaptureSucceeded event. Capture only indicates that a
    // capture was requested, but this capture may later be aborted without
    // running. CaptureSucceeded tracks successful frame captures.
    EXPECT_TRUE(
        PrintResults(analyzer.get(), test_name, "CaptureSucceeded", "ms"));
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_P(TabCapturePerformanceTest, Performance) {
  RunTest("TabCapturePerformance");
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_CASE_P(
    ,
    TabCapturePerformanceTest,
    testing::Values(
        0,
        kUseGpu | kForceGpuComposited,
        kDisableVsync,
        kDisableVsync | kUseGpu | kForceGpuComposited,
        kTestThroughWebRTC,
        kTestThroughWebRTC | kUseGpu | kForceGpuComposited,
        kTestThroughWebRTC | kDisableVsync,
        kTestThroughWebRTC | kDisableVsync | kUseGpu | kForceGpuComposited));

