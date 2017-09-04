// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_PRE_EXEC_DELEGATE_H_
#define SANDBOX_MAC_PRE_EXEC_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/process/launch.h"
#include "sandbox/mac/xpc.h"

namespace sandbox {

// This PreExecDelegate will communicate with the BootstrapSandbox running
// the Mach server registered under |sandbox_server_bootstrap_name|. It will
// check in with th BootstrapSandbox using the |sandbox_token| and will
// replace the task's bootstrap port with one provided by the sandbox.
class PreExecDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  PreExecDelegate(const std::string& sandbox_server_bootstrap_name,
                  uint64_t sandbox_token);
  ~PreExecDelegate() override;

  void RunAsyncSafe() override;

  uint64_t sandbox_token() const { return sandbox_token_; }

 private:
  // Allocates the bootstrap_look_up IPC message prior to fork().
  xpc_object_t CreateBootstrapLookUpMessage();

  // Performs a bootstrap_look_up(), either using the pre-allocated message
  // or the normal routine, depending on the OS X system version.
  kern_return_t DoBootstrapLookUp(mach_port_t* out_port);

  const std::string sandbox_server_bootstrap_name_;
  const char* const sandbox_server_bootstrap_name_ptr_;
  const uint64_t sandbox_token_;
  const bool is_yosemite_or_later_;

  // If is_yosemite_or_later_, this field is used to hold the pre-allocated XPC
  // object needed to interact with the bootstrap server in RunAsyncSafe().
  // This is deliberately leaked in the fork()ed process.
  xpc_object_t look_up_message_;

  DISALLOW_COPY_AND_ASSIGN(PreExecDelegate);
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_PRE_EXEC_DELEGATE_H_
