// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_consumer_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/data_pipe_control_message.h"
#include "mojo/edk/system/node_controller.h"
#include "mojo/edk/system/ports_message.h"
#include "mojo/edk/system/request_context.h"
#include "mojo/public/c/system/data_pipe.h"

namespace mojo {
namespace edk {

namespace {

const uint8_t kFlagPeerClosed = 0x01;

#pragma pack(push, 1)

struct SerializedState {
  MojoCreateDataPipeOptions options;
  uint64_t pipe_id;
  uint32_t read_offset;
  uint32_t bytes_available;
  uint8_t flags;
  char padding[7];
};

static_assert(sizeof(SerializedState) % 8 == 0,
              "Invalid SerializedState size.");

#pragma pack(pop)

}  // namespace

// A PortObserver which forwards to a DataPipeConsumerDispatcher. This owns a
// reference to the dispatcher to ensure it lives as long as the observed port.
class DataPipeConsumerDispatcher::PortObserverThunk
    : public NodeController::PortObserver {
 public:
  explicit PortObserverThunk(
      scoped_refptr<DataPipeConsumerDispatcher> dispatcher)
      : dispatcher_(dispatcher) {}

 private:
  ~PortObserverThunk() override {}

  // NodeController::PortObserver:
  void OnPortStatusChanged() override { dispatcher_->OnPortStatusChanged(); }

  scoped_refptr<DataPipeConsumerDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PortObserverThunk);
};

DataPipeConsumerDispatcher::DataPipeConsumerDispatcher(
    NodeController* node_controller,
    const ports::PortRef& control_port,
    scoped_refptr<PlatformSharedBuffer> shared_ring_buffer,
    const MojoCreateDataPipeOptions& options,
    bool initialized,
    uint64_t pipe_id)
    : options_(options),
      node_controller_(node_controller),
      control_port_(control_port),
      pipe_id_(pipe_id),
      shared_ring_buffer_(shared_ring_buffer) {
  if (initialized) {
    base::AutoLock lock(lock_);
    InitializeNoLock();
  }
}

Dispatcher::Type DataPipeConsumerDispatcher::GetType() const {
  return Type::DATA_PIPE_CONSUMER;
}

MojoResult DataPipeConsumerDispatcher::Close() {
  base::AutoLock lock(lock_);
  DVLOG(1) << "Closing data pipe consumer " << pipe_id_;
  return CloseNoLock();
}


MojoResult DataPipeConsumerDispatcher::Watch(
    MojoHandleSignals signals,
    const Watcher::WatchCallback& callback,
    uintptr_t context) {
  base::AutoLock lock(lock_);

  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return awakable_list_.AddWatcher(
      signals, callback, context, GetHandleSignalsStateNoLock());
}

MojoResult DataPipeConsumerDispatcher::CancelWatch(uintptr_t context) {
  base::AutoLock lock(lock_);

  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return awakable_list_.RemoveWatcher(context);
}

MojoResult DataPipeConsumerDispatcher::ReadData(void* elements,
                                                uint32_t* num_bytes,
                                                MojoReadDataFlags flags) {
  base::AutoLock lock(lock_);
  if (!shared_ring_buffer_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  if ((flags & MOJO_READ_DATA_FLAG_QUERY)) {
    if ((flags & MOJO_READ_DATA_FLAG_PEEK) ||
        (flags & MOJO_READ_DATA_FLAG_DISCARD))
      return MOJO_RESULT_INVALID_ARGUMENT;
    DCHECK(!(flags & MOJO_READ_DATA_FLAG_DISCARD));  // Handled above.
    DVLOG_IF(2, elements)
        << "Query mode: ignoring non-null |elements|";
    *num_bytes = static_cast<uint32_t>(bytes_available_);
    return MOJO_RESULT_OK;
  }

  bool discard = false;
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD)) {
    // These flags are mutally exclusive.
    if (flags & MOJO_READ_DATA_FLAG_PEEK)
      return MOJO_RESULT_INVALID_ARGUMENT;
    DVLOG_IF(2, elements)
        << "Discard mode: ignoring non-null |elements|";
    discard = true;
  }

  uint32_t max_num_bytes_to_read = *num_bytes;
  if (max_num_bytes_to_read % options_.element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  bool all_or_none = flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  uint32_t min_num_bytes_to_read =
      all_or_none ? max_num_bytes_to_read : 0;

  if (min_num_bytes_to_read > bytes_available_) {
    return peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                        : MOJO_RESULT_OUT_OF_RANGE;
  }

  uint32_t bytes_to_read = std::min(max_num_bytes_to_read, bytes_available_);
  if (bytes_to_read == 0) {
    return peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                        : MOJO_RESULT_SHOULD_WAIT;
  }

  if (!discard) {
    uint8_t* data = static_cast<uint8_t*>(ring_buffer_mapping_->GetBase());
    CHECK(data);

    uint8_t* destination = static_cast<uint8_t*>(elements);
    CHECK(destination);

    DCHECK_LE(read_offset_, options_.capacity_num_bytes);
    uint32_t tail_bytes_to_copy =
        std::min(options_.capacity_num_bytes - read_offset_, bytes_to_read);
    uint32_t head_bytes_to_copy = bytes_to_read - tail_bytes_to_copy;
    if (tail_bytes_to_copy > 0)
      memcpy(destination, data + read_offset_, tail_bytes_to_copy);
    if (head_bytes_to_copy > 0)
      memcpy(destination + tail_bytes_to_copy, data, head_bytes_to_copy);
  }
  *num_bytes = bytes_to_read;

  bool peek = !!(flags & MOJO_READ_DATA_FLAG_PEEK);
  if (discard || !peek) {
    read_offset_ = (read_offset_ + bytes_to_read) % options_.capacity_num_bytes;
    bytes_available_ -= bytes_to_read;

    base::AutoUnlock unlock(lock_);
    NotifyRead(bytes_to_read);
  }

  return MOJO_RESULT_OK;
}

MojoResult DataPipeConsumerDispatcher::BeginReadData(const void** buffer,
                                                     uint32_t* buffer_num_bytes,
                                                     MojoReadDataFlags flags) {
  base::AutoLock lock(lock_);
  if (!shared_ring_buffer_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  // These flags may not be used in two-phase mode.
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD) ||
      (flags & MOJO_READ_DATA_FLAG_QUERY) ||
      (flags & MOJO_READ_DATA_FLAG_PEEK))
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (bytes_available_ == 0) {
    return peer_closed_ ? MOJO_RESULT_FAILED_PRECONDITION
                        : MOJO_RESULT_SHOULD_WAIT;
  }

  DCHECK_LT(read_offset_, options_.capacity_num_bytes);
  uint32_t bytes_to_read = std::min(bytes_available_,
                                    options_.capacity_num_bytes - read_offset_);

  CHECK(ring_buffer_mapping_);
  uint8_t* data = static_cast<uint8_t*>(ring_buffer_mapping_->GetBase());
  CHECK(data);

  in_two_phase_read_ = true;
  *buffer = data + read_offset_;
  *buffer_num_bytes = bytes_to_read;
  two_phase_max_bytes_read_ = bytes_to_read;

  return MOJO_RESULT_OK;
}

MojoResult DataPipeConsumerDispatcher::EndReadData(uint32_t num_bytes_read) {
  base::AutoLock lock(lock_);
  if (!in_two_phase_read_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  if (in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  CHECK(shared_ring_buffer_);

  HandleSignalsState old_state = GetHandleSignalsStateNoLock();
  MojoResult rv;
  if (num_bytes_read > two_phase_max_bytes_read_ ||
      num_bytes_read % options_.element_num_bytes != 0) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
  } else {
    rv = MOJO_RESULT_OK;
    read_offset_ =
        (read_offset_ + num_bytes_read) % options_.capacity_num_bytes;

    DCHECK_GE(bytes_available_, num_bytes_read);
    bytes_available_ -= num_bytes_read;

    base::AutoUnlock unlock(lock_);
    NotifyRead(num_bytes_read);
  }

  in_two_phase_read_ = false;
  two_phase_max_bytes_read_ = 0;

  HandleSignalsState new_state = GetHandleSignalsStateNoLock();
  if (!new_state.equals(old_state))
    awakable_list_.AwakeForStateChange(new_state);

  return rv;
}

HandleSignalsState DataPipeConsumerDispatcher::GetHandleSignalsState() const {
  base::AutoLock lock(lock_);
  return GetHandleSignalsStateNoLock();
}

MojoResult DataPipeConsumerDispatcher::AddAwakable(
    Awakable* awakable,
    MojoHandleSignals signals,
    uintptr_t context,
    HandleSignalsState* signals_state) {
  base::AutoLock lock(lock_);
  if (!shared_ring_buffer_ || in_transit_) {
    if (signals_state)
      *signals_state = HandleSignalsState();
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  UpdateSignalsStateNoLock();
  HandleSignalsState state = GetHandleSignalsStateNoLock();
  if (state.satisfies(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  awakable_list_.Add(awakable, signals, context);
  return MOJO_RESULT_OK;
}

void DataPipeConsumerDispatcher::RemoveAwakable(
    Awakable* awakable,
    HandleSignalsState* signals_state) {
  base::AutoLock lock(lock_);
  if ((!shared_ring_buffer_ || in_transit_) && signals_state)
    *signals_state = HandleSignalsState();
  else if (signals_state)
    *signals_state = GetHandleSignalsStateNoLock();
  awakable_list_.Remove(awakable);
}

void DataPipeConsumerDispatcher::StartSerialize(uint32_t* num_bytes,
                                                uint32_t* num_ports,
                                                uint32_t* num_handles) {
  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  *num_bytes = static_cast<uint32_t>(sizeof(SerializedState));
  *num_ports = 1;
  *num_handles = 1;
}

bool DataPipeConsumerDispatcher::EndSerialize(
    void* destination,
    ports::PortName* ports,
    PlatformHandle* platform_handles) {
  SerializedState* state = static_cast<SerializedState*>(destination);
  memcpy(&state->options, &options_, sizeof(MojoCreateDataPipeOptions));
  memset(state->padding, 0, sizeof(state->padding));

  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  state->pipe_id = pipe_id_;
  state->read_offset = read_offset_;
  state->bytes_available = bytes_available_;
  state->flags = peer_closed_ ? kFlagPeerClosed : 0;

  ports[0] = control_port_.name();

  buffer_handle_for_transit_ = shared_ring_buffer_->DuplicatePlatformHandle();
  platform_handles[0] = buffer_handle_for_transit_.get();

  return true;
}

bool DataPipeConsumerDispatcher::BeginTransit() {
  base::AutoLock lock(lock_);
  if (in_transit_)
    return false;
  in_transit_ = !in_two_phase_read_;
  return in_transit_;
}

void DataPipeConsumerDispatcher::CompleteTransitAndClose() {
  node_controller_->SetPortObserver(control_port_, nullptr);

  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  in_transit_ = false;
  transferred_ = true;
  ignore_result(buffer_handle_for_transit_.release());
  CloseNoLock();
}

void DataPipeConsumerDispatcher::CancelTransit() {
  base::AutoLock lock(lock_);
  DCHECK(in_transit_);
  in_transit_ = false;
  buffer_handle_for_transit_.reset();
  UpdateSignalsStateNoLock();
}

// static
scoped_refptr<DataPipeConsumerDispatcher>
DataPipeConsumerDispatcher::Deserialize(const void* data,
                                        size_t num_bytes,
                                        const ports::PortName* ports,
                                        size_t num_ports,
                                        PlatformHandle* handles,
                                        size_t num_handles) {
  if (num_ports != 1 || num_handles != 1 ||
      num_bytes != sizeof(SerializedState)) {
    return nullptr;
  }

  const SerializedState* state = static_cast<const SerializedState*>(data);

  NodeController* node_controller = internal::g_core->GetNodeController();
  ports::PortRef port;
  if (node_controller->node()->GetPort(ports[0], &port) != ports::OK)
    return nullptr;

  PlatformHandle buffer_handle;
  std::swap(buffer_handle, handles[0]);
  scoped_refptr<PlatformSharedBuffer> ring_buffer =
      PlatformSharedBuffer::CreateFromPlatformHandle(
          state->options.capacity_num_bytes,
          false /* read_only */,
          ScopedPlatformHandle(buffer_handle));
  if (!ring_buffer) {
    DLOG(ERROR) << "Failed to deserialize shared buffer handle.";
    return nullptr;
  }

  scoped_refptr<DataPipeConsumerDispatcher> dispatcher =
      new DataPipeConsumerDispatcher(node_controller, port, ring_buffer,
                                     state->options, false /* initialized */,
                                     state->pipe_id);

  {
    base::AutoLock lock(dispatcher->lock_);
    dispatcher->read_offset_ = state->read_offset;
    dispatcher->bytes_available_ = state->bytes_available;
    dispatcher->peer_closed_ = state->flags & kFlagPeerClosed;
    dispatcher->InitializeNoLock();
  }

  return dispatcher;
}

DataPipeConsumerDispatcher::~DataPipeConsumerDispatcher() {
  DCHECK(is_closed_ && !shared_ring_buffer_ && !ring_buffer_mapping_ &&
         !in_transit_);
}

void DataPipeConsumerDispatcher::InitializeNoLock() {
  lock_.AssertAcquired();

  if (shared_ring_buffer_) {
    DCHECK(!ring_buffer_mapping_);
    ring_buffer_mapping_ =
        shared_ring_buffer_->Map(0, options_.capacity_num_bytes);
    if (!ring_buffer_mapping_) {
      DLOG(ERROR) << "Failed to map shared buffer.";
      shared_ring_buffer_ = nullptr;
    }
  }

  base::AutoUnlock unlock(lock_);
  node_controller_->SetPortObserver(
      control_port_,
      make_scoped_refptr(new PortObserverThunk(this)));
}

MojoResult DataPipeConsumerDispatcher::CloseNoLock() {
  lock_.AssertAcquired();
  if (is_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  is_closed_ = true;
  ring_buffer_mapping_.reset();
  shared_ring_buffer_ = nullptr;

  awakable_list_.CancelAll();
  if (!transferred_) {
    base::AutoUnlock unlock(lock_);
    node_controller_->ClosePort(control_port_);
  }

  return MOJO_RESULT_OK;
}

HandleSignalsState
DataPipeConsumerDispatcher::GetHandleSignalsStateNoLock() const {
  lock_.AssertAcquired();

  HandleSignalsState rv;
  if (shared_ring_buffer_ && bytes_available_) {
    if (!in_two_phase_read_)
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  } else if (!peer_closed_ && shared_ring_buffer_) {
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }

  if (peer_closed_)
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

void DataPipeConsumerDispatcher::NotifyRead(uint32_t num_bytes) {
  DVLOG(1) << "Data pipe consumer " << pipe_id_ << " notifying peer: "
           << num_bytes << " bytes read. [control_port="
           << control_port_.name() << "]";

  SendDataPipeControlMessage(node_controller_, control_port_,
                             DataPipeCommand::DATA_WAS_READ, num_bytes);
}

void DataPipeConsumerDispatcher::OnPortStatusChanged() {
  DCHECK(RequestContext::current());

  base::AutoLock lock(lock_);

  // We stop observing the control port as soon it's transferred, but this can
  // race with events which are raised right before that happens. This is fine
  // to ignore.
  if (transferred_)
    return;

  DVLOG(1) << "Control port status changed for data pipe producer " << pipe_id_;

  UpdateSignalsStateNoLock();
}

void DataPipeConsumerDispatcher::UpdateSignalsStateNoLock() {
  lock_.AssertAcquired();

  bool was_peer_closed = peer_closed_;
  size_t previous_bytes_available = bytes_available_;

  ports::PortStatus port_status;
  int rv = node_controller_->node()->GetStatus(control_port_, &port_status);
  if (rv != ports::OK || !port_status.receiving_messages) {
    DVLOG(1) << "Data pipe consumer " << pipe_id_ << " is aware of peer closure"
             << " [control_port=" << control_port_.name() << "]";
    peer_closed_ = true;
  } else if (rv == ports::OK && port_status.has_messages && !in_transit_) {
    ports::ScopedMessage message;
    do {
      int rv = node_controller_->node()->GetMessageIf(control_port_, nullptr,
                                                      &message);
      if (rv != ports::OK)
        peer_closed_ = true;
      if (message) {
        if (message->num_payload_bytes() < sizeof(DataPipeControlMessage)) {
          peer_closed_ = true;
          break;
        }

        const DataPipeControlMessage* m =
            static_cast<const DataPipeControlMessage*>(
                message->payload_bytes());

        if (m->command != DataPipeCommand::DATA_WAS_WRITTEN) {
          DLOG(ERROR) << "Unexpected control message from producer.";
          peer_closed_ = true;
          break;
        }

        if (static_cast<size_t>(bytes_available_) + m->num_bytes >
              options_.capacity_num_bytes) {
          DLOG(ERROR) << "Producer claims to have written too many bytes.";
          peer_closed_ = true;
          break;
        }

        DVLOG(1) << "Data pipe consumer " << pipe_id_ << " is aware that "
                 << m->num_bytes << " bytes were written. [control_port="
                 << control_port_.name() << "]";

        bytes_available_ += m->num_bytes;
      }
    } while (message);
  }

  if (peer_closed_ != was_peer_closed ||
      bytes_available_ != previous_bytes_available) {
    awakable_list_.AwakeForStateChange(GetHandleSignalsStateNoLock());
  }
}

}  // namespace edk
}  // namespace mojo
