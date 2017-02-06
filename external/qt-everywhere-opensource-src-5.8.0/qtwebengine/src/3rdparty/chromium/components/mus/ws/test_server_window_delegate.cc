// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/server_window.h"

namespace mus {

namespace ws {

TestServerWindowDelegate::TestServerWindowDelegate()
    : root_window_(nullptr), surfaces_state_(new SurfacesState()) {}

TestServerWindowDelegate::~TestServerWindowDelegate() {}

mus::SurfacesState* TestServerWindowDelegate::GetSurfacesState() {
  return surfaces_state_.get();
}

void TestServerWindowDelegate::OnScheduleWindowPaint(ServerWindow* window) {}

const ServerWindow* TestServerWindowDelegate::GetRootWindow(
    const ServerWindow* window) const {
  return root_window_;
}

void TestServerWindowDelegate::ScheduleSurfaceDestruction(
    ServerWindow* window) {}

}  // namespace ws

}  // namespace mus
