// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/gpu_impl.h"

#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/gles2/command_buffer_impl.h"

namespace mus {

GpuImpl::GpuImpl(mojo::InterfaceRequest<Gpu> request,
                 const scoped_refptr<GpuState>& state)
    : binding_(this, std::move(request)), state_(state) {}

GpuImpl::~GpuImpl() {}

void GpuImpl::CreateOffscreenGLES2Context(
    mojo::InterfaceRequest<mojom::CommandBuffer> request) {
  new CommandBufferImpl(std::move(request), state_);
}

void GpuImpl::GetGpuInfo(const GetGpuInfoCallback& callback) {
  callback.Run(mojom::GpuInfo::From<gpu::GPUInfo>(state_->gpu_info()));
}

}  // namespace mus
