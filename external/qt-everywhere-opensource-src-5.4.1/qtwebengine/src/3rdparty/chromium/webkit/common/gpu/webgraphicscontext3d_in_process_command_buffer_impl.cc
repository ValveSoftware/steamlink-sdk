// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <string>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_implementation.h"

using gpu::gles2::GLES2Implementation;
using gpu::GLInProcessContext;

namespace webkit {
namespace gpu {

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
const size_t kMinTransferBufferSize = 1 * 256 * 1024;
const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() {
    ::gles2::Initialize();
  }

  ~GLES2Initializer() {
    ::gles2::Terminate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

static base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace anonymous

// static
scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::CreateViewContext(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    bool lose_context_when_out_of_memory,
    gfx::AcceleratedWidget window) {
  DCHECK_NE(gfx::GetGLImplementation(), gfx::kGLImplementationNone);
  bool is_offscreen = false;
  return make_scoped_ptr(new WebGraphicsContext3DInProcessCommandBufferImpl(
      scoped_ptr< ::gpu::GLInProcessContext>(),
      attributes,
      lose_context_when_out_of_memory,
      is_offscreen,
      window));
}

// static
scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    bool lose_context_when_out_of_memory) {
  bool is_offscreen = true;
  return make_scoped_ptr(new WebGraphicsContext3DInProcessCommandBufferImpl(
      scoped_ptr< ::gpu::GLInProcessContext>(),
      attributes,
      lose_context_when_out_of_memory,
      is_offscreen,
      gfx::kNullAcceleratedWidget));
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
    scoped_ptr< ::gpu::GLInProcessContext> context,
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  bool lose_context_when_out_of_memory = false;  // Not used.
  bool is_offscreen = true;                      // Not used.
  return make_scoped_ptr(new WebGraphicsContext3DInProcessCommandBufferImpl(
      context.Pass(),
      attributes,
      lose_context_when_out_of_memory,
      is_offscreen,
      gfx::kNullAcceleratedWidget /* window. Not used. */));
}

WebGraphicsContext3DInProcessCommandBufferImpl::
    WebGraphicsContext3DInProcessCommandBufferImpl(
        scoped_ptr< ::gpu::GLInProcessContext> context,
        const blink::WebGraphicsContext3D::Attributes& attributes,
        bool lose_context_when_out_of_memory,
        bool is_offscreen,
        gfx::AcceleratedWidget window)
    : share_resources_(attributes.shareResources),
      webgl_context_(attributes.noExtensions),
      is_offscreen_(is_offscreen),
      window_(window),
      context_(context.Pass()) {
  ConvertAttributes(attributes, &attribs_);
  attribs_.lose_context_when_out_of_memory = lose_context_when_out_of_memory;
}

WebGraphicsContext3DInProcessCommandBufferImpl::
    ~WebGraphicsContext3DInProcessCommandBufferImpl() {
}

// static
void WebGraphicsContext3DInProcessCommandBufferImpl::ConvertAttributes(
    const blink::WebGraphicsContext3D::Attributes& attributes,
    ::gpu::GLInProcessContextAttribs* output_attribs) {
  output_attribs->alpha_size = attributes.alpha ? 8 : 0;
  output_attribs->depth_size = attributes.depth ? 24 : 0;
  output_attribs->stencil_size = attributes.stencil ? 8 : 0;
  output_attribs->samples = attributes.antialias ? 4 : 0;
  output_attribs->sample_buffers = attributes.antialias ? 1 : 0;
  output_attribs->fail_if_major_perf_caveat =
      attributes.failIfMajorPerformanceCaveat ? 1 : 0;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::MaybeInitializeGL() {
  if (initialized_)
    return true;

  if (initialize_failed_)
    return false;

  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  if (!context_) {
    // TODO(kbr): More work will be needed in this implementation to
    // properly support GPU switching. Like in the out-of-process
    // command buffer implementation, all previously created contexts
    // will need to be lost either when the first context requesting the
    // discrete GPU is created, or the last one is destroyed.
    gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

    context_.reset(GLInProcessContext::CreateContext(
        is_offscreen_,
        window_,
        gfx::Size(1, 1),
        share_resources_,
        attribs_,
        gpu_preference));
  }

  if (context_) {
    base::Closure context_lost_callback = base::Bind(
        &WebGraphicsContext3DInProcessCommandBufferImpl::OnContextLost,
        base::Unretained(this));
    context_->SetContextLostCallback(context_lost_callback);
  } else {
    initialize_failed_ = true;
    return false;
  }

  real_gl_ = context_->GetImplementation();
  setGLInterface(real_gl_);

  if (real_gl_ && webgl_context_)
    real_gl_->EnableFeatureCHROMIUM("webgl_enable_glsl_webgl_validation");

  initialized_ = true;
  return true;
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::makeContextCurrent() {
  if (!MaybeInitializeGL())
    return false;
  ::gles2::SetGLContext(GetGLInterface());
  return context_ && !isContextLost();
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::isContextLost() {
  return context_lost_reason_ != GL_NO_ERROR;
}

WGC3Denum WebGraphicsContext3DInProcessCommandBufferImpl::
    getGraphicsResetStatusARB() {
  return context_lost_reason_;
}

::gpu::ContextSupport*
WebGraphicsContext3DInProcessCommandBufferImpl::GetContextSupport() {
  return real_gl_;
}

void WebGraphicsContext3DInProcessCommandBufferImpl::OnContextLost() {
  // TODO(kbr): improve the precision here.
  context_lost_reason_ = GL_UNKNOWN_CONTEXT_RESET_ARB;
  if (context_lost_callback_) {
    context_lost_callback_->onContextLost();
  }
}

}  // namespace gpu
}  // namespace webkit
