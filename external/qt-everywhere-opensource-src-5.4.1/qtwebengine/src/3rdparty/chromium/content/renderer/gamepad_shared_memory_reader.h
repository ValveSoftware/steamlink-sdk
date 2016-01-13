// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
#define CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "content/common/gamepad_messages.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

struct GamepadHardwareBuffer;
class RendererWebKitPlatformSupportImpl;

class GamepadSharedMemoryReader
    : public RenderProcessObserver,
      public RendererGamepadProvider {
 public:
  GamepadSharedMemoryReader(
      RendererWebKitPlatformSupportImpl* webkit_platform_support);
  virtual ~GamepadSharedMemoryReader();

  // RendererGamepadProvider implementation.
  virtual void SampleGamepads(
      blink::WebGamepads& gamepads) OVERRIDE;
  virtual void SetGamepadListener(
      blink::WebGamepadListener* listener) OVERRIDE;

  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnGamepadConnected(int index, const blink::WebGamepad& gamepad);
  void OnGamepadDisconnected(int index, const blink::WebGamepad& gamepad);

  void StartPollingIfNecessary();
  void StopPollingIfNecessary();

  base::SharedMemoryHandle renderer_shared_memory_handle_;
  scoped_ptr<base::SharedMemory> renderer_shared_memory_;
  GamepadHardwareBuffer* gamepad_hardware_buffer_;
  blink::WebGamepadListener* gamepad_listener_;

  bool is_polling_;
  bool ever_interacted_with_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
