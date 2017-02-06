// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_display.h"

namespace ui {

TEST(WaylandDisplayTest, UseUnstableVersion) {
  base::MessageLoopForUI message_loop;
  wl::FakeServer server;
  EXPECT_CALL(*server.xdg_shell(),
              UseUnstableVersion(XDG_SHELL_VERSION_CURRENT));
  ASSERT_TRUE(server.Start());
  WaylandDisplay display;
  ASSERT_TRUE(display.Initialize());
  display.StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();
}

TEST(WaylandDisplayTest, Ping) {
  base::MessageLoopForUI message_loop;
  wl::FakeServer server;
  ASSERT_TRUE(server.Start());
  WaylandDisplay display;
  ASSERT_TRUE(display.Initialize());
  display.StartProcessingEvents();

  base::RunLoop().RunUntilIdle();
  server.Pause();

  xdg_shell_send_ping(server.xdg_shell()->resource(), 1234);
  EXPECT_CALL(*server.xdg_shell(), Pong(1234));

  server.Resume();
  base::RunLoop().RunUntilIdle();
  server.Pause();
}

}  // namespace ui
