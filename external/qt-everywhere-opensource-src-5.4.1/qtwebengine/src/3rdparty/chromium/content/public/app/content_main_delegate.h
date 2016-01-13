// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_

#include <string>

#include "build/build_config.h"
#include "content/common/content_export.h"

template <typename>
class ScopedVector;

namespace content {

class ContentBrowserClient;
class ContentPluginClient;
class ContentRendererClient;
class ContentUtilityClient;
class ZygoteForkDelegate;
struct MainFunctionParams;

class CONTENT_EXPORT ContentMainDelegate {
 public:
  virtual ~ContentMainDelegate() {}

  // Tells the embedder that the absolute basic startup has been done, i.e.
  // it's now safe to create singletons and check the command line. Return true
  // if the process should exit afterwards, and if so, |exit_code| should be
  // set. This is the place for embedder to do the things that must happen at
  // the start. Most of its startup code should be in the methods below.
  virtual bool BasicStartupComplete(int* exit_code);

  // This is where the embedder puts all of its startup code that needs to run
  // before the sandbox is engaged.
  virtual void PreSandboxStartup() {}

  // This is where the embedder can add startup code to run after the sandbox
  // has been initialized.
  virtual void SandboxInitialized(const std::string& process_type) {}

  // Asks the embedder to start a process. Return -1 for the default behavior.
  virtual int RunProcess(
      const std::string& process_type,
      const MainFunctionParams& main_function_params);

  // Called right before the process exits.
  virtual void ProcessExiting(const std::string& process_type) {}

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Returns true if the process registers with the system monitor, so that we
  // can allocate an IO port for it before the sandbox is initialized. Embedders
  // are called only for process types that content doesn't know about.
  virtual bool ProcessRegistersWithSystemProcess(
      const std::string& process_type);

  // Used to determine if we should send the mach port to the parent process or
  // not. The embedder usually sends it for all child processes, use this to
  // override this behavior.
  virtual bool ShouldSendMachPort(const std::string& process_type);

  // Allows the embedder to override initializing the sandbox. This is needed
  // because some processes might not want to enable it right away or might not
  // want it at all.
  virtual bool DelaySandboxInitialization(const std::string& process_type);

#elif defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_IOS)
  // Tells the embedder that the zygote process is starting, and allows it to
  // specify one or more zygote delegates if it wishes by storing them in
  // |*delegates|.
  virtual void ZygoteStarting(ScopedVector<ZygoteForkDelegate>* delegates);

  // Called every time the zygote process forks.
  virtual void ZygoteForked() {}
#endif  // OS_MACOSX

 protected:
  friend class ContentClientInitializer;

  // Called once per relevant process type to allow the embedder to customize
  // content. If an embedder wants the default (empty) implementation, don't
  // override this.
  virtual ContentBrowserClient* CreateContentBrowserClient();
  virtual ContentPluginClient* CreateContentPluginClient();
  virtual ContentRendererClient* CreateContentRendererClient();
  virtual ContentUtilityClient* CreateContentUtilityClient();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
