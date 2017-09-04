// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/sync_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace blimp {
namespace helium {
namespace {

class SyncManagerImpl : public SyncManager {
 public:
  explicit SyncManagerImpl(std::unique_ptr<HeliumTransport> transport);
  ~SyncManagerImpl() override;

  // HeliumSyncManager implementation.
  std::unique_ptr<SyncRegistration> Register(Syncable* object) override;
  std::unique_ptr<SyncRegistration> RegisterExisting(HeliumObjectId id,
                                                     Syncable* object) override;
  void Unregister(HeliumObjectId id) override;
  void Pause(HeliumObjectId id, bool paused) override;

 private:
  std::unique_ptr<HeliumTransport> transport_;

  DISALLOW_COPY_AND_ASSIGN(SyncManagerImpl);
};

SyncManagerImpl::SyncManagerImpl(std::unique_ptr<HeliumTransport> transport)
    : transport_(std::move(transport)) {
  DCHECK(transport_);
}

SyncManagerImpl::~SyncManagerImpl() {}

std::unique_ptr<SyncManager::SyncRegistration> SyncManagerImpl::Register(
    Syncable* object) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<SyncManager::SyncRegistration>
SyncManagerImpl::RegisterExisting(HeliumObjectId id, Syncable* object) {
  NOTIMPLEMENTED();
  return nullptr;
}

void SyncManagerImpl::Unregister(HeliumObjectId id) {
  NOTIMPLEMENTED();
}

void SyncManagerImpl::Pause(HeliumObjectId id, bool paused) {
  NOTIMPLEMENTED();
}

}  // namespace

// static
std::unique_ptr<SyncManager> SyncManager::Create(
    std::unique_ptr<HeliumTransport> transport) {
  return base::MakeUnique<SyncManagerImpl>(std::move(transport));
}

SyncManager::SyncRegistration::SyncRegistration(SyncManager* sync_manager,
                                                HeliumObjectId id)
    : id_(id), sync_manager_(sync_manager) {
  DCHECK(sync_manager);
}

SyncManager::SyncRegistration::~SyncRegistration() {
  sync_manager_->Unregister(id_);
}

void SyncManager::SyncRegistration::Pause(bool paused) {
  sync_manager_->Pause(id_, paused);
}

}  // namespace helium
}  // namespace blimp
