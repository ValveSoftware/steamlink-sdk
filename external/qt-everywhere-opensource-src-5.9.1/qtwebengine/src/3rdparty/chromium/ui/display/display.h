// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_H_
#define UI_DISPLAY_DISPLAY_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ui/display/display_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/icc_profile.h"

#if !defined(OS_IOS)
#include "mojo/public/cpp/bindings/struct_traits.h"  // nogncheck
#endif

namespace display {

#if !defined(OS_IOS)
namespace mojom {
class DisplayDataView;
}
#endif

// This class typically, but does not always, correspond to a physical display
// connected to the system. A fake Display may exist on a headless system, or a
// Display may correspond to a remote, virtual display.
//
// Note: The screen and display currently uses pixel coordinate
// system. For platforms that support DIP (density independent pixel),
// |bounds()| and |work_area| will return values in DIP coordinate
// system, not in backing pixels.
class DISPLAY_EXPORT Display final {
 public:
  // Screen Rotation in clock-wise degrees.
  // This enum corresponds to DisplayRotationDefaultProto::Rotation in
  // chrome/browser/chromeos/policy/proto/chrome_device_policy.proto.
  enum Rotation {
    ROTATE_0 = 0,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
  };

  // The display rotation can have multiple causes for change. A user can set a
  // preference. On devices with accelerometers, they can change the rotation.
  // RotationSource allows for the tracking of a Rotation per source of the
  // change. ROTATION_SOURCE_ACTIVE is the current rotation of the display.
  // Rotation changes not due to an accelerometer, nor the user, are to use this
  // source directly. ROTATION_SOURCE_UNKNOWN is when no rotation source has
  // been provided.
  enum RotationSource {
    ROTATION_SOURCE_ACCELEROMETER = 0,
    ROTATION_SOURCE_ACTIVE,
    ROTATION_SOURCE_USER,
    ROTATION_SOURCE_UNKNOWN,
  };

  // Touch support for the display.
  enum TouchSupport {
    TOUCH_SUPPORT_UNKNOWN,
    TOUCH_SUPPORT_AVAILABLE,
    TOUCH_SUPPORT_UNAVAILABLE,
  };

  enum : int64_t { kInvalidDisplayID = -1 };

  // Creates a display with kInvalidDisplayID as default.
  Display();
  explicit Display(int64_t id);
  Display(int64_t id, const gfx::Rect& bounds);
  Display(const Display& other);
  ~Display();

  // Returns the forced device scale factor, which is given by
  // "--force-device-scale-factor".
  static float GetForcedDeviceScaleFactor();

  // Indicates if a device scale factor is being explicitly enforced from the
  // command line via "--force-device-scale-factor".
  static bool HasForceDeviceScaleFactor();

  // Resets the caches used to determine if a device scale factor is being
  // forced from the command line via "--force-device-scale-factor", and thus
  // ensures that the command line is reevaluated.
  static void ResetForceDeviceScaleFactorForTesting();

  // Sets/Gets unique identifier associated with the display.
  // -1 means invalid display and it doesn't not exit.
  int64_t id() const { return id_; }
  void set_id(int64_t id) { id_ = id; }

  // Gets/Sets the display's bounds in display::Screen's coordinates.
  const gfx::Rect& bounds() const { return bounds_; }
  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  // Gets/Sets the display's work area in display::Screen's coordinates.
  const gfx::Rect& work_area() const { return work_area_; }
  void set_work_area(const gfx::Rect& work_area) { work_area_ = work_area; }

  // Output device's pixel scale factor. This specifies how much the
  // UI should be scaled when the actual output has more pixels than
  // standard displays (which is around 100~120dpi.) The potential return
  // values depend on each platforms.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  Rotation rotation() const { return rotation_; }
  void set_rotation(Rotation rotation) { rotation_ = rotation; }
  int RotationAsDegree() const;
  void SetRotationAsDegree(int rotation);

  TouchSupport touch_support() const { return touch_support_; }
  void set_touch_support(TouchSupport support) { touch_support_ = support; }

  // Utility functions that just return the size of display and
  // work area.
  const gfx::Size& size() const { return bounds_.size(); }
  const gfx::Size& work_area_size() const { return work_area_.size(); }

  // Returns the work area insets.
  gfx::Insets GetWorkAreaInsets() const;

  // Sets the device scale factor and display bounds in pixel. This
  // updates the work are using the same insets between old bounds and
  // work area.
  void SetScaleAndBounds(float device_scale_factor,
                         const gfx::Rect& bounds_in_pixel);

  // Sets the display's size. This updates the work area using the same insets
  // between old bounds and work area.
  void SetSize(const gfx::Size& size_in_pixel);

  // Computes and updates the display's work are using
  // |work_area_insets| and the bounds.
  void UpdateWorkAreaFromInsets(const gfx::Insets& work_area_insets);

  // Returns the display's size in pixel coordinates.
  gfx::Size GetSizeInPixel() const;

  // Returns a string representation of the display;
  std::string ToString() const;

  // True if the display contains valid display id.
  bool is_valid() const { return id_ != kInvalidDisplayID; }

  // True if the display corresponds to internal panel.
  bool IsInternal() const;

  // Gets/Sets an id of display corresponding to internal panel.
  static int64_t InternalDisplayId();
  static void SetInternalDisplayId(int64_t internal_display_id);

  // Test if the |id| is for the internal display if any.
  static bool IsInternalDisplayId(int64_t id);

  // True if there is an internal display.
  static bool HasInternalDisplay();

  // Maximum cursor size in native pixels.
  const gfx::Size& maximum_cursor_size() const { return maximum_cursor_size_; }
  void set_maximum_cursor_size(const gfx::Size& size) {
    maximum_cursor_size_ = size;
  }

  // The full ICC profile of the display.
  gfx::ICCProfile icc_profile() const { return icc_profile_; }
  void set_icc_profile(const gfx::ICCProfile& icc_profile) {
    icc_profile_ = icc_profile;
  }

  // The number of bits per pixel. Used by media query APIs.
  int color_depth() const { return color_depth_; }
  void set_color_depth(int color_depth) {
    color_depth_ = color_depth;
  }

  // The number of bits per color component (all color components are assumed to
  // have the same number of bits). Used by media query APIs.
  int depth_per_component() const { return depth_per_component_; }
  void set_depth_per_component(int depth_per_component) {
    depth_per_component_ = depth_per_component;
  }

  // True if this is a monochrome display (e.g, for accessiblity). Used by media
  // query APIs.
  bool is_monochrome() const { return is_monochrome_; }
  void set_is_monochrome(bool is_monochrome) {
    is_monochrome_ = is_monochrome;
  }

 private:
  int64_t id_;
  gfx::Rect bounds_;
  gfx::Rect work_area_;
  float device_scale_factor_;
  Rotation rotation_ = ROTATE_0;
  TouchSupport touch_support_ = TOUCH_SUPPORT_UNKNOWN;
  gfx::Size maximum_cursor_size_;
  gfx::ICCProfile icc_profile_;
  int color_depth_;
  int depth_per_component_;
  bool is_monochrome_ = false;

#if !defined(OS_IOS)
  friend struct mojo::StructTraits<display::mojom::DisplayDataView,
                                   display::Display>;
#endif
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_H_
