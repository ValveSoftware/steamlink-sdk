// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/screen_manager.h"

#include <xf86drmMode.h>

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/scanout_surface.h"

namespace ui {

ScreenManager::ScreenManager(
    DriWrapper* dri, ScanoutSurfaceGenerator* surface_generator)
    : dri_(dri), surface_generator_(surface_generator), last_added_widget_(0) {
}

ScreenManager::~ScreenManager() {
  STLDeleteContainerPairSecondPointers(
      controllers_.begin(), controllers_.end());
}

void ScreenManager::RemoveDisplayController(uint32_t crtc, uint32_t connector) {
  HardwareDisplayControllerMap::iterator it =
      FindDisplayController(crtc, connector);
  if (it != controllers_.end()) {
    delete it->second;
    controllers_.erase(it);
  }
}

bool ScreenManager::ConfigureDisplayController(uint32_t crtc,
                                               uint32_t connector,
                                               const drmModeModeInfo& mode) {
  HardwareDisplayControllerMap::iterator it =
      FindDisplayController(crtc, connector);
  HardwareDisplayController* controller = NULL;
  if (it != controllers_.end()) {
    if (SameMode(mode, it->second->get_mode()))
      return it->second->Enable();

    controller = it->second;
    controller->UnbindSurfaceFromController();
  }

  if (it == controllers_.end()) {
    controller = new HardwareDisplayController(dri_, connector, crtc);
    controllers_.insert(std::make_pair(++last_added_widget_, controller));
  }

  // Create a surface suitable for the current controller.
  scoped_ptr<ScanoutSurface> surface(
      surface_generator_->Create(gfx::Size(mode.hdisplay, mode.vdisplay)));

  if (!surface->Initialize()) {
    LOG(ERROR) << "Failed to initialize surface";
    return false;
  }

  // Bind the surface to the controller. This will register the backing buffers
  // with the hardware CRTC such that we can show the buffers and performs the
  // initial modeset. The controller takes ownership of the surface.
  if (!controller->BindSurfaceToController(surface.Pass(), mode)) {
    LOG(ERROR) << "Failed to bind surface to controller";
    return false;
  }

  return true;
}

bool ScreenManager::DisableDisplayController(uint32_t crtc,
                                             uint32_t connector) {
  HardwareDisplayControllerMap::iterator it =
      FindDisplayController(crtc, connector);
  if (it != controllers_.end()) {
    it->second->Disable();
    return true;
  }

  return false;
}

base::WeakPtr<HardwareDisplayController> ScreenManager::GetDisplayController(
    gfx::AcceleratedWidget widget) {
  // TODO(dnicoara): Remove hack once TestScreen uses a simple Ozone display
  // configuration reader and ScreenManager is called from there to create the
  // one display needed by the content_shell target.
  if (controllers_.empty() && last_added_widget_ == 0)
    ForceInitializationOfPrimaryDisplay();

  HardwareDisplayControllerMap::iterator it = controllers_.find(widget);
  if (it != controllers_.end())
    return it->second->AsWeakPtr();

  return base::WeakPtr<HardwareDisplayController>();
}

ScreenManager::HardwareDisplayControllerMap::iterator
ScreenManager::FindDisplayController(uint32_t crtc, uint32_t connector) {
  for (HardwareDisplayControllerMap::iterator it = controllers_.begin();
       it != controllers_.end();
       ++it) {
    if (it->second->connector_id() == connector &&
        it->second->crtc_id() == crtc)
      return it;
  }

  return controllers_.end();
}

void ScreenManager::ForceInitializationOfPrimaryDisplay() {
  drmModeRes* resources = drmModeGetResources(dri_->get_fd());
  DCHECK(resources) << "Failed to get DRM resources";
  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(dri_->get_fd(), resources);
  drmModeFreeResources(resources);

  CHECK_NE(0u, displays.size());

  drmModePropertyRes* dpms =
      dri_->GetProperty(displays[0]->connector(), "DPMS");
  if (dpms)
    dri_->SetProperty(displays[0]->connector()->connector_id,
                      dpms->prop_id,
                      DRM_MODE_DPMS_ON);

  ConfigureDisplayController(displays[0]->crtc()->crtc_id,
                             displays[0]->connector()->connector_id,
                             displays[0]->connector()->modes[0]);
}

}  // namespace ui
