// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/launchd_interception_server.h"

#include <servers/bootstrap.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "sandbox/mac/bootstrap_sandbox.h"
#include "sandbox/mac/mach_message_server.h"
#include "sandbox/mac/os_compatibility.h"
#include "sandbox/mac/xpc_message_server.h"

namespace sandbox {

// The buffer size for all launchd messages. This comes from
// sizeof(union __RequestUnion__vproc_mig_job_subsystem) in launchd, and it
// is larger than the __ReplyUnion.
const mach_msg_size_t kBufferSize = 2096;

LaunchdInterceptionServer::LaunchdInterceptionServer(
    const BootstrapSandbox* sandbox)
    : sandbox_(sandbox),
      xpc_launchd_(false),
      sandbox_port_(MACH_PORT_NULL),
      compat_shim_(OSCompatibility::CreateForPlatform()) {
}

LaunchdInterceptionServer::~LaunchdInterceptionServer() {
  message_server_->Shutdown();
}

bool LaunchdInterceptionServer::Initialize(mach_port_t server_receive_right) {
  mach_port_t task = mach_task_self();
  kern_return_t kr;

  // Allocate the dummy sandbox port.
  mach_port_t port;
  if ((kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port)) !=
          KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "Failed to allocate dummy sandbox port.";
    return false;
  }
  sandbox_port_.reset(port);
  if ((kr = mach_port_insert_right(task, sandbox_port_.get(),
          sandbox_port_.get(), MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)) {
    MACH_LOG(ERROR, kr) << "Failed to allocate dummy sandbox port send right.";
    return false;
  }
  sandbox_send_port_.reset(sandbox_port_.get());

  if (base::mac::IsOSYosemiteOrLater()) {
    message_server_.reset(new XPCMessageServer(this, server_receive_right));
    xpc_launchd_ = true;
  } else {
    message_server_.reset(
        new MachMessageServer(this, server_receive_right, kBufferSize));
  }
  return message_server_->Initialize();
}

void LaunchdInterceptionServer::DemuxMessage(IPCMessage request) {
  const uint64_t message_subsystem =
      compat_shim_->GetMessageSubsystem(request);
  const uint64_t message_id = compat_shim_->GetMessageID(request);
  VLOG(3) << "Incoming message #" << message_subsystem << "," << message_id;

  pid_t sender_pid = message_server_->GetMessageSenderPID(request);
  const BootstrapSandboxPolicy* policy =
      sandbox_->PolicyForProcess(sender_pid);
  if (policy == NULL) {
    // No sandbox policy is in place for the sender of this message, which
    // means it came from the unknown. Reject it.
    VLOG(3) << "Message from unknown pid " << sender_pid << " rejected.";
    message_server_->RejectMessage(request, MIG_REMOTE_ERROR);
    return;
  }

  if (compat_shim_->IsServiceLookUpRequest(request)) {
    // Filter messages sent via bootstrap_look_up to enforce the sandbox policy
    // over the bootstrap namespace.
    HandleLookUp(request, policy);
  } else if (compat_shim_->IsVprocSwapInteger(request)) {
    // Ensure that any vproc_swap_integer requests are safe.
    HandleSwapInteger(request);
  } else if (compat_shim_->IsXPCDomainManagement(request)) {
    // XPC domain management requests just require an ACK.
    message_server_->SendReply(message_server_->CreateReply(request));
  } else {
    // All other messages are not permitted.
    VLOG(1) << "Rejecting unhandled message #" << message_id;
    message_server_->RejectMessage(request, MIG_REMOTE_ERROR);
  }
}

void LaunchdInterceptionServer::HandleLookUp(
    IPCMessage request,
    const BootstrapSandboxPolicy* policy) {
  const std::string request_service_name(
      compat_shim_->GetServiceLookupName(request));
  VLOG(2) << "Incoming look_up2 request for " << request_service_name;

  // Find the Rule for this service. If a named rule is not found, use the
  // default specified by the policy.
  const BootstrapSandboxPolicy::NamedRules::const_iterator it =
      policy->rules.find(request_service_name);
  Rule rule(policy->default_rule);
  if (it != policy->rules.end())
    rule = it->second;

  if (rule.result == POLICY_ALLOW) {
    // This service is explicitly allowed, so this message will not be
    // intercepted by the sandbox.
    VLOG(1) << "Permitting and forwarding look_up2: " << request_service_name;
    ForwardMessage(request);
  } else if (rule.result == POLICY_DENY_ERROR) {
    // The child is not permitted to look up this service. Send a MIG error
    // reply to the client. Returning a NULL or unserviced port for a look up
    // can cause clients to crash or hang.
    VLOG(1) << "Denying look_up2 with MIG error: " << request_service_name;
    message_server_->RejectMessage(request, BOOTSTRAP_UNKNOWN_SERVICE);
  } else if (rule.result == POLICY_DENY_DUMMY_PORT ||
             rule.result == POLICY_SUBSTITUTE_PORT) {
    // The policy result is to deny access to the real service port, replying
    // with a sandboxed port in its stead. Use either the dummy sandbox_port_
    // or the one specified in the policy.
    VLOG(1) << "Intercepting look_up2 with a sandboxed service port: "
            << request_service_name;

    mach_port_t result_port;
    if (rule.result == POLICY_DENY_DUMMY_PORT)
      result_port = sandbox_port_.get();
    else
      result_port = rule.substitute_port;

    IPCMessage reply = message_server_->CreateReply(request);
    compat_shim_->WriteServiceLookUpReply(reply, result_port);
    // If the message was sent successfully, clear the result_port out of the
    // message so that it is not destroyed at the end of ReceiveMessage. The
    // above-inserted right has been moved out of the process, and destroying
    // the message will unref yet another right.
    if (message_server_->SendReply(reply))
      compat_shim_->WriteServiceLookUpReply(reply, MACH_PORT_NULL);
  } else {
    NOTREACHED();
  }
}

void LaunchdInterceptionServer::HandleSwapInteger(IPCMessage request) {
  // Only allow getting information out of launchd. Do not allow setting
  // values. Two commonly observed values that are retrieved are
  // VPROC_GSK_MGR_PID and VPROC_GSK_TRANSACTIONS_ENABLED.
  if (compat_shim_->IsSwapIntegerReadOnly(request)) {
    VLOG(2) << "Forwarding vproc swap_integer message.";
    ForwardMessage(request);
  } else {
    VLOG(2) << "Rejecting non-read-only swap_integer message.";
    message_server_->RejectMessage(request, BOOTSTRAP_NOT_PRIVILEGED);
  }
}
void LaunchdInterceptionServer::ForwardMessage(IPCMessage request) {
  // If launchd is using XPC, then when the request is forwarded, it must
  // contain a valid domain port. Because the client processes are sandboxed,
  // they have not had their launchd domains uncorked (and launchd will
  // reject the message as being from an invalid client). Instead, provide the
  // original bootstrap as the domain port, so launchd services the request
  // as if it were coming from the sandbox host process (this).
  if (xpc_launchd_) {
    // xpc_dictionary_set_mach_send increments the send right count.
    xpc_dictionary_set_mach_send(request.xpc, "domain-port",
                                 sandbox_->real_bootstrap_port());
  }

  message_server_->ForwardMessage(request, sandbox_->real_bootstrap_port());
}

}  // namespace sandbox
