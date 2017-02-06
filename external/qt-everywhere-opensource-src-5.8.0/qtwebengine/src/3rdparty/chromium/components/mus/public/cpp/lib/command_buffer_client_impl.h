// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_COMMAND_BUFFER_CLIENT_IMPL_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_COMMAND_BUFFER_CLIENT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class RunLoop;
}

namespace mus {
class CommandBufferClientImpl;

class CommandBufferClientImpl : public mus::mojom::CommandBufferClient,
                                public gpu::CommandBuffer,
                                public gpu::GpuControl {
 public:
  explicit CommandBufferClientImpl(
      const std::vector<int32_t>& attribs,
      mus::mojom::CommandBufferPtr command_buffer_ptr);
  ~CommandBufferClientImpl() override;
  bool Initialize();

  // CommandBuffer implementation:
  State GetLastState() override;
  int32_t GetLastToken() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  void WaitForTokenInRange(int32_t start, int32_t end) override;
  void WaitForGetOffsetInRange(int32_t start, int32_t end) override;
  void SetGetBuffer(int32_t shm_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32_t* id) override;
  void DestroyTransferBuffer(int32_t id) override;

  // gpu::GpuControl implementation:
  void SetGpuControlClient(gpu::GpuControlClient*) override;
  gpu::Capabilities GetCapabilities() override;
  int32_t CreateImage(ClientBuffer buffer,
                      size_t width,
                      size_t height,
                      unsigned internalformat) override;
  void DestroyImage(int32_t id) override;
  int32_t CreateGpuMemoryBufferImage(size_t width,
                                     size_t height,
                                     unsigned internalformat,
                                     unsigned usage) override;
  int32_t GetImageGpuMemoryBufferId(unsigned image_id) override;
  void SignalQuery(uint32_t query, const base::Closure& callback) override;
  void SetLock(base::Lock*) override;
  void EnsureWorkVisible() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  gpu::CommandBufferId GetCommandBufferID() const override;
  int32_t GetExtraCommandBufferData() const override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncRelease(uint64_t release) override;
  bool IsFenceSyncFlushed(uint64_t release) override;
  bool IsFenceSyncFlushReceived(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken* sync_token) override;

 private:
  // mus::mojom::CommandBufferClient implementation:
  void Destroyed(int32_t lost_reason, int32_t error) override;
  void SignalAck(uint32_t id) override;
  void SwapBuffersCompleted(int32_t result) override;
  void UpdateState(const gpu::CommandBuffer::State& state) override;
  void UpdateVSyncParameters(int64_t timebase, int64_t interval) override;

  void TryUpdateState();
  void MakeProgressAndUpdateState();

  gpu::CommandBufferSharedState* shared_state() const {
    return reinterpret_cast<gpu::CommandBufferSharedState*>(
        shared_state_.get());
  }

  gpu::GpuControlClient* gpu_control_client_;
  bool destroyed_;
  std::vector<int32_t> attribs_;
  mojo::Binding<mus::mojom::CommandBufferClient> client_binding_;
  mus::mojom::CommandBufferPtr command_buffer_;

  gpu::CommandBufferId command_buffer_id_;
  gpu::Capabilities capabilities_;
  State last_state_;
  mojo::ScopedSharedBufferMapping shared_state_;
  int32_t last_put_offset_;
  int32_t next_transfer_buffer_id_;

  // Image IDs are allocated in sequence.
  int next_image_id_;

  uint64_t next_fence_sync_release_;
  uint64_t flushed_fence_sync_release_;
};

}  // mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_COMMAND_BUFFER_CLIENT_IMPL_H_
