// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/keep_alive.h"

#include "base/bind.h"
#include "mojo/shell/context.h"

namespace mojo {
namespace shell {

KeepAlive::KeepAlive(Context* context) : context_(context) {
  DCHECK(context_->task_runners()->ui_runner()->RunsTasksOnCurrentThread());
  ++context_->keep_alive_counter()->count_;
}

KeepAlive::~KeepAlive() {
  DCHECK(context_->task_runners()->ui_runner()->RunsTasksOnCurrentThread());
  if (--context_->keep_alive_counter()->count_ == 0) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&KeepAlive::MaybeQuit, context_));
  }
}

// static
void KeepAlive::MaybeQuit(Context* context) {
  if (context->keep_alive_counter()->count_ == 0)
    base::MessageLoop::current()->Quit();
}

}  // namespace shell
}  // namespace mojo
