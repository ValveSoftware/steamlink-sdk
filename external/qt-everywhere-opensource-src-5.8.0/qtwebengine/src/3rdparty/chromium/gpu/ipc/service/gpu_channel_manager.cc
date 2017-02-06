// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_memory_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

namespace {
#if defined(OS_ANDROID)
// Amount of time we expect the GPU to stay powered up without being used.
const int kMaxGpuIdleTimeMs = 40;
// Maximum amount of time we keep pinging the GPU waiting for the client to
// draw.
const int kMaxKeepAliveTimeMs = 200;
#endif

}

GpuChannelManager::GpuChannelManager(
    const GpuPreferences& gpu_preferences,
    GpuChannelManagerDelegate* delegate,
    GpuWatchdog* watchdog,
    base::SingleThreadTaskRunner* task_runner,
    base::SingleThreadTaskRunner* io_task_runner,
    base::WaitableEvent* shutdown_event,
    SyncPointManager* sync_point_manager,
    GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      gpu_preferences_(gpu_preferences),
      gpu_driver_bug_workarounds_(base::CommandLine::ForCurrentProcess()),
      delegate_(delegate),
      watchdog_(watchdog),
      shutdown_event_(shutdown_event),
      share_group_(new gl::GLShareGroup),
      mailbox_manager_(gles2::MailboxManager::Create(gpu_preferences)),
      gpu_memory_manager_(this),
      sync_point_manager_(sync_point_manager),
      sync_point_client_waiter_(
          sync_point_manager->CreateSyncPointClientWaiter()),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      exiting_for_lost_context_(false),
      weak_factory_(this) {
  DCHECK(task_runner);
  DCHECK(io_task_runner);
  if (gpu_preferences_.ui_prioritize_in_gpu_process)
    preemption_flag_ = new PreemptionFlag;
}

GpuChannelManager::~GpuChannelManager() {
  // Destroy channels before anything else because of dependencies.
  gpu_channels_.clear();
  if (default_offscreen_surface_.get()) {
    default_offscreen_surface_->Destroy();
    default_offscreen_surface_ = NULL;
  }
}

gles2::ProgramCache* GpuChannelManager::program_cache() {
  if (!program_cache_.get() &&
      (gl::g_driver_gl.ext.b_GL_ARB_get_program_binary ||
       gl::g_driver_gl.ext.b_GL_OES_get_program_binary) &&
      !gpu_preferences_.disable_gpu_program_cache) {
    program_cache_.reset(new gles2::MemoryProgramCache(
          gpu_preferences_.gpu_program_cache_size,
          gpu_preferences_.disable_gpu_shader_disk_cache));
  }
  return program_cache_.get();
}

gles2::ShaderTranslatorCache*
GpuChannelManager::shader_translator_cache() {
  if (!shader_translator_cache_.get()) {
    shader_translator_cache_ =
        new gles2::ShaderTranslatorCache(gpu_preferences_);
  }
  return shader_translator_cache_.get();
}

gles2::FramebufferCompletenessCache*
GpuChannelManager::framebuffer_completeness_cache() {
  if (!framebuffer_completeness_cache_.get())
    framebuffer_completeness_cache_ =
        new gles2::FramebufferCompletenessCache;
  return framebuffer_completeness_cache_.get();
}

void GpuChannelManager::RemoveChannel(int client_id) {
  delegate_->DidDestroyChannel(client_id);
  gpu_channels_.erase(client_id);
}

GpuChannel* GpuChannelManager::LookupChannel(int32_t client_id) const {
  const auto& it = gpu_channels_.find(client_id);
  return it != gpu_channels_.end() ? it->second : nullptr;
}

void GpuChannelManager::set_share_group(gl::GLShareGroup* share_group) {
    share_group_ = share_group;
}

std::unique_ptr<GpuChannel> GpuChannelManager::CreateGpuChannel(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams) {
  return base::WrapUnique(
      new GpuChannel(this, sync_point_manager(), watchdog_, share_group(),
                     mailbox_manager(), preempts ? preemption_flag() : nullptr,
                     preempts ? nullptr : preemption_flag(), task_runner_.get(),
                     io_task_runner_.get(), client_id, client_tracing_id,
                     allow_view_command_buffers, allow_real_time_streams));
}

IPC::ChannelHandle GpuChannelManager::EstablishChannel(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams) {
  std::unique_ptr<GpuChannel> channel(
      CreateGpuChannel(client_id, client_tracing_id, preempts,
                       allow_view_command_buffers, allow_real_time_streams));
  IPC::ChannelHandle channel_handle = channel->Init(shutdown_event_);
  gpu_channels_.set(client_id, std::move(channel));
  return channel_handle;
}

void GpuChannelManager::InternalDestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuChannelManager::InternalDestroyGpuMemoryBufferOnIO,
                 base::Unretained(this), id, client_id));
}

void GpuChannelManager::InternalDestroyGpuMemoryBufferOnIO(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  gpu_memory_buffer_factory_->DestroyGpuMemoryBuffer(id, client_id);
}

void GpuChannelManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const SyncToken& sync_token) {
  if (sync_token.HasData()) {
    scoped_refptr<SyncPointClientState> release_state =
        sync_point_manager()->GetSyncPointClientState(
            sync_token.namespace_id(), sync_token.command_buffer_id());
    if (release_state) {
      sync_point_client_waiter_->WaitOutOfOrder(
          release_state.get(), sync_token.release_count(),
          base::Bind(&GpuChannelManager::InternalDestroyGpuMemoryBuffer,
                     base::Unretained(this), id, client_id));
      return;
    }
  }

  // No sync token or invalid sync token, destroy immediately.
  InternalDestroyGpuMemoryBuffer(id, client_id);
}

void GpuChannelManager::PopulateShaderCache(const std::string& program_proto) {
  if (program_cache())
    program_cache()->LoadProgram(program_proto);
}

uint32_t GpuChannelManager::GetUnprocessedOrderNum() const {
  uint32_t unprocessed_order_num = 0;
  for (auto& kv : gpu_channels_) {
    unprocessed_order_num =
        std::max(unprocessed_order_num, kv.second->GetUnprocessedOrderNum());
  }
  return unprocessed_order_num;
}

uint32_t GpuChannelManager::GetProcessedOrderNum() const {
  uint32_t processed_order_num = 0;
  for (auto& kv : gpu_channels_) {
    processed_order_num =
        std::max(processed_order_num, kv.second->GetProcessedOrderNum());
  }
  return processed_order_num;
}

void GpuChannelManager::LoseAllContexts() {
  for (auto& kv : gpu_channels_) {
    kv.second->MarkAllContextsLost();
  }
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&GpuChannelManager::DestroyAllChannels,
                                    weak_factory_.GetWeakPtr()));
}

void GpuChannelManager::MaybeExitOnContextLost() {
  if (!gpu_preferences().single_process && !gpu_preferences().in_process_gpu) {
    LOG(ERROR) << "Exiting GPU process because some drivers cannot recover"
               << " from problems.";
    // Signal the message loop to quit to shut down other threads
    // gracefully.
    base::MessageLoop::current()->QuitNow();
    exiting_for_lost_context_ = true;
  }
}

void GpuChannelManager::DestroyAllChannels() {
  gpu_channels_.clear();
}

gl::GLSurface* GpuChannelManager::GetDefaultOffscreenSurface() {
  if (!default_offscreen_surface_.get()) {
    default_offscreen_surface_ =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
  }
  return default_offscreen_surface_.get();
}

#if defined(OS_ANDROID)
void GpuChannelManager::DidAccessGpu() {
  last_gpu_access_time_ = base::TimeTicks::Now();
}

void GpuChannelManager::WakeUpGpu() {
  begin_wake_up_time_ = base::TimeTicks::Now();
  ScheduleWakeUpGpu();
}

void GpuChannelManager::ScheduleWakeUpGpu() {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT2("gpu", "GpuChannelManager::ScheduleWakeUp",
               "idle_time", (now - last_gpu_access_time_).InMilliseconds(),
               "keep_awake_time", (now - begin_wake_up_time_).InMilliseconds());
  if (now - last_gpu_access_time_ <
      base::TimeDelta::FromMilliseconds(kMaxGpuIdleTimeMs))
    return;
  if (now - begin_wake_up_time_ >
      base::TimeDelta::FromMilliseconds(kMaxKeepAliveTimeMs))
    return;

  DoWakeUpGpu();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&GpuChannelManager::ScheduleWakeUpGpu,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kMaxGpuIdleTimeMs));
}

void GpuChannelManager::DoWakeUpGpu() {
  const GpuCommandBufferStub* stub = nullptr;
  for (const auto& kv : gpu_channels_) {
    const GpuChannel* channel = kv.second;
    stub = channel->GetOneStub();
    if (stub) {
      DCHECK(stub->decoder());
      break;
    }
  }
  if (!stub || !stub->decoder()->MakeCurrent())
    return;
  glFinish();
  DidAccessGpu();
}
#endif

}  // namespace gpu
