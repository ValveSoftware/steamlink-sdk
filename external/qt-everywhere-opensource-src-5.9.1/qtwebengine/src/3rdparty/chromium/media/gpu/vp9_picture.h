// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP9_PICTURE_H_
#define MEDIA_GPU_VP9_PICTURE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/filters/vp9_parser.h"

namespace media {

class V4L2VP9Picture;
class VaapiVP9Picture;

class VP9Picture : public base::RefCounted<VP9Picture> {
 public:
  VP9Picture();

  virtual V4L2VP9Picture* AsV4L2VP9Picture();
  virtual VaapiVP9Picture* AsVaapiVP9Picture();

  std::unique_ptr<Vp9FrameHeader> frame_hdr;

 protected:
  friend class base::RefCounted<VP9Picture>;
  virtual ~VP9Picture();

  DISALLOW_COPY_AND_ASSIGN(VP9Picture);
};

}  // namespace media

#endif  // MEDIA_GPU_VP9_PICTURE_H_
