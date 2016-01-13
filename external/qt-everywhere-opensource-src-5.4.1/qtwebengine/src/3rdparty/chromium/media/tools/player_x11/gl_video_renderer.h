// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

namespace media {
class VideoFrame;
}

class GlVideoRenderer : public base::RefCountedThreadSafe<GlVideoRenderer> {
 public:
  GlVideoRenderer(Display* display, Window window);

  void Paint(const scoped_refptr<media::VideoFrame>& video_frame);

 private:
  friend class base::RefCountedThreadSafe<GlVideoRenderer>;
  ~GlVideoRenderer();

  // Initializes GL rendering for the given dimensions.
  void Initialize(gfx::Size coded_size, gfx::Rect visible_rect);

  Display* display_;
  Window window_;

  // GL context.
  GLXContext gl_context_;

  // 3 textures, one for each plane.
  GLuint textures_[3];

  DISALLOW_COPY_AND_ASSIGN(GlVideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
