// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/app/blimp_startup.h"
#include "blimp/client/app/linux/blimp_client_session_linux.h"
#include "blimp/client/feature/navigation_feature.h"
#include "blimp/client/feature/tab_control_feature.h"
#include "blimp/client/session/assignment_source.h"
#include "ui/gfx/x/x11_connection.h"

namespace {
const char kDummyLoginToken[] = "";
const char kDefaultUrl[] = "https://www.google.com";
const int kDummyTabId = 0;
}

int main(int argc, const char**argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  CHECK(gfx::InitializeThreadedX11());

  blimp::client::InitializeLogging();
  blimp::client::InitializeMainMessageLoop();

  blimp::client::BlimpClientSessionLinux session;
  session.GetTabControlFeature()->CreateTab(kDummyTabId);
  session.Connect(kDummyLoginToken);

  // If there is a non-switch argument to the command line, load that url.
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  std::string url = args.size() > 0 ? args[0] : kDefaultUrl;
  session.GetNavigationFeature()->NavigateToUrlText(kDummyTabId, url);

  base::RunLoop().Run();
}
