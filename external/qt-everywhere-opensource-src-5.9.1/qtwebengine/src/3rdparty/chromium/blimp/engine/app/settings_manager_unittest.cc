// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/settings_manager.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {

namespace {

class MockSettingsManagerObserver : public SettingsManager::Observer {
 public:
  MOCK_METHOD0(OnWebPreferencesChanged, void());
};

class SettingsManagerTest : public testing::Test {
 public:
  SettingsManagerTest() {}

  void SetUp() override { settings_manager_.AddObserver(&observer_); }

  void TearDown() override { settings_manager_.RemoveObserver(&observer_); }

 protected:
  SettingsManager settings_manager_;
  MockSettingsManagerObserver observer_;
};

TEST_F(SettingsManagerTest, InformsObserversCorrectly) {
  EngineSettings settings = settings_manager_.GetEngineSettings();
  EXPECT_FALSE(settings.record_whole_document);

  EXPECT_CALL(observer_, OnWebPreferencesChanged()).Times(1);

  settings.record_whole_document = true;
  settings_manager_.UpdateEngineSettings(settings);
}

TEST_F(SettingsManagerTest, UpdatesSettingsCorrectly) {
  EngineSettings settings = settings_manager_.GetEngineSettings();
  EXPECT_FALSE(settings.record_whole_document);
  EXPECT_EQ(settings.animation_policy,
            content::ImageAnimationPolicy::IMAGE_ANIMATION_POLICY_NO_ANIMATION);

  settings.record_whole_document = true;
  settings.animation_policy =
      content::ImageAnimationPolicy::IMAGE_ANIMATION_POLICY_ANIMATION_ONCE;
  settings_manager_.UpdateEngineSettings(settings);

  content::WebPreferences prefs;
  settings_manager_.UpdateWebkitPreferences(&prefs);
  EXPECT_TRUE(prefs.record_whole_document);
  EXPECT_EQ(
      prefs.animation_policy,
      content::ImageAnimationPolicy::IMAGE_ANIMATION_POLICY_ANIMATION_ONCE);
  EXPECT_TRUE(prefs.viewport_meta_enabled);
  EXPECT_EQ(prefs.default_minimum_page_scale_factor, 0.25f);
  EXPECT_EQ(prefs.default_maximum_page_scale_factor, 5.f);
  EXPECT_TRUE(prefs.shrinks_viewport_contents_to_fit);
}

}  // namespace

}  // namespace engine
}  // namespace blimp
