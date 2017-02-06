// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/media/webrtc/webrtc_internals.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "media/audio/audio_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace {

const int kExpectedConsumerId = 1;
const int kExpectedStreamId = 1;

// Get the ID for the render process host when there should only be one.
bool GetRenderProcessHostId(base::ProcessId* id) {
  content::RenderProcessHost::iterator it(
      content::RenderProcessHost::AllHostsIterator());
  *id = base::GetProcId(it.GetCurrentValue()->GetHandle());
  EXPECT_NE(base::kNullProcessId, *id);
  if (*id == base::kNullProcessId)
    return false;
  it.Advance();
  EXPECT_TRUE(it.IsAtEnd());
  return it.IsAtEnd();
}

// Get the expected AEC dump file name. The name will be
// <temporary path>.<render process id>.aec_dump.<consumer id>, for example
// "/tmp/.com.google.Chrome.Z6UC3P.12345.aec_dump.1".
base::FilePath GetExpectedAecDumpFileName(const base::FilePath& base_file,
                                          int render_process_id) {
  return base_file.AddExtension(IntToStringType(render_process_id))
                  .AddExtension(FILE_PATH_LITERAL("aec_dump"))
                  .AddExtension(IntToStringType(kExpectedConsumerId));
}

// Get the expected input audio file name. The name will be
// <temporary path>.<render process id>.source_input.<stream id>.pcm, for
// example "/tmp/.com.google.Chrome.Z6UC3P.12345.source_input.1.pcm".
base::FilePath GetExpectedInputAudioFileName(const base::FilePath& base_file,
                                             int render_process_id) {
  return base_file.AddExtension(IntToStringType(render_process_id))
                  .AddExtension(FILE_PATH_LITERAL("source_input"))
                  .AddExtension(IntToStringType(kExpectedStreamId))
                  .AddExtension(FILE_PATH_LITERAL("wav"));
}

}  // namespace

namespace content {

class WebRtcAudioDebugRecordingsBrowserTest : public WebRtcContentBrowserTest {
 public:
  WebRtcAudioDebugRecordingsBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }
  ~WebRtcAudioDebugRecordingsBrowserTest() override {}
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAudioDebugRecordings DISABLED_CallWithAudioDebugRecordings
#elif defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_CallWithAudioDebugRecordings DISABLED_CallWithAudioDebugRecordings
#elif defined(OS_ANDROID)
// Renderer crashes on Android M. https://crbug.com/535728.
#define MAYBE_CallWithAudioDebugRecordings DISABLED_CallWithAudioDebugRecordings
#else
#define MAYBE_CallWithAudioDebugRecordings CallWithAudioDebugRecordings
#endif

// This tests will make a complete PeerConnection-based call, verify that
// video is playing for the call, and verify that a non-empty AEC dump file
// exists. The AEC dump is enabled through webrtc-internals. The HTML and
// Javascript is bypassed since it would trigger a file picker dialog. Instead,
// the dialog callback FileSelected() is invoked directly. In fact, there's
// never a webrtc-internals page opened at all since that's not needed.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       MAYBE_CallWithAudioDebugRecordings) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath base_file;
  ASSERT_TRUE(CreateTemporaryFile(&base_file));
  base::DeleteFile(base_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab.
  WebRTCInternals::GetInstance()->FileSelected(base_file, -1, NULL);

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");

  EXPECT_FALSE(base::PathExists(base_file));

  // Verify that the expected AEC dump file exists and contains some data.
  base::ProcessId render_process_id = base::kNullProcessId;
  EXPECT_TRUE(GetRenderProcessHostId(&render_process_id));
  base::FilePath aec_dump_file = GetExpectedAecDumpFileName(base_file,
                                                            render_process_id);

  EXPECT_TRUE(base::PathExists(aec_dump_file));
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(aec_dump_file, &file_size));
  EXPECT_GT(file_size, 0);

  base::DeleteFile(aec_dump_file, false);

  // Verify that the expected input audio file exists and contains some data.
  base::FilePath input_audio_file =
      GetExpectedInputAudioFileName(base_file, render_process_id);

  EXPECT_TRUE(base::PathExists(input_audio_file));
  file_size = 0;
  EXPECT_TRUE(base::GetFileSize(input_audio_file, &file_size));
  EXPECT_GT(file_size, 0);

  base::DeleteFile(input_audio_file, false);
}

// TODO(grunell): Add test for multiple dumps when re-use of
// MediaStreamAudioProcessor in AudioCapturer has been removed.

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled DISABLED_CallWithAudioDebugRecordingsEnabledThenDisabled
#elif defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled DISABLED_CallWithAudioDebugRecordingsEnabledThenDisabled
#else
#define MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled CallWithAudioDebugRecordingsEnabledThenDisabled
#endif

// As above, but enable and disable dump before starting a call. The file should
// be created, but should be empty.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath base_file;
  ASSERT_TRUE(CreateTemporaryFile(&base_file));
  base::DeleteFile(base_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab, then disabling it.
  WebRTCInternals::GetInstance()->FileSelected(base_file, -1, NULL);
  WebRTCInternals::GetInstance()->DisableAudioDebugRecordings();

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");

  // Verify that the expected AEC dump file doesn't exist.
  base::ProcessId render_process_id = base::kNullProcessId;
  EXPECT_TRUE(GetRenderProcessHostId(&render_process_id));
  base::FilePath aec_dump_file = GetExpectedAecDumpFileName(base_file,
                                                            render_process_id);
  EXPECT_FALSE(base::PathExists(aec_dump_file));
  base::DeleteFile(aec_dump_file, false);

  // Verify that the expected input audio file doesn't exist.
  base::FilePath input_audio_file =
      GetExpectedInputAudioFileName(base_file, render_process_id);
  EXPECT_FALSE(base::PathExists(input_audio_file));
  base::DeleteFile(input_audio_file, false);
}

// Timing out on ARM linux bot: http://crbug.com/238490
// Renderer crashes under Android ASAN: https://crbug.com/408496.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       DISABLED_TwoCallsWithAudioDebugRecordings) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  // Create a second window.
  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell2, GURL(""));

  base::FilePath base_file;
  ASSERT_TRUE(CreateTemporaryFile(&base_file));
  base::DeleteFile(base_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab.
  WebRTCInternals::GetInstance()->FileSelected(base_file, -1, NULL);

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));

  NavigateToURL(shell(), url);
  NavigateToURL(shell2, url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell2, "call({video: true, audio: true});", &result));
  ASSERT_STREQ("OK", result.c_str());

  EXPECT_FALSE(base::PathExists(base_file));

  RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();

  for (; !it.IsAtEnd(); it.Advance()) {
    base::ProcessId render_process_id =
        base::GetProcId(it.GetCurrentValue()->GetHandle());
    EXPECT_NE(base::kNullProcessId, render_process_id);

    // Verify that the expected AEC dump file exists and contains some data.
    base::FilePath aec_dump_file =
        GetExpectedAecDumpFileName(base_file, render_process_id);

    EXPECT_TRUE(base::PathExists(aec_dump_file));
    int64_t file_size = 0;
    EXPECT_TRUE(base::GetFileSize(aec_dump_file, &file_size));
    EXPECT_GT(file_size, 0);

    base::DeleteFile(aec_dump_file, false);

    // Verify that the expected input audio file exists and contains some data.
    base::FilePath input_audio_file =
        GetExpectedInputAudioFileName(base_file, render_process_id);

    EXPECT_TRUE(base::PathExists(input_audio_file));
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(input_audio_file, &file_size));
    EXPECT_GT(file_size, 0);

    base::DeleteFile(input_audio_file, false);
  }
}

}  // namespace content
