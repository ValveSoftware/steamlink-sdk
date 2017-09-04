// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_FEATURE_H_
#define BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_FEATURE_H_

#include "base/macros.h"
#include "blimp/client/core/settings/settings_observer.h"
#include "blimp/net/blimp_message_processor.h"

namespace blimp {
namespace client {

class Settings;

// The feature is used to send global settings to the engine.
class SettingsFeature : public BlimpMessageProcessor, public SettingsObserver {
 public:
  // Caller ensures |settings| outlives this object.
  explicit SettingsFeature(Settings* settings);
  ~SettingsFeature() override;

  // Set the BlimpMessageProcessor that will be used to send
  // BlimpMessage::SETTINGS messages to the engine.
  void set_outgoing_message_processor(
      std::unique_ptr<BlimpMessageProcessor> processor);

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  // SettingsObserver implementation.
  void OnRecordWholeDocumentChanged(bool enable) override;

  // Send the necessary settings to the engine.
  void PushSettings();

 private:
  // Used to send BlimpMessage::TAB_CONTROL messages to the engine.
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor_;

  Settings* settings_;

  DISALLOW_COPY_AND_ASSIGN(SettingsFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_FEATURE_H_
