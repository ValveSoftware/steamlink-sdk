// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_STANDALONE_BRIDGE_RUNNER_H_
#define COMPONENTS_ARC_ARC_STANDALONE_BRIDGE_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_checker.h"

namespace arc {

// Runner of the standalone ARC++ bridge, which hosts an IO message loop.
class ArcStandaloneBridgeRunner {
 public:
  ArcStandaloneBridgeRunner();
  ~ArcStandaloneBridgeRunner();

  // Starts the message loop. Needs to be run on the thread where this
  // instance is created.
  int Run();

  // Stops the message loop. Needs to be called in a message loop.
  void Stop(int exit_code);

 private:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::ThreadChecker thread_checker_;
  int exit_code_;

  DISALLOW_COPY_AND_ASSIGN(ArcStandaloneBridgeRunner);
};


}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_STANDALONE_BRIDGE_RUNNER_H_
