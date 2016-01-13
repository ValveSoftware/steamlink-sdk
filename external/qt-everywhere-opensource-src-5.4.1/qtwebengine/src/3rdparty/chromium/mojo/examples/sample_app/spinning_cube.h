// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_SPINNING_CUBE_H_
#define MOJO_EXAMPLES_SAMPLE_APP_SPINNING_CUBE_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"

namespace mojo {
namespace examples {

class SpinningCube {
 public:
  SpinningCube();
  ~SpinningCube();

  void Init(uint32_t width, uint32_t height);
  void set_direction(int direction) { direction_ = direction; }
  void set_color(float r, float g, float b) {
    color_[0] = r;
    color_[1] = g;
    color_[2] = b;
  }
  void SetFlingMultiplier(float drag_distance, float drag_time);
  void UpdateForTimeDelta(float delta_time);
  void UpdateForDragDistance(float distance);
  void Draw();

  void OnGLContextLost();

 private:
  class GLState;

  void Update();

  bool initialized_;
  uint32_t width_;
  uint32_t height_;
  scoped_ptr<GLState> state_;
  float fling_multiplier_;
  int direction_;
  float color_[3];
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_SPINNING_CUBE_H_
