// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/tracing/tracing_app.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ApplicationRunner runner(new tracing::TracingApp);
  return runner.Run(shell_handle);
}
