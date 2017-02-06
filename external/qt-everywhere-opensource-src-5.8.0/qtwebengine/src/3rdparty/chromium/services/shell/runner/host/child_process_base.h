// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_RUNNER_HOST_CHILD_PROCESS_BASE_H_
#define SERVICES_SHELL_RUNNER_HOST_CHILD_PROCESS_BASE_H_

#include "base/callback.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace shell {

// Child processes call this to establish the connection to the shell and obtain
// the ShellClientRequest. Once the connection has been established |callback|
// is run. ChildProcessMainWithCallback() returns once the the callback
// completes.
using RunCallback = base::Callback<void(mojom::ShellClientRequest)>;
void ChildProcessMainWithCallback(const RunCallback& callback);

}  // namespace shell

#endif  // SERVICES_SHELL_RUNNER_HOST_CHILD_PROCESS_BASE_H_
