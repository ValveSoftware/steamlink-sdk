// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame_sink.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace cc {

CompositorFrameSink::CompositorFrameSink(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    SharedBitmapManager* shared_bitmap_manager)
    : context_provider_(std::move(context_provider)),
      worker_context_provider_(std::move(worker_context_provider)),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      shared_bitmap_manager_(shared_bitmap_manager) {
  client_thread_checker_.DetachFromThread();
}

CompositorFrameSink::CompositorFrameSink(
    scoped_refptr<VulkanContextProvider> vulkan_context_provider)
    : vulkan_context_provider_(vulkan_context_provider),
      gpu_memory_buffer_manager_(nullptr),
      shared_bitmap_manager_(nullptr) {
  client_thread_checker_.DetachFromThread();
}

CompositorFrameSink::~CompositorFrameSink() {
  if (client_)
    DetachFromClient();
}

bool CompositorFrameSink::BindToClient(CompositorFrameSinkClient* client) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
  bool success = true;

  if (context_provider_.get()) {
    success = context_provider_->BindToCurrentThread();
    if (success) {
      context_provider_->SetLostContextCallback(
          base::Bind(&CompositorFrameSink::DidLoseCompositorFrameSink,
                     base::Unretained(this)));
    }
  }

  if (!success) {
    // Destroy the ContextProvider on the thread attempted to be bound.
    context_provider_ = nullptr;
    client_ = nullptr;
  }

  return success;
}

void CompositorFrameSink::DetachFromClient() {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  DCHECK(client_);

  if (context_provider_.get()) {
    context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
  }
  // Destroy the ContextProvider on the bound thread.
  context_provider_ = nullptr;
  client_ = nullptr;
}

void CompositorFrameSink::DidLoseCompositorFrameSink() {
  TRACE_EVENT0("cc", "CompositorFrameSink::DidLoseCompositorFrameSink");
  client_->DidLoseCompositorFrameSink();
}

}  // namespace cc
