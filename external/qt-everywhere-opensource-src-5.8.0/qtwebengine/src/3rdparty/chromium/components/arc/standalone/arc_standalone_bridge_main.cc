// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "components/arc/standalone/arc_standalone_bridge_runner.h"
#include "components/arc/standalone/service_helper.h"
#include "mojo/edk/embedder/embedder.h"

// This is experimental only. Do not use in production.
// All threads in the standalone runner must be I/O threads.
int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager at_exit_manager;
  mojo::edk::Init();

  // Instantiate runner, which instantiates message loop.
  arc::ArcStandaloneBridgeRunner runner;
  arc::ServiceHelper helper;
  helper.Init(base::Bind(&arc::ArcStandaloneBridgeRunner::Stop,
                         base::Unretained(&runner),
                         1));

  // Spin the message loop.
  int exit_code = runner.Run();
  return exit_code;
}
