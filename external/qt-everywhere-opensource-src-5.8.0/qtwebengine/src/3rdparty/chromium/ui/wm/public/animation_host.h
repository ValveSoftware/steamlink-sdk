// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_ANIMATION_HOST_H_
#define UI_WM_PUBLIC_ANIMATION_HOST_H_

#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"

namespace gfx {
class Vector2d;
}

namespace aura {
class Window;
namespace client {

// Interface for top level window host of animation. Communicates additional
// bounds required for animation as well as animation completion for deferring
// window closes on hide.
class AURA_EXPORT AnimationHost {
 public:
  // Ensure the host window is at least this large so that transitions have
  // sufficient space.
  // The |top_left_delta| parameter contains the offset to be subtracted from
  // the window bounds for the top left corner.
  // The |bottom_right_delta| parameter contains the offset to be added to the
  // window bounds for the bottom right.
  virtual void SetHostTransitionOffsets(
      const gfx::Vector2d& top_left_delta,
      const gfx::Vector2d& bottom_right_delta) = 0;

  // Called after the window has faded out on a hide.
  virtual void OnWindowHidingAnimationCompleted() = 0;

 protected:
  virtual ~AnimationHost() {}
};

AURA_EXPORT void SetAnimationHost(Window* window,
                                  AnimationHost* animation_host);
AURA_EXPORT AnimationHost* GetAnimationHost(Window* window);

}  // namespace client
}  // namespace aura

#endif  // UI_WM_PUBLIC_ANIMATION_HOST_H_
