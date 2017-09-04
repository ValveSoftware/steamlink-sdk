// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/bootstrap_sandbox.h"

#include <servers/bootstrap.h>
#include <stdint.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mach_logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "sandbox/mac/launchd_interception_server.h"
#include "sandbox/mac/pre_exec_delegate.h"

namespace sandbox {

namespace {

struct SandboxCheckInRequest {
  mach_msg_header_t header;
  uint64_t token;
};

struct SandboxCheckInReply {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t bootstrap_port;
};

class ScopedCallMachMsgDestroy {
 public:
  explicit ScopedCallMachMsgDestroy(mach_msg_header_t* message)
      : message_(message) {}

  ~ScopedCallMachMsgDestroy() {
    if (message_)
      mach_msg_destroy(message_);
  }

  void Disarm() {
    message_ = nullptr;
  }

 private:
  mach_msg_header_t* message_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCallMachMsgDestroy);
};

}  // namespace

// static
std::unique_ptr<BootstrapSandbox> BootstrapSandbox::Create() {
  std::unique_ptr<BootstrapSandbox> null;  // Used for early returns.
  std::unique_ptr<BootstrapSandbox> sandbox(new BootstrapSandbox());
  sandbox->launchd_server_.reset(new LaunchdInterceptionServer(sandbox.get()));

  // Check in with launchd to get the receive right for the server that is
  // published in the bootstrap namespace.
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = bootstrap_check_in(bootstrap_port,
      sandbox->server_bootstrap_name().c_str(), &port);
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr)
        << "Failed to bootstrap_check_in the sandbox server.";
    return null;
  }
  sandbox->check_in_port_.reset(port);

  BootstrapSandbox* __block sandbox_ptr = sandbox.get();
  sandbox->check_in_server_.reset(new base::DispatchSourceMach(
      "org.chromium.sandbox.BootstrapClientManager",
      sandbox->check_in_port_.get(),
      ^{ sandbox_ptr->HandleChildCheckIn(); }));
  sandbox->check_in_server_->Resume();

  // Start the sandbox server.
  if (!sandbox->launchd_server_->Initialize(MACH_PORT_NULL))
    return null;

  return sandbox;
}

// Warning: This function must be safe to call in
// PreExecDelegate::RunAsyncSafe().
// static
bool BootstrapSandbox::ClientCheckIn(mach_port_t sandbox_server_port,
                                     uint64_t sandbox_token,
                                     mach_port_t* new_bootstrap_port) {
  // Create a reply port for the check in message.
  mach_port_t reply_port;
  kern_return_t kr = mach_port_allocate(mach_task_self(),
                                        MACH_PORT_RIGHT_RECEIVE,
                                        &reply_port);
  if (kr != KERN_SUCCESS) {
    RAW_LOG(ERROR, "ClientCheckIn: mach_port_allocate failed");
    return false;
  }
  base::mac::ScopedMachReceiveRight scoped_reply_port(reply_port);

  // Check in with the sandbox server, presenting the |sandbox_token| in
  // exchange for a new task bootstrap port.
  union {
    SandboxCheckInRequest request;
    struct {
      SandboxCheckInReply reply;
      mach_msg_trailer_t trailer;
    };
  } msg = {};
  msg.request.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
                                                MACH_MSG_TYPE_MAKE_SEND_ONCE);
  msg.request.header.msgh_remote_port = sandbox_server_port;
  msg.request.header.msgh_local_port = reply_port;
  msg.request.header.msgh_size = sizeof(msg);
  msg.request.token = sandbox_token;

  kr = mach_msg(&msg.request.header, MACH_SEND_MSG | MACH_RCV_MSG,
                sizeof(msg.request), sizeof(msg), reply_port,
                MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr == KERN_SUCCESS) {
    *new_bootstrap_port = msg.reply.bootstrap_port.name;
    return true;
  } else {
    RAW_LOG(ERROR, "ClientCheckIn: mach_msg failed");
    return false;
  }
}

BootstrapSandbox::~BootstrapSandbox() {
}

void BootstrapSandbox::RegisterSandboxPolicy(
    int sandbox_policy_id,
    const BootstrapSandboxPolicy& policy) {
  CHECK(IsPolicyValid(policy));
  base::AutoLock lock(lock_);
  DCHECK(policies_.find(sandbox_policy_id) == policies_.end());
  policies_.insert(std::make_pair(sandbox_policy_id, policy));
}

std::unique_ptr<PreExecDelegate> BootstrapSandbox::NewClient(
    int sandbox_policy_id) {
  base::AutoLock lock(lock_);

  DCHECK(policies_.find(sandbox_policy_id) != policies_.end());

  uint64_t token;
  while (true) {
    token = base::RandUint64();
    if (awaiting_processes_.find(token) == awaiting_processes_.end())
      break;
  }

  awaiting_processes_[token] = sandbox_policy_id;
  return base::MakeUnique<PreExecDelegate>(server_bootstrap_name_, token);
}

void BootstrapSandbox::RevokeToken(uint64_t token) {
  base::AutoLock lock(lock_);
  const auto& it = awaiting_processes_.find(token);
  if (it != awaiting_processes_.end())
    awaiting_processes_.erase(it);
}

void BootstrapSandbox::InvalidateClient(base::ProcessHandle handle) {
  base::AutoLock lock(lock_);
  const auto& it = sandboxed_processes_.find(handle);
  if (it != sandboxed_processes_.end())
    sandboxed_processes_.erase(it);
}

const BootstrapSandboxPolicy* BootstrapSandbox::PolicyForProcess(
    pid_t pid) const {
  base::AutoLock lock(lock_);
  const auto& process = sandboxed_processes_.find(pid);
  if (process != sandboxed_processes_.end()) {
    return &policies_.find(process->second)->second;
  }

  return nullptr;
}

BootstrapSandbox::BootstrapSandbox()
    : server_bootstrap_name_(
          base::StringPrintf("%s.sandbox.%d", base::mac::BaseBundleID(),
              getpid())),
      real_bootstrap_port_(MACH_PORT_NULL) {
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = task_get_special_port(
      mach_task_self(), TASK_BOOTSTRAP_PORT, &port);
  MACH_CHECK(kr == KERN_SUCCESS, kr);
  real_bootstrap_port_.reset(port);
}

void BootstrapSandbox::HandleChildCheckIn() {
  struct {
    SandboxCheckInRequest request;
    mach_msg_audit_trailer_t trailer;
  } msg = {};
  msg.request.header.msgh_local_port = check_in_port_.get();
  msg.request.header.msgh_size = sizeof(msg.request);
  const mach_msg_option_t kOptions = MACH_RCV_MSG |
      MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
      MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);
  kern_return_t kr = mach_msg(&msg.request.header, kOptions, 0,
                              sizeof(msg), check_in_port_.get(),
                              MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "HandleChildCheckIn mach_msg MACH_RCV_MSG";
    return;
  }

  // Call mach_msg_destroy to clean up the reply send-once right.
  ScopedCallMachMsgDestroy message_destroyer(&msg.request.header);
  pid_t client_pid = audit_token_to_pid(msg.trailer.msgh_audit);
  {
    base::AutoLock lock(lock_);

    auto awaiting_it = awaiting_processes_.find(msg.request.token);
    if (awaiting_it == awaiting_processes_.end()) {
      LOG(ERROR) << "Received sandbox check-in message from unknown client.";
      return;
    }

    CHECK(sandboxed_processes_.find(client_pid) == sandboxed_processes_.end());
    sandboxed_processes_[client_pid] = awaiting_it->second;
    awaiting_processes_.erase(awaiting_it);
  }

  SandboxCheckInReply reply = {};
  reply.header.msgh_bits = MACH_MSGH_BITS_REMOTE(msg.request.header.msgh_bits) |
                           MACH_MSGH_BITS_COMPLEX;
  reply.header.msgh_remote_port = msg.request.header.msgh_remote_port;
  reply.header.msgh_size = sizeof(reply);
  reply.body.msgh_descriptor_count = 1;
  reply.bootstrap_port.name = launchd_server_->server_port();
  reply.bootstrap_port.disposition = MACH_MSG_TYPE_MAKE_SEND;
  reply.bootstrap_port.type = MACH_MSG_PORT_DESCRIPTOR;

  kr = mach_msg(&reply.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                sizeof(reply), 0, MACH_PORT_NULL, 100 /*ms*/, MACH_PORT_NULL);
  if (kr == KERN_SUCCESS) {
    message_destroyer.Disarm();  // The send-once was consumed at mach_msg().
  } else {
    {
      base::AutoLock lock(lock_);
      sandboxed_processes_.erase(client_pid);
    }
    MACH_LOG(ERROR, kr) << "HandleChildCheckIn mach_msg MACH_SEND_MSG";
  }
}

}  // namespace sandbox
