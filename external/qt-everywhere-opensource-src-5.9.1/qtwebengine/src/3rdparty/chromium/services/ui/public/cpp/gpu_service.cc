// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/gpu_service.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/switches.h"
#include "services/ui/public/cpp/mojo_gpu_memory_buffer_manager.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"

namespace ui {

GpuService::GpuService(service_manager::Connector* connector,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(std::move(task_runner)),
      connector_(connector),
      shutdown_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      gpu_memory_buffer_manager_(new MojoGpuMemoryBufferManager(connector_)) {
  DCHECK(main_task_runner_);
  DCHECK(connector_);
  if (!io_task_runner_) {
    io_thread_.reset(new base::Thread("GPUIOThread"));
    base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
    thread_options.priority = base::ThreadPriority::NORMAL;
    CHECK(io_thread_->StartWithOptions(thread_options));
    io_task_runner_ = io_thread_->task_runner();
  }
}

GpuService::~GpuService() {
  DCHECK(IsMainThread());
  for (const auto& callback : establish_callbacks_)
    callback.Run(nullptr);
  shutdown_event_.Signal();
  if (gpu_channel_)
    gpu_channel_->DestroyChannel();
}

// static
std::unique_ptr<GpuService> GpuService::Create(
    service_manager::Connector* connector,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return base::WrapUnique(new GpuService(connector, std::move(task_runner)));
}

void GpuService::EstablishGpuChannel(
    const gpu::GpuChannelEstablishedCallback& callback) {
  DCHECK(IsMainThread());
  scoped_refptr<gpu::GpuChannelHost> channel = GetGpuChannel();
  if (channel) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(callback, std::move(channel)));
    return;
  }
  establish_callbacks_.push_back(callback);
  if (gpu_service_)
    return;

  connector_->ConnectToInterface(ui::mojom::kServiceName, &gpu_service_);
  gpu_service_->EstablishGpuChannel(
      base::Bind(&GpuService::OnEstablishedGpuChannel, base::Unretained(this)));
}

scoped_refptr<gpu::GpuChannelHost> GpuService::EstablishGpuChannelSync() {
  DCHECK(IsMainThread());
  if (GetGpuChannel())
    return gpu_channel_;

  int client_id = 0;
  mojo::ScopedMessagePipeHandle channel_handle;
  gpu::GPUInfo gpu_info;
  connector_->ConnectToInterface(ui::mojom::kServiceName, &gpu_service_);

  mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
  if (!gpu_service_->EstablishGpuChannel(&client_id, &channel_handle,
                                         &gpu_info)) {
    DLOG(WARNING)
        << "Channel encountered error while establishing gpu channel.";
    return nullptr;
  }
  OnEstablishedGpuChannel(client_id, std::move(channel_handle), gpu_info);
  return gpu_channel_;
}

gpu::GpuMemoryBufferManager* GpuService::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_.get();
}

scoped_refptr<gpu::GpuChannelHost> GpuService::GetGpuChannel() {
  DCHECK(IsMainThread());
  if (gpu_channel_ && gpu_channel_->IsLost()) {
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }
  return gpu_channel_;
}

void GpuService::OnEstablishedGpuChannel(
    int client_id,
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info) {
  DCHECK(IsMainThread());
  DCHECK(gpu_service_.get());
  DCHECK(!gpu_channel_);

  if (client_id) {
    gpu_channel_ = gpu::GpuChannelHost::Create(
        this, client_id, gpu_info, IPC::ChannelHandle(channel_handle.release()),
        &shutdown_event_, gpu_memory_buffer_manager_.get());
  }

  gpu_service_.reset();
  for (const auto& i : establish_callbacks_)
    i.Run(gpu_channel_);
  establish_callbacks_.clear();
}

bool GpuService::IsMainThread() {
  return main_task_runner_->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuService::GetIOThreadTaskRunner() {
  return io_task_runner_;
}

std::unique_ptr<base::SharedMemory> GpuService::AllocateSharedMemory(
    size_t size) {
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(size);
  if (!handle.is_valid())
    return nullptr;

  base::SharedMemoryHandle platform_handle;
  size_t shared_memory_size;
  bool readonly;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &platform_handle, &shared_memory_size, &readonly);
  if (result != MOJO_RESULT_OK)
    return nullptr;
  DCHECK_EQ(shared_memory_size, size);

  return base::MakeUnique<base::SharedMemory>(platform_handle, readonly);
}

}  // namespace ui
