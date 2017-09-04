// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_group.h"

#include "mojo/public/cpp/bindings/associated_group_controller.h"

namespace mojo {

AssociatedGroup::AssociatedGroup() {}

AssociatedGroup::AssociatedGroup(const AssociatedGroup& other)
    : controller_(other.controller_) {}

AssociatedGroup::~AssociatedGroup() {}

AssociatedGroup& AssociatedGroup::operator=(const AssociatedGroup& other) {
  if (this == &other)
    return *this;

  controller_ = other.controller_;
  return *this;
}

void AssociatedGroup::CreateEndpointHandlePair(
    ScopedInterfaceEndpointHandle* local_endpoint,
    ScopedInterfaceEndpointHandle* remote_endpoint) {
  if (!controller_)
    return;

  controller_->CreateEndpointHandlePair(local_endpoint, remote_endpoint);
}

}  // namespace mojo
