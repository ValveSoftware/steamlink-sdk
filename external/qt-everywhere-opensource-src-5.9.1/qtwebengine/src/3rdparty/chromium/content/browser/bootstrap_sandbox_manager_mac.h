// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BOOTSTRAP_SANDBOX_MANAGER_MAC_H_
#define CONTENT_BROWSER_BOOTSTRAP_SANDBOX_MANAGER_MAC_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/sandbox_type.h"

namespace sandbox {
class BootstrapSandbox;
struct BootstrapSandboxPolicy;
}

namespace content {

// This class is responsible for creating the BootstrapSandbox global
// singleton, as well as registering all associated policies with it.
//
// This class is thread-safe.
class BootstrapSandboxManager : public BrowserChildProcessObserver,
                                public RenderProcessHostObserver {
 public:
  // Whether or not the bootstrap sandbox should be enabled globally.
  static bool ShouldEnable();

  // Gets the singleton instance. The first call to this function, which
  // instantiates the object, must be on the UI thread.
  static BootstrapSandboxManager* GetInstance();

  // Whether or not the bootstrap sandbox applies to the given sandbox type.
  bool EnabledForSandbox(SandboxType sandbox_type);

  // BrowserChildProcessObserver:
  void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const ChildProcessData& data,
      int exit_code) override;

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;

  sandbox::BootstrapSandbox* sandbox() const { return sandbox_.get(); }

 private:
  friend struct base::DefaultSingletonTraits<BootstrapSandboxManager>;
  BootstrapSandboxManager();
  ~BootstrapSandboxManager() override;

  void RegisterSandboxPolicies();
  void RegisterRendererPolicy();

  void AddBaselinePolicy(sandbox::BootstrapSandboxPolicy* policy);

  std::unique_ptr<sandbox::BootstrapSandbox> sandbox_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapSandboxManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BOOTSTRAP_SANDBOX_MANAGER_MAC_H_
