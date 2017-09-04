// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gamepad_browser_message_filter.h"

#include "content/browser/gamepad/gamepad_service.h"
#include "content/common/gamepad_messages.h"

namespace content {

GamepadBrowserMessageFilter::GamepadBrowserMessageFilter()
    : BrowserMessageFilter(GamepadMsgStart),
      is_started_(false) {
}

GamepadBrowserMessageFilter::~GamepadBrowserMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_started_)
    GamepadService::GetInstance()->RemoveConsumer(this);
}

bool GamepadBrowserMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GamepadBrowserMessageFilter, message)
    IPC_MESSAGE_HANDLER(GamepadHostMsg_StartPolling, OnGamepadStartPolling)
    IPC_MESSAGE_HANDLER(GamepadHostMsg_StopPolling, OnGamepadStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GamepadBrowserMessageFilter::OnGamepadConnected(
    unsigned index,
    const blink::WebGamepad& gamepad) {
  Send(new GamepadMsg_GamepadConnected(index, gamepad));
}

void GamepadBrowserMessageFilter::OnGamepadDisconnected(
    unsigned index,
    const blink::WebGamepad& gamepad) {
  Send(new GamepadMsg_GamepadDisconnected(index, gamepad));
}

void GamepadBrowserMessageFilter::OnGamepadStartPolling(
    base::SharedMemoryHandle* renderer_handle) {
  GamepadService* service = GamepadService::GetInstance();
  DCHECK(!is_started_);
  if (is_started_)
    return;

  is_started_ = true;
  service->ConsumerBecameActive(this);
  *renderer_handle = service->GetSharedMemoryHandleForProcess(PeerHandle());
}

void GamepadBrowserMessageFilter::OnGamepadStopPolling() {
  DCHECK(is_started_);
  if (!is_started_)
    return;

  is_started_ = false;
  GamepadService::GetInstance()->ConsumerBecameInactive(this);
}

}  // namespace content
