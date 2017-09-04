// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake_display_delegate.h"

#include <inttypes.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/display/util/display_util.h"

namespace display {

namespace {

// The EDID specification marks the top bit of the manufacturer id as reserved.
const uint16_t kReservedManufacturerID = 1 << 15;

// A random product name hash.
const uint32_t kProductCodeHash = base::Hash("Very Generic Display");

}  // namespace

FakeDisplayDelegate::FakeDisplayDelegate() {}

FakeDisplayDelegate::~FakeDisplayDelegate() {}

int64_t FakeDisplayDelegate::AddDisplay(const gfx::Size& display_size) {
  DCHECK(!display_size.IsEmpty());

  if (next_display_id_ == 0xFF) {
    LOG(ERROR) << "Exceeded display id limit";
    return Display::kInvalidDisplayID;
  }

  int64_t id = GenerateDisplayID(kReservedManufacturerID, kProductCodeHash,
                                 ++next_display_id_);

  FakeDisplaySnapshot::Builder builder;
  builder.SetId(id).SetNativeMode(display_size);
  builder.SetName(base::StringPrintf("Fake Display %" PRId64, id));

  return AddDisplay(builder.Build()) ? id : Display::kInvalidDisplayID;
}

bool FakeDisplayDelegate::AddDisplay(
    std::unique_ptr<ui::DisplaySnapshot> display) {
  DCHECK(display);

  int64_t display_id = display->display_id();
  // Check there is no existing display with the same id.
  for (auto& existing_display : displays_) {
    if (existing_display->display_id() == display_id) {
      LOG(ERROR) << "Display with id " << display_id << " already exists";
      return false;
    }
  }

  DVLOG(1) << "Added display " << display->ToString();
  displays_.push_back(std::move(display));
  OnConfigurationChanged();

  return true;
}

bool FakeDisplayDelegate::RemoveDisplay(int64_t display_id) {
  // Find display snapshot with matching id and remove it.
  for (auto iter = displays_.begin(); iter != displays_.end(); ++iter) {
    if ((*iter)->display_id() == display_id) {
      DVLOG(1) << "Removed display " << (*iter)->ToString();
      displays_.erase(iter);
      OnConfigurationChanged();
      return true;
    }
  }

  return false;
}

void FakeDisplayDelegate::Initialize() {
  DCHECK(!initialized_);

  // The default display will be an internal display with a native resolution
  // of 1024x768 if --screen-config not specified on the command line.
  std::string command_str = "1024x768/i";

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kScreenConfig))
    command_str = command_line->GetSwitchValueASCII(switches::kScreenConfig);

  if (!InitializeFromSpecString(command_str)) {
    NOTREACHED() << "Bad --" << switches::kScreenConfig << " flag provided.";
    return;
  }

  initialized_ = true;
}

void FakeDisplayDelegate::GrabServer() {}

void FakeDisplayDelegate::UngrabServer() {}

void FakeDisplayDelegate::TakeDisplayControl(
    const ui::DisplayControlCallback& callback) {
  callback.Run(false);
}

void FakeDisplayDelegate::RelinquishDisplayControl(
    const ui::DisplayControlCallback& callback) {
  callback.Run(false);
}

void FakeDisplayDelegate::SyncWithServer() {}

void FakeDisplayDelegate::SetBackgroundColor(uint32_t color_argb) {}

void FakeDisplayDelegate::ForceDPMSOn() {}

void FakeDisplayDelegate::GetDisplays(const ui::GetDisplaysCallback& callback) {
  std::vector<ui::DisplaySnapshot*> displays;
  for (auto& display : displays_)
    displays.push_back(display.get());
  callback.Run(displays);
}

void FakeDisplayDelegate::AddMode(const ui::DisplaySnapshot& output,
                                  const ui::DisplayMode* mode) {}

void FakeDisplayDelegate::Configure(const ui::DisplaySnapshot& output,
                                    const ui::DisplayMode* mode,
                                    const gfx::Point& origin,
                                    const ui::ConfigureCallback& callback) {
  // Check the display mode is appropriate for the display snapshot.
  for (const auto& existing_mode : output.modes()) {
    if (existing_mode.get() == mode) {
      callback.Run(true);
      return;
    }
  }

  callback.Run(false);
}

void FakeDisplayDelegate::CreateFrameBuffer(const gfx::Size& size) {}

void FakeDisplayDelegate::GetHDCPState(
    const ui::DisplaySnapshot& output,
    const ui::GetHDCPStateCallback& callback) {
  callback.Run(false, ui::HDCP_STATE_UNDESIRED);
}

void FakeDisplayDelegate::SetHDCPState(
    const ui::DisplaySnapshot& output,
    ui::HDCPState state,
    const ui::SetHDCPStateCallback& callback) {
  callback.Run(false);
}

std::vector<ui::ColorCalibrationProfile>
FakeDisplayDelegate::GetAvailableColorCalibrationProfiles(
    const ui::DisplaySnapshot& output) {
  return std::vector<ui::ColorCalibrationProfile>();
}

bool FakeDisplayDelegate::SetColorCalibrationProfile(
    const ui::DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  return false;
}

bool FakeDisplayDelegate::SetColorCorrection(
    const ui::DisplaySnapshot& output,
    const std::vector<ui::GammaRampRGBEntry>& degamma_lut,
    const std::vector<ui::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  return false;
}

void FakeDisplayDelegate::AddObserver(ui::NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeDisplayDelegate::RemoveObserver(ui::NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

FakeDisplayController* FakeDisplayDelegate::GetFakeDisplayController() {
  return static_cast<FakeDisplayController*>(this);
}

bool FakeDisplayDelegate::InitializeFromSpecString(const std::string& str) {
  // Start without any displays.
  if (str == "none")
    return true;

  // Split on commas and parse each display string.
  for (const std::string& part : base::SplitString(
           str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int64_t id = GenerateDisplayID(kReservedManufacturerID, kProductCodeHash,
                                   next_display_id_);
    std::unique_ptr<ui::DisplaySnapshot> snapshot =
        FakeDisplaySnapshot::CreateFromSpec(id, part);
    if (snapshot) {
      AddDisplay(std::move(snapshot));
      next_display_id_++;
    } else {
      LOG(ERROR) << "Failed to parse display \"" << part << "\"";
      return false;
    }
  }

  return true;
}

void FakeDisplayDelegate::OnConfigurationChanged() {
  if (!initialized_)
    return;

  for (ui::NativeDisplayObserver& observer : observers_)
    observer.OnConfigurationChanged();
}

}  // namespace display
