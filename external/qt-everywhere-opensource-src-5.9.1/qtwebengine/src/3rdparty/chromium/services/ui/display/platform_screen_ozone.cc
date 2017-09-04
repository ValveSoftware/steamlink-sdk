// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/platform_screen_ozone.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace display {
namespace {

// Needed for DisplayConfigurator::ForceInitialConfigure.
const SkColor kChromeOsBootColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);

const float kInchInMm = 25.4f;

float ComputeDisplayDPI(const gfx::Size& pixel_size,
                        const gfx::Size& physical_size) {
  DCHECK(!physical_size.IsEmpty());
  return (pixel_size.width() / static_cast<float>(physical_size.width())) *
         kInchInMm;
}

// Finds the device scale factor based on the display DPI. Will use forced
// device scale factor if provided via command line.
float FindDeviceScaleFactor(float dpi) {
  if (Display::HasForceDeviceScaleFactor())
    return Display::GetForcedDeviceScaleFactor();

  // TODO(kylechar): If dpi > 150 then ash uses 1.25 now. Ignoring that for now.
  if (dpi > 200.0)
    return 2.0f;
  else
    return 1.0f;
}

}  // namespace

// static
std::unique_ptr<PlatformScreen> PlatformScreen::Create() {
  return base::MakeUnique<PlatformScreenOzone>();
}

PlatformScreenOzone::PlatformScreenOzone() {}

PlatformScreenOzone::~PlatformScreenOzone() {
  // We are shutting down and don't want to make anymore display changes.
  fake_display_controller_ = nullptr;
  display_configurator_.RemoveObserver(this);
}

void PlatformScreenOzone::AddInterfaces(
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::DisplayController>(this);
  registry->AddInterface<mojom::TestDisplayController>(this);
}

void PlatformScreenOzone::Init(PlatformScreenDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;

  std::unique_ptr<ui::NativeDisplayDelegate> native_display_delegate =
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();

  // The FakeDisplayController gives us a way to make the NativeDisplayDelegate
  // pretend something display related has happened.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    fake_display_controller_ =
        native_display_delegate->GetFakeDisplayController();
  }

  // We want display configuration to happen even off device to keep the control
  // flow similar.
  display_configurator_.set_configure_display(true);
  display_configurator_.AddObserver(this);
  display_configurator_.set_state_controller(this);
  display_configurator_.Init(std::move(native_display_delegate), false);
  display_configurator_.ForceInitialConfigure(kChromeOsBootColor);
}

void PlatformScreenOzone::RequestCloseDisplay(int64_t display_id) {
  if (!fake_display_controller_ || wait_for_display_config_update_)
    return;

  CachedDisplayIterator iter = GetCachedDisplayIterator(display_id);
  if (iter != cached_displays_.end()) {
    // Tell the NDD to remove the display. PlatformScreen will get an update
    // that the display configuration has changed and the display will be gone.
    wait_for_display_config_update_ =
        fake_display_controller_->RemoveDisplay(iter->id);
  }
}

int64_t PlatformScreenOzone::GetPrimaryDisplayId() const {
  return primary_display_id_;
}

void PlatformScreenOzone::ToggleAddRemoveDisplay() {
  if (!fake_display_controller_ || wait_for_display_config_update_)
    return;

  if (cached_displays_.size() == 1) {
    const gfx::Size& pixel_size = cached_displays_[0].metrics.pixel_size;
    wait_for_display_config_update_ =
        fake_display_controller_->AddDisplay(pixel_size) !=
        Display::kInvalidDisplayID;
  } else if (cached_displays_.size() > 1) {
    wait_for_display_config_update_ =
        fake_display_controller_->RemoveDisplay(cached_displays_.back().id);
  } else {
    NOTREACHED();
  }
}

void PlatformScreenOzone::ToggleDisplayResolution() {
  DisplayInfo& display = cached_displays_[0];

  // Toggle the display size to use.
  size_t num_sizes = display.supported_sizes.size();
  for (size_t i = 0; i < num_sizes; i++) {
    if (display.supported_sizes[i] == display.requested_size) {
      if (i + 1 == num_sizes)
        display.requested_size = display.supported_sizes[0];
      else
        display.requested_size = display.supported_sizes[i + 1];
      break;
    }
  }

  display_configurator_.OnConfigurationChanged();
}

void PlatformScreenOzone::SwapPrimaryDisplay() {
  const size_t num_displays = cached_displays_.size();
  if (num_displays <= 1)
    return;

  // Find index of current primary display.
  size_t primary_display_index = 0;
  for (size_t i = 0; i < num_displays; i++) {
    if (cached_displays_[i].id == primary_display_id_) {
      primary_display_index = i;
      break;
    }
  }

  // Set next display index as primary, or loop back to first display if last.
  if (primary_display_index + 1 == num_displays) {
    primary_display_id_ = cached_displays_[0].id;
  } else {
    primary_display_id_ = cached_displays_[primary_display_index + 1].id;
  }

  delegate_->OnPrimaryDisplayChanged(primary_display_id_);
}

void PlatformScreenOzone::SetDisplayWorkArea(int64_t display_id,
                                             const gfx::Size& size,
                                             const gfx::Insets& insets) {
  CachedDisplayIterator iter = GetCachedDisplayIterator(display_id);
  if (iter == cached_displays_.end()) {
    NOTREACHED() << display_id;
    return;
  }

  DisplayInfo& display_info = *iter;
  if (display_info.metrics.bounds.size() == size) {
    gfx::Rect new_work_area = display_info.metrics.bounds;
    new_work_area.Inset(insets);

    if (new_work_area != display_info.metrics.work_area) {
      display_info.last_work_area_insets = insets;
      display_info.metrics.work_area = new_work_area;
      display_info.modified = true;
      UpdateCachedDisplays();
    }
  }
}

PlatformScreenOzone::DisplayInfo::DisplayInfo() = default;
PlatformScreenOzone::DisplayInfo::DisplayInfo(const DisplayInfo& other) =
    default;
PlatformScreenOzone::DisplayInfo::~DisplayInfo() = default;

void PlatformScreenOzone::ProcessRemovedDisplays(
    const ui::DisplayConfigurator::DisplayStateList& snapshots) {
  std::vector<int64_t> current_ids;
  for (ui::DisplaySnapshot* snapshot : snapshots)
    current_ids.push_back(snapshot->display_id());

  // Find cached displays with no matching snapshot and mark as removed.
  for (DisplayInfo& display : cached_displays_) {
    if (std::find(current_ids.begin(), current_ids.end(), display.id) ==
        current_ids.end()) {
      display.removed = true;
      if (primary_display_id_ == display.id)
        primary_display_id_ = Display::kInvalidDisplayID;
    }
  }
}

void PlatformScreenOzone::ProcessModifiedDisplays(
    const ui::DisplayConfigurator::DisplayStateList& snapshots) {
  for (ui::DisplaySnapshot* snapshot : snapshots) {
    auto iter = GetCachedDisplayIterator(snapshot->display_id());
    if (iter != cached_displays_.end()) {
      DisplayInfo& display_info = *iter;
      ViewportMetrics new_metrics =
          MetricsFromSnapshot(*snapshot, display_info.metrics.bounds.origin());
      new_metrics.work_area.Inset(display_info.last_work_area_insets);

      if (new_metrics != display_info.metrics) {
        display_info.metrics = new_metrics;
        display_info.modified = true;
      }
    }
  }
}

void PlatformScreenOzone::UpdateCachedDisplays() {
  // Walk through cached displays after processing the snapshots to find any
  // removed or modified displays. This ensures that we only send one update per
  // display to the delegate.
  next_display_origin_.SetPoint(0, 0);
  for (auto iter = cached_displays_.begin(); iter != cached_displays_.end();) {
    DisplayInfo& display_info = *iter;
    if (display_info.removed) {
      // Update delegate and remove from cache.
      delegate_->OnDisplayRemoved(display_info.id);
      iter = cached_displays_.erase(iter);
    } else {
      // Check if the display origin needs to be updated.
      if (next_display_origin_ != display_info.metrics.bounds.origin()) {
        display_info.metrics.bounds.set_origin(next_display_origin_);
        display_info.metrics.work_area.set_origin(next_display_origin_);
        display_info.modified = true;
      }
      next_display_origin_.Offset(display_info.metrics.bounds.width(), 0);

      // Check if the window bounds have changed and update delegate.
      if (display_info.modified) {
        display_info.modified = false;
        delegate_->OnDisplayModified(display_info.id, display_info.metrics);
      }
      ++iter;
    }
  }
}

void PlatformScreenOzone::AddNewDisplays(
    const ui::DisplayConfigurator::DisplayStateList& snapshots) {
  for (ui::DisplaySnapshot* snapshot : snapshots) {
    const int64_t id = snapshot->display_id();

    // Check if display already exists and skip.
    if (GetCachedDisplayIterator(id) != cached_displays_.end())
      continue;

    DisplayInfo display_info;
    display_info.id = snapshot->display_id();
    display_info.metrics = MetricsFromSnapshot(*snapshot, next_display_origin_);

    // Store the display mode sizes so we can toggle through them.
    for (auto& mode : snapshot->modes()) {
      display_info.supported_sizes.push_back(mode->size());
      if (mode.get() == snapshot->current_mode())
        display_info.requested_size = mode->size();
    }

    // Move the origin so that next display is to the right of current display.
    next_display_origin_.Offset(display_info.metrics.bounds.width(), 0);

    cached_displays_.push_back(display_info);
    delegate_->OnDisplayAdded(display_info.id, display_info.metrics);

    // If we have no primary display then this one should be it.
    if (primary_display_id_ == Display::kInvalidDisplayID) {
      primary_display_id_ = id;
      delegate_->OnPrimaryDisplayChanged(primary_display_id_);
    }
  }
}

PlatformScreenOzone::CachedDisplayIterator
PlatformScreenOzone::GetCachedDisplayIterator(int64_t display_id) {
  return std::find_if(cached_displays_.begin(), cached_displays_.end(),
                      [display_id](const DisplayInfo& display_info) {
                        return display_info.id == display_id;
                      });
}

ViewportMetrics PlatformScreenOzone::MetricsFromSnapshot(
    const ui::DisplaySnapshot& snapshot,
    const gfx::Point& origin) {
  const ui::DisplayMode* current_mode = snapshot.current_mode();
  DCHECK(current_mode);

  ViewportMetrics metrics;
  metrics.pixel_size = current_mode->size();
  metrics.device_scale_factor = FindDeviceScaleFactor(
      ComputeDisplayDPI(current_mode->size(), snapshot.physical_size()));
  // Get DIP size based on device scale factor. We are assuming the
  // ui scale factor is always 1.0 here for now.
  gfx::Size scaled_size = gfx::ScaleToRoundedSize(
      current_mode->size(), 1.0f / metrics.device_scale_factor);
  metrics.bounds = gfx::Rect(origin, scaled_size);
  metrics.work_area = metrics.bounds;
  return metrics;
}

void PlatformScreenOzone::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& displays) {
  ProcessRemovedDisplays(displays);
  ProcessModifiedDisplays(displays);

  // If the primary display is marked as removed we'll try to find a new primary
  // display and update the delegate before removing the old primary display.
  if (primary_display_id_ == Display::kInvalidDisplayID) {
    for (const DisplayInfo& display : cached_displays_) {
      if (!display.removed) {
        primary_display_id_ = display.id;
        delegate_->OnPrimaryDisplayChanged(primary_display_id_);
        break;
      }
    }
  }

  UpdateCachedDisplays();
  AddNewDisplays(displays);

  wait_for_display_config_update_ = false;
}

void PlatformScreenOzone::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    ui::MultipleDisplayState failed_new_state) {
  LOG(ERROR) << "OnDisplayModeChangeFailed from DisplayConfigurator";
  wait_for_display_config_update_ = false;
}

void PlatformScreenOzone::Create(
    const service_manager::Identity& remote_identity,
    mojom::DisplayControllerRequest request) {
  controller_bindings_.AddBinding(this, std::move(request));
}

ui::MultipleDisplayState PlatformScreenOzone::GetStateForDisplayIds(
    const ui::DisplayConfigurator::DisplayStateList& display_states) const {
  return (display_states.size() == 1
              ? ui::MULTIPLE_DISPLAY_STATE_SINGLE
              : ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED);
}

bool PlatformScreenOzone::GetResolutionForDisplayId(int64_t display_id,
                                                    gfx::Size* size) const {
  for (const DisplayInfo& display : cached_displays_) {
    if (display.id == display_id) {
      *size = display.requested_size;
      return true;
    }
  }

  return false;
}

void PlatformScreenOzone::Create(
    const service_manager::Identity& remote_identity,
    mojom::TestDisplayControllerRequest request) {
  test_bindings_.AddBinding(this, std::move(request));
}

}  // namespace display
