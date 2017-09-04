// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemDisplayFunction : public UIThreadExtensionFunction {
 public:
  static const char kCrosOnlyError[];
  static const char kKioskOnlyError[];

 protected:
  ~SystemDisplayFunction() override {}
  bool PreRunValidation(std::string* error) override;

  // Returns true if this function should be restricted to kiosk-mode apps and
  // webui. The default is true.
  virtual bool ShouldRestrictToKioskAndWebUI();
};

// This function inherits from UIThreadExtensionFunction because, unlike the
// rest of this API, it's available on all platforms.
class SystemDisplayGetInfoFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getInfo", SYSTEM_DISPLAY_GETINFO);

 protected:
  ~SystemDisplayGetInfoFunction() override {}
  ResponseAction Run() override;
};

class SystemDisplayGetDisplayLayoutFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getDisplayLayout",
                             SYSTEM_DISPLAY_GETDISPLAYLAYOUT);

 protected:
  ~SystemDisplayGetDisplayLayoutFunction() override {}
  ResponseAction Run() override;
  bool ShouldRestrictToKioskAndWebUI() override;
};

class SystemDisplaySetDisplayPropertiesFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayProperties",
                             SYSTEM_DISPLAY_SETDISPLAYPROPERTIES);

 protected:
  ~SystemDisplaySetDisplayPropertiesFunction() override {}
  ResponseAction Run() override;
};

class SystemDisplaySetDisplayLayoutFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayLayout",
                             SYSTEM_DISPLAY_SETDISPLAYLAYOUT);

 protected:
  ~SystemDisplaySetDisplayLayoutFunction() override {}
  ResponseAction Run() override;
};

class SystemDisplayEnableUnifiedDesktopFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.enableUnifiedDesktop",
                             SYSTEM_DISPLAY_ENABLEUNIFIEDDESKTOP);

 protected:
  ~SystemDisplayEnableUnifiedDesktopFunction() override {}
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationStartFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationStart",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONSTART);

 protected:
  ~SystemDisplayOverscanCalibrationStartFunction() override {}
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationAdjustFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationAdjust",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONADJUST);

 protected:
  ~SystemDisplayOverscanCalibrationAdjustFunction() override {}
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationResetFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationReset",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONRESET);

 protected:
  ~SystemDisplayOverscanCalibrationResetFunction() override {}
  ResponseAction Run() override;
};

class SystemDisplayOverscanCalibrationCompleteFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationComplete",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONCOMPLETE);

 protected:
  ~SystemDisplayOverscanCalibrationCompleteFunction() override {}
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
