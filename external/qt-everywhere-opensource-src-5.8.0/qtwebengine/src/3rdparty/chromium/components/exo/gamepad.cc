// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gamepad.h"

#include <cmath>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_platform_data_fetcher.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"

namespace exo {
namespace {

constexpr double kGamepadButtonValueEpsilon = 0.001;
bool GamepadButtonValuesAreEqual(double a, double b) {
  return fabs(a - b) < kGamepadButtonValueEpsilon;
}

std::unique_ptr<device::GamepadDataFetcher> CreateGamepadPlatformDataFetcher() {
  return std::unique_ptr<device::GamepadDataFetcher>(
      new device::GamepadPlatformDataFetcher());
}

// Time between gamepad polls in milliseconds.
constexpr unsigned kPollingTimeIntervalMs = 16;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Gamepad::ThreadSafeGamepadChangeFetcher

// Implements all methods and resources running on the polling thread.
// This class is reference counted to allow it to shut down safely on the
// polling thread even if the Gamepad has been destroyed on the origin thread.
class Gamepad::ThreadSafeGamepadChangeFetcher
    : public base::RefCountedThreadSafe<
          Gamepad::ThreadSafeGamepadChangeFetcher> {
 public:
  using ProcessGamepadChangesCallback =
      base::Callback<void(const blink::WebGamepad)>;

  ThreadSafeGamepadChangeFetcher(
      const ProcessGamepadChangesCallback& post_gamepad_changes,
      const CreateGamepadDataFetcherCallback& create_fetcher_callback,
      base::SingleThreadTaskRunner* task_runner)
      : process_gamepad_changes_(post_gamepad_changes),
        create_fetcher_callback_(create_fetcher_callback),
        polling_task_runner_(task_runner),
        origin_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    thread_checker_.DetachFromThread();
  }

  // Enable or disable gamepad polling. Can be called from any thread.
  void EnablePolling(bool enabled) {
    polling_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &ThreadSafeGamepadChangeFetcher::EnablePollingOnPollingThread,
            make_scoped_refptr(this), enabled));
  }

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeGamepadChangeFetcher>;

  virtual ~ThreadSafeGamepadChangeFetcher() {}

  // Enables or disables polling.
  void EnablePollingOnPollingThread(bool enabled) {
    DCHECK(thread_checker_.CalledOnValidThread());
    is_enabled_ = enabled;

    if (is_enabled_) {
      if (!fetcher_) {
        fetcher_ = create_fetcher_callback_.Run();
        DCHECK(fetcher_);
      }
      SchedulePollOnPollingThread();
    } else {
      fetcher_.reset();
    }
  }

  // Schedules the next poll on the polling thread.
  void SchedulePollOnPollingThread() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(fetcher_);

    if (!is_enabled_ || has_poll_scheduled_)
      return;

    has_poll_scheduled_ = true;
    polling_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ThreadSafeGamepadChangeFetcher::PollOnPollingThread,
                   make_scoped_refptr(this)),
        base::TimeDelta::FromMilliseconds(kPollingTimeIntervalMs));
  }

  // Polls devices for new data and posts gamepad changes back to origin thread.
  void PollOnPollingThread() {
    DCHECK(thread_checker_.CalledOnValidThread());

    has_poll_scheduled_ = false;
    if (!is_enabled_)
      return;

    DCHECK(fetcher_);

    blink::WebGamepads new_state = state_;
    fetcher_->GetGamepadData(&new_state, false);

    if (std::max(new_state.length, state_.length) > 0) {
      if (new_state.items[0].connected != state_.items[0].connected ||
          new_state.items[0].timestamp > state_.items[0].timestamp) {
        origin_task_runner_->PostTask(
            FROM_HERE,
            base::Bind(process_gamepad_changes_, new_state.items[0]));
      }
    }

    state_ = new_state;
    SchedulePollOnPollingThread();
  }

  // Callback to ProcessGamepadChanges using weak reference to Gamepad.
  ProcessGamepadChangesCallback process_gamepad_changes_;

  // Callback method to create a gamepad data fetcher.
  CreateGamepadDataFetcherCallback create_fetcher_callback_;

  // Reference to task runner on polling thread.
  scoped_refptr<base::SingleThreadTaskRunner> polling_task_runner_;

  // Reference to task runner on origin thread.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  // Gamepad data fetcher used for querying gamepad devices.
  std::unique_ptr<device::GamepadDataFetcher> fetcher_;

  // The current state of all gamepads.
  blink::WebGamepads state_;

  // True if a poll has been scheduled.
  bool has_poll_scheduled_ = false;

  // True if the polling thread is paused.
  bool is_enabled_ = false;

  // ThreadChecker for the polling thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeGamepadChangeFetcher);
};

////////////////////////////////////////////////////////////////////////////////
// Gamepad, public:

Gamepad::Gamepad(GamepadDelegate* delegate,
                 base::SingleThreadTaskRunner* polling_task_runner)
    : Gamepad(delegate,
              polling_task_runner,
              base::Bind(CreateGamepadPlatformDataFetcher)) {}

Gamepad::Gamepad(GamepadDelegate* delegate,
                 base::SingleThreadTaskRunner* polling_task_runner,
                 CreateGamepadDataFetcherCallback create_fetcher_callback)
    : delegate_(delegate), weak_factory_(this) {
  gamepad_change_fetcher_ = new ThreadSafeGamepadChangeFetcher(
      base::Bind(&Gamepad::ProcessGamepadChanges, weak_factory_.GetWeakPtr()),
      create_fetcher_callback, polling_task_runner);

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
  OnWindowFocused(focus_client->GetFocusedWindow(), nullptr);
}

Gamepad::~Gamepad() {
  // Disable polling. Since ThreadSafeGamepadChangeFetcher are reference
  // counted, we can safely have it shut down after Gamepad has been destroyed.
  gamepad_change_fetcher_->EnablePolling(false);

  delegate_->OnGamepadDestroying(this);
  aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
      ->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void Gamepad::OnWindowFocused(aura::Window* gained_focus,
                              aura::Window* lost_focus) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Surface* target = nullptr;
  if (gained_focus) {
    target = Surface::AsSurface(gained_focus);
    if (!target) {
      aura::Window* top_level_window = gained_focus->GetToplevelWindow();
      if (top_level_window)
        target = ShellSurface::GetMainSurface(top_level_window);
    }
  }

  bool focused = target && delegate_->CanAcceptGamepadEventsForSurface(target);
  gamepad_change_fetcher_->EnablePolling(focused);
}

////////////////////////////////////////////////////////////////////////////////
// Gamepad, private:

void Gamepad::ProcessGamepadChanges(const blink::WebGamepad new_pad) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bool send_frame = false;

  // Update connection state.
  if (new_pad.connected != pad_state_.connected) {
    delegate_->OnStateChange(new_pad.connected);
  }

  if (!new_pad.connected || new_pad.timestamp <= pad_state_.timestamp) {
    pad_state_ = new_pad;
    return;
  }

  // Notify delegate of updated axes.
  for (size_t axis = 0;
       axis < std::max(pad_state_.axesLength, new_pad.axesLength); ++axis) {
    if (!GamepadButtonValuesAreEqual(new_pad.axes[axis],
                                     pad_state_.axes[axis])) {
      send_frame = true;
      delegate_->OnAxis(axis, new_pad.axes[axis]);
    }
  }

  // Notify delegate of updated buttons.
  for (size_t button_id = 0;
       button_id < std::max(pad_state_.buttonsLength, new_pad.buttonsLength);
       ++button_id) {
    auto button = pad_state_.buttons[button_id];
    auto new_button = new_pad.buttons[button_id];
    if (button.pressed != new_button.pressed ||
        !GamepadButtonValuesAreEqual(button.value, new_button.value)) {
      send_frame = true;
      delegate_->OnButton(button_id, new_button.pressed, new_button.value);
    }
  }
  if (send_frame)
    delegate_->OnFrame();

  pad_state_ = new_pad;
}

}  // namespace exo
