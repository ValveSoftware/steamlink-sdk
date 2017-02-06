// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/generic_shared_memory_id_generator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_switches.h"

namespace content {
namespace {

void HostCreateGpuMemoryBuffer(
    gpu::SurfaceHandle surface_handle,
    GpuProcessHost* host,
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    const BrowserGpuMemoryBufferManager::CreateCallback& callback) {
  host->CreateGpuMemoryBuffer(id, size, format, usage, client_id,
                              surface_handle, callback);
}

void GpuMemoryBufferDeleted(
    scoped_refptr<base::SingleThreadTaskRunner> destruction_task_runner,
    const gpu::GpuMemoryBufferImpl::DestructionCallback& destruction_callback,
    const gpu::SyncToken& sync_token) {
  destruction_task_runner->PostTask(
      FROM_HERE, base::Bind(destruction_callback, sync_token));
}

bool IsNativeGpuMemoryBufferFactoryConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  switch (gpu::GetNativeGpuMemoryBufferType()) {
    case gfx::SHARED_MEMORY_BUFFER:
      return false;
    case gfx::IO_SURFACE_BUFFER:
    case gfx::SURFACE_TEXTURE_BUFFER:
    case gfx::OZONE_NATIVE_PIXMAP:
      return gpu::IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
    default:
      NOTREACHED();
      return false;
  }
}

GpuMemoryBufferConfigurationSet GetNativeGpuMemoryBufferConfigurations() {
  GpuMemoryBufferConfigurationSet configurations;

  if (BrowserGpuMemoryBufferManager::IsNativeGpuMemoryBuffersEnabled()) {
    const gfx::BufferFormat kNativeFormats[] = {
        gfx::BufferFormat::R_8,       gfx::BufferFormat::BGR_565,
        gfx::BufferFormat::RGBA_4444, gfx::BufferFormat::RGBA_8888,
        gfx::BufferFormat::BGRA_8888, gfx::BufferFormat::UYVY_422,
        gfx::BufferFormat::YVU_420,   gfx::BufferFormat::YUV_420_BIPLANAR};
    const gfx::BufferUsage kNativeUsages[] = {
        gfx::BufferUsage::GPU_READ, gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT};
    for (auto& format : kNativeFormats) {
      for (auto& usage : kNativeUsages) {
        if (IsNativeGpuMemoryBufferFactoryConfigurationSupported(format, usage))
          configurations.insert(std::make_pair(format, usage));
      }
    }
  }

#if defined(USE_OZONE) || defined(OS_MACOSX)
  // Disable native buffers only when using Mesa.
  bool force_native_gpu_read_write_formats =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseGL) != gl::kGLImplementationOSMesaName;
#else
  bool force_native_gpu_read_write_formats = false;
#endif
  if (force_native_gpu_read_write_formats) {
    const gfx::BufferFormat kGPUReadWriteFormats[] = {
        gfx::BufferFormat::BGR_565,   gfx::BufferFormat::RGBA_8888,
        gfx::BufferFormat::RGBX_8888, gfx::BufferFormat::BGRA_8888,
        gfx::BufferFormat::BGRX_8888, gfx::BufferFormat::UYVY_422,
        gfx::BufferFormat::YVU_420,   gfx::BufferFormat::YUV_420_BIPLANAR};
    const gfx::BufferUsage kGPUReadWriteUsages[] = {
        gfx::BufferUsage::GPU_READ, gfx::BufferUsage::SCANOUT};
    for (auto& format : kGPUReadWriteFormats) {
      for (auto& usage : kGPUReadWriteUsages) {
        if (IsNativeGpuMemoryBufferFactoryConfigurationSupported(format, usage))
          configurations.insert(std::make_pair(format, usage));
      }
    }
  }

  return configurations;
}

BrowserGpuMemoryBufferManager* g_gpu_memory_buffer_manager = nullptr;

}  // namespace

struct BrowserGpuMemoryBufferManager::CreateGpuMemoryBufferRequest {
  CreateGpuMemoryBufferRequest(const gfx::Size& size,
                               gfx::BufferFormat format,
                               gfx::BufferUsage usage,
                               int client_id,
                               gpu::SurfaceHandle surface_handle)
      : event(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED),
        size(size),
        format(format),
        usage(usage),
        client_id(client_id),
        surface_handle(surface_handle) {}
  ~CreateGpuMemoryBufferRequest() {}
  base::WaitableEvent event;
  gfx::Size size;
  gfx::BufferFormat format;
  gfx::BufferUsage usage;
  int client_id;
  gpu::SurfaceHandle surface_handle;
  std::unique_ptr<gfx::GpuMemoryBuffer> result;
};

struct BrowserGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandleRequest
    : public CreateGpuMemoryBufferRequest {
  CreateGpuMemoryBufferFromHandleRequest(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id)
      : CreateGpuMemoryBufferRequest(size,
                                     format,
                                     gfx::BufferUsage::GPU_READ,
                                     client_id,
                                     gpu::kNullSurfaceHandle),
        handle(handle) {}
  ~CreateGpuMemoryBufferFromHandleRequest() {}
  gfx::GpuMemoryBufferHandle handle;
};

BrowserGpuMemoryBufferManager::BrowserGpuMemoryBufferManager(
    int gpu_client_id,
    uint64_t gpu_client_tracing_id)
    : native_configurations_(GetNativeGpuMemoryBufferConfigurations()),
      gpu_client_id_(gpu_client_id),
      gpu_client_tracing_id_(gpu_client_tracing_id),
      gpu_host_id_(0) {
  DCHECK(!g_gpu_memory_buffer_manager);
  g_gpu_memory_buffer_manager = this;
}

BrowserGpuMemoryBufferManager::~BrowserGpuMemoryBufferManager() {
  g_gpu_memory_buffer_manager = nullptr;
}

// static
BrowserGpuMemoryBufferManager* BrowserGpuMemoryBufferManager::current() {
  return g_gpu_memory_buffer_manager;
}

// static
bool BrowserGpuMemoryBufferManager::IsNativeGpuMemoryBuffersEnabled() {
  // Disable native buffers when using Mesa.
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseGL) == gl::kGLImplementationOSMesaName) {
    return false;
  }

#if defined(OS_MACOSX)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableNativeGpuMemoryBuffers);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNativeGpuMemoryBuffers);
#endif
}

// static
uint32_t BrowserGpuMemoryBufferManager::GetImageTextureTarget(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  GpuMemoryBufferConfigurationSet native_configurations =
      GetNativeGpuMemoryBufferConfigurations();
  if (native_configurations.find(std::make_pair(format, usage)) ==
      native_configurations.end()) {
    return GL_TEXTURE_2D;
  }

  switch (gpu::GetNativeGpuMemoryBufferType()) {
    case gfx::SURFACE_TEXTURE_BUFFER:
    case gfx::OZONE_NATIVE_PIXMAP:
      // GPU memory buffers that are shared with the GL using EGLImages
      // require TEXTURE_EXTERNAL_OES.
      return GL_TEXTURE_EXTERNAL_OES;
    case gfx::IO_SURFACE_BUFFER:
      // IOSurface backed images require GL_TEXTURE_RECTANGLE_ARB.
      return GL_TEXTURE_RECTANGLE_ARB;
    case gfx::SHARED_MEMORY_BUFFER:
      return GL_TEXTURE_2D;
    case gfx::EMPTY_BUFFER:
      NOTREACHED();
      return GL_TEXTURE_2D;
  }

  NOTREACHED();
  return GL_TEXTURE_2D;
}

std::unique_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  return AllocateGpuMemoryBufferForSurface(size, format, usage, surface_handle);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  CreateGpuMemoryBufferFromHandleRequest request(handle, size, format,
                                                 gpu_client_id_);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowserGpuMemoryBufferManager::
                     HandleCreateGpuMemoryBufferFromHandleOnIO,
                 base::Unretained(this),  // Safe as we wait for result below.
                 base::Unretained(&request)));

  // We're blocking the UI thread, which is generally undesirable.
  TRACE_EVENT0(
      "browser",
      "BrowserGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle");
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  request.event.Wait();
  return std::move(request.result);
}

void BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForChildProcess(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    const AllocationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Use service side allocation for native configurations.
  if (IsNativeGpuMemoryBufferConfiguration(format, usage)) {
    CreateGpuMemoryBufferOnIO(
        base::Bind(&HostCreateGpuMemoryBuffer, gpu::kNullSurfaceHandle), id,
        size, format, usage, child_client_id, false, callback);
    return;
  }

  // Early out if we cannot fallback to shared memory buffer.
  if (!gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage) ||
      !gpu::GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(size,
                                                                  format)) {
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  BufferMap& buffers = clients_[child_client_id];

  // Allocate shared memory buffer as fallback.
  auto insert_result = buffers.insert(std::make_pair(
      id, BufferInfo(size, gfx::SHARED_MEMORY_BUFFER, format, usage, 0)));
  if (!insert_result.second) {
    DLOG(ERROR) << "Child process attempted to allocate a GpuMemoryBuffer with "
                   "an existing ID.";
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  callback.Run(gpu::GpuMemoryBufferImplSharedMemory::AllocateForChildProcess(
      id, size, format, child_process_handle));
}

gfx::GpuMemoryBuffer*
BrowserGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return gpu::GpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void BrowserGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

bool BrowserGpuMemoryBufferManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& client : clients_) {
    int client_id = client.first;

    for (const auto& buffer : client.second) {
      if (buffer.second.type == gfx::EMPTY_BUFFER)
        continue;

      gfx::GpuMemoryBufferId buffer_id = buffer.first;
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(base::StringPrintf(
              "gpumemorybuffer/client_%d/buffer_%d", client_id, buffer_id.id));
      if (!dump)
        return false;

      size_t buffer_size_in_bytes = gfx::BufferSizeForBufferFormat(
          buffer.second.size, buffer.second.format);
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      buffer_size_in_bytes);

      // Create the cross-process ownership edge. If the client creates a
      // corresponding dump for the same buffer, this will avoid to
      // double-count them in tracing. If, instead, no other process will emit a
      // dump with the same guid, the segment will be accounted to the browser.
      uint64_t client_tracing_process_id =
          ClientIdToTracingProcessId(client_id);

      base::trace_event::MemoryAllocatorDumpGuid shared_buffer_guid =
          gfx::GetGpuMemoryBufferGUIDForTracing(client_tracing_process_id,
                                                buffer_id);
      pmd->CreateSharedGlobalAllocatorDump(shared_buffer_guid);
      pmd->AddOwnershipEdge(dump->guid(), shared_buffer_guid);
    }
  }

  return true;
}

void BrowserGpuMemoryBufferManager::ChildProcessDeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    base::ProcessHandle child_process_handle,
    int child_client_id,
    const gpu::SyncToken& sync_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DestroyGpuMemoryBufferOnIO(id, child_client_id, sync_token);
}

void BrowserGpuMemoryBufferManager::ProcessRemoved(
    base::ProcessHandle process_handle,
    int client_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ClientMap::iterator client_it = clients_.find(client_id);
  if (client_it == clients_.end())
    return;

  for (const auto& buffer : client_it->second) {
    // This might happen if buffer is currenlty in the process of being
    // allocated. The buffer will in that case be cleaned up when allocation
    // completes.
    if (buffer.second.type == gfx::EMPTY_BUFFER)
      continue;

    GpuProcessHost* host = GpuProcessHost::FromID(buffer.second.gpu_host_id);
    if (host)
      host->DestroyGpuMemoryBuffer(buffer.first, client_id, gpu::SyncToken());
  }

  clients_.erase(client_it);
}

bool BrowserGpuMemoryBufferManager::IsNativeGpuMemoryBufferConfiguration(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) const {
  return native_configurations_.find(std::make_pair(format, usage)) !=
         native_configurations_.end();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForSurface(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

  CreateGpuMemoryBufferRequest request(size, format, usage, gpu_client_id_,
                                       surface_handle);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &BrowserGpuMemoryBufferManager::HandleCreateGpuMemoryBufferOnIO,
          base::Unretained(this),  // Safe as we wait for result below.
          base::Unretained(&request)));

  // We're blocking the UI thread, which is generally undesirable.
  TRACE_EVENT0(
      "browser",
      "BrowserGpuMemoryBufferManager::AllocateGpuMemoryBufferForSurface");
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  request.event.Wait();
  return std::move(request.result);
}

void BrowserGpuMemoryBufferManager::HandleCreateGpuMemoryBufferOnIO(
    CreateGpuMemoryBufferRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  gfx::GpuMemoryBufferId new_id = content::GetNextGenericSharedMemoryId();

  // Use service side allocation for native configurations.
  if (IsNativeGpuMemoryBufferConfiguration(request->format, request->usage)) {
    // Note: Unretained is safe as this is only used for synchronous allocation
    // from a non-IO thread.
    CreateGpuMemoryBufferOnIO(
        base::Bind(&HostCreateGpuMemoryBuffer, request->surface_handle), new_id,
        request->size, request->format, request->usage, request->client_id,
        false,
        base::Bind(
            &BrowserGpuMemoryBufferManager::HandleGpuMemoryBufferCreatedOnIO,
            base::Unretained(this), base::Unretained(request)));
    return;
  }

  DCHECK(gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(request->usage))
      << static_cast<int>(request->usage);

  BufferMap& buffers = clients_[request->client_id];

  // Allocate shared memory buffer as fallback.
  auto insert_result = buffers.insert(std::make_pair(
      new_id, BufferInfo(request->size, gfx::SHARED_MEMORY_BUFFER,
                         request->format, request->usage, 0)));
  DCHECK(insert_result.second);

  // Note: Unretained is safe as IO thread is stopped before manager is
  // destroyed.
  request->result = gpu::GpuMemoryBufferImplSharedMemory::Create(
      new_id, request->size, request->format,
      base::Bind(
          &GpuMemoryBufferDeleted,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          base::Bind(&BrowserGpuMemoryBufferManager::DestroyGpuMemoryBufferOnIO,
                     base::Unretained(this), new_id, request->client_id)));
  request->event.Signal();
}

void BrowserGpuMemoryBufferManager::HandleCreateGpuMemoryBufferFromHandleOnIO(
    CreateGpuMemoryBufferFromHandleRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  gfx::GpuMemoryBufferId new_id = content::GetNextGenericSharedMemoryId();

  BufferMap& buffers = clients_[request->client_id];
  auto insert_result = buffers.insert(std::make_pair(
      new_id, BufferInfo(request->size, request->handle.type,
                         request->format, request->usage, 0)));
  DCHECK(insert_result.second);

  gfx::GpuMemoryBufferHandle handle = request->handle;
  handle.id = new_id;

  // Note: Unretained is safe as IO thread is stopped before manager is
  // destroyed.
  request->result = gpu::GpuMemoryBufferImpl::CreateFromHandle(
      handle, request->size, request->format, request->usage,
      base::Bind(
          &GpuMemoryBufferDeleted,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          base::Bind(&BrowserGpuMemoryBufferManager::DestroyGpuMemoryBufferOnIO,
                     base::Unretained(this), new_id, request->client_id)));
  request->event.Signal();
}

void BrowserGpuMemoryBufferManager::HandleGpuMemoryBufferCreatedOnIO(
    CreateGpuMemoryBufferRequest* request,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Early out if factory failed to create the buffer.
  if (handle.is_null()) {
    request->event.Signal();
    return;
  }

  // Note: Unretained is safe as IO thread is stopped before manager is
  // destroyed.
  request->result = gpu::GpuMemoryBufferImpl::CreateFromHandle(
      handle, request->size, request->format, request->usage,
      base::Bind(
          &GpuMemoryBufferDeleted,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          base::Bind(&BrowserGpuMemoryBufferManager::DestroyGpuMemoryBufferOnIO,
                     base::Unretained(this), handle.id, request->client_id)));
  request->event.Signal();
}

void BrowserGpuMemoryBufferManager::CreateGpuMemoryBufferOnIO(
    const CreateDelegate& create_delegate,
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    bool reused_gpu_process,
    const CreateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               CAUSE_FOR_GPU_LAUNCH_GPU_MEMORY_BUFFER_ALLOCATE);
    if (!host) {
      LOG(ERROR) << "Failed to launch GPU process.";
      callback.Run(gfx::GpuMemoryBufferHandle());
      return;
    }
    gpu_host_id_ = host->host_id();
    reused_gpu_process = false;
  } else {
    if (reused_gpu_process) {
      // We come here if we retried to create the buffer because of a failure
      // in GpuMemoryBufferCreatedOnIO, but we ended up with the same process
      // ID, meaning the failure was not because of a channel error, but
      // another reason. So fail now.
      LOG(ERROR) << "Failed to create GpuMemoryBuffer.";
      callback.Run(gfx::GpuMemoryBufferHandle());
      return;
    }
    reused_gpu_process = true;
  }

  BufferMap& buffers = clients_[client_id];

  // Note: Handling of cases where the client is removed before the allocation
  // completes is less subtle if we set the buffer type to EMPTY_BUFFER here
  // and verify that this has not changed when creation completes.
  auto insert_result = buffers.insert(std::make_pair(
      id, BufferInfo(size, gfx::EMPTY_BUFFER, format, usage, 0)));
  if (!insert_result.second) {
    DLOG(ERROR) << "Child process attempted to create a GpuMemoryBuffer with "
                   "an existing ID.";
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  // Note: Unretained is safe as IO thread is stopped before manager is
  // destroyed.
  create_delegate.Run(
      host, id, size, format, usage, client_id,
      base::Bind(&BrowserGpuMemoryBufferManager::GpuMemoryBufferCreatedOnIO,
                 base::Unretained(this), create_delegate, id, client_id,
                 gpu_host_id_, reused_gpu_process, callback));
}

void BrowserGpuMemoryBufferManager::GpuMemoryBufferCreatedOnIO(
    const CreateDelegate& create_delegate,
    gfx::GpuMemoryBufferId id,
    int client_id,
    int gpu_host_id,
    bool reused_gpu_process,
    const CreateCallback& callback,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ClientMap::iterator client_it = clients_.find(client_id);

  // This can happen if client is removed while the buffer is being allocated.
  if (client_it == clients_.end()) {
    if (!handle.is_null()) {
      GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id);
      if (host)
        host->DestroyGpuMemoryBuffer(handle.id, client_id, gpu::SyncToken());
    }
    callback.Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  BufferMap& buffers = client_it->second;

  BufferMap::iterator buffer_it = buffers.find(id);
  DCHECK(buffer_it != buffers.end());
  DCHECK_EQ(buffer_it->second.type, gfx::EMPTY_BUFFER);

  // If the handle isn't valid, that means that the GPU process crashed or is
  // misbehaving.
  bool valid_handle = !handle.is_null() && handle.id == id;
  if (!valid_handle) {
    // If we failed after re-using the GPU process, it may have died in the
    // mean time. Retry to have a chance to create a fresh GPU process.
    if (handle.is_null() && reused_gpu_process) {
      DVLOG(1) << "Failed to create buffer through existing GPU process. "
                  "Trying to restart GPU process.";
      // If the GPU process has already been restarted, retry without failure
      // when GPU process host ID already exists.
      if (gpu_host_id != gpu_host_id_)
        reused_gpu_process = false;
      gfx::Size size = buffer_it->second.size;
      gfx::BufferFormat format = buffer_it->second.format;
      gfx::BufferUsage usage = buffer_it->second.usage;
      // Remove the buffer entry and call CreateGpuMemoryBufferOnIO again.
      buffers.erase(buffer_it);
      CreateGpuMemoryBufferOnIO(create_delegate, id, size, format, usage,
                                client_id, reused_gpu_process, callback);
    } else {
      // Remove the buffer entry and run the allocation callback with an empty
      // handle to indicate failure.
      buffers.erase(buffer_it);
      callback.Run(gfx::GpuMemoryBufferHandle());
    }
    return;
  }

  // Store the type and host id of this buffer so it can be cleaned up if the
  // client is removed.
  buffer_it->second.type = handle.type;
  buffer_it->second.gpu_host_id = gpu_host_id;

  callback.Run(handle);
}

void BrowserGpuMemoryBufferManager::DestroyGpuMemoryBufferOnIO(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(clients_.find(client_id) != clients_.end());

  BufferMap& buffers = clients_[client_id];

  BufferMap::iterator buffer_it = buffers.find(id);
  if (buffer_it == buffers.end()) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer ID for client.";
    return;
  }

  // This can happen if a client managed to call this while a buffer is in the
  // process of being allocated.
  if (buffer_it->second.type == gfx::EMPTY_BUFFER) {
    LOG(ERROR) << "Invalid GpuMemoryBuffer type.";
    return;
  }

  GpuProcessHost* host = GpuProcessHost::FromID(buffer_it->second.gpu_host_id);
  if (host)
    host->DestroyGpuMemoryBuffer(id, client_id, sync_token);

  buffers.erase(buffer_it);
}

uint64_t BrowserGpuMemoryBufferManager::ClientIdToTracingProcessId(
    int client_id) const {
  if (client_id == gpu_client_id_) {
    // The gpu_client uses a fixed tracing ID.
    return gpu_client_tracing_id_;
  }

  // In normal cases, |client_id| is a child process id, so we can perform
  // the standard conversion.
  return ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
      client_id);
}

BrowserGpuMemoryBufferManager::BufferInfo::BufferInfo() = default;

BrowserGpuMemoryBufferManager::BufferInfo::BufferInfo(
    const gfx::Size& size,
    gfx::GpuMemoryBufferType type,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int gpu_host_id)
    : size(size),
      type(type),
      format(format),
      usage(usage),
      gpu_host_id(gpu_host_id) {}

BrowserGpuMemoryBufferManager::BufferInfo::BufferInfo(const BufferInfo& other) =
    default;

BrowserGpuMemoryBufferManager::BufferInfo::~BufferInfo() {}

}  // namespace content
