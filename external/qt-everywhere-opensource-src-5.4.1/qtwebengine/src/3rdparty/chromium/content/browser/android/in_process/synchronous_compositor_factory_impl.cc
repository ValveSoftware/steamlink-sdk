// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"

#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

using webkit::gpu::ContextProviderWebContext;

namespace content {

namespace {

blink::WebGraphicsContext3D::Attributes GetDefaultAttribs() {
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return attributes;
}

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

scoped_ptr<gpu::GLInProcessContext> CreateOffscreenContext(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  const gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  gpu::GLInProcessContextAttribs in_process_attribs;
  WebGraphicsContext3DInProcessCommandBufferImpl::ConvertAttributes(
      attributes, &in_process_attribs);
  in_process_attribs.lose_context_when_out_of_memory = 1;

  scoped_ptr<gpu::GLInProcessContext> context(
      gpu::GLInProcessContext::Create(NULL /* service */,
                                      NULL /* surface */,
                                      true /* is_offscreen */,
                                      gfx::kNullAcceleratedWidget,
                                      gfx::Size(1, 1),
                                      NULL /* share_context */,
                                      false /* share_resources */,
                                      in_process_attribs,
                                      gpu_preference));
  return context.Pass();
}

scoped_ptr<gpu::GLInProcessContext> CreateContext(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    gpu::GLInProcessContext* share_context) {
  const gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;
  gpu::GLInProcessContextAttribs in_process_attribs;
  WebGraphicsContext3DInProcessCommandBufferImpl::ConvertAttributes(
      GetDefaultAttribs(), &in_process_attribs);
  in_process_attribs.lose_context_when_out_of_memory = 1;

  scoped_ptr<gpu::GLInProcessContext> context(
      gpu::GLInProcessContext::Create(service,
                                      NULL /* surface */,
                                      false /* is_offscreen */,
                                      gfx::kNullAcceleratedWidget,
                                      gfx::Size(1, 1),
                                      share_context,
                                      false /* share_resources */,
                                      in_process_attribs,
                                      gpu_preference));
  return context.Pass();
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> WrapContext(
    scoped_ptr<gpu::GLInProcessContext> context) {
  if (!context.get())
    return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>();

  return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(
      WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
          context.Pass(), GetDefaultAttribs()));
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WrapContextWithAttributes(
    scoped_ptr<gpu::GLInProcessContext> context,
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  if (!context.get())
    return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>();

  return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(
      WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
          context.Pass(), attributes));
}

class VideoContextProvider
    : public StreamTextureFactorySynchronousImpl::ContextProvider {
 public:
  VideoContextProvider(
      scoped_ptr<gpu::GLInProcessContext> gl_in_process_context)
      : gl_in_process_context_(gl_in_process_context.get()) {

    context_provider_ = webkit::gpu::ContextProviderInProcess::Create(
        WrapContext(gl_in_process_context.Pass()),
        "Video-Offscreen-main-thread");
    context_provider_->BindToCurrentThread();
  }

  virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32 stream_id) OVERRIDE {
    return gl_in_process_context_->GetSurfaceTexture(stream_id);
  }

  virtual gpu::gles2::GLES2Interface* ContextGL() OVERRIDE {
    return context_provider_->ContextGL();
  }

 private:
  friend class base::RefCountedThreadSafe<VideoContextProvider>;
  virtual ~VideoContextProvider() {}

  scoped_refptr<cc::ContextProvider> context_provider_;
  gpu::GLInProcessContext* gl_in_process_context_;

  DISALLOW_COPY_AND_ASSIGN(VideoContextProvider);
};

}  // namespace

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

SynchronousCompositorFactoryImpl::SynchronousCompositorFactoryImpl()
    : record_full_layer_(true),
      num_hardware_compositors_(0) {
  SynchronousCompositorFactory::SetInstance(this);
}

SynchronousCompositorFactoryImpl::~SynchronousCompositorFactoryImpl() {}

scoped_refptr<base::MessageLoopProxy>
SynchronousCompositorFactoryImpl::GetCompositorMessageLoop() {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

bool
SynchronousCompositorFactoryImpl::RecordFullLayer() {
  return record_full_layer_;
}

scoped_ptr<cc::OutputSurface>
SynchronousCompositorFactoryImpl::CreateOutputSurface(int routing_id) {
  scoped_ptr<SynchronousCompositorOutputSurface> output_surface(
      new SynchronousCompositorOutputSurface(routing_id));
  return output_surface.PassAs<cc::OutputSurface>();
}

InputHandlerManagerClient*
SynchronousCompositorFactoryImpl::GetInputHandlerManagerClient() {
  return synchronous_input_event_filter();
}

scoped_refptr<ContextProviderWebContext> SynchronousCompositorFactoryImpl::
    GetSharedOffscreenContextProviderForMainThread() {
  bool failed = false;
  if ((!offscreen_context_for_main_thread_.get() ||
       offscreen_context_for_main_thread_->DestroyedOnMainThread())) {
    scoped_ptr<gpu::GLInProcessContext> context =
        CreateOffscreenContext(GetDefaultAttribs());
    offscreen_context_for_main_thread_ =
        webkit::gpu::ContextProviderInProcess::Create(
            WrapContext(context.Pass()),
            "Compositor-Offscreen-main-thread");
    failed = !offscreen_context_for_main_thread_.get() ||
             !offscreen_context_for_main_thread_->BindToCurrentThread();
  }

  if (failed) {
    offscreen_context_for_main_thread_ = NULL;
  }
  return offscreen_context_for_main_thread_;
}

scoped_refptr<cc::ContextProvider> SynchronousCompositorFactoryImpl::
    CreateOnscreenContextProviderForCompositorThread() {
  DCHECK(service_);

  if (!share_context_.get())
    share_context_ = CreateContext(service_, NULL);
  return webkit::gpu::ContextProviderInProcess::Create(
      WrapContext(CreateContext(service_, share_context_.get())),
      "Child-Compositor");
}

gpu::GLInProcessContext* SynchronousCompositorFactoryImpl::GetShareContext() {
  DCHECK(share_context_.get());
  return share_context_.get();
}

scoped_refptr<StreamTextureFactory>
SynchronousCompositorFactoryImpl::CreateStreamTextureFactory(int frame_id) {
  scoped_refptr<StreamTextureFactorySynchronousImpl> factory(
      StreamTextureFactorySynchronousImpl::Create(
          base::Bind(
              &SynchronousCompositorFactoryImpl::TryCreateStreamTextureFactory,
              base::Unretained(this)),
          frame_id));
  return factory;
}

blink::WebGraphicsContext3D*
SynchronousCompositorFactoryImpl::CreateOffscreenGraphicsContext3D(
    const blink::WebGraphicsContext3D::Attributes& attributes) {
  return WrapContextWithAttributes(CreateOffscreenContext(attributes),
                                   attributes).release();
}

void SynchronousCompositorFactoryImpl::CompositorInitializedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  num_hardware_compositors_++;
}

void SynchronousCompositorFactoryImpl::CompositorReleasedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  DCHECK_GT(num_hardware_compositors_, 0u);
  num_hardware_compositors_--;
  if (num_hardware_compositors_ == 0) {
    // Nullify the video_context_provider_ now so that it is not null only if
    // there is at least 1 hardware compositor
    video_context_provider_ = NULL;
  }
}

bool SynchronousCompositorFactoryImpl::CanCreateMainThreadContext() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  return num_hardware_compositors_ > 0;
}

scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
SynchronousCompositorFactoryImpl::TryCreateStreamTextureFactory() {
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      context_provider;
  // This check only guarantees the main thread context is created after
  // a compositor did successfully initialize hardware draw in the past.
  // When all compositors have released hardware draw, main thread context
  // creation is guaranteed to fail.
  if (CanCreateMainThreadContext() && !video_context_provider_) {
    DCHECK(service_);
    DCHECK(share_context_.get());

    video_context_provider_ = new VideoContextProvider(
        CreateContext(service_, share_context_.get()));
  }
  return video_context_provider_;
}

void SynchronousCompositorFactoryImpl::SetDeferredGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(!service_);
  service_ = service;
}

void SynchronousCompositorFactoryImpl::SetRecordFullDocument(
    bool record_full_document) {
  record_full_layer_ = record_full_document;
}

}  // namespace content
