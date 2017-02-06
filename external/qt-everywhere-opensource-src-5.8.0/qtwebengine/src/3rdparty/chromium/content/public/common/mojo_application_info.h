// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MOJO_APPLICATION_INFO_H_
#define CONTENT_PUBLIC_COMMON_MOJO_APPLICATION_INFO_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"

namespace shell {
class ShellClient;
}

namespace content {

// MojoApplicationInfo provides details necessary to construct and bind new
// instances of embedded Mojo applications.
struct CONTENT_EXPORT MojoApplicationInfo {
  using ApplicationFactory = base::Callback<std::unique_ptr<shell::ShellClient>(
      const base::Closure& quit_closure)>;

  MojoApplicationInfo();
  MojoApplicationInfo(const MojoApplicationInfo& other);
  ~MojoApplicationInfo();

  // A factory function which will be called to produce a new ShellClient
  // instance for this app whenever one is needed.
  ApplicationFactory application_factory;

  // The task runner on which to construct and bind new ShellClient instances
  // for this app. If null, behavior depends on the value of |use_own_thread|
  // below.
  scoped_refptr<base::SingleThreadTaskRunner> application_task_runner;

  // If |application_task_runner| is null, setting this to |true| will give
  // each instance of this app its own thread to run on. Setting this to |false|
  // (the default) will instead run the app on the main thread's task runner.
  //
  // If |application_task_runner| is not null, this value is ignored.
  bool use_own_thread = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MOJO_APPLICATION_INFO_H_
