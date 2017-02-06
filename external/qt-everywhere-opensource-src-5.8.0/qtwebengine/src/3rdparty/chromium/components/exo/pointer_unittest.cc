// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shell_window_ids.h"
#include "ash/shell.h"
#include "components/exo/buffer.h"
#include "components/exo/pointer.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"

namespace exo {
namespace {

using PointerTest = test::ExoTestBase;

class MockPointerDelegate : public PointerDelegate {
 public:
  MockPointerDelegate() {}

  // Overridden from PointerDelegate:
  MOCK_METHOD1(OnPointerDestroying, void(Pointer*));
  MOCK_CONST_METHOD1(CanAcceptPointerEventsForSurface, bool(Surface*));
  MOCK_METHOD3(OnPointerEnter, void(Surface*, const gfx::PointF&, int));
  MOCK_METHOD1(OnPointerLeave, void(Surface*));
  MOCK_METHOD2(OnPointerMotion, void(base::TimeTicks, const gfx::PointF&));
  MOCK_METHOD3(OnPointerButton, void(base::TimeTicks, int, bool));
  MOCK_METHOD3(OnPointerScroll,
               void(base::TimeTicks, const gfx::Vector2dF&, bool));
  MOCK_METHOD1(OnPointerScrollCancel, void(base::TimeTicks));
  MOCK_METHOD1(OnPointerScrollStop, void(base::TimeTicks));
  MOCK_METHOD0(OnPointerFrame, void());
};

TEST_F(PointerTest, SetCursor) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  std::unique_ptr<Surface> pointer_surface(new Surface);
  std::unique_ptr<Buffer> pointer_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  pointer_surface->Attach(pointer_buffer.get());
  pointer_surface->Commit();

  // Set pointer surface.
  pointer->SetCursor(pointer_surface.get(), gfx::Point());

  // Adjust hotspot.
  pointer->SetCursor(pointer_surface.get(), gfx::Point(1, 1));

  // Unset pointer surface.
  pointer->SetCursor(nullptr, gfx::Point());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerEnter) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(1);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerLeave) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(2);
  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().bottom_right());

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerMotion) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(6);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(1, 1));

  std::unique_ptr<Surface> sub_surface(new Surface);
  surface->AddSubSurface(sub_surface.get());
  surface->SetSubSurfacePosition(sub_surface.get(), gfx::Point(5, 5));
  gfx::Size sub_buffer_size(5, 5);
  std::unique_ptr<Buffer> sub_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(sub_buffer_size)));
  sub_surface->Attach(sub_buffer.get());
  sub_surface->Commit();
  surface->Commit();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(sub_surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, OnPointerLeave(surface.get()));
  EXPECT_CALL(delegate, OnPointerEnter(sub_surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(sub_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(1, 1)));
  generator.MoveMouseTo(sub_surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(1, 1));

  std::unique_ptr<Surface> child_surface(new Surface);
  std::unique_ptr<ShellSurface> child_shell_surface(new ShellSurface(
      child_surface.get(), shell_surface.get(), gfx::Rect(9, 9, 1, 1), true,
      ash::kShellWindowId_DefaultContainer));
  gfx::Size child_buffer_size(15, 15);
  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size)));
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(child_surface.get()))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, OnPointerLeave(sub_surface.get()));
  EXPECT_CALL(delegate, OnPointerEnter(child_surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate, OnPointerMotion(testing::_, gfx::PointF(10, 10)));
  generator.MoveMouseTo(child_surface->window()->GetBoundsInScreen().origin() +
                        gfx::Vector2d(10, 10));

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerButton) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(3);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate,
              OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, true));
  EXPECT_CALL(delegate,
              OnPointerButton(testing::_, ui::EF_LEFT_MOUSE_BUTTON, false));
  generator.ClickLeftButton();

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerScroll) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  gfx::Point location = surface->window()->GetBoundsInScreen().origin();

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(4);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(location);

  {
    // Expect fling stop followed by scroll and scroll stop.
    testing::InSequence sequence;

    EXPECT_CALL(delegate, OnPointerScrollCancel(testing::_));
    EXPECT_CALL(delegate,
                OnPointerScroll(testing::_, gfx::Vector2dF(1.2, 1.2), false));
    EXPECT_CALL(delegate, OnPointerScrollStop(testing::_));
  }
  generator.ScrollSequence(location, base::TimeDelta(), 1, 1, 1, 1);

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

TEST_F(PointerTest, OnPointerScrollDiscrete) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  MockPointerDelegate delegate;
  std::unique_ptr<Pointer> pointer(new Pointer(&delegate));
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  EXPECT_CALL(delegate, CanAcceptPointerEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnPointerFrame()).Times(2);

  EXPECT_CALL(delegate, OnPointerEnter(surface.get(), gfx::PointF(), 0));
  generator.MoveMouseTo(surface->window()->GetBoundsInScreen().origin());

  EXPECT_CALL(delegate,
              OnPointerScroll(testing::_, gfx::Vector2dF(1, 1), true));
  generator.MoveMouseWheel(1, 1);

  EXPECT_CALL(delegate, OnPointerDestroying(pointer.get()));
  pointer.reset();
}

}  // namespace
}  // namespace exo
