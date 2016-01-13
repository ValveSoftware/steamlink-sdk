// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/texture_image_transport_surface.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "ui/gl/scoped_binders.h"

using gpu::gles2::ContextGroup;
using gpu::gles2::GLES2Decoder;
using gpu::gles2::MailboxManager;
using gpu::gles2::Texture;
using gpu::gles2::TextureManager;
using gpu::gles2::TextureRef;
using gpu::Mailbox;

namespace content {
namespace {

bool IsContextValid(ImageTransportHelper* helper) {
  return helper->stub()->decoder()->GetGLContext()->IsCurrent(NULL);
}

}  // namespace

TextureImageTransportSurface::TextureImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle)
      : fbo_id_(0),
        current_size_(1, 1),
        scale_factor_(1.f),
        stub_destroyed_(false),
        backbuffer_suggested_allocation_(true),
        frontbuffer_suggested_allocation_(true),
        handle_(handle),
        is_swap_buffers_pending_(false),
        did_unschedule_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

TextureImageTransportSurface::~TextureImageTransportSurface() {
  DCHECK(stub_destroyed_);
  Destroy();
}

bool TextureImageTransportSurface::Initialize() {
  mailbox_manager_ =
      helper_->stub()->decoder()->GetContextGroup()->mailbox_manager();

  GpuChannelManager* manager = helper_->manager();
  surface_ = manager->GetDefaultOffscreenSurface();
  if (!surface_.get())
    return false;

  if (!helper_->Initialize())
    return false;

  GpuChannel* parent_channel = manager->LookupChannel(handle_.parent_client_id);
  if (parent_channel) {
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess))
      helper_->SetPreemptByFlag(parent_channel->GetPreemptionFlag());
  }

  return true;
}

void TextureImageTransportSurface::Destroy() {
  if (surface_.get())
    surface_ = NULL;

  helper_->Destroy();
}

bool TextureImageTransportSurface::DeferDraws() {
  // The command buffer hit a draw/clear command that could clobber the
  // texture in use by the UI compositor. If a Swap is pending, abort
  // processing of the command by returning true and unschedule until the Swap
  // Ack arrives.
  DCHECK(!did_unschedule_);
  if (is_swap_buffers_pending_) {
    did_unschedule_ = true;
    helper_->SetScheduled(false);
    return true;
  }
  return false;
}

bool TextureImageTransportSurface::IsOffscreen() {
  return true;
}

unsigned int TextureImageTransportSurface::GetBackingFrameBufferObject() {
  DCHECK(IsContextValid(helper_.get()));
  if (!fbo_id_) {
    glGenFramebuffersEXT(1, &fbo_id_);
    glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_id_);
    helper_->stub()->AddDestructionObserver(this);
    CreateBackTexture();
  }

  return fbo_id_;
}

bool TextureImageTransportSurface::SetBackbufferAllocation(bool allocation) {
  DCHECK(!is_swap_buffers_pending_);
  if (backbuffer_suggested_allocation_ == allocation)
     return true;
  backbuffer_suggested_allocation_ = allocation;

  if (backbuffer_suggested_allocation_) {
    DCHECK(!backbuffer_.get());
    CreateBackTexture();
  } else {
    ReleaseBackTexture();
  }

  return true;
}

void TextureImageTransportSurface::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;

  // If a swapbuffers is in flight, wait for the ack before releasing the front
  // buffer:
  // - we don't know yet which texture the browser will want to keep
  // - we want to ensure we don't destroy a texture that is in flight before the
  // browser got a reference on it.
  if (!frontbuffer_suggested_allocation_ &&
      !is_swap_buffers_pending_ &&
      helper_->MakeCurrent()) {
    ReleaseFrontTexture();
  }
}

void* TextureImageTransportSurface::GetShareHandle() {
  return GetHandle();
}

void* TextureImageTransportSurface::GetDisplay() {
  return surface_.get() ? surface_->GetDisplay() : NULL;
}

void* TextureImageTransportSurface::GetConfig() {
  return surface_.get() ? surface_->GetConfig() : NULL;
}

void TextureImageTransportSurface::OnResize(gfx::Size size,
                                            float scale_factor) {
  DCHECK_GE(size.width(), 1);
  DCHECK_GE(size.height(), 1);
  current_size_ = size;
  scale_factor_ = scale_factor;
  if (backbuffer_suggested_allocation_)
    CreateBackTexture();
}

void TextureImageTransportSurface::OnWillDestroyStub() {
  bool have_context = IsContextValid(helper_.get());
  helper_->stub()->RemoveDestructionObserver(this);

  // We are losing the stub owning us, this is our last chance to clean up the
  // resources we allocated in the stub's context.
  if (have_context) {
    ReleaseBackTexture();
    ReleaseFrontTexture();
  } else {
    backbuffer_ = NULL;
    back_mailbox_ = Mailbox();
    frontbuffer_ = NULL;
    front_mailbox_ = Mailbox();
  }

  if (fbo_id_ && have_context) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    CHECK_GL_ERROR();
  }
  fbo_id_ = 0;

  stub_destroyed_ = true;
}

void TextureImageTransportSurface::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (size_t i = 0; i < latency_info.size(); i++)
    latency_info_.push_back(latency_info[i]);
}

void TextureImageTransportSurface::WakeUpGpu() {
  NOTIMPLEMENTED();
}

bool TextureImageTransportSurface::SwapBuffers() {
  DCHECK(IsContextValid(helper_.get()));
  DCHECK(backbuffer_suggested_allocation_);

  if (!frontbuffer_suggested_allocation_)
    return true;

  if (!backbuffer_.get()) {
    LOG(ERROR) << "Swap without valid backing.";
    return true;
  }

  DCHECK(backbuffer_size() == current_size_);
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.size = backbuffer_size();
  params.scale_factor = scale_factor_;
  params.mailbox = back_mailbox_;

  glFlush();

  params.latency_info.swap(latency_info_);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

bool TextureImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(IsContextValid(helper_.get()));
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  const gfx::Rect new_damage_rect(x, y, width, height);
  DCHECK(gfx::Rect(gfx::Point(), current_size_).Contains(new_damage_rect));

  // An empty damage rect is a successful no-op.
  if (new_damage_rect.IsEmpty())
    return true;

  if (!backbuffer_.get()) {
    LOG(ERROR) << "Swap without valid backing.";
    return true;
  }

  DCHECK(current_size_ == backbuffer_size());
  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_size = backbuffer_size();
  params.surface_scale_factor = scale_factor_;
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;
  params.mailbox = back_mailbox_;

  glFlush();

  params.latency_info.swap(latency_info_);
  helper_->SendAcceleratedSurfacePostSubBuffer(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

bool TextureImageTransportSurface::SupportsPostSubBuffer() {
  return true;
}

gfx::Size TextureImageTransportSurface::GetSize() {
  return current_size_;
}

void* TextureImageTransportSurface::GetHandle() {
  return surface_.get() ? surface_->GetHandle() : NULL;
}

unsigned TextureImageTransportSurface::GetFormat() {
  return surface_.get() ? surface_->GetFormat() : 0;
}

void TextureImageTransportSurface::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  if (params.sync_point == 0) {
    BufferPresentedImpl(params.mailbox);
  } else {
    helper_->manager()->sync_point_manager()->AddSyncPointCallback(
        params.sync_point,
        base::Bind(&TextureImageTransportSurface::BufferPresentedImpl,
                   this,
                   params.mailbox));
  }
}

void TextureImageTransportSurface::BufferPresentedImpl(const Mailbox& mailbox) {
  DCHECK(is_swap_buffers_pending_);
  is_swap_buffers_pending_ = false;

  // When we wait for a sync point, we may get called back after the stub is
  // destroyed. In that case there's no need to do anything with the returned
  // mailbox.
  if (stub_destroyed_)
    return;

  // We should not have allowed the backbuffer to be discarded while the ack
  // was pending.
  DCHECK(backbuffer_suggested_allocation_);
  DCHECK(backbuffer_.get());

  bool swap = true;
  if (!mailbox.IsZero()) {
    if (mailbox == back_mailbox_) {
      // The browser has skipped the frame to unblock the GPU process, waiting
      // for one of the right size, and returned the back buffer, so don't swap.
      swap = false;
    }
  }
  if (swap) {
    std::swap(backbuffer_, frontbuffer_);
    std::swap(back_mailbox_, front_mailbox_);
  }

  // We're relying on the fact that the parent context is
  // finished with its context when it inserts the sync point that
  // triggers this callback.
  if (helper_->MakeCurrent()) {
    if (frontbuffer_.get() && !frontbuffer_suggested_allocation_)
      ReleaseFrontTexture();
    if (!backbuffer_.get() || backbuffer_size() != current_size_)
      CreateBackTexture();
    else
      AttachBackTextureToFBO();
  }

  // Even if MakeCurrent fails, schedule anyway, to trigger the lost context
  // logic.
  if (did_unschedule_) {
    did_unschedule_ = false;
    helper_->SetScheduled(true);
  }
}

void TextureImageTransportSurface::ReleaseBackTexture() {
  DCHECK(IsContextValid(helper_.get()));
  backbuffer_ = NULL;
  back_mailbox_ = Mailbox();
  glFlush();
  CHECK_GL_ERROR();
}

void TextureImageTransportSurface::ReleaseFrontTexture() {
  DCHECK(IsContextValid(helper_.get()));
  frontbuffer_ = NULL;
  front_mailbox_ = Mailbox();
  glFlush();
  CHECK_GL_ERROR();
  helper_->SendAcceleratedSurfaceRelease();
}

void TextureImageTransportSurface::CreateBackTexture() {
  DCHECK(IsContextValid(helper_.get()));
  // If |is_swap_buffers_pending| we are waiting for our backbuffer
  // in the mailbox, so we shouldn't be reallocating it now.
  DCHECK(!is_swap_buffers_pending_);

  if (backbuffer_.get() && backbuffer_size() == current_size_)
    return;

  VLOG(1) << "Allocating new backbuffer texture";

  GLES2Decoder* decoder = helper_->stub()->decoder();
  TextureManager* texture_manager =
      decoder->GetContextGroup()->texture_manager();
  if (!backbuffer_.get()) {
    back_mailbox_ = gpu::Mailbox::Generate();
    GLuint service_id;
    glGenTextures(1, &service_id);
    backbuffer_ = TextureRef::Create(texture_manager, 0, service_id);
    texture_manager->SetTarget(backbuffer_.get(), GL_TEXTURE_2D);
    Texture* texture = texture_manager->Produce(backbuffer_.get());
    mailbox_manager_->ProduceTexture(GL_TEXTURE_2D, back_mailbox_, texture);
  }

  {
    gfx::ScopedTextureBinder texture_binder(GL_TEXTURE_2D,
                                            backbuffer_->service_id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        current_size_.width(), current_size_.height(), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gpu::gles2::ErrorState* error_state = decoder->GetErrorState();
    texture_manager->SetParameteri("Backbuffer",
                                   error_state,
                                   backbuffer_.get(),
                                   GL_TEXTURE_MIN_FILTER,
                                   GL_LINEAR);
    texture_manager->SetParameteri("Backbuffer",
                                   error_state,
                                   backbuffer_.get(),
                                   GL_TEXTURE_MAG_FILTER,
                                   GL_LINEAR);
    texture_manager->SetParameteri("Backbuffer",
                                   error_state,
                                   backbuffer_.get(),
                                   GL_TEXTURE_WRAP_S,
                                   GL_CLAMP_TO_EDGE);
    texture_manager->SetParameteri("Backbuffer",
                                   error_state,
                                   backbuffer_.get(),
                                   GL_TEXTURE_WRAP_T,
                                   GL_CLAMP_TO_EDGE);
    texture_manager->SetLevelInfo(backbuffer_.get(),
                                  GL_TEXTURE_2D,
                                  0,
                                  GL_RGBA,
                                  current_size_.width(),
                                  current_size_.height(),
                                  1,
                                  0,
                                  GL_RGBA,
                                  GL_UNSIGNED_BYTE,
                                  true);
    DCHECK(texture_manager->CanRender(backbuffer_.get()));
    CHECK_GL_ERROR();
  }

  AttachBackTextureToFBO();
}

void TextureImageTransportSurface::AttachBackTextureToFBO() {
  DCHECK(IsContextValid(helper_.get()));
  DCHECK(backbuffer_.get());
  gfx::ScopedFrameBufferBinder fbo_binder(fbo_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D,
      backbuffer_->service_id(),
      0);
  CHECK_GL_ERROR();

#ifndef NDEBUG
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(FATAL) << "Framebuffer incomplete: " << status;
  }
#endif
}

}  // namespace content
