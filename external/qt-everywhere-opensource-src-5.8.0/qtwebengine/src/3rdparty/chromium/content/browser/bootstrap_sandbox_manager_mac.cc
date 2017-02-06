// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bootstrap_sandbox_manager_mac.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "content/browser/mach_broker_mac.h"
#include "content/common/sandbox_init_mac.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "sandbox/mac/bootstrap_sandbox.h"

namespace content {

// static
bool BootstrapSandboxManager::ShouldEnable() {
  return false;
}

// static
BootstrapSandboxManager* BootstrapSandboxManager::GetInstance() {
  return base::Singleton<BootstrapSandboxManager>::get();
}

bool BootstrapSandboxManager::EnabledForSandbox(SandboxType sandbox_type) {
  return sandbox_type == SANDBOX_TYPE_RENDERER;
}

void BootstrapSandboxManager::BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) {
  sandbox()->InvalidateClient(data.handle);
}

void BootstrapSandboxManager::BrowserChildProcessCrashed(
      const ChildProcessData& data,
      int exit_code) {
  sandbox()->InvalidateClient(data.handle);
}

void BootstrapSandboxManager::RenderProcessExited(
    RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  sandbox()->InvalidateClient(host->GetHandle());
}

BootstrapSandboxManager::BootstrapSandboxManager()
    : sandbox_(sandbox::BootstrapSandbox::Create()) {
  CHECK(sandbox_.get());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserChildProcessObserver::Add(this);
  RegisterSandboxPolicies();
}

BootstrapSandboxManager::~BootstrapSandboxManager() {
  BrowserChildProcessObserver::Remove(this);
}

void BootstrapSandboxManager::RegisterSandboxPolicies() {
  RegisterRendererPolicy();
}

void BootstrapSandboxManager::RegisterRendererPolicy() {
  sandbox::BootstrapSandboxPolicy policy;
  AddBaselinePolicy(&policy);

  // Permit font queries.
  policy.rules["com.apple.FontServer"] = sandbox::Rule(sandbox::POLICY_ALLOW);
  policy.rules["com.apple.FontObjectsServer"] =
      sandbox::Rule(sandbox::POLICY_ALLOW);

  // Allow access to the windowserver. This is needed to get the colorspace
  // during sandbox warmup. Since NSColorSpace conforms to NSCoding, this
  // should be plumbed over IPC instead <http://crbug.com/265709>.
  policy.rules["com.apple.windowserver.active"] =
      sandbox::Rule(sandbox::POLICY_ALLOW);

  // Allow access to launchservicesd on 10.10+ otherwise the renderer will crash
  // attempting to get its ASN. http://crbug.com/533537
  if (base::mac::IsOSYosemiteOrLater()) {
    policy.rules["com.apple.coreservices.launchservicesd"] =
        sandbox::Rule(sandbox::POLICY_ALLOW);
  }

  sandbox_->RegisterSandboxPolicy(SANDBOX_TYPE_RENDERER, policy);
}

void BootstrapSandboxManager::AddBaselinePolicy(
    sandbox::BootstrapSandboxPolicy* policy) {
  auto& rules = policy->rules;

  // Allow the child to send its task port to the MachBroker.
  rules[MachBroker::GetMachPortName()] = sandbox::Rule(sandbox::POLICY_ALLOW);

  // Allow logging to the syslog.
  rules["com.apple.system.logger"] = sandbox::Rule(sandbox::POLICY_ALLOW);
}

}  // namespace content
