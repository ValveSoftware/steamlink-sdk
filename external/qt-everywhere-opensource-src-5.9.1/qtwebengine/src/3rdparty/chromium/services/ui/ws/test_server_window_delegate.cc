// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/test_server_window_delegate.h"

namespace ui {

namespace ws {

TestServerWindowDelegate::TestServerWindowDelegate()
    : root_window_(nullptr),
      display_compositor_(new DisplayCompositor(nullptr)) {}

TestServerWindowDelegate::~TestServerWindowDelegate() {}

ui::DisplayCompositor* TestServerWindowDelegate::GetDisplayCompositor() {
  return display_compositor_.get();
}

ServerWindow* TestServerWindowDelegate::GetRootWindow(
    const ServerWindow* window) {
  return root_window_;
}

}  // namespace ws

}  // namespace ui
