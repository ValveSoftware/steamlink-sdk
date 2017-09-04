// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"

extern "C" typedef struct _ClientBuffer* ClientBuffer;

namespace base {
class Lock;
}

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
class GpuControlClient;
struct SyncToken;

// Common interface for GpuControl implementations.
class GPU_EXPORT GpuControl {
 public:
  GpuControl() {}
  virtual ~GpuControl() {}

  virtual void SetGpuControlClient(GpuControlClient* gpu_control_client) = 0;

  virtual Capabilities GetCapabilities() = 0;

  // Create an image for a client buffer with the given dimensions and
  // format. Returns its ID or -1 on error.
  virtual int32_t CreateImage(ClientBuffer buffer,
                              size_t width,
                              size_t height,
                              unsigned internalformat) = 0;

  // Destroy an image. The ID must be positive.
  virtual void DestroyImage(int32_t id) = 0;

  // Create a gpu memory buffer backed image with the given dimensions and
  // format for |usage|. Returns its ID or -1 on error.
  virtual int32_t CreateGpuMemoryBufferImage(size_t width,
                                             size_t height,
                                             unsigned internalformat,
                                             unsigned usage) = 0;

  // Runs |callback| when a query created via glCreateQueryEXT() has cleared
  // passed the glEndQueryEXT() point.
  virtual void SignalQuery(uint32_t query, const base::Closure& callback) = 0;

  // Sets a lock this will be held on every callback from the GPU
  // implementation. This lock must be set and must be held on every call into
  // the GPU implementation if it is to be used from multiple threads. This
  // may not be supported with all implementations.
  virtual void SetLock(base::Lock*) = 0;

  // When this function returns it ensures all previously flushed work is
  // visible by the service. This command does this by sending a synchronous
  // IPC. Note just because the work is visible to the server does not mean
  // that it has been processed. This is only relevant for out of process
  // services and will be treated as a NOP for in process command buffers.
  virtual void EnsureWorkVisible() = 0;

  // The namespace and command buffer ID forms a unique pair for all existing
  // GpuControl (on client) and matches for the corresponding command buffer
  // (on server) in a single server process. The extra command buffer data can
  // be used for extra identification purposes. One usage is to store some
  // extra field to identify unverified sync tokens for the implementation of
  // the CanWaitUnverifiedSyncToken() function.
  virtual CommandBufferNamespace GetNamespaceID() const = 0;
  virtual CommandBufferId GetCommandBufferID() const = 0;
  virtual int32_t GetExtraCommandBufferData() const = 0;

  // Fence Syncs use release counters at a context level, these fence syncs
  // need to be flushed before they can be shared with other contexts across
  // channels. Subclasses should implement these functions and take care of
  // figuring out when a fence sync has been flushed. The difference between
  // IsFenceSyncFlushed and IsFenceSyncFlushReceived, one is testing is the
  // client has issued the flush, and the other is testing if the service
  // has received the flush.
  virtual uint64_t GenerateFenceSyncRelease() = 0;
  virtual bool IsFenceSyncRelease(uint64_t release) = 0;
  virtual bool IsFenceSyncFlushed(uint64_t release) = 0;
  virtual bool IsFenceSyncFlushReceived(uint64_t release) = 0;

  // Runs |callback| when sync token is signalled.
  virtual void SignalSyncToken(const SyncToken& sync_token,
                               const base::Closure& callback) = 0;

  // Under some circumstances a sync token may be used which has not been
  // verified to have been flushed. For example, fence syncs queued on the
  // same channel as the wait command guarantee that the fence sync will
  // be enqueued first so does not need to be flushed.
  virtual bool CanWaitUnverifiedSyncToken(const SyncToken* sync_token) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuControl);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_H_
