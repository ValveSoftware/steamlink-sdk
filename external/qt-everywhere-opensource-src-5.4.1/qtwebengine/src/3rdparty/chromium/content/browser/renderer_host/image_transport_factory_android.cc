// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_factory_android.h"

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/android/device_display_info.h"

namespace content {

base::LazyInstance<ObserverList<ImageTransportFactoryAndroidObserver> >::Leaky
    g_factory_observers = LAZY_INSTANCE_INITIALIZER;

class GLContextLostListener
    : public blink::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  // WebGraphicsContextLostCallback implementation.
  virtual void onContextLost() OVERRIDE;
 private:
  static void DidLoseContext();
};

namespace {

static ImageTransportFactoryAndroid* g_factory = NULL;

class CmdBufferImageTransportFactory : public ImageTransportFactoryAndroid {
 public:
  CmdBufferImageTransportFactory();
  virtual ~CmdBufferImageTransportFactory();

  virtual GLHelper* GetGLHelper() OVERRIDE;
  virtual uint32 GetChannelID() OVERRIDE {
    return BrowserGpuChannelHostFactory::instance()->GetGpuChannelId();
  }

 private:
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context_;
  scoped_ptr<GLHelper> gl_helper_;

  DISALLOW_COPY_AND_ASSIGN(CmdBufferImageTransportFactory);
};

CmdBufferImageTransportFactory::CmdBufferImageTransportFactory() {
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  scoped_refptr<GpuChannelHost> gpu_channel_host(factory->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE));
  DCHECK(gpu_channel_host);

  blink::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  GURL url("chrome://gpu/ImageTransportFactoryAndroid");
  static const size_t kBytesPerPixel = 4;
  gfx::DeviceDisplayInfo display_info;
  size_t full_screen_texture_size_in_bytes = display_info.GetDisplayHeight() *
                                             display_info.GetDisplayWidth() *
                                             kBytesPerPixel;
  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits limits;
  limits.command_buffer_size = 64 * 1024;
  limits.start_transfer_buffer_size = 64 * 1024;
  limits.min_transfer_buffer_size = 64 * 1024;
  limits.max_transfer_buffer_size = std::min(
      3 * full_screen_texture_size_in_bytes, kDefaultMaxTransferBufferSize);
  limits.mapped_memory_reclaim_limit =
      WebGraphicsContext3DCommandBufferImpl::kNoLimit;
  bool lose_context_when_out_of_memory = false;
  context_.reset(
      new WebGraphicsContext3DCommandBufferImpl(0,  // offscreen
                                                url,
                                                gpu_channel_host.get(),
                                                attrs,
                                                lose_context_when_out_of_memory,
                                                limits,
                                                NULL));
  context_->setContextLostCallback(context_lost_listener_.get());
  if (context_->makeContextCurrent())
    context_->pushGroupMarkerEXT(
        base::StringPrintf("CmdBufferImageTransportFactory-%p",
                           context_.get()).c_str());
}

CmdBufferImageTransportFactory::~CmdBufferImageTransportFactory() {
  context_->setContextLostCallback(NULL);
}

GLHelper* CmdBufferImageTransportFactory::GetGLHelper() {
  if (!gl_helper_)
    gl_helper_.reset(new GLHelper(context_->GetImplementation(),
                                  context_->GetContextSupport()));

  return gl_helper_.get();
}

}  // anonymous namespace

// static
ImageTransportFactoryAndroid* ImageTransportFactoryAndroid::GetInstance() {
  if (!g_factory)
    g_factory = new CmdBufferImageTransportFactory();

  return g_factory;
}

ImageTransportFactoryAndroid::ImageTransportFactoryAndroid()
    : context_lost_listener_(new GLContextLostListener()) {}

ImageTransportFactoryAndroid::~ImageTransportFactoryAndroid() {}

void ImageTransportFactoryAndroid::AddObserver(
    ImageTransportFactoryAndroidObserver* observer) {
  g_factory_observers.Get().AddObserver(observer);
}

void ImageTransportFactoryAndroid::RemoveObserver(
    ImageTransportFactoryAndroidObserver* observer) {
  g_factory_observers.Get().RemoveObserver(observer);
}

void GLContextLostListener::onContextLost() {
  // Need to post a task because the command buffer client cannot be deleted
  // from within this callback.
  LOG(ERROR) << "Context lost.";
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GLContextLostListener::DidLoseContext));
}

void GLContextLostListener::DidLoseContext() {
  delete g_factory;
  g_factory = NULL;
  FOR_EACH_OBSERVER(ImageTransportFactoryAndroidObserver,
      g_factory_observers.Get(),
      OnLostResources());
}

} // namespace content
