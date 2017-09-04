// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* const kContentSettingNames[] = {
  "default",
  "allow",
  "block",
  "ask",
  "session_only",
  "detect_important_content",
};
static_assert(arraysize(kContentSettingNames) == CONTENT_SETTING_NUM_SETTINGS,
              "kContentSettingNames has an unexpected number of elements");

}  // namespace

TEST(ContentSettingsUtilsTest, ParsePatternString) {
  content_settings::PatternPair pattern_pair;

  pattern_pair = content_settings::ParsePatternString(std::string());
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(",");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString("http://www.foo.com");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_EQ(pattern_pair.second, ContentSettingsPattern::Wildcard());

  // This inconsistency is to recover from some broken code.
  pattern_pair = content_settings::ParsePatternString("http://www.foo.com,");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(
      "http://www.foo.com,http://www.bar.com");
  EXPECT_TRUE(pattern_pair.first.IsValid());
  EXPECT_TRUE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(
      "http://www.foo.com,http://www.bar.com,");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());

  pattern_pair = content_settings::ParsePatternString(
      "http://www.foo.com,http://www.bar.com,http://www.error.com");
  EXPECT_FALSE(pattern_pair.first.IsValid());
  EXPECT_FALSE(pattern_pair.second.IsValid());
}

TEST(ContentSettingsUtilsTest, ContentSettingsStringMap) {
  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_NUM_SETTINGS);
  EXPECT_TRUE(setting_string.empty());

  for (size_t i = 0; i < arraysize(kContentSettingNames); ++i) {
    ContentSetting setting = static_cast<ContentSetting>(i);
    setting_string = content_settings::ContentSettingToString(setting);
    EXPECT_EQ(kContentSettingNames[i], setting_string);

    ContentSetting converted_setting;
    if (i == 0) {
      EXPECT_FALSE(content_settings::ContentSettingFromString(
          kContentSettingNames[i], &converted_setting));
    } else {
      EXPECT_TRUE(content_settings::ContentSettingFromString(
          kContentSettingNames[i], &converted_setting));
    }
    EXPECT_EQ(setting, converted_setting);
  }
}
