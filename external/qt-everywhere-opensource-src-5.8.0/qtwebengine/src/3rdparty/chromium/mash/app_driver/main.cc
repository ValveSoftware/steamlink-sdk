// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/app_driver/app_driver.h"
#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/application_runner.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ApplicationRunner runner(new mash::app_driver::AppDriver);
  return runner.Run(shell_handle);
}
