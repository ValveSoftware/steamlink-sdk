// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_group_controller.h"

#include "mojo/public/cpp/bindings/associated_group.h"

namespace mojo {

AssociatedGroupController::~AssociatedGroupController() {}

std::unique_ptr<AssociatedGroup>
AssociatedGroupController::CreateAssociatedGroup() {
  std::unique_ptr<AssociatedGroup> group(new AssociatedGroup);
  group->controller_ = this;
  return group;
}

ScopedInterfaceEndpointHandle
AssociatedGroupController::CreateScopedInterfaceEndpointHandle(
    InterfaceId id,
    bool is_local) {
  return ScopedInterfaceEndpointHandle(id, is_local, this);
}

}  // namespace mojo
