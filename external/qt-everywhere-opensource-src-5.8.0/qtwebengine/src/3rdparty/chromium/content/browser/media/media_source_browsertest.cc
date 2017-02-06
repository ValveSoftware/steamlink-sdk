// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/common/content_switches.h"
#include "media/media_features.h"
#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

// Common media types.
#if defined(USE_PROPRIETARY_CODECS) && !defined(OS_ANDROID)
const char kAAC_ADTS_AudioOnly[] = "audio/aac";
#endif
const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
#if !defined(OS_ANDROID)
const char kWebMOpusAudioOnly[] = "audio/webm; codecs=\"opus\"";
#endif
const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";

#if defined(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
const char kMp2tAudioVideo[] = "video/mp2t; codecs=\"mp4a.40.2, avc1.42E01E\"";
#endif
#endif

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
  void TestSimplePlayback(const std::string& media_file,
                          const std::string& media_type,
                          const std::string& expectation) {
    if (!IsMSESupported()) {
      VLOG(0) << "Skipping test - MSE not supported.";
      return;
    }

    base::StringPairs query_params;
    query_params.push_back(std::make_pair("mediaFile", media_file));
    query_params.push_back(std::make_pair("mediaType", media_type));
    RunMediaTestPage("media_source_player.html", query_params, expectation,
                     false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
  }
};

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240.webm", kWebMAudioVideo, kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-video-only.webm", kWebMVideoOnly, kEnded);
}

// TODO(servolk): Android is supposed to support AAC in ADTS container with
// 'audio/aac' mime type, but for some reason playback fails on trybots due to
// some issue in OMX AAC decoder (crbug.com/528361)
#if defined(USE_PROPRIETARY_CODECS) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_AAC_ADTS) {
  TestSimplePlayback("sfx.adts", kAAC_ADTS_AudioOnly, kEnded);
}
#endif

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
  RunMediaTestPage("mse_config_change.html", base::StringPairs(), kEnded, true);
}

#if defined(USE_PROPRIETARY_CODECS)

// TODO(chcunningham): Figure out why this is flaky on android. crbug/607841
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Video_MP4_Audio_WEBM) {
  if (!IsMSESupported()) {
    VLOG(0) << "Skipping test - MSE not supported.";
    return;
  }
  base::StringPairs query_params;
  query_params.push_back(std::make_pair("videoFormat", "CLEAR_MP4"));
  query_params.push_back(std::make_pair("audioFormat", "CLEAR_WEBM"));
  RunMediaTestPage("mse_different_containers.html", query_params, kEnded, true);
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Video_WEBM_Audio_MP4) {
  if (!IsMSESupported()) {
    VLOG(0) << "Skipping test - MSE not supported.";
    return;
  }
  base::StringPairs query_params;
  query_params.push_back(std::make_pair("videoFormat", "CLEAR_WEBM"));
  query_params.push_back(std::make_pair("audioFormat", "CLEAR_MP4"));
  RunMediaTestPage("mse_different_containers.html", query_params, kEnded, true);
}
#endif

#if defined(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioVideo_Mp2t) {
  TestSimplePlayback("bear-1280x720.ts", kMp2tAudioVideo, kEnded);
}
#endif
#endif
}  // namespace content
