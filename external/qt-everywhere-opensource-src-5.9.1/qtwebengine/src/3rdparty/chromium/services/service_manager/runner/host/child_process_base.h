// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_RUNNER_HOST_CHILD_PROCESS_BASE_H_
#define SERVICES_SERVICE_MANAGER_RUNNER_HOST_CHILD_PROCESS_BASE_H_

#include "base/callback.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace service_manager {

// Child processes call this to establish the connection to the service manager
// and obtain
// the ServiceRequest. Once the connection has been established |callback|
// is run. ChildProcessMainWithCallback() returns once the the callback
// completes.
using RunCallback = base::Callback<void(mojom::ServiceRequest)>;
void ChildProcessMainWithCallback(const RunCallback& callback);

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_RUNNER_HOST_CHILD_PROCESS_BASE_H_
