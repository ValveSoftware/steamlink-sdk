// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_NATIVE_HANDLE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_NATIVE_HANDLE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"
#include "third_party/webrtc/common_video/interface/native_handle.h"

namespace content {

class NativeHandleImpl : public webrtc::NativeHandle {
 public:
  // Wraps a video frame in the handle.
  explicit NativeHandleImpl(scoped_refptr<media::VideoFrame> frame);
  virtual ~NativeHandleImpl();

  // Retrieves the video frame in the handle. The frame is still ref-counted by
  // the handle. The ref count decreases when NativeHandleImpl is destroyed.
  virtual void* GetHandle() OVERRIDE;

 private:
  scoped_refptr<media::VideoFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(NativeHandleImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_NATIVE_HANDLE_IMPL_H_
