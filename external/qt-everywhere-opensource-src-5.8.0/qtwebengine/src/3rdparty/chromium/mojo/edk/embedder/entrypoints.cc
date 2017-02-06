// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/entrypoints.h"

#include <stdint.h>

#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/c/system/wait_set.h"

using mojo::edk::internal::g_core;

// Definitions of the system functions.
extern "C" {

MojoTimeTicks MojoGetTimeTicksNowImpl() {
  return g_core->GetTimeTicksNow();
}

MojoResult MojoCloseImpl(MojoHandle handle) {
  return g_core->Close(handle);
}

MojoResult MojoWaitImpl(MojoHandle handle,
                        MojoHandleSignals signals,
                        MojoDeadline deadline,
                        MojoHandleSignalsState* signals_state) {
  return g_core->Wait(handle, signals, deadline, signals_state);
}

MojoResult MojoWaitManyImpl(const MojoHandle* handles,
                            const MojoHandleSignals* signals,
                            uint32_t num_handles,
                            MojoDeadline deadline,
                            uint32_t* result_index,
                            MojoHandleSignalsState* signals_states) {
  return g_core->WaitMany(handles, signals, num_handles, deadline, result_index,
                          signals_states);
}

MojoResult MojoWatchImpl(MojoHandle handle,
                         MojoHandleSignals signals,
                         MojoWatchCallback callback,
                         uintptr_t context) {
  return g_core->Watch(handle, signals, callback, context);
}

MojoResult MojoCancelWatchImpl(MojoHandle handle, uintptr_t context) {
  return g_core->CancelWatch(handle, context);
}

MojoResult MojoAllocMessageImpl(uint32_t num_bytes,
                                const MojoHandle* handles,
                                uint32_t num_handles,
                                MojoAllocMessageFlags flags,
                                MojoMessageHandle* message) {
  return g_core->AllocMessage(num_bytes, handles, num_handles, flags, message);
}

MojoResult MojoFreeMessageImpl(MojoMessageHandle message) {
  return g_core->FreeMessage(message);
}

MojoResult MojoGetMessageBufferImpl(MojoMessageHandle message, void** buffer) {
  return g_core->GetMessageBuffer(message, buffer);
}

MojoResult MojoCreateWaitSetImpl(MojoHandle* wait_set_handle) {
  return g_core->CreateWaitSet(wait_set_handle);
}

MojoResult MojoAddHandleImpl(MojoHandle wait_set_handle,
                             MojoHandle handle,
                             MojoHandleSignals signals) {
  return g_core->AddHandle(wait_set_handle, handle, signals);
}

MojoResult MojoRemoveHandleImpl(MojoHandle wait_set_handle, MojoHandle handle) {
  return g_core->RemoveHandle(wait_set_handle, handle);
}

MojoResult MojoGetReadyHandlesImpl(
    MojoHandle wait_set_handle,
    uint32_t* count,
    MojoHandle* handles,
    MojoResult* results,
    struct MojoHandleSignalsState* signals_states) {
  return g_core->GetReadyHandles(wait_set_handle, count, handles, results,
                                 signals_states);
}

MojoResult MojoCreateMessagePipeImpl(
    const MojoCreateMessagePipeOptions* options,
    MojoHandle* message_pipe_handle0,
    MojoHandle* message_pipe_handle1) {
  return g_core->CreateMessagePipe(options, message_pipe_handle0,
                                   message_pipe_handle1);
}

MojoResult MojoWriteMessageImpl(MojoHandle message_pipe_handle,
                                const void* bytes,
                                uint32_t num_bytes,
                                const MojoHandle* handles,
                                uint32_t num_handles,
                                MojoWriteMessageFlags flags) {
  return g_core->WriteMessage(message_pipe_handle, bytes, num_bytes, handles,
                              num_handles, flags);
}

MojoResult MojoWriteMessageNewImpl(MojoHandle message_pipe_handle,
                                   MojoMessageHandle message,
                                   MojoWriteMessageFlags flags) {
  return g_core->WriteMessageNew(message_pipe_handle, message, flags);
}

MojoResult MojoReadMessageImpl(MojoHandle message_pipe_handle,
                               void* bytes,
                               uint32_t* num_bytes,
                               MojoHandle* handles,
                               uint32_t* num_handles,
                               MojoReadMessageFlags flags) {
  return g_core->ReadMessage(
      message_pipe_handle, bytes, num_bytes, handles, num_handles, flags);
}

MojoResult MojoReadMessageNewImpl(MojoHandle message_pipe_handle,
                                  MojoMessageHandle* message,
                                  uint32_t* num_bytes,
                                  MojoHandle* handles,
                                  uint32_t* num_handles,
                                  MojoReadMessageFlags flags) {
  return g_core->ReadMessageNew(
      message_pipe_handle, message, num_bytes, handles, num_handles, flags);
}

MojoResult MojoFuseMessagePipesImpl(MojoHandle handle0, MojoHandle handle1) {
  return g_core->FuseMessagePipes(handle0, handle1);
}

MojoResult MojoCreateDataPipeImpl(const MojoCreateDataPipeOptions* options,
                                  MojoHandle* data_pipe_producer_handle,
                                  MojoHandle* data_pipe_consumer_handle) {
  return g_core->CreateDataPipe(options, data_pipe_producer_handle,
                                data_pipe_consumer_handle);
}

MojoResult MojoWriteDataImpl(MojoHandle data_pipe_producer_handle,
                             const void* elements,
                             uint32_t* num_elements,
                             MojoWriteDataFlags flags) {
  return g_core->WriteData(data_pipe_producer_handle, elements, num_elements,
                           flags);
}

MojoResult MojoBeginWriteDataImpl(MojoHandle data_pipe_producer_handle,
                                  void** buffer,
                                  uint32_t* buffer_num_elements,
                                  MojoWriteDataFlags flags) {
  return g_core->BeginWriteData(data_pipe_producer_handle, buffer,
                                buffer_num_elements, flags);
}

MojoResult MojoEndWriteDataImpl(MojoHandle data_pipe_producer_handle,
                                uint32_t num_elements_written) {
  return g_core->EndWriteData(data_pipe_producer_handle, num_elements_written);
}

MojoResult MojoReadDataImpl(MojoHandle data_pipe_consumer_handle,
                            void* elements,
                            uint32_t* num_elements,
                            MojoReadDataFlags flags) {
  return g_core->ReadData(data_pipe_consumer_handle, elements, num_elements,
                          flags);
}

MojoResult MojoBeginReadDataImpl(MojoHandle data_pipe_consumer_handle,
                                 const void** buffer,
                                 uint32_t* buffer_num_elements,
                                 MojoReadDataFlags flags) {
  return g_core->BeginReadData(data_pipe_consumer_handle, buffer,
                               buffer_num_elements, flags);
}

MojoResult MojoEndReadDataImpl(MojoHandle data_pipe_consumer_handle,
                               uint32_t num_elements_read) {
  return g_core->EndReadData(data_pipe_consumer_handle, num_elements_read);
}

MojoResult MojoCreateSharedBufferImpl(
    const struct MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  return g_core->CreateSharedBuffer(options, num_bytes, shared_buffer_handle);
}

MojoResult MojoDuplicateBufferHandleImpl(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  return g_core->DuplicateBufferHandle(buffer_handle, options,
                                       new_buffer_handle);
}

MojoResult MojoMapBufferImpl(MojoHandle buffer_handle,
                             uint64_t offset,
                             uint64_t num_bytes,
                             void** buffer,
                             MojoMapBufferFlags flags) {
  return g_core->MapBuffer(buffer_handle, offset, num_bytes, buffer, flags);
}

MojoResult MojoUnmapBufferImpl(void* buffer) {
  return g_core->UnmapBuffer(buffer);
}

MojoResult MojoWrapPlatformHandleImpl(const MojoPlatformHandle* platform_handle,
                                      MojoHandle* mojo_handle) {
  return g_core->WrapPlatformHandle(platform_handle, mojo_handle);
}

MojoResult MojoUnwrapPlatformHandleImpl(MojoHandle mojo_handle,
                                        MojoPlatformHandle* platform_handle) {
  return g_core->UnwrapPlatformHandle(mojo_handle, platform_handle);
}

MojoResult MojoWrapPlatformSharedBufferHandleImpl(
    const MojoPlatformHandle* platform_handle,
    size_t num_bytes,
    MojoPlatformSharedBufferHandleFlags flags,
    MojoHandle* mojo_handle) {
  return g_core->WrapPlatformSharedBufferHandle(platform_handle, num_bytes,
                                                flags, mojo_handle);
}

MojoResult MojoUnwrapPlatformSharedBufferHandleImpl(
    MojoHandle mojo_handle,
    MojoPlatformHandle* platform_handle,
    size_t* num_bytes,
    MojoPlatformSharedBufferHandleFlags* flags) {
  return g_core->UnwrapPlatformSharedBufferHandle(mojo_handle, platform_handle,
                                                  num_bytes, flags);
}

MojoResult MojoNotifyBadMessageImpl(MojoMessageHandle message,
                                    const char* error,
                                    size_t error_num_bytes) {
  return g_core->NotifyBadMessage(message, error, error_num_bytes);
}

MojoResult MojoGetPropertyImpl(MojoPropertyType type, void* value) {
  return g_core->GetProperty(type, value);
}

}  // extern "C"

namespace mojo {
namespace edk {

MojoSystemThunks MakeSystemThunks() {
  MojoSystemThunks system_thunks = {sizeof(MojoSystemThunks),
                                    MojoGetTimeTicksNowImpl,
                                    MojoCloseImpl,
                                    MojoWaitImpl,
                                    MojoWaitManyImpl,
                                    MojoCreateMessagePipeImpl,
                                    MojoWriteMessageImpl,
                                    MojoReadMessageImpl,
                                    MojoCreateDataPipeImpl,
                                    MojoWriteDataImpl,
                                    MojoBeginWriteDataImpl,
                                    MojoEndWriteDataImpl,
                                    MojoReadDataImpl,
                                    MojoBeginReadDataImpl,
                                    MojoEndReadDataImpl,
                                    MojoCreateSharedBufferImpl,
                                    MojoDuplicateBufferHandleImpl,
                                    MojoMapBufferImpl,
                                    MojoUnmapBufferImpl,
                                    MojoCreateWaitSetImpl,
                                    MojoAddHandleImpl,
                                    MojoRemoveHandleImpl,
                                    MojoGetReadyHandlesImpl,
                                    MojoWatchImpl,
                                    MojoCancelWatchImpl,
                                    MojoFuseMessagePipesImpl,
                                    MojoWriteMessageNewImpl,
                                    MojoReadMessageNewImpl,
                                    MojoAllocMessageImpl,
                                    MojoFreeMessageImpl,
                                    MojoGetMessageBufferImpl,
                                    MojoWrapPlatformHandleImpl,
                                    MojoUnwrapPlatformHandleImpl,
                                    MojoWrapPlatformSharedBufferHandleImpl,
                                    MojoUnwrapPlatformSharedBufferHandleImpl,
                                    MojoNotifyBadMessageImpl,
                                    MojoGetPropertyImpl};
  return system_thunks;
}

}  // namespace edk
}  // namespace mojo
