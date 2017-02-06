// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_DRIVER_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_DRIVER_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"

namespace gl {
class GLContext;
class GLSurface;
}

namespace gpu {
class CommandBufferService;
class CommandExecutor;
class SyncPointClient;
class SyncPointOrderData;

namespace gles2 {
class GLES2Decoder;
}  // namespace gles2

}  // namespace gpu

namespace ui {
class NativePixmap;
}

namespace mus {

class GpuState;

// This class receives CommandBuffer messages on the same thread as the native
// viewport.
class CommandBufferDriver : base::NonThreadSafe {
 public:
  class Client {
   public:
    virtual ~Client();
    virtual void DidLoseContext(uint32_t reason) = 0;
    virtual void UpdateVSyncParameters(const base::TimeTicks& timebase,
                                       const base::TimeDelta& interval) = 0;
    virtual void OnGpuCompletedSwapBuffers(gfx::SwapResult result) = 0;
  };
  CommandBufferDriver(gpu::CommandBufferNamespace command_buffer_namespace,
                      gpu::CommandBufferId command_buffer_id,
                      gfx::AcceleratedWidget widget,
                      scoped_refptr<GpuState> gpu_state);
  ~CommandBufferDriver();

  // The class owning the CommandBufferDriver instance (e.g. CommandBufferLocal)
  // is itself the Client implementation so CommandBufferDriver does not own the
  // client.
  void set_client(Client* client) { client_ = client; }

  bool Initialize(mojo::ScopedSharedBufferHandle shared_state,
                  mojo::Array<int32_t> attribs);
  void SetGetBuffer(int32_t buffer);
  void Flush(int32_t put_offset);
  void RegisterTransferBuffer(int32_t id,
                              mojo::ScopedSharedBufferHandle transfer_buffer,
                              uint32_t size);
  void DestroyTransferBuffer(int32_t id);
  void CreateImage(int32_t id,
                   mojo::ScopedHandle memory_handle,
                   int32_t type,
                   const gfx::Size& size,
                   int32_t format,
                   int32_t internal_format);
  void CreateImageNativeOzone(int32_t id,
                              int32_t type,
                              gfx::Size size,
                              gfx::BufferFormat format,
                              uint32_t internal_format,
                              ui::NativePixmap* pixmap);
  void DestroyImage(int32_t id);
  bool IsScheduled() const;
  bool HasUnprocessedCommands() const;
  gpu::Capabilities GetCapabilities() const;
  gpu::CommandBuffer::State GetLastState() const;
  gpu::CommandBufferNamespace GetNamespaceID() const {
    return command_buffer_namespace_;
  }
  gpu::CommandBufferId GetCommandBufferID() const { return command_buffer_id_; }
  gpu::SyncPointOrderData* sync_point_order_data() {
    return sync_point_order_data_.get();
  }
  uint32_t GetUnprocessedOrderNum() const;
  uint32_t GetProcessedOrderNum() const;
  void SignalQuery(uint32_t query_id, const base::Closure& callback);

 private:
  bool MakeCurrent();

  // Process pending queries and call |ScheduleDelayedWork| to schedule
  // processing of delayed work.
  void ProcessPendingAndIdleWork();

  // Schedule processing of delayed work. This updates the time at which
  // delayed work should be processed. |process_delayed_work_time_| is
  // updated to current time + delay. Call this after processing some amount
  // of delayed work.
  void ScheduleDelayedWork(base::TimeDelta delay);

  // Poll the command buffer to execute work.
  void PollWork();
  void PerformWork();

  void DestroyDecoder();

  // Callbacks:
  void OnUpdateVSyncParameters(const base::TimeTicks timebase,
                               const base::TimeDelta interval);
  void OnFenceSyncRelease(uint64_t release);
  bool OnWaitFenceSync(gpu::CommandBufferNamespace namespace_id,
                       gpu::CommandBufferId command_buffer_id,
                       uint64_t release);
  void OnDescheduleUntilFinished();
  void OnRescheduleAfterFinished();
  void OnParseError();
  void OnContextLost(uint32_t reason);
  void OnGpuCompletedSwapBuffers(gfx::SwapResult result);

  const gpu::CommandBufferNamespace command_buffer_namespace_;
  const gpu::CommandBufferId command_buffer_id_;
  gfx::AcceleratedWidget widget_;
  Client* client_;  // NOT OWNED.
  std::unique_ptr<gpu::CommandBufferService> command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Decoder> decoder_;
  std::unique_ptr<gpu::CommandExecutor> executor_;
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;
  std::unique_ptr<gpu::SyncPointClient> sync_point_client_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<GpuState> gpu_state_;

  scoped_refptr<base::SingleThreadTaskRunner> context_lost_task_runner_;
  base::Callback<void(int32_t)> context_lost_callback_;

  base::TimeTicks process_delayed_work_time_;
  uint32_t previous_processed_num_;
  base::TimeTicks last_idle_time_;

  base::WeakPtrFactory<CommandBufferDriver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferDriver);
};

}  // namespace mus

#endif  // COMPONENTS_GLES2_COMMAND_BUFFER_DRIVER_H_
