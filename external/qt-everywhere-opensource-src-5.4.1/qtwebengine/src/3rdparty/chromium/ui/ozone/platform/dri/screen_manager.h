// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_SCREEN_MANAGER_H_
#define UI_OZONE_PLATFORM_DRI_SCREEN_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace ui {

class DriWrapper;
class ScanoutSurfaceGenerator;

// Responsible for keeping track of active displays and configuring them.
class OZONE_EXPORT ScreenManager {
 public:
  ScreenManager(DriWrapper* dri, ScanoutSurfaceGenerator* surface_generator);
  virtual ~ScreenManager();

  // Remove a display controller from the list of active controllers. The
  // controller is removed since it was disconnected.
  void RemoveDisplayController(uint32_t crtc, uint32_t connector);

  // Configure (and add if not present) a display controller. The display
  // controller is identified by (|crtc|, |connector|) and the controller is
  // modeset using |mode|.
  bool ConfigureDisplayController(uint32_t crtc,
                                  uint32_t connector,
                                  const drmModeModeInfo& mode);

  // Disable the display controller identified by (|crtc|, |connector|). Note,
  // the controller may still be connected, so this does not remove the
  // controller.
  bool DisableDisplayController(uint32_t crtc, uint32_t connector);

  // Returns a reference to the display controller associated with |widget|.
  // This returns a weak reference since the display controller may be destroyed
  // at any point in time, but the changes are propagated to the compositor much
  // later (Compositor owns SurfaceOzone*, which is responsible for updating the
  // display surface).
  base::WeakPtr<HardwareDisplayController> GetDisplayController(
      gfx::AcceleratedWidget widget);

 private:
  typedef std::map<gfx::AcceleratedWidget, HardwareDisplayController*>
      HardwareDisplayControllerMap;

  // Returns an iterator into |controllers_| for the controller identified by
  // (|crtc|, |connector|).
  HardwareDisplayControllerMap::iterator FindDisplayController(
      uint32_t crtc, uint32_t connector);

  // On non CrOS builds there is no display configurator to look-up available
  // displays and initialize the HDCs. In such cases this is called internally
  // to initialize a display.
  virtual void ForceInitializationOfPrimaryDisplay();

  DriWrapper* dri_;  // Not owned.
  ScanoutSurfaceGenerator* surface_generator_;  // Not owned.
  // Mapping between an accelerated widget and an active display.
  HardwareDisplayControllerMap controllers_;
  gfx::AcceleratedWidget last_added_widget_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_SCREEN_MANAGER_H_
