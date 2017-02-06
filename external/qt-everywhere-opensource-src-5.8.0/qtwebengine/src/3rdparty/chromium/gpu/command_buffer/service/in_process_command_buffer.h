// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IN_PROCESS_COMMAND_BUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_IN_PROCESS_COMMAND_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"

namespace base {
class SequenceChecker;
}

namespace gl {
class GLContext;
class GLShareGroup;
class GLSurface;
}

namespace gfx {
class Size;
}

namespace gpu {
class SyncPointClient;
class SyncPointOrderData;
class SyncPointManager;

namespace gles2 {
struct ContextCreationAttribHelper;
class FramebufferCompletenessCache;
class GLES2Decoder;
class MailboxManager;
class ProgramCache;
class ShaderTranslatorCache;
}

class CommandBufferServiceBase;
class GpuMemoryBufferManager;
class CommandExecutor;
class ImageFactory;
class TransferBufferManagerInterface;

// This class provides a thread-safe interface to the global GPU service (for
// example GPU thread) when being run in single process mode.
// However, the behavior for accessing one context (i.e. one instance of this
// class) from different client threads is undefined.
class GPU_EXPORT InProcessCommandBuffer : public CommandBuffer,
                                          public GpuControl {
 public:
  class Service;
  explicit InProcessCommandBuffer(const scoped_refptr<Service>& service);
  ~InProcessCommandBuffer() override;

  // If |surface| is not null, use it directly; in this case, the command
  // buffer gpu thread must be the same as the client thread. Otherwise create
  // a new GLSurface.
  bool Initialize(scoped_refptr<gl::GLSurface> surface,
                  bool is_offscreen,
                  gfx::AcceleratedWidget window,
                  const gles2::ContextCreationAttribHelper& attribs,
                  InProcessCommandBuffer* share_group,
                  GpuMemoryBufferManager* gpu_memory_buffer_manager,
                  ImageFactory* image_factory);

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
  gpu::error::Error GetLastError() override;

  // GpuControl implementation:
  // NOTE: The GpuControlClient will be called on the client thread.
  void SetGpuControlClient(GpuControlClient*) override;
  gpu::Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internalformat) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internalformat,
                                     unsigned usage) override;
  int32_t GetImageGpuMemoryBufferId(unsigned image_id) override;
  void SignalQuery(uint32_t query_id, const base::Closure& callback) override;
  void SetLock(base::Lock*) override;
  void EnsureWorkVisible() override;
  CommandBufferNamespace GetNamespaceID() const override;
  CommandBufferId GetCommandBufferID() const override;
  int32_t GetExtraCommandBufferData() const override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncRelease(uint64_t release) override;
  bool IsFenceSyncFlushed(uint64_t release) override;
  bool IsFenceSyncFlushReceived(uint64_t release) override;
  void SignalSyncToken(const SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool CanWaitUnverifiedSyncToken(const SyncToken* sync_token) override;

  // The serializer interface to the GPU service (i.e. thread).
  class Service {
   public:
    Service();
    Service(const gpu::GpuPreferences& gpu_preferences);
    virtual ~Service();

    virtual void AddRef() const = 0;
    virtual void Release() const = 0;

    // Queues a task to run as soon as possible.
    virtual void ScheduleTask(const base::Closure& task) = 0;

    // Schedules |callback| to run at an appropriate time for performing delayed
    // work.
    virtual void ScheduleDelayedWork(const base::Closure& task) = 0;

    virtual bool UseVirtualizedGLContexts() = 0;
    virtual scoped_refptr<gles2::ShaderTranslatorCache>
        shader_translator_cache() = 0;
    virtual scoped_refptr<gles2::FramebufferCompletenessCache>
    framebuffer_completeness_cache() = 0;
    virtual SyncPointManager* sync_point_manager() = 0;
    const GpuPreferences& gpu_preferences();
    const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds();
    scoped_refptr<gl::GLShareGroup> share_group();
    scoped_refptr<gles2::MailboxManager> mailbox_manager();
    gpu::gles2::ProgramCache* program_cache();

   private:
    const GpuPreferences gpu_preferences_;
    const GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
    scoped_refptr<gl::GLShareGroup> share_group_;
    scoped_refptr<gles2::MailboxManager> mailbox_manager_;
    std::unique_ptr<gpu::gles2::ProgramCache> program_cache_;
  };

 private:
  struct InitializeOnGpuThreadParams {
    bool is_offscreen;
    gfx::AcceleratedWidget window;
    const gles2::ContextCreationAttribHelper& attribs;
    gpu::Capabilities* capabilities;  // Ouptut.
    InProcessCommandBuffer* context_group;
    ImageFactory* image_factory;

    InitializeOnGpuThreadParams(
        bool is_offscreen,
        gfx::AcceleratedWidget window,
        const gles2::ContextCreationAttribHelper& attribs,
        gpu::Capabilities* capabilities,
        InProcessCommandBuffer* share_group,
        ImageFactory* image_factory)
        : is_offscreen(is_offscreen),
          window(window),
          attribs(attribs),
          capabilities(capabilities),
          context_group(share_group),
          image_factory(image_factory) {}
  };

  bool InitializeOnGpuThread(const InitializeOnGpuThreadParams& params);
  void Destroy();
  bool DestroyOnGpuThread();
  void FlushOnGpuThread(int32_t put_offset, uint32_t order_num);
  void ScheduleDelayedWorkOnGpuThread();
  bool MakeCurrent();
  base::Closure WrapCallback(const base::Closure& callback);
  State GetStateFast();
  void QueueTask(const base::Closure& task) { service_->ScheduleTask(task); }
  void CheckSequencedThread();
  void FenceSyncReleaseOnGpuThread(uint64_t release);
  bool WaitFenceSyncOnGpuThread(gpu::CommandBufferNamespace namespace_id,
                                gpu::CommandBufferId command_buffer_id,
                                uint64_t release);
  void DescheduleUntilFinishedOnGpuThread();
  void RescheduleAfterFinishedOnGpuThread();
  void SignalSyncTokenOnGpuThread(const SyncToken& sync_token,
                                  const base::Closure& callback);
  void SignalQueryOnGpuThread(unsigned query_id, const base::Closure& callback);
  void DestroyTransferBufferOnGpuThread(int32_t id);
  void CreateImageOnGpuThread(int32_t id,
                              const gfx::GpuMemoryBufferHandle& handle,
                              const gfx::Size& size,
                              gfx::BufferFormat format,
                              uint32_t internalformat,
                              uint32_t order_num,
                              uint64_t fence_sync);
  void DestroyImageOnGpuThread(int32_t id);
  void SetGetBufferOnGpuThread(int32_t shm_id, base::WaitableEvent* completion);

  // Callbacks on the gpu thread.
  void OnContextLostOnGpuThread();
  void PumpCommandsOnGpuThread();
  void PerformDelayedWorkOnGpuThread();
  // Callback implementations on the client thread.
  void OnContextLost();

  const CommandBufferId command_buffer_id_;

  // Members accessed on the gpu thread (possibly with the exception of
  // creation):
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
  scoped_refptr<TransferBufferManagerInterface> transfer_buffer_manager_;
  std::unique_ptr<CommandExecutor> executor_;
  std::unique_ptr<gles2::GLES2Decoder> decoder_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<SyncPointOrderData> sync_point_order_data_;
  std::unique_ptr<SyncPointClient> sync_point_client_;
  base::Closure context_lost_callback_;
  // Used to throttle PerformDelayedWorkOnGpuThread.
  bool delayed_work_pending_;
  ImageFactory* image_factory_;

  // Members accessed on the client thread:
  GpuControlClient* gpu_control_client_;
#if DCHECK_IS_ON()
  bool context_lost_;
#endif
  State last_state_;
  int32_t last_put_offset_;
  gpu::Capabilities capabilities_;
  GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  base::AtomicSequenceNumber next_image_id_;
  uint64_t next_fence_sync_release_;
  uint64_t flushed_fence_sync_release_;

  // Accessed on both threads:
  std::unique_ptr<CommandBufferServiceBase> command_buffer_;
  base::Lock command_buffer_lock_;
  base::WaitableEvent flush_event_;
  scoped_refptr<Service> service_;
  State state_after_last_flush_;
  base::Lock state_after_last_flush_lock_;
  scoped_refptr<gl::GLShareGroup> gl_share_group_;
  base::WaitableEvent fence_sync_wait_event_;

  // Only used with explicit scheduling and the gpu thread is the same as
  // the client thread.
  std::unique_ptr<base::SequenceChecker> sequence_checker_;

  // A map from image id to GpuMemoryBuffer id.
  std::map<int32_t, int32_t> image_gmb_ids_map_;

  base::WeakPtr<InProcessCommandBuffer> client_thread_weak_ptr_;
  base::WeakPtr<InProcessCommandBuffer> gpu_thread_weak_ptr_;
  base::WeakPtrFactory<InProcessCommandBuffer> client_thread_weak_ptr_factory_;
  base::WeakPtrFactory<InProcessCommandBuffer> gpu_thread_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InProcessCommandBuffer);
};

// Default Service class when a null service is used.
class GPU_EXPORT GpuInProcessThread
    : public base::Thread,
      public NON_EXPORTED_BASE(InProcessCommandBuffer::Service),
      public base::RefCountedThreadSafe<GpuInProcessThread> {
 public:
  explicit GpuInProcessThread(SyncPointManager* sync_point_manager);

  void AddRef() const override;
  void Release() const override;
  void ScheduleTask(const base::Closure& task) override;
  void ScheduleDelayedWork(const base::Closure& callback) override;
  bool UseVirtualizedGLContexts() override;
  scoped_refptr<gles2::ShaderTranslatorCache> shader_translator_cache()
      override;
  scoped_refptr<gles2::FramebufferCompletenessCache>
  framebuffer_completeness_cache() override;
  SyncPointManager* sync_point_manager() override;

 private:
  ~GpuInProcessThread() override;
  friend class base::RefCountedThreadSafe<GpuInProcessThread>;

  SyncPointManager* sync_point_manager_;  // Non-owning.
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  scoped_refptr<gpu::gles2::FramebufferCompletenessCache>
      framebuffer_completeness_cache_;

  DISALLOW_COPY_AND_ASSIGN(GpuInProcessThread);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IN_PROCESS_COMMAND_BUFFER_H_
