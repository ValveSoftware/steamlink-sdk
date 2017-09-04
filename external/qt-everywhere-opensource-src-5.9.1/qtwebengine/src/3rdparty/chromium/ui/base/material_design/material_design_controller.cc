// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/material_design/material_design_controller.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_CHROMEOS)
#include "ui/base/touch/touch_device.h"
#include "ui/events/devices/device_data_manager.h"

#if defined(USE_OZONE)
#include <fcntl.h>

#include "base/files/file_enumerator.h"
#include "base/threading/thread_restrictions.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#endif  // defined(USE_OZONE)

#endif  // defined(OS_CHROMEOS)

namespace ui {

bool MaterialDesignController::is_mode_initialized_ = false;

MaterialDesignController::Mode MaterialDesignController::mode_ =
    MaterialDesignController::MATERIAL_NORMAL;

bool MaterialDesignController::include_secondary_ui_ = false;

// static
void MaterialDesignController::Initialize() {
  TRACE_EVENT0("startup", "MaterialDesignController::InitializeMode");
  CHECK(!is_mode_initialized_);
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTopChromeMD);

  if (switch_value == switches::kTopChromeMDMaterial) {
    SetMode(MATERIAL_NORMAL);
  } else if (switch_value == switches::kTopChromeMDMaterialHybrid) {
    SetMode(MATERIAL_HYBRID);
  } else {
    if (!switch_value.empty()) {
      LOG(ERROR) << "Invalid value='" << switch_value
                 << "' for command line switch '" << switches::kTopChromeMD
                 << "'.";
    }
    SetMode(DefaultMode());
  }

  include_secondary_ui_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExtendMdToSecondaryUi);
}

// static
MaterialDesignController::Mode MaterialDesignController::GetMode() {
  CHECK(is_mode_initialized_);
  return mode_;
}

// static
bool MaterialDesignController::IsSecondaryUiMaterial() {
  return include_secondary_ui_;
}

// static
MaterialDesignController::Mode MaterialDesignController::DefaultMode() {
#if defined(OS_CHROMEOS)
  // If a scan of available devices has already completed, use material-hybrid
  // if a touchscreen is present.
  if (DeviceDataManager::HasInstance() &&
      DeviceDataManager::GetInstance()->AreDeviceListsComplete()) {
    return GetTouchScreensAvailability() == TouchScreensAvailability::ENABLED
               ? MATERIAL_HYBRID
               : MATERIAL_NORMAL;
  }

#if defined(USE_OZONE)
  // Otherwise perform our own scan to determine the presence of a touchscreen.
  // Note this is a one-time call that occurs during device startup or restart.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FileEnumerator file_enum(
      base::FilePath(FILE_PATH_LITERAL("/dev/input")), false,
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("event*[0-9]"));
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    EventDeviceInfo devinfo;
    int fd = open(path.value().c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
      if (devinfo.Initialize(fd, path) && devinfo.HasTouchscreen()) {
        close(fd);
        return MATERIAL_HYBRID;
      }
      close(fd);
    }
  }
#endif  // defined(USE_OZONE)
#endif  // defined(OS_CHROMEOS)

  return MATERIAL_NORMAL;
}

// static
void MaterialDesignController::Uninitialize() {
  is_mode_initialized_ = false;
}

// static
void MaterialDesignController::SetMode(MaterialDesignController::Mode mode) {
  mode_ = mode;
  is_mode_initialized_ = true;
}

}  // namespace ui
