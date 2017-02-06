// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_browsertest.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/test_data_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

// Common test results.
const char MediaBrowserTest::kEnded[] = "ENDED";
const char MediaBrowserTest::kError[] = "ERROR";
const char MediaBrowserTest::kFailed[] = "FAILED";

void MediaBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(
      switches::kDisableGestureRequirementForMediaPlayback);
}

void MediaBrowserTest::RunMediaTestPage(const std::string& html_page,
                                        const base::StringPairs& query_params,
                                        const std::string& expected_title,
                                        bool http) {
  GURL gurl;
  std::string query = media::GetURLQueryString(query_params);
  std::unique_ptr<net::EmbeddedTestServer> http_test_server;
  if (http) {
    http_test_server.reset(new net::EmbeddedTestServer);
    http_test_server->ServeFilesFromSourceDirectory(media::GetTestDataPath());
    CHECK(http_test_server->Start());
    gurl = http_test_server->GetURL("/" + html_page + "?" + query);
  } else {
    gurl = content::GetFileUrlWithQuery(media::GetTestDataFilePath(html_page),
                                        query);
  }
  std::string final_title = RunTest(gurl, expected_title);
  EXPECT_EQ(expected_title, final_title);
}

std::string MediaBrowserTest::RunTest(const GURL& gurl,
                                      const std::string& expected_title) {
  VLOG(0) << "Running test URL: " << gurl;
  TitleWatcher title_watcher(shell()->web_contents(),
                             base::ASCIIToUTF16(expected_title));
  AddWaitForTitles(&title_watcher);
  NavigateToURL(shell(), gurl);
  base::string16 result = title_watcher.WaitAndGetTitle();
  return base::UTF16ToASCII(result);
}

void MediaBrowserTest::AddWaitForTitles(content::TitleWatcher* title_watcher) {
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEnded));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kError));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kFailed));
}

// Tests playback and seeking of an audio or video file over file or http based
// on a test parameter.  Test starts with playback, then, after X seconds or the
// ended event fires, seeks near end of file; see player.html for details.  The
// test completes when either the last 'ended' or an 'error' event fires.
class MediaTest : public testing::WithParamInterface<bool>,
                  public MediaBrowserTest {
 public:
  // Play specified audio over http:// or file:// depending on |http| setting.
  void PlayAudio(const std::string& media_file, bool http) {
    PlayMedia("audio", media_file, http);
  }

  // Play specified video over http:// or file:// depending on |http| setting.
  void PlayVideo(const std::string& media_file, bool http) {
    PlayMedia("video", media_file, http);
  }

  // Run specified color format test with the expected result.
  void RunColorFormatTest(const std::string& media_file,
                          const std::string& expected) {
    base::FilePath test_file_path =
        media::GetTestDataFilePath("blackwhite.html");
    RunTest(GetFileUrlWithQuery(test_file_path, media_file), expected);
  }

  void PlayMedia(const std::string& tag,
                 const std::string& media_file,
                 bool http) {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair(tag, media_file));
    RunMediaTestPage("player.html", query_params, kEnded, http);
  }

  void RunVideoSizeTest(const char* media_file, int width, int height) {
    std::string expected;
    expected += base::IntToString(width);
    expected += " ";
    expected += base::IntToString(height);
    base::StringPairs query_params;
    query_params.push_back(std::make_pair("video", media_file));
    RunMediaTestPage("player.html", query_params, expected, false);
  }
};

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearTheora) {
  PlayVideo("bear.ogv", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentTheora) {
  PlayVideo("bear_silent.ogv", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWebm) {
  PlayVideo("bear.webm", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearOpusWebm) {
  PlayVideo("bear-opus.webm", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearOpusOgg) {
  PlayVideo("bear-opus.ogg", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm", GetParam());
}

#if defined(USE_PROPRIETARY_CODECS)
// Crashes on Mac only.  http://crbug.com/621857
#if defined(OS_MACOSX)
#define MAYBE_VideoBearMp4 DISABLED_VideoBearMp4
#else
#define MAYBE_VideoBearMp4 VideoBearMp4
#endif
IN_PROC_BROWSER_TEST_P(MediaTest, MAYBE_VideoBearMp4) {
  PlayVideo("bear.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearHighBitDepthMp4) {
  PlayVideo("bear-320x180-hi10p.mp4", GetParam());
}

// Crashes on Mac only.  http://crbug.com/621857
#if defined(OS_MACOSX)
#define MAYBE_VideoBearSilentMp4 DISABLED_VideoBearSilentMp4
#else
#define MAYBE_VideoBearSilentMp4 VideoBearSilentMp4
#endif
IN_PROC_BROWSER_TEST_P(MediaTest, MAYBE_VideoBearSilentMp4) {
  PlayVideo("bear_silent.mp4", GetParam());
}

// While we support the big endian (be) PCM codecs on Chromium, Quicktime seems
// to be the only creator of this format and only for .mov files.
// TODO(dalecurtis/ihf): Find or create some .wav test cases for "be" format.
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS16be) {
  PlayVideo("bear_pcm_s16be.mov", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS24be) {
  PlayVideo("bear_pcm_s24be.mov", GetParam());
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated0) {
  RunVideoSizeTest("bear_rotate_0.mp4", 1280, 720);
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated90) {
  RunVideoSizeTest("bear_rotate_90.mp4", 720, 1280);
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated180) {
  RunVideoSizeTest("bear_rotate_180.mp4", 1280, 720);
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearRotated270) {
  RunVideoSizeTest("bear_rotate_270.mp4", 720, 1280);
}
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(OS_CHROMEOS)
#if defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Mpeg4) {
  PlayVideo("bear_mpeg4_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Mpeg4Asp) {
  PlayVideo("bear_mpeg4asp_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Divx) {
  PlayVideo("bear_divx_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear3gpAacH264) {
  PlayVideo("bear_h264_aac.3gp", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear3gpAmrnbMpeg4) {
  PlayVideo("bear_mpeg4_amrnb.3gp", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavGsmms) {
  PlayAudio("bear_gsm_ms.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearFlac) {
  PlayAudio("bear.flac", GetParam());
}
#endif  // defined(USE_PROPRIETARY_CODECS)
#endif  // defined(OS_CHROMEOS)


IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavAlaw) {
  PlayAudio("bear_alaw.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavMulaw) {
  PlayAudio("bear_mulaw.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm) {
  PlayAudio("bear_pcm.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm3kHz) {
  PlayAudio("bear_3kHz.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm192kHz) {
  PlayAudio("bear_192kHz.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoTulipWebm) {
  PlayVideo("tulip2.webm", GetParam());
}

// Covers tear-down when navigating away as opposed to browser exiting.
IN_PROC_BROWSER_TEST_F(MediaTest, Navigate) {
  PlayVideo("bear.ogv", false);
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

INSTANTIATE_TEST_CASE_P(File, MediaTest, ::testing::Values(false));
INSTANTIATE_TEST_CASE_P(Http, MediaTest, ::testing::Values(true));

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv420pTheora) {
  RunColorFormatTest("yuv420p.ogv", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv422pTheora) {
  RunColorFormatTest("yuv422p.ogv", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv444pTheora) {
  RunColorFormatTest("yuv444p.ogv", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv420pVp8) {
  RunColorFormatTest("yuv420p.webm", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv444pVp9) {
  RunColorFormatTest("yuv444p.webm", "ENDED");
}

#if defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(MediaTest, Yuv420pH264) {
  RunColorFormatTest("yuv420p.mp4", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv420pRec709H264) {
  RunColorFormatTest("yuv420p_rec709.mp4", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv420pHighBitDepth) {
  RunColorFormatTest("yuv420p_hi10p.mp4", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuvj420pH264) {
  RunColorFormatTest("yuvj420p.mp4", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv422pH264) {
  RunColorFormatTest("yuv422p.mp4", kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaTest, Yuv444pH264) {
  RunColorFormatTest("yuv444p.mp4", kEnded);
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MediaTest, Yuv420pMpeg4) {
  RunColorFormatTest("yuv420p.avi", kEnded);
}
#endif  // defined(OS_CHROMEOS)
#endif  // defined(USE_PROPRIETARY_CODECS)

}  // namespace content
