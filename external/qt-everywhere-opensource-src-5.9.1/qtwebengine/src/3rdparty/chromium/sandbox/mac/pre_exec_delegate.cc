// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/pre_exec_delegate.h"

#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "sandbox/mac/bootstrap_sandbox.h"
#include "sandbox/mac/xpc.h"

namespace sandbox {

PreExecDelegate::PreExecDelegate(
    const std::string& sandbox_server_bootstrap_name,
    uint64_t sandbox_token)
    : sandbox_server_bootstrap_name_(sandbox_server_bootstrap_name),
      sandbox_server_bootstrap_name_ptr_(
          sandbox_server_bootstrap_name_.c_str()),
      sandbox_token_(sandbox_token),
      is_yosemite_or_later_(base::mac::IsAtLeastOS10_10()),
      look_up_message_(CreateBootstrapLookUpMessage()) {}

PreExecDelegate::~PreExecDelegate() {}

void PreExecDelegate::RunAsyncSafe() {
  mach_port_t sandbox_server_port = MACH_PORT_NULL;
  kern_return_t kr = DoBootstrapLookUp(&sandbox_server_port);
  if (kr != KERN_SUCCESS)
    RAW_LOG(FATAL, "Failed to look up bootstrap sandbox server port.");

  mach_port_t new_bootstrap_port = MACH_PORT_NULL;
  if (!BootstrapSandbox::ClientCheckIn(sandbox_server_port,
                                       sandbox_token_,
                                       &new_bootstrap_port)) {
    RAW_LOG(FATAL, "Failed to check in with sandbox server.");
  }

  kr = task_set_bootstrap_port(mach_task_self(), new_bootstrap_port);
  if (kr != KERN_SUCCESS)
    RAW_LOG(FATAL, "Failed to replace bootstrap port.");

  // On OS X 10.10 and higher, libxpc uses the port stash to transfer the
  // XPC root port. This is effectively the same connection as the Mach
  // bootstrap port, but not transferred using the task special port.
  // Therefore, stash the replacement bootstrap port, so that on 10.10 it
  // will be retrieved by the XPC code and used as a replacement for the
  // XPC root port as well.
  if (is_yosemite_or_later_) {
    kr = mach_ports_register(mach_task_self(), &new_bootstrap_port, 1);
    if (kr != KERN_SUCCESS)
      RAW_LOG(ERROR, "Failed to register replacement bootstrap port.");
  }
}

xpc_object_t PreExecDelegate::CreateBootstrapLookUpMessage() {
  if (is_yosemite_or_later_) {
    xpc_object_t dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_uint64(dictionary, "type", 7);
    xpc_dictionary_set_uint64(dictionary, "handle", 0);
    xpc_dictionary_set_string(dictionary, "name",
        sandbox_server_bootstrap_name_ptr_);
    xpc_dictionary_set_int64(dictionary, "targetpid", 0);
    xpc_dictionary_set_uint64(dictionary, "flags", 0);
    xpc_dictionary_set_uint64(dictionary, "subsystem", 5);
    xpc_dictionary_set_uint64(dictionary, "routine", 207);
    // Add a NULL port so that the slot in the dictionary is already
    // allocated.
    xpc_dictionary_set_mach_send(dictionary, "domain-port", MACH_PORT_NULL);
    return dictionary;
  }

  return nullptr;
}

kern_return_t PreExecDelegate::DoBootstrapLookUp(mach_port_t* out_port) {
  if (is_yosemite_or_later_) {
    xpc_dictionary_set_mach_send(look_up_message_, "domain-port",
        bootstrap_port);

    // |pipe| cannot be created pre-fork() since the |bootstrap_port| will
    // be invalidated. Deliberately leak |pipe| as well.
    xpc_pipe_t pipe = xpc_pipe_create_from_port(bootstrap_port, 0);
    xpc_object_t reply;
    int rv = xpc_pipe_routine(pipe, look_up_message_, &reply);
    if (rv != 0) {
      return xpc_dictionary_get_int64(reply, "error");
    } else {
      xpc_object_t port_value = xpc_dictionary_get_value(reply, "port");
      *out_port = xpc_mach_send_get_right(port_value);
      return *out_port != MACH_PORT_NULL ? KERN_SUCCESS : KERN_INVALID_RIGHT;
    }
  } else {
    // On non-XPC launchd systems, bootstrap_look_up() is MIG-based and
    // generally safe.
    return bootstrap_look_up(bootstrap_port,
        sandbox_server_bootstrap_name_ptr_, out_port);
  }
}

}  // namespace sandbox
