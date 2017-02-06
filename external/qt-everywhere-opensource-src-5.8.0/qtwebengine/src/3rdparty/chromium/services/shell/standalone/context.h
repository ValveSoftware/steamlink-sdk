// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_STANDALONE_CONTEXT_H_
#define SERVICES_SHELL_STANDALONE_CONTEXT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "services/shell/shell.h"
#include "services/shell/standalone/tracer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace catalog {
class Catalog;
class Store;
}

namespace shell {
class NativeRunnerDelegate;

// The "global" context for the shell's main process.
class Context : public mojo::edk::ProcessDelegate {
 public:
  struct InitParams {
    InitParams();
    ~InitParams();

    NativeRunnerDelegate* native_runner_delegate = nullptr;
    std::unique_ptr<catalog::Store> catalog_store;
    // If true the edk is initialized.
    bool init_edk = true;
  };

  Context();
  ~Context() override;

  static void EnsureEmbedderIsInitialized();

  // This must be called with a message loop set up for the current thread,
  // which must remain alive until after Shutdown() is called.
  void Init(std::unique_ptr<InitParams> init_params);

  // If Init() was called and succeeded, this must be called before destruction.
  void Shutdown();

  // Run the application specified on the command line.
  void RunCommandLineApplication();

  Shell* shell() { return shell_.get(); }

 private:
  // mojo::edk::ProcessDelegate:
  void OnShutdownComplete() override;

  // Runs the app specified by |name|.
  void Run(const std::string& name);

  scoped_refptr<base::SingleThreadTaskRunner> shell_runner_;
  std::unique_ptr<base::Thread> io_thread_;
  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  // Ensure this is destructed before task_runners_ since it owns a message pipe
  // that needs the IO thread to destruct cleanly.
  Tracer tracer_;
  std::unique_ptr<catalog::Catalog> catalog_;
  std::unique_ptr<Shell> shell_;
  base::Time main_entry_time_;
  bool init_edk_ = false;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace shell

#endif  // SERVICES_SHELL_STANDALONE_CONTEXT_H_
