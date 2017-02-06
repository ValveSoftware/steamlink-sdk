// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/standalone/arc_standalone_bridge_runner.h"

#include "base/bind.h"
#include "base/run_loop.h"

namespace arc {

ArcStandaloneBridgeRunner::ArcStandaloneBridgeRunner() : exit_code_(0) {
}

ArcStandaloneBridgeRunner::~ArcStandaloneBridgeRunner() {
}

int ArcStandaloneBridgeRunner::Run() {
  DCHECK(thread_checker_.CalledOnValidThread());
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
  run_loop_.reset(nullptr);

  return exit_code_;
}

void ArcStandaloneBridgeRunner::Stop(int exit_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  exit_code_ = exit_code;
  CHECK(run_loop_);
  message_loop_.PostTask(FROM_HERE, run_loop_->QuitClosure());
}

}  // namespace arc
