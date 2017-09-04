// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_MANAGER_EMBEDDED_SERVICE_RUNNER_H_
#define CONTENT_COMMON_SERVICE_MANAGER_EMBEDDED_SERVICE_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "content/public/common/service_info.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace content {

// Hosts in-process service instances for a given service.
class EmbeddedServiceRunner {
 public:
  // Constructs a runner for a service. Every new instance started by the
  // Service Manager for this service will invoke the factory function on |info|
  // to create a new concrete instance of the Service implementation.
  EmbeddedServiceRunner(const base::StringPiece& name,
                        const ServiceInfo& info);
  ~EmbeddedServiceRunner();

  // Binds an incoming ServiceRequest for this service. This creates a new
  // instance of the Service implementation.
  void BindServiceRequest(service_manager::mojom::ServiceRequest request);

  // Sets a callback to run when all instances of the service have stopped.
  void SetQuitClosure(const base::Closure& quit_closure);

 private:
  class InstanceManager;

  void OnQuit();

  // A reference to the instance manager, which may operate on another thread.
  scoped_refptr<InstanceManager> instance_manager_;

  base::Closure quit_closure_;

  base::WeakPtrFactory<EmbeddedServiceRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedServiceRunner);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_MANAGER_EMBEDDED_SERVICE_RUNNER_H_
