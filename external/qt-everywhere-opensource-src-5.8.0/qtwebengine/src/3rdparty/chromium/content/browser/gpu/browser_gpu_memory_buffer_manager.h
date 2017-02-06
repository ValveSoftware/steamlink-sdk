// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "base/macros.h"
#include "base/trace_event/memory_dump_provider.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/surface_handle.h"

namespace content {

using GpuMemoryBufferConfigurationKey =
    std::pair<gfx::BufferFormat, gfx::BufferUsage>;
using GpuMemoryBufferConfigurationSet =
    base::hash_set<GpuMemoryBufferConfigurationKey>;

}  // content

namespace BASE_HASH_NAMESPACE {

template <>
struct hash<content::GpuMemoryBufferConfigurationKey> {
  size_t operator()(const content::GpuMemoryBufferConfigurationKey& key) const {
    return base::HashInts(static_cast<int>(key.first),
                          static_cast<int>(key.second));
  }
};

}  // namespace BASE_HASH_NAMESPACE

namespace content {
class GpuProcessHost;

class CONTENT_EXPORT BrowserGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  using CreateCallback =
      base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>;
  using AllocationCallback = CreateCallback;

  BrowserGpuMemoryBufferManager(int gpu_client_id,
                                uint64_t gpu_client_tracing_id);
  ~BrowserGpuMemoryBufferManager() override;

  static BrowserGpuMemoryBufferManager* current();

  static bool IsNativeGpuMemoryBuffersEnabled();

  static uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                        gfx::BufferUsage usage);

  // Overridden from gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void AllocateGpuMemoryBufferForChildProcess(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      base::ProcessHandle child_process_handle,
      int child_client_id,
      const AllocationCallback& callback);
  void ChildProcessDeletedGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      base::ProcessHandle child_process_handle,
      int child_client_id,
      const gpu::SyncToken& sync_token);
  void ProcessRemoved(base::ProcessHandle process_handle, int client_id);

  bool IsNativeGpuMemoryBufferConfiguration(gfx::BufferFormat format,
                                            gfx::BufferUsage usage) const;

 private:
  struct BufferInfo {
    BufferInfo();
    BufferInfo(const gfx::Size& size,
               gfx::GpuMemoryBufferType type,
               gfx::BufferFormat format,
               gfx::BufferUsage usage,
               int gpu_host_id);
    BufferInfo(const BufferInfo& other);
    ~BufferInfo();

    gfx::Size size;
    gfx::GpuMemoryBufferType type = gfx::EMPTY_BUFFER;
    gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
    gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
    int gpu_host_id = 0;
  };

  struct CreateGpuMemoryBufferRequest;
  struct CreateGpuMemoryBufferFromHandleRequest;

  using CreateDelegate = base::Callback<void(GpuProcessHost* host,
                                             gfx::GpuMemoryBufferId id,
                                             const gfx::Size& size,
                                             gfx::BufferFormat format,
                                             gfx::BufferUsage usage,
                                             int client_id,
                                             const CreateCallback& callback)>;

  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferForSurface(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle);

  // Functions that handle synchronous buffer creation requests.
  void HandleCreateGpuMemoryBufferOnIO(CreateGpuMemoryBufferRequest* request);
  void HandleCreateGpuMemoryBufferFromHandleOnIO(
      CreateGpuMemoryBufferFromHandleRequest* request);
  void HandleGpuMemoryBufferCreatedOnIO(
      CreateGpuMemoryBufferRequest* request,
      const gfx::GpuMemoryBufferHandle& handle);

  // Functions that implement asynchronous buffer creation.
  void CreateGpuMemoryBufferOnIO(const CreateDelegate& create_delegate,
                                 gfx::GpuMemoryBufferId id,
                                 const gfx::Size& size,
                                 gfx::BufferFormat format,
                                 gfx::BufferUsage usage,
                                 int client_id,
                                 bool reused_gpu_process,
                                 const CreateCallback& callback);
  void GpuMemoryBufferCreatedOnIO(const CreateDelegate& create_delegate,
                                  gfx::GpuMemoryBufferId id,
                                  int client_id,
                                  int gpu_host_id,
                                  bool reused_gpu_process,
                                  const CreateCallback& callback,
                                  const gfx::GpuMemoryBufferHandle& handle);
  void DestroyGpuMemoryBufferOnIO(gfx::GpuMemoryBufferId id,
                                  int client_id,
                                  const gpu::SyncToken& sync_token);

  uint64_t ClientIdToTracingProcessId(int client_id) const;

  const GpuMemoryBufferConfigurationSet native_configurations_;
  const int gpu_client_id_;
  const uint64_t gpu_client_tracing_id_;

  // The GPU process host ID. This should only be accessed on the IO thread.
  int gpu_host_id_;

  // Stores info about buffers for all clients. This should only be accessed
  // on the IO thread.
  using BufferMap = base::hash_map<gfx::GpuMemoryBufferId, BufferInfo>;
  using ClientMap = base::hash_map<int, BufferMap>;
  ClientMap clients_;

  DISALLOW_COPY_AND_ASSIGN(BrowserGpuMemoryBufferManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_MEMORY_BUFFER_MANAGER_H_
