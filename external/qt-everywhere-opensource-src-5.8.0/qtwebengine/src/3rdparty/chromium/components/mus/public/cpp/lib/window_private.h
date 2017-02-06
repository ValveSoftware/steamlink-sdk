// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "components/mus/public/cpp/window.h"
#include "mojo/public/cpp/bindings/array.h"

namespace mus {

// This class is a friend of a Window and contains functions to mutate internal
// state of Window.
class WindowPrivate {
 public:
  explicit WindowPrivate(Window* window);
  ~WindowPrivate();

  // Creates and returns a new Window. Caller owns the return value.
  static Window* LocalCreate();

  base::ObserverList<WindowObserver>* observers() {
    return &window_->observers_;
  }

  void ClearParent() { window_->parent_ = nullptr; }

  void ClearTransientParent() { window_->transient_parent_ = nullptr; }

  void set_visible(bool visible) { window_->visible_ = visible; }

  void set_parent_drawn(bool drawn) { window_->parent_drawn_ = drawn; }
  bool parent_drawn() { return window_->parent_drawn_; }

  void set_server_id(Id id) { window_->server_id_ = id; }
  Id server_id() { return window_->server_id_; }

  void set_client(WindowTreeClient* client) {
    window_->client_ = client;
  }

  void set_properties(const std::map<std::string, std::vector<uint8_t>>& data) {
    window_->properties_ = data;
  }

  void LocalSetDisplay(int64_t new_display) {
    window_->LocalSetDisplay(new_display);
  }

  void LocalDestroy() { window_->LocalDestroy(); }
  void LocalAddChild(Window* child) { window_->LocalAddChild(child); }
  void LocalRemoveChild(Window* child) { window_->LocalRemoveChild(child); }
  void LocalAddTransientWindow(Window* child) {
    window_->LocalAddTransientWindow(child);
  }
  void LocalRemoveTransientWindow(Window* child) {
    window_->LocalRemoveTransientWindow(child);
  }
  void LocalUnsetModal() { window_->is_modal_ = false; }
  void LocalReorder(Window* relative, mojom::OrderDirection direction) {
    window_->LocalReorder(relative, direction);
  }
  void LocalSetBounds(const gfx::Rect& old_bounds,
                      const gfx::Rect& new_bounds) {
    window_->LocalSetBounds(old_bounds, new_bounds);
  }
  void LocalSetClientArea(
      const gfx::Insets& client_area,
      const std::vector<gfx::Rect>& additional_client_areas) {
    window_->LocalSetClientArea(client_area, additional_client_areas);
  }
  void LocalSetParentDrawn(bool drawn) { window_->LocalSetParentDrawn(drawn); }
  void LocalSetVisible(bool visible) { window_->LocalSetVisible(visible); }
  void LocalSetOpacity(float opacity) { window_->LocalSetOpacity(opacity); }
  void LocalSetPredefinedCursor(mojom::Cursor cursor) {
    window_->LocalSetPredefinedCursor(cursor);
  }
  void LocalSetSharedProperty(const std::string& name,
                              mojo::Array<uint8_t> new_data);
  void LocalSetSharedProperty(const std::string& name,
                              const std::vector<uint8_t>* data) {
    window_->LocalSetSharedProperty(name, data);
  }
  void NotifyWindowStackingChanged() { window_->NotifyWindowStackingChanged(); }

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowPrivate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_WINDOW_PRIVATE_H_
