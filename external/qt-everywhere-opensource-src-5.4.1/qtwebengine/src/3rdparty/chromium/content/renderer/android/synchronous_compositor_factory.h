// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_FACTORY_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace base {
class MessageLoopProxy;
}

namespace cc {
class ContextProvider;
class OutputSurface;
}

namespace webkit {
namespace gpu {
class ContextProviderWebContext;
}
}

namespace content {

class InputHandlerManagerClient;
class StreamTextureFactory;

// Decouples creation from usage of the parts needed for the synchonous
// compositor rendering path. In practice this is only used in single
// process mode (namely, for Android WebView) hence the implementation of
// this will be injected from the logical 'browser' side code.
class SynchronousCompositorFactory {
 public:
  // Must only be called once, e.g. on startup. Ownership of |instance| remains
  // with the caller.
  static void SetInstance(SynchronousCompositorFactory* instance);
  static SynchronousCompositorFactory* GetInstance();

  virtual scoped_refptr<base::MessageLoopProxy>
      GetCompositorMessageLoop() = 0;
  virtual bool RecordFullLayer() = 0;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      int routing_id) = 0;

  // The factory maintains ownership of the returned interface.
  virtual InputHandlerManagerClient* GetInputHandlerManagerClient() = 0;

  virtual scoped_refptr<webkit::gpu::ContextProviderWebContext>
      GetSharedOffscreenContextProviderForMainThread() = 0;
  virtual scoped_refptr<StreamTextureFactory> CreateStreamTextureFactory(
      int frame_id) = 0;
  virtual blink::WebGraphicsContext3D* CreateOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes) = 0;

 protected:
  SynchronousCompositorFactory() {}
  virtual ~SynchronousCompositorFactory() {}
};

}

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_FACTORY_H_
