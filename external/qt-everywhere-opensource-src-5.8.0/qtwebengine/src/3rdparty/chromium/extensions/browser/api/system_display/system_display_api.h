// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemDisplayFunction : public SyncExtensionFunction {
 public:
  static const char kCrosOnlyError[];
  static const char kKioskOnlyError[];

 protected:
  ~SystemDisplayFunction() override {}
  bool CheckValidExtension();
};

class SystemDisplayGetInfoFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getInfo", SYSTEM_DISPLAY_GETINFO);

 protected:
  ~SystemDisplayGetInfoFunction() override {}
  bool RunSync() override;
};

class SystemDisplayGetDisplayLayoutFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.getDisplayLayout",
                             SYSTEM_DISPLAY_GETDISPLAYLAYOUT);

 protected:
  ~SystemDisplayGetDisplayLayoutFunction() override {}
  bool RunSync() override;
};

class SystemDisplaySetDisplayPropertiesFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayProperties",
                             SYSTEM_DISPLAY_SETDISPLAYPROPERTIES);

 protected:
  ~SystemDisplaySetDisplayPropertiesFunction() override {}
  bool RunSync() override;
};

class SystemDisplaySetDisplayLayoutFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.setDisplayLayout",
                             SYSTEM_DISPLAY_SETDISPLAYLAYOUT);

 protected:
  ~SystemDisplaySetDisplayLayoutFunction() override {}
  bool RunSync() override;
};

class SystemDisplayEnableUnifiedDesktopFunction : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.enableUnifiedDesktop",
                             SYSTEM_DISPLAY_ENABLEUNIFIEDDESKTOP);

 protected:
  ~SystemDisplayEnableUnifiedDesktopFunction() override {}
  bool RunSync() override;
};

class SystemDisplayOverscanCalibrationStartFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationStart",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONSTART);

 protected:
  ~SystemDisplayOverscanCalibrationStartFunction() override {}
  bool RunSync() override;
};

class SystemDisplayOverscanCalibrationAdjustFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationAdjust",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONADJUST);

 protected:
  ~SystemDisplayOverscanCalibrationAdjustFunction() override {}
  bool RunSync() override;
};

class SystemDisplayOverscanCalibrationResetFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationReset",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONRESET);

 protected:
  ~SystemDisplayOverscanCalibrationResetFunction() override {}
  bool RunSync() override;
};

class SystemDisplayOverscanCalibrationCompleteFunction
    : public SystemDisplayFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.display.overscanCalibrationComplete",
                             SYSTEM_DISPLAY_OVERSCANCALIBRATIONCOMPLETE);

 protected:
  ~SystemDisplayOverscanCalibrationCompleteFunction() override {}
  bool RunSync() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_SYSTEM_DISPLAY_API_H_
