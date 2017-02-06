// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_GAMEPAD_H_
#define COMPONENTS_EXO_GAMEPAD_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "ui/aura/client/focus_change_observer.h"

namespace exo {
class GamepadDelegate;

using CreateGamepadDataFetcherCallback =
    base::Callback<std::unique_ptr<device::GamepadDataFetcher>()>;

// This class represents one or more gamepads, it uses a background thread
// for polling gamepad devices and notifies the GamepadDelegate of any
// changes.
class Gamepad : public aura::client::FocusChangeObserver {
 public:
  // This class will post tasks to invoke the delegate on the thread runner
  // which is associated with the thread that is creating this instance.
  Gamepad(GamepadDelegate* delegate,
          base::SingleThreadTaskRunner* polling_task_runner);
  // Allows test cases to specify a CreateGamepadDataFetcherCallback that
  // overrides the default GamepadPlatformDataFetcher.
  Gamepad(GamepadDelegate* delegate,
          base::SingleThreadTaskRunner* polling_task_runner,
          CreateGamepadDataFetcherCallback create_fetcher_callback);
  ~Gamepad() override;

  // Overridden aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

 private:
  class ThreadSafeGamepadChangeFetcher;

  // Processes updates of gamepad data and passes changes on to delegate.
  void ProcessGamepadChanges(const blink::WebGamepad new_pad);

  // Private implementation of methods and resources that are used on the
  // polling thread.
  scoped_refptr<ThreadSafeGamepadChangeFetcher> gamepad_change_fetcher_;

  // The delegate instance that all events are dispatched to.
  GamepadDelegate* const delegate_;

  // The current state of the gamepad represented by this instance.
  blink::WebGamepad pad_state_;

  // ThreadChecker for the origin thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<Gamepad> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Gamepad);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_GAMEPAD_H_
