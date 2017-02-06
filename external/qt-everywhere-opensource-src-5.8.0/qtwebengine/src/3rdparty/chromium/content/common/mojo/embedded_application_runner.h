// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_EMBEDDED_APPLICATION_RUNNER_H_
#define CONTENT_COMMON_MOJO_EMBEDDED_APPLICATION_RUNNER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "content/public/common/mojo_application_info.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace content {

// Hosts an in-process application instance that supports multiple ShellClient
// connections. The first incoming connection will invoke a provided factory
// function to instantiate the application, and the application will
// automatically be torn down when its last connection is lost. The application
// may be launched and torn down multiple times by a single
// EmbeddedApplicationRunner instance.
class EmbeddedApplicationRunner {
 public:
  // Constructs a runner which hosts a Mojo application. If an existing instance
  // of the app is not running when an incoming connection is made, details from
  // |info| will be used to construct a new instance.
  EmbeddedApplicationRunner(const base::StringPiece& name,
                            const MojoApplicationInfo& info);
  ~EmbeddedApplicationRunner();

  // Binds an incoming ShellClientRequest for this application. If the
  // application isn't already running, it's started. Otherwise the request is
  // bound to the running instance.
  void BindShellClientRequest(shell::mojom::ShellClientRequest request);

  // Sets a callback to run after the application loses its last connection and
  // is torn down.
  void SetQuitClosure(const base::Closure& quit_closure);

 private:
  class Instance;

  void OnQuit();

  // A reference to the application instance which may operate on the
  // |application_task_runner_|'s thread.
  scoped_refptr<Instance> instance_;

  base::Closure quit_closure_;

  base::WeakPtrFactory<EmbeddedApplicationRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedApplicationRunner);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_EMBEDDED_APPLICATION_RUNNER_H_
