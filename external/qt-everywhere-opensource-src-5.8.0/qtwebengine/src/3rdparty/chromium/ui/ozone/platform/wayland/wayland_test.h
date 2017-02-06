// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_TEST_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_TEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/mock_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/wayland_display.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

// WaylandTest is a base class that sets up a display, window, and fake server,
// and allows easy synchronization between them.
class WaylandTest : public testing::Test {
 public:
  WaylandTest();
  ~WaylandTest() override;

  void SetUp() override;
  void TearDown() override;

  void Sync();

 private:
  bool initialized = false;
  base::MessageLoopForUI message_loop;

 protected:
  wl::FakeServer server;
  wl::MockSurface* surface;

  WaylandDisplay display;
  MockPlatformWindowDelegate delegate;
  WaylandWindow window;
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandTest);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_TEST_H_
