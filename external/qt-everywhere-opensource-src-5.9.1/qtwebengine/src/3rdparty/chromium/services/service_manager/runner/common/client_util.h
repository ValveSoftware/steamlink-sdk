// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_RUNNER_COMMON_CLIENT_UTIL_H_
#define SERVICES_SERVICE_MANAGER_RUNNER_COMMON_CLIENT_UTIL_H_

#include <string>

#include "services/service_manager/public/interfaces/service.mojom.h"

namespace base {
class CommandLine;
}

namespace service_manager {

// Creates a new Service pipe and returns one end of it. The other end is
// passed via a token in |command_line|. A child of the calling process may
// extract a ServiceRequest from this by calling
// GetServiceRequestFromCommandLine().
mojom::ServicePtr PassServiceRequestOnCommandLine(
    base::CommandLine* command_line, const std::string& child_token);

// Extracts a ServiceRequest from the command line of the current process.
// The parent of this process should have passed a request using
// PassServiceRequestOnCommandLine().
mojom::ServiceRequest GetServiceRequestFromCommandLine();

// Returns true if the ServiceRequest came via the command line from a service
// manager
// instance in another process.
bool ServiceManagerIsRemote();

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_RUNNER_COMMON_CLIENT_UTIL_H_
