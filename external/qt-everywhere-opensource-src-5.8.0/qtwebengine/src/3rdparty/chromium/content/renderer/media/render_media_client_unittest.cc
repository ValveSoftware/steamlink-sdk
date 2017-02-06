// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/media/render_media_client.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace content {

namespace {

class TestKeySystemProperties : public media::KeySystemProperties {
 public:
  TestKeySystemProperties(const std::string& key_system_name)
      : key_system_name_(key_system_name) {}

  std::string GetKeySystemName() const override { return key_system_name_; }
  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override {
    return false;
  }
  media::SupportedCodecs GetSupportedCodecs() const override {
    return media::EME_CODEC_NONE;
  }
  media::EmeConfigRule GetRobustnessConfigRule(
      media::EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    return requested_robustness.empty() ? media::EmeConfigRule::SUPPORTED
                                        : media::EmeConfigRule::NOT_SUPPORTED;
  }
  media::EmeSessionTypeSupport GetPersistentLicenseSessionSupport()
      const override {
    return media::EmeSessionTypeSupport::NOT_SUPPORTED;
  }
  media::EmeSessionTypeSupport GetPersistentReleaseMessageSessionSupport()
      const override {
    return media::EmeSessionTypeSupport::NOT_SUPPORTED;
  }
  media::EmeFeatureSupport GetPersistentStateSupport() const override {
    return media::EmeFeatureSupport::NOT_SUPPORTED;
  }
  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return media::EmeFeatureSupport::NOT_SUPPORTED;
  }

 private:
  const std::string key_system_name_;
};

class TestContentRendererClient : public ContentRendererClient {
 public:
  TestContentRendererClient() : is_extra_key_system_enabled_(false) {}

  // ContentRendererClient implementation.
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>*
          key_systems_properties) override {
    key_systems_properties->emplace_back(
        new TestKeySystemProperties("test.keysystem"));

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
    if (is_extra_key_system_enabled_) {
      key_systems_properties->emplace_back(
          new TestKeySystemProperties(kWidevineKeySystem));
    }
#endif
  }

  void EnableExtraKeySystem() { is_extra_key_system_enabled_ = true; }

 private:
  // Whether a platform-specific extra key system is "supported" by |this|.
  bool is_extra_key_system_enabled_;
};

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
bool ContainsWidevine(
    const std::vector<std::unique_ptr<media::KeySystemProperties>>&
        key_systems_properties) {
  for (const auto& key_system_properties : key_systems_properties) {
    if (key_system_properties->GetKeySystemName() == kWidevineKeySystem)
      return true;
  }
  return false;
}
#endif

}  // namespace

class RenderMediaClientTest : public testing::Test {
 protected:
  RenderMediaClientTest()
      : render_media_client_(GetRenderMediaClientInstanceForTesting()) {
    SetContentClient(&test_content_client_);
    SetRendererClientForTesting(&test_content_renderer_client_);
  }

  void EnableExtraKeySystem() {
    test_content_renderer_client_.EnableExtraKeySystem();
  }

  RenderMediaClient* render_media_client_;

 private:
  typedef base::hash_map<std::string, std::string> KeySystemNameForUMAMap;

  TestContentClient test_content_client_;
  TestContentRendererClient test_content_renderer_client_;
  KeySystemNameForUMAMap key_system_name_for_uma_map_;
};

TEST_F(RenderMediaClientTest, KeySystemNameForUMA) {
  std::vector<media::KeySystemInfoForUMA> key_systems_info_for_uma;
  render_media_client_->AddKeySystemsInfoForUMA(&key_systems_info_for_uma);

  std::string widevine_uma_name;
  std::string clearkey_uma_name;
  for (const media::KeySystemInfoForUMA& info : key_systems_info_for_uma) {
    if (info.key_system == "com.widevine.alpha")
      widevine_uma_name = info.key_system_name_for_uma;
    if (info.key_system == "org.w3.clearkey")
      clearkey_uma_name = info.key_system_name_for_uma;
  }

#if defined(WIDEVINE_CDM_AVAILABLE)
  EXPECT_EQ("Widevine", widevine_uma_name);
#else
  EXPECT_TRUE(widevine_uma_name.empty());
#endif

  EXPECT_TRUE(clearkey_uma_name.empty()) << "Clear Key is added by media/ and "
                                            "should not be added by the "
                                            "MediaClient.";
}

TEST_F(RenderMediaClientTest, IsKeySystemsUpdateNeeded) {
  base::SimpleTestTickClock* tick_clock = new base::SimpleTestTickClock();
  render_media_client_->SetTickClockForTesting(
      std::unique_ptr<base::TickClock>(tick_clock));

  // IsKeySystemsUpdateNeeded() always returns true after construction.
  EXPECT_TRUE(render_media_client_->IsKeySystemsUpdateNeeded());

  std::vector<std::unique_ptr<media::KeySystemProperties>>
      key_systems_properties;
  render_media_client_->AddSupportedKeySystems(&key_systems_properties);

  // No update needed immediately after AddSupportedKeySystems() call.
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  // Widevine not supported because extra key system isn't enabled.
  EXPECT_FALSE(ContainsWidevine(key_systems_properties));

  // This is timing related. The update interval for Widevine is 1000 ms.
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(990));
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(render_media_client_->IsKeySystemsUpdateNeeded());

  EnableExtraKeySystem();

  key_systems_properties.clear();
  render_media_client_->AddSupportedKeySystems(&key_systems_properties);
  EXPECT_TRUE(ContainsWidevine(key_systems_properties));

  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
  tick_clock->Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(render_media_client_->IsKeySystemsUpdateNeeded());
#endif
}

}  // namespace content
