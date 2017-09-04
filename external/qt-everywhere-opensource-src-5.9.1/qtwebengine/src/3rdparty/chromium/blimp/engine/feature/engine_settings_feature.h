// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_FEATURE_ENGINE_SETTINGS_FEATURE_H_
#define BLIMP_ENGINE_FEATURE_ENGINE_SETTINGS_FEATURE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "blimp/net/blimp_message_processor.h"
#include "content/public/browser/render_view_host.h"

namespace blimp {
class EngineSettingsMessage;

namespace engine {
class SettingsManager;

// The feature is used to receive global settings from the client.
class EngineSettingsFeature : public BlimpMessageProcessor {
 public:
  explicit EngineSettingsFeature(SettingsManager* settings_manager);
  ~EngineSettingsFeature() override;

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  void ProcessSettings(const EngineSettingsMessage& settings);

  SettingsManager* settings_manager_;

  DISALLOW_COPY_AND_ASSIGN(EngineSettingsFeature);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_FEATURE_ENGINE_SETTINGS_FEATURE_H_
