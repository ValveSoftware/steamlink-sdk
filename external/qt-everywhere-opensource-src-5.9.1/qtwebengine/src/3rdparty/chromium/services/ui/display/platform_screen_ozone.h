// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_OZONE_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_OZONE_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/display/viewport_metrics.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"
#include "services/ui/public/interfaces/display/test_display_controller.mojom.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/display.h"
#include "ui/display/types/fake_display_controller.h"

namespace display {

// PlatformScreenOzone provides the necessary functionality to configure all
// attached physical displays on the ozone platform.
class PlatformScreenOzone
    : public PlatformScreen,
      public ui::DisplayConfigurator::Observer,
      public ui::DisplayConfigurator::StateController,
      public service_manager::InterfaceFactory<mojom::DisplayController>,
      public service_manager::InterfaceFactory<mojom::TestDisplayController>,
      public mojom::DisplayController,
      public mojom::TestDisplayController {
 public:
  PlatformScreenOzone();
  ~PlatformScreenOzone() override;

  // PlatformScreen:
  void AddInterfaces(service_manager::InterfaceRegistry* registry) override;
  void Init(PlatformScreenDelegate* delegate) override;
  void RequestCloseDisplay(int64_t display_id) override;
  int64_t GetPrimaryDisplayId() const override;

  // mojom::TestDisplayController:
  void ToggleAddRemoveDisplay() override;
  void ToggleDisplayResolution() override;

  // mojom::DisplayController:
  void SwapPrimaryDisplay() override;
  void SetDisplayWorkArea(int64_t display_id,
                          const gfx::Size& size,
                          const gfx::Insets& insets) override;

 private:
  friend class PlatformScreenOzoneTest;

  // TODO(kylechar): This struct is just temporary until we migrate
  // DisplayManager code out of ash so it can be used here.
  struct DisplayInfo {
    DisplayInfo();
    DisplayInfo(const DisplayInfo& other);
    ~DisplayInfo();

    int64_t id = Display::kInvalidDisplayID;
    // Information about display viewport.
    ViewportMetrics metrics;
    // Last insets received from WM.
    gfx::Insets last_work_area_insets;

    // Temporary hack to allow changing display resolution.
    std::vector<gfx::Size> supported_sizes;
    gfx::Size requested_size;

    // The display bounds have been modified and delegate should be updated.
    bool modified = false;
    // The display has been removed and delegate should be updated.
    bool removed = false;
  };
  using CachedDisplayIterator = std::vector<DisplayInfo>::iterator;

  // Processes list of display snapshots and sets |removed| on any displays that
  // have been removed. Updates |primary_display_id_| if the primary display was
  // removed. Does not remove displays from |cached_displays_| or send updates
  // to delegate.
  void ProcessRemovedDisplays(
      const ui::DisplayConfigurator::DisplayStateList& snapshots);

  // Processes list of display snapshots and updates the bounds of any displays
  // in |cached_displays_| that have changed size. Does not send updates to
  // delegate.
  void ProcessModifiedDisplays(
      const ui::DisplayConfigurator::DisplayStateList& snapshots);

  // Looks at |cached_displays_| for modified or removed displays. Also updates
  // display bounds in response to modified or removed displays. Sends updates
  // to the delegate when appropriate by calling OnDisplayModified() or
  // OnDisplayRemoved(). Makes at most one call to delegate per display.
  //
  // Usually used after ProcessRemovedDisplays() and ProcessModifiedDisplays().
  void UpdateCachedDisplays();

  // Processes list of display snapshots and adds any new displays to
  // |cached_displays_|. Updates delegate by calling OnDisplayAdded().
  void AddNewDisplays(
      const ui::DisplayConfigurator::DisplayStateList& snapshots);

  // Returns an iterator to the cached display with |display_id| or an end
  // iterator if there is no display with that id.
  CachedDisplayIterator GetCachedDisplayIterator(int64_t display_id);

  // Converts |snapshot| into ViewportMetrics.
  ViewportMetrics MetricsFromSnapshot(const ui::DisplaySnapshot& snapshot,
                                      const gfx::Point& origin);

  // ui::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& displays) override;
  void OnDisplayModeChangeFailed(
      const ui::DisplayConfigurator::DisplayStateList& displays,
      ui::MultipleDisplayState failed_new_state) override;

  // ui::DisplayConfigurator::StateController:
  ui::MultipleDisplayState GetStateForDisplayIds(
      const ui::DisplayConfigurator::DisplayStateList& display_states)
      const override;
  bool GetResolutionForDisplayId(int64_t display_id,
                                 gfx::Size* size) const override;

  // mojo::InterfaceFactory<mojom::DisplayController>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::DisplayControllerRequest request) override;

  // mojo::InterfaceFactory<mojom::TestDisplayController>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::TestDisplayControllerRequest request) override;

  ui::DisplayConfigurator display_configurator_;
  PlatformScreenDelegate* delegate_ = nullptr;

  // If not null it provides a way to modify the display state when running off
  // device (eg. running mustash on Linux).
  FakeDisplayController* fake_display_controller_ = nullptr;

  // Tracks if we've made a display configuration change and want to wait for
  // the display configuration to update before making further changes.
  bool wait_for_display_config_update_ = false;

  // TODO(kylechar): These values can/should be replaced by DisplayLayout.
  int64_t primary_display_id_ = display::Display::kInvalidDisplayID;
  std::vector<DisplayInfo> cached_displays_;
  gfx::Point next_display_origin_;

  mojo::BindingSet<mojom::DisplayController> controller_bindings_;
  mojo::BindingSet<mojom::TestDisplayController> test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenOzone);
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_OZONE_H_
