// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_fbo_mac.h"

#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/image_transport_surface_iosurface_mac.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_osmesa.h"

namespace content {

ImageTransportSurfaceFBO::ImageTransportSurfaceFBO(
    StorageProvider* storage_provider,
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle)
    : storage_provider_(storage_provider),
      backbuffer_suggested_allocation_(true),
      frontbuffer_suggested_allocation_(true),
      fbo_id_(0),
      texture_id_(0),
      depth_stencil_renderbuffer_id_(0),
      has_complete_framebuffer_(false),
      context_(NULL),
      scale_factor_(1.f),
      made_current_(false),
      is_swap_buffers_pending_(false),
      did_unschedule_(false) {
  helper_.reset(new ImageTransportHelper(this, manager, stub, handle));
}

ImageTransportSurfaceFBO::~ImageTransportSurfaceFBO() {
}

bool ImageTransportSurfaceFBO::Initialize() {
  // Only support IOSurfaces if the GL implementation is the native desktop GL.
  // IO surfaces will not work with, for example, OSMesa software renderer
  // GL contexts.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL &&
      gfx::GetGLImplementation() != gfx::kGLImplementationAppleGL)
    return false;

  if (!helper_->Initialize())
    return false;

  helper_->stub()->AddDestructionObserver(this);
  return true;
}

void ImageTransportSurfaceFBO::Destroy() {
  DestroyFramebuffer();

  helper_->Destroy();
}

bool ImageTransportSurfaceFBO::DeferDraws() {
  // The command buffer hit a draw/clear command that could clobber the
  // IOSurface in use by an earlier SwapBuffers. If a Swap is pending, abort
  // processing of the command by returning true and unschedule until the Swap
  // Ack arrives.
  if(did_unschedule_)
    return true;  // Still unscheduled, so just return true.
  if (is_swap_buffers_pending_) {
    did_unschedule_ = true;
    helper_->SetScheduled(false);
    return true;
  }
  return false;
}

bool ImageTransportSurfaceFBO::IsOffscreen() {
  return false;
}

bool ImageTransportSurfaceFBO::OnMakeCurrent(gfx::GLContext* context) {
  context_ = context;

  if (made_current_)
    return true;

  OnResize(gfx::Size(1, 1), 1.f);

  made_current_ = true;
  return true;
}

unsigned int ImageTransportSurfaceFBO::GetBackingFrameBufferObject() {
  return fbo_id_;
}

bool ImageTransportSurfaceFBO::SetBackbufferAllocation(bool allocation) {
  if (backbuffer_suggested_allocation_ == allocation)
    return true;
  backbuffer_suggested_allocation_ = allocation;
  AdjustBufferAllocation();
  return true;
}

void ImageTransportSurfaceFBO::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;
  AdjustBufferAllocation();
}

void ImageTransportSurfaceFBO::AdjustBufferAllocation() {
  // On mac, the frontbuffer and backbuffer are the same buffer. The buffer is
  // free'd when both the browser and gpu processes have Unref'd the IOSurface.
  if (!backbuffer_suggested_allocation_ &&
      !frontbuffer_suggested_allocation_ &&
      has_complete_framebuffer_) {
    DestroyFramebuffer();
    helper_->Suspend();
  } else if (backbuffer_suggested_allocation_ && !has_complete_framebuffer_) {
    CreateFramebuffer();
  }
}

bool ImageTransportSurfaceFBO::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = storage_provider_->GetSurfaceHandle();
  params.size = GetSize();
  params.scale_factor = scale_factor_;
  params.latency_info.swap(latency_info_);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

bool ImageTransportSurfaceFBO::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  glFlush();

  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_handle = storage_provider_->GetSurfaceHandle();
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;
  params.surface_size = GetSize();
  params.surface_scale_factor = scale_factor_;
  params.latency_info.swap(latency_info_);
  helper_->SendAcceleratedSurfacePostSubBuffer(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

bool ImageTransportSurfaceFBO::SupportsPostSubBuffer() {
  return true;
}

gfx::Size ImageTransportSurfaceFBO::GetSize() {
  return size_;
}

void* ImageTransportSurfaceFBO::GetHandle() {
  return NULL;
}

void* ImageTransportSurfaceFBO::GetDisplay() {
  return NULL;
}

void ImageTransportSurfaceFBO::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  DCHECK(is_swap_buffers_pending_);

  context_->share_group()->SetRendererID(params.renderer_id);
  is_swap_buffers_pending_ = false;
  if (did_unschedule_) {
    did_unschedule_ = false;
    helper_->SetScheduled(true);
  }
}

void ImageTransportSurfaceFBO::OnResize(gfx::Size size,
                                        float scale_factor) {
  // This trace event is used in gpu_feature_browsertest.cc - the test will need
  // to be updated if this event is changed or moved.
  TRACE_EVENT2("gpu", "ImageTransportSurfaceFBO::OnResize",
               "old_width", size_.width(), "new_width", size.width());
  // Caching |context_| from OnMakeCurrent. It should still be current.
  DCHECK(context_->IsCurrent(this));

  size_ = size;
  scale_factor_ = scale_factor;

  CreateFramebuffer();
}

void ImageTransportSurfaceFBO::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (size_t i = 0; i < latency_info.size(); i++)
    latency_info_.push_back(latency_info[i]);
}

void ImageTransportSurfaceFBO::WakeUpGpu() {
  NOTIMPLEMENTED();
}

void ImageTransportSurfaceFBO::OnWillDestroyStub() {
  helper_->stub()->RemoveDestructionObserver(this);
  Destroy();
}

void ImageTransportSurfaceFBO::DestroyFramebuffer() {
  // If we have resources to destroy, then make sure that we have a current
  // context which we can use to delete the resources.
  if (context_ || fbo_id_ || texture_id_ || depth_stencil_renderbuffer_id_) {
    DCHECK(gfx::GLContext::GetCurrent() == context_);
    DCHECK(context_->IsCurrent(this));
    DCHECK(CGLGetCurrentContext());
  }

  if (fbo_id_) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    fbo_id_ = 0;
  }

  if (texture_id_) {
    glDeleteTextures(1, &texture_id_);
    texture_id_ = 0;
  }

  if (depth_stencil_renderbuffer_id_) {
    glDeleteRenderbuffersEXT(1, &depth_stencil_renderbuffer_id_);
    depth_stencil_renderbuffer_id_ = 0;
  }

  storage_provider_->FreeColorBufferStorage();

  has_complete_framebuffer_ = false;
}

void ImageTransportSurfaceFBO::CreateFramebuffer() {
  gfx::Size new_rounded_size = storage_provider_->GetRoundedSize(size_);

  // Only recreate surface when the rounded up size has changed.
  if (has_complete_framebuffer_ && new_rounded_size == rounded_size_)
    return;

  // This trace event is used in gpu_feature_browsertest.cc - the test will need
  // to be updated if this event is changed or moved.
  TRACE_EVENT2("gpu", "ImageTransportSurfaceFBO::CreateFramebuffer",
               "width", new_rounded_size.width(),
               "height", new_rounded_size.height());

  rounded_size_ = new_rounded_size;

  // GL_TEXTURE_RECTANGLE_ARB is the best supported render target on
  // Mac OS X and is required for IOSurface interoperability.
  GLint previous_texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &previous_texture_id);

  // Free the old IO Surface first to reduce memory fragmentation.
  DestroyFramebuffer();

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);

  glGenTextures(1, &texture_id_);

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_id_);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                            GL_COLOR_ATTACHMENT0_EXT,
                            GL_TEXTURE_RECTANGLE_ARB,
                            texture_id_,
                            0);

  // Search through the provided attributes; if the caller has
  // requested a stencil buffer, try to get one.

  int32 stencil_bits =
      helper_->stub()->GetRequestedAttribute(EGL_STENCIL_SIZE);
  if (stencil_bits > 0) {
    // Create and bind the stencil buffer
    bool has_packed_depth_stencil =
         GLSurface::ExtensionsContain(
             reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                            "GL_EXT_packed_depth_stencil");

    if (has_packed_depth_stencil) {
      glGenRenderbuffersEXT(1, &depth_stencil_renderbuffer_id_);
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,
                            depth_stencil_renderbuffer_id_);
      glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT,
                              rounded_size_.width(), rounded_size_.height());
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                  GL_STENCIL_ATTACHMENT_EXT,
                                  GL_RENDERBUFFER_EXT,
                                  depth_stencil_renderbuffer_id_);
    }

    // If we asked for stencil but the extension isn't present,
    // it's OK to silently fail; subsequent code will/must check
    // for the presence of a stencil buffer before attempting to
    // do stencil-based operations.
  }

  bool allocated_color_buffer = storage_provider_->AllocateColorBufferStorage(
      static_cast<CGLContextObj>(context_->GetHandle()),
      rounded_size_);
  if (!allocated_color_buffer) {
    DLOG(ERROR) << "Failed to allocate color buffer storage.";
    DestroyFramebuffer();
    return;
  }

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
    DLOG(ERROR) << "Framebuffer was incomplete: " << status;
    DestroyFramebuffer();
    return;
  }

  has_complete_framebuffer_ = true;

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, previous_texture_id);
  // The FBO remains bound for this GL context.
}

}  // namespace content
