// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/settings/settings_feature.h"

#include "blimp/client/core/settings/settings.h"
#include "blimp/client/core/settings/user_agent.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/settings.pb.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace client {

SettingsFeature::SettingsFeature(Settings* settings) : settings_(settings) {
  DCHECK(settings_);
}

SettingsFeature::~SettingsFeature() = default;

void SettingsFeature::set_outgoing_message_processor(
    std::unique_ptr<BlimpMessageProcessor> processor) {
  outgoing_message_processor_ = std::move(processor);
}

void SettingsFeature::ProcessMessage(std::unique_ptr<BlimpMessage> message,
                                     const net::CompletionCallback& callback) {
  // We don't receive any messages from the engine yet.
  NOTREACHED() << "Invalid settings message received from the engine.";
  callback.Run(net::OK);
}

void SettingsFeature::OnRecordWholeDocumentChanged(bool enable) {
  EngineSettingsMessage* engine_settings;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&engine_settings);
  engine_settings->set_record_whole_document(enable);
  outgoing_message_processor_->ProcessMessage(std::move(message),
                                              net::CompletionCallback());
}

void SettingsFeature::PushSettings() {
  EngineSettingsMessage* engine_settings;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&engine_settings);
  engine_settings->set_record_whole_document(
      settings_->IsRecordWholeDocument());
  engine_settings->set_client_os_info(GetOSVersionInfoForUserAgent());
  outgoing_message_processor_->ProcessMessage(std::move(message),
                                              net::CompletionCallback());
}

}  // namespace client
}  // namespace blimp
