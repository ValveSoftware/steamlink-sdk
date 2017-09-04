// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_MANAGER_CHILD_CONNECTION_H_
#define CONTENT_COMMON_SERVICE_MANAGER_CHILD_CONNECTION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"

namespace service_manager {
class Connection;
class Connector;
}

namespace content {

// Helper class to establish a connection between the Service Manager and a
// single child process. Process hosts can use this when launching new processes
// which should be registered with the service manager.
class CONTENT_EXPORT ChildConnection {
 public:
  // Prepares a new child connection for a child process which will be
  // identified to the service manager as |name|. |instance_id| must be unique
  // among all child connections using the same |name|. |connector| is the
  // connector to use to establish the connection.
  ChildConnection(const std::string& name,
                  const std::string& instance_id,
                  const std::string& child_token,
                  service_manager::Connector* connector,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~ChildConnection();

  service_manager::InterfaceProvider* GetRemoteInterfaces() {
    return &remote_interfaces_;
  }

  const service_manager::Identity& child_identity() const {
    return child_identity_;
  }

  // A token which must be passed to the child process via
  // |switches::kPrimordialPipeToken| in order for the child to initialize its
  // end of the Service Manager connection pipe.
  std::string service_token() const { return service_token_; }

  // Sets the child connection's process handle. This should be called as soon
  // as the process has been launched, and the connection will not be fully
  // functional until this is called.
  void SetProcessHandle(base::ProcessHandle handle);

 private:
  class IOThreadContext;

  const std::string child_token_;
  scoped_refptr<IOThreadContext> context_;
  service_manager::Identity child_identity_;
  const std::string service_token_;
  base::ProcessHandle process_handle_ = base::kNullProcessHandle;

  service_manager::InterfaceProvider remote_interfaces_;

  base::WeakPtrFactory<ChildConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildConnection);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_MANAGER_CHILD_CONNECTION_H_
