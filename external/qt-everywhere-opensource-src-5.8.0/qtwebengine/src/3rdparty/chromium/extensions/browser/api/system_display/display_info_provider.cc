// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/display_info_provider.h"

#include "base/strings/string_number_conversions.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace extensions {

namespace {

// Created on demand and will leak when the process exits.
DisplayInfoProvider* g_display_info_provider = NULL;

// Converts Rotation enum to integer.
int RotationToDegrees(display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return 0;
    case display::Display::ROTATE_90:
      return 90;
    case display::Display::ROTATE_180:
      return 180;
    case display::Display::ROTATE_270:
      return 270;
  }
  return 0;
}

}  // namespace

DisplayInfoProvider::~DisplayInfoProvider() {}

// static
DisplayInfoProvider* DisplayInfoProvider::Get() {
  if (g_display_info_provider == NULL)
    g_display_info_provider = DisplayInfoProvider::Create();
  return g_display_info_provider;
}

// static
void DisplayInfoProvider::InitializeForTesting(
    DisplayInfoProvider* display_info_provider) {
  DCHECK(display_info_provider);
  g_display_info_provider = display_info_provider;
}

// static
// Creates new DisplayUnitInfo struct for |display|.
api::system_display::DisplayUnitInfo DisplayInfoProvider::CreateDisplayUnitInfo(
    const display::Display& display,
    int64_t primary_display_id) {
  api::system_display::DisplayUnitInfo unit;
  const gfx::Rect& bounds = display.bounds();
  const gfx::Rect& work_area = display.work_area();
  unit.id = base::Int64ToString(display.id());
  unit.is_primary = (display.id() == primary_display_id);
  unit.is_internal = display.IsInternal();
  unit.is_enabled = true;
  unit.rotation = RotationToDegrees(display.rotation());
  unit.bounds.left = bounds.x();
  unit.bounds.top = bounds.y();
  unit.bounds.width = bounds.width();
  unit.bounds.height = bounds.height();
  unit.work_area.left = work_area.x();
  unit.work_area.top = work_area.y();
  unit.work_area.width = work_area.width();
  unit.work_area.height = work_area.height();
  return unit;
}

bool DisplayInfoProvider::SetDisplayLayout(const DisplayLayoutList& layout) {
  NOTREACHED();  // Implemented on Chrome OS only in override.
  return false;
}

void DisplayInfoProvider::EnableUnifiedDesktop(bool enable) {}

DisplayInfoProvider::DisplayUnitInfoList
DisplayInfoProvider::GetAllDisplaysInfo() {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  std::vector<display::Display> displays = screen->GetAllDisplays();
  DisplayUnitInfoList all_displays;
  for (const display::Display& display : displays) {
    api::system_display::DisplayUnitInfo unit =
        CreateDisplayUnitInfo(display, primary_id);
    UpdateDisplayUnitInfoForPlatform(display, &unit);
    all_displays.push_back(std::move(unit));
  }
  return all_displays;
}

DisplayInfoProvider::DisplayLayoutList DisplayInfoProvider::GetDisplayLayout() {
  NOTREACHED();  // Implemented on Chrome OS only in override.
  return DisplayLayoutList();
}

bool DisplayInfoProvider::OverscanCalibrationStart(const std::string& id) {
  return false;
}

bool DisplayInfoProvider::OverscanCalibrationAdjust(
    const std::string& id,
    const api::system_display::Insets& delta) {
  return false;
}

bool DisplayInfoProvider::OverscanCalibrationReset(const std::string& id) {
  return false;
}

bool DisplayInfoProvider::OverscanCalibrationComplete(const std::string& id) {
  return false;
}

DisplayInfoProvider::DisplayInfoProvider() {
}

}  // namespace extensions
