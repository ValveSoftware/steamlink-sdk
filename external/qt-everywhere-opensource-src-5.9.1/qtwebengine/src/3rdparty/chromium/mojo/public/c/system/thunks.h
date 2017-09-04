// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_THUNKS_H_
#define MOJO_PUBLIC_C_SYSTEM_THUNKS_H_

#include <stddef.h>
#include <stdint.h>

#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/system_export.h"

// The embedder needs to bind the basic Mojo Core functions of a DSO to those of
// the embedder when loading a DSO that is dependent on mojo_system.
// The typical usage would look like:
// base::ScopedNativeLibrary app_library(
//     base::LoadNativeLibrary(app_path_, &error));
// typedef MojoResult (*MojoSetSystemThunksFn)(MojoSystemThunks*);
// MojoSetSystemThunksFn mojo_set_system_thunks_fn =
//     reinterpret_cast<MojoSetSystemThunksFn>(app_library.GetFunctionPointer(
//         "MojoSetSystemThunks"));
// MojoSystemThunks system_thunks = MojoMakeSystemThunks();
// size_t expected_size = mojo_set_system_thunks_fn(&system_thunks);
// if (expected_size > sizeof(MojoSystemThunks)) {
//   LOG(ERROR)
//       << "Invalid DSO. Expected MojoSystemThunks size: "
//       << expected_size;
//   break;
// }

// Structure used to bind the basic Mojo Core functions of a DSO to those of
// the embedder.
// This is the ABI between the embedder and the DSO. It can only have new
// functions added to the end. No other changes are supported.
#pragma pack(push, 8)
struct MojoSystemThunks {
  size_t size;  // Should be set to sizeof(MojoSystemThunks).
  MojoTimeTicks (*GetTimeTicksNow)();
  MojoResult (*Close)(MojoHandle handle);
  MojoResult (*Wait)(MojoHandle handle,
                     MojoHandleSignals signals,
                     MojoDeadline deadline,
                     struct MojoHandleSignalsState* signals_state);
  MojoResult (*WaitMany)(const MojoHandle* handles,
                         const MojoHandleSignals* signals,
                         uint32_t num_handles,
                         MojoDeadline deadline,
                         uint32_t* result_index,
                         struct MojoHandleSignalsState* signals_states);
  MojoResult (*CreateMessagePipe)(
      const struct MojoCreateMessagePipeOptions* options,
      MojoHandle* message_pipe_handle0,
      MojoHandle* message_pipe_handle1);
  MojoResult (*WriteMessage)(MojoHandle message_pipe_handle,
                             const void* bytes,
                             uint32_t num_bytes,
                             const MojoHandle* handles,
                             uint32_t num_handles,
                             MojoWriteMessageFlags flags);
  MojoResult (*ReadMessage)(MojoHandle message_pipe_handle,
                            void* bytes,
                            uint32_t* num_bytes,
                            MojoHandle* handles,
                            uint32_t* num_handles,
                            MojoReadMessageFlags flags);
  MojoResult (*CreateDataPipe)(const struct MojoCreateDataPipeOptions* options,
                               MojoHandle* data_pipe_producer_handle,
                               MojoHandle* data_pipe_consumer_handle);
  MojoResult (*WriteData)(MojoHandle data_pipe_producer_handle,
                          const void* elements,
                          uint32_t* num_elements,
                          MojoWriteDataFlags flags);
  MojoResult (*BeginWriteData)(MojoHandle data_pipe_producer_handle,
                               void** buffer,
                               uint32_t* buffer_num_elements,
                               MojoWriteDataFlags flags);
  MojoResult (*EndWriteData)(MojoHandle data_pipe_producer_handle,
                             uint32_t num_elements_written);
  MojoResult (*ReadData)(MojoHandle data_pipe_consumer_handle,
                         void* elements,
                         uint32_t* num_elements,
                         MojoReadDataFlags flags);
  MojoResult (*BeginReadData)(MojoHandle data_pipe_consumer_handle,
                              const void** buffer,
                              uint32_t* buffer_num_elements,
                              MojoReadDataFlags flags);
  MojoResult (*EndReadData)(MojoHandle data_pipe_consumer_handle,
                            uint32_t num_elements_read);
  MojoResult (*CreateSharedBuffer)(
      const struct MojoCreateSharedBufferOptions* options,
      uint64_t num_bytes,
      MojoHandle* shared_buffer_handle);
  MojoResult (*DuplicateBufferHandle)(
      MojoHandle buffer_handle,
      const struct MojoDuplicateBufferHandleOptions* options,
      MojoHandle* new_buffer_handle);
  MojoResult (*MapBuffer)(MojoHandle buffer_handle,
                          uint64_t offset,
                          uint64_t num_bytes,
                          void** buffer,
                          MojoMapBufferFlags flags);
  MojoResult (*UnmapBuffer)(void* buffer);

  MojoResult (*CreateWaitSet)(MojoHandle* wait_set);
  MojoResult (*AddHandle)(MojoHandle wait_set,
                          MojoHandle handle,
                          MojoHandleSignals signals);
  MojoResult (*RemoveHandle)(MojoHandle wait_set,
                             MojoHandle handle);
  MojoResult (*GetReadyHandles)(MojoHandle wait_set,
                                uint32_t* count,
                                MojoHandle* handles,
                                MojoResult* results,
                                struct MojoHandleSignalsState* signals_states);
  MojoResult (*Watch)(MojoHandle handle,
                      MojoHandleSignals signals,
                      MojoWatchCallback callback,
                      uintptr_t context);
  MojoResult (*CancelWatch)(MojoHandle handle, uintptr_t context);
  MojoResult (*FuseMessagePipes)(MojoHandle handle0, MojoHandle handle1);
  MojoResult (*WriteMessageNew)(MojoHandle message_pipe_handle,
                                MojoMessageHandle message,
                                MojoWriteMessageFlags flags);
  MojoResult (*ReadMessageNew)(MojoHandle message_pipe_handle,
                               MojoMessageHandle* message,
                               uint32_t* num_bytes,
                               MojoHandle* handles,
                               uint32_t* num_handles,
                               MojoReadMessageFlags flags);
  MojoResult (*AllocMessage)(uint32_t num_bytes,
                             const MojoHandle* handles,
                             uint32_t num_handles,
                             MojoAllocMessageFlags flags,
                             MojoMessageHandle* message);
  MojoResult (*FreeMessage)(MojoMessageHandle message);
  MojoResult (*GetMessageBuffer)(MojoMessageHandle message, void** buffer);
  MojoResult (*WrapPlatformHandle)(
      const struct MojoPlatformHandle* platform_handle,
      MojoHandle* mojo_handle);
  MojoResult (*UnwrapPlatformHandle)(
      MojoHandle mojo_handle,
      struct MojoPlatformHandle* platform_handle);
  MojoResult (*WrapPlatformSharedBufferHandle)(
      const struct MojoPlatformHandle* platform_handle,
      size_t num_bytes,
      MojoPlatformSharedBufferHandleFlags flags,
      MojoHandle* mojo_handle);
  MojoResult (*UnwrapPlatformSharedBufferHandle)(
      MojoHandle mojo_handle,
      struct MojoPlatformHandle* platform_handle,
      size_t* num_bytes,
      MojoPlatformSharedBufferHandleFlags* flags);
  MojoResult (*NotifyBadMessage)(MojoMessageHandle message,
                                 const char* error,
                                 size_t error_num_bytes);
  MojoResult (*GetProperty)(MojoPropertyType type, void* value);
};
#pragma pack(pop)

// Use this type for the function found by dynamically discovering it in
// a DSO linked with mojo_system. For example:
// MojoSetSystemThunksFn mojo_set_system_thunks_fn =
//     reinterpret_cast<MojoSetSystemThunksFn>(app_library.GetFunctionPointer(
//         "MojoSetSystemThunks"));
// The expected size of |system_thunks| is returned.
// The contents of |system_thunks| are copied.
typedef size_t (*MojoSetSystemThunksFn)(
    const struct MojoSystemThunks* system_thunks);

// A function for setting up the embedder's own system thunks. This should only
// be called by Mojo embedder code.
MOJO_SYSTEM_EXPORT size_t MojoEmbedderSetSystemThunks(
    const struct MojoSystemThunks* system_thunks);

#endif  // MOJO_PUBLIC_C_SYSTEM_THUNKS_H_
