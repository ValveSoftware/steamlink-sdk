// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

#include "base/logging.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"

namespace mojo {

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle()
    : ScopedInterfaceEndpointHandle(kInvalidInterfaceId, true, nullptr) {}

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle(
    ScopedInterfaceEndpointHandle&& other)
    : id_(other.id_), is_local_(other.is_local_) {
  group_controller_.swap(other.group_controller_);
  other.id_ = kInvalidInterfaceId;
}

ScopedInterfaceEndpointHandle::~ScopedInterfaceEndpointHandle() {
  reset();
}

ScopedInterfaceEndpointHandle& ScopedInterfaceEndpointHandle::operator=(
    ScopedInterfaceEndpointHandle&& other) {
  reset();
  swap(other);

  return *this;
}

void ScopedInterfaceEndpointHandle::reset() {
  if (!IsValidInterfaceId(id_))
    return;

  group_controller_->CloseEndpointHandle(id_, is_local_);

  id_ = kInvalidInterfaceId;
  is_local_ = true;
  group_controller_ = nullptr;
}

void ScopedInterfaceEndpointHandle::swap(ScopedInterfaceEndpointHandle& other) {
  using std::swap;
  swap(other.id_, id_);
  swap(other.is_local_, is_local_);
  swap(other.group_controller_, group_controller_);
}

InterfaceId ScopedInterfaceEndpointHandle::release() {
  InterfaceId result = id_;

  id_ = kInvalidInterfaceId;
  is_local_ = true;
  group_controller_ = nullptr;

  return result;
}

ScopedInterfaceEndpointHandle::ScopedInterfaceEndpointHandle(
    InterfaceId id,
    bool is_local,
    scoped_refptr<AssociatedGroupController> group_controller)
    : id_(id),
      is_local_(is_local),
      group_controller_(std::move(group_controller)) {
  DCHECK(!IsValidInterfaceId(id) || group_controller_);
}

}  // namespace mojo
