// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SURFACE_TEXTURE_PEER_BROWSER_IMPL_H_
#define CONTENT_BROWSER_ANDROID_SURFACE_TEXTURE_PEER_BROWSER_IMPL_H_

#include "base/compiler_specific.h"
#include "content/common/android/surface_texture_peer.h"

namespace content {

// The SurfaceTexturePeer implementation for browser process.
class SurfaceTexturePeerBrowserImpl : public SurfaceTexturePeer {
 public:
  // Construct a SurfaceTexturePeerBrowserImpl object. If
  // |player_in_render_process| is true, calling EstablishSurfaceTexturePeer()
  // will send the java surface texture object to the render process through
  // ChildProcessService. Otherwise, it will pass the surface texture
  // to the MediaPlayerBridge object in the browser process.
  SurfaceTexturePeerBrowserImpl();
  virtual ~SurfaceTexturePeerBrowserImpl();

  // SurfaceTexturePeer implementation.
  virtual void EstablishSurfaceTexturePeer(
      base::ProcessHandle render_process_handle,
      scoped_refptr<gfx::SurfaceTexture> surface_texture,
      int render_frame_id,
      int player_id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceTexturePeerBrowserImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SURFACE_TEXTURE_PEER_BROWSER_IMPL_H_
