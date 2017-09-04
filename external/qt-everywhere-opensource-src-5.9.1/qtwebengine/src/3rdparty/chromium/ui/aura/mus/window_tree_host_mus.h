// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host_platform.h"

namespace display {
class Display;
}

namespace aura {

class InputMethodMus;
class WindowPortMus;
class WindowTreeClient;
class WindowTreeHostMusDelegate;

class AURA_EXPORT WindowTreeHostMus : public aura::WindowTreeHostPlatform {
 public:
  WindowTreeHostMus(
      std::unique_ptr<WindowPortMus> window_port,
      WindowTreeHostMusDelegate* delegate,
      int64_t display_id,
      const std::map<std::string, std::vector<uint8_t>>* properties = nullptr);
  WindowTreeHostMus(
      WindowTreeClient* window_tree_client,
      const std::map<std::string, std::vector<uint8_t>>* properties = nullptr);

  ~WindowTreeHostMus() override;

  // Sets the bounds in dips.
  void SetBoundsFromServer(const gfx::Rect& bounds);

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return aura::WindowTreeHostPlatform::SendEventToProcessor(event);
  }

  InputMethodMus* input_method() { return input_method_.get(); }

  // Intended only for WindowTreeClient to call.
  void set_display_id(int64_t id) { display_id_ = id; }
  int64_t display_id() const { return display_id_; }
  display::Display GetDisplay() const;

  // aura::WindowTreeHostPlatform:
  void ShowImpl() override;
  void HideImpl() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;
  gfx::ICCProfile GetICCProfileForCurrentDisplay() override;

 private:
  int64_t display_id_;

  WindowTreeHostMusDelegate* delegate_;

  bool in_set_bounds_from_server_ = false;

  std::unique_ptr<InputMethodMus> input_method_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
