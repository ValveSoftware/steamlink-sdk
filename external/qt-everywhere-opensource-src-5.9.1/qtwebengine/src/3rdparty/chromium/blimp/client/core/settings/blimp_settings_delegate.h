// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_BLIMP_SETTINGS_DELEGATE_H_
#define BLIMP_CLIENT_CORE_SETTINGS_BLIMP_SETTINGS_DELEGATE_H_

#include "base/macros.h"

namespace blimp {
namespace client {

class ConnectionStatus;
class IdentitySource;

// BlimpSettingsDelegate is an interface that provides functionalities needed
// by core/settings.
class BlimpSettingsDelegate {
 public:
  // Get IdentitySource that provides sign in state change.
  virtual IdentitySource* GetIdentitySource() = 0;

  // Get the connection status that provides engine IP and broadcasts network
  // events.
  virtual ConnectionStatus* GetConnectionStatus() = 0;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SETTINGS_BLIMP_SETTINGS_DELEGATE_H_
