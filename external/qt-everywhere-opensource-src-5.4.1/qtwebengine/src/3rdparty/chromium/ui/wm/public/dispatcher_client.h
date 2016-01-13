// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_DISPATCHER_CLIENT_H_
#define UI_WM_PUBLIC_DISPATCHER_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "ui/aura/aura_export.h"

namespace aura {
class Window;
namespace client {

class DispatcherClient;

// A base::RunLoop like object for running a nested message-loop with a
// specified DispatcherClient and a MessagePumpDispatcher.
class AURA_EXPORT DispatcherRunLoop {
 public:
  DispatcherRunLoop(DispatcherClient* client,
                    base::MessagePumpDispatcher* dispatcher);
  ~DispatcherRunLoop();

  void Run();
  base::Closure QuitClosure();
  void Quit();

 private:
  base::Closure run_closure_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherRunLoop);
};

// An interface implemented by an object which handles nested dispatchers.
class AURA_EXPORT DispatcherClient {
 public:
  virtual ~DispatcherClient() {}

 protected:
  friend class DispatcherRunLoop;

  virtual void PrepareNestedLoopClosures(
      base::MessagePumpDispatcher* dispatcher,
      base::Closure* run_closure,
      base::Closure* quit_closure) = 0;
};

AURA_EXPORT void SetDispatcherClient(Window* root_window,
                                     DispatcherClient* client);
AURA_EXPORT DispatcherClient* GetDispatcherClient(Window* root_window);

}  // namespace client
}  // namespace aura

#endif  // UI_WM_PUBLIC_DISPATCHER_CLIENT_H_
