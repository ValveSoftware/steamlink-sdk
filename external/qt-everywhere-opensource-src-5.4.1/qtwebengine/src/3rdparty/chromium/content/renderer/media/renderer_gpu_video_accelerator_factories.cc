// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/bind.h"
#include "content/child/child_thread.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace content {

// static
scoped_refptr<RendererGpuVideoAcceleratorFactories>
RendererGpuVideoAcceleratorFactories::Create(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider) {
  scoped_refptr<RendererGpuVideoAcceleratorFactories> factories =
      new RendererGpuVideoAcceleratorFactories(
          gpu_channel_host, message_loop_proxy, context_provider);
  // Post task from outside constructor, since AddRef()/Release() is unsafe from
  // within.
  message_loop_proxy->PostTask(
      FROM_HERE,
      base::Bind(&RendererGpuVideoAcceleratorFactories::BindContext,
                 factories));
  return factories;
}

RendererGpuVideoAcceleratorFactories::RendererGpuVideoAcceleratorFactories(
    GpuChannelHost* gpu_channel_host,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider)
    : task_runner_(message_loop_proxy),
      gpu_channel_host_(gpu_channel_host),
      context_provider_(context_provider),
      thread_safe_sender_(ChildThread::current()->thread_safe_sender()) {}

RendererGpuVideoAcceleratorFactories::~RendererGpuVideoAcceleratorFactories() {}

void RendererGpuVideoAcceleratorFactories::BindContext() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!context_provider_->BindToCurrentThread())
    context_provider_ = NULL;
}

WebGraphicsContext3DCommandBufferImpl*
RendererGpuVideoAcceleratorFactories::GetContext3d() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!context_provider_)
    return NULL;
  if (context_provider_->IsContextLost()) {
    context_provider_->VerifyContexts();
    context_provider_ = NULL;
    return NULL;
  }
  return context_provider_->WebContext3D();
}

scoped_ptr<media::VideoDecodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (context && context->GetCommandBufferProxy()) {
    return gpu_channel_host_->CreateVideoDecoder(
        context->GetCommandBufferProxy()->GetRouteID());
  }

  return scoped_ptr<media::VideoDecodeAccelerator>();
}

scoped_ptr<media::VideoEncodeAccelerator>
RendererGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (context && context->GetCommandBufferProxy()) {
    return gpu_channel_host_->CreateVideoEncoder(
        context->GetCommandBufferProxy()->GetRouteID());
  }

  return scoped_ptr<media::VideoEncodeAccelerator>();
}

bool RendererGpuVideoAcceleratorFactories::CreateTextures(
    int32 count,
    const gfx::Size& size,
    std::vector<uint32>* texture_ids,
    std::vector<gpu::Mailbox>* texture_mailboxes,
    uint32 texture_target) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(texture_target);

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return false;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  texture_ids->resize(count);
  texture_mailboxes->resize(count);
  gles2->GenTextures(count, &texture_ids->at(0));
  for (int i = 0; i < count; ++i) {
    gles2->ActiveTexture(GL_TEXTURE0);
    uint32 texture_id = texture_ids->at(i);
    gles2->BindTexture(texture_target, texture_id);
    gles2->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (texture_target == GL_TEXTURE_2D) {
      gles2->TexImage2D(texture_target,
                        0,
                        GL_RGBA,
                        size.width(),
                        size.height(),
                        0,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        NULL);
    }
    gles2->GenMailboxCHROMIUM(texture_mailboxes->at(i).name);
    gles2->ProduceTextureCHROMIUM(texture_target,
                                  texture_mailboxes->at(i).name);
  }

  // We need ShallowFlushCHROMIUM() here to order the command buffer commands
  // with respect to IPC to the GPU process, to guarantee that the decoder in
  // the GPU process can use these textures as soon as it receives IPC
  // notification of them.
  gles2->ShallowFlushCHROMIUM();
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
  return true;
}

void RendererGpuVideoAcceleratorFactories::DeleteTexture(uint32 texture_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  gles2->DeleteTextures(1, &texture_id);
  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

void RendererGpuVideoAcceleratorFactories::WaitSyncPoint(uint32 sync_point) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();
  gles2->WaitSyncPointCHROMIUM(sync_point);

  // Callers expect the WaitSyncPoint to affect the next IPCs. Make sure to
  // flush the command buffers to ensure that.
  gles2->ShallowFlushCHROMIUM();
}

void RendererGpuVideoAcceleratorFactories::ReadPixels(
    uint32 texture_id,
    const gfx::Rect& visible_rect,
    const SkBitmap& pixels) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  WebGraphicsContext3DCommandBufferImpl* context = GetContext3d();
  if (!context)
    return;

  gpu::gles2::GLES2Implementation* gles2 = context->GetImplementation();

  GLuint tmp_texture;
  gles2->GenTextures(1, &tmp_texture);
  gles2->BindTexture(GL_TEXTURE_2D, tmp_texture);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  context->copyTextureCHROMIUM(
      GL_TEXTURE_2D, texture_id, tmp_texture, 0, GL_RGBA, GL_UNSIGNED_BYTE);

  GLuint fb;
  gles2->GenFramebuffers(1, &fb);
  gles2->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gles2->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tmp_texture, 0);
  gles2->PixelStorei(GL_PACK_ALIGNMENT, 4);

#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
  GLenum skia_format = GL_BGRA_EXT;
  GLenum read_format = GL_BGRA_EXT;
  GLint supported_format = 0;
  GLint supported_type = 0;
  gles2->GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &supported_format);
  gles2->GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &supported_type);
  if (supported_format != GL_BGRA_EXT || supported_type != GL_UNSIGNED_BYTE) {
    read_format = GL_RGBA;
  }
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
  GLenum skia_format = GL_RGBA;
  GLenum read_format = GL_RGBA;
#else
#error Unexpected Skia ARGB_8888 layout!
#endif
  gles2->ReadPixels(visible_rect.x(),
                    visible_rect.y(),
                    visible_rect.width(),
                    visible_rect.height(),
                    read_format,
                    GL_UNSIGNED_BYTE,
                    pixels.pixelRef()->pixels());
  gles2->DeleteFramebuffers(1, &fb);
  gles2->DeleteTextures(1, &tmp_texture);

  if (skia_format != read_format) {
    DCHECK(read_format == GL_RGBA);
    int pixel_count = visible_rect.width() * visible_rect.height();
    uint32_t* pixels_ptr = static_cast<uint32_t*>(pixels.pixelRef()->pixels());
    for (int i = 0; i < pixel_count; ++i) {
        uint32_t r = pixels_ptr[i] & 0xFF;
        uint32_t g = (pixels_ptr[i] >> 8) & 0xFF;
        uint32_t b = (pixels_ptr[i] >> 16) & 0xFF;
        uint32_t a = (pixels_ptr[i] >> 24) & 0xFF;
        pixels_ptr[i] = (r << SK_R32_SHIFT) |
                        (g << SK_G32_SHIFT) |
                        (b << SK_B32_SHIFT) |
                        (a << SK_A32_SHIFT);
    }
  }

  DCHECK_EQ(gles2->GetError(), static_cast<GLenum>(GL_NO_ERROR));
}

base::SharedMemory* RendererGpuVideoAcceleratorFactories::CreateSharedMemory(
    size_t size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return ChildThread::AllocateSharedMemory(size, thread_safe_sender_.get());
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererGpuVideoAcceleratorFactories::GetTaskRunner() {
  return task_runner_;
}

}  // namespace content
