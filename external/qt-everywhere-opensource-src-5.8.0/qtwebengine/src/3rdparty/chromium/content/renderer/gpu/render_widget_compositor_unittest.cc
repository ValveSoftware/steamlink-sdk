// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/failure_output_surface.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/trees/layer_tree_host.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/render_widget.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/fake_renderer_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

using testing::AllOf;
using testing::Field;

namespace content {
namespace {

class StubRenderWidgetCompositorDelegate
    : public RenderWidgetCompositorDelegate {
 public:
  // RenderWidgetCompositorDelegate implementation.
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void BeginMainFrame(double frame_time_sec) override {}
  std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      bool fallback) override {
    return nullptr;
  }
  std::unique_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource()
      override {
    return nullptr;
  }
  void DidCommitAndDrawCompositorFrame() override {}
  void DidCommitCompositorFrame() override {}
  void DidCompletePageScaleAnimation() override {}
  void DidCompleteSwapBuffers() override {}
  void ForwardCompositorProto(const std::vector<uint8_t>& proto) override {}
  bool IsClosing() const override { return false; }
  void OnSwapBuffersAborted() override {}
  void OnSwapBuffersComplete() override {}
  void OnSwapBuffersPosted() override {}
  void RequestScheduleAnimation() override {}
  void UpdateVisualState() override {}
  void WillBeginCompositorFrame() override {}
};

class FakeRenderWidgetCompositorDelegate
    : public StubRenderWidgetCompositorDelegate {
 public:
  FakeRenderWidgetCompositorDelegate() = default;

  std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      bool fallback) override {
    EXPECT_EQ(
        num_requests_since_last_success_ >
            RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
        fallback);
    last_create_was_fallback_ = fallback;
    bool success = num_failures_ >= num_failures_before_success_;
    if (success) {
      std::unique_ptr<cc::TestWebGraphicsContext3D> context =
          cc::TestWebGraphicsContext3D::Create();
      // Image support required for synchronous compositing.
      context->set_support_image(true);
      // Create delegating surface so that max_pending_frames = 1.
      return cc::FakeOutputSurface::CreateDelegating3d(std::move(context));
    }
    return use_null_output_surface_
               ? nullptr
               : base::WrapUnique(new cc::FailureOutputSurface(true));
  }

  std::unique_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource()
      override {
    double refresh_rate = 200.0;
    bool tick_automatically = true;
    return base::MakeUnique<cc::FakeExternalBeginFrameSource>(
        refresh_rate, tick_automatically);
  }

  void add_success() {
    if (last_create_was_fallback_)
      ++num_fallback_successes_;
    else
      ++num_successes_;
    num_requests_since_last_success_ = 0;
  }
  int num_successes() const { return num_successes_; }
  int num_fallback_successes() const { return num_fallback_successes_; }

  void add_request() {
    ++num_requests_since_last_success_;
    ++num_requests_;
  }
  int num_requests() const { return num_requests_; }

  void add_failure() { ++num_failures_; }
  int num_failures() const { return num_failures_; }

  void set_num_failures_before_success(int n) {
    num_failures_before_success_ = n;
  }
  int num_failures_before_success() const {
    return num_failures_before_success_;
  }

  void set_use_null_output_surface(bool u) { use_null_output_surface_ = u; }
  bool use_null_output_surface() const { return use_null_output_surface_; }

 private:
  int num_requests_ = 0;
  int num_requests_since_last_success_ = 0;
  int num_failures_ = 0;
  int num_failures_before_success_ = 0;
  int num_fallback_successes_ = 0;
  int num_successes_ = 0;
  bool last_create_was_fallback_ = false;
  bool use_null_output_surface_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeRenderWidgetCompositorDelegate);
};

// Verify that failing to create an output surface will cause the compositor
// to attempt to repeatedly create another output surface.  After enough
// failures, verify that it attempts to create a fallback output surface.
// The use null output surface parameter allows testing whether failures
// from RenderWidget (couldn't create an output surface) vs failures from
// the compositor (couldn't bind the output surface) are handled identically.
class RenderWidgetCompositorOutputSurface : public RenderWidgetCompositor {
 public:
  RenderWidgetCompositorOutputSurface(
      FakeRenderWidgetCompositorDelegate* delegate,
      CompositorDependencies* compositor_deps)
      : RenderWidgetCompositor(delegate, compositor_deps),
        delegate_(delegate) {}

  using RenderWidgetCompositor::Initialize;

  // Force a new output surface to be created.
  void SynchronousComposite() {
   layer_tree_host()->DidLoseOutputSurface();

   base::TimeTicks some_time;
   layer_tree_host()->Composite(some_time);
  }

  void RequestNewOutputSurface() override {
    delegate_->add_request();
    RenderWidgetCompositor::RequestNewOutputSurface();
  }

  void DidInitializeOutputSurface() override {
    delegate_->add_success();
    if (delegate_->num_requests() == expected_requests_) {
      EndTest();
    } else {
      RenderWidgetCompositor::DidInitializeOutputSurface();
      // Post the synchronous composite task so that it is not called
      // reentrantly as a part of RequestNewOutputSurface.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&RenderWidgetCompositorOutputSurface::SynchronousComposite,
                     base::Unretained(this)));
    }
  }

  void DidFailToInitializeOutputSurface() override {
    delegate_->add_failure();
    if (delegate_->num_requests() == expected_requests_) {
      EndTest();
      return;
    }

    RenderWidgetCompositor::DidFailToInitializeOutputSurface();
  }

  void SetUp(int expected_successes, int expected_fallback_succeses) {
    expected_successes_ = expected_successes;
    expected_fallback_successes_ = expected_fallback_succeses;
    expected_requests_ = delegate_->num_failures_before_success() +
                         expected_successes_ + expected_fallback_successes_;
  }

  void EndTest() { base::MessageLoop::current()->QuitWhenIdle(); }

  void AfterTest() {
    EXPECT_EQ(delegate_->num_failures_before_success(),
              delegate_->num_failures());
    EXPECT_EQ(expected_successes_, delegate_->num_successes());
    EXPECT_EQ(expected_fallback_successes_,
              delegate_->num_fallback_successes());
    EXPECT_EQ(expected_requests_, delegate_->num_requests());
  }

 private:
  FakeRenderWidgetCompositorDelegate* delegate_;
  int expected_successes_ = 0;
  int expected_fallback_successes_ = 0;
  int expected_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetCompositorOutputSurface);
};

class RenderWidgetCompositorOutputSurfaceTest : public testing::Test {
 public:
  RenderWidgetCompositorOutputSurfaceTest()
      : render_widget_compositor_(&compositor_delegate_, &compositor_deps_) {
    render_widget_compositor_.Initialize(1.f /* initial_device_scale_factor */);
  }

  void RunTest(bool use_null_output_surface,
               int num_failures_before_success,
               int expected_successes,
               int expected_fallback_succeses) {
    compositor_delegate_.set_use_null_output_surface(use_null_output_surface);
    compositor_delegate_.set_num_failures_before_success(
        num_failures_before_success);
    render_widget_compositor_.SetUp(expected_successes,
                                    expected_fallback_succeses);
    render_widget_compositor_.setVisible(true);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&RenderWidgetCompositorOutputSurface::SynchronousComposite,
                   base::Unretained(&render_widget_compositor_)));
    base::MessageLoop::current()->Run();
    render_widget_compositor_.AfterTest();
  }

 protected:
  base::MessageLoop ye_olde_message_loope_;
  MockRenderThread render_thread_;
  FakeCompositorDependencies compositor_deps_;
  FakeRenderWidgetCompositorDelegate compositor_delegate_;
  RenderWidgetCompositorOutputSurface render_widget_compositor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetCompositorOutputSurfaceTest);
};

TEST_F(RenderWidgetCompositorOutputSurfaceTest, SucceedOnce) {
  RunTest(false, 0, 1, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, SucceedTwice) {
  RunTest(false, 0, 2, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FailOnceNull) {
  static_assert(
      RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK >= 2,
      "Adjust the values of this test if this fails");
  RunTest(true, 1, 1, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FailOnceBind) {
  static_assert(
      RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK >= 2,
      "Adjust the values of this test if this fails");
  RunTest(false, 1, 1, 0);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FallbackSuccessNull) {
  RunTest(true, RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
          0, 1);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FallbackSuccessBind) {
  RunTest(false, RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
          0, 1);
}

TEST_F(RenderWidgetCompositorOutputSurfaceTest, FallbackSuccessNormalSuccess) {
  // The first success is a fallback, but the next should not be a fallback.
  RunTest(false, RenderWidgetCompositor::OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK,
          1, 1);
}

}  // namespace
}  // namespace content
