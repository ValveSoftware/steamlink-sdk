// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_

#include <utility>

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class AssociatedGroupController;

// AssociatedGroup refers to all the interface endpoints running at one end of a
// message pipe. It is used to create associated interfaces for that message
// pipe.
// It is thread safe and cheap to make copies.
class AssociatedGroup {
 public:
  // Configuration used by CreateAssociatedInterface(). Please see the comments
  // of that method for more details.
  enum AssociatedInterfaceConfig { WILL_PASS_PTR, WILL_PASS_REQUEST };

  AssociatedGroup();
  AssociatedGroup(const AssociatedGroup& other);

  ~AssociatedGroup();

  AssociatedGroup& operator=(const AssociatedGroup& other);

  // |config| indicates whether |ptr_info| or |request| will be sent to the
  // remote side of the message pipe.
  //
  // NOTE: If |config| is |WILL_PASS_REQUEST|, you will want to bind |ptr_info|
  // to a local AssociatedInterfacePtr to make calls. However, there is one
  // restriction: the pointer should NOT be used to make calls before |request|
  // is sent. Violating that will cause the message pipe to be closed. On the
  // other hand, as soon as |request| is sent, the pointer is usable. There is
  // no need to wait until |request| is bound to an implementation at the remote
  // side.
  template <typename T>
  void CreateAssociatedInterface(
      AssociatedInterfaceConfig config,
      AssociatedInterfacePtrInfo<T>* ptr_info,
      AssociatedInterfaceRequest<T>* request) {
    ScopedInterfaceEndpointHandle local;
    ScopedInterfaceEndpointHandle remote;
    CreateEndpointHandlePair(&local, &remote);

    if (!local.is_valid() || !remote.is_valid()) {
      *ptr_info = AssociatedInterfacePtrInfo<T>();
      *request = AssociatedInterfaceRequest<T>();
      return;
    }

    if (config == WILL_PASS_PTR) {
      ptr_info->set_handle(std::move(remote));

      // The implementation is local, therefore set the version according to
      // the interface definition that this code is built against.
      ptr_info->set_version(T::Version_);
      request->Bind(std::move(local));
    } else {
      ptr_info->set_handle(std::move(local));

      // The implementation is remote, we don't know about its actual version
      // yet.
      ptr_info->set_version(0u);
      request->Bind(std::move(remote));
    }
  }

 private:
  friend class AssociatedGroupController;

  void CreateEndpointHandlePair(
      ScopedInterfaceEndpointHandle* local_endpoint,
      ScopedInterfaceEndpointHandle* remote_endpoint);

  scoped_refptr<AssociatedGroupController> controller_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_
