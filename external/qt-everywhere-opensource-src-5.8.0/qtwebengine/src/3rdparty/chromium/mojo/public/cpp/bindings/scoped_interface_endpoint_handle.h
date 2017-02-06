// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SCOPED_INTERFACE_ENDPOINT_HANDLE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SCOPED_INTERFACE_ENDPOINT_HANDLE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/interface_id.h"

namespace mojo {

class AssociatedGroupController;

// ScopedInterfaceEndpointHandle refers to one end of an interface, either the
// implementation side or the client side.
class ScopedInterfaceEndpointHandle {
 public:
  // Creates an invalid endpoint handle.
  ScopedInterfaceEndpointHandle();

  ScopedInterfaceEndpointHandle(ScopedInterfaceEndpointHandle&& other);

  ~ScopedInterfaceEndpointHandle();

  ScopedInterfaceEndpointHandle& operator=(
      ScopedInterfaceEndpointHandle&& other);

  bool is_valid() const { return IsValidInterfaceId(id_); }

  bool is_local() const { return is_local_; }

  void reset();
  void swap(ScopedInterfaceEndpointHandle& other);

  // DO NOT USE METHODS BELOW THIS LINE. These are for internal use and testing.

  InterfaceId id() const { return id_; }

  AssociatedGroupController* group_controller() const {
    return group_controller_.get();
  }

  // Releases the handle without closing it.
  InterfaceId release();

 private:
  friend class AssociatedGroupController;

  // This is supposed to be used by AssociatedGroupController only.
  // |id| is the corresponding interface ID.
  // If |is_local| is false, this handle is meant to be passed over a message
  // pipe the remote side of the associated group.
  ScopedInterfaceEndpointHandle(
      InterfaceId id,
      bool is_local,
      scoped_refptr<AssociatedGroupController> group_controller);

  InterfaceId id_;
  bool is_local_;
  scoped_refptr<AssociatedGroupController> group_controller_;

  DISALLOW_COPY_AND_ASSIGN(ScopedInterfaceEndpointHandle);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SCOPED_INTERFACE_ENDPOINT_HANDLE_H_
