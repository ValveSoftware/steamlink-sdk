// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_CLIENT_H_
#define CC_SURFACES_DISPLAY_CLIENT_H_

#include "cc/quads/render_pass.h"

namespace cc {

class DisplayClient {
 public:
  virtual ~DisplayClient() {}
  virtual void DisplayOutputSurfaceLost() = 0;
  virtual void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                                      const RenderPassList& render_passes) = 0;
  virtual void DisplayDidDrawAndSwap() = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_DISPLAY_CLIENT_H_
