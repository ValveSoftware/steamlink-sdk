// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_message_filter.h"

#include "content/common/battery_status_messages.h"

namespace content {

BatteryStatusMessageFilter::BatteryStatusMessageFilter()
    : BrowserMessageFilter(BatteryStatusMsgStart),
      is_started_(false) {
  callback_ = base::Bind(&BatteryStatusMessageFilter::SendBatteryChange,
                         base::Unretained(this));
}

BatteryStatusMessageFilter::~BatteryStatusMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (is_started_)
    subscription_.reset();
}

bool BatteryStatusMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BatteryStatusMessageFilter, message)
    IPC_MESSAGE_HANDLER(BatteryStatusHostMsg_Start, OnBatteryStatusStart)
    IPC_MESSAGE_HANDLER(BatteryStatusHostMsg_Stop, OnBatteryStatusStop)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BatteryStatusMessageFilter::OnBatteryStatusStart() {
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  subscription_ = BatteryStatusService::GetInstance()->AddCallback(callback_);
}

void BatteryStatusMessageFilter::OnBatteryStatusStop() {
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  subscription_.reset();
}

void BatteryStatusMessageFilter::SendBatteryChange(
    const blink::WebBatteryStatus& status) {
  Send(new BatteryStatusMsg_DidChange(status));
}

}  // namespace content
