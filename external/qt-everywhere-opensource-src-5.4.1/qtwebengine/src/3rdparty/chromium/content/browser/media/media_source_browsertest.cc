// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/common/content_switches.h"
#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

// Common media types.
const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
#if !defined(OS_ANDROID)
const char kWebMOpusAudioOnly[] = "audio/webm; codecs=\"opus\"";
#endif
const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";

namespace content {

// MSE is available on all desktop platforms and on Android 4.1 and later.
static bool IsMSESupported() {
#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 16) {
    VLOG(0) << "MSE is only supported in Android 4.1 and later.";
    return false;
  }
#endif  // defined(OS_ANDROID)
  return true;
}

class MediaSourceTest : public content::MediaBrowserTest {
 public:
  void TestSimplePlayback(const char* media_file, const char* media_type,
                          const char* expectation) {
    if (!IsMSESupported()) {
      VLOG(0) << "Skipping test - MSE not supported.";
      return;
    }

    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("mediafile", media_file));
    query_params.push_back(std::make_pair("mediatype", media_type));
    RunMediaTestPage("media_source_player.html", &query_params, expectation,
                     true);
  }

#if defined(OS_ANDROID)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
  }
#endif
};

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240.webm", kWebMAudioVideo, kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-video-only.webm", kWebMVideoOnly, kEnded);
}

// Opus is not supported in Android as of now.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_Opus_WebM) {
  TestSimplePlayback("bear-opus.webm", kWebMOpusAudioOnly, kEnded);
}
#endif

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-320x240-audio-only.webm", kWebMAudioOnly, kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Type_Error) {
  TestSimplePlayback("bear-320x240-video-only.webm", kWebMAudioOnly, kError);
}

// Flaky test crbug.com/246308
// Test changed to skip checks resulting in flakiness. Proper fix still needed.
IN_PROC_BROWSER_TEST_F(MediaSourceTest, ConfigChangeVideo) {
  if (!IsMSESupported()) {
    VLOG(0) << "Skipping test - MSE not supported.";
    return;
  }
  RunMediaTestPage("mse_config_change.html", NULL, kEnded, true);
}

}  // namespace content
