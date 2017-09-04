// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_OBSERVER_H_
#define BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_OBSERVER_H_

#include "base/observer_list.h"

namespace blimp {
namespace client {

class SettingsObserver {
 public:
  virtual ~SettingsObserver() = default;

  // Invoked when enabling or disabling Blimp mode in the Settings.
  virtual void OnBlimpModeEnabled(bool enable) {}

  // Invoked when enabling or disabling to show the network stats in the
  // Settings.
  virtual void OnShowNetworkStatsChanged(bool enable) {}

  // Invoked when enabling or disabling to download the whole document in the
  // Settings.
  virtual void OnRecordWholeDocumentChanged(bool enable) {}

  // Invoked when a setting changed that requires the application to restart
  // before it can take effect.
  virtual void OnRestartRequired() {}

 protected:
  SettingsObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsObserver);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SETTINGS_SETTINGS_OBSERVER_H_
