// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_GLES2_COMMAND_BUFFER_IMPL_H_
#define MOJO_SERVICES_GLES2_COMMAND_BUFFER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/gles2/command_buffer.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gpu {
class CommandBufferService;
class GpuScheduler;
class GpuControlService;
namespace gles2 {
class GLES2Decoder;
}
}

namespace mojo {
namespace services {

class CommandBufferImpl : public InterfaceImpl<CommandBuffer> {
 public:
  CommandBufferImpl(gfx::AcceleratedWidget widget,
                    const gfx::Size& size);
  virtual ~CommandBufferImpl();

  virtual void OnConnectionError() OVERRIDE;
  virtual void Initialize(CommandBufferSyncClientPtr sync_client,
                          mojo::ScopedSharedBufferHandle shared_state) OVERRIDE;
  virtual void SetGetBuffer(int32_t buffer) OVERRIDE;
  virtual void Flush(int32_t put_offset) OVERRIDE;
  virtual void MakeProgress(int32_t last_get_offset) OVERRIDE;
  virtual void RegisterTransferBuffer(
      int32_t id,
      mojo::ScopedSharedBufferHandle transfer_buffer,
      uint32_t size) OVERRIDE;
  virtual void DestroyTransferBuffer(int32_t id) OVERRIDE;
  virtual void Echo(const Callback<void()>& callback) OVERRIDE;

  virtual void RequestAnimationFrames() OVERRIDE;
  virtual void CancelAnimationFrames() OVERRIDE;

 private:
  bool DoInitialize(mojo::ScopedSharedBufferHandle shared_state);

  void OnParseError();

  void DrawAnimationFrame();

  CommandBufferSyncClientPtr sync_client_;

  gfx::AcceleratedWidget widget_;
  gfx::Size size_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  scoped_ptr<gpu::GpuControlService> gpu_control_;
  base::RepeatingTimer<CommandBufferImpl> timer_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferImpl);
};

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_GLES2_COMMAND_BUFFER_IMPL_H_
