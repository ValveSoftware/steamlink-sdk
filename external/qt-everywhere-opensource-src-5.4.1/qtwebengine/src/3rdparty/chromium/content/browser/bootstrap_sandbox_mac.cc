// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bootstrap_sandbox_mac.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "content/browser/mach_broker_mac.h"
#include "content/common/sandbox_init_mac.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/sandbox_type_mac.h"
#include "sandbox/mac/bootstrap_sandbox.h"

namespace content {

namespace {

// This class is responsible for creating the BootstrapSandbox global
// singleton, as well as registering all associated policies with it.
class BootstrapSandboxPolicy : public BrowserChildProcessObserver,
                               public NotificationObserver {
 public:
  static BootstrapSandboxPolicy* GetInstance();

  sandbox::BootstrapSandbox* sandbox() const {
    return sandbox_.get();
  }

  // BrowserChildProcessObserver:
  virtual void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessCrashed(
      const ChildProcessData& data) OVERRIDE;

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<BootstrapSandboxPolicy>;
  BootstrapSandboxPolicy();
  virtual ~BootstrapSandboxPolicy();

  void RegisterSandboxPolicies();
  void RegisterRendererPolicy();

  void AddBaselinePolicy(sandbox::BootstrapSandboxPolicy* policy);

  NotificationRegistrar notification_registrar_;

  scoped_ptr<sandbox::BootstrapSandbox> sandbox_;
};

BootstrapSandboxPolicy* BootstrapSandboxPolicy::GetInstance() {
  return Singleton<BootstrapSandboxPolicy>::get();
}

void BootstrapSandboxPolicy::BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) {
  sandbox()->ChildDied(data.handle);
}

void BootstrapSandboxPolicy::BrowserChildProcessCrashed(
      const ChildProcessData& data) {
  sandbox()->ChildDied(data.handle);
}

void BootstrapSandboxPolicy::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_CLOSED:
      sandbox()->ChildDied(
          Details<RenderProcessHost::RendererClosedDetails>(details)->handle);
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
}

BootstrapSandboxPolicy::BootstrapSandboxPolicy()
    : sandbox_(sandbox::BootstrapSandbox::Create()) {
  CHECK(sandbox_.get());
  BrowserChildProcessObserver::Add(this);
  notification_registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
      NotificationService::AllBrowserContextsAndSources());
  RegisterSandboxPolicies();
}

BootstrapSandboxPolicy::~BootstrapSandboxPolicy() {
  BrowserChildProcessObserver::Remove(this);
}

void BootstrapSandboxPolicy::RegisterSandboxPolicies() {
  RegisterRendererPolicy();
}

void BootstrapSandboxPolicy::RegisterRendererPolicy() {
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

  sandbox_->RegisterSandboxPolicy(SANDBOX_TYPE_RENDERER, policy);
}

void BootstrapSandboxPolicy::AddBaselinePolicy(
    sandbox::BootstrapSandboxPolicy* policy) {
  auto& rules = policy->rules;

  // Allow the child to send its task port to the MachBroker.
  rules[MachBroker::GetMachPortName()] = sandbox::Rule(sandbox::POLICY_ALLOW);

  // Allow logging to the syslog.
  rules["com.apple.system.logger"] = sandbox::Rule(sandbox::POLICY_ALLOW);
}

}  // namespace

bool ShouldEnableBootstrapSandbox() {
  return false;
}

sandbox::BootstrapSandbox* GetBootstrapSandbox() {
  return BootstrapSandboxPolicy::GetInstance()->sandbox();
}

}  // namespace content
