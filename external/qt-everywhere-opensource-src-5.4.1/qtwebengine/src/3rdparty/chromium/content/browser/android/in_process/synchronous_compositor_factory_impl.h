// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_

#include "base/synchronization/lock.h"
#include "content/browser/android/in_process/synchronous_input_event_filter.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/stream_texture_factory_synchronous_impl.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "webkit/common/gpu/context_provider_web_context.h"

namespace gpu {
class GLInProcessContext;
}

namespace webkit {
namespace gpu {
class WebGraphicsContext3DInProcessCommandBufferImpl;
}
}

namespace content {

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl();
  virtual ~SynchronousCompositorFactoryImpl();

  // SynchronousCompositorFactory
  virtual scoped_refptr<base::MessageLoopProxy> GetCompositorMessageLoop()
      OVERRIDE;
  virtual bool RecordFullLayer() OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(int routing_id)
      OVERRIDE;
  virtual InputHandlerManagerClient* GetInputHandlerManagerClient() OVERRIDE;
  virtual scoped_refptr<webkit::gpu::ContextProviderWebContext>
      GetSharedOffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<StreamTextureFactory> CreateStreamTextureFactory(
      int view_id) OVERRIDE;
  virtual blink::WebGraphicsContext3D* CreateOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes) OVERRIDE;


  SynchronousInputEventFilter* synchronous_input_event_filter() {
    return &synchronous_input_event_filter_;
  }

  void SetDeferredGpuService(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service);
  void SetRecordFullDocument(bool record_full_document);
  void CompositorInitializedHardwareDraw();
  void CompositorReleasedHardwareDraw();

  scoped_refptr<cc::ContextProvider>
      CreateOnscreenContextProviderForCompositorThread();
  gpu::GLInProcessContext* GetShareContext();

 private:
  bool CanCreateMainThreadContext();
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      TryCreateStreamTextureFactory();

  SynchronousInputEventFilter synchronous_input_event_filter_;

  scoped_refptr<webkit::gpu::ContextProviderWebContext>
      offscreen_context_for_main_thread_;

  scoped_refptr<gpu::InProcessCommandBuffer::Service> service_;
  scoped_ptr<gpu::GLInProcessContext> share_context_;
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      video_context_provider_;

  bool record_full_layer_;

  // |num_hardware_compositor_lock_| is updated on UI thread only but can be
  // read on renderer main thread.
  base::Lock num_hardware_compositor_lock_;
  unsigned int num_hardware_compositors_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_FACTORY_IMPL_H_
