// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_SHARED_BUFFER_IMPL_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_SHARED_BUFFER_IMPL_H_

#include "device/gamepad/gamepad_shared_buffer.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

struct GamepadHardwareBuffer;

class GamepadSharedBufferImpl : public device::GamepadSharedBuffer {
 public:
  GamepadSharedBufferImpl();
  ~GamepadSharedBufferImpl() override;

  base::SharedMemory* shared_memory() override;
  blink::WebGamepads* buffer() override;
  GamepadHardwareBuffer* hardware_buffer();

  void WriteBegin() override;
  void WriteEnd() override;

 private:
  base::SharedMemory shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(GamepadSharedBufferImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_SHARED_BUFFER_IMPL_H_
