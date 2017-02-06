// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/gpu_service.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/common/switches.h"
#include "components/mus/public/interfaces/gpu_service.mojom.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/shell/public/cpp/connector.h"

namespace mus {

namespace {

void PostTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
              const tracked_objects::Location& from_here,
              const base::Closure& callback) {
  runner->PostTask(from_here, callback);
}

GpuService* g_gpu_service = nullptr;
}

GpuService::GpuService(shell::Connector* connector)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      connector_(connector),
      shutdown_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      io_thread_("GPUIOThread"),
      gpu_memory_buffer_manager_(new MojoGpuMemoryBufferManager),
      is_establishing_(false),
      establishing_condition_(&lock_) {
  DCHECK(main_task_runner_);
  DCHECK(connector_);
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
  CHECK(io_thread_.StartWithOptions(thread_options));
}

GpuService::~GpuService() {
  DCHECK(IsMainThread());
  if (gpu_channel_)
    gpu_channel_->DestroyChannel();
}

// static
bool GpuService::UseChromeGpuCommandBuffer() {
// TODO(penghuang): Kludge: Running with Chrome GPU command buffer by default
// breaks unit tests on Windows
#if defined(OS_WIN)
  return false;
#else
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMojoGpuCommandBufferInMus);
#endif
}

// static
void GpuService::Initialize(shell::Connector* connector) {
  DCHECK(!g_gpu_service);
  g_gpu_service = new GpuService(connector);
}

// static
void GpuService::Terminate() {
  DCHECK(g_gpu_service);
  delete g_gpu_service;
  g_gpu_service = nullptr;
}

// static
GpuService* GpuService::GetInstance() {
  DCHECK(g_gpu_service);
  return g_gpu_service;
}

void GpuService::EstablishGpuChannel(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  auto runner = base::ThreadTaskRunnerHandle::Get();
  if (GetGpuChannelLocked()) {
    runner->PostTask(FROM_HERE, callback);
    return;
  }

  base::Closure wrapper_callback =
      IsMainThread() ? callback
                     : base::Bind(PostTask, runner, FROM_HERE, callback);
  establish_callbacks_.push_back(wrapper_callback);

  if (!is_establishing_) {
    is_establishing_ = true;
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&GpuService::EstablishGpuChannelOnMainThread,
                              base::Unretained(this)));
  }
}

scoped_refptr<gpu::GpuChannelHost> GpuService::EstablishGpuChannelSync() {
  base::AutoLock auto_lock(lock_);
  if (GetGpuChannelLocked())
    return gpu_channel_;

  if (IsMainThread()) {
    is_establishing_ = true;
    EstablishGpuChannelOnMainThreadSyncLocked();
  } else {
    if (!is_establishing_) {
      // Create an establishing gpu channel task, if there isn't one.
      is_establishing_ = true;
      main_task_runner_->PostTask(
          FROM_HERE, base::Bind(&GpuService::EstablishGpuChannelOnMainThread,
                                base::Unretained(this)));
    }

    // Wait until the pending establishing task is finished.
    do {
      establishing_condition_.Wait();
    } while (is_establishing_);
  }
  return gpu_channel_;
}

scoped_refptr<gpu::GpuChannelHost> GpuService::GetGpuChannel() {
  base::AutoLock auto_lock(lock_);
  return GetGpuChannelLocked();
}

scoped_refptr<gpu::GpuChannelHost> GpuService::GetGpuChannelLocked() {
  if (gpu_channel_ && gpu_channel_->IsLost()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&gpu::GpuChannelHost::DestroyChannel, gpu_channel_));
    gpu_channel_ = nullptr;
  }
  return gpu_channel_;
}

void GpuService::EstablishGpuChannelOnMainThread() {
  base::AutoLock auto_lock(lock_);
  DCHECK(IsMainThread());

  // In GpuService::EstablishGpuChannelOnMainThreadSyncLocked(), we use the sync
  // mojo EstablishGpuChannel call, after that call the gpu_service_ will be
  // reset immediatelly. So gpu_service_ should be always null here.
  DCHECK(!gpu_service_);

  // is_establishing_ is false, it means GpuService::EstablishGpuChannelSync()
  // has been used, and we don't need try to establish a new GPU channel
  // anymore.
  if (!is_establishing_)
    return;

  connector_->ConnectToInterface("mojo:mus", &gpu_service_);
  const bool locked = false;
  gpu_service_->EstablishGpuChannel(
      base::Bind(&GpuService::EstablishGpuChannelOnMainThreadDone,
                 base::Unretained(this), locked));
}

void GpuService::EstablishGpuChannelOnMainThreadSyncLocked() {
  DCHECK(IsMainThread());
  DCHECK(is_establishing_);

  // In browser process, EstablishGpuChannelSync() is only used by testing &
  // GpuProcessTransportFactory::GetGLHelper(). For GetGLHelper(), it expects
  // the gpu channel has been established, so it should not reach here.
  // For testing, the  asyc method should not be used.
  // In renderer process, we only use EstablishGpuChannelSync().
  // So the gpu_service_ should be null here.
  DCHECK(!gpu_service_);

  int client_id = 0;
  mojom::ChannelHandlePtr channel_handle;
  mojom::GpuInfoPtr gpu_info;
  connector_->ConnectToInterface("mojo:mus", &gpu_service_);
  {
    base::AutoUnlock auto_unlock(lock_);
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    if (!gpu_service_->EstablishGpuChannel(&client_id, &channel_handle,
                                           &gpu_info)) {
      DLOG(WARNING)
          << "Channel encountered error while establishing gpu channel.";
      return;
    }
  }
  const bool locked = true;
  EstablishGpuChannelOnMainThreadDone(
      locked, client_id, std::move(channel_handle), std::move(gpu_info));
}

void GpuService::EstablishGpuChannelOnMainThreadDone(
    bool locked,
    int client_id,
    mojom::ChannelHandlePtr channel_handle,
    mojom::GpuInfoPtr gpu_info) {
  DCHECK(IsMainThread());
  scoped_refptr<gpu::GpuChannelHost> gpu_channel;
  if (client_id) {
    // TODO(penghuang): Get the real gpu info from mus.
    gpu_channel = gpu::GpuChannelHost::Create(
        this, client_id, gpu::GPUInfo(),
        channel_handle.To<IPC::ChannelHandle>(), &shutdown_event_,
        gpu_memory_buffer_manager_.get());
  }

  auto auto_lock = base::WrapUnique<base::AutoLock>(
      locked ? nullptr : new base::AutoLock(lock_));
  DCHECK(is_establishing_);
  DCHECK(!gpu_channel_);

  is_establishing_ = false;
  gpu_channel_ = gpu_channel;
  establishing_condition_.Broadcast();

  for (const auto& i : establish_callbacks_)
    i.Run();
  establish_callbacks_.clear();
  gpu_service_.reset();
}

bool GpuService::IsMainThread() {
  return main_task_runner_->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuService::GetIOThreadTaskRunner() {
  return io_thread_.task_runner();
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

}  // namespace mus
