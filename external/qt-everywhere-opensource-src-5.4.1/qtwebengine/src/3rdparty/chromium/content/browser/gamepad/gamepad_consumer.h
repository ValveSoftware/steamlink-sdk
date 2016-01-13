// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_CONSUMER_H
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_CONSUMER_H

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGamepad.h"

namespace content {

class CONTENT_EXPORT GamepadConsumer {
 public:
  virtual void OnGamepadConnected(
      unsigned index,
      const blink::WebGamepad& gamepad) = 0;
  virtual void OnGamepadDisconnected(
      unsigned index,
      const blink::WebGamepad& gamepad) = 0;

 protected:
  virtual ~GamepadConsumer() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_CONSUMER_H
