// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_TREE_HOST_H_
#define BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_TREE_HOST_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window_tree_host_platform.h"

namespace blimp {
namespace engine {

// This WindowTreeHost represents a top-level window in a blimp client, and its
// children.
// Note that the Aura WindowTreeHost impl creates a compositor internally,
// which will eventually get replaced with client-side rendering.
class BlimpWindowTreeHost : public aura::WindowTreeHostPlatform {
 public:
  BlimpWindowTreeHost();
  ~BlimpWindowTreeHost() override;

 private:
  // aura::WindowTreeHostPlatform overrides.
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override;

  DISALLOW_COPY_AND_ASSIGN(BlimpWindowTreeHost);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_UI_BLIMP_WINDOW_TREE_HOST_H_
