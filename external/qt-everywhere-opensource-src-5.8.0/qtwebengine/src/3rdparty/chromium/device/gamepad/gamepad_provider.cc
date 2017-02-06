// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_provider.h"

#include <stddef.h>
#include <string.h>
#include <cmath>
#include <set>
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
#include "device/gamepad/gamepad_platform_data_fetcher.h"
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
      gamepad_shared_buffer_(std::move(buffer)),
      connection_change_client_(connection_change_client) {
  Initialize(std::move(fetcher));
}

GamepadProvider::~GamepadProvider() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveDevicesChangedObserver(this);

  // Use Stop() to join the polling thread, as there may be pending callbacks
  // which dereference |polling_thread_|.
  polling_thread_->Stop();
  data_fetcher_.reset();
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

  pad_states_.reset(new PadState[WebGamepads::itemsLengthCap]);

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

  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GamepadProvider::DoInitializePollingThread,
                            base::Unretained(this), base::Passed(&fetcher)));
}

void GamepadProvider::DoInitializePollingThread(
    std::unique_ptr<GamepadDataFetcher> fetcher) {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(!data_fetcher_.get());  // Should only initialize once.

  if (!fetcher)
    fetcher.reset(new GamepadPlatformDataFetcher);
  data_fetcher_ = std::move(fetcher);
}

void GamepadProvider::SendPauseHint(bool paused) {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  if (data_fetcher_)
    data_fetcher_->PauseHint(paused);
}

bool GamepadProvider::PadState::Match(const WebGamepad& pad) const {
  return connected_ == pad.connected && axes_length_ == pad.axesLength &&
         buttons_length_ == pad.buttonsLength &&
         memcmp(id_, pad.id, sizeof(id_)) == 0 &&
         memcmp(mapping_, pad.mapping, sizeof(mapping_)) == 0;
}

void GamepadProvider::PadState::SetPad(const WebGamepad& pad) {
  connected_ = pad.connected;
  axes_length_ = pad.axesLength;
  buttons_length_ = pad.buttonsLength;
  memcpy(id_, pad.id, sizeof(id_));
  memcpy(mapping_, pad.mapping, sizeof(mapping_));
}

void GamepadProvider::PadState::SetDisconnected() {
  connected_ = false;
  axes_length_ = 0;
  buttons_length_ = 0;
  memset(id_, 0, sizeof(id_));
  memset(mapping_, 0, sizeof(mapping_));
}

void GamepadProvider::PadState::AsWebGamepad(WebGamepad* pad) {
  pad->connected = connected_;
  pad->axesLength = axes_length_;
  pad->buttonsLength = buttons_length_;
  memcpy(pad->id, id_, sizeof(id_));
  memcpy(pad->mapping, mapping_, sizeof(mapping_));
  memset(pad->axes, 0, sizeof(pad->axes));
  memset(pad->buttons, 0, sizeof(pad->buttons));
}

void GamepadProvider::DoPoll() {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
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

  {
    base::AutoLock lock(shared_memory_lock_);

    // Acquire the SeqLock. There is only ever one writer to this data.
    // See gamepad_hardware_buffer.h.
    gamepad_shared_buffer_->WriteBegin();
    data_fetcher_->GetGamepadData(gamepad_shared_buffer_->buffer(), changed);
    gamepad_shared_buffer_->WriteEnd();
  }

  if (ever_had_user_gesture_) {
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      WebGamepad& pad = gamepad_shared_buffer_->buffer()->items[i];
      PadState& state = pad_states_.get()[i];
      if (pad.connected && !state.connected()) {
        OnGamepadConnectionChange(true, i, pad);
      } else if (!pad.connected && state.connected()) {
        OnGamepadConnectionChange(false, i, pad);
      } else if (pad.connected && state.connected() && !state.Match(pad)) {
        WebGamepad old_pad;
        state.AsWebGamepad(&old_pad);
        OnGamepadConnectionChange(false, i, old_pad);
        OnGamepadConnectionChange(true, i, pad);
      }
    }
  }

  CheckForUserGesture();

  // Schedule our next interval of polling.
  ScheduleDoPoll();
}

void GamepadProvider::ScheduleDoPoll() {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
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
  PadState& state = pad_states_.get()[index];
  if (connected)
    state.SetPad(pad);
  else
    state.SetDisconnected();

  if (connection_change_client_)
    connection_change_client_->OnGamepadConnectionChange(connected, index, pad);
}

void GamepadProvider::CheckForUserGesture() {
  base::AutoLock lock(user_gesture_lock_);
  if (user_gesture_observers_.empty() && ever_had_user_gesture_)
    return;

  bool had_gesture_before = ever_had_user_gesture_;
  const WebGamepads* pads = gamepad_shared_buffer_->buffer();
  if (GamepadsHaveUserGesture(*pads)) {
    ever_had_user_gesture_ = true;
    for (size_t i = 0; i < user_gesture_observers_.size(); i++) {
      user_gesture_observers_[i].task_runner->PostTask(
          FROM_HERE, user_gesture_observers_[i].closure);
    }
    user_gesture_observers_.clear();
  }
  if (!had_gesture_before && ever_had_user_gesture_) {
    // Initialize pad_states_ for the first time.
    for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      pad_states_.get()[i].SetPad(pads->items[i]);
    }
  }
}

}  // namespace device
