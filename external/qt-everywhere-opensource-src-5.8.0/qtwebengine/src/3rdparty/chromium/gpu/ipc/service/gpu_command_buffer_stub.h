// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_COMMAND_BUFFER_STUB_H_
#define GPU_IPC_SERVICE_GPU_COMMAND_BUFFER_STUB_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/command_executor.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_memory_manager.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {
struct Mailbox;
struct SyncToken;
class SyncPointClient;
class SyncPointManager;
namespace gles2 {
class MailboxManager;
}
}

struct GPUCreateCommandBufferConfig;
struct GpuCommandBufferMsg_CreateImage_Params;
struct GpuCommandBufferMsg_SwapBuffersCompleted_Params;

namespace gpu {

class GpuChannel;
class GpuWatchdog;
struct WaitForCommandState;

class GPU_EXPORT GpuCommandBufferStub
    : public IPC::Listener,
      public IPC::Sender,
      public base::SupportsWeakPtr<GpuCommandBufferStub> {
 public:
  class DestructionObserver {
   public:
    // Called in Destroy(), before the context/surface are released.
    virtual void OnWillDestroyStub() = 0;

   protected:
    virtual ~DestructionObserver() {}
  };

  typedef base::Callback<void(const std::vector<ui::LatencyInfo>&)>
      LatencyInfoCallback;

  static std::unique_ptr<GpuCommandBufferStub> Create(
    GpuChannel* channel,
    GpuCommandBufferStub* share_group,
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id,
    std::unique_ptr<base::SharedMemory> shared_state_shm);

  ~GpuCommandBufferStub() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  gles2::MemoryTracker* GetMemoryTracker() const;

  // Whether this command buffer can currently handle IPC messages.
  bool IsScheduled();

  // Whether there are commands in the buffer that haven't been processed.
  bool HasUnprocessedCommands();

  gles2::GLES2Decoder* decoder() const { return decoder_.get(); }
  CommandExecutor* scheduler() const { return executor_.get(); }
  GpuChannel* channel() const { return channel_; }

  // Unique command buffer ID for this command buffer stub.
  CommandBufferId command_buffer_id() const { return command_buffer_id_; }

  // Identifies the various GpuCommandBufferStubs in the GPU process belonging
  // to the same renderer process.
  int32_t route_id() const { return route_id_; }

  // Identifies the stream for this command buffer.
  int32_t stream_id() const { return stream_id_; }

  // Sends a message to the console.
  void SendConsoleMessage(int32_t id, const std::string& message);

  void SendCachedShader(const std::string& key, const std::string& shader);

  gl::GLSurface* surface() const { return surface_.get(); }

  void AddDestructionObserver(DestructionObserver* observer);
  void RemoveDestructionObserver(DestructionObserver* observer);

  void SetLatencyInfoCallback(const LatencyInfoCallback& callback);

  void MarkContextLost();

  const gles2::FeatureInfo* GetFeatureInfo() const;

  void SendSwapBuffersCompleted(
      const GpuCommandBufferMsg_SwapBuffersCompleted_Params& params);
  void SendUpdateVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval);

 private:
  GpuCommandBufferStub(GpuChannel* channel,
                       const GPUCreateCommandBufferConfig& init_params,
                       int32_t route_id);

  bool Initialize(GpuCommandBufferStub* share_group,
                  const GPUCreateCommandBufferConfig& init_params,
                  std::unique_ptr<base::SharedMemory> shared_state_shm);

  GpuMemoryManager* GetMemoryManager() const;

  void Destroy();

  bool MakeCurrent();

  // Message handlers:
  void OnSetGetBuffer(int32_t shm_id, IPC::Message* reply_message);
  void OnTakeFrontBuffer(const Mailbox& mailbox);
  void OnReturnFrontBuffer(const Mailbox& mailbox, bool is_lost);
  void OnGetState(IPC::Message* reply_message);
  void OnWaitForTokenInRange(int32_t start,
                             int32_t end,
                             IPC::Message* reply_message);
  void OnWaitForGetOffsetInRange(int32_t start,
                                 int32_t end,
                                 IPC::Message* reply_message);
  void OnAsyncFlush(int32_t put_offset,
                    uint32_t flush_count,
                    const std::vector<ui::LatencyInfo>& latency_info);
  void OnRegisterTransferBuffer(int32_t id,
                                base::SharedMemoryHandle transfer_buffer,
                                uint32_t size);
  void OnDestroyTransferBuffer(int32_t id);
  void OnGetTransferBuffer(int32_t id, IPC::Message* reply_message);

  void OnEnsureBackbuffer();

  void OnWaitSyncToken(const SyncToken& sync_token);
  void OnSignalSyncToken(const SyncToken& sync_token, uint32_t id);
  void OnSignalAck(uint32_t id);
  void OnSignalQuery(uint32_t query, uint32_t id);

  void OnFenceSyncRelease(uint64_t release);
  bool OnWaitFenceSync(CommandBufferNamespace namespace_id,
                       CommandBufferId command_buffer_id,
                       uint64_t release);
  void OnWaitFenceSyncCompleted(CommandBufferNamespace namespace_id,
                                CommandBufferId command_buffer_id,
                                uint64_t release);

  void OnDescheduleUntilFinished();
  void OnRescheduleAfterFinished();

  void OnCreateImage(const GpuCommandBufferMsg_CreateImage_Params& params);
  void OnDestroyImage(int32_t id);
  void OnCreateStreamTexture(uint32_t texture_id,
                             int32_t stream_id,
                             bool* succeeded);
  void OnCommandProcessed();
  void OnParseError();

  void ReportState();

  // Wrapper for CommandExecutor::PutChanged that sets the crash report URL.
  void PutChanged();

  // Poll the command buffer to execute work.
  void PollWork();
  void PerformWork();

  // Schedule processing of delayed work. This updates the time at which
  // delayed work should be processed. |process_delayed_work_time_| is
  // updated to current time + delay. Call this after processing some amount
  // of delayed work.
  void ScheduleDelayedWork(base::TimeDelta delay);

  bool CheckContextLost();
  void CheckCompleteWaits();
  void PullTextureUpdates(CommandBufferNamespace namespace_id,
                          CommandBufferId command_buffer_id,
                          uint32_t release);

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the GpuCommandBufferStubs that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannel* const channel_;

  // The group of contexts that share namespaces with this context.
  scoped_refptr<gles2::ContextGroup> context_group_;

  bool initialized_;
  const SurfaceHandle surface_handle_;
  bool use_virtualized_gl_context_;
  const CommandBufferId command_buffer_id_;
  const int32_t stream_id_;
  const int32_t route_id_;
  uint32_t last_flush_count_;

  std::unique_ptr<CommandBufferService> command_buffer_;
  std::unique_ptr<gles2::GLES2Decoder> decoder_;
  std::unique_ptr<CommandExecutor> executor_;
  std::unique_ptr<SyncPointClient> sync_point_client_;
  scoped_refptr<gl::GLSurface> surface_;

  base::ObserverList<DestructionObserver> destruction_observers_;

  bool waiting_for_sync_point_;

  base::TimeTicks process_delayed_work_time_;
  uint32_t previous_processed_num_;
  base::TimeTicks last_idle_time_;

  LatencyInfoCallback latency_info_callback_;

  GURL active_url_;
  size_t active_url_hash_;

  std::unique_ptr<WaitForCommandState> wait_for_token_;
  std::unique_ptr<WaitForCommandState> wait_for_get_offset_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_COMMAND_BUFFER_STUB_H_
