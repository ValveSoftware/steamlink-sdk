// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/mojo/services/mojo_media_application.h"
#include "media/mojo/services/test_mojo_media_client.h"
#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/application_runner.h"

namespace {

shell::ApplicationRunner* g_runner = nullptr;

void QuitApplication() {
  DCHECK(g_runner);
  g_runner->Quit();
}

}  // namespace

MojoResult MojoMain(MojoHandle mojo_handle) {
  // Enable logging.
  base::AtExitManager at_exit;
  shell::ApplicationRunner::InitBaseCommandLine();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  shell::ApplicationRunner runner(new media::MojoMediaApplication(
      base::WrapUnique(new media::TestMojoMediaClient()),
      base::Bind(&QuitApplication)));
  g_runner = &runner;
  return runner.Run(mojo_handle, false /* init_base */);
}
