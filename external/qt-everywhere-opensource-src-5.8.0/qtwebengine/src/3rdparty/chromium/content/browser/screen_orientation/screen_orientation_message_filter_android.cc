// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_message_filter_android.h"

#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"
#include "content/common/screen_orientation_messages.h"

namespace content {

ScreenOrientationMessageFilterAndroid::ScreenOrientationMessageFilterAndroid()
    : BrowserMessageFilter(ScreenOrientationMsgStart)
    , listeners_count_(0) {
}

ScreenOrientationMessageFilterAndroid::~ScreenOrientationMessageFilterAndroid()
{
  if (listeners_count_ > 0)
    ScreenOrientationDelegateAndroid::StopAccurateListening();
}

bool ScreenOrientationMessageFilterAndroid::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ScreenOrientationMessageFilterAndroid, message)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_StartListening,
                        OnStartListening)
    IPC_MESSAGE_HANDLER(ScreenOrientationHostMsg_StopListening,
                        OnStopListening)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ScreenOrientationMessageFilterAndroid::OnStartListening() {
  ++listeners_count_;
  if (listeners_count_ == 1)
    ScreenOrientationDelegateAndroid::StartAccurateListening();
}

void ScreenOrientationMessageFilterAndroid::OnStopListening() {
  DCHECK(listeners_count_ > 0);
  --listeners_count_;
  if (listeners_count_ == 0)
    ScreenOrientationDelegateAndroid::StopAccurateListening();
}

}  // namespace content
