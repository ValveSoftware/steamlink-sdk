// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_FRAME_PROVIDER_H_
#define CC_LAYERS_VIDEO_FRAME_PROVIDER_H_

#include "base/memory/ref_counted.h"

namespace media {
class VideoFrame;
}

namespace cc {

// Threading notes: This class may be used in a multi threaded manner.
// Specifically, the implementation may call GetCurrentFrame() or
// PutCurrentFrame() from the compositor thread. If so, the caller is
// responsible for making sure Client::DidReceiveFrame() and
// Client::DidUpdateMatrix() are only called from this same thread.
class VideoFrameProvider {
 public:
  virtual ~VideoFrameProvider() {}

  class Client {
   public:
    // Provider will call this method to tell the client to stop using it.
    // StopUsingProvider() may be called from any thread. The client should
    // block until it has PutCurrentFrame() any outstanding frames.
    virtual void StopUsingProvider() = 0;

    // Notifies the provider's client that a call to GetCurrentFrame() will
    // return new data.
    virtual void DidReceiveFrame() = 0;

    // Notifies the provider's client of a new UV transform matrix to be used.
    virtual void DidUpdateMatrix(const float* matrix) = 0;

   protected:
    virtual ~Client() {}
  };

  // May be called from any thread, but there must be some external guarantee
  // that the provider is not destroyed before this call returns.
  virtual void SetVideoFrameProviderClient(Client* client) = 0;

  // This function places a lock on the current frame and returns a pointer to
  // it. Calls to this method should always be followed with a call to
  // PutCurrentFrame().
  // Only the current provider client should call this function.
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() = 0;

  // This function releases the lock on the video frame. It should always be
  // called after GetCurrentFrame(). Frames passed into this method
  // should no longer be referenced after the call is made. Only the current
  // provider client should call this function.
  virtual void PutCurrentFrame(
      const scoped_refptr<media::VideoFrame>& frame) = 0;
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_FRAME_PROVIDER_H_
