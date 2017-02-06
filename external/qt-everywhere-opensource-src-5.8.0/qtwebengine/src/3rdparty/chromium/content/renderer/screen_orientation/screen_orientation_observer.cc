// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_observer.h"

#include "content/common/screen_orientation_messages.h"

namespace content {

ScreenOrientationObserver::ScreenOrientationObserver() {
}

ScreenOrientationObserver::~ScreenOrientationObserver() {
  StopIfObserving();
}

void ScreenOrientationObserver::Start(
    blink::WebPlatformEventListener* listener) {
  // This should never be called with a proper listener.
  CHECK(listener == 0);
  PlatformEventObserver<blink::WebPlatformEventListener>::Start(0);
}

void ScreenOrientationObserver::SendStartMessage() {
  RenderThread::Get()->Send(new ScreenOrientationHostMsg_StartListening());
}

void ScreenOrientationObserver::SendStopMessage() {
  RenderThread::Get()->Send(new ScreenOrientationHostMsg_StopListening());
}

} // namespace content
