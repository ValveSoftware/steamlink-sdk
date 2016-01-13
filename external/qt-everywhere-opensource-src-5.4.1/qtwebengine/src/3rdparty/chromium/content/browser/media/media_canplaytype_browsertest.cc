// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/browser/media/media_browsertest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

const char kProbably[] = "probably";
const char kMaybe[] = "maybe";
const char kNot[] = "";

#if defined(USE_PROPRIETARY_CODECS)
const char kPropProbably[] = "probably";
const char kPropMaybe[] = "maybe";
#else
const char kPropProbably[] = "";
const char kPropMaybe[] = "";
#endif  // USE_PROPRIETARY_CODECS

// TODO(amogh.bihani): Change the opus tests when opus is  on
// Android. (http://crbug.com/318436).
#if !defined(OS_ANDROID)
const char kOggVideoProbably[] = "probably";
const char kOggVideoMaybe[] = "maybe";
const char kTheoraProbably[] = "probably";
const char kOpusProbably[] = "probably";
#if defined(USE_PROPRIETARY_CODECS)
const char kTheoraAndPropProbably[] = "probably";
const char kOpusAndPropProbably[] = "probably";
#else
const char kTheoraAndPropProbably[] = "";
const char kOpusAndPropProbably[] = "";
#endif  // USE_PROPRIETARY_CODECS
#else
const char kOggVideoProbably[] = "";
const char kOggVideoMaybe[] = "";
const char kTheoraProbably[] = "";
const char kOpusProbably[] = "";
const char kTheoraAndPropProbably[] = "maybe";
const char kOpusAndPropProbably[] = "maybe";
#endif  // !OS_ANDROID

namespace content {

class MediaCanPlayTypeTest : public MediaBrowserTest {
public:
  MediaCanPlayTypeTest() : url_("about:blank") { }

  virtual void SetUpOnMainThread() OVERRIDE {
    NavigateToURL(shell(), url_);
  }

  std::string CanPlay(const std::string& type) {
    std::string command("document.createElement('video').canPlayType(");
    command.append(type);
    command.append(")");

    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        shell()->web_contents(),
        "window.domAutomationController.send(" + command + ");",
        &result));
    return result;
  }

private:
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(MediaCanPlayTypeTest);
};

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_wav) {
  EXPECT_EQ(kMaybe, CanPlay("'audio/wav'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/wav; codecs=\"1\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"theora\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"vp9.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"mp4a.40.5\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"1, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"1, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"1, theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"1, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"opus, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"opus, theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"vorbis, mp4a\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/wav; codecs=\"unknown\"'"));

  EXPECT_EQ(kMaybe, CanPlay("'audio/x-wav'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/x-wav; codecs=\"1\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"theora\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"vp9.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"mp4a.40.5\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"1, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"1, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"1, theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"1, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"opus, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"opus, theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"vorbis, mp4a\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-wav; codecs=\"unknown\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_webm) {
  // On Android, VP9 is supported only on KitKat and above (API level 19).
  std::string VP9Probably = "probably";
  std::string VP9AndOpusProbably = "probably";
#if defined(OS_ANDROID)
  VP9AndOpusProbably = "";
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 19)
    VP9Probably = "";
#endif
  EXPECT_EQ(kMaybe, CanPlay("'video/webm'"));

  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8, vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0, vorbis\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'video/webm; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'video/webm; codecs=\"vp8.0, opus\"'"));

  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9.0\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9, vorbis\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9.0, vorbis\"'"));
  EXPECT_EQ(VP9AndOpusProbably, CanPlay("'video/webm; codecs=\"vp9, opus\"'"));
  EXPECT_EQ(VP9AndOpusProbably,
            CanPlay("'video/webm; codecs=\"vp9.0, opus\"'"));

  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp8, vp9\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp8.0, vp9.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp8, theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp8, avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp9, avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp8, 1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp8.0, 1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp8, mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp8.0, mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp9, 1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp9.0, 1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp9, mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp9.0, mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"1\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"VP8\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"VP8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"VP9\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"Vp9.0\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"VP8, Vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp8, Vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"VP9, Opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"vp9, Opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/webm; codecs=\"unknown\"'"));

  EXPECT_EQ(kMaybe, CanPlay("'audio/webm'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/webm; codecs=\"vorbis\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/webm; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/webm; codecs=\"opus, vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"1, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"1, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vorbis, mp4a\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"unknown\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_ogg) {
  EXPECT_EQ(kOggVideoMaybe, CanPlay("'video/ogg'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"opus, vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc3, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc3, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1, vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc3, vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1, avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"mp4a.4.02\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1.4D401E, mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc3.64001F, mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc1.4D401E, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"avc3.64001F, vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"Theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"Opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"Vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"Theora, Opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"Theora, Vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'video/ogg; codecs=\"unknown\"'"));

  EXPECT_EQ(kMaybe, CanPlay("'audio/ogg'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/ogg; codecs=\"vorbis, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"opus, 1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"vorbis, 1\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"Theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"Opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"Vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"Theora, Vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"Theora, Opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"unknown\"'"));

  EXPECT_EQ(kMaybe, CanPlay("'application/ogg'"));
  EXPECT_EQ(kProbably, CanPlay("'application/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kTheoraProbably, CanPlay("'application/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'application/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kOpusProbably,
            CanPlay("'application/ogg; codecs=\"opus, vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc1, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc3, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc3, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc1, vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc3, vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc1, avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot,
            CanPlay("'application/ogg; codecs=\"avc1.4D401E, mp4a.40.2\"'"));
  EXPECT_EQ(kNot,
            CanPlay("'application/ogg; codecs=\"avc3.64001F, mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc1.4D401E, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"avc3.64001F, vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"Theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"Vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"Opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"Theora, Vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"Theora, Opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'application/ogg; codecs=\"unknown\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_mp3) {
  EXPECT_EQ(kNot, CanPlay("'video/mp3'"));
  EXPECT_EQ(kNot, CanPlay("'video/mpeg'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-mp3'"));

  // audio/mpeg without a codecs parameter (RFC 3003 compliant)
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mpeg'"));

  // audio/mpeg with mp3 in codecs parameter. (Not RFC compliant, but
  // very common in the wild so it is a defacto standard).
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mpeg; codecs=\"mp3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1.\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3.\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"AVC1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"AVC3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"MP4A\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc4\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1x\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3x\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4ax\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"unknown\"'"));

  // audio/mp3 does not allow any codecs parameter
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1.\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3.\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"AVC1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"AVC3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"MP4A\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc4\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1x\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3x\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4ax\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"unknown\"'"));

  // audio/x-mp3 does not allow any codecs parameter
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"AVC1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"AVC3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"MP4A\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc4\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1x\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3x\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4ax\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"unknown\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_mp4) {
  // TODO(amogh.bihani): Change this expectation when bug 53193 is fixed.
  std::string PropAndVP9Probably = "";
#if defined (OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 19)
    PropAndVP9Probably = "maybe";
  else
    PropAndVP9Probably = "probably";
#else
#if defined(USE_PROPRIETARY_CODECS)
  PropAndVP9Probably = "probably";
#endif  // USE_PROPRIETARY_CODECS
#endif  // OS_ANDROID
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1, avc3\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.4D401E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.64001F, mp4a.40.5\"'"));

  // TODO(amogh.bihani): Change these tests when bug 53193 is fixed.
  // http://crbug.com/53193 ----------------------------------------------------
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"vp8\"'"));
  EXPECT_EQ(PropAndVP9Probably, CanPlay("'video/mp4; codecs=\"vp9\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"vorbis\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1, vorbis\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3, vorbis\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.4D401E, vorbis\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.64001F, vorbis\"'"));

  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'video/mp4; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'video/mp4; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'video/mp4; codecs=\"vp9, opus\"'"));

  EXPECT_EQ(kTheoraAndPropProbably, CanPlay("'video/mp4; codecs=\"theora\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/mp4; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/mp4; codecs=\"theora, mp4a\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/mp4; codecs=\"theora, mp4a.40.2\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/mp4; codecs=\"theora, avc1\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/mp4; codecs=\"theora, avc3\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/mp4; codecs=\"theora, avc1.4D401E\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/mp4; codecs=\"theora, avc3.64001F\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"AVC1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"AVC3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"MP4A\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/mp4; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/mp4; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc4\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"mp4ax\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"unknown\"'"));
  // ---------------------------------------------------------------------------

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1, avc3\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc1.4D401E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.64001F, mp4a.40.5\"'"));

  // TODO(amogh.bihani): Change these tests when bug 53193 is fixed.
  // http://crbug.com/53193 ----------------------------------------------------
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"vp8\"'"));
  EXPECT_EQ(PropAndVP9Probably, CanPlay("'video/x-m4v; codecs=\"vp9\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"vorbis\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1, vorbis\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3, vorbis\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc1.4D401E, vorbis\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.64001F, vorbis\"'"));

  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'video/x-m4v; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"vp9, opus\"'"));

  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora, mp4a\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora, mp4a.40.2\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora, avc1\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora, avc3\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora, avc1.4D401E\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'video/x-m4v; codecs=\"theora, avc3.64001F\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"AVC1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"AVC3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"MP4A\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc4\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"mp4ax\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"unknown\"'"));
  // ---------------------------------------------------------------------------

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.2\"'"));

  // TODO(amogh.bihani): Change these tests when bug 53193 is fixed.
  // http://crbug.com/53193 ----------------------------------------------------
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc3, mp4a\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc1.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc3.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/mp4; codecs=\"avc1.4D401E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/mp4; codecs=\"avc3.64001F mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/mp4; codecs=\"mp4a, vorbis\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/mp4; codecs=\"mp4a.40.2, vorbis\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"vorbis\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"vp8\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"vp8.0\"'"));
  EXPECT_EQ(PropAndVP9Probably, CanPlay("'audio/mp4; codecs=\"vp9\"'"));
  EXPECT_EQ(PropAndVP9Probably, CanPlay("'audio/mp4; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'audio/mp4; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'audio/mp4; codecs=\"mp4a, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'audio/mp4; codecs=\"vorbis, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'audio/mp4; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'audio/mp4; codecs=\"vp9, opus\"'"));

  EXPECT_EQ(kTheoraAndPropProbably, CanPlay("'audio/mp4; codecs=\"theora\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'audio/mp4; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'audio/mp4; codecs=\"theora, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"avc1, vorbis\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"AVC1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"AVC3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"MP4A\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'audio/mp4; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'audio/mp4; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"avc2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"avc4\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"avc1x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"avc3x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"mp4ax\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"unknown\"'"));
  // ---------------------------------------------------------------------------

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.2\"'"));

  // TODO(amogh.bihani): Change these tests when bug 53193 is fixed.
  // http://crbug.com/53193 ----------------------------------------------------
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc3, mp4a\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc1.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc3.\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"avc1.4D401E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"avc3.64001F mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"mp4a, vorbis\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"mp4a.40.2, vorbis\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"vorbis\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"vp8\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"vp8.0\"'"));
  EXPECT_EQ(PropAndVP9Probably, CanPlay("'audio/x-m4a; codecs=\"vp9\"'"));
  EXPECT_EQ(PropAndVP9Probably, CanPlay("'audio/x-m4a; codecs=\"vp9.0\"'"));

  EXPECT_EQ(kOpusAndPropProbably, CanPlay("'audio/x-m4a; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"mp4a, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"vorbis, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kOpusAndPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"vp9, opus\"'"));

  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"theora\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kTheoraAndPropProbably,
            CanPlay("'audio/x-m4a; codecs=\"theora, mp4a\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"avc1, vorbis\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"AVC1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"AVC3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"MP4A\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'audio/x-m4a; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'audio/x-m4a; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"avc2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"avc4\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"avc1x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"avc3x\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"mp4ax\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"unknown\"'"));
  // ---------------------------------------------------------------------------
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_HLS) {
  // HLS are supported only on Android IceCreamSandwich and above (API level 14)
  std::string HLSProbably = "";
  std::string HLSMaybe = "";
  std::string HLSAndVP9Probably = "";
#if defined(OS_ANDROID)
  int sdk = base::android::BuildInfo::GetInstance()->sdk_int();
  if (sdk > 13) {
    HLSProbably = "probably";
    HLSMaybe = "maybe";
    if (sdk < 19)
      HLSAndVP9Probably = "maybe";
    else
      HLSAndVP9Probably = "probably";
  }
#endif
  EXPECT_EQ(HLSMaybe, CanPlay("'application/x-mpegurl'"));

  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc1\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc3\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"mp4a\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc3, mp4a\"'"));

  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(HLSProbably,
        CanPlay("'application/x-mpegurl; codecs=\"avc1.4D401E, mp4a.40.2\"'"));
  EXPECT_EQ(HLSProbably,
        CanPlay("'application/x-mpegurl; codecs=\"avc3.64001F, mp4a.40.5\"'"));

  // TODO(amogh.bihani): Change these tests when bug 53193 is fixed.
  // http://crbug.com/53193 ----------------------------------------------------
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(HLSProbably, CanPlay("'application/x-mpegurl; codecs=\"avc1.\"'"));
  EXPECT_EQ(HLSProbably, CanPlay("'application/x-mpegurl; codecs=\"avc3.\"'"));
  EXPECT_EQ(HLSProbably, CanPlay("'application/x-mpegurl; codecs=\"mp4a.\"'"));

  EXPECT_EQ(HLSProbably, CanPlay("'application/x-mpegurl; codecs=\"vp8\"'"));
  EXPECT_EQ(HLSAndVP9Probably,
            CanPlay("'application/x-mpegurl; codecs=\"vp9\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"vorbis\"'"));

  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc1, vorbis\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc3, vorbis\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.4D401E, vorbis\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/x-mpegurl; codecs=\"avc3.64001F, vorbis\"'"));

  EXPECT_EQ(HLSMaybe, CanPlay("'application/x-mpegurl; codecs=\"opus\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"vp9, opus\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"theora\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"theora, mp4a\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"theora, mp4a.40.2\"'"));

  EXPECT_EQ(HLSMaybe, CanPlay("'application/x-mpegurl; codecs=\"AVC1\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(HLSMaybe, CanPlay("'application/x-mpegurl; codecs=\"AVC3\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(HLSMaybe, CanPlay("'application/x-mpegurl; codecs=\"MP4A\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(HLSMaybe,
        CanPlay("'application/x-mpegurl; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(HLSMaybe,
        CanPlay("'application/x-mpegurl; codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"avc2\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"avc4\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"avc1x\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"avc3x\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"mp4ax\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/x-mpegurl; codecs=\"unknown\"'"));
  // ---------------------------------------------------------------------------

  EXPECT_EQ(HLSMaybe, CanPlay("'application/vnd.apple.mpegurl'"));

  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3, mp4a\"'"));

  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3.64001F\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40.2\"'"));

  // TODO(amogh.bihani): Change these tests when bug 53193 is fixed.
  // http://crbug.com/53193 ----------------------------------------------------
  EXPECT_EQ(HLSProbably,
          CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.unknown\"'"));
  EXPECT_EQ(HLSProbably,
          CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3.unknown\"'"));
  EXPECT_EQ(HLSProbably,
          CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.unknown\"'"));

  EXPECT_EQ(HLSProbably,
          CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.\"'"));
  EXPECT_EQ(HLSProbably,
          CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3.\"'"));
  EXPECT_EQ(HLSProbably,
          CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.\"'"));

  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"vp8\"'"));
  EXPECT_EQ(HLSAndVP9Probably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"vp9\"'"));
  EXPECT_EQ(HLSProbably,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"vorbis\"'"));

  EXPECT_EQ(HLSProbably,
           CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1, vorbis\"'"));
  EXPECT_EQ(HLSProbably,
           CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3, vorbis\"'"));
  EXPECT_EQ(HLSProbably,
    CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.4D401E, vorbis\"'"));
  EXPECT_EQ(HLSProbably,
    CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3.64001F, vorbis\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"opus\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"vp9, opus\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"theora\"'"));
  EXPECT_EQ(HLSMaybe,
        CanPlay("'application/vnd.apple.mpegurl; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(HLSMaybe,
          CanPlay("'application/vnd.apple.mpegurl; codecs=\"theora, mp4a\"'"));
  EXPECT_EQ(HLSMaybe,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"theora, mp4a.40.2\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"AVC1\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"AVC1.4d401e\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"AVC3\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"AVC3.64001f\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"MP4A\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"MP4A.40.2\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"AVC1, MP4\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"AVC3, MP4\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; "
                    "codecs=\", AVC1.4D401E, MP4.40.2\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; "
                    "codecs=\", AVC3.64001F, MP4.40.2\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc2\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc4\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1x\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3x\"'"));
  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4ax\"'"));

  EXPECT_EQ(HLSMaybe,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"unknown\"'"));
  // ---------------------------------------------------------------------------
}

}  // namespace content
