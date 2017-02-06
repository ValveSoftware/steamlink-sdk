// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_CONTROLLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class AssociatedGroup;
class InterfaceEndpointClient;
class InterfaceEndpointController;

// An internal interface used to manage endpoints within an associated group.
class AssociatedGroupController :
    public base::RefCountedDeleteOnMessageLoop<AssociatedGroupController> {
 public:
  explicit AssociatedGroupController(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Creates a pair of interface endpoint handles. The method generates a new
  // interface ID and assigns it to the two handles. |local_endpoint| is used
  // locally; while |remote_endpoint| is sent over the message pipe.
  virtual void CreateEndpointHandlePair(
      ScopedInterfaceEndpointHandle* local_endpoint,
      ScopedInterfaceEndpointHandle* remote_endpoint) = 0;

  // Creates an interface endpoint handle from a given interface ID. The handle
  // is used locally.
  // Typically, this method is used to (1) create an endpoint handle for the
  // master interface; or (2) create an endpoint handle on receiving an
  // interface ID from the message pipe.
  virtual ScopedInterfaceEndpointHandle CreateLocalEndpointHandle(
      InterfaceId id) = 0;

  // Closes an interface endpoint handle.
  virtual void CloseEndpointHandle(InterfaceId id, bool is_local) = 0;

  // Attaches a client to the specified endpoint to send and receive messages.
  // The returned object is still owned by the controller. It must only be used
  // on the same thread as this call, and only before the client is detached
  // using DetachEndpointClient().
  virtual InterfaceEndpointController* AttachEndpointClient(
      const ScopedInterfaceEndpointHandle& handle,
      InterfaceEndpointClient* endpoint_client,
      scoped_refptr<base::SingleThreadTaskRunner> runner) = 0;

  // Detaches the client attached to the specified endpoint. It must be called
  // on the same thread as the corresponding AttachEndpointClient() call.
  virtual void DetachEndpointClient(
      const ScopedInterfaceEndpointHandle& handle) = 0;

  // Raises an error on the underlying message pipe. It disconnects the pipe
  // and notifies all interfaces running on this pipe.
  virtual void RaiseError() = 0;

  std::unique_ptr<AssociatedGroup> CreateAssociatedGroup();

 protected:
  friend class base::RefCountedDeleteOnMessageLoop<AssociatedGroupController>;
  friend class base::DeleteHelper<AssociatedGroupController>;

  // Creates a new ScopedInterfaceEndpointHandle associated with this
  // controller.
  ScopedInterfaceEndpointHandle CreateScopedInterfaceEndpointHandle(
      InterfaceId id,
      bool is_local);

  virtual ~AssociatedGroupController();

  DISALLOW_COPY_AND_ASSIGN(AssociatedGroupController);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_CONTROLLER_H_
