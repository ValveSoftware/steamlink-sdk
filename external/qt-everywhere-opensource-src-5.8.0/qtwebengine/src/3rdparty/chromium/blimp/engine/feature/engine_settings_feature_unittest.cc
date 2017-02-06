// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/engine_settings_feature.h"

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/settings.pb.h"
#include "blimp/engine/app/settings_manager.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {

namespace {

class EngineSettingsFeatureTest : public testing::Test {
 public:
  EngineSettingsFeatureTest() : feature_(&settings_manager_) {}

 protected:
  SettingsManager settings_manager_;
  EngineSettingsFeature feature_;
};

TEST_F(EngineSettingsFeatureTest, UpdatesSettingsCorrectly) {
  EXPECT_FALSE(settings_manager_.GetEngineSettings().record_whole_document);

  EngineSettingsMessage* engine_settings;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&engine_settings);
  engine_settings->set_record_whole_document(true);

  net::TestCompletionCallback cb;
  feature_.ProcessMessage(std::move(message), cb.callback());

  EXPECT_TRUE(settings_manager_.GetEngineSettings().record_whole_document);
}

}  // namespace

}  // namespace engine
}  // namespace blimp
