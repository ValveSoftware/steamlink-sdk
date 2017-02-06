// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SHARED_BUFFER_H_
#define DEVICE_GAMEPAD_SHARED_BUFFER_H_

#include "base/memory/shared_memory.h"
#include "device/gamepad/gamepad_export.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace device {

class DEVICE_GAMEPAD_EXPORT GamepadSharedBuffer {
 public:
  virtual ~GamepadSharedBuffer() {}

  virtual base::SharedMemory* shared_memory() = 0;
  virtual blink::WebGamepads* buffer() = 0;

  virtual void WriteBegin() = 0;
  virtual void WriteEnd() = 0;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SHARED_BUFFER_H_
