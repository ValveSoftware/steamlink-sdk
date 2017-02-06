// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
#define CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "content/common/gamepad_messages.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

struct GamepadHardwareBuffer;

class GamepadSharedMemoryReader : public RendererGamepadProvider {
 public:
  explicit GamepadSharedMemoryReader(RenderThread* thread);
  ~GamepadSharedMemoryReader() override;

  // RendererGamepadProvider implementation.
  void SampleGamepads(blink::WebGamepads& gamepads) override;
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void Start(blink::WebPlatformEventListener* listener) override;

 protected:
  // PlatformEventObserver protected methods.
  void SendStartMessage() override;
  void SendStopMessage() override;

 private:
  void OnGamepadConnected(int index, const blink::WebGamepad& gamepad);
  void OnGamepadDisconnected(int index, const blink::WebGamepad& gamepad);

  base::SharedMemoryHandle renderer_shared_memory_handle_;
  std::unique_ptr<base::SharedMemory> renderer_shared_memory_;
  GamepadHardwareBuffer* gamepad_hardware_buffer_;

  bool ever_interacted_with_;

  DISALLOW_COPY_AND_ASSIGN(GamepadSharedMemoryReader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
