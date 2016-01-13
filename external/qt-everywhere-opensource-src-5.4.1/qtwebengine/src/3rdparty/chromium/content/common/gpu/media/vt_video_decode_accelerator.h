// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/gl_context_cgl.h"

namespace content {

// (Stub of a) VideoToolbox.framework implementation of the
// VideoDecodeAccelerator interface for Mac OS X.
class VTVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator,
      public base::NonThreadSafe {
 public:
  explicit VTVideoDecodeAccelerator(CGLContextObj cgl_context);
  virtual ~VTVideoDecodeAccelerator();

  // VideoDecodeAccelerator implementation.
  virtual bool Initialize(
      media::VideoCodecProfile profile,
      Client* client) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& pictures) OVERRIDE;
  virtual void ReusePictureBuffer(int32_t picture_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool CanDecodeOnIOThread() OVERRIDE;

 private:
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  CGLContextObj cgl_context_;
  media::VideoDecodeAccelerator::Client* client_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<VTVideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VTVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
