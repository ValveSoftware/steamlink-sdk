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
  auto storage_manager_instance = GetStorageManagerInstance();
  if (!storage_manager_instance) {
    return false;
  }
  storage_manager_instance->OpenPrivateVolumeSettings();
  return true;
}

bool ArcStorageManager::GetApplicationsSize(
    const GetApplicationsSizeCallback& callback) {
  auto storage_manager_instance = GetStorageManagerInstance();
  if (!storage_manager_instance) {
    return false;
  }
  storage_manager_instance->GetApplicationsSize(callback);
  return true;
}

bool ArcStorageManager::DeleteApplicationsCache(
    const base::Callback<void()>& callback) {
  auto storage_manager_instance = GetStorageManagerInstance();
  if (!storage_manager_instance) {
    return false;
  }
  storage_manager_instance->DeleteApplicationsCache(callback);
  return true;
}

mojom::StorageManagerInstance* ArcStorageManager::GetStorageManagerInstance() {
  auto bridge_service = arc_bridge_service();
  auto storage_manager_instance = bridge_service->storage_manager()->instance();
  if (!storage_manager_instance) {
    DLOG(WARNING) << "ARC storage manager instance is not ready.";
    return nullptr;
  }
  auto storage_manager_version = bridge_service->storage_manager()->version();
  if (storage_manager_version < kMinInstanceVersion) {
    DLOG(ERROR) << "ARC storage manager instance (version "
                << storage_manager_version << ") is too old.";
    return nullptr;
  }
  return storage_manager_instance;
}

}  // namespace arc
