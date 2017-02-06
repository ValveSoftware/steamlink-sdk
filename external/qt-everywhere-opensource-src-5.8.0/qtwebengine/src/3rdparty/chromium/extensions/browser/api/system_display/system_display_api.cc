// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/system_display_api.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"

#if defined(OS_CHROMEOS)
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#endif

namespace extensions {

namespace system_display = api::system_display;

const char SystemDisplayFunction::kCrosOnlyError[] =
    "Function available only on ChromeOS.";
const char SystemDisplayFunction::kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

bool SystemDisplayFunction::CheckValidExtension() {
  if (!extension())
    return true;
#if defined(OS_CHROMEOS)
  if (KioskModeInfo::IsKioskEnabled(extension()))
    return true;
#endif
  SetError(kKioskOnlyError);
  return false;
}

bool SystemDisplayGetInfoFunction::RunSync() {
  DisplayInfoProvider::DisplayUnitInfoList all_displays_info =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo();
  results_ = system_display::GetInfo::Results::Create(all_displays_info);
  return true;
}

bool SystemDisplayGetDisplayLayoutFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  DisplayInfoProvider::DisplayLayoutList display_layout =
      DisplayInfoProvider::Get()->GetDisplayLayout();
  results_ = system_display::GetDisplayLayout::Results::Create(display_layout);
  return true;
#endif
}

bool SystemDisplaySetDisplayPropertiesFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  if (!CheckValidExtension())
    return false;
  std::string error;
  std::unique_ptr<system_display::SetDisplayProperties::Params> params(
      system_display::SetDisplayProperties::Params::Create(*args_));
  bool result =
      DisplayInfoProvider::Get()->SetInfo(params->id, params->info, &error);
  if (!result)
    SetError(error);
  return result;
#endif
}

bool SystemDisplaySetDisplayLayoutFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  if (!CheckValidExtension())
    return false;
  std::unique_ptr<system_display::SetDisplayLayout::Params> params(
      system_display::SetDisplayLayout::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->SetDisplayLayout(params->layouts)) {
    SetError("Unable to set display layout");
    return false;
  }
  return true;
#endif
}

bool SystemDisplayEnableUnifiedDesktopFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  if (!CheckValidExtension())
    return false;
  std::unique_ptr<system_display::EnableUnifiedDesktop::Params> params(
      system_display::EnableUnifiedDesktop::Params::Create(*args_));
  DisplayInfoProvider::Get()->EnableUnifiedDesktop(params->enabled);
  return true;
#endif
}

bool SystemDisplayOverscanCalibrationStartFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  if (!CheckValidExtension())
    return false;
  std::unique_ptr<system_display::OverscanCalibrationStart::Params> params(
      system_display::OverscanCalibrationStart::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationStart(params->id)) {
    SetError("Invalid display ID: " + params->id);
    return false;
  }
  return true;
#endif
}

bool SystemDisplayOverscanCalibrationAdjustFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  if (!CheckValidExtension())
    return false;
  std::unique_ptr<system_display::OverscanCalibrationAdjust::Params> params(
      system_display::OverscanCalibrationAdjust::Params::Create(*args_));
  if (!params) {
    SetError("Invalid parameters");
    return false;
  }
  if (!DisplayInfoProvider::Get()->OverscanCalibrationAdjust(params->id,
                                                             params->delta)) {
    SetError("Calibration not started for display ID: " + params->id);
    return false;
  }
  return true;
#endif
}

bool SystemDisplayOverscanCalibrationResetFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  if (!CheckValidExtension())
    return false;
  std::unique_ptr<system_display::OverscanCalibrationReset::Params> params(
      system_display::OverscanCalibrationReset::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationReset(params->id)) {
    SetError("Calibration not started for display ID: " + params->id);
    return false;
  }
  return true;
#endif
}

bool SystemDisplayOverscanCalibrationCompleteFunction::RunSync() {
#if !defined(OS_CHROMEOS)
  SetError(kCrosOnlyError);
  return false;
#else
  if (!CheckValidExtension())
    return false;
  std::unique_ptr<system_display::OverscanCalibrationComplete::Params> params(
      system_display::OverscanCalibrationComplete::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationComplete(params->id)) {
    SetError("Calibration not started for display ID: " + params->id);
    return false;
  }
  return true;
#endif
}

}  // namespace extensions
