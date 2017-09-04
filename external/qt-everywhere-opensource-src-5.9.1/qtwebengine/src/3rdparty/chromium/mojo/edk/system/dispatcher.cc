// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/dispatcher.h"

#include "base/logging.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/data_pipe_consumer_dispatcher.h"
#include "mojo/edk/system/data_pipe_producer_dispatcher.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"
#include "mojo/edk/system/shared_buffer_dispatcher.h"

namespace mojo {
namespace edk {

Dispatcher::DispatcherInTransit::DispatcherInTransit() {}

Dispatcher::DispatcherInTransit::DispatcherInTransit(
    const DispatcherInTransit& other) = default;

Dispatcher::DispatcherInTransit::~DispatcherInTransit() {}

MojoResult Dispatcher::Watch(MojoHandleSignals signals,
                             const Watcher::WatchCallback& callback,
                             uintptr_t context) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::CancelWatch(uintptr_t context) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::WriteMessage(std::unique_ptr<MessageForTransit> message,
                                    MojoWriteMessageFlags flags) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadMessage(std::unique_ptr<MessageForTransit>* message,
                                   uint32_t* num_bytes,
                                   MojoHandle* handles,
                                   uint32_t* num_handles,
                                   MojoReadMessageFlags flags,
                                   bool read_any_size) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::DuplicateBufferHandle(
    const MojoDuplicateBufferHandleOptions* options,
    scoped_refptr<Dispatcher>* new_dispatcher) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::MapBuffer(
    uint64_t offset,
    uint64_t num_bytes,
    MojoMapBufferFlags flags,
    std::unique_ptr<PlatformSharedBufferMapping>* mapping) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadData(void* elements,
                                uint32_t* num_bytes,
                                MojoReadDataFlags flags) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginReadData(const void** buffer,
                                     uint32_t* buffer_num_bytes,
                                     MojoReadDataFlags flags) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndReadData(uint32_t num_bytes_read) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::WriteData(const void* elements,
                                 uint32_t* num_bytes,
                                 MojoWriteDataFlags flags) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::BeginWriteData(void** buffer,
                                      uint32_t* buffer_num_bytes,
                                      MojoWriteDataFlags flags) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::EndWriteData(uint32_t num_bytes_written) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::AddWaitingDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher,
    MojoHandleSignals signals,
    uintptr_t context) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::RemoveWaitingDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::GetReadyDispatchers(uint32_t* count,
                                           DispatcherVector* dispatchers,
                                           MojoResult* results,
                                           uintptr_t* contexts) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

HandleSignalsState Dispatcher::GetHandleSignalsState() const {
  return HandleSignalsState();
}

MojoResult Dispatcher::AddAwakable(Awakable* awakable,
                                   MojoHandleSignals signals,
                                   uintptr_t context,
                                   HandleSignalsState* signals_state) {
  return MOJO_RESULT_INVALID_ARGUMENT;
}

void Dispatcher::RemoveAwakable(Awakable* awakable,
                                HandleSignalsState* handle_signals_state) {
  NOTREACHED();
}

void Dispatcher::StartSerialize(uint32_t* num_bytes,
                                uint32_t* num_ports,
                                uint32_t* num_platform_handles) {
  *num_bytes = 0;
  *num_ports = 0;
  *num_platform_handles = 0;
}

bool Dispatcher::EndSerialize(void* destination,
                              ports::PortName* ports,
                              PlatformHandle* handles) {
  LOG(ERROR) << "Attempting to serialize a non-transferrable dispatcher.";
  return true;
}

bool Dispatcher::BeginTransit() { return true; }

void Dispatcher::CompleteTransitAndClose() {}

void Dispatcher::CancelTransit() {}

// static
scoped_refptr<Dispatcher> Dispatcher::Deserialize(
    Type type,
    const void* bytes,
    size_t num_bytes,
    const ports::PortName* ports,
    size_t num_ports,
    PlatformHandle* platform_handles,
    size_t num_platform_handles) {
  switch (type) {
    case Type::MESSAGE_PIPE:
      return MessagePipeDispatcher::Deserialize(
          bytes, num_bytes, ports, num_ports, platform_handles,
          num_platform_handles);
    case Type::SHARED_BUFFER:
      return SharedBufferDispatcher::Deserialize(
          bytes, num_bytes, ports, num_ports, platform_handles,
          num_platform_handles);
    case Type::DATA_PIPE_CONSUMER:
      return DataPipeConsumerDispatcher::Deserialize(
          bytes, num_bytes, ports, num_ports, platform_handles,
          num_platform_handles);
    case Type::DATA_PIPE_PRODUCER:
      return DataPipeProducerDispatcher::Deserialize(
          bytes, num_bytes, ports, num_ports, platform_handles,
          num_platform_handles);
    case Type::PLATFORM_HANDLE:
      return PlatformHandleDispatcher::Deserialize(
          bytes, num_bytes, ports, num_ports, platform_handles,
          num_platform_handles);
    default:
      LOG(ERROR) << "Deserializing invalid dispatcher type.";
      return nullptr;
  }
}

Dispatcher::Dispatcher() {}

Dispatcher::~Dispatcher() {}

}  // namespace edk
}  // namespace mojo
