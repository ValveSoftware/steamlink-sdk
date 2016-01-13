// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GAMEPAD_RENDERER_PROVIDER_H_
#define CONTENT_GAMEPAD_RENDERER_PROVIDER_H_

namespace blink {
class WebGamepadListener;
class WebGamepads;
}

namespace content {

// Provides gamepad data and events for blink.
class RendererGamepadProvider {
 public:
  // Provides latest snapshot of gamepads.
  virtual void SampleGamepads(blink::WebGamepads& gamepads) = 0;

  // Registers listener for be notified of events.
  virtual void SetGamepadListener(blink::WebGamepadListener* listener) = 0;

 protected:
  virtual ~RendererGamepadProvider() {}
};

} // namespace content

#endif
