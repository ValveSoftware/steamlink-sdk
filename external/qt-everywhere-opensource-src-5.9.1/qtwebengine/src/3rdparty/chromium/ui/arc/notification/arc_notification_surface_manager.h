// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/exo/notification_surface_manager.h"

namespace arc {

// Keeps track of NotificationSurface.
class ArcNotificationSurfaceManager : public exo::NotificationSurfaceManager {
 public:
  class Observer {
   public:
    // Invoked when a notification surface is added to the registry.
    virtual void OnNotificationSurfaceAdded(
        exo::NotificationSurface* surface) = 0;

    // Invoked when a notification surface is removed from the registry.
    virtual void OnNotificationSurfaceRemoved(
        exo::NotificationSurface* surface) = 0;

   protected:
    virtual ~Observer() = default;
  };

  ArcNotificationSurfaceManager();
  ~ArcNotificationSurfaceManager() override;

  static ArcNotificationSurfaceManager* Get();

  // exo::NotificationSurfaceManager:
  exo::NotificationSurface* GetSurface(
      const std::string& notification_id) const override;
  void AddSurface(exo::NotificationSurface* surface) override;
  void RemoveSurface(exo::NotificationSurface* surface) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  using NotificationSurfaceMap =
      std::map<std::string, exo::NotificationSurface*>;
  NotificationSurfaceMap notification_surface_map_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationSurfaceManager);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_
