// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_gamepad_host.h"

#include <stddef.h>
#include <string.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/gamepad/gamepad_service_test_helpers.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_test.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/gamepad_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/ppb_gamepad_shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class PepperGamepadHostTest : public testing::Test,
                              public BrowserPpapiHostTest {
 public:
  PepperGamepadHostTest() {}
  ~PepperGamepadHostTest() override {}

  void ConstructService(const blink::WebGamepads& test_data) {
    service_.reset(new GamepadServiceTestConstructor(test_data));
  }

  GamepadService* gamepad_service() { return service_->gamepad_service(); }

 protected:
  std::unique_ptr<GamepadServiceTestConstructor> service_;

  DISALLOW_COPY_AND_ASSIGN(PepperGamepadHostTest);
};

inline ptrdiff_t AddressDiff(const void* a, const void* b) {
  return static_cast<const char*>(a) - static_cast<const char*>(b);
}

}  // namespace

// Validate the memory layout of the Pepper proxy struct matches the content
// one. The proxy can't depend on content so has a duplicate definition. This
// code can see both definitions so we do the validation here.
TEST_F(PepperGamepadHostTest, ValidateHardwareBuffersMatch) {
  // Hardware buffer.
  static_assert(sizeof(ppapi::ContentGamepadHardwareBuffer) ==
                    sizeof(GamepadHardwareBuffer),
                "gamepad hardware buffers must match");
  ppapi::ContentGamepadHardwareBuffer ppapi_buf;
  GamepadHardwareBuffer content_buf;
  EXPECT_EQ(AddressDiff(&content_buf.sequence, &content_buf),
            AddressDiff(&ppapi_buf.sequence, &ppapi_buf));
  EXPECT_EQ(AddressDiff(&content_buf.buffer, &content_buf),
            AddressDiff(&ppapi_buf.buffer, &ppapi_buf));
}

TEST_F(PepperGamepadHostTest, ValidateGamepadsMatch) {
  // Gamepads.
  static_assert(sizeof(ppapi::WebKitGamepads) == sizeof(blink::WebGamepads),
                "gamepads data must match");
  ppapi::WebKitGamepads ppapi_gamepads;
  blink::WebGamepads web_gamepads;
  EXPECT_EQ(AddressDiff(&web_gamepads.length, &web_gamepads),
            AddressDiff(&ppapi_gamepads.length, &ppapi_gamepads));

  // See comment below on storage & the EXPECT macro.
  size_t webkit_items_length_cap = blink::WebGamepads::itemsLengthCap;
  size_t ppapi_items_length_cap = ppapi::WebKitGamepads::kItemsLengthCap;
  EXPECT_EQ(webkit_items_length_cap, ppapi_items_length_cap);

  for (size_t i = 0; i < web_gamepads.itemsLengthCap; i++) {
    EXPECT_EQ(AddressDiff(&web_gamepads.items[0], &web_gamepads),
              AddressDiff(&ppapi_gamepads.items[0], &ppapi_gamepads));
  }
}

TEST_F(PepperGamepadHostTest, ValidateGamepadMatch) {
  // Gamepad.
  static_assert(sizeof(ppapi::WebKitGamepad) == sizeof(blink::WebGamepad),
                "gamepad data must match");
  ppapi::WebKitGamepad ppapi_gamepad;
  blink::WebGamepad web_gamepad;

  // Using EXPECT seems to force storage for the parameter, which the constants
  // in the WebKit/PPAPI headers don't have. So we have to use temporaries
  // before comparing them.
  size_t webkit_id_length_cap = blink::WebGamepad::idLengthCap;
  size_t ppapi_id_length_cap = ppapi::WebKitGamepad::kIdLengthCap;
  EXPECT_EQ(webkit_id_length_cap, ppapi_id_length_cap);

  size_t webkit_axes_length_cap = blink::WebGamepad::axesLengthCap;
  size_t ppapi_axes_length_cap = ppapi::WebKitGamepad::kAxesLengthCap;
  EXPECT_EQ(webkit_axes_length_cap, ppapi_axes_length_cap);

  size_t webkit_buttons_length_cap = blink::WebGamepad::buttonsLengthCap;
  size_t ppapi_buttons_length_cap = ppapi::WebKitGamepad::kButtonsLengthCap;
  EXPECT_EQ(webkit_buttons_length_cap, ppapi_buttons_length_cap);

  EXPECT_EQ(AddressDiff(&web_gamepad.connected, &web_gamepad),
            AddressDiff(&ppapi_gamepad.connected, &ppapi_gamepad));
  EXPECT_EQ(AddressDiff(&web_gamepad.id, &web_gamepad),
            AddressDiff(&ppapi_gamepad.id, &ppapi_gamepad));
  EXPECT_EQ(AddressDiff(&web_gamepad.timestamp, &web_gamepad),
            AddressDiff(&ppapi_gamepad.timestamp, &ppapi_gamepad));
  EXPECT_EQ(AddressDiff(&web_gamepad.axesLength, &web_gamepad),
            AddressDiff(&ppapi_gamepad.axes_length, &ppapi_gamepad));
  EXPECT_EQ(AddressDiff(&web_gamepad.axes, &web_gamepad),
            AddressDiff(&ppapi_gamepad.axes, &ppapi_gamepad));
  EXPECT_EQ(AddressDiff(&web_gamepad.buttonsLength, &web_gamepad),
            AddressDiff(&ppapi_gamepad.buttons_length, &ppapi_gamepad));
  EXPECT_EQ(AddressDiff(&web_gamepad.buttons, &web_gamepad),
            AddressDiff(&ppapi_gamepad.buttons, &ppapi_gamepad));
}

// crbug.com/147549
#if defined(OS_ANDROID)
#define MAYBE_WaitForReply DISABLED_WaitForReply
#else
#define MAYBE_WaitForReply WaitForReply
#endif
TEST_F(PepperGamepadHostTest, MAYBE_WaitForReply) {
  blink::WebGamepads default_data;
  memset(&default_data, 0, sizeof(blink::WebGamepads));
  default_data.length = 1;
  default_data.items[0].connected = true;
  default_data.items[0].buttonsLength = 1;
  ConstructService(default_data);

  PP_Instance pp_instance = 12345;
  PP_Resource pp_resource = 67890;
  PepperGamepadHost gamepad_host(
      gamepad_service(), GetBrowserPpapiHost(), pp_instance, pp_resource);

  // Synthesize a request for gamepad data.
  ppapi::host::HostMessageContext context(
      ppapi::proxy::ResourceMessageCallParams(pp_resource, 1));
  EXPECT_EQ(PP_OK_COMPLETIONPENDING,
            gamepad_host.OnResourceMessageReceived(
                PpapiHostMsg_Gamepad_RequestMemory(), &context));

  device::MockGamepadDataFetcher* fetcher = service_->data_fetcher();
  fetcher->WaitForDataReadAndCallbacksIssued();

  // It should not have sent the callback message.
  service_->message_loop().RunUntilIdle();
  EXPECT_EQ(0u, sink().message_count());

  // Set a button down and wait for it to be read twice.
  blink::WebGamepads button_down_data = default_data;
  button_down_data.items[0].buttons[0].value = 1.f;
  button_down_data.items[0].buttons[0].pressed = true;
  fetcher->SetTestData(button_down_data);
  fetcher->WaitForDataReadAndCallbacksIssued();

  // It should have sent a callback.
  service_->message_loop().RunUntilIdle();
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message reply_msg;
  ASSERT_TRUE(sink().GetFirstResourceReplyMatching(
      PpapiPluginMsg_Gamepad_SendMemory::ID, &reply_params, &reply_msg));

  // Extract the shared memory handle.
  base::SharedMemoryHandle reply_handle;
  EXPECT_TRUE(reply_params.TakeSharedMemoryHandleAtIndex(0, &reply_handle));

  // Validate the shared memory.
  base::SharedMemory shared_memory(reply_handle, true);
  EXPECT_TRUE(shared_memory.Map(sizeof(ppapi::ContentGamepadHardwareBuffer)));
  const ppapi::ContentGamepadHardwareBuffer* buffer =
      static_cast<const ppapi::ContentGamepadHardwareBuffer*>(
          shared_memory.memory());
  EXPECT_EQ(button_down_data.length, buffer->buffer.length);
  EXPECT_EQ(button_down_data.items[0].buttonsLength,
            buffer->buffer.items[0].buttons_length);
  for (size_t i = 0; i < ppapi::WebKitGamepad::kButtonsLengthCap; i++) {
    EXPECT_EQ(button_down_data.items[0].buttons[i].value,
              buffer->buffer.items[0].buttons[i].value);
    EXPECT_EQ(button_down_data.items[0].buttons[i].pressed,
              buffer->buffer.items[0].buttons[i].pressed);
  }

  // Duplicate requests should be denied.
  EXPECT_EQ(PP_ERROR_FAILED,
            gamepad_host.OnResourceMessageReceived(
                PpapiHostMsg_Gamepad_RequestMemory(), &context));
}

}  // namespace content
