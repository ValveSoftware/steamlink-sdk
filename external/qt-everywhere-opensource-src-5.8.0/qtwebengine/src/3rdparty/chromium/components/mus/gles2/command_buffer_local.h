// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_LOCAL_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_LOCAL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"

namespace {
class WaitableEvent;
}

namespace gl {
class GLContext;
class GLSurface;
}

namespace gpu {
class GpuControlClient;
class SyncPointClient;
}

namespace mus {

class CommandBufferDriver;
class CommandBufferLocalClient;
class GpuState;

// This class provides a wrapper around a CommandBufferDriver and a GpuControl
// implementation to allow cc::Display to generate GL directly on the main
// thread.
class CommandBufferLocal : public gpu::CommandBuffer,
                           public gpu::GpuControl,
                           public CommandBufferDriver::Client,
                           base::NonThreadSafe {
 public:
  CommandBufferLocal(CommandBufferLocalClient* client,
                     gfx::AcceleratedWidget widget,
                     const scoped_refptr<GpuState>& gpu_state);

  bool Initialize();
  // Destroy the CommandBufferLocal. The client should not use this class
  // after calling it.
  void Destroy();

  // gpu::CommandBuffer implementation:
  gpu::CommandBuffer::State GetLastState() override;
  int32_t GetLastToken() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  void WaitForTokenInRange(int32_t start, int32_t end) override;
  void WaitForGetOffsetInRange(int32_t start, int32_t end) override;
  void SetGetBuffer(int32_t buffer) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

  // gpu::GpuControl implementation:
  void SetGpuControlClient(gpu::GpuControlClient*) override;
  gpu::Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internalformat) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internal_format,
                                     unsigned usage) override;
  int32_t GetImageGpuMemoryBufferId(unsigned image_id) override;
  void SignalQuery(uint32_t query_id, const base::Closure& callback) override;
  void SetLock(base::Lock*) override;
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

 private:
  // CommandBufferDriver::Client implementation. All called on GPU thread.
  void DidLoseContext(uint32_t reason) override;
  void UpdateVSyncParameters(const base::TimeTicks& timebase,
                             const base::TimeDelta& interval) override;
  void OnGpuCompletedSwapBuffers(gfx::SwapResult result) override;

  ~CommandBufferLocal() override;

  gpu::CommandBufferSharedState* shared_state() const {
    return reinterpret_cast<gpu::CommandBufferSharedState*>(
        shared_state_.get());
  }
  void TryUpdateState();
  void MakeProgressAndUpdateState();

  // Helper functions are called in the GPU thread.
  void InitializeOnGpuThread(base::WaitableEvent* event, bool* result);
  bool FlushOnGpuThread(int32_t put_offset, uint32_t order_num);
  bool SetGetBufferOnGpuThread(int32_t buffer);
  bool RegisterTransferBufferOnGpuThread(
      int32_t id,
      mojo::ScopedSharedBufferHandle transfer_buffer,
      uint32_t size);
  bool DestroyTransferBufferOnGpuThread(int32_t id);
  bool CreateImageOnGpuThread(int32_t id,
                              mojo::ScopedHandle memory_handle,
                              int32_t type,
                              const gfx::Size& size,
                              int32_t format,
                              int32_t internal_format);
  bool CreateImageNativeOzoneOnGpuThread(int32_t id,
                                         int32_t type,
                                         gfx::Size size,
                                         gfx::BufferFormat format,
                                         uint32_t internal_format,
                                         ui::NativePixmap* pixmap);
  bool DestroyImageOnGpuThread(int32_t id);
  bool MakeProgressOnGpuThread(base::WaitableEvent* event,
                               gpu::CommandBuffer::State* state);
  bool DeleteOnGpuThread(base::WaitableEvent* event);
  bool SignalQueryOnGpuThread(uint32_t query_id, const base::Closure& callback);

  // Helper functions are called in the client thread.
  void DidLoseContextOnClientThread(uint32_t reason);
  void UpdateVSyncParametersOnClientThread(const base::TimeTicks& timebase,
                                           const base::TimeDelta& interval);
  void OnGpuCompletedSwapBuffersOnClientThread(gfx::SwapResult result);

  gfx::AcceleratedWidget widget_;
  scoped_refptr<GpuState> gpu_state_;
  std::unique_ptr<CommandBufferDriver> driver_;
  CommandBufferLocalClient* client_;
  scoped_refptr<base::SingleThreadTaskRunner> client_thread_task_runner_;

  // Members accessed on the client thread:
  gpu::GpuControlClient* gpu_control_client_;
  gpu::CommandBuffer::State last_state_;
  mojo::ScopedSharedBufferMapping shared_state_;
  int32_t last_put_offset_;
  gpu::Capabilities capabilities_;
  int32_t next_transfer_buffer_id_;
  int32_t next_image_id_;
  uint64_t next_fence_sync_release_;
  uint64_t flushed_fence_sync_release_;
  bool lost_context_;

  // This sync point client is only for out of order Wait on client thread.
  std::unique_ptr<gpu::SyncPointClient> sync_point_client_waiter_;

  base::WeakPtr<CommandBufferLocal> weak_ptr_;

  // This weak factory will be invalidated in the client thread, so all weak
  // pointers have to be dereferenced in the client thread too.
  base::WeakPtrFactory<CommandBufferLocal> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferLocal);
};

}  //  namespace mus

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_LOCAL_H_
