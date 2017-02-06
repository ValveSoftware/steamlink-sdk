// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/gpu_state.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "gpu/config/gpu_info_collector.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace mus {

GpuState::GpuState()
    : gpu_thread_("gpu_thread"),
      control_thread_("gpu_command_buffer_control"),
      gpu_driver_bug_workarounds_(base::CommandLine::ForCurrentProcess()),
      hardware_rendering_available_(false) {
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  gpu_thread_.Start();
  control_thread_.Start();
  control_thread_task_runner_ = control_thread_.task_runner();
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&GpuState::InitializeOnGpuThread,
                            base::Unretained(this), &event));
  event.Wait();
}

GpuState::~GpuState() {}

void GpuState::StopThreads() {
  control_thread_.Stop();
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GpuState::DestroyGpuSpecificStateOnGpuThread, this));
  gpu_thread_.Stop();
}

void GpuState::InitializeOnGpuThread(base::WaitableEvent* event) {
#if defined(USE_OZONE)
  ui::OzonePlatform::InitializeForGPU();
#endif
  hardware_rendering_available_ = gl::init::InitializeGLOneOff();
  command_buffer_task_runner_ = new CommandBufferTaskRunner;
  driver_manager_.reset(new CommandBufferDriverManager);
  sync_point_manager_.reset(new gpu::SyncPointManager(true));
  share_group_ = new gl::GLShareGroup;
  mailbox_manager_ = new gpu::gles2::MailboxManagerImpl;

  // TODO(penghuang): investigate why gpu::CollectBasicGraphicsInfo() failed on
  // windows remote desktop.
  const gl::GLImplementation impl = gl::GetGLImplementation();
  if (impl != gl::kGLImplementationNone &&
      impl != gl::kGLImplementationOSMesaGL &&
      impl != gl::kGLImplementationMockGL) {
    gpu::CollectInfoResult result = gpu::CollectBasicGraphicsInfo(&gpu_info_);
    LOG_IF(ERROR, result != gpu::kCollectInfoSuccess)
        << "Collect basic graphics info failed!";
  }
  if (impl != gl::kGLImplementationNone) {
    gpu::CollectInfoResult result = gpu::CollectContextGraphicsInfo(&gpu_info_);
    LOG_IF(ERROR, result != gpu::kCollectInfoSuccess)
        << "Collect context graphics info failed!";
  }
  event->Signal();

}

void GpuState::DestroyGpuSpecificStateOnGpuThread() {
  driver_manager_.reset();
}

}  // namespace mus
