// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the chrome.settingsPrivate.setPref method.
class SettingsPrivateSetPrefFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateSetPrefFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setPref",
                             SETTINGSPRIVATE_SETPREF);

 protected:
  ~SettingsPrivateSetPrefFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateSetPrefFunction);
};

// Implements the chrome.settingsPrivate.getAllPrefs method.
class SettingsPrivateGetAllPrefsFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateGetAllPrefsFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getAllPrefs",
                             SETTINGSPRIVATE_GETALLPREFS);

 protected:
  ~SettingsPrivateGetAllPrefsFunction() override;

  // AsyncExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateGetAllPrefsFunction);
};

// Implements the chrome.settingsPrivate.getPref method.
class SettingsPrivateGetPrefFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateGetPrefFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getPref",
                             SETTINGSPRIVATE_GETPREF);

 protected:
  ~SettingsPrivateGetPrefFunction() override;

  // AsyncExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateGetPrefFunction);
};

// Implements the chrome.settingsPrivate.getDefaultZoomPercent method.
class SettingsPrivateGetDefaultZoomPercentFunction
    : public UIThreadExtensionFunction {
 public:
  SettingsPrivateGetDefaultZoomPercentFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getDefaultZoomPercent",
                             SETTINGSPRIVATE_GETDEFAULTZOOMPERCENTFUNCTION);

 protected:
  ~SettingsPrivateGetDefaultZoomPercentFunction() override;

  // AsyncExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateGetDefaultZoomPercentFunction);
};

// Implements the chrome.settingsPrivate.setDefaultZoomPercent method.
class SettingsPrivateSetDefaultZoomPercentFunction
    : public UIThreadExtensionFunction {
 public:
  SettingsPrivateSetDefaultZoomPercentFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setDefaultZoomPercent",
                             SETTINGSPRIVATE_SETDEFAULTZOOMPERCENTFUNCTION);

 protected:
  ~SettingsPrivateSetDefaultZoomPercentFunction() override;

  // AsyncExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateSetDefaultZoomPercentFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_
