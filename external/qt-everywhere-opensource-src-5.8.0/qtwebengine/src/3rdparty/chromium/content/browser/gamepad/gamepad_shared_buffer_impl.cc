// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_shared_buffer_impl.h"

#include "content/common/gamepad_hardware_buffer.h"

namespace content {

GamepadSharedBufferImpl::GamepadSharedBufferImpl() {
  size_t data_size = sizeof(GamepadHardwareBuffer);
  bool res = shared_memory_.CreateAndMapAnonymous(data_size);
  CHECK(res);

  void* mem = shared_memory_.memory();
  DCHECK(mem);
  new (mem) GamepadHardwareBuffer();
}

GamepadSharedBufferImpl::~GamepadSharedBufferImpl() {
}

base::SharedMemory* GamepadSharedBufferImpl::shared_memory() {
  return &shared_memory_;
}

blink::WebGamepads* GamepadSharedBufferImpl::buffer() {
  return &(hardware_buffer()->buffer);
}

GamepadHardwareBuffer* GamepadSharedBufferImpl::hardware_buffer() {
  void* mem = shared_memory_.memory();
  DCHECK(mem);
  return static_cast<GamepadHardwareBuffer*>(mem);
}

void GamepadSharedBufferImpl::WriteBegin() {
  hardware_buffer()->sequence.WriteBegin();
}

void GamepadSharedBufferImpl::WriteEnd() {
  hardware_buffer()->sequence.WriteEnd();
}

}  // namespace content
