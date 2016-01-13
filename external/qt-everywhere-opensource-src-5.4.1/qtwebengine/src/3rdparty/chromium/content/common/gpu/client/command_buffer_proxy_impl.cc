// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/command_buffer_proxy_impl.h"

#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/stl_util.h"
#include "content/common/child_process_messages.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"
#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "ui/gfx/size.h"

namespace content {

CommandBufferProxyImpl::CommandBufferProxyImpl(
    GpuChannelHost* channel,
    int route_id)
    : channel_(channel),
      route_id_(route_id),
      flush_count_(0),
      last_put_offset_(-1),
      next_signal_id_(0) {
}

CommandBufferProxyImpl::~CommandBufferProxyImpl() {
  FOR_EACH_OBSERVER(DeletionObserver,
                    deletion_observers_,
                    OnWillDeleteImpl());
}

bool CommandBufferProxyImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CommandBufferProxyImpl, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Destroyed, OnDestroyed);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_EchoAck, OnEchoAck);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ConsoleMsg, OnConsoleMessage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetMemoryAllocation,
                        OnSetMemoryAllocation);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncPointAck,
                        OnSignalSyncPointAck);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void CommandBufferProxyImpl::OnChannelError() {
  OnDestroyed(gpu::error::kUnknown);
}

void CommandBufferProxyImpl::OnDestroyed(gpu::error::ContextLostReason reason) {
  // Prevent any further messages from being sent.
  channel_ = NULL;

  // When the client sees that the context is lost, they should delete this
  // CommandBufferProxyImpl and create a new one.
  last_state_.error = gpu::error::kLostContext;
  last_state_.context_lost_reason = reason;

  if (!channel_error_callback_.is_null()) {
    channel_error_callback_.Run();
    // Avoid calling the error callback more than once.
    channel_error_callback_.Reset();
  }
}

void CommandBufferProxyImpl::OnEchoAck() {
  DCHECK(!echo_tasks_.empty());
  base::Closure callback = echo_tasks_.front();
  echo_tasks_.pop();
  callback.Run();
}

void CommandBufferProxyImpl::OnConsoleMessage(
    const GPUCommandBufferConsoleMessage& message) {
  if (!console_message_callback_.is_null()) {
    console_message_callback_.Run(message.message, message.id);
  }
}

void CommandBufferProxyImpl::SetMemoryAllocationChangedCallback(
    const MemoryAllocationChangedCallback& callback) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  memory_allocation_changed_callback_ = callback;
  Send(new GpuCommandBufferMsg_SetClientHasMemoryAllocationChangedCallback(
      route_id_, !memory_allocation_changed_callback_.is_null()));
}

void CommandBufferProxyImpl::AddDeletionObserver(DeletionObserver* observer) {
  deletion_observers_.AddObserver(observer);
}

void CommandBufferProxyImpl::RemoveDeletionObserver(
    DeletionObserver* observer) {
  deletion_observers_.RemoveObserver(observer);
}

void CommandBufferProxyImpl::OnSetMemoryAllocation(
    const gpu::MemoryAllocation& allocation) {
  if (!memory_allocation_changed_callback_.is_null())
    memory_allocation_changed_callback_.Run(allocation);
}

void CommandBufferProxyImpl::OnSignalSyncPointAck(uint32 id) {
  SignalTaskMap::iterator it = signal_tasks_.find(id);
  DCHECK(it != signal_tasks_.end());
  base::Closure callback = it->second;
  signal_tasks_.erase(it);
  callback.Run();
}

void CommandBufferProxyImpl::SetChannelErrorCallback(
    const base::Closure& callback) {
  channel_error_callback_ = callback;
}

bool CommandBufferProxyImpl::Initialize() {
  TRACE_EVENT0("gpu", "CommandBufferProxyImpl::Initialize");
  shared_state_shm_.reset(channel_->factory()->AllocateSharedMemory(
      sizeof(*shared_state())).release());
  if (!shared_state_shm_)
    return false;

  if (!shared_state_shm_->Map(sizeof(*shared_state())))
    return false;

  shared_state()->Initialize();

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the Initialize IPC below.
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(shared_state_shm_->handle());
  if (!base::SharedMemory::IsHandleValid(handle))
    return false;

  bool result;
  if (!Send(new GpuCommandBufferMsg_Initialize(
      route_id_, handle, &result, &capabilities_))) {
    LOG(ERROR) << "Could not send GpuCommandBufferMsg_Initialize.";
    return false;
  }

  if (!result) {
    LOG(ERROR) << "Failed to initialize command buffer service.";
    return false;
  }

  capabilities_.map_image = true;

  return true;
}

gpu::CommandBuffer::State CommandBufferProxyImpl::GetLastState() {
  return last_state_;
}

int32 CommandBufferProxyImpl::GetLastToken() {
  TryUpdateState();
  return last_state_.token;
}

void CommandBufferProxyImpl::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu",
               "CommandBufferProxyImpl::Flush",
               "put_offset",
               put_offset);

  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;

  Send(new GpuCommandBufferMsg_AsyncFlush(route_id_,
                                          put_offset,
                                          ++flush_count_));
}

void CommandBufferProxyImpl::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  if (last_state_.error != gpu::error::kNoError)
    return;
  Send(new GpuCommandBufferMsg_SetLatencyInfo(route_id_, latency_info));
}

void CommandBufferProxyImpl::WaitForTokenInRange(int32 start, int32 end) {
  TRACE_EVENT2("gpu",
               "CommandBufferProxyImpl::WaitForToken",
               "start",
               start,
               "end",
               end);
  TryUpdateState();
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForTokenInRange(
            route_id_, start, end, &state)))
      OnUpdateState(state);
  }
  DCHECK(InRange(start, end, last_state_.token) ||
         last_state_.error != gpu::error::kNoError);
}

void CommandBufferProxyImpl::WaitForGetOffsetInRange(int32 start, int32 end) {
  TRACE_EVENT2("gpu",
               "CommandBufferProxyImpl::WaitForGetOffset",
               "start",
               start,
               "end",
               end);
  TryUpdateState();
  if (!InRange(start, end, last_state_.get_offset) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForGetOffsetInRange(
            route_id_, start, end, &state)))
      OnUpdateState(state);
  }
  DCHECK(InRange(start, end, last_state_.get_offset) ||
         last_state_.error != gpu::error::kNoError);
}

void CommandBufferProxyImpl::SetGetBuffer(int32 shm_id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetGetBuffer(route_id_, shm_id));
  last_put_offset_ = -1;
}

scoped_refptr<gpu::Buffer> CommandBufferProxyImpl::CreateTransferBuffer(
    size_t size,
    int32* id) {
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return NULL;

  int32 new_id = channel_->ReserveTransferBufferId();

  scoped_ptr<base::SharedMemory> shared_memory(
      channel_->factory()->AllocateSharedMemory(size));
  if (!shared_memory)
    return NULL;

  DCHECK(!shared_memory->memory());
  if (!shared_memory->Map(size))
    return NULL;

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the RegisterTransferBuffer IPC below.
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(shared_memory->handle());
  if (!base::SharedMemory::IsHandleValid(handle))
    return NULL;

  if (!Send(new GpuCommandBufferMsg_RegisterTransferBuffer(route_id_,
                                                           new_id,
                                                           handle,
                                                           size))) {
    return NULL;
  }

  *id = new_id;
  scoped_refptr<gpu::Buffer> buffer(
      gpu::MakeBufferFromSharedMemory(shared_memory.Pass(), size));
  return buffer;
}

void CommandBufferProxyImpl::DestroyTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_DestroyTransferBuffer(route_id_, id));
}

gpu::Capabilities CommandBufferProxyImpl::GetCapabilities() {
  return capabilities_;
}

gfx::GpuMemoryBuffer* CommandBufferProxyImpl::CreateGpuMemoryBuffer(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage,
    int32* id) {
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return NULL;

  int32 new_id = channel_->ReserveGpuMemoryBufferId();
  DCHECK(gpu_memory_buffers_.find(new_id) == gpu_memory_buffers_.end());

  scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer(
      channel_->factory()->AllocateGpuMemoryBuffer(
          width, height, internalformat, usage));
  if (!gpu_memory_buffer)
    return NULL;

  DCHECK(GpuChannelHost::IsValidGpuMemoryBuffer(
             gpu_memory_buffer->GetHandle()));

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the RegisterGpuMemoryBuffer IPC below.
  gfx::GpuMemoryBufferHandle handle =
      channel_->ShareGpuMemoryBufferToGpuProcess(
          gpu_memory_buffer->GetHandle());

  if (!Send(new GpuCommandBufferMsg_RegisterGpuMemoryBuffer(
                route_id_,
                new_id,
                handle,
                width,
                height,
                internalformat))) {
    return NULL;
  }

  *id = new_id;
  gpu_memory_buffers_[new_id] = gpu_memory_buffer.release();
  return gpu_memory_buffers_[new_id];
}

void CommandBufferProxyImpl::DestroyGpuMemoryBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Remove the gpu memory buffer from the client side cache.
  GpuMemoryBufferMap::iterator it = gpu_memory_buffers_.find(id);
  if (it != gpu_memory_buffers_.end()) {
    delete it->second;
    gpu_memory_buffers_.erase(it);
  }

  Send(new GpuCommandBufferMsg_DestroyGpuMemoryBuffer(route_id_, id));
}

int CommandBufferProxyImpl::GetRouteID() const {
  return route_id_;
}

void CommandBufferProxyImpl::Echo(const base::Closure& callback) {
  if (last_state_.error != gpu::error::kNoError) {
    return;
  }

  if (!Send(new GpuCommandBufferMsg_Echo(
           route_id_, GpuCommandBufferMsg_EchoAck(route_id_)))) {
    return;
  }

  echo_tasks_.push(callback);
}

uint32 CommandBufferProxyImpl::CreateStreamTexture(uint32 texture_id) {
  if (last_state_.error != gpu::error::kNoError)
    return 0;

  int32 stream_id = channel_->GenerateRouteID();
  bool succeeded;
  Send(new GpuCommandBufferMsg_CreateStreamTexture(
      route_id_, texture_id, stream_id, &succeeded));
  if (!succeeded) {
    DLOG(ERROR) << "GpuCommandBufferMsg_CreateStreamTexture returned failure";
    return 0;
  }
  return stream_id;
}

uint32 CommandBufferProxyImpl::InsertSyncPoint() {
  if (last_state_.error != gpu::error::kNoError)
    return 0;

  uint32 sync_point = 0;
  Send(new GpuCommandBufferMsg_InsertSyncPoint(route_id_, &sync_point));
  return sync_point;
}

void CommandBufferProxyImpl::SignalSyncPoint(uint32 sync_point,
                                             const base::Closure& callback) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  uint32 signal_id = next_signal_id_++;
  if (!Send(new GpuCommandBufferMsg_SignalSyncPoint(route_id_,
                                                    sync_point,
                                                    signal_id))) {
    return;
  }

  signal_tasks_.insert(std::make_pair(signal_id, callback));
}

void CommandBufferProxyImpl::SignalQuery(uint32 query,
                                         const base::Closure& callback) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Signal identifiers are hidden, so nobody outside of this class will see
  // them. (And thus, they cannot save them.) The IDs themselves only last
  // until the callback is invoked, which will happen as soon as the GPU
  // catches upwith the command buffer.
  // A malicious caller trying to create a collision by making next_signal_id
  // would have to make calls at an astounding rate (300B/s) and even if they
  // could do that, all they would do is to prevent some callbacks from getting
  // called, leading to stalled threads and/or memory leaks.
  uint32 signal_id = next_signal_id_++;
  if (!Send(new GpuCommandBufferMsg_SignalQuery(route_id_,
                                                query,
                                                signal_id))) {
    return;
  }

  signal_tasks_.insert(std::make_pair(signal_id, callback));
}

void CommandBufferProxyImpl::SetSurfaceVisible(bool visible) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetSurfaceVisible(route_id_, visible));
}

bool CommandBufferProxyImpl::ProduceFrontBuffer(const gpu::Mailbox& mailbox) {
  if (last_state_.error != gpu::error::kNoError)
    return false;

  return Send(new GpuCommandBufferMsg_ProduceFrontBuffer(route_id_, mailbox));
}

scoped_ptr<media::VideoDecodeAccelerator>
CommandBufferProxyImpl::CreateVideoDecoder() {
  if (!channel_)
    return scoped_ptr<media::VideoDecodeAccelerator>();
  return scoped_ptr<media::VideoDecodeAccelerator>(
      new GpuVideoDecodeAcceleratorHost(channel_, this));
}

scoped_ptr<media::VideoEncodeAccelerator>
CommandBufferProxyImpl::CreateVideoEncoder() {
  if (!channel_)
    return scoped_ptr<media::VideoEncodeAccelerator>();
  return scoped_ptr<media::VideoEncodeAccelerator>(
      new GpuVideoEncodeAcceleratorHost(channel_, this));
}

gpu::error::Error CommandBufferProxyImpl::GetLastError() {
  return last_state_.error;
}

bool CommandBufferProxyImpl::Send(IPC::Message* msg) {
  // Caller should not intentionally send a message if the context is lost.
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (channel_) {
    if (channel_->Send(msg)) {
      return true;
    } else {
      // Flag the command buffer as lost. Defer deleting the channel until
      // OnChannelError is called after returning to the message loop in case
      // it is referenced elsewhere.
      DVLOG(1) << "CommandBufferProxyImpl::Send failed. Losing context.";
      last_state_.error = gpu::error::kLostContext;
      return false;
    }
  }

  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Sender.
  delete msg;
  return false;
}

void CommandBufferProxyImpl::OnUpdateState(
    const gpu::CommandBuffer::State& state) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

void CommandBufferProxyImpl::SetOnConsoleMessageCallback(
    const GpuConsoleMessageCallback& callback) {
  console_message_callback_ = callback;
}

void CommandBufferProxyImpl::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

gpu::CommandBufferSharedState* CommandBufferProxyImpl::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_shm_->memory());
}

}  // namespace content
