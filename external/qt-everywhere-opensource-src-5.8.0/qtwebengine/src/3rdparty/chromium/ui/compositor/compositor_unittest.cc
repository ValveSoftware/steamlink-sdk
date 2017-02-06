// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/compositor/test/draw_waiter_for_test.h"

using testing::Mock;
using testing::_;

namespace ui {
namespace {

ACTION_P2(RemoveObserver, compositor, observer) {
  compositor->RemoveBeginFrameObserver(observer);
}

// Test fixture for tests that require a ui::Compositor with a real task
// runner.
class CompositorTest : public testing::Test {
 public:
  CompositorTest() {}
  ~CompositorTest() override {}

  void SetUp() override {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();

    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(false);

    compositor_.reset(new ui::Compositor(context_factory, task_runner_));
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  }
  void TearDown() override {
    compositor_.reset();
    ui::TerminateContextFactoryForTests();
  }

 protected:
  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }
  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(CompositorTest);
};

}  // namespace

TEST_F(CompositorTest, LocksTimeOut) {
  scoped_refptr<ui::CompositorLock> lock;
  {
    base::RunLoop run_loop;
    // Ensure that the lock times out by default.
    lock = compositor()->GetCompositorLock();
    EXPECT_TRUE(compositor()->IsLocked());
    task_runner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
    run_loop.Run();
    EXPECT_FALSE(compositor()->IsLocked());
  }

  {
    base::RunLoop run_loop;
    // Ensure that the lock does not time out when set.
    compositor()->SetLocksWillTimeOut(false);
    lock = compositor()->GetCompositorLock();
    EXPECT_TRUE(compositor()->IsLocked());
    task_runner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
    run_loop.Run();
    EXPECT_TRUE(compositor()->IsLocked());
  }
}

TEST_F(CompositorTest, ReleaseWidgetWithOutputSurfaceNeverCreated) {
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
}

#if defined(OS_WIN)
// TODO(crbug.com/608436): Flaky on windows trybots
#define MAYBE_CreateAndReleaseOutputSurface \
  DISABLED_CreateAndReleaseOutputSurface
#else
#define MAYBE_CreateAndReleaseOutputSurface CreateAndReleaseOutputSurface
#endif
TEST_F(CompositorTest, MAYBE_CreateAndReleaseOutputSurface) {
  std::unique_ptr<Layer> root_layer(new Layer(ui::LAYER_SOLID_COLOR));
  root_layer->SetBounds(gfx::Rect(10, 10));
  compositor()->SetRootLayer(root_layer.get());
  compositor()->SetScaleAndSize(1.0f, gfx::Size(10, 10));
  DCHECK(compositor()->IsVisible());
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  compositor()->SetRootLayer(nullptr);
}

}  // namespace ui
