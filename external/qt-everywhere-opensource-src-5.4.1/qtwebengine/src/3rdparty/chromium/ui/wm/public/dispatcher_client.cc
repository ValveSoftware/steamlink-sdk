// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/dispatcher_client.h"

#include "base/callback.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::DispatcherClient*);

namespace aura {
namespace client {

DispatcherRunLoop::DispatcherRunLoop(DispatcherClient* client,
                                     base::MessagePumpDispatcher* dispatcher) {
  client->PrepareNestedLoopClosures(dispatcher, &run_closure_, &quit_closure_);
}

DispatcherRunLoop::~DispatcherRunLoop() {
}

void DispatcherRunLoop::Run() {
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
  run_closure_.Run();
}

base::Closure DispatcherRunLoop::QuitClosure() {
  return quit_closure_;
}

void DispatcherRunLoop::Quit() {
  quit_closure_.Run();
}

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(DispatcherClient*, kDispatcherClientKey, NULL);

void SetDispatcherClient(Window* root_window, DispatcherClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kDispatcherClientKey, client);
}

DispatcherClient* GetDispatcherClient(Window* root_window) {
  if (root_window)
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  return root_window ? root_window->GetProperty(kDispatcherClientKey) : NULL;
}

}  // namespace client
}  // namespace aura
