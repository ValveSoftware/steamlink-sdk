// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_SYNCHRONOUS_IMPL_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_SYNCHRONOUS_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/media/android/stream_texture_factory.h"

namespace gfx {
class SurfaceTexture;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}  //  namespace gles2
}  //  namespace gpu

namespace content {

// Factory for when using synchronous compositor in Android WebView.
class StreamTextureFactorySynchronousImpl : public StreamTextureFactory {
 public:
  class ContextProvider : public base::RefCountedThreadSafe<ContextProvider> {
   public:
    virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
        uint32 stream_id) = 0;

    virtual gpu::gles2::GLES2Interface* ContextGL() = 0;

   protected:
    friend class base::RefCountedThreadSafe<ContextProvider>;
    virtual ~ContextProvider() {}
  };

  typedef base::Callback<scoped_refptr<ContextProvider>(void)>
      CreateContextProviderCallback;

  static scoped_refptr<StreamTextureFactorySynchronousImpl> Create(
      const CreateContextProviderCallback& try_create_callback,
      int frame_id);

  virtual StreamTextureProxy* CreateProxy() OVERRIDE;
  virtual void EstablishPeer(int32 stream_id, int player_id) OVERRIDE;
  virtual unsigned CreateStreamTexture(unsigned texture_target,
                                       unsigned* texture_id,
                                       gpu::Mailbox* texture_mailbox) OVERRIDE;
  virtual void SetStreamTextureSize(int32 stream_id,
                                    const gfx::Size& size) OVERRIDE;
  virtual gpu::gles2::GLES2Interface* ContextGL() OVERRIDE;

 private:
  friend class base::RefCounted<StreamTextureFactorySynchronousImpl>;
  StreamTextureFactorySynchronousImpl(
      const CreateContextProviderCallback& try_create_callback,
      int frame_id);
  virtual ~StreamTextureFactorySynchronousImpl();

  CreateContextProviderCallback create_context_provider_callback_;
  scoped_refptr<ContextProvider> context_provider_;
  int frame_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureFactorySynchronousImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_SYNCHRONOUS_IMPL_H_
