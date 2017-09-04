// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DEMO_MUS_DEMO_H_
#define SERVICES_UI_DEMO_MUS_DEMO_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/screen_base.h"

namespace ui {
class BitmapUploader;
class GpuService;

namespace demo {

// A simple MUS Demo service. This service connects to the service:ui, creates a
// new window and draws a spinning square in the center of the window. Provides
// a simple way to demonstrate that the graphic stack works as intended.
class MusDemo : public service_manager::Service,
                public WindowTreeClientDelegate,
                public WindowManagerDelegate {
 public:
  MusDemo();
  ~MusDemo() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // WindowTreeClientDelegate:
  void OnEmbed(Window* root) override;
  void OnEmbedRootDestroyed(Window* root) override;
  void OnLostConnection(WindowTreeClient* client) override;
  void OnPointerEventObserved(const PointerEvent& event,
                              Window* target) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(WindowManagerClient* client) override;
  bool OnWmSetBounds(Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<Window*>& client_windows,
                                  bool janky) override;
  void OnWmNewDisplay(Window* window, const display::Display& display) override;
  void OnWmDisplayRemoved(ui::Window* window) override;
  void OnWmDisplayModified(const display::Display& display) override;
  void OnWmPerformMoveLoop(Window* window,
                           mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(Window* window) override;

  // Allocate a bitmap the same size as the window to draw into.
  void AllocBitmap();

  // Draws one frame, incrementing the rotation angle.
  void DrawFrame();

  Window* window_ = nullptr;
  std::unique_ptr<WindowTreeClient> window_tree_client_;
  std::unique_ptr<GpuService> gpu_service_;

  // Dummy screen required to be the screen instance.
  std::unique_ptr<display::ScreenBase> screen_;

  // Used to send frames to mus.
  std::unique_ptr<BitmapUploader> uploader_;

  // Bitmap that is the same size as our client window area.
  SkBitmap bitmap_;

  // Timer for calling DrawFrame().
  base::RepeatingTimer timer_;

  // Current rotation angle for drawing.
  double angle_ = 0.0;

  // Last time a frame was drawn.
  base::TimeTicks last_draw_frame_time_;

  DISALLOW_COPY_AND_ASSIGN(MusDemo);
};

}  // namespace demo
}  // namespace ui

#endif  // SERVICES_UI_DEMO_MUS_DEMO_H_
