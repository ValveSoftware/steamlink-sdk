// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager_client.h"

#include "content/common/gpu/gpu_memory_manager.h"

namespace content {

GpuMemoryManagerClientState::GpuMemoryManagerClientState(
    GpuMemoryManager* memory_manager,
    GpuMemoryManagerClient* client,
    GpuMemoryTrackingGroup* tracking_group,
    bool has_surface,
    bool visible)
    : memory_manager_(memory_manager),
      client_(client),
      tracking_group_(tracking_group),
      has_surface_(has_surface),
      visible_(visible),
      list_iterator_valid_(false),
      hibernated_(false) {
}

GpuMemoryManagerClientState::~GpuMemoryManagerClientState() {
  memory_manager_->OnDestroyClientState(this);
}

void GpuMemoryManagerClientState::SetVisible(bool visible) {
  memory_manager_->SetClientStateVisible(this, visible);
}

}  // namespace content
