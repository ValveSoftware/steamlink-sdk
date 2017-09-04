// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_NOTIFICATION_SURFACE_H_
#define COMPONENTS_EXO_NOTIFICATION_SURFACE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}

namespace exo {
class NotificationSurfaceManager;
class Surface;

// Handles notification surface role of a given surface.
class NotificationSurface : public SurfaceDelegate, public SurfaceObserver {
 public:
  NotificationSurface(NotificationSurfaceManager* manager,
                      Surface* surface,
                      const std::string& notification_id);
  ~NotificationSurface() override;

  gfx::Size GetSize() const;

  aura::Window* window() { return window_.get(); }
  const std::string& notification_id() const { return notification_id_; }

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  NotificationSurfaceManager* const manager_;  // Not owned.
  Surface* surface_;                           // Not owned.
  const std::string notification_id_;

  std::unique_ptr<aura::Window> window_;
  bool added_to_manager_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_NOTIFICATION_SURFACE_H_
