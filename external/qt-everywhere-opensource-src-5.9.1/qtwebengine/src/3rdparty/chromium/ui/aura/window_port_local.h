// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_PORT_LOCAL_H_
#define UI_AURA_WINDOW_PORT_LOCAL_H_

#include "base/macros.h"
#include "ui/aura/window_port.h"

namespace aura {

class Window;

// WindowPort implementation for classic aura, e.g. not mus.
class AURA_EXPORT WindowPortLocal : public WindowPort {
 public:
  explicit WindowPortLocal(Window* window);
  ~WindowPortLocal() override;

  // WindowPort:
  void OnPreInit(Window* window) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWillAddChild(Window* child) override;
  void OnWillRemoveChild(Window* child) override;
  void OnWillMoveChild(size_t current_index, size_t dest_index) override;
  void OnVisibilityChanged(bool visible) override;
  void OnDidChangeBounds(const gfx::Rect& old_bounds,
                         const gfx::Rect& new_bounds) override;
  std::unique_ptr<WindowPortPropertyData> OnWillChangeProperty(
      const void* key) override;
  void OnPropertyChanged(const void* key,
                         std::unique_ptr<WindowPortPropertyData> data) override;

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowPortLocal);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_PORT_LOCAL_H_
