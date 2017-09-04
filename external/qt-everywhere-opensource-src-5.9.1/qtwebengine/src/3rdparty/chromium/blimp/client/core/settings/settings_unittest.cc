// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/settings/settings.h"

#include "base/command_line.h"
#include "blimp/client/core/settings/settings_observer.h"
#include "blimp/client/core/settings/settings_prefs.h"
#include "blimp/client/core/switches/blimp_client_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {
namespace {

class MockSettingsObserver : public SettingsObserver {
 public:
  explicit MockSettingsObserver(Settings* settings) : settings_(settings) {
    settings_->AddObserver(this);
  }
  ~MockSettingsObserver() {
    if (settings_) {
      settings_->RemoveObserver(this);
    }
  }

  MOCK_METHOD1(OnShowNetworkStatsChanged, void(bool));
  MOCK_METHOD1(OnBlimpModeEnabled, void(bool));
  MOCK_METHOD1(OnRecordWholeDocumentChanged, void(bool));
  MOCK_METHOD0(OnRestartRequired, void());

 private:
  Settings* settings_;
};

class SettingsTest : public testing::Test {
 public:
  SettingsTest() { Settings::RegisterPrefs(prefs.registry()); }

  ~SettingsTest() override = default;

  TestingPrefServiceSimple prefs;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsTest);
};

TEST_F(SettingsTest, TestSetShowNetworkStats) {
  Settings settings(&prefs);
  MockSettingsObserver observer(&settings);

  EXPECT_FALSE(settings.show_network_stats());

  EXPECT_CALL(observer, OnShowNetworkStatsChanged(_)).Times(0);
  settings.SetShowNetworkStats(false);
  EXPECT_CALL(observer, OnShowNetworkStatsChanged(true)).Times(1);
  settings.SetShowNetworkStats(true);
  EXPECT_TRUE(settings.show_network_stats());
}

TEST_F(SettingsTest, TestSetEnableBlimpMode) {
  Settings settings(&prefs);
  MockSettingsObserver observer(&settings);

  EXPECT_FALSE(settings.IsBlimpEnabled());

  EXPECT_CALL(observer, OnBlimpModeEnabled(true)).Times(1);
  EXPECT_CALL(observer, OnRestartRequired()).Times(1);
  settings.SetEnableBlimpMode(true);
  EXPECT_TRUE(settings.IsBlimpEnabled());

  EXPECT_CALL(observer, OnBlimpModeEnabled(_)).Times(0);
  EXPECT_CALL(observer, OnRestartRequired()).Times(0);
  settings.SetEnableBlimpMode(true);
}

TEST_F(SettingsTest, TestSetRecordWholeDocument) {
  Settings settings(&prefs);
  MockSettingsObserver observer(&settings);

  EXPECT_FALSE(settings.IsRecordWholeDocument());

  EXPECT_CALL(observer, OnRecordWholeDocumentChanged(true)).Times(1);
  settings.SetRecordWholeDocument(true);
  EXPECT_TRUE(settings.IsRecordWholeDocument());

  EXPECT_CALL(observer, OnRecordWholeDocumentChanged(_)).Times(0);
  settings.SetRecordWholeDocument(true);
}

}  // namespace
}  // namespace client
}  // namespace blimp
