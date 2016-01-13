// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/key_system_info.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

// Death tests are not always available, including on Android.
// EXPECT_DEBUG_DEATH_PORTABLE executes tests correctly except in the case that
// death tests are not available and NDEBUG is not defined.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
#define EXPECT_DEBUG_DEATH_PORTABLE(statement, regex) \
  EXPECT_DEBUG_DEATH(statement, regex)
#else
#if defined(NDEBUG)
#define EXPECT_DEBUG_DEATH_PORTABLE(statement, regex) \
  do { statement; } while (false)
#else
#include "base/logging.h"
#define EXPECT_DEBUG_DEATH_PORTABLE(statement, regex) \
  LOG(WARNING) << "Death tests are not supported on this platform.\n" \
               << "Statement '" #statement "' cannot be verified.";
#endif  // defined(NDEBUG)
#endif  // defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

namespace content {

using blink::WebString;

// These are the (fake) key systems that are registered for these tests.
// kUsesAes uses the AesDecryptor like Clear Key.
// kExternal uses an external CDM, such as Pepper-based or Android platform CDM.
const char kUsesAes[] = "org.example.clear";
const char kUsesAesParent[] = "org.example";  // Not registered.
const char kExternal[] = "com.example.test";
const char kExternalParent[] = "com.example";

const char kClearKey[] = "org.w3.clearkey";
const char kPrefixedClearKey[] = "webkit-org.w3.clearkey";
const char kExternalClearKey[] = "org.chromium.externalclearkey";

const char kAudioWebM[] = "audio/webm";
const char kVideoWebM[] = "video/webm";
const char kAudioFoo[] = "audio/foo";
const char kVideoFoo[] = "video/foo";

// Pick some arbitrary bit fields as long as they are not in conflict with the
// real ones.
enum TestCodec {
  TEST_CODEC_FOO_AUDIO = 1 << 10,  // An audio codec for foo container.
  TEST_CODEC_FOO_AUDIO_ALL = TEST_CODEC_FOO_AUDIO,
  TEST_CODEC_FOO_VIDEO = 1 << 11,  // A video codec for foo container.
  TEST_CODEC_FOO_VIDEO_ALL = TEST_CODEC_FOO_VIDEO,
  TEST_CODEC_FOO_ALL = TEST_CODEC_FOO_AUDIO_ALL | TEST_CODEC_FOO_VIDEO_ALL
};

COMPILE_ASSERT((TEST_CODEC_FOO_ALL & EME_CODEC_ALL) == EME_CODEC_NONE,
                test_codec_masks_should_only_use_invalid_codec_masks);

// Adds test container and codec masks.
// This function must be called after SetContentClient() is called.
// More details: AddXxxMask() will create KeySystems if it hasn't been created.
// During KeySystems's construction GetContentClient() will be used to add key
// systems. In test code, the content client is set by SetContentClient().
// Therefore, SetContentClient() must be called before this function to avoid
// access violation.
static void AddContainerAndCodecMasksForTest() {
  // Since KeySystems is a singleton. Make sure we only add test container and
  // codec masks once per process.
  static bool is_test_masks_added = false;

  if (is_test_masks_added)
    return;

  AddContainerMask("audio/foo", TEST_CODEC_FOO_AUDIO_ALL);
  AddContainerMask("video/foo", TEST_CODEC_FOO_ALL);
  AddCodecMask("fooaudio", TEST_CODEC_FOO_AUDIO);
  AddCodecMask("foovideo", TEST_CODEC_FOO_VIDEO);

  is_test_masks_added = true;
}

class TestContentRendererClient : public ContentRendererClient {
  virtual void AddKeySystems(
      std::vector<content::KeySystemInfo>* key_systems) OVERRIDE;
};

void TestContentRendererClient::AddKeySystems(
    std::vector<content::KeySystemInfo>* key_systems) {
  KeySystemInfo aes(kUsesAes);
  aes.supported_codecs = EME_CODEC_WEBM_ALL;
  aes.supported_codecs |= TEST_CODEC_FOO_ALL;
  aes.use_aes_decryptor = true;
  key_systems->push_back(aes);

  KeySystemInfo ext(kExternal);
  ext.supported_codecs = EME_CODEC_WEBM_ALL;
  ext.supported_codecs |= TEST_CODEC_FOO_ALL;
  ext.parent_key_system = kExternalParent;
#if defined(ENABLE_PEPPER_CDMS)
  ext.pepper_type = "application/x-ppapi-external-cdm";
#endif  // defined(ENABLE_PEPPER_CDMS)
  key_systems->push_back(ext);
}

class KeySystemsTest : public testing::Test {
 protected:
  KeySystemsTest() {
    vp8_codec_.push_back("vp8");

    vp80_codec_.push_back("vp8.0");

    vp9_codec_.push_back("vp9");

    vp90_codec_.push_back("vp9.0");

    vorbis_codec_.push_back("vorbis");

    vp8_and_vorbis_codecs_.push_back("vp8");
    vp8_and_vorbis_codecs_.push_back("vorbis");

    vp9_and_vorbis_codecs_.push_back("vp9");
    vp9_and_vorbis_codecs_.push_back("vorbis");

    foovideo_codec_.push_back("foovideo");

    foovideo_extended_codec_.push_back("foovideo.4D400C");

    foovideo_dot_codec_.push_back("foovideo.");

    fooaudio_codec_.push_back("fooaudio");

    foovideo_and_fooaudio_codecs_.push_back("foovideo");
    foovideo_and_fooaudio_codecs_.push_back("fooaudio");

    unknown_codec_.push_back("unknown");

    mixed_codecs_.push_back("vorbis");
    mixed_codecs_.push_back("foovideo");

    // KeySystems requires a valid ContentRendererClient and thus ContentClient.
    // The TestContentClient is not available inside Death Tests on some
    // platforms (see below). Therefore, always provide a TestContentClient.
    // Explanation: When Death Tests fork, there is a valid ContentClient.
    // However, when they launch a new process instead of forking, the global
    // variable is not copied and for some reason TestContentClientInitializer
    // does not get created to set the global variable in the new process.
    SetContentClient(&test_content_client_);
    SetRendererClientForTesting(&content_renderer_client_);
  }

  virtual void SetUp() OVERRIDE {
    AddContainerAndCodecMasksForTest();
  }

  virtual ~KeySystemsTest() {
    // Clear the use of content_client_, which was set in SetUp().
    SetContentClient(NULL);
  }

  typedef std::vector<std::string> CodecVector;

  const CodecVector& no_codecs() const { return no_codecs_; }

  const CodecVector& vp8_codec() const { return vp8_codec_; }
  const CodecVector& vp80_codec() const { return vp80_codec_; }
  const CodecVector& vp9_codec() const { return vp9_codec_; }
  const CodecVector& vp90_codec() const { return vp90_codec_; }

  const CodecVector& vorbis_codec() const { return vorbis_codec_; }

  const CodecVector& vp8_and_vorbis_codecs() const {
    return vp8_and_vorbis_codecs_;
  }
  const CodecVector& vp9_and_vorbis_codecs() const {
    return vp9_and_vorbis_codecs_;
  }

  const CodecVector& foovideo_codec() const { return foovideo_codec_; }
  const CodecVector& foovideo_extended_codec() const {
    return foovideo_extended_codec_;
  }
  const CodecVector& foovideo_dot_codec() const { return foovideo_dot_codec_; }
  const CodecVector& fooaudio_codec() const { return fooaudio_codec_; }
  const CodecVector& foovideo_and_fooaudio_codecs() const {
    return foovideo_and_fooaudio_codecs_;
  }

  const CodecVector& unknown_codec() const { return unknown_codec_; }

  const CodecVector& mixed_codecs() const { return mixed_codecs_; }

 private:
  const CodecVector no_codecs_;
  CodecVector vp8_codec_;
  CodecVector vp80_codec_;
  CodecVector vp9_codec_;
  CodecVector vp90_codec_;
  CodecVector vorbis_codec_;
  CodecVector vp8_and_vorbis_codecs_;
  CodecVector vp9_and_vorbis_codecs_;

  CodecVector foovideo_codec_;
  CodecVector foovideo_extended_codec_;
  CodecVector foovideo_dot_codec_;
  CodecVector fooaudio_codec_;
  CodecVector foovideo_and_fooaudio_codecs_;

  CodecVector unknown_codec_;

  CodecVector mixed_codecs_;

  TestContentClient test_content_client_;
  TestContentRendererClient content_renderer_client_;
};

// TODO(ddorwin): Consider moving GetPepperType() calls out to their own test.

TEST_F(KeySystemsTest, EmptyKeySystem) {
  EXPECT_FALSE(IsConcreteSupportedKeySystem(std::string()));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), std::string()));
  EXPECT_EQ("Unknown", KeySystemNameForUMA(std::string()));
}

// Clear Key is the only key system registered in content.
TEST_F(KeySystemsTest, ClearKey) {
  EXPECT_TRUE(IsConcreteSupportedKeySystem(kClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kClearKey));

  EXPECT_EQ("ClearKey", KeySystemNameForUMA(kClearKey));

  // Prefixed Clear Key is not supported internally.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kPrefixedClearKey));
  EXPECT_EQ("Unknown", KeySystemNameForUMA(kPrefixedClearKey));
}

// The key system is not registered and therefore is unrecognized.
TEST_F(KeySystemsTest, Basic_UnrecognizedKeySystem) {
  static const char* const kUnrecognized = "org.example.unrecognized";

  EXPECT_FALSE(IsConcreteSupportedKeySystem(kUnrecognized));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUnrecognized));

  EXPECT_EQ("Unknown", KeySystemNameForUMA(kUnrecognized));

  bool can_use = false;
  EXPECT_DEBUG_DEATH_PORTABLE(
      can_use = CanUseAesDecryptor(kUnrecognized),
      "org.example.unrecognized is not a known concrete system");
  EXPECT_FALSE(can_use);

#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(type = GetPepperType(kUnrecognized),
                     "org.example.unrecognized is not a known concrete system");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest, Basic_UsesAesDecryptor) {
  EXPECT_TRUE(IsConcreteSupportedKeySystem(kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUsesAes));

  // No UMA value for this test key system.
  EXPECT_EQ("Unknown", KeySystemNameForUMA(kUsesAes));

  EXPECT_TRUE(CanUseAesDecryptor(kUsesAes));
#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(type = GetPepperType(kUsesAes),
                     "org.example.clear is not Pepper-based");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer1) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp80_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp90_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vorbis_codec(), kUsesAes));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, mixed_codecs(), kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vorbis_codec(), kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kUsesAes));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, fooaudio_codec(), kUsesAes));
}

// No parent is registered for UsesAes.
TEST_F(KeySystemsTest, Parent_NoParentRegistered) {
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kUsesAesParent));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kUsesAesParent));

  // The parent is not supported for most things.
  EXPECT_EQ("Unknown", KeySystemNameForUMA(kUsesAesParent));
  bool result = false;
  EXPECT_DEBUG_DEATH_PORTABLE(result = CanUseAesDecryptor(kUsesAesParent),
                              "org.example is not a known concrete system");
  EXPECT_FALSE(result);
#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(type = GetPepperType(kUsesAesParent),
                     "org.example is not a known concrete system");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(KeySystemsTest, IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.example.ClEaR"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), "org.example.ClEaR"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), "org."));
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), "com"));

  // Extra period.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.example."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), "org.example."));

  // Incomplete.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.example.clea"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), "org.example.clea"));

  // Extra character.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.example.clearz"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), "org.example.clearz"));

  // There are no child key systems for UsesAes.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.example.clear.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), "org.example.clear.foo"));
}

TEST_F(KeySystemsTest, IsSupportedKeySystemWithMediaMimeType_NoType) {
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kUsesAesParent));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "org.example.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "org.example.clear.foo"));
}

// Tests the second registered container type.
// TODO(ddorwin): Combined with TypesContainer1 in a future CL.
TEST_F(KeySystemsTest,
       IsSupportedKeySystemWithMediaMimeType_UsesAesDecryptor_TypesContainer2) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, no_codecs(), kUsesAes));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, no_codecs(), kUsesAesParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_codec(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kUsesAes));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kUsesAes));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, vp8_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, unknown_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, mixed_codecs(), kUsesAes));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, no_codecs(), kUsesAes));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, fooaudio_codec(), kUsesAes));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, foovideo_codec(), kUsesAes));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kUsesAes));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, vorbis_codec(), kUsesAes));
}

//
// Non-AesDecryptor-based key system.
//

TEST_F(KeySystemsTest, Basic_ExternalDecryptor) {
  EXPECT_TRUE(IsConcreteSupportedKeySystem(kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternal));

  EXPECT_FALSE(CanUseAesDecryptor(kExternal));
#if defined(ENABLE_PEPPER_CDMS)
  EXPECT_EQ("application/x-ppapi-external-cdm", GetPepperType(kExternal));
#endif  // defined(ENABLE_PEPPER_CDMS)

}

TEST_F(KeySystemsTest, Parent_ParentRegistered) {
  // The parent system is not a concrete system but is supported.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternalParent));

  // The parent is not supported for most things.
  EXPECT_EQ("Unknown", KeySystemNameForUMA(kExternalParent));
  bool result = false;
  EXPECT_DEBUG_DEATH_PORTABLE(result = CanUseAesDecryptor(kExternalParent),
                              "com.example is not a known concrete system");
  EXPECT_FALSE(result);
#if defined(ENABLE_PEPPER_CDMS)
  std::string type;
  EXPECT_DEBUG_DEATH(type = GetPepperType(kExternalParent),
                     "com.example is not a known concrete system");
  EXPECT_TRUE(type.empty());
#endif
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer1) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp80_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp90_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vorbis_codec(), kExternal));

  // Valid video types - parent key system.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, no_codecs(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_codec(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp80_codec(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp8_and_vorbis_codecs(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_codec(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp90_codec(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vp9_and_vorbis_codecs(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, vorbis_codec(), kExternalParent));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, unknown_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoWebM, mixed_codecs(), kExternal));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vorbis_codec(), kExternal));

  // Valid audio types - parent key system.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, no_codecs(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vorbis_codec(), kExternalParent));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp8_and_vorbis_codecs(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, vp9_and_vorbis_codecs(), kExternal));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioWebM, fooaudio_codec(), kExternal));
}

TEST_F(
    KeySystemsTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalDecryptor_TypesContainer2) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_codec(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kExternal));

  // Valid video types - parent key system.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, no_codecs(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_codec(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_and_fooaudio_codecs(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, fooaudio_codec(), kExternalParent));

  // Extended codecs fail because this is handled by SimpleWebMimeRegistryImpl.
  // They should really pass canPlayType().
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_extended_codec(), kExternal));

  // Invalid codec format.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, foovideo_dot_codec(), kExternal));

  // Non-container2 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, vp8_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, unknown_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kVideoFoo, mixed_codecs(), kExternal));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, no_codecs(), kExternal));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, fooaudio_codec(), kExternal));

  // Valid audio types - parent key system.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, no_codecs(), kExternalParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, fooaudio_codec(), kExternalParent));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, foovideo_codec(), kExternal));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, foovideo_and_fooaudio_codecs(), kExternal));

  // Non-container2 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      kAudioFoo, vorbis_codec(), kExternal));
}

TEST_F(KeySystemsTest, KeySystemNameForUMA) {
  EXPECT_EQ("ClearKey", KeySystemNameForUMA(kClearKey));
  // Prefixed is not supported internally.
  EXPECT_EQ("Unknown", KeySystemNameForUMA(kPrefixedClearKey));

  // External Clear Key never has a UMA name.
  EXPECT_EQ("Unknown", KeySystemNameForUMA(kExternalClearKey));

#if defined(WIDEVINE_CDM_AVAILABLE)
  const char* const kTestWidevineUmaName = "Widevine";
#else
  const char* const kTestWidevineUmaName = "Unknown";
#endif
  EXPECT_EQ(kTestWidevineUmaName, KeySystemNameForUMA("com.widevine.alpha"));
}

}  // namespace content
