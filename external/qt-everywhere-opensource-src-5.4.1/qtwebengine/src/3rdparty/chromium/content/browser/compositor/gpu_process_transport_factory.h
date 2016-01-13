// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_

#include <map>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/compositor/compositor.h"

namespace base {
class Thread;
}

namespace cc {
class SurfaceManager;
}

namespace content {
class BrowserCompositorOutputSurface;
class BrowserCompositorOutputSurfaceProxy;
class CompositorSwapClient;
class ContextProviderCommandBuffer;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;

class GpuProcessTransportFactory
    : public ui::ContextFactory,
      public ImageTransportFactory {
 public:
  GpuProcessTransportFactory();

  virtual ~GpuProcessTransportFactory();

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
  CreateOffscreenCommandBufferContext();

  // ui::ContextFactory implementation.
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      ui::Compositor* compositor, bool software_fallback) OVERRIDE;
  virtual scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* source,
      ui::Layer* target) OVERRIDE;
  virtual void RemoveReflector(
      scoped_refptr<ui::Reflector> reflector) OVERRIDE;
  virtual void RemoveCompositor(ui::Compositor* compositor) OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      SharedMainThreadContextProvider() OVERRIDE;
  virtual bool DoesCreateTestContexts() OVERRIDE;
  virtual cc::SharedBitmapManager* GetSharedBitmapManager() OVERRIDE;
  virtual base::MessageLoopProxy* GetCompositorMessageLoop() OVERRIDE;

  // ImageTransportFactory implementation.
  virtual ui::ContextFactory* GetContextFactory() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetSharedSurfaceHandle() OVERRIDE;
  virtual GLHelper* GetGLHelper() OVERRIDE;
  virtual void AddObserver(ImageTransportFactoryObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      ImageTransportFactoryObserver* observer) OVERRIDE;
#if defined(OS_MACOSX)
  virtual void OnSurfaceDisplayed(int surface_id) OVERRIDE;
#endif

 private:
  struct PerCompositorData;

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor);
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
      CreateContextCommon(int surface_id);

  void OnLostMainThreadSharedContextInsideCallback();
  void OnLostMainThreadSharedContext();

  typedef std::map<ui::Compositor*, PerCompositorData*> PerCompositorDataMap;
  scoped_ptr<base::Thread> compositor_thread_;
  PerCompositorDataMap per_compositor_data_;
  scoped_refptr<ContextProviderCommandBuffer> shared_main_thread_contexts_;
  scoped_ptr<GLHelper> gl_helper_;
  ObserverList<ImageTransportFactoryObserver> observer_list_;
  base::WeakPtrFactory<GpuProcessTransportFactory> callback_factory_;
  scoped_ptr<cc::SurfaceManager> surface_manager_;

  // The contents of this map and its methods may only be used on the compositor
  // thread.
  IDMap<BrowserCompositorOutputSurface> output_surface_map_;

  scoped_refptr<BrowserCompositorOutputSurfaceProxy> output_surface_proxy_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
