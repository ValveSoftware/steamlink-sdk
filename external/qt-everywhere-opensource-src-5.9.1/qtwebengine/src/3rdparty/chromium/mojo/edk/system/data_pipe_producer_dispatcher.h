// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/ports/port_ref.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

struct DataPipeControlMessage;
class NodeController;

// This is the Dispatcher implementation for the producer handle for data
// pipes created by the Mojo primitive MojoCreateDataPipe(). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeProducerDispatcher final
    : public Dispatcher {
 public:
  DataPipeProducerDispatcher(
      NodeController* node_controller,
      const ports::PortRef& port,
      scoped_refptr<PlatformSharedBuffer> shared_ring_buffer,
      const MojoCreateDataPipeOptions& options,
      bool initialized,
      uint64_t pipe_id);

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult Watch(MojoHandleSignals signals,
                   const Watcher::WatchCallback& callback,
                   uintptr_t context) override;
  MojoResult CancelWatch(uintptr_t context) override;
  MojoResult WriteData(const void* elements,
                       uint32_t* num_bytes,
                       MojoReadDataFlags flags) override;
  MojoResult BeginWriteData(void** buffer,
                            uint32_t* buffer_num_bytes,
                            MojoWriteDataFlags flags) override;
  MojoResult EndWriteData(uint32_t num_bytes_written) override;
  HandleSignalsState GetHandleSignalsState() const override;
  MojoResult AddAwakable(Awakable* awakable,
                         MojoHandleSignals signals,
                         uintptr_t context,
                         HandleSignalsState* signals_state) override;
  void RemoveAwakable(Awakable* awakable,
                      HandleSignalsState* signals_state) override;
  void StartSerialize(uint32_t* num_bytes,
                      uint32_t* num_ports,
                      uint32_t* num_handles) override;
  bool EndSerialize(void* destination,
                    ports::PortName* ports,
                    PlatformHandle* handles) override;
  bool BeginTransit() override;
  void CompleteTransitAndClose() override;
  void CancelTransit() override;

  static scoped_refptr<DataPipeProducerDispatcher>
  Deserialize(const void* data,
              size_t num_bytes,
              const ports::PortName* ports,
              size_t num_ports,
              PlatformHandle* handles,
              size_t num_handles);

 private:
  class PortObserverThunk;
  friend class PortObserverThunk;

  ~DataPipeProducerDispatcher() override;

  void OnSharedBufferCreated(const scoped_refptr<PlatformSharedBuffer>& buffer);
  void InitializeNoLock();
  MojoResult CloseNoLock();
  HandleSignalsState GetHandleSignalsStateNoLock() const;
  void NotifyWrite(uint32_t num_bytes);
  void OnPortStatusChanged();
  void UpdateSignalsStateNoLock();
  bool ProcessMessageNoLock(const DataPipeControlMessage& message,
                            ScopedPlatformHandleVectorPtr handles);

  const MojoCreateDataPipeOptions options_;
  NodeController* const node_controller_;
  const ports::PortRef control_port_;
  const uint64_t pipe_id_;

  // Guards access to the fields below.
  mutable base::Lock lock_;

  AwakableList awakable_list_;

  bool buffer_requested_ = false;

  scoped_refptr<PlatformSharedBuffer> shared_ring_buffer_;
  std::unique_ptr<PlatformSharedBufferMapping> ring_buffer_mapping_;
  ScopedPlatformHandle buffer_handle_for_transit_;

  bool in_transit_ = false;
  bool is_closed_ = false;
  bool peer_closed_ = false;
  bool transferred_ = false;
  bool in_two_phase_write_ = false;

  uint32_t write_offset_ = 0;
  uint32_t available_capacity_;

  DISALLOW_COPY_AND_ASSIGN(DataPipeProducerDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_
