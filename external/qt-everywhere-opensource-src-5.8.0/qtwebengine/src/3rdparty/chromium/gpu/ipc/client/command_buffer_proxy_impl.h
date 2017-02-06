// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
#define GPU_IPC_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_stream_constants.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/ipc_listener.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gpu_preference.h"

struct GPUCommandBufferConsoleMessage;
struct GPUCreateCommandBufferConfig;
struct GpuCommandBufferMsg_SwapBuffersCompleted_Params;
class GURL;

namespace base {
class SharedMemory;
}

namespace gfx {
class Size;
}

namespace gpu {
struct GpuProcessHostedCALayerTreeParamsMac;
struct Mailbox;
struct SyncToken;

namespace gles2 {
struct ContextCreationAttribHelper;
}
}

namespace gpu {
class GpuChannelHost;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class GPU_EXPORT CommandBufferProxyImpl
    : public gpu::CommandBuffer,
      public gpu::GpuControl,
      public IPC::Listener,
      public base::SupportsWeakPtr<CommandBufferProxyImpl> {
 public:
  class DeletionObserver {
   public:
    // Called during the destruction of the CommandBufferProxyImpl.
    virtual void OnWillDeleteImpl() = 0;

   protected:
    virtual ~DeletionObserver() {}
  };

  typedef base::Callback<void(const std::string& msg, int id)>
      GpuConsoleMessageCallback;

  // Create and connect to a command buffer in the GPU process.
  static std::unique_ptr<CommandBufferProxyImpl> Create(
      scoped_refptr<GpuChannelHost> host,
      gpu::SurfaceHandle surface_handle,
      CommandBufferProxyImpl* share_group,
      int32_t stream_id,
      gpu::GpuStreamPriority stream_priority,
      const gpu::gles2::ContextCreationAttribHelper& attribs,
      const GURL& active_url,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~CommandBufferProxyImpl() override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // CommandBuffer implementation:
  State GetLastState() override;
  int32_t GetLastToken() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  void WaitForTokenInRange(int32_t start, int32_t end) override;
  void WaitForGetOffsetInRange(int32_t start, int32_t end) override;
  void SetGetBuffer(int32_t shm_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

  // gpu::GpuControl implementation:
  void SetGpuControlClient(GpuControlClient* client) override;
  gpu::Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internal_format) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internal_format,
                                     unsigned usage) override;
  int32_t GetImageGpuMemoryBufferId(unsigned image_id) override;
  void SignalQuery(uint32_t query, const base::Closure& callback) override;
  void SetLock(base::Lock* lock) override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  gpu::CommandBufferId GetCommandBufferID() const override;
  int32_t GetExtraCommandBufferData() const override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncRelease(uint64_t release) override;
  bool IsFenceSyncFlushed(uint64_t release) override;
  bool IsFenceSyncFlushReceived(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken* sync_token) override;

  void TakeFrontBuffer(const gpu::Mailbox& mailbox);
  void ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                         const gpu::SyncToken& sync_token,
                         bool is_lost);

  void AddDeletionObserver(DeletionObserver* observer);
  void RemoveDeletionObserver(DeletionObserver* observer);

  bool EnsureBackbuffer();

  void SetOnConsoleMessageCallback(const GpuConsoleMessageCallback& callback);

  void SetLatencyInfo(const std::vector<ui::LatencyInfo>& latency_info);
  using SwapBuffersCompletionCallback = base::Callback<void(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac)>;
  void SetSwapBuffersCompletionCallback(
      const SwapBuffersCompletionCallback& callback);

  using UpdateVSyncParametersCallback =
      base::Callback<void(base::TimeTicks timebase, base::TimeDelta interval)>;
  void SetUpdateVSyncParametersCallback(
      const UpdateVSyncParametersCallback& callback);

  // TODO(apatrick): this is a temporary optimization while skia is calling
  // ContentGLContext::MakeCurrent prior to every GL call. It saves returning 6
  // ints redundantly when only the error is needed for the
  // CommandBufferProxyImpl implementation.
  gpu::error::Error GetLastError() override;

  int32_t route_id() const { return route_id_; }

  const scoped_refptr<GpuChannelHost>& channel() const { return channel_; }

  base::SharedMemoryHandle GetSharedStateHandle() const {
    return shared_state_shm_->handle();
  }
  uint32_t CreateStreamTexture(uint32_t texture_id);

 private:
  typedef std::map<int32_t, scoped_refptr<gpu::Buffer>> TransferBufferMap;
  typedef base::hash_map<uint32_t, base::Closure> SignalTaskMap;

  CommandBufferProxyImpl(int channel_id, int32_t route_id, int32_t stream_id);
  bool Initialize(scoped_refptr<GpuChannelHost> channel,
                  const GPUCreateCommandBufferConfig& config,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void CheckLock() {
    if (lock_) {
      lock_->AssertAcquired();
    } else {
      DCHECK(lockless_thread_checker_.CalledOnValidThread());
    }
  }

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  // Message handlers:
  void OnDestroyed(gpu::error::ContextLostReason reason,
                   gpu::error::Error error);
  void OnConsoleMessage(const GPUCommandBufferConsoleMessage& message);
  void OnSignalAck(uint32_t id);
  void OnSwapBuffersCompleted(
      const GpuCommandBufferMsg_SwapBuffersCompleted_Params& params);
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

  // Updates the highest verified release fence sync.
  void UpdateVerifiedReleases(uint32_t verified_flush);

  // Try to read an updated copy of the state from shared memory, and calls
  // OnGpuStateError() if the new state has an error.
  void TryUpdateState();
  // Like the above but does not call the error event handler if the new state
  // has an error.
  void TryUpdateStateDontReportError();
  // Sets the state, and calls OnGpuStateError() if the new state has an error.
  void SetStateFromSyncReply(const gpu::CommandBuffer::State& state);

  // Loses the context after we received an invalid reply from the GPU
  // process.
  void OnGpuSyncReplyError();

  // Loses the context when receiving a message from the GPU process.
  void OnGpuAsyncMessageError(gpu::error::ContextLostReason reason,
                              gpu::error::Error error);

  // Loses the context after we receive an error state from the GPU process.
  void OnGpuStateError();

  // Sets an error on the last_state_ and loses the context due to client-side
  // errors.
  void OnClientError(gpu::error::Error error);

  // Helper methods, don't call these directly.
  void DisconnectChannelInFreshCallStack();
  void LockAndDisconnectChannel();
  void DisconnectChannel();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const;

  // There should be a lock_ if this is going to be used across multiple
  // threads, or we guarantee it is used by a single thread by using a thread
  // checker if no lock_ is set.
  base::Lock* lock_;
  base::ThreadChecker lockless_thread_checker_;

  // Client that wants to listen for important events on the GpuControl.
  gpu::GpuControlClient* gpu_control_client_;

  // Unowned list of DeletionObservers.
  base::ObserverList<DeletionObserver> deletion_observers_;

  // The last cached state received from the service.
  State last_state_;

  // The shared memory area used to update state.
  std::unique_ptr<base::SharedMemory> shared_state_shm_;

  scoped_refptr<GpuChannelHost> channel_;
  const gpu::CommandBufferId command_buffer_id_;
  const int32_t route_id_;
  const int32_t stream_id_;
  uint32_t flush_count_;
  int32_t last_put_offset_;
  int32_t last_barrier_put_offset_;

  // Next generated fence sync.
  uint64_t next_fence_sync_release_;

  // Unverified flushed fence syncs with their corresponding flush id.
  std::queue<std::pair<uint64_t, uint32_t>> flushed_release_flush_id_;

  // Last flushed fence sync release, same as last item in queue if not empty.
  uint64_t flushed_fence_sync_release_;

  // Last verified fence sync.
  uint64_t verified_fence_sync_release_;

  GpuConsoleMessageCallback console_message_callback_;

  // Tasks to be invoked in SignalSyncPoint responses.
  uint32_t next_signal_id_;
  SignalTaskMap signal_tasks_;

  gpu::Capabilities capabilities_;

  std::vector<ui::LatencyInfo> latency_info_;

  SwapBuffersCompletionCallback swap_buffers_completion_callback_;
  UpdateVSyncParametersCallback update_vsync_parameters_completion_callback_;

  // |gpu_memory_buffer_id| will always contain a valid (not -1) id.
  // If the GpuMemoryBuffer was created through this class, then
  // |owned_gpu_memory_buffer| will also be not null.
  struct ImageInfo {
    ImageInfo();
    ~ImageInfo();
    ImageInfo(ImageInfo&& other);
    ImageInfo& operator=(ImageInfo&& other);
    int32_t gpu_memory_buffer_id = -1;
    std::unique_ptr<gfx::GpuMemoryBuffer> owned_gpu_memory_buffer;
  };
  // A map from image id to ImageInfo.
  std::map<int32_t, ImageInfo> image_gmb_map_;

  base::WeakPtr<CommandBufferProxyImpl> weak_this_;
  scoped_refptr<base::SequencedTaskRunner> callback_thread_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxyImpl);
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_COMMAND_BUFFER_PROXY_IMPL_H_
