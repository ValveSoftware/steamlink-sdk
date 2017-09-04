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
#include "media/base/media.h"
#include "media/base/media_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(ENABLE_MOJO_CDM)
// When mojo CDM is enabled, External Clear Key is supported in //content/shell/
// by using mojo CDM with AesDecryptor running in the remote (e.g. GPU or
// Browser) process.
// Note that External Clear Key is also supported in chrome/ when pepper CDM is
// used, which is tested in browser_tests.
#define SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL
#endif

// Available key systems.
const char kClearKeyKeySystem[] = "org.w3.clearkey";

#if defined(SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL)
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
#endif

// Supported media types.
const char kWebMVorbisAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMOpusAudioOnly[] = "audio/webm; codecs=\"opus\"";
const char kWebMVP8VideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMVP9VideoOnly[] = "video/webm; codecs=\"vp9\"";
const char kWebMOpusAudioVP9Video[] = "video/webm; codecs=\"opus, vp9\"";
const char kWebMVorbisAudioVP8Video[] = "video/webm; codecs=\"vorbis, vp8\"";

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

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - SrcType: The type of video src used to load media, MSE or SRC.
// It is okay to run this test as a non-parameterized test, in this case,
// GetParam() should not be called.
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
                          "frame_size_change-av_enc-v.webm",
                          kWebMVorbisAudioVP8Video, CurrentKeySystem(),
                          CurrentSourceType(), kEnded);
  }

  void TestConfigChange() {
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
  void AddTitlesToAwait(content::TitleWatcher* title_watcher) override {
    MediaBrowserTest::AddTitlesToAwait(title_watcher);
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeKeyError));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
#if defined(SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL)
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    media::kExternalClearKeyForTesting.name);
#endif
  }
};

using ::testing::Combine;
using ::testing::Values;

INSTANTIATE_TEST_CASE_P(SRC_ClearKey, EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem), Values(SRC)));

INSTANTIATE_TEST_CASE_P(MSE_ClearKey, EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem), Values(MSE)));

#if defined(SUPPORTS_EXTERNAL_CLEAR_KEY_IN_CONTENT_SHELL)
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SRC)));

INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(MSE)));
#endif

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a_enc-a.webm", kWebMVorbisAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm", kWebMVorbisAudioVP8Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm", kWebMVorbisAudioVP8Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v_enc-v.webm", kWebMVP8VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_Fullsample) {
  TestSimplePlayback("bear-320x240-v-vp9_fullsample_enc-v.webm",
                     kWebMVP9VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_Subsample) {
  TestSimplePlayback("bear-320x240-v-vp9_subsample_enc-v.webm",
                     kWebMVP9VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm", kWebMVorbisAudioVP8Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-a_enc-a.webm", kWebMOpusAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-av.webm",
                     kWebMOpusAudioVP9Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus) {
#if defined(OS_ANDROID)
  if (!media::PlatformHasOpusSupport())
    return;
#endif
  TestSimplePlayback("bear-320x240-opus-av_enc-v.webm", kWebMOpusAudioVP9Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo) {
  TestConfigChange();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  TestFrameSizeChange();
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, UnknownKeySystemThrowsException) {
  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm",
                        kWebMVorbisAudioOnly, "com.example.foo", MSE,
                        kEmeNotSupportedError);
}

}  // namespace content
