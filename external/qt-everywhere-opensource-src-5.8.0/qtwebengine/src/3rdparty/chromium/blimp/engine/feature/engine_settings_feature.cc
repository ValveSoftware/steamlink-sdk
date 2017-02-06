// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/engine_settings_feature.h"

#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/settings.pb.h"
#include "blimp/engine/app/settings_manager.h"
#include "blimp/engine/common/blimp_content_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/web_preferences.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace engine {

EngineSettingsFeature::EngineSettingsFeature(SettingsManager* settings_manager)
    : settings_manager_(settings_manager) {
  DCHECK(settings_manager_);
}

EngineSettingsFeature::~EngineSettingsFeature() {}

void EngineSettingsFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK_EQ(BlimpMessage::kSettings, message->feature_case());

  const SettingsMessage& settings = message->settings();
  DCHECK(settings.has_engine_settings());

  const EngineSettingsMessage& engine_settings = settings.engine_settings();
  ProcessSettings(engine_settings);

  callback.Run(net::OK);
}

void EngineSettingsFeature::ProcessSettings(
    const EngineSettingsMessage& engine_settings) {
  if (engine_settings.has_record_whole_document()) {
    EngineSettings settings = settings_manager_->GetEngineSettings();
    settings.record_whole_document = engine_settings.record_whole_document();
    settings_manager_->UpdateEngineSettings(settings);
  }

  // Set the client OS information for building user agent.
  if (engine_settings.has_client_os_info()) {
    std::string client_os_info = engine_settings.client_os_info();
    SetClientOSInfo(client_os_info);
  }
}

}  // namespace engine
}  // namespace blimp
