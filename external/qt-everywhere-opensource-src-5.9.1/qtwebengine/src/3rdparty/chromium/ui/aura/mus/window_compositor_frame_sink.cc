// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_compositor_frame_sink.h"

#include "base/bind.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace aura {

// static
std::unique_ptr<WindowCompositorFrameSink> WindowCompositorFrameSink::Create(
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    std::unique_ptr<WindowCompositorFrameSinkBinding>*
        compositor_frame_sink_binding) {
  cc::mojom::MojoCompositorFrameSinkPtr compositor_frame_sink;
  cc::mojom::MojoCompositorFrameSinkClientPtr compositor_frame_sink_client;
  cc::mojom::MojoCompositorFrameSinkClientRequest
      compositor_frame_sink_client_request =
          GetProxy(&compositor_frame_sink_client);

  compositor_frame_sink_binding->reset(new WindowCompositorFrameSinkBinding(
      GetProxy(&compositor_frame_sink),
      compositor_frame_sink_client.PassInterface()));
  return base::WrapUnique(new WindowCompositorFrameSink(
      std::move(context_provider), gpu_memory_buffer_manager,
      compositor_frame_sink.PassInterface(),
      std::move(compositor_frame_sink_client_request)));
}

WindowCompositorFrameSink::~WindowCompositorFrameSink() {}

bool WindowCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* client) {
  if (!cc::CompositorFrameSink::BindToClient(client))
    return false;

  DCHECK(!thread_checker_);
  thread_checker_.reset(new base::ThreadChecker());
  compositor_frame_sink_.Bind(std::move(compositor_frame_sink_info_));
  client_binding_.reset(
      new mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient>(
          this, std::move(client_request_)));

  begin_frame_source_ = base::MakeUnique<cc::ExternalBeginFrameSource>(this);

  client->SetBeginFrameSource(begin_frame_source_.get());
  return true;
}

void WindowCompositorFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  compositor_frame_sink_.reset();
  cc::CompositorFrameSink::DetachFromClient();
}

void WindowCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!compositor_frame_sink_)
    return;
  compositor_frame_sink_->SubmitCompositorFrame(std::move(frame));
}

WindowCompositorFrameSink::WindowCompositorFrameSink(
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSink>
        compositor_frame_sink_info,
    cc::mojom::MojoCompositorFrameSinkClientRequest client_request)
    : cc::CompositorFrameSink(std::move(context_provider),
                              nullptr,
                              gpu_memory_buffer_manager,
                              nullptr),
      compositor_frame_sink_info_(std::move(compositor_frame_sink_info)),
      client_request_(std::move(client_request)) {}

void WindowCompositorFrameSink::DidReceiveCompositorFrameAck() {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->DidReceiveCompositorFrameAck();
}

void WindowCompositorFrameSink::OnBeginFrame(
    const cc::BeginFrameArgs& begin_frame_args) {
  begin_frame_source_->OnBeginFrame(begin_frame_args);
}

void WindowCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->ReclaimResources(resources);
}

void WindowCompositorFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  compositor_frame_sink_->SetNeedsBeginFrame(needs_begin_frames);
}

WindowCompositorFrameSinkBinding::~WindowCompositorFrameSinkBinding() {}

WindowCompositorFrameSinkBinding::WindowCompositorFrameSinkBinding(
    cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink_request,
    mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSinkClient>
        compositor_frame_sink_client)
    : compositor_frame_sink_request_(std::move(compositor_frame_sink_request)),
      compositor_frame_sink_client_(std::move(compositor_frame_sink_client)) {}

}  // namespace aura
