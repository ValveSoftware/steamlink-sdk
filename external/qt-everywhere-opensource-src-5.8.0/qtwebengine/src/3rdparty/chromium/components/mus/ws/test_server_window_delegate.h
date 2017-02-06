// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TEST_SERVER_WINDOW_DELEGATE_H_
#define COMPONENTS_MUS_WS_TEST_SERVER_WINDOW_DELEGATE_H_

#include "base/macros.h"
#include "components/mus/ws/server_window_delegate.h"

namespace mus {

namespace ws {

struct WindowId;

class TestServerWindowDelegate : public ServerWindowDelegate {
 public:
  TestServerWindowDelegate();
  ~TestServerWindowDelegate() override;

  void set_root_window(const ServerWindow* window) { root_window_ = window; }

 private:
  // ServerWindowDelegate:
  mus::SurfacesState* GetSurfacesState() override;
  void OnScheduleWindowPaint(ServerWindow* window) override;
  const ServerWindow* GetRootWindow(const ServerWindow* window) const override;
  void ScheduleSurfaceDestruction(ServerWindow* window) override;

  const ServerWindow* root_window_;
  scoped_refptr<mus::SurfacesState> surfaces_state_;

  DISALLOW_COPY_AND_ASSIGN(TestServerWindowDelegate);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TEST_SERVER_WINDOW_DELEGATE_H_
