// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_provider.h"

#include <stddef.h>
#include <string.h>
#include <cmath>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/gamepad/gamepad_user_gesture.h"

using blink::WebGamepad;
using blink::WebGamepads;

namespace device {

GamepadProvider::ClosureAndThread::ClosureAndThread(
    const base::Closure& c,
    const scoped_refptr<base::SingleThreadTaskRunner>& m)
    : closure(c), task_runner(m) {}

GamepadProvider::ClosureAndThread::ClosureAndThread(
    const ClosureAndThread& other) = default;

GamepadProvider::ClosureAndThread::~ClosureAndThread() {}

GamepadProvider::GamepadProvider(
    std::unique_ptr<GamepadSharedBuffer> buffer,
    GamepadConnectionChangeClient* connection_change_client)
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true),
      ever_had_user_gesture_(false),
      sanitize_(true),
      gamepad_shared_buffer_(std::move(buffer)),
      connection_change_client_(connection_change_client) {
  Initialize(std::unique_ptr<GamepadDataFetcher>());
}

GamepadProvider::GamepadProvider(
    std::unique_ptr<GamepadSharedBuffer> buffer,
    GamepadConnectionChangeClient* connection_change_client,
    std::unique_ptr<GamepadDataFetcher> fetcher)
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true),
      ever_had_user_gesture_(false),
      sanitize_(true),
      gamepad_shared_buffer_(std::move(buffer)),
      connection_change_client_(connection_change_client) {
  Initialize(std::move(fetcher));
}

GamepadProvider::~GamepadProvider() {
  GamepadDataFetcherManager::GetInstance()->ClearProvider();

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveDevicesChangedObserver(this);

  // Delete GamepadDataFetchers on |polling_thread_|. This is important because
  // some of them require their destructor to be called on the same sequence as
  // their other methods.
  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GamepadFetcherVector::clear,
                            base::Unretained(&data_fetchers_)));

  // Use Stop() to join the polling thread, as there may be pending callbacks
  // which dereference |polling_thread_|.
  polling_thread_->Stop();

  DCHECK(data_fetchers_.empty());
}

base::SharedMemoryHandle GamepadProvider::GetSharedMemoryHandleForProcess(
    base::ProcessHandle process) {
  base::SharedMemoryHandle renderer_handle;
  gamepad_shared_buffer_->shared_memory()->ShareToProcess(process,
                                                          &renderer_handle);
  return renderer_handle;
}

void GamepadProvider::GetCurrentGamepadData(WebGamepads* data) {
  const WebGamepads* pads = gamepad_shared_buffer_->buffer();
  base::AutoLock lock(shared_memory_lock_);
  *data = *pads;
}

void GamepadProvider::Pause() {
  {
    base::AutoLock lock(is_paused_lock_);
    is_paused_ = true;
  }
  base::MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::SendPauseHint, Unretained(this), true));
}

void GamepadProvider::Resume() {
  {
    base::AutoLock lock(is_paused_lock_);
    if (!is_paused_)
      return;
    is_paused_ = false;
  }

  base::MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::SendPauseHint, Unretained(this), false));
  polling_loop->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::ScheduleDoPoll, Unretained(this)));
}

void GamepadProvider::RegisterForUserGesture(const base::Closure& closure) {
  base::AutoLock lock(user_gesture_lock_);
  user_gesture_observers_.push_back(
      ClosureAndThread(closure, base::ThreadTaskRunnerHandle::Get()));
}

void GamepadProvider::OnDevicesChanged(base::SystemMonitor::DeviceType type) {
  base::AutoLock lock(devices_changed_lock_);
  devices_changed_ = true;
}

void GamepadProvider::Initialize(std::unique_ptr<GamepadDataFetcher> fetcher) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddDevicesChangedObserver(this);

  polling_thread_.reset(new base::Thread("Gamepad polling thread"));
#if defined(OS_LINUX)
  // On Linux, the data fetcher needs to watch file descriptors, so the message
  // loop needs to be a libevent loop.
  const base::MessageLoop::Type kMessageLoopType = base::MessageLoop::TYPE_IO;
#elif defined(OS_ANDROID)
  // On Android, keeping a message loop of default type.
  const base::MessageLoop::Type kMessageLoopType =
      base::MessageLoop::TYPE_DEFAULT;
#else
  // On Mac, the data fetcher uses IOKit which depends on CFRunLoop, so the
  // message loop needs to be a UI-type loop. On Windows it must be a UI loop
  // to properly pump the MessageWindow that captures device state.
  const base::MessageLoop::Type kMessageLoopType = base::MessageLoop::TYPE_UI;
#endif
  polling_thread_->StartWithOptions(base::Thread::Options(kMessageLoopType, 0));

  if (fetcher) {
    AddGamepadDataFetcher(std::move(fetcher));
  } else {
    GamepadDataFetcherManager::GetInstance()->InitializeProvider(this);
  }
}

void GamepadProvider::AddGamepadDataFetcher(
    std::unique_ptr<GamepadDataFetcher> fetcher) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GamepadProvider::DoAddGamepadDataFetcher,
                            base::Unretained(this), base::Passed(&fetcher)));
}

void GamepadProvider::RemoveSourceGamepadDataFetcher(GamepadSource source) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GamepadProvider::DoRemoveSourceGamepadDataFetcher,
                            base::Unretained(this), source));
}

void GamepadProvider::DoAddGamepadDataFetcher(
    std::unique_ptr<GamepadDataFetcher> fetcher) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());

  if (!fetcher)
    return;

  InitializeDataFetcher(fetcher.get());
  data_fetchers_.push_back(std::move(fetcher));
}

void GamepadProvider::DoRemoveSourceGamepadDataFetcher(GamepadSource source) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());

  for (GamepadFetcherVector::iterator it = data_fetchers_.begin();
       it != data_fetchers_.end();) {
    if ((*it)->source() == source) {
      it = data_fetchers_.erase(it);
    } else {
      ++it;
    }
  }
}

void GamepadProvider::SendPauseHint(bool paused) {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  for (const auto& it : data_fetchers_) {
    it->PauseHint(paused);
  }
}

void GamepadProvider::DoPoll() {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  DCHECK(have_scheduled_do_poll_);
  have_scheduled_do_poll_ = false;

  bool changed;

  ANNOTATE_BENIGN_RACE_SIZED(gamepad_shared_buffer_->buffer(),
                             sizeof(WebGamepads), "Racey reads are discarded");

  {
    base::AutoLock lock(devices_changed_lock_);
    changed = devices_changed_;
    devices_changed_ = false;
  }

  // Loop through each registered data fetcher and poll it's gamepad data.
  // It's expected that GetGamepadData will mark each gamepad as active (via
  // GetPadState). If a gamepad is not marked as active during the calls to
  // GetGamepadData then it's assumed to be disconnected.
  for (const auto& it : data_fetchers_) {
    it->GetGamepadData(changed);
  }

  blink::WebGamepads* buffer = gamepad_shared_buffer_->buffer();

  // Send out disconnect events using the last polled data before we wipe it out
  // in the mapping step.
  if (ever_had_user_gesture_) {
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];

      if (!state.active_state && state.source != GAMEPAD_SOURCE_NONE) {
        OnGamepadConnectionChange(false, i, buffer->items[i]);
        ClearPadState(state);
      }
    }
  }

  {
    base::AutoLock lock(shared_memory_lock_);

    // Acquire the SeqLock. There is only ever one writer to this data.
    // See gamepad_hardware_buffer.h.
    gamepad_shared_buffer_->WriteBegin();
    buffer->length = 0;
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];
      // Must run through the map+sanitize here or CheckForUserGesture may fail.
      MapAndSanitizeGamepadData(&state, &buffer->items[i], sanitize_);
      if (state.active_state)
        buffer->length++;
    }
    gamepad_shared_buffer_->WriteEnd();
  }

  if (ever_had_user_gesture_) {
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];

      if (state.active_state) {
        if (state.active_state == GAMEPAD_NEWLY_ACTIVE)
          OnGamepadConnectionChange(true, i, buffer->items[i]);
        state.active_state = GAMEPAD_INACTIVE;
      }
    }
  }

  CheckForUserGesture();

  // Schedule our next interval of polling.
  ScheduleDoPoll();
}

void GamepadProvider::ScheduleDoPoll() {
  DCHECK(polling_thread_->task_runner()->BelongsToCurrentThread());
  if (have_scheduled_do_poll_)
    return;

  {
    base::AutoLock lock(is_paused_lock_);
    if (is_paused_)
      return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&GamepadProvider::DoPoll, Unretained(this)),
      base::TimeDelta::FromMilliseconds(kDesiredSamplingIntervalMs));
  have_scheduled_do_poll_ = true;
}

void GamepadProvider::OnGamepadConnectionChange(bool connected,
                                                int index,
                                                const WebGamepad& pad) {
  if (connection_change_client_)
    connection_change_client_->OnGamepadConnectionChange(connected, index, pad);
}

void GamepadProvider::CheckForUserGesture() {
  base::AutoLock lock(user_gesture_lock_);
  if (user_gesture_observers_.empty() && ever_had_user_gesture_)
    return;

  const WebGamepads* pads = gamepad_shared_buffer_->buffer();
  if (GamepadsHaveUserGesture(*pads)) {
    ever_had_user_gesture_ = true;
    for (size_t i = 0; i < user_gesture_observers_.size(); i++) {
      user_gesture_observers_[i].task_runner->PostTask(
          FROM_HERE, user_gesture_observers_[i].closure);
    }
    user_gesture_observers_.clear();
  }
}

}  // namespace device
