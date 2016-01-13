// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_

#include "mojo/examples/sample_app/spinning_cube.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/size.h"

namespace mojo {
namespace examples {

class GLES2ClientImpl {
 public:
  explicit GLES2ClientImpl(CommandBufferPtr command_buffer);
  virtual ~GLES2ClientImpl();

  void SetSize(const Size& size);
  void HandleInputEvent(const Event& event);

 private:
  void ContextLost();
  static void ContextLostThunk(void* closure);
  void DrawAnimationFrame();
  static void DrawAnimationFrameThunk(void* closure);

  void RequestAnimationFrames();
  void CancelAnimationFrames();

  MojoTimeTicks last_time_;
  gfx::Size size_;
  SpinningCube cube_;
  gfx::PointF capture_point_;
  gfx::PointF last_drag_point_;
  MojoTimeTicks drag_start_time_;
  bool getting_animation_frames_;

  MojoGLES2Context context_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(GLES2ClientImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
