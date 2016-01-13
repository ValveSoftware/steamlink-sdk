// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "battery_status_dispatcher.h"

#include "base/logging.h"
#include "content/common/battery_status_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebBatteryStatusListener.h"

namespace content {

BatteryStatusDispatcher::BatteryStatusDispatcher(RenderThread* thread)
    : listener_(0) {
  if (thread)
    thread->AddObserver(this);
}

BatteryStatusDispatcher::~BatteryStatusDispatcher() {
  if (listener_)
    Stop();
}

bool BatteryStatusDispatcher::SetListener(
    blink::WebBatteryStatusListener* listener) {
  listener_ = listener;
  return listener ? Start() : Stop();
}

bool BatteryStatusDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BatteryStatusDispatcher, message)
    IPC_MESSAGE_HANDLER(BatteryStatusMsg_DidChange, OnDidChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool BatteryStatusDispatcher::Start() {
  return RenderThread::Get()->Send(new BatteryStatusHostMsg_Start());
}

bool BatteryStatusDispatcher::Stop() {
  return RenderThread::Get()->Send(new BatteryStatusHostMsg_Stop());
}

void BatteryStatusDispatcher::OnDidChange(
    const blink::WebBatteryStatus& status) {
  if (listener_)
    listener_->updateBatteryStatus(status);
}

}  // namespace content
