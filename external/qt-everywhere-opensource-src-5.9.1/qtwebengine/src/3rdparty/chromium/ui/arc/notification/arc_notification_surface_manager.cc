// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_surface_manager.h"

#include <algorithm>

#include "components/exo/notification_surface.h"

namespace arc {

namespace {

ArcNotificationSurfaceManager* instance = nullptr;

}  // namespace

ArcNotificationSurfaceManager::ArcNotificationSurfaceManager() {
  DCHECK(!instance);
  instance = this;
}

ArcNotificationSurfaceManager::~ArcNotificationSurfaceManager() {
  DCHECK_EQ(this, instance);
  instance = nullptr;
}

// static
ArcNotificationSurfaceManager* ArcNotificationSurfaceManager::Get() {
  return instance;
}

exo::NotificationSurface* ArcNotificationSurfaceManager::GetSurface(
    const std::string& notification_key) const {
  auto it = notification_surface_map_.find(notification_key);
  return it == notification_surface_map_.end() ? nullptr : it->second;
}

void ArcNotificationSurfaceManager::AddSurface(
    exo::NotificationSurface* surface) {
  if (notification_surface_map_.find(surface->notification_id()) !=
      notification_surface_map_.end()) {
    NOTREACHED();
    return;
  }

  notification_surface_map_[surface->notification_id()] = surface;

  for (auto& observer : observers_)
    observer.OnNotificationSurfaceAdded(surface);
}

void ArcNotificationSurfaceManager::RemoveSurface(
    exo::NotificationSurface* surface) {
  auto it = notification_surface_map_.find(surface->notification_id());
  if (it == notification_surface_map_.end())
    return;

  notification_surface_map_.erase(it);
  for (auto& observer : observers_)
    observer.OnNotificationSurfaceRemoved(surface);
}

void ArcNotificationSurfaceManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcNotificationSurfaceManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace arc
