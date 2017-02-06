// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/gamepad_resource.h"

#include <string.h>

#include "base/bind.h"
#include "base/threading/platform_thread.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppb_gamepad_shared.h"

namespace ppapi {
namespace proxy {

namespace {

// This is the read logic from content/common/gamepad_seqlock.h
base::subtle::Atomic32 ReadBegin(const base::subtle::Atomic32* sequence) {
  base::subtle::Atomic32 version;
  for (;;) {
    version = base::subtle::NoBarrier_Load(sequence);

    // If the counter is even, then the associated data might be in a
    // consistent state, so we can try to read.
    if ((version & 1) == 0)
      break;

    // Otherwise, the writer is in the middle of an update. Retry the read.
    base::PlatformThread::YieldCurrentThread();
  }
  return version;
}

bool ReadRetry(const base::subtle::Atomic32* sequence,
               base::subtle::Atomic32 version) {
  // If the sequence number was updated then a read should be re-attempted.
  // -- Load fence, read membarrier
  return base::subtle::Release_Load(sequence) != version;
}

}  // namespace

GamepadResource::GamepadResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance),
      buffer_(NULL) {
  memset(&last_read_, 0, sizeof(last_read_));

  SendCreate(BROWSER, PpapiHostMsg_Gamepad_Create());
  Call<PpapiPluginMsg_Gamepad_SendMemory>(
      BROWSER,
      PpapiHostMsg_Gamepad_RequestMemory(),
      base::Bind(&GamepadResource::OnPluginMsgSendMemory, this));
}

GamepadResource::~GamepadResource() {
}

thunk::PPB_Gamepad_API* GamepadResource::AsPPB_Gamepad_API() {
  return this;
}

void GamepadResource::Sample(PP_Instance /* instance */,
                             PP_GamepadsSampleData* data) {
  if (!buffer_) {
    // Browser hasn't sent back our shared memory, give the plugin gamepad
    // data corresponding to "not connected".
    memset(data, 0, sizeof(PP_GamepadsSampleData));
    return;
  }

  // ==========
  //   DANGER
  // ==========
  //
  // This logic is duplicated in the renderer as well. If you change it, that
  // also needs to be in sync. See gamepad_shared_memory_reader.cc.

  // Only try to read this many times before failing to avoid waiting here
  // very long in case of contention with the writer.
  const int kMaximumContentionCount = 10;
  int contention_count = -1;
  base::subtle::Atomic32 version;
  WebKitGamepads read_into;
  do {
    version = ReadBegin(&buffer_->sequence);
    memcpy(&read_into, &buffer_->buffer, sizeof(read_into));
    ++contention_count;
    if (contention_count == kMaximumContentionCount)
      break;
  } while (ReadRetry(&buffer_->sequence, version));

  // In the event of a read failure, just leave the last read data as-is (the
  // hardware thread is taking unusally long).
  if (contention_count < kMaximumContentionCount)
    ConvertWebKitGamepadData(read_into, &last_read_);

  memcpy(data, &last_read_, sizeof(PP_GamepadsSampleData));
}

void GamepadResource::OnPluginMsgSendMemory(
    const ResourceMessageReplyParams& params) {
  // On failure, the handle will be null and the CHECK below will be tripped.
  base::SharedMemoryHandle handle = base::SharedMemory::NULLHandle();
  params.TakeSharedMemoryHandleAtIndex(0, &handle);

  shared_memory_.reset(new base::SharedMemory(handle, true));
  CHECK(shared_memory_->Map(sizeof(ContentGamepadHardwareBuffer)));
  buffer_ = static_cast<const ContentGamepadHardwareBuffer*>(
      shared_memory_->memory());
}

}  // namespace proxy
}  // namespace ppapi
