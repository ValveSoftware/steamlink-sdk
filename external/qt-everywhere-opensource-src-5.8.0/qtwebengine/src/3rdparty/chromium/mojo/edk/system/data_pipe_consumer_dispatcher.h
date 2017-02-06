// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/ports/port_ref.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

struct DataPipeControlMessage;
class NodeController;

// This is the Dispatcher implementation for the consumer handle for data
// pipes created by the Mojo primitive MojoCreateDataPipe(). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeConsumerDispatcher final
    : public Dispatcher {
 public:
  DataPipeConsumerDispatcher(
      NodeController* node_controller,
      const ports::PortRef& control_port,
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
  MojoResult ReadData(void* elements,
                      uint32_t* num_bytes,
                      MojoReadDataFlags flags) override;
  MojoResult BeginReadData(const void** buffer,
                           uint32_t* buffer_num_bytes,
                           MojoReadDataFlags flags) override;
  MojoResult EndReadData(uint32_t num_bytes_read) override;
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

  static scoped_refptr<DataPipeConsumerDispatcher>
  Deserialize(const void* data,
              size_t num_bytes,
              const ports::PortName* ports,
              size_t num_ports,
              PlatformHandle* handles,
              size_t num_handles);

 private:
  class PortObserverThunk;
  friend class PortObserverThunk;

  ~DataPipeConsumerDispatcher() override;

  void InitializeNoLock();
  MojoResult CloseNoLock();
  HandleSignalsState GetHandleSignalsStateNoLock() const;
  void NotifyRead(uint32_t num_bytes);
  void OnPortStatusChanged();
  void UpdateSignalsStateNoLock();

  const MojoCreateDataPipeOptions options_;
  NodeController* const node_controller_;
  const ports::PortRef control_port_;
  const uint64_t pipe_id_;

  // Guards access to the fields below.
  mutable base::Lock lock_;

  AwakableList awakable_list_;

  scoped_refptr<PlatformSharedBuffer> shared_ring_buffer_;
  std::unique_ptr<PlatformSharedBufferMapping> ring_buffer_mapping_;
  ScopedPlatformHandle buffer_handle_for_transit_;

  bool in_two_phase_read_ = false;
  uint32_t two_phase_max_bytes_read_ = 0;

  bool in_transit_ = false;
  bool is_closed_ = false;
  bool peer_closed_ = false;
  bool transferred_ = false;

  uint32_t read_offset_ = 0;
  uint32_t bytes_available_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DataPipeConsumerDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
