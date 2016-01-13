// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/browser/gamepad/gamepad_consumer.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

GamepadService::GamepadService()
    : num_active_consumers_(0),
      gesture_callback_pending_(false) {
}

GamepadService::GamepadService(scoped_ptr<GamepadDataFetcher> fetcher)
    : provider_(new GamepadProvider(fetcher.Pass())),
      num_active_consumers_(0),
      gesture_callback_pending_(false) {
  thread_checker_.DetachFromThread();
}

GamepadService::~GamepadService() {
}

GamepadService* GamepadService::GetInstance() {
  return Singleton<GamepadService,
                   LeakySingletonTraits<GamepadService> >::get();
}

void GamepadService::ConsumerBecameActive(GamepadConsumer* consumer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!provider_)
    provider_.reset(new GamepadProvider);

  std::pair<ConsumerSet::iterator, bool> insert_result =
      consumers_.insert(consumer);
  insert_result.first->is_active = true;
  if (!insert_result.first->did_observe_user_gesture &&
      !gesture_callback_pending_) {
    provider_->RegisterForUserGesture(
          base::Bind(&GamepadService::OnUserGesture,
                     base::Unretained(this)));
  }

  if (num_active_consumers_++ == 0)
    provider_->Resume();
}

void GamepadService::ConsumerBecameInactive(GamepadConsumer* consumer) {
  DCHECK(provider_);
  DCHECK(num_active_consumers_ > 0);
  DCHECK(consumers_.count(consumer) > 0);
  DCHECK(consumers_.find(consumer)->is_active);

  consumers_.find(consumer)->is_active = false;
  if (--num_active_consumers_ == 0)
    provider_->Pause();
}

void GamepadService::RemoveConsumer(GamepadConsumer* consumer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ConsumerSet::iterator it = consumers_.find(consumer);
  if (it->is_active && --num_active_consumers_ == 0)
    provider_->Pause();
  consumers_.erase(it);
}

void GamepadService::RegisterForUserGesture(const base::Closure& closure) {
  DCHECK(consumers_.size() > 0);
  DCHECK(thread_checker_.CalledOnValidThread());
  provider_->RegisterForUserGesture(closure);
}

void GamepadService::Terminate() {
  provider_.reset();
}

void GamepadService::OnGamepadConnected(
    int index,
    const blink::WebGamepad& pad) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (ConsumerSet::iterator it = consumers_.begin();
       it != consumers_.end(); ++it) {
    if (it->did_observe_user_gesture && it->is_active)
      it->consumer->OnGamepadConnected(index, pad);
  }
}

void GamepadService::OnGamepadDisconnected(
    int index,
    const blink::WebGamepad& pad) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (ConsumerSet::iterator it = consumers_.begin();
       it != consumers_.end(); ++it) {
    if (it->did_observe_user_gesture && it->is_active)
      it->consumer->OnGamepadDisconnected(index, pad);
  }
}

base::SharedMemoryHandle GamepadService::GetSharedMemoryHandleForProcess(
    base::ProcessHandle handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return provider_->GetSharedMemoryHandleForProcess(handle);
}

void GamepadService::OnUserGesture() {
  DCHECK(thread_checker_.CalledOnValidThread());

  gesture_callback_pending_ = false;

  if (!provider_ ||
      num_active_consumers_ == 0)
    return;

  for (ConsumerSet::iterator it = consumers_.begin();
       it != consumers_.end(); ++it) {
    if (!it->did_observe_user_gesture && it->is_active) {
      const ConsumerInfo& info = *it;
      info.did_observe_user_gesture = true;
      blink::WebGamepads gamepads;
      provider_->GetCurrentGamepadData(&gamepads);
      for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; ++i) {
        const blink::WebGamepad& pad = gamepads.items[i];
        if (pad.connected)
          info.consumer->OnGamepadConnected(i, pad);
      }
    }
  }
}

}  // namespace content
