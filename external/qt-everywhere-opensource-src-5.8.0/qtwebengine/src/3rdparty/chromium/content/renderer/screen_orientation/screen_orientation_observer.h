// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/platform_event_observer.h"

namespace content {

// ScreenOrientationObserver is a helper class implementing the
// PlatformEventObserver template with blink::WebPlatformEventListener, which is
// a pure virtual class because it doesn't expect listeners.
// It is implemented on top of PlatformEventObserver to make sure it follows a
// common code path even though it only uses part of this code path: it is not
// expected to receive messages back but should send messages to start/stop
// listening.
// The intent of this class is to make sure that platforms that can't listen for
// display rotations at a marginal cost can be told when to actually do it.
class ScreenOrientationObserver
    : public PlatformEventObserver<blink::WebPlatformEventListener> {
 public:
  ScreenOrientationObserver();
  ~ScreenOrientationObserver() override;

  // Overriding this method just to make sure |listener| is always null.
  void Start(blink::WebPlatformEventListener* listener) override;

 protected:
  void SendStartMessage() override;
  void SendStopMessage() override;
};

}; // namespace content
