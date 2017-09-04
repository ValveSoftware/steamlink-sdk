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

namespace display = api::system_display;

const char SystemDisplayFunction::kCrosOnlyError[] =
    "Function available only on ChromeOS.";
const char SystemDisplayFunction::kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

bool SystemDisplayFunction::PreRunValidation(std::string* error) {
  if (!UIThreadExtensionFunction::PreRunValidation(error))
    return false;

#if !defined(OS_CHROMEOS)
  *error = kCrosOnlyError;
  return false;
#else
  if (!ShouldRestrictToKioskAndWebUI())
    return true;

  if (source_context_type() == Feature::WEBUI_CONTEXT)
    return true;
  if (KioskModeInfo::IsKioskEnabled(extension()))
    return true;
  *error = kKioskOnlyError;
  return false;
#endif
}

bool SystemDisplayFunction::ShouldRestrictToKioskAndWebUI() {
  return true;
}

ExtensionFunction::ResponseAction SystemDisplayGetInfoFunction::Run() {
  DisplayInfoProvider::DisplayUnitInfoList all_displays_info =
      DisplayInfoProvider::Get()->GetAllDisplaysInfo();
  return RespondNow(
      ArgumentList(display::GetInfo::Results::Create(all_displays_info)));
}

ExtensionFunction::ResponseAction SystemDisplayGetDisplayLayoutFunction::Run() {
  DisplayInfoProvider::DisplayLayoutList display_layout =
      DisplayInfoProvider::Get()->GetDisplayLayout();
  return RespondNow(
      ArgumentList(display::GetDisplayLayout::Results::Create(display_layout)));
}

bool SystemDisplayGetDisplayLayoutFunction::ShouldRestrictToKioskAndWebUI() {
  return false;
}

ExtensionFunction::ResponseAction
SystemDisplaySetDisplayPropertiesFunction::Run() {
  std::string error;
  std::unique_ptr<display::SetDisplayProperties::Params> params(
      display::SetDisplayProperties::Params::Create(*args_));
  bool result =
      DisplayInfoProvider::Get()->SetInfo(params->id, params->info, &error);
  if (!result)
    return RespondNow(Error(error));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SystemDisplaySetDisplayLayoutFunction::Run() {
  std::unique_ptr<display::SetDisplayLayout::Params> params(
      display::SetDisplayLayout::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->SetDisplayLayout(params->layouts))
    return RespondNow(Error("Unable to set display layout"));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayEnableUnifiedDesktopFunction::Run() {
  std::unique_ptr<display::EnableUnifiedDesktop::Params> params(
      display::EnableUnifiedDesktop::Params::Create(*args_));
  DisplayInfoProvider::Get()->EnableUnifiedDesktop(params->enabled);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationStartFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationStart::Params> params(
      display::OverscanCalibrationStart::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationStart(params->id))
    return RespondNow(Error("Invalid display ID: " + params->id));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationAdjustFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationAdjust::Params> params(
      display::OverscanCalibrationAdjust::Params::Create(*args_));
  if (!params)
    return RespondNow(Error("Invalid parameters"));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationAdjust(params->id,
                                                             params->delta)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationResetFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationReset::Params> params(
      display::OverscanCalibrationReset::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationReset(params->id))
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SystemDisplayOverscanCalibrationCompleteFunction::Run() {
  std::unique_ptr<display::OverscanCalibrationComplete::Params> params(
      display::OverscanCalibrationComplete::Params::Create(*args_));
  if (!DisplayInfoProvider::Get()->OverscanCalibrationComplete(params->id)) {
    return RespondNow(
        Error("Calibration not started for display ID: " + params->id));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
