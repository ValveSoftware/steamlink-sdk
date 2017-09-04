// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_TEST_SERVER_WINDOW_DELEGATE_H_
#define SERVICES_UI_WS_TEST_SERVER_WINDOW_DELEGATE_H_

#include "base/macros.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {

namespace ws {

struct WindowId;

class TestServerWindowDelegate : public ServerWindowDelegate {
 public:
  TestServerWindowDelegate();
  ~TestServerWindowDelegate() override;

  void set_root_window(ServerWindow* window) { root_window_ = window; }

 private:
  // ServerWindowDelegate:
  ui::DisplayCompositor* GetDisplayCompositor() override;
  ServerWindow* GetRootWindow(const ServerWindow* window) override;

  ServerWindow* root_window_;
  scoped_refptr<ui::DisplayCompositor> display_compositor_;

  DISALLOW_COPY_AND_ASSIGN(TestServerWindowDelegate);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_TEST_SERVER_WINDOW_DELEGATE_H_
