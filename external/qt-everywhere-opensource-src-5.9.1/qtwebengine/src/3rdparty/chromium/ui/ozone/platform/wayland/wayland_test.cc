// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_test.h"

#include "base/run_loop.h"

using ::testing::SaveArg;
using ::testing::_;

namespace ui {

WaylandTest::WaylandTest()
    : window(&delegate, &connection, gfx::Rect(0, 0, 800, 600)) {}

WaylandTest::~WaylandTest() {}

void WaylandTest::SetUp() {
  ASSERT_TRUE(server.Start());
  ASSERT_TRUE(connection.Initialize());
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_, _))
      .WillOnce(SaveArg<0>(&widget));
  ASSERT_TRUE(window.Initialize());
  ASSERT_NE(widget, gfx::kNullAcceleratedWidget);

  // Wait for the client to flush all pending requests from initialization.
  base::RunLoop().RunUntilIdle();

  // Pause the server after it has responded to all incoming events.
  server.Pause();

  surface = server.GetObject<wl::MockSurface>(widget);
  ASSERT_TRUE(surface);

  initialized = true;
}

void WaylandTest::TearDown() {
  if (initialized)
    Sync();
}

void WaylandTest::Sync() {
  // Resume the server, flushing its pending events.
  server.Resume();

  // Wait for the client to finish processing these events.
  base::RunLoop().RunUntilIdle();

  // Pause the server, after it has finished processing any follow-up requests
  // from the client.
  server.Pause();
}

}  // namespace ui
