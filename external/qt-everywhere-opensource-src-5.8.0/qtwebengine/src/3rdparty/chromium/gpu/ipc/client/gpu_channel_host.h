// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
#define GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_stream_constants.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/message_filter.h"
#include "ipc/message_router.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class WaitableEvent;
}

namespace IPC {
class SyncMessageFilter;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace gpu {

class GPU_EXPORT GpuChannelHostFactory {
 public:
  virtual ~GpuChannelHostFactory() {}

  virtual bool IsMainThread() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetIOThreadTaskRunner() = 0;
  virtual std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
// Every method can be called on any thread with a message loop, except for the
// IO thread.
class GPU_EXPORT GpuChannelHost
    : public IPC::Sender,
      public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  // Must be called on the main thread (as defined by the factory).
  static scoped_refptr<GpuChannelHost> Create(
      GpuChannelHostFactory* factory,
      int channel_id,
      const gpu::GPUInfo& gpu_info,
      const IPC::ChannelHandle& channel_handle,
      base::WaitableEvent* shutdown_event,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  bool IsLost() const {
    DCHECK(channel_filter_.get());
    return channel_filter_->IsLost();
  }

  int channel_id() const { return channel_id_; }

  // The GPU stats reported by the GPU process.
  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // Set an ordering barrier.  AsyncFlushes any pending barriers on other
  // routes. Combines multiple OrderingBarriers into a single AsyncFlush.
  // Returns the flush ID for the stream or 0 if put offset was not changed.
  // Outputs *highest_verified_flush_id.
  uint32_t OrderingBarrier(int32_t route_id,
                           int32_t stream_id,
                           int32_t put_offset,
                           uint32_t flush_count,
                           const std::vector<ui::LatencyInfo>& latency_info,
                           bool put_offset_changed,
                           bool do_flush,
                           uint32_t* highest_verified_flush_id);

  void FlushPendingStream(int32_t stream_id);

  // Destroy this channel. Must be called on the main thread, before
  // destruction.
  void DestroyChannel();

  // Add a message route for the current message loop.
  void AddRoute(int route_id, base::WeakPtr<IPC::Listener> listener);

  // Add a message route to be handled on the provided |task_runner|.
  void AddRouteWithTaskRunner(
      int route_id,
      base::WeakPtr<IPC::Listener> listener,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Remove the message route associated with |route_id|.
  void RemoveRoute(int route_id);

  GpuChannelHostFactory* factory() const { return factory_; }

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }

  // Returns a handle to the shared memory that can be sent via IPC to the
  // GPU process. The caller is responsible for ensuring it is closed. Returns
  // an invalid handle on failure.
  base::SharedMemoryHandle ShareToGpuProcess(
      base::SharedMemoryHandle source_handle);

  // Reserve one unused transfer buffer ID.
  int32_t ReserveTransferBufferId();

  // Returns a GPU memory buffer handle to the buffer that can be sent via
  // IPC to the GPU process. The caller is responsible for ensuring it is
  // closed. Returns an invalid handle on failure.
  gfx::GpuMemoryBufferHandle ShareGpuMemoryBufferToGpuProcess(
      const gfx::GpuMemoryBufferHandle& source_handle,
      bool* requires_sync_point);

  // Reserve one unused image ID.
  int32_t ReserveImageId();

  // Generate a route ID guaranteed to be unique for this channel.
  int32_t GenerateRouteID();

  // Generate a stream ID guaranteed to be unique for this channel.
  int32_t GenerateStreamID();

  // Sends a synchronous nop to the server which validate that all previous IPC
  // messages have been received. Once the synchronous nop has been sent to the
  // server all previous flushes will all be marked as validated, including
  // flushes for other streams on the same channel. Once a validation has been
  // sent, it will return the highest validated flush id for the stream.
  // If the validation fails (which can only happen upon context lost), the
  // highest validated flush id will not change. If no flush ID were ever
  // validated then it will return 0 (Note the lowest valid flush ID is 1).
  uint32_t ValidateFlushIDReachedServer(int32_t stream_id, bool force_validate);

  // Returns the highest validated flush ID for a given stream.
  uint32_t GetHighestValidatedFlushID(int32_t stream_id);

 private:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop. It also maintains some shared state between
  // all the contexts.
  class MessageFilter : public IPC::MessageFilter {
   public:
    MessageFilter();

    // Called on the IO thread.
    void AddRoute(int32_t route_id,
                  base::WeakPtr<IPC::Listener> listener,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    // Called on the IO thread.
    void RemoveRoute(int32_t route_id);

    // IPC::MessageFilter implementation
    // (called on the IO thread):
    bool OnMessageReceived(const IPC::Message& msg) override;
    void OnChannelError() override;

    // The following methods can be called on any thread.

    // Whether the channel is lost.
    bool IsLost() const;

   private:
    struct ListenerInfo {
      ListenerInfo();
      ListenerInfo(const ListenerInfo& other);
      ~ListenerInfo();

      base::WeakPtr<IPC::Listener> listener;
      scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    };

    ~MessageFilter() override;

    // Threading notes: |listeners_| is only accessed on the IO thread. Every
    // other field is protected by |lock_|.
    base::hash_map<int32_t, ListenerInfo> listeners_;

    // Protects all fields below this one.
    mutable base::Lock lock_;

    // Whether the channel has been lost.
    bool lost_;
  };

  struct StreamFlushInfo {
    StreamFlushInfo();
    StreamFlushInfo(const StreamFlushInfo& other);
    ~StreamFlushInfo();

    // These are global per stream.
    uint32_t next_stream_flush_id;
    uint32_t flushed_stream_flush_id;
    uint32_t verified_stream_flush_id;

    // These are local per context.
    bool flush_pending;
    int32_t route_id;
    int32_t put_offset;
    uint32_t flush_count;
    uint32_t flush_id;
    std::vector<ui::LatencyInfo> latency_info;
  };

  GpuChannelHost(GpuChannelHostFactory* factory,
                 int channel_id,
                 const gpu::GPUInfo& gpu_info,
                 gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuChannelHost() override;
  void Connect(const IPC::ChannelHandle& channel_handle,
               base::WaitableEvent* shutdown_event);
  bool InternalSend(IPC::Message* msg);
  void InternalFlush(StreamFlushInfo* flush_info);

  // Threading notes: all fields are constant during the lifetime of |this|
  // except:
  // - |next_image_id_|, atomic type
  // - |next_route_id_|, atomic type
  // - |next_stream_id_|, atomic type
  // - |channel_| and |stream_flush_info_|, protected by |context_lock_|
  GpuChannelHostFactory* const factory_;

  const int channel_id_;
  const gpu::GPUInfo gpu_info_;

  scoped_refptr<MessageFilter> channel_filter_;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;

  // A filter for sending messages from thread other than the main thread.
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  // Image IDs are allocated in sequence.
  base::AtomicSequenceNumber next_image_id_;

  // Route IDs are allocated in sequence.
  base::AtomicSequenceNumber next_route_id_;

  // Stream IDs are allocated in sequence.
  base::AtomicSequenceNumber next_stream_id_;

  // Protects channel_ and stream_flush_info_.
  mutable base::Lock context_lock_;
  std::unique_ptr<IPC::SyncChannel> channel_;
  base::hash_map<int32_t, StreamFlushInfo> stream_flush_info_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
