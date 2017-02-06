// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_GPU_STATE_H_
#define COMPONENTS_MUS_GLES2_GPU_STATE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/mus/gles2/command_buffer_driver_manager.h"
#include "components/mus/gles2/command_buffer_task_runner.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "ui/gl/gl_share_group.h"

namespace {
class WaitableEvent;
}

namespace mus {

// We need to share these across all CommandBuffer instances so that contexts
// they create can share resources with each other via mailboxes.
class GpuState : public base::RefCountedThreadSafe<GpuState> {
 public:
  GpuState();

  // We run the CommandBufferImpl on the control_task_runner, which forwards
  // most method class to the CommandBufferDriver, which runs on the "driver",
  // thread (i.e., the thread on which GpuImpl instances are created).
  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner() {
    return control_thread_task_runner_;
  }

  void StopThreads();

  const gpu::GpuPreferences& gpu_preferences() const {
    return gpu_preferences_;
  }

  const gpu::GpuDriverBugWorkarounds& gpu_driver_bug_workarounds() const {
    return gpu_driver_bug_workarounds_;
  }

  CommandBufferTaskRunner* command_buffer_task_runner() const {
    return command_buffer_task_runner_.get();
  }

  CommandBufferDriverManager* driver_manager() const {
    return driver_manager_.get();
  }

  // These objects are intended to be used on the "driver" thread (i.e., the
  // thread on which GpuImpl instances are created).
  gl::GLShareGroup* share_group() const { return share_group_.get(); }
  gpu::gles2::MailboxManager* mailbox_manager() const {
    return mailbox_manager_.get();
  }
  gpu::SyncPointManager* sync_point_manager() const {
    return sync_point_manager_.get();
  }

  const gpu::GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  bool HardwareRenderingAvailable() const {
    return hardware_rendering_available_;
  }

 private:
  friend class base::RefCountedThreadSafe<GpuState>;
  ~GpuState();

  void InitializeOnGpuThread(base::WaitableEvent* event);

  void DestroyGpuSpecificStateOnGpuThread();

  // |gpu_thread_| is for executing OS GL calls.
  base::Thread gpu_thread_;
  // |control_thread_| is for mojo incoming calls of CommandBufferImpl.
  base::Thread control_thread_;

  // Same as control_thread_->task_runner(). The TaskRunner is cached as it may
  // be needed during shutdown.
  scoped_refptr<base::SingleThreadTaskRunner> control_thread_task_runner_;

  gpu::GpuPreferences gpu_preferences_;
  const gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
  scoped_refptr<CommandBufferTaskRunner> command_buffer_task_runner_;
  std::unique_ptr<CommandBufferDriverManager> driver_manager_;
  std::unique_ptr<gpu::SyncPointManager> sync_point_manager_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;
  gpu::GPUInfo gpu_info_;
  bool hardware_rendering_available_;
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_GPU_STATE_H_
