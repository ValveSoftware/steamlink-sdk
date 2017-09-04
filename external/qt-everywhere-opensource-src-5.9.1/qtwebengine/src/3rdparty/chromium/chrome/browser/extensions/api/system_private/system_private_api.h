// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This extension API contains system-wide preferences and functions that shall
// be only available to component extensions.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_PRIVATE_SYSTEM_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_PRIVATE_SYSTEM_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemPrivateGetIncognitoModeAvailabilityFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("systemPrivate.getIncognitoModeAvailability",
                             SYSTEMPRIVATE_GETINCOGNITOMODEAVAILABILITY)

 protected:
  ~SystemPrivateGetIncognitoModeAvailabilityFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// API function which returns the status of system update.
class SystemPrivateGetUpdateStatusFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("systemPrivate.getUpdateStatus",
                             SYSTEMPRIVATE_GETUPDATESTATUS)

 protected:
  ~SystemPrivateGetUpdateStatusFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// API function which returns the Google API key.
class SystemPrivateGetApiKeyFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("systemPrivate.getApiKey", SYSTEMPRIVATE_GETAPIKEY)

 protected:
  ~SystemPrivateGetApiKeyFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Dispatches systemPrivate.onBrightnessChanged event for extensions.
void DispatchBrightnessChangedEvent(int brightness, bool user_initiated);

// Dispatches systemPrivate.onVolumeChanged event for extensions.
void DispatchVolumeChangedEvent(double volume, bool is_volume_muted);

// Dispatches systemPrivate.onScreenChanged event for extensions.
void DispatchScreenUnlockedEvent();

// Dispatches systemPrivate.onWokeUp event for extensions.
void DispatchWokeUpEvent();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_PRIVATE_SYSTEM_PRIVATE_API_H_
