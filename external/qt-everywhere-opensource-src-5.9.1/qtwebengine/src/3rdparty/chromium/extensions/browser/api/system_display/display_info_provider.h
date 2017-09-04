// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"

namespace display {
class Display;
}

namespace extensions {

namespace api {
namespace system_display {
struct DisplayLayout;
struct DisplayProperties;
struct DisplayUnitInfo;
struct Insets;
}
}

class DisplayInfoProvider {
 public:
  using DisplayUnitInfoList = std::vector<api::system_display::DisplayUnitInfo>;
  using DisplayLayoutList = std::vector<api::system_display::DisplayLayout>;

  virtual ~DisplayInfoProvider();

  // Returns a pointer to DisplayInfoProvider or NULL if Create()
  // or InitializeForTesting() or not called yet.
  static DisplayInfoProvider* Get();

  // This is for tests that run in its own process (e.g. browser_tests).
  // Using this in other tests (e.g. unit_tests) will result in DCHECK failure.
  static void InitializeForTesting(DisplayInfoProvider* display_info_provider);

  // Updates the display with |display_id| according to |info|. Returns whether
  // the display was successfully updated. On failure, no display parameters
  // should be changed, and |error| should be set to the error string.
  virtual bool SetInfo(const std::string& display_id,
                       const api::system_display::DisplayProperties& info,
                       std::string* error) = 0;

  // Implements SetDisplayLayout methods. See system_display.idl. Returns
  // false if the layout input is invalid.
  virtual bool SetDisplayLayout(const DisplayLayoutList& layouts);

  // Enables the unified desktop feature.
  virtual void EnableUnifiedDesktop(bool enable);

  // Gets display information.
  virtual DisplayUnitInfoList GetAllDisplaysInfo();

  // Gets display layout information.
  virtual DisplayLayoutList GetDisplayLayout();

  // Implements overscan calbiration methods. See system_display.idl. These
  // return false if |id| is invalid.
  virtual bool OverscanCalibrationStart(const std::string& id);
  virtual bool OverscanCalibrationAdjust(
      const std::string& id,
      const api::system_display::Insets& delta);
  virtual bool OverscanCalibrationReset(const std::string& id);
  virtual bool OverscanCalibrationComplete(const std::string& id);

 protected:
  DisplayInfoProvider();

  // Create a DisplayUnitInfo from a display::Display for implementations of
  // GetAllDisplaysInfo()
  static api::system_display::DisplayUnitInfo CreateDisplayUnitInfo(
      const display::Display& display,
      int64_t primary_display_id);

 private:
  static DisplayInfoProvider* Create();

  // Update the content of the |unit| obtained for |display| using
  // platform specific method.
  virtual void UpdateDisplayUnitInfoForPlatform(
      const display::Display& display,
      api::system_display::DisplayUnitInfo* unit) = 0;

  DISALLOW_COPY_AND_ASSIGN(DisplayInfoProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_H_
