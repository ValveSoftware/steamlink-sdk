// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/public/interfaces/command_buffer.mojom.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {

class CommandBufferDriver;
class GpuState;

// This class listens to the CommandBuffer message pipe on a low-latency thread
// so that we can insert sync points without blocking on the GL driver. It
// forwards most method calls to the CommandBufferDriver, which runs on the
// same thread as the native viewport.
class CommandBufferImpl : public mojom::CommandBuffer,
                          public CommandBufferDriver::Client {
 public:
  CommandBufferImpl(mojo::InterfaceRequest<CommandBuffer> request,
                    scoped_refptr<GpuState> gpu_state);

 private:
  class CommandBufferDriverClientImpl;
  ~CommandBufferImpl() override;

  // CommandBufferDriver::Client. All called on the GPU thread.
  void DidLoseContext(uint32_t reason) override;
  void UpdateVSyncParameters(const base::TimeTicks& timebase,
                             const base::TimeDelta& interval) override;
  void OnGpuCompletedSwapBuffers(gfx::SwapResult result) override;

  // mojom::CommandBuffer:
  void Initialize(
      mojom::CommandBufferClientPtr client,
      mojo::ScopedSharedBufferHandle shared_state,
      mojo::Array<int32_t> attribs,
      const mojom::CommandBuffer::InitializeCallback& callback) override;
  void SetGetBuffer(int32_t buffer) override;
  void Flush(int32_t put_offset) override;
  void MakeProgress(
      int32_t last_get_offset,
      const mojom::CommandBuffer::MakeProgressCallback& callback) override;
  void RegisterTransferBuffer(int32_t id,
                              mojo::ScopedSharedBufferHandle transfer_buffer,
                              uint32_t size) override;
  void DestroyTransferBuffer(int32_t id) override;
  void CreateImage(int32_t id,
                   mojo::ScopedHandle memory_handle,
                   int32_t type,
                   const gfx::Size& size,
                   int32_t format,
                   int32_t internal_format) override;
  void DestroyImage(int32_t id) override;
  void CreateStreamTexture(
      uint32_t client_texture_id,
      const mojom::CommandBuffer::CreateStreamTextureCallback& callback
      ) override;
  void TakeFrontBuffer(const gpu::Mailbox& mailbox) override;
  void ReturnFrontBuffer(const gpu::Mailbox& mailbox, bool is_lost) override;
  void SignalQuery(uint32_t query, uint32_t signal_id) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       uint32_t signal_id) override;
  void WaitForGetOffsetInRange(
      int32_t start, int32_t end,
      const mojom::CommandBuffer::WaitForGetOffsetInRangeCallback& callback
      ) override;
  void WaitForTokenInRange(
      int32_t start, int32_t end,
      const mojom::CommandBuffer::WaitForGetOffsetInRangeCallback& callback
      ) override;

  // All helper functions are called in the GPU therad.
  void InitializeOnGpuThread(
      mojom::CommandBufferClientPtr client,
      mojo::ScopedSharedBufferHandle shared_state,
      mojo::Array<int32_t> attribs,
      const base::Callback<
          void(mojom::CommandBufferInitializeResultPtr)>& callback);
  bool SetGetBufferOnGpuThread(int32_t buffer);
  bool FlushOnGpuThread(int32_t put_offset, uint32_t order_num);
  bool MakeProgressOnGpuThread(
      int32_t last_get_offset,
      const base::Callback<void(const gpu::CommandBuffer::State&)>& callback);
  bool RegisterTransferBufferOnGpuThread(
      int32_t id,
      mojo::ScopedSharedBufferHandle transfer_buffer,
      uint32_t size);
  bool DestroyTransferBufferOnGpuThread(int32_t id);
  bool CreateImageOnGpuThread(int32_t id,
                              mojo::ScopedHandle memory_handle,
                              int32_t type,
                              const gfx::Size& size,
                              int32_t format,
                              int32_t internal_format);
  bool DestroyImageOnGpuThread(int32_t id);

  void BindToRequest(mojo::InterfaceRequest<CommandBuffer> request);

  void OnConnectionError();
  bool DeleteOnGpuThread();
  void DeleteOnGpuThread2();

  scoped_refptr<GpuState> gpu_state_;
  std::unique_ptr<CommandBufferDriver> driver_;
  std::unique_ptr<mojo::Binding<CommandBuffer>> binding_;
  mojom::CommandBufferClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_IMPL_H_
