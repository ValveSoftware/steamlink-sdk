// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/output/context_provider.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ui/gfx/geometry/size.h"

namespace content {

StreamTextureProxy::StreamTextureProxy(std::unique_ptr<StreamTextureHost> host)
    : host_(std::move(host)) {}

StreamTextureProxy::~StreamTextureProxy() {}

void StreamTextureProxy::Release() {
  {
    // Cannot call |received_frame_cb_| after returning from here.
    base::AutoLock lock(lock_);
    received_frame_cb_.Reset();
  }
  // Release is analogous to the destructor, so there should be no more external
  // calls to this object in Release. Therefore there is no need to acquire the
  // lock to access |task_runner_|.
  if (!task_runner_.get() || task_runner_->BelongsToCurrentThread() ||
      !task_runner_->DeleteSoon(FROM_HERE, this)) {
    delete this;
  }
}

void StreamTextureProxy::BindToTaskRunner(
    const base::Closure& received_frame_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner.get());

  {
    base::AutoLock lock(lock_);
    DCHECK(!task_runner_.get() || (task_runner.get() == task_runner_.get()));
    task_runner_ = task_runner;
    received_frame_cb_ = received_frame_cb;
  }

  if (task_runner->BelongsToCurrentThread()) {
    BindOnThread();
    return;
  }
  // Unretained is safe here only because the object is deleted on |loop_|
  // thread.
  task_runner->PostTask(FROM_HERE, base::Bind(&StreamTextureProxy::BindOnThread,
                                              base::Unretained(this)));
}

void StreamTextureProxy::BindOnThread() {
  host_->BindToCurrentThread(this);
}

void StreamTextureProxy::OnFrameAvailable() {
  base::AutoLock lock(lock_);
  if (!received_frame_cb_.is_null())
    received_frame_cb_.Run();
}

void StreamTextureProxy::EstablishPeer(int player_id, int frame_id) {
  host_->EstablishPeer(player_id, frame_id);
}

void StreamTextureProxy::SetStreamTextureSize(const gfx::Size& size) {
  host_->SetStreamTextureSize(size);
}

void StreamTextureProxy::ForwardStreamTextureForSurfaceRequest(
    const base::UnguessableToken& request_token) {
  host_->ForwardStreamTextureForSurfaceRequest(request_token);
}

// static
scoped_refptr<StreamTextureFactory> StreamTextureFactory::Create(
    scoped_refptr<ContextProviderCommandBuffer> context_provider) {
  return new StreamTextureFactory(std::move(context_provider));
}

StreamTextureFactory::StreamTextureFactory(
    scoped_refptr<ContextProviderCommandBuffer> context_provider)
    : context_provider_(std::move(context_provider)),
      channel_(context_provider_->GetCommandBufferProxy()->channel()) {
  DCHECK(channel_);
}

StreamTextureFactory::~StreamTextureFactory() {}

ScopedStreamTextureProxy StreamTextureFactory::CreateProxy(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox) {
  int32_t route_id =
      CreateStreamTexture(texture_target, texture_id, texture_mailbox);
  if (!route_id)
    return ScopedStreamTextureProxy();
  return ScopedStreamTextureProxy(new StreamTextureProxy(
      base::MakeUnique<StreamTextureHost>(channel_, route_id)));
}

unsigned StreamTextureFactory::CreateStreamTexture(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox) {
  GLuint route_id = 0;
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, texture_id);
  gl->ShallowFlushCHROMIUM();
  route_id = context_provider_->GetCommandBufferProxy()->CreateStreamTexture(
      *texture_id);
  if (!route_id) {
    gl->DeleteTextures(1, texture_id);
    // Flush to ensure that the stream texture gets deleted in a timely fashion.
    gl->ShallowFlushCHROMIUM();
    *texture_id = 0;
    *texture_mailbox = gpu::Mailbox();
  } else {
    gl->GenMailboxCHROMIUM(texture_mailbox->name);
    gl->ProduceTextureDirectCHROMIUM(*texture_id, texture_target,
                                     texture_mailbox->name);
  }
  return route_id;
}

gpu::gles2::GLES2Interface* StreamTextureFactory::ContextGL() {
  return context_provider_->ContextGL();
}

}  // namespace content
