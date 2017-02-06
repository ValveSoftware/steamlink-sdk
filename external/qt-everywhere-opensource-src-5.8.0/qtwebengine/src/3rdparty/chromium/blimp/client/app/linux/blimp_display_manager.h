// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_LINUX_BLIMP_DISPLAY_MANAGER_H_
#define BLIMP_CLIENT_APP_LINUX_BLIMP_DISPLAY_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "blimp/client/feature/compositor/blimp_compositor_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace gfx {
class Size;
}

namespace ui {
class PlatformWindow;
}

namespace blimp {
namespace client {

class BlimpCompositorManager;
class RenderWidgetFeature;
class TabControlFeature;

class BlimpDisplayManagerDelegate {
 public:
  virtual void OnClosed() = 0;
};

class BlimpDisplayManager : public ui::PlatformWindowDelegate,
                            BlimpCompositorManagerClient {
 public:
  BlimpDisplayManager(const gfx::Size& window_size,
                      BlimpDisplayManagerDelegate* delegate,
                      RenderWidgetFeature* render_widget_feature,
                      TabControlFeature* tab_control_feature);
  ~BlimpDisplayManager() override;

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override;
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;

 private:
  // BlimpCompositorManagerClient implementation.
  void OnSwapBuffersCompleted() override;
  void DidCommitAndDrawFrame() override;

  float device_pixel_ratio_;

  BlimpDisplayManagerDelegate* delegate_;
  TabControlFeature* tab_control_feature_;

  std::unique_ptr<BlimpCompositorManager> blimp_compositor_manager_;
  std::unique_ptr<ui::PlatformWindow> platform_window_;

  DISALLOW_COPY_AND_ASSIGN(BlimpDisplayManager);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_LINUX_BLIMP_DISPLAY_MANAGER_H_
