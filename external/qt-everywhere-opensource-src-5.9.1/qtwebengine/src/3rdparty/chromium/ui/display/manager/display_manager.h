// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_MANAGER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_layout.h"
#include "ui/display/manager/managed_display_info.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#endif

namespace gfx {
class Insets;
class Rect;
}

namespace display {
using DisplayInfoList = std::vector<ManagedDisplayInfo>;

class DisplayLayoutStore;
class DisplayObserver;
class Screen;

namespace test {
class DisplayManagerTestApi;
}

// DisplayManager maintains the current display configurations,
// and notifies observers when configuration changes.
class DISPLAY_EXPORT DisplayManager
#if defined(OS_CHROMEOS)
    : public ui::DisplayConfigurator::SoftwareMirroringController
#endif
{
 public:
  class DISPLAY_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    // Create or updates the mirroring window with |display_info_list|.
    virtual void CreateOrUpdateMirroringDisplay(
        const DisplayInfoList& display_info_list) = 0;

    // Closes the mirror window if not necessary.
    virtual void CloseMirroringDisplayIfNotNecessary() = 0;

    // Called before and after the display configuration changes.
    // When |clear_focus| is true, the implementation should
    // deactivate the active window and set the focus window to NULL.
    virtual void PreDisplayConfigurationChange(bool clear_focus) = 0;
    virtual void PostDisplayConfigurationChange(bool must_clear_window) = 0;

#if defined(OS_CHROMEOS)
    // Get the ui::DisplayConfigurator.
    virtual ui::DisplayConfigurator* display_configurator() = 0;
#endif

    virtual std::string GetInternalDisplayNameString() = 0;
    virtual std::string GetUnknownDisplayNameString() = 0;
  };

  // How the second display will be used.
  // 1) EXTENDED mode extends the desktop to the second dislpay.
  // 2) MIRRORING mode copies the content of the primary display to
  //    the 2nd display. (Software Mirroring).
  // 3) UNIFIED mode creates single desktop across multiple displays.
  enum MultiDisplayMode {
    EXTENDED = 0,
    MIRRORING,
    UNIFIED,
  };

  // The display ID for a virtual display assigned to a unified desktop.
  static int64_t kUnifiedDisplayId;

  explicit DisplayManager(std::unique_ptr<Screen> screen);
#if defined(OS_CHROMEOS)
  ~DisplayManager() override;
#else
  virtual ~DisplayManager();
#endif

  DisplayLayoutStore* layout_store() { return layout_store_.get(); }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // When set to true, the DisplayManager calls OnDisplayMetricsChanged
  // even if the display's bounds didn't change. Used to swap primary
  // display.
  void set_force_bounds_changed(bool force_bounds_changed) {
    force_bounds_changed_ = force_bounds_changed;
  }

  // Returns the display id of the first display in the outupt list.
  int64_t first_display_id() const { return first_display_id_; }

  // Initializes displays using command line flag. Returns false
  // if no command line flag was provided.
  bool InitFromCommandLine();

  // Initialize default display.
  void InitDefaultDisplay();

  // Initializes font related params that depends on display
  // configuration.
  void RefreshFontParams();

  // Returns the display layout used for current displays.
  const display::DisplayLayout& GetCurrentDisplayLayout() const;

  // Returns the current display list.
  display::DisplayIdList GetCurrentDisplayIdList() const;

  // Sets the layout for the current display pair. The |layout| specifies
  // the locaion of the displays relative to their parents.
  void SetLayoutForCurrentDisplays(
      std::unique_ptr<display::DisplayLayout> layout);

  // Returns display for given |id|;
  const display::Display& GetDisplayForId(int64_t id) const;

  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  const display::Display& FindDisplayContainingPoint(
      const gfx::Point& point_in_screen) const;

  // Sets the work area's |insets| to the display given by |display_id|.
  bool UpdateWorkAreaOfDisplay(int64_t display_id, const gfx::Insets& insets);

  // Registers the overscan insets for the display of the specified ID. Note
  // that the insets size should be specified in DIP size. It also triggers the
  // display's bounds change.
  void SetOverscanInsets(int64_t display_id, const gfx::Insets& insets_in_dip);

  // Sets the display's rotation for the given |source|. The new |rotation| will
  // also become active.
  void SetDisplayRotation(int64_t display_id,
                          display::Display::Rotation rotation,
                          display::Display::RotationSource source);

  // Resets the UI scale of the display with |display_id| to the one defined in
  // the default mode.
  bool ResetDisplayToDefaultMode(int64_t display_id);

  // Sets the external display's configuration, including resolution change,
  // ui-scale change, and device scale factor change. Returns true if it changes
  // the display resolution so that the caller needs to show a notification in
  // case the new resolution actually doesn't work.
  bool SetDisplayMode(
      int64_t display_id,
      const scoped_refptr<display::ManagedDisplayMode>& display_mode);

  // Register per display properties. |overscan_insets| is NULL if
  // the display has no custom overscan insets.
  void RegisterDisplayProperty(int64_t display_id,
                               display::Display::Rotation rotation,
                               float ui_scale,
                               const gfx::Insets* overscan_insets,
                               const gfx::Size& resolution_in_pixels,
                               float device_scale_factor,
                               ui::ColorCalibrationProfile color_profile);

  // Register stored rotation properties for the internal display.
  void RegisterDisplayRotationProperties(bool rotation_lock,
                                         display::Display::Rotation rotation);

  // Returns the stored rotation lock preference if it has been loaded,
  // otherwise false.
  bool registered_internal_display_rotation_lock() const {
    return registered_internal_display_rotation_lock_;
  }

  // Returns the stored rotation preference for the internal display if it has
  // been loaded, otherwise |display::Display::Rotate_0|.
  display::Display::Rotation registered_internal_display_rotation() const {
    return registered_internal_display_rotation_;
  }

  // Returns the display mode of |display_id| which is currently used.
  scoped_refptr<display::ManagedDisplayMode> GetActiveModeForDisplayId(
      int64_t display_id) const;

  // Returns the display's selected mode. This returns false and doesn't
  // set |mode_out| if the display mode is in default.
  scoped_refptr<display::ManagedDisplayMode> GetSelectedModeForDisplayId(
      int64_t display_id) const;

  // Tells if the virtual resolution feature is enabled.
  bool IsDisplayUIScalingEnabled() const;

  // Returns the current overscan insets for the specified |display_id|.
  // Returns an empty insets (0, 0, 0, 0) if no insets are specified for
  // the display.
  gfx::Insets GetOverscanInsets(int64_t display_id) const;

  // Sets the color calibration of the display to |profile|.
  void SetColorCalibrationProfile(int64_t display_id,
                                  ui::ColorCalibrationProfile profile);

  // Called when display configuration has changed. The new display
  // configurations is passed as a vector of Display object, which
  // contains each display's new infomration.
  void OnNativeDisplaysChanged(
      const std::vector<display::ManagedDisplayInfo>& display_info_list);

  // Updates the internal display data and notifies observers about the changes.
  void UpdateDisplaysWith(
      const std::vector<display::ManagedDisplayInfo>& display_info_list);

  // Updates current displays using current |display_info_|.
  void UpdateDisplays();

  // Returns the display at |index|. The display at 0 is
  // no longer considered "primary".
  const display::Display& GetDisplayAt(size_t index) const;

  const display::Display& GetPrimaryDisplayCandidate() const;

  // Returns the logical number of displays. This returns 1
  // when displays are mirrored.
  size_t GetNumDisplays() const;

  // Returns only the currently active displays. This list does not include the
  // displays that will be removed if |UpdateDisplaysWith| is currently
  // executing.
  // See https://crbug.com/632755
  const display::Displays& active_only_display_list() const {
    return is_updating_display_list_ ? active_only_display_list_
                                     : active_display_list();
  }

  const display::Displays& active_display_list() const {
    return active_display_list_;
  }

  // Returns true if the display specified by |display_id| is currently
  // connected and active. (mirroring display isn't active, for example).
  bool IsActiveDisplayId(int64_t display_id) const;

  // Returns the number of connected displays. This returns 2
  // when displays are mirrored.
  size_t num_connected_displays() const { return num_connected_displays_; }

  // Returns the mirroring status.
  bool IsInMirrorMode() const;
  int64_t mirroring_display_id() const { return mirroring_display_id_; }
  const display::Displays& software_mirroring_display_list() const {
    return software_mirroring_display_list_;
  }

  // Sets/gets if the unified desktop feature is enabled.
  void SetUnifiedDesktopEnabled(bool enabled);
  bool unified_desktop_enabled() const { return unified_desktop_enabled_; }

  // Returns true if it's in unified desktop mode.
  bool IsInUnifiedMode() const;

  // Returns the display used for software mirrroring. Returns invalid
  // display if not found.
  const display::Display GetMirroringDisplayById(int64_t id) const;

  // Retuns the display info associated with |display_id|.
  const display::ManagedDisplayInfo& GetDisplayInfo(int64_t display_id) const;

  // Returns the human-readable name for the display |id|.
  std::string GetDisplayNameForId(int64_t id);

  // Returns the display id that is capable of UI scaling. On device,
  // this returns internal display's ID if its device scale factor is 2,
  // or invalid ID if such internal display doesn't exist. On linux
  // desktop, this returns the first display ID.
  int64_t GetDisplayIdForUIScaling() const;

  // Change the mirror mode.
  void SetMirrorMode(bool mirrored);

  // Used to emulate display change when run in a desktop environment instead
  // of on a device.
  void AddRemoveDisplay();
  void ToggleDisplayScaleFactor();

// SoftwareMirroringController override:
#if defined(OS_CHROMEOS)
  void SetSoftwareMirroring(bool enabled) override;
  bool SoftwareMirroringEnabled() const override;
#endif

  // Sets/gets default multi display mode.
  void SetDefaultMultiDisplayModeForCurrentDisplays(MultiDisplayMode mode);
  MultiDisplayMode current_default_multi_display_mode() const {
    return current_default_multi_display_mode_;
  }

  // Sets multi display mode.
  void SetMultiDisplayMode(MultiDisplayMode mode);

  // Reconfigure display configuration using the same
  // physical display. TODO(oshima): Refactor and move this
  // impl to |SetDefaultMultiDisplayMode|.
  void ReconfigureDisplays();

  // Update the bounds of the display given by |display_id|.
  bool UpdateDisplayBounds(int64_t display_id, const gfx::Rect& new_bounds);

  // Creates mirror window asynchronously if the software mirror mode
  // is enabled.
  void CreateMirrorWindowAsyncIfAny();

  // A unit test may change the internal display id (which never happens on
  // a real device). This will update the mode list for internal display
  // for this test scenario.
  void UpdateInternalManagedDisplayModeListForTest();

  // Zoom the internal display.
  bool ZoomInternalDisplay(bool up);

  // Reset the internal display zoom.
  void ResetInternalDisplayZoom();

  // Notifies observers of display configuration changes.
  void NotifyMetricsChanged(const display::Display& display, uint32_t metrics);
  void NotifyDisplayAdded(const display::Display& display);
  void NotifyDisplayRemoved(const display::Display& display);

  // Delegated from the Screen implementation.
  void AddObserver(display::DisplayObserver* observer);
  void RemoveObserver(display::DisplayObserver* observer);

  // Returns a display::Display object for a secondary display if it exists
  // or returns invalid display if there is no secondary display.
  // TODO(rjkroege): Display swapping is an obsolete feature pre-dating
  // multi-display support so remove it.
  const display::Display& GetSecondaryDisplay() const;

 private:
  friend class test::DisplayManagerTestApi;

  bool software_mirroring_enabled() const {
    return multi_display_mode_ == MIRRORING;
  };

  void set_change_display_upon_host_resize(bool value) {
    change_display_upon_host_resize_ = value;
  }

  // Creates software mirroring display related information. The display
  // used to mirror the content is removed from the |display_info_list|.
  void CreateSoftwareMirroringDisplayInfo(DisplayInfoList* display_info_list);

  display::Display* FindDisplayForId(int64_t id);

  // Add the mirror display's display info if the software based
  // mirroring is in use.
  void AddMirrorDisplayInfoIfAny(DisplayInfoList* display_info_list);

  // Inserts and update the display::ManagedDisplayInfo according to the
  // overscan
  // state. Note that The display::ManagedDisplayInfo stored in the
  // |internal_display_info_|
  // can be different from |new_info| (due to overscan state), so
  // you must use |GetDisplayInfo| to get the correct
  // display::ManagedDisplayInfo for
  // a display.
  void InsertAndUpdateDisplayInfo(const display::ManagedDisplayInfo& new_info);

  // Called when the display info is updated through InsertAndUpdateDisplayInfo.
  void OnDisplayInfoUpdated(const display::ManagedDisplayInfo& display_info);

  // Creates a display object from the display::ManagedDisplayInfo for
  // |display_id|.
  display::Display CreateDisplayFromDisplayInfoById(int64_t display_id);

  // Creates a display object from the display::ManagedDisplayInfo for
  // |display_id|
  // for
  // mirroring. The size of the display will be scaled using |scale|
  // with the offset using |origin|.
  display::Display CreateMirroringDisplayFromDisplayInfoById(
      int64_t display_id,
      const gfx::Point& origin,
      float scale);

  // Updates the bounds of all non-primary displays in |display_list| and
  // append the indices of displays updated to |updated_indices|.
  // When the size of |display_list| equals 2, the bounds are updated using
  // the layout registered for the display pair. For more than 2 displays,
  // the bounds are updated using horizontal layout.
  void UpdateNonPrimaryDisplayBoundsForLayout(
      display::Displays* display_list,
      std::vector<size_t>* updated_indices);

  void CreateMirrorWindowIfAny();

  void RunPendingTasksForTest();

  // Applies the |layout| and updates the bounds of displays in |display_list|.
  // |updated_ids| contains the ids for displays whose bounds have changed.
  void ApplyDisplayLayout(const display::DisplayLayout& layout,
                          display::Displays* display_list,
                          std::vector<int64_t>* updated_ids);

  Delegate* delegate_ = nullptr;  // not owned.

  std::unique_ptr<Screen> screen_;

  std::unique_ptr<DisplayLayoutStore> layout_store_;

  int64_t first_display_id_ = Display::kInvalidDisplayID;

  // List of current active displays.
  Displays active_display_list_;
  // This list does not include the displays that will be removed if
  // |UpdateDisplaysWith| is under execution.
  // See https://crbug.com/632755
  Displays active_only_display_list_;

  // True if active_display_list is being modified and has displays that are not
  // presently active.
  // See https://crbug.com/632755
  bool is_updating_display_list_ = false;

  size_t num_connected_displays_ = 0;

  bool force_bounds_changed_ = false;

  // The mapping from the display ID to its internal data.
  std::map<int64_t, ManagedDisplayInfo> display_info_;

  // Selected display modes for displays. Key is the displays' ID.
  std::map<int64_t, scoped_refptr<ManagedDisplayMode>> display_modes_;

  // When set to true, the host window's resize event updates
  // the display's size. This is set to true when running on
  // desktop environment (for debugging) so that resizing the host
  // window will update the display properly. This is set to false
  // on device as well as during the unit tests.
  bool change_display_upon_host_resize_ = false;

  MultiDisplayMode multi_display_mode_ = EXTENDED;
  MultiDisplayMode current_default_multi_display_mode_ = EXTENDED;

  int64_t mirroring_display_id_ = Display::kInvalidDisplayID;
  Displays software_mirroring_display_list_;

  // User preference for rotation lock of the internal display.
  bool registered_internal_display_rotation_lock_ = false;

  // User preference for the rotation of the internal display.
  Display::Rotation registered_internal_display_rotation_ = Display::ROTATE_0;

  bool unified_desktop_enabled_ = false;

  base::ObserverList<DisplayObserver> observers_;

  base::WeakPtrFactory<DisplayManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_MANAGER_H_
