// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/gpu_service_impl.h"

#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/gpu/gpu_service_mus.h"
#include "services/shell/public/cpp/connection.h"

namespace mus {

namespace {

void EstablishGpuChannelDone(
    const mojom::GpuService::EstablishGpuChannelCallback& callback,
    int32_t client_id,
    const IPC::ChannelHandle& channel_handle) {
  // TODO(penghuang): Send the real GPUInfo to the client.
  callback.Run(client_id, mojom::ChannelHandle::From(channel_handle),
               mojom::GpuInfo::From<gpu::GPUInfo>(gpu::GPUInfo()));
}
}

GpuServiceImpl::GpuServiceImpl(
    mojo::InterfaceRequest<mojom::GpuService> request,
    shell::Connection* connection)
    : binding_(this, std::move(request)) {}

GpuServiceImpl::~GpuServiceImpl() {}

void GpuServiceImpl::EstablishGpuChannel(
    const mojom::GpuService::EstablishGpuChannelCallback& callback) {
  GpuServiceMus* service = GpuServiceMus::GetInstance();
  // TODO(penghuang): crbug.com/617415 figure out how to generate a meaningful
  // tracing id.
  const uint64_t client_tracing_id = 0;
  // TODO(penghuang): windows server may want to control those flags.
  // Add a private interface for windows server.
  const bool preempts = false;
  const bool allow_view_command_buffers = false;
  const bool allow_real_time_streams = false;
  service->EstablishGpuChannel(
      client_tracing_id, preempts, allow_view_command_buffers,
      allow_real_time_streams, base::Bind(&EstablishGpuChannelDone, callback));
}

void GpuServiceImpl::CreateGpuMemoryBuffer(
    mojom::GpuMemoryBufferIdPtr id,
    const gfx::Size& size,
    mojom::BufferFormat format,
    mojom::BufferUsage usage,
    uint64_t surface_id,
    const mojom::GpuService::CreateGpuMemoryBufferCallback& callback) {
  NOTIMPLEMENTED();
}

void GpuServiceImpl::DestroyGpuMemoryBuffer(mojom::GpuMemoryBufferIdPtr id,
                                            const gpu::SyncToken& sync_token) {
  NOTIMPLEMENTED();
}

}  // namespace mus
