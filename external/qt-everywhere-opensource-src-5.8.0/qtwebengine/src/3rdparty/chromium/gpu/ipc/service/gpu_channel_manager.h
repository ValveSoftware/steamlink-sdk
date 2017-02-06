// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/service/gpu_memory_manager.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "url/gurl.h"

namespace base {
class WaitableEvent;
}

namespace gl {
class GLShareGroup;
}

namespace gpu {
struct GpuPreferences;
class PreemptionFlag;
class SyncPointClient;
class SyncPointManager;
struct SyncToken;
namespace gles2 {
class FramebufferCompletenessCache;
class MailboxManager;
class ProgramCache;
class ShaderTranslatorCache;
}
}

namespace IPC {
struct ChannelHandle;
}

namespace gpu {
class GpuChannel;
class GpuChannelManagerDelegate;
class GpuMemoryBufferFactory;
class GpuWatchdog;

// A GpuChannelManager is a thread responsible for issuing rendering commands
// managing the lifetimes of GPU channels and forwarding IPC requests from the
// browser process to them based on the corresponding renderer ID.
class GPU_EXPORT GpuChannelManager {
 public:
  GpuChannelManager(const GpuPreferences& gpu_preferences,
                    GpuChannelManagerDelegate* delegate,
                    GpuWatchdog* watchdog,
                    base::SingleThreadTaskRunner* task_runner,
                    base::SingleThreadTaskRunner* io_task_runner,
                    base::WaitableEvent* shutdown_event,
                    SyncPointManager* sync_point_manager,
                    GpuMemoryBufferFactory* gpu_memory_buffer_factory);
  virtual ~GpuChannelManager();

  GpuChannelManagerDelegate* delegate() const { return delegate_; }

  IPC::ChannelHandle EstablishChannel(int client_id,
                                      uint64_t client_tracing_id,
                                      bool preempts,
                                      bool allow_view_command_buffers,
                                      bool allow_real_time_streams);

  void PopulateShaderCache(const std::string& shader);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const SyncToken& sync_token);
#if defined(OS_ANDROID)
  void WakeUpGpu();
#endif
  void DestroyAllChannels();

  // Remove the channel for a particular renderer.
  void RemoveChannel(int client_id);

  void LoseAllContexts();
  void MaybeExitOnContextLost();

  const GpuPreferences& gpu_preferences() const {
    return gpu_preferences_;
  }
  const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds() const {
    return gpu_driver_bug_workarounds_;
  }
  gles2::ProgramCache* program_cache();
  gles2::ShaderTranslatorCache* shader_translator_cache();
  gles2::FramebufferCompletenessCache* framebuffer_completeness_cache();

  GpuMemoryManager* gpu_memory_manager() { return &gpu_memory_manager_; }

  GpuChannel* LookupChannel(int32_t client_id) const;

  gl::GLSurface* GetDefaultOffscreenSurface();

  GpuMemoryBufferFactory* gpu_memory_buffer_factory() {
    return gpu_memory_buffer_factory_;
  }

  // Returns the maximum order number for unprocessed IPC messages across all
  // channels.
  uint32_t GetUnprocessedOrderNum() const;

  // Returns the maximum order number for processed IPC messages across all
  // channels.
  uint32_t GetProcessedOrderNum() const;

#if defined(OS_ANDROID)
  void DidAccessGpu();
#endif

  bool is_exiting_for_lost_context() {
    return exiting_for_lost_context_;
  }

 protected:
  virtual std::unique_ptr<GpuChannel> CreateGpuChannel(
      int client_id,
      uint64_t client_tracing_id,
      bool preempts,
      bool allow_view_command_buffers,
      bool allow_real_time_streams);

 public:
  SyncPointManager* sync_point_manager() const {
    return sync_point_manager_;
  }
  gles2::MailboxManager* mailbox_manager() const {
    return mailbox_manager_.get();
  }

  gl::GLShareGroup* share_group() const { return share_group_.get(); }
  void set_share_group(gl::GLShareGroup* share_group);

 protected:
  PreemptionFlag* preemption_flag() const {
    return preemption_flag_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // These objects manage channels to individual renderer processes there is
  // one channel for each renderer process that has connected to this GPU
  // process.
  base::ScopedPtrHashMap<int32_t, std::unique_ptr<GpuChannel>> gpu_channels_;

 private:
  void InternalDestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, int client_id);
  void InternalDestroyGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id,
                                          int client_id);
#if defined(OS_ANDROID)
  void ScheduleWakeUpGpu();
  void DoWakeUpGpu();
#endif

  const GpuPreferences& gpu_preferences_;
  GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;

  GpuChannelManagerDelegate* const delegate_;

  GpuWatchdog* watchdog_;

  base::WaitableEvent* shutdown_event_;

  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gles2::MailboxManager> mailbox_manager_;
  scoped_refptr<PreemptionFlag> preemption_flag_;
  GpuMemoryManager gpu_memory_manager_;
  // SyncPointManager guaranteed to outlive running MessageLoop.
  SyncPointManager* sync_point_manager_;
  std::unique_ptr<SyncPointClient> sync_point_client_waiter_;
  std::unique_ptr<gles2::ProgramCache> program_cache_;
  scoped_refptr<gles2::ShaderTranslatorCache> shader_translator_cache_;
  scoped_refptr<gles2::FramebufferCompletenessCache>
      framebuffer_completeness_cache_;
  scoped_refptr<gl::GLSurface> default_offscreen_surface_;
  GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
#if defined(OS_ANDROID)
  // Last time we know the GPU was powered on. Global for tracking across all
  // transport surfaces.
  base::TimeTicks last_gpu_access_time_;
  base::TimeTicks begin_wake_up_time_;
#endif

  // Set during intentional GPU process shutdown.
  bool exiting_for_lost_context_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<GpuChannelManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelManager);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_MANAGER_H_
