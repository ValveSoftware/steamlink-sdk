// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_dispatcher_client.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/run_loop.h"

namespace views {

DesktopDispatcherClient::DesktopDispatcherClient() {
}

DesktopDispatcherClient::~DesktopDispatcherClient() {
}

void DesktopDispatcherClient::PrepareNestedLoopClosures(
    base::MessagePumpDispatcher* dispatcher,
    base::Closure* run_closure,
    base::Closure* quit_closure) {
#if defined(OS_WIN)
  scoped_ptr<base::RunLoop> run_loop(new base::RunLoop(dispatcher));
#else
  scoped_ptr<base::RunLoop> run_loop(new base::RunLoop());
#endif
  *quit_closure = run_loop->QuitClosure();
  *run_closure =
      base::Bind(&base::RunLoop::Run, base::Owned(run_loop.release()));
}

}  // namespace views
