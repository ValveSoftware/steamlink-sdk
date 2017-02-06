// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_RUNNER_COMMON_CLIENT_UTIL_H_
#define SERVICES_SHELL_RUNNER_COMMON_CLIENT_UTIL_H_

#include <string>

#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace base {
class CommandLine;
}

namespace shell {

// Creates a new ShellClient pipe and returns one end of it. The other end is
// passed via a token in |command_line|. A child of the calling process may
// extract a ShellClientRequest from this by calling
// GetShellClientRequestFromCommandLine().
mojom::ShellClientPtr PassShellClientRequestOnCommandLine(
    base::CommandLine* command_line, const std::string& child_token);

// Extracts a ShellClientRequest from the command line of the current process.
// The parent of this process should have passed a request using
// PassShellClientRequestOnCommandLine().
mojom::ShellClientRequest GetShellClientRequestFromCommandLine();

// Returns true if the ShellClientRequest came via the command line from a shell
// instance in another process.
bool ShellIsRemote();

}  // namespace shell

#endif  // SERVICES_SHELL_RUNNER_COMMON_CLIENT_UTIL_H_
