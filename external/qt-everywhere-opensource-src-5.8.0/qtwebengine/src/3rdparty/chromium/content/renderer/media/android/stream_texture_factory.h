// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "cc/layers/video_frame_provider.h"
#include "content/renderer/gpu/stream_texture_host_android.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}  // namespace gles2
class GpuChannelHost;
}  // namespace gpu

namespace content {

class ContextProviderCommandBuffer;
class StreamTextureFactory;

// The proxy class for the gpu thread to notify the compositor thread
// when a new video frame is available.
class StreamTextureProxy : public StreamTextureHost::Listener {
 public:
  ~StreamTextureProxy() override;

  // Initialize and bind to the loop, which becomes the thread that
  // a connected client will receive callbacks on. This can be called
  // on any thread, but must be called with the same loop every time.
  void BindToLoop(int32_t stream_id,
                  cc::VideoFrameProvider::Client* client,
                  scoped_refptr<base::SingleThreadTaskRunner> loop);

  // StreamTextureHost::Listener implementation:
  void OnFrameAvailable() override;

  struct Deleter {
    inline void operator()(StreamTextureProxy* ptr) const { ptr->Release(); }
  };
 private:
  friend class StreamTextureFactory;
  explicit StreamTextureProxy(StreamTextureHost* host);

  void BindOnThread(int32_t stream_id);
  void Release();

  const std::unique_ptr<StreamTextureHost> host_;

  // Protects access to |client_| and |loop_|.
  base::Lock lock_;
  cc::VideoFrameProvider::Client* client_;
  scoped_refptr<base::SingleThreadTaskRunner> loop_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureProxy);
};

typedef std::unique_ptr<StreamTextureProxy, StreamTextureProxy::Deleter>
    ScopedStreamTextureProxy;

// Factory class for managing stream textures.
class StreamTextureFactory : public base::RefCounted<StreamTextureFactory> {
 public:
  static scoped_refptr<StreamTextureFactory> Create(
      scoped_refptr<ContextProviderCommandBuffer> context_provider);

  // Create the StreamTextureProxy object.
  StreamTextureProxy* CreateProxy();

  // Send an IPC message to the browser process to request a java surface
  // object for the given stream_id. After the the surface is created,
  // it will be passed back to the WebMediaPlayerAndroid object identified by
  // the player_id.
  void EstablishPeer(int32_t stream_id, int player_id, int frame_id);

  // Creates a gpu::StreamTexture and returns its id.  Sets |*texture_id| to the
  // client-side id of the gpu::StreamTexture. The texture is produced into
  // a mailbox so it can be shipped in a VideoFrame.
  unsigned CreateStreamTexture(unsigned texture_target,
                               unsigned* texture_id,
                               gpu::Mailbox* texture_mailbox);

  // Set the streamTexture size for the given stream Id.
  void SetStreamTextureSize(int32_t texture_id, const gfx::Size& size);

  gpu::gles2::GLES2Interface* ContextGL();

 private:
  friend class base::RefCounted<StreamTextureFactory>;
  StreamTextureFactory(
      scoped_refptr<ContextProviderCommandBuffer> context_provider);
  ~StreamTextureFactory();

  scoped_refptr<ContextProviderCommandBuffer> context_provider_;
  scoped_refptr<gpu::GpuChannelHost> channel_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
