// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "media/base/media.h"
#endif

// Available key systems.
const char kClearKeyKeySystem[] = "org.w3.clearkey";

// Supported media types.
const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";

// EME-specific test results and errors.
const char kEmeKeyError[] = "KEYERROR";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";

const char kDefaultEmePlayer[] = "eme_player.html";

// The type of video src used to load media.
enum SrcType {
  SRC,
  MSE
};

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

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - bool: True to load media using MSE, otherwise use src.
class EncryptedMediaTest : public content::MediaBrowserTest,
    public testing::WithParamInterface<std::tr1::tuple<const char*, SrcType> > {
 public:
  // Can only be used in parameterized (*_P) tests.
  const std::string CurrentKeySystem() {
    return std::tr1::get<0>(GetParam());
  }

  // Can only be used in parameterized (*_P) tests.
  SrcType CurrentSourceType() {
    return std::tr1::get<1>(GetParam());
  }

  void TestSimplePlayback(const std::string& encrypted_media,
                          const std::string& media_type) {
    RunSimpleEncryptedMediaTest(
        encrypted_media, media_type, CurrentKeySystem(), CurrentSourceType());
  }

  void TestFrameSizeChange() {
    RunEncryptedMediaTest("encrypted_frame_size_change.html",
                          "frame_size_change-av_enc-v.webm", kWebMAudioVideo,
                          CurrentKeySystem(), CurrentSourceType(), kEnded);
  }

  void TestConfigChange() {
    if (CurrentSourceType() != MSE || !IsMSESupported()) {
      VLOG(0) << "Skipping test - config change test requires MSE.";
      return;
    }

    base::StringPairs query_params;
    query_params.push_back(std::make_pair("keySystem", CurrentKeySystem()));
    query_params.push_back(std::make_pair("runEncrypted", "1"));
    RunMediaTestPage("mse_config_change.html", query_params, kEnded, true);
  }

  void RunEncryptedMediaTest(const std::string& html_page,
                             const std::string& media_file,
                             const std::string& media_type,
                             const std::string& key_system,
                             SrcType src_type,
                             const std::string& expectation) {
    if (src_type == MSE && !IsMSESupported()) {
      VLOG(0) << "Skipping test - MSE not supported.";
      return;
    }

    base::StringPairs query_params;
    query_params.push_back(std::make_pair("mediaFile", media_file));
    query_params.push_back(std::make_pair("mediaType", media_type));
    query_params.push_back(std::make_pair("keySystem", key_system));
    if (src_type == MSE)
      query_params.push_back(std::make_pair("useMSE", "1"));
    RunMediaTestPage(html_page, query_params, expectation, true);
  }

  void RunSimpleEncryptedMediaTest(const std::string& media_file,
                                   const std::string& media_type,
                                   const std::string& key_system,
                                   SrcType src_type) {
    RunEncryptedMediaTest(kDefaultEmePlayer,
                          media_file,
                          media_type,
                          key_system,
                          src_type,
                          kEnded);
  }

 protected:
  // We want to fail quickly when a test fails because an error is encountered.
  void AddWaitForTitles(content::TitleWatcher* title_watcher) override {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeKeyError));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
  }
};

using ::testing::Combine;
using ::testing::Values;

INSTANTIATE_TEST_CASE_P(SRC_ClearKey, EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem), Values(SRC)));

INSTANTIATE_TEST_CASE_P(MSE_ClearKey, EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem), Values(MSE)));

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a_enc-a.webm", kWebMAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v_enc-v.webm", kWebMVideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-a_enc-a.webm", kWebMAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-av.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-v.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo) {
  TestConfigChange();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  TestFrameSizeChange();
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, UnknownKeySystemThrowsException) {
  RunEncryptedMediaTest(kDefaultEmePlayer,
                        "bear-a_enc-a.webm",
                        kWebMAudioOnly,
                        "com.example.foo",
                        MSE,
                        kEmeNotSupportedError);
}

}  // namespace content
