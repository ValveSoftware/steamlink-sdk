// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_stream_constants.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "gpu/ipc/service/gpu_memory_manager.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/message_router.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

struct GPUCreateCommandBufferConfig;

namespace base {
class WaitableEvent;
}

namespace IPC {
class MessageFilter;
}

namespace gpu {

class PreemptionFlag;
class SyncPointOrderData;
class SyncPointManager;
class GpuChannelManager;
class GpuChannelMessageFilter;
class GpuChannelMessageQueue;
class GpuWatchdog;

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GPU_EXPORT GpuChannel
    : public IPC::Listener,
      public IPC::Sender {
 public:
  // Takes ownership of the renderer process handle.
  GpuChannel(GpuChannelManager* gpu_channel_manager,
             SyncPointManager* sync_point_manager,
             GpuWatchdog* watchdog,
             gl::GLShareGroup* share_group,
             gles2::MailboxManager* mailbox_manager,
             PreemptionFlag* preempting_flag,
             PreemptionFlag* preempted_flag,
             base::SingleThreadTaskRunner* task_runner,
             base::SingleThreadTaskRunner* io_task_runner,
             int32_t client_id,
             uint64_t client_tracing_id,
             bool allow_view_command_buffers,
             bool allow_real_time_streams);
  ~GpuChannel() override;

  // Initializes the IPC channel. Caller takes ownership of the client FD in
  // the returned handle and is responsible for closing it.
  virtual IPC::ChannelHandle Init(base::WaitableEvent* shutdown_event);

  void SetUnhandledMessageListener(IPC::Listener* listener);

  // Get the GpuChannelManager that owns this channel.
  GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_;
  }

  SyncPointManager* sync_point_manager() const { return sync_point_manager_; }

  GpuWatchdog* watchdog() const { return watchdog_; }

  const scoped_refptr<gles2::MailboxManager>& mailbox_manager() const {
    return mailbox_manager_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return task_runner_;
  }

  const scoped_refptr<PreemptionFlag>& preempted_flag() const {
    return preempted_flag_;
  }

  const std::string& channel_id() const { return channel_id_; }

  virtual base::ProcessId GetClientPID() const;

  int client_id() const { return client_id_; }

  uint64_t client_tracing_id() const { return client_tracing_id_; }

  base::WeakPtr<GpuChannel> AsWeakPtr();

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  void OnStreamRescheduled(int32_t stream_id, bool scheduled);

  gl::GLShareGroup* share_group() const { return share_group_.get(); }

  GpuCommandBufferStub* LookupCommandBuffer(int32_t route_id);

  void LoseAllContexts();
  void MarkAllContextsLost();

  // Called to add a listener for a particular message routing ID.
  // Returns true if succeeded.
  bool AddRoute(int32_t route_id, int32_t stream_id, IPC::Listener* listener);

  // Called to remove a listener for a particular message routing ID.
  void RemoveRoute(int32_t route_id);

  void CacheShader(const std::string& key, const std::string& shader);

  void AddFilter(IPC::MessageFilter* filter);
  void RemoveFilter(IPC::MessageFilter* filter);

  uint64_t GetMemoryUsage();

  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32_t internalformat,
      SurfaceHandle surface_handle);

  GpuChannelMessageFilter* filter() const { return filter_.get(); }

  // Returns the global order number for the last processed IPC message.
  uint32_t GetProcessedOrderNum() const;

  // Returns the global order number for the last unprocessed IPC message.
  uint32_t GetUnprocessedOrderNum() const;

  // Returns the shared sync point global order data for the stream.
  scoped_refptr<SyncPointOrderData> GetSyncPointOrderData(
      int32_t stream_id);

  void PostHandleOutOfOrderMessage(const IPC::Message& message);
  void PostHandleMessage(const scoped_refptr<GpuChannelMessageQueue>& queue);

  // Synchronously handle the message to make testing convenient.
  void HandleMessageForTesting(const IPC::Message& msg);

#if defined(OS_ANDROID)
  const GpuCommandBufferStub* GetOneStub() const;
#endif

 protected:
  // The message filter on the io thread.
  scoped_refptr<GpuChannelMessageFilter> filter_;

  // Map of routing id to command buffer stub.
  base::ScopedPtrHashMap<int32_t, std::unique_ptr<GpuCommandBufferStub>> stubs_;

 private:
  bool OnControlMessageReceived(const IPC::Message& msg);

  void HandleMessage(const scoped_refptr<GpuChannelMessageQueue>& queue);

  // Some messages such as WaitForGetOffsetInRange and WaitForTokenInRange are
  // processed as soon as possible because the client is blocked until they
  // are completed.
  void HandleOutOfOrderMessage(const IPC::Message& msg);

  void HandleMessageHelper(const IPC::Message& msg);

  scoped_refptr<GpuChannelMessageQueue> CreateStream(
      int32_t stream_id,
      GpuStreamPriority stream_priority);

  scoped_refptr<GpuChannelMessageQueue> LookupStream(int32_t stream_id);

  void DestroyStreamIfNecessary(
      const scoped_refptr<GpuChannelMessageQueue>& queue);

  void AddRouteToStream(int32_t route_id, int32_t stream_id);
  void RemoveRouteFromStream(int32_t route_id);

  // Message handlers for control messages.
  void OnCreateCommandBuffer(const GPUCreateCommandBufferConfig& init_params,
                             int32_t route_id,
                             base::SharedMemoryHandle shared_state_shm,
                             bool* result,
                             gpu::Capabilities* capabilities);
  void OnDestroyCommandBuffer(int32_t route_id);
  void OnGetDriverBugWorkArounds(
      std::vector<std::string>* gpu_driver_bug_workarounds);

  std::unique_ptr<GpuCommandBufferStub> CreateCommandBuffer(
      const GPUCreateCommandBufferConfig& init_params,
      int32_t route_id,
      std::unique_ptr<base::SharedMemory> shared_state_shm);

  // The lifetime of objects of this class is managed by a GpuChannelManager.
  // The GpuChannelManager destroy all the GpuChannels that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannelManager* const gpu_channel_manager_;

  // Sync point manager. Outlives the channel and is guaranteed to outlive the
  // message loop.
  SyncPointManager* const sync_point_manager_;

  std::unique_ptr<IPC::SyncChannel> channel_;

  IPC::Listener* unhandled_message_listener_;

  // Uniquely identifies the channel within this GPU process.
  std::string channel_id_;

  // Used to implement message routing functionality to CommandBuffer objects
  IPC::MessageRouter router_;

  // Whether the processing of IPCs on this channel is stalled and we should
  // preempt other GpuChannels.
  scoped_refptr<PreemptionFlag> preempting_flag_;

  // If non-NULL, all stubs on this channel should stop processing GL
  // commands (via their CommandExecutor) when preempted_flag_->IsSet()
  scoped_refptr<PreemptionFlag> preempted_flag_;

  // The id of the client who is on the other side of the channel.
  const int32_t client_id_;

  // The tracing ID used for memory allocations associated with this client.
  const uint64_t client_tracing_id_;

  // The task runners for the main thread and the io thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The share group that all contexts associated with a particular renderer
  // process use.
  scoped_refptr<gl::GLShareGroup> share_group_;

  scoped_refptr<gles2::MailboxManager> mailbox_manager_;

  GpuWatchdog* const watchdog_;

  // Map of stream id to appropriate message queue.
  base::hash_map<int32_t, scoped_refptr<GpuChannelMessageQueue>> streams_;

  // Multimap of stream id to route ids.
  base::hash_map<int32_t, int> streams_to_num_routes_;

  // Map of route id to stream id;
  base::hash_map<int32_t, int32_t> routes_to_streams_;

  // Can view command buffers be created on this channel.
  const bool allow_view_command_buffers_;

  // Can real time streams be created on this channel.
  const bool allow_real_time_streams_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

// This filter does three things:
// - it counts and timestamps each message forwarded to the channel
//   so that we can preempt other channels if a message takes too long to
//   process. To guarantee fairness, we must wait a minimum amount of time
//   before preempting and we limit the amount of time that we can preempt in
//   one shot (see constants above).
// - it handles the GpuCommandBufferMsg_InsertSyncPoint message on the IO
//   thread, generating the sync point ID and responding immediately, and then
//   posting a task to insert the GpuCommandBufferMsg_RetireSyncPoint message
//   into the channel's queue.
// - it generates mailbox names for clients of the GPU process on the IO thread.
class GPU_EXPORT GpuChannelMessageFilter : public IPC::MessageFilter {
 public:
  GpuChannelMessageFilter();

  // IPC::MessageFilter implementation.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void AddChannelFilter(scoped_refptr<IPC::MessageFilter> filter);
  void RemoveChannelFilter(scoped_refptr<IPC::MessageFilter> filter);

  void AddRoute(int32_t route_id,
                const scoped_refptr<GpuChannelMessageQueue>& queue);
  void RemoveRoute(int32_t route_id);

  bool Send(IPC::Message* message);

 protected:
  ~GpuChannelMessageFilter() override;

 private:
  scoped_refptr<GpuChannelMessageQueue> LookupStreamByRoute(int32_t route_id);

  bool MessageErrorHandler(const IPC::Message& message, const char* error_msg);

  // Map of route id to message queue.
  base::hash_map<int32_t, scoped_refptr<GpuChannelMessageQueue>> routes_;
  base::Lock routes_lock_;  // Protects |routes_|.

  IPC::Sender* sender_;
  base::ProcessId peer_pid_;
  std::vector<scoped_refptr<IPC::MessageFilter>> channel_filters_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessageFilter);
};

struct GpuChannelMessage {
  IPC::Message message;
  uint32_t order_number;
  base::TimeTicks time_received;

  GpuChannelMessage(const IPC::Message& msg,
                    uint32_t order_num,
                    base::TimeTicks ts)
      : message(msg), order_number(order_num), time_received(ts) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessage);
};

class GpuChannelMessageQueue
    : public base::RefCountedThreadSafe<GpuChannelMessageQueue> {
 public:
  static scoped_refptr<GpuChannelMessageQueue> Create(
      int32_t stream_id,
      GpuStreamPriority stream_priority,
      GpuChannel* channel,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<PreemptionFlag>& preempting_flag,
      const scoped_refptr<PreemptionFlag>& preempted_flag,
      SyncPointManager* sync_point_manager);

  void Disable();
  void DisableIO();

  int32_t stream_id() const { return stream_id_; }
  GpuStreamPriority stream_priority() const { return stream_priority_; }

  bool IsScheduled() const;
  void OnRescheduled(bool scheduled);

  bool HasQueuedMessages() const;

  base::TimeTicks GetNextMessageTimeTick() const;

  scoped_refptr<SyncPointOrderData> GetSyncPointOrderData();

  // Returns the global order number for the last unprocessed IPC message.
  uint32_t GetUnprocessedOrderNum() const;

  // Returns the global order number for the last unprocessed IPC message.
  uint32_t GetProcessedOrderNum() const;

  // Should be called before a message begins to be processed. Returns false if
  // there are no messages to process.
  const GpuChannelMessage* BeginMessageProcessing();
  // Should be called if a message began processing but did not finish.
  void PauseMessageProcessing();
  // Should be called if a message is completely processed. Returns true if
  // there are more messages to process.
  void FinishMessageProcessing();

  bool PushBackMessage(const IPC::Message& message);

 private:
  enum PreemptionState {
    // Either there's no other channel to preempt, there are no messages
    // pending processing, or we just finished preempting and have to wait
    // before preempting again.
    IDLE,
    // We are waiting kPreemptWaitTimeMs before checking if we should preempt.
    WAITING,
    // We can preempt whenever any IPC processing takes more than
    // kPreemptWaitTimeMs.
    CHECKING,
    // We are currently preempting (i.e. no stub is descheduled).
    PREEMPTING,
    // We would like to preempt, but some stub is descheduled.
    WOULD_PREEMPT_DESCHEDULED,
  };

  friend class base::RefCountedThreadSafe<GpuChannelMessageQueue>;

  GpuChannelMessageQueue(
      int32_t stream_id,
      GpuStreamPriority stream_priority,
      GpuChannel* channel,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<PreemptionFlag>& preempting_flag,
      const scoped_refptr<PreemptionFlag>& preempted_flag,
      SyncPointManager* sync_point_manager);
  ~GpuChannelMessageQueue();

  void UpdatePreemptionState();
  void UpdatePreemptionStateHelper();

  void UpdateStateIdle();
  void UpdateStateWaiting();
  void UpdateStateChecking();
  void UpdateStatePreempting();
  void UpdateStateWouldPreemptDescheduled();

  void TransitionToIdle();
  void TransitionToWaiting();
  void TransitionToChecking();
  void TransitionToPreempting();
  void TransitionToWouldPreemptDescheduled();

  bool ShouldTransitionToIdle() const;

  const int32_t stream_id_;
  const GpuStreamPriority stream_priority_;

  // These can be accessed from both IO and main threads and are protected by
  // |channel_lock_|.
  bool enabled_;
  bool scheduled_;
  GpuChannel* const channel_;
  std::deque<std::unique_ptr<GpuChannelMessage>> channel_messages_;
  mutable base::Lock channel_lock_;

  // The following are accessed on the IO thread only.
  // No lock is necessary for preemption state because it's only accessed on the
  // IO thread.
  PreemptionState preemption_state_;
  // Maximum amount of time that we can spend in PREEMPTING.
  // It is reset when we transition to IDLE.
  base::TimeDelta max_preemption_time_;
  // This timer is used and runs tasks on the IO thread.
  std::unique_ptr<base::OneShotTimer> timer_;
  base::ThreadChecker io_thread_checker_;

  // Keeps track of sync point related state such as message order numbers.
  scoped_refptr<SyncPointOrderData> sync_point_order_data_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<PreemptionFlag> preempting_flag_;
  scoped_refptr<PreemptionFlag> preempted_flag_;
  SyncPointManager* const sync_point_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelMessageQueue);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_H_
