// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/cursor_renderer_aura.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/window_util.h"

namespace content {

using aura::test::AuraTestBase;

class CursorRendererAuraTest : public AuraTestBase {
 public:
  CursorRendererAuraTest() {}
  ~CursorRendererAuraTest() override {}

  void SetUp() override {
    AuraTestBase::SetUp();
    // This is needed to avoid duplicate initialization across tests that leads
    // to a failure.
    if (!ui::ResourceBundle::HasSharedInstance()) {
      // Initialize the shared global resource bundle that has bitmap
      // resources needed by CursorRenderer
      base::FilePath pak_file;
      bool r = PathService::Get(base::DIR_MODULE, &pak_file);
      DCHECK(r);
      pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));
      ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
    }

    window_.reset(aura::test::CreateTestWindowWithBounds(
        gfx::Rect(0, 0, 800, 600), root_window()));
    cursor_renderer_.reset(
        new CursorRendererAura(window_.get(), kCursorEnabledOnMouseMovement));
    new wm::DefaultActivationClient(root_window());
  }

  void TearDown() override {
    cursor_renderer_.reset();
    window_.reset();
    AuraTestBase::TearDown();
  }

  void SetTickClock(base::SimpleTestTickClock* clock) {
    cursor_renderer_->tick_clock_ = clock;
  }

  base::TimeTicks Now() { return cursor_renderer_->tick_clock_->NowTicks(); }

  bool CursorDisplayed() { return cursor_renderer_->cursor_displayed_; }

  void RenderCursorOnVideoFrame(
      const scoped_refptr<media::VideoFrame>& target) {
    cursor_renderer_->RenderOnVideoFrame(target);
  }

  void SnapshotCursorState(gfx::Rect region_in_frame) {
    cursor_renderer_->SnapshotCursorState(region_in_frame);
  }

  void MoveMouseCursorWithinWindow() {
    gfx::Point point1(20, 20);
    ui::MouseEvent event1(ui::ET_MOUSE_MOVED, point1, point1, Now(), 0, 0);
    cursor_renderer_->OnMouseEvent(&event1);
    gfx::Point point2(60, 60);
    ui::MouseEvent event2(ui::ET_MOUSE_MOVED, point2, point2, Now(), 0, 0);
    cursor_renderer_->OnMouseEvent(&event2);
    aura::Env::GetInstance()->set_last_mouse_location(point2);
  }

  void MoveMouseCursorWithinWindow(gfx::Point point) {
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, point, point, Now(), 0, 0);
    cursor_renderer_->OnMouseEvent(&event);
    aura::Env::GetInstance()->set_last_mouse_location(point);
  }

  void MoveMouseCursorOutsideWindow() {
    gfx::Point point(1000, 1000);
    ui::MouseEvent event1(ui::ET_MOUSE_MOVED, point, point, Now(), 0, 0);
    cursor_renderer_->OnMouseEvent(&event1);
    aura::Env::GetInstance()->set_last_mouse_location(point);
  }

  // A very simple test of whether there are any non-zero pixels
  // in the region |rect| within |frame|.
  bool NonZeroPixelsInRegion(scoped_refptr<media::VideoFrame> frame,
                             gfx::Rect rect) {
    bool y_found = false, u_found = false, v_found = false;
    for (int y = rect.y(); y < rect.bottom(); ++y) {
      uint8_t* yplane = frame->data(media::VideoFrame::kYPlane) +
                        y * frame->row_bytes(media::VideoFrame::kYPlane);
      uint8_t* uplane = frame->data(media::VideoFrame::kUPlane) +
                        (y / 2) * frame->row_bytes(media::VideoFrame::kUPlane);
      uint8_t* vplane = frame->data(media::VideoFrame::kVPlane) +
                        (y / 2) * frame->row_bytes(media::VideoFrame::kVPlane);
      for (int x = rect.x(); x < rect.right(); ++x) {
        if (yplane[x] != 0)
          y_found = true;
        if (uplane[x / 2])
          u_found = true;
        if (vplane[x / 2])
          v_found = true;
      }
    }
    return (y_found && u_found && v_found);
  }

 protected:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<CursorRendererAura> cursor_renderer_;
};

TEST_F(CursorRendererAuraTest, CursorAlwaysDisplayed) {
  // Set up cursor renderer to always display cursor.
  cursor_renderer_.reset(
      new CursorRendererAura(window_.get(), kCursorAlwaysEnabled));

  // Cursor displayed at start.
  EXPECT_TRUE(CursorDisplayed());

  base::SimpleTestTickClock clock;
  SetTickClock(&clock);

  // Cursor displayed after mouse movement.
  MoveMouseCursorWithinWindow();
  EXPECT_TRUE(CursorDisplayed());

  // Cursor displayed after idle period.
  clock.Advance(base::TimeDelta::FromSeconds(5));
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_TRUE(CursorDisplayed());

  // Cursor displayed with mouse outside the window.
  MoveMouseCursorOutsideWindow();
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_TRUE(CursorDisplayed());
}

TEST_F(CursorRendererAuraTest, CursorDuringMouseMovement) {
  // Keep window activated.
  wm::ActivateWindow(window_.get());

  EXPECT_FALSE(CursorDisplayed());

  base::SimpleTestTickClock clock;
  SetTickClock(&clock);

  // Cursor displayed after mouse movement.
  MoveMouseCursorWithinWindow();
  EXPECT_TRUE(CursorDisplayed());

  // Cursor not be displayed after idle period.
  clock.Advance(base::TimeDelta::FromSeconds(5));
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_FALSE(CursorDisplayed());

  // Cursor displayed with mouse movement following idle period.
  MoveMouseCursorWithinWindow();
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_TRUE(CursorDisplayed());

  // Cursor not displayed if mouse outside the window
  MoveMouseCursorOutsideWindow();
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_FALSE(CursorDisplayed());
}

TEST_F(CursorRendererAuraTest, CursorOnActiveWindow) {
  EXPECT_FALSE(CursorDisplayed());

  // Cursor displayed after mouse movement.
  MoveMouseCursorWithinWindow();
  EXPECT_TRUE(CursorDisplayed());

  // Cursor not be displayed if a second window is activated.
  std::unique_ptr<aura::Window> window2(aura::test::CreateTestWindowWithBounds(
      gfx::Rect(0, 0, 800, 600), root_window()));
  wm::ActivateWindow(window2.get());
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_FALSE(CursorDisplayed());

  // Cursor displayed if window activated again.
  MoveMouseCursorWithinWindow();
  wm::ActivateWindow(window_.get());
  SnapshotCursorState(gfx::Rect(10, 10, 200, 200));
  EXPECT_TRUE(CursorDisplayed());
}

TEST_F(CursorRendererAuraTest, CursorRenderedOnFrame) {
  // Keep window activated.
  wm::ActivateWindow(window_.get());

  EXPECT_FALSE(CursorDisplayed());

  gfx::Size size(800, 600);
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateZeroInitializedFrame(media::PIXEL_FORMAT_YV12,
                                                    size, gfx::Rect(size), size,
                                                    base::TimeDelta());

  MoveMouseCursorWithinWindow(gfx::Point(60, 60));
  SnapshotCursorState(gfx::Rect(size));
  EXPECT_TRUE(CursorDisplayed());

  EXPECT_FALSE(NonZeroPixelsInRegion(frame, gfx::Rect(50, 50, 70, 70)));
  RenderCursorOnVideoFrame(frame);
  EXPECT_TRUE(NonZeroPixelsInRegion(frame, gfx::Rect(50, 50, 70, 70)));
}

TEST_F(CursorRendererAuraTest, CursorRenderedOnRootWindow) {
  cursor_renderer_.reset(new CursorRendererAura(root_window(),
      kCursorEnabledOnMouseMovement));
  EXPECT_FALSE(CursorDisplayed());

  // Cursor displayed after mouse movement.
  MoveMouseCursorWithinWindow();
  EXPECT_TRUE(CursorDisplayed());

  // Cursor being displayed even if another window is activated.
  std::unique_ptr<aura::Window> window2(aura::test::CreateTestWindowWithBounds(
      gfx::Rect(0, 0, 800, 600), root_window()));
  wm::ActivateWindow(window2.get());
  SnapshotCursorState(gfx::Rect(0, 0, 800, 600));
  EXPECT_TRUE(CursorDisplayed());
}

}  // namespace content
