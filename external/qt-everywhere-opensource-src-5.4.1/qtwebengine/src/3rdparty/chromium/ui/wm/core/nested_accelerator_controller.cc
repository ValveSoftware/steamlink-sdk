// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/nested_accelerator_controller.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "ui/wm/core/nested_accelerator_delegate.h"
#include "ui/wm/core/nested_accelerator_dispatcher.h"

namespace wm {

NestedAcceleratorController::NestedAcceleratorController(
    NestedAcceleratorDelegate* delegate)
    : dispatcher_delegate_(delegate) {
  DCHECK(delegate);
}

NestedAcceleratorController::~NestedAcceleratorController() {
}

void NestedAcceleratorController::PrepareNestedLoopClosures(
    base::MessagePumpDispatcher* nested_dispatcher,
    base::Closure* run_closure,
    base::Closure* quit_closure) {
  scoped_ptr<NestedAcceleratorDispatcher> old_accelerator_dispatcher =
      accelerator_dispatcher_.Pass();
  accelerator_dispatcher_ = NestedAcceleratorDispatcher::Create(
      dispatcher_delegate_.get(), nested_dispatcher);

  scoped_ptr<base::RunLoop> run_loop = accelerator_dispatcher_->CreateRunLoop();
  *quit_closure =
      base::Bind(&NestedAcceleratorController::QuitNestedMessageLoop,
                 base::Unretained(this),
                 run_loop->QuitClosure());
  *run_closure = base::Bind(&NestedAcceleratorController::RunNestedMessageLoop,
                            base::Unretained(this),
                            base::Passed(&run_loop),
                            base::Passed(&old_accelerator_dispatcher));
}

void NestedAcceleratorController::RunNestedMessageLoop(
    scoped_ptr<base::RunLoop> run_loop,
    scoped_ptr<NestedAcceleratorDispatcher> old_accelerator_dispatcher) {
  run_loop->Run();
  accelerator_dispatcher_ = old_accelerator_dispatcher.Pass();
}

void NestedAcceleratorController::QuitNestedMessageLoop(
    const base::Closure& quit_runloop) {
  quit_runloop.Run();
  accelerator_dispatcher_.reset();
}

}  // namespace wm
