// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_platform_data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/browser/gamepad/gamepad_service.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "content/common/gamepad_messages.h"
#include "content/common/gamepad_user_gesture.h"
#include "content/public/browser/browser_thread.h"

using blink::WebGamepad;
using blink::WebGamepads;

namespace content {

GamepadProvider::ClosureAndThread::ClosureAndThread(
    const base::Closure& c,
    const scoped_refptr<base::MessageLoopProxy>& m)
    : closure(c),
      message_loop(m) {
}

GamepadProvider::ClosureAndThread::~ClosureAndThread() {
}

GamepadProvider::GamepadProvider()
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true),
      ever_had_user_gesture_(false) {
  Initialize(scoped_ptr<GamepadDataFetcher>());
}

GamepadProvider::GamepadProvider(scoped_ptr<GamepadDataFetcher> fetcher)
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true),
      ever_had_user_gesture_(false) {
  Initialize(fetcher.Pass());
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
  gamepad_shared_memory_.ShareToProcess(process, &renderer_handle);
  return renderer_handle;
}

void GamepadProvider::GetCurrentGamepadData(WebGamepads* data) {
  const WebGamepads& pads = SharedMemoryAsHardwareBuffer()->buffer;
  base::AutoLock lock(shared_memory_lock_);
  *data = pads;
}

void GamepadProvider::Pause() {
  {
    base::AutoLock lock(is_paused_lock_);
    is_paused_ = true;
  }
  base::MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(
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
  polling_loop->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::SendPauseHint, Unretained(this), false));
  polling_loop->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::ScheduleDoPoll, Unretained(this)));
}

void GamepadProvider::RegisterForUserGesture(const base::Closure& closure) {
  base::AutoLock lock(user_gesture_lock_);
  user_gesture_observers_.push_back(ClosureAndThread(
      closure, base::MessageLoop::current()->message_loop_proxy()));
}

void GamepadProvider::OnDevicesChanged(base::SystemMonitor::DeviceType type) {
  base::AutoLock lock(devices_changed_lock_);
  devices_changed_ = true;
}

void GamepadProvider::Initialize(scoped_ptr<GamepadDataFetcher> fetcher) {
  size_t data_size = sizeof(GamepadHardwareBuffer);
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddDevicesChangedObserver(this);
  bool res = gamepad_shared_memory_.CreateAndMapAnonymous(data_size);
  CHECK(res);
  GamepadHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();
  memset(hwbuf, 0, sizeof(GamepadHardwareBuffer));
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

  polling_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::DoInitializePollingThread,
                 base::Unretained(this),
                 base::Passed(&fetcher)));
}

void GamepadProvider::DoInitializePollingThread(
    scoped_ptr<GamepadDataFetcher> fetcher) {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(!data_fetcher_.get());  // Should only initialize once.

  if (!fetcher)
    fetcher.reset(new GamepadPlatformDataFetcher);
  data_fetcher_ = fetcher.Pass();
}

void GamepadProvider::SendPauseHint(bool paused) {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  if (data_fetcher_)
    data_fetcher_->PauseHint(paused);
}

bool GamepadProvider::PadState::Match(const WebGamepad& pad) const {
  return connected_ == pad.connected &&
         axes_length_ == pad.axesLength &&
         buttons_length_ == pad.buttonsLength &&
         memcmp(id_, pad.id, arraysize(id_)) == 0 &&
         memcmp(mapping_, pad.mapping, arraysize(mapping_)) == 0;
}

void GamepadProvider::PadState::SetPad(const WebGamepad& pad) {
  DCHECK(pad.connected);
  connected_ = true;
  axes_length_ = pad.axesLength;
  buttons_length_ = pad.buttonsLength;
  memcpy(id_, pad.id, arraysize(id_));
  memcpy(mapping_, pad.mapping, arraysize(mapping_));
}

void GamepadProvider::PadState::SetDisconnected() {
  connected_ = false;
  axes_length_ = 0;
  buttons_length_ = 0;
  memset(id_, 0, arraysize(id_));
  memset(mapping_, 0, arraysize(mapping_));
}

void GamepadProvider::PadState::AsWebGamepad(WebGamepad* pad) {
  pad->connected = connected_;
  pad->axesLength = axes_length_;
  pad->buttonsLength = buttons_length_;
  memcpy(pad->id, id_, arraysize(id_));
  memcpy(pad->mapping, mapping_, arraysize(mapping_));
  memset(pad->axes, 0, arraysize(pad->axes));
  memset(pad->buttons, 0, arraysize(pad->buttons));
}

void GamepadProvider::DoPoll() {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(have_scheduled_do_poll_);
  have_scheduled_do_poll_ = false;

  bool changed;
  GamepadHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();

  ANNOTATE_BENIGN_RACE_SIZED(
      &hwbuf->buffer,
      sizeof(WebGamepads),
      "Racey reads are discarded");

  {
    base::AutoLock lock(devices_changed_lock_);
    changed = devices_changed_;
    devices_changed_ = false;
  }

  {
    base::AutoLock lock(shared_memory_lock_);

    // Acquire the SeqLock. There is only ever one writer to this data.
    // See gamepad_hardware_buffer.h.
    hwbuf->sequence.WriteBegin();
    data_fetcher_->GetGamepadData(&hwbuf->buffer, changed);
    hwbuf->sequence.WriteEnd();
  }

  CheckForUserGesture();

  if (ever_had_user_gesture_) {
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      WebGamepad& pad = hwbuf->buffer.items[i];
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

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::DoPoll, Unretained(this)),
      base::TimeDelta::FromMilliseconds(kDesiredSamplingIntervalMs));
  have_scheduled_do_poll_ = true;
}

void GamepadProvider::OnGamepadConnectionChange(
    bool connected, int index, const WebGamepad& pad) {
  PadState& state = pad_states_.get()[index];
  if (connected)
    state.SetPad(pad);
  else
    state.SetDisconnected();

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GamepadProvider::DispatchGamepadConnectionChange,
                 base::Unretained(this),
                 connected,
                 index,
                 pad));
}

void GamepadProvider::DispatchGamepadConnectionChange(
    bool connected, int index, const WebGamepad& pad) {
  if (connected)
    GamepadService::GetInstance()->OnGamepadConnected(index, pad);
  else
    GamepadService::GetInstance()->OnGamepadDisconnected(index, pad);
}

GamepadHardwareBuffer* GamepadProvider::SharedMemoryAsHardwareBuffer() {
  void* mem = gamepad_shared_memory_.memory();
  CHECK(mem);
  return static_cast<GamepadHardwareBuffer*>(mem);
}

void GamepadProvider::CheckForUserGesture() {
  base::AutoLock lock(user_gesture_lock_);
  if (user_gesture_observers_.empty() && ever_had_user_gesture_)
    return;

  if (GamepadsHaveUserGesture(SharedMemoryAsHardwareBuffer()->buffer)) {
    ever_had_user_gesture_ = true;
    for (size_t i = 0; i < user_gesture_observers_.size(); i++) {
      user_gesture_observers_[i].message_loop->PostTask(FROM_HERE,
          user_gesture_observers_[i].closure);
    }
    user_gesture_observers_.clear();
  }
}

}  // namespace content
