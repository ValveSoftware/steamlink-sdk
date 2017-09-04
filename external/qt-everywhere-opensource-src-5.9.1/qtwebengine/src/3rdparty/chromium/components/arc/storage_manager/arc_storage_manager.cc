// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/storage_manager/arc_storage_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

namespace {

const int kMinInstanceVersion = 1;  // See storage_manager.mojom.

// This class is owned by ArcServiceManager so that it is safe to use this raw
// pointer as the singleton reference.
ArcStorageManager* g_arc_storage_manager = nullptr;

}  // namespace

ArcStorageManager::ArcStorageManager(ArcBridgeService* bridge_service)
    : ArcService(bridge_service) {
  DCHECK(!g_arc_storage_manager);
  g_arc_storage_manager = this;
}

ArcStorageManager::~ArcStorageManager() {
  DCHECK_EQ(this, g_arc_storage_manager);
  g_arc_storage_manager = nullptr;
}

// static
ArcStorageManager* ArcStorageManager::Get() {
  DCHECK(g_arc_storage_manager);
  return g_arc_storage_manager;
}

bool ArcStorageManager::OpenPrivateVolumeSettings() {
  auto* storage_manager_instance =
      arc_bridge_service()->storage_manager()->GetInstanceForMethod(
          "OpenPrivateVolumeSettings", kMinInstanceVersion);
  if (!storage_manager_instance)
    return false;
  storage_manager_instance->OpenPrivateVolumeSettings();
  return true;
}

bool ArcStorageManager::GetApplicationsSize(
    const GetApplicationsSizeCallback& callback) {
  auto* storage_manager_instance =
      arc_bridge_service()->storage_manager()->GetInstanceForMethod(
          "GetApplicationsSize", kMinInstanceVersion);
  if (!storage_manager_instance)
    return false;
  storage_manager_instance->GetApplicationsSize(callback);
  return true;
}

bool ArcStorageManager::DeleteApplicationsCache(
    const base::Callback<void()>& callback) {
  auto* storage_manager_instance =
      arc_bridge_service()->storage_manager()->GetInstanceForMethod(
          "DeleteApplicationsCache", kMinInstanceVersion);
  if (!storage_manager_instance)
    return false;
  storage_manager_instance->DeleteApplicationsCache(callback);
  return true;
}

}  // namespace arc
