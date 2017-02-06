// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/video_frame.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "net/base/filename_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {
namespace {

// Convenience macro: Short-circuit a pass for the tests where platform support
// for forced-compositing mode (or disabled-compositing mode) is lacking.
#define SET_UP_SURFACE_OR_PASS_TEST(wait_message)  \
  if (!SetUpSourceSurface(wait_message)) {  \
    LOG(WARNING)  \
        << ("Blindly passing this test: This platform does not support "  \
            "forced compositing (or forced-disabled compositing) mode.");  \
    return;  \
  }

// Common base class for browser tests.  This is subclassed twice: Once to test
// the browser in forced-compositing mode, and once to test with compositing
// mode disabled.
class RenderWidgetHostViewBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewBrowserTest()
      : frame_size_(400, 300),
        callback_invoke_count_(0),
        frames_captured_(0) {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir_));
  }

  // Attempts to set up the source surface.  Returns false if unsupported on the
  // current platform.
  virtual bool SetUpSourceSurface(const char* wait_message) = 0;

  int callback_invoke_count() const {
    return callback_invoke_count_;
  }

  int frames_captured() const {
    return frames_captured_;
  }

  const gfx::Size& frame_size() const {
    return frame_size_;
  }

  const base::FilePath& test_dir() const {
    return test_dir_;
  }

  RenderViewHost* GetRenderViewHost() const {
    RenderViewHost* const rvh = shell()->web_contents()->GetRenderViewHost();
    CHECK(rvh);
    return rvh;
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    RenderWidgetHostImpl* const rwh = RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderWidgetHostView()->
            GetRenderWidgetHost());
    CHECK(rwh);
    return rwh;
  }

  RenderWidgetHostViewBase* GetRenderWidgetHostView() const {
    return static_cast<RenderWidgetHostViewBase*>(
        GetRenderViewHost()->GetWidget()->GetView());
  }

  // Callback when using CopyFromBackingStore() API.
  void FinishCopyFromBackingStore(const base::Closure& quit_closure,
                                  const SkBitmap& bitmap,
                                  ReadbackResponse response) {
    ++callback_invoke_count_;
    if (response == READBACK_SUCCESS) {
      ++frames_captured_;
      EXPECT_FALSE(bitmap.empty());
    }
    if (!quit_closure.is_null())
      quit_closure.Run();
  }

  // Callback when using CopyFromCompositingSurfaceToVideoFrame() API.
  void FinishCopyFromCompositingSurface(const base::Closure& quit_closure,
                                        const gfx::Rect& region_in_frame,
                                        bool frame_captured) {
    ++callback_invoke_count_;
    if (frame_captured)
      ++frames_captured_;
    if (!quit_closure.is_null())
      quit_closure.Run();
  }

  // Callback when using frame subscriber API.
  void FrameDelivered(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::Closure quit_closure,
      base::TimeTicks timestamp,
      const gfx::Rect& region_in_frame,
      bool frame_captured) {
    ++callback_invoke_count_;
    if (frame_captured)
      ++frames_captured_;
    if (!quit_closure.is_null())
      task_runner->PostTask(FROM_HERE, quit_closure);
  }

  // Copy one frame using the CopyFromBackingStore API.
  void RunBasicCopyFromBackingStoreTest() {
    SET_UP_SURFACE_OR_PASS_TEST(NULL);

    // Repeatedly call CopyFromBackingStore() since, on some platforms (e.g.,
    // Windows), the operation will fail until the first "present" has been
    // made.
    int count_attempts = 0;
    while (true) {
      ++count_attempts;
      base::RunLoop run_loop;
      GetRenderViewHost()->GetWidget()->CopyFromBackingStore(
          gfx::Rect(), frame_size(),
          base::Bind(
              &RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
              base::Unretained(this), run_loop.QuitClosure()),
          kN32_SkColorType);
      run_loop.Run();

      if (frames_captured())
        break;
      else
        GiveItSomeTime();
    }

    EXPECT_EQ(count_attempts, callback_invoke_count());
    EXPECT_EQ(1, frames_captured());
  }

 protected:
  // Waits until the source is available for copying.
  void WaitForCopySourceReady() {
    while (!GetRenderWidgetHostView()->IsSurfaceAvailableForCopy())
      GiveItSomeTime();
  }

  // Run the current message loop for a short time without unwinding the current
  // call stack.
  static void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(10));
    run_loop.Run();
  }

 private:
  const gfx::Size frame_size_;
  base::FilePath test_dir_;
  int callback_invoke_count_;
  int frames_captured_;
};

enum CompositingMode {
  GL_COMPOSITING,
  SOFTWARE_COMPOSITING,
};

class CompositingRenderWidgetHostViewBrowserTest
    : public RenderWidgetHostViewBrowserTest,
      public testing::WithParamInterface<CompositingMode> {
 public:
  CompositingRenderWidgetHostViewBrowserTest()
      : compositing_mode_(GetParam()) {}

  void SetUp() override {
    if (compositing_mode_ == SOFTWARE_COMPOSITING)
      UseSoftwareCompositing();
    RenderWidgetHostViewBrowserTest::SetUp();
  }

  virtual GURL TestUrl() {
    return net::FilePathToFileURL(
        test_dir().AppendASCII("rwhv_compositing_animation.html"));
  }

  bool SetUpSourceSurface(const char* wait_message) override {
    content::DOMMessageQueue message_queue;
    NavigateToURL(shell(), TestUrl());
    if (wait_message != NULL) {
      std::string result(wait_message);
      if (!message_queue.WaitForMessage(&result)) {
        EXPECT_TRUE(false) << "WaitForMessage " << result << " failed.";
        return false;
      }
    }

    // A frame might not be available yet. So, wait for it.
    WaitForCopySourceReady();
    return true;
  }

 private:
  const CompositingMode compositing_mode_;

  DISALLOW_COPY_AND_ASSIGN(CompositingRenderWidgetHostViewBrowserTest);
};

class FakeFrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FakeFrameSubscriber(
    RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback)
      : callback_(callback) {
  }

  bool ShouldCaptureFrame(const gfx::Rect& damage_rect,
                          base::TimeTicks present_time,
                          scoped_refptr<media::VideoFrame>* storage,
                          DeliverFrameCallback* callback) override {
    // Only allow one frame capture to be made.  Otherwise, the compositor could
    // start multiple captures, unbounded, and eventually its own limiter logic
    // will begin invoking |callback| with a |false| result.  This flakes out
    // the unit tests, since they receive a "failed" callback before the later
    // "success" callbacks.
    if (callback_.is_null())
      return false;
    *storage = media::VideoFrame::CreateBlackFrame(gfx::Size(100, 100));
    *callback = callback_;
    callback_.Reset();
    return true;
  }

 private:
  DeliverFrameCallback callback_;
};

// Disable tests for Android as it has an incomplete implementation.
#if !defined(OS_ANDROID)

// The CopyFromBackingStore() API should work on all platforms when compositing
// is enabled.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromBackingStore) {
  RunBasicCopyFromBackingStoreTest();
}

// Tests that the callback passed to CopyFromBackingStore is always called,
// even when the RenderWidgetHost is deleting in the middle of an async copy.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromBackingStore_CallbackDespiteDelete) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);

  base::RunLoop run_loop;
  GetRenderViewHost()->GetWidget()->CopyFromBackingStore(
      gfx::Rect(), frame_size(),
      base::Bind(&RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
                 base::Unretained(this), run_loop.QuitClosure()),
      kN32_SkColorType);
  run_loop.Run();

  EXPECT_EQ(1, callback_invoke_count());
}

// Tests that the callback passed to CopyFromCompositingSurfaceToVideoFrame is
// always called, even when the RenderWidgetHost is deleting in the middle of
// an async copy.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       CopyFromCompositingSurface_CallbackDespiteDelete) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  RenderWidgetHostViewBase* const view = GetRenderWidgetHostView();
  if (!view->CanCopyToVideoFrame()) {
    LOG(WARNING) <<
        ("Blindly passing this test: CopyFromCompositingSurfaceToVideoFrame() "
         "not supported on this platform.");
    return;
  }

  base::RunLoop run_loop;
  scoped_refptr<media::VideoFrame> dest =
      media::VideoFrame::CreateBlackFrame(frame_size());
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()), dest, base::Bind(
          &RenderWidgetHostViewBrowserTest::FinishCopyFromCompositingSurface,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(1, callback_invoke_count());
}

// Test basic frame subscription functionality.  We subscribe, and then run
// until at least one DeliverFrameCallback has been invoked.
// https://crbug.com/542896
#if defined(MEMORY_SANITIZER)
#define MAYBE_FrameSubscriberTest DISABLED_FrameSubscriberTest
#else
#define MAYBE_FrameSubscriberTest FrameSubscriberTest
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       MAYBE_FrameSubscriberTest) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  RenderWidgetHostViewBase* const view = GetRenderWidgetHostView();

  base::RunLoop run_loop;
  std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
      new FakeFrameSubscriber(base::Bind(
          &RenderWidgetHostViewBrowserTest::FrameDelivered,
          base::Unretained(this), base::ThreadTaskRunnerHandle::Get(),
          run_loop.QuitClosure())));
  view->BeginFrameSubscription(std::move(subscriber));
  run_loop.Run();
  view->EndFrameSubscription();

  EXPECT_LE(1, callback_invoke_count());
  EXPECT_LE(1, frames_captured());
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest, CopyTwice) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  RenderWidgetHostViewBase* const view = GetRenderWidgetHostView();
  if (!view->CanCopyToVideoFrame()) {
    LOG(WARNING) << ("Blindly passing this test: "
                     "CopyFromCompositingSurfaceToVideoFrame() not supported "
                     "on this platform.");
    return;
  }

  base::RunLoop run_loop;
  scoped_refptr<media::VideoFrame> first_output =
      media::VideoFrame::CreateBlackFrame(frame_size());
  ASSERT_TRUE(first_output.get());
  scoped_refptr<media::VideoFrame> second_output =
      media::VideoFrame::CreateBlackFrame(frame_size());
  ASSERT_TRUE(second_output.get());
  base::Closure closure = base::BarrierClosure(2, run_loop.QuitClosure());
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()), first_output,
      base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                 base::Unretained(this), base::ThreadTaskRunnerHandle::Get(),
                 closure, base::TimeTicks::Now()));
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()), second_output,
      base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                 base::Unretained(this), base::ThreadTaskRunnerHandle::Get(),
                 closure, base::TimeTicks::Now()));
  run_loop.Run();

  EXPECT_EQ(2, callback_invoke_count());
  EXPECT_EQ(2, frames_captured());
}

class CompositingRenderWidgetHostViewBrowserTestTabCapture
    : public CompositingRenderWidgetHostViewBrowserTest {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCapture()
      : readback_response_(READBACK_NO_RESPONSE),
        allowable_error_(0),
        test_url_("data:text/html,<!doctype html>") {}

  void SetUp() override {
    EnablePixelOutput();
    CompositingRenderWidgetHostViewBrowserTest::SetUp();
  }

  void ReadbackRequestCallbackTest(base::Closure quit_callback,
                                   const SkBitmap& bitmap,
                                   ReadbackResponse response) {
    readback_response_ = response;
    if (response != READBACK_SUCCESS) {
      quit_callback.Run();
      return;
    }

    SkAutoLockPixels bitmap_lock(bitmap);

    // Check that the |bitmap| contains cyan and/or yellow pixels.  This is
    // needed because the compositor will read back "blank" frames until the
    // first frame from the renderer is composited.  See comments in
    // PerformTestWithLeftRightRects() for more details about eliminating test
    // flakiness.
    bool contains_a_test_color = false;
    for (int i = 0; i < bitmap.width(); ++i) {
      for (int j = 0; j < bitmap.height(); ++j) {
        if (!exclude_rect_.IsEmpty() && exclude_rect_.Contains(i, j))
          continue;

        const unsigned high_threshold = 0xff - allowable_error_;
        const unsigned low_threshold = 0x00 + allowable_error_;
        const SkColor color = bitmap.getColor(i, j);
        const bool is_cyan = SkColorGetR(color) <= low_threshold &&
                             SkColorGetG(color) >= high_threshold &&
                             SkColorGetB(color) >= high_threshold;
        const bool is_yellow = SkColorGetR(color) >= high_threshold &&
                               SkColorGetG(color) >= high_threshold &&
                               SkColorGetB(color) <= low_threshold;
        if (is_cyan || is_yellow) {
          contains_a_test_color = true;
          break;
        }
      }
    }
    if (!contains_a_test_color) {
      readback_response_ = READBACK_NO_TEST_COLORS;
      quit_callback.Run();
      return;
    }

    // Compare the readback |bitmap| to the |expected_bitmap|, pixel-by-pixel.
    const SkBitmap& expected_bitmap =
        expected_copy_from_compositing_surface_bitmap_;
    EXPECT_EQ(expected_bitmap.width(), bitmap.width());
    EXPECT_EQ(expected_bitmap.height(), bitmap.height());
    EXPECT_EQ(expected_bitmap.colorType(), bitmap.colorType());
    SkAutoLockPixels expected_bitmap_lock(expected_bitmap);
    int fails = 0;
    for (int i = 0; i < bitmap.width() && fails < 10; ++i) {
      for (int j = 0; j < bitmap.height() && fails < 10; ++j) {
        if (!exclude_rect_.IsEmpty() && exclude_rect_.Contains(i, j))
          continue;

        SkColor expected_color = expected_bitmap.getColor(i, j);
        SkColor color = bitmap.getColor(i, j);
        int expected_alpha = SkColorGetA(expected_color);
        int alpha = SkColorGetA(color);
        int expected_red = SkColorGetR(expected_color);
        int red = SkColorGetR(color);
        int expected_green = SkColorGetG(expected_color);
        int green = SkColorGetG(color);
        int expected_blue = SkColorGetB(expected_color);
        int blue = SkColorGetB(color);
        EXPECT_NEAR(expected_alpha, alpha, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_red, red, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_green, green, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
        EXPECT_NEAR(expected_blue, blue, allowable_error_)
            << "expected_color: " << std::hex << expected_color
            << " color: " <<  color
            << " Failed at " << std::dec << i << ", " << j
            << " Failure " << ++fails;
      }
    }
    EXPECT_LT(fails, 10);

    quit_callback.Run();
  }

  void ReadbackRequestCallbackForVideo(
      scoped_refptr<media::VideoFrame> video_frame,
      base::Closure quit_callback,
      const gfx::Rect& region_in_frame,
      bool result) {
    if (!result) {
      readback_response_ = READBACK_TO_VIDEO_FRAME_FAILED;
      quit_callback.Run();
      return;
    }

    media::SkCanvasVideoRenderer video_renderer;

    SkBitmap bitmap;
    bitmap.allocN32Pixels(video_frame->visible_rect().width(),
                          video_frame->visible_rect().height());
    // Don't clear the canvas because drawing a video frame by Src mode.
    SkCanvas canvas(bitmap);
    video_renderer.Copy(video_frame, &canvas, media::Context3D());

    ReadbackRequestCallbackTest(quit_callback, bitmap, READBACK_SUCCESS);
  }

  void SetAllowableError(int amount) { allowable_error_ = amount; }
  void SetExcludeRect(gfx::Rect exclude) { exclude_rect_ = exclude; }

  GURL TestUrl() override { return GURL(test_url_); }

  void SetTestUrl(const std::string& url) { test_url_ = url; }

  // Loads a page two boxes side-by-side, each half the width of
  // |html_rect_size|, and with different background colors. The test then
  // copies from |copy_rect| region of the page into a bitmap of size
  // |output_size|, and examines the resulting bitmap/VideoFrame.
  // Note that |output_size| may not have the same size as |copy_rect| (e.g.
  // when the output is scaled).
  void PerformTestWithLeftRightRects(const gfx::Size& html_rect_size,
                                     const gfx::Rect& copy_rect,
                                     const gfx::Size& output_size,
                                     bool video_frame) {
    const gfx::Size box_size(html_rect_size.width() / 2,
                             html_rect_size.height());
    SetTestUrl(base::StringPrintf(
        "data:text/html,<!doctype html>"
        "<div class='left'>"
        "  <div class='right'></div>"
        "</div>"
        "<style>"
        "body { padding: 0; margin: 0; }"
        ".left { position: absolute;"
        "        background: #0ff;"
        "        width: %dpx;"
        "        height: %dpx;"
        "}"
        ".right { position: absolute;"
        "         left: %dpx;"
        "         background: #ff0;"
        "         width: %dpx;"
        "         height: %dpx;"
        "}"
        "</style>"
        "<script>"
        "  domAutomationController.setAutomationId(0);"
        "  domAutomationController.send(\"DONE\");"
        "</script>",
        box_size.width(),
        box_size.height(),
        box_size.width(),
        box_size.width(),
        box_size.height()));

    SET_UP_SURFACE_OR_PASS_TEST("\"DONE\"");
    if (!ShouldContinueAfterTestURLLoad())
      return;

    RenderWidgetHostViewBase* rwhv = GetRenderWidgetHostView();
    ASSERT_TRUE(!video_frame || rwhv->CanCopyToVideoFrame());

    SetupLeftRightBitmap(output_size,
                         &expected_copy_from_compositing_surface_bitmap_);

    // The page is loaded in the renderer.  Request frames from the renderer
    // until readback succeeds.  When readback succeeds, the resulting
    // SkBitmap/VideoFrame is examined to ensure it matches the expected result.
    // This loop is needed because:
    //   1. Painting/Compositing is not synchronous with the Javascript engine,
    //      and so the "DONE" signal above could be received before the renderer
    //      provides a frame with the expected content.  http://crbug.com/405282
    //   2. Avoiding test flakiness: On some platforms, the readback operation
    //      is allowed to transiently fail.  The purpose of these tests is to
    //      confirm correct cropping/scaling behavior; and not that every
    //      readback must succeed.  http://crbug.com/444237
    uint32_t last_frame_number = 0;
    do {
      // Wait for renderer to provide the next frame.
      while (!GetRenderWidgetHost()->ScheduleComposite())
        GiveItSomeTime();
      while (rwhv->RendererFrameNumber() == last_frame_number)
        GiveItSomeTime();
      last_frame_number = rwhv->RendererFrameNumber();

      // Request readback.  The callbacks will examine the pixels in the
      // SkBitmap/VideoFrame result if readback was successful.
      readback_response_ = READBACK_NO_RESPONSE;
      base::RunLoop run_loop;
      if (video_frame) {
        // Allow pixel differences as long as we have the right idea.
        SetAllowableError(0x10);
        // Exclude the middle two columns which are blended between the two
        // sides.
        SetExcludeRect(
            gfx::Rect(output_size.width() / 2 - 1, 0, 2, output_size.height()));

        scoped_refptr<media::VideoFrame> video_frame =
            media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_YV12,
                                           output_size, gfx::Rect(output_size),
                                           output_size, base::TimeDelta());

        base::Callback<void(const gfx::Rect& rect, bool success)> callback =
            base::Bind(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                           ReadbackRequestCallbackForVideo,
                       base::Unretained(this), video_frame,
                       run_loop.QuitClosure());
        rwhv->CopyFromCompositingSurfaceToVideoFrame(
            copy_rect, video_frame, callback);
      } else {
        if (!content::GpuDataManager::GetInstance()
                 ->CanUseGpuBrowserCompositor()) {
          // Skia rendering can cause color differences, particularly in the
          // middle two columns.
          SetAllowableError(2);
          SetExcludeRect(gfx::Rect(output_size.width() / 2 - 1, 0, 2,
                                   output_size.height()));
        }

        const ReadbackRequestCallback callback =
            base::Bind(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                           ReadbackRequestCallbackTest,
                       base::Unretained(this),
                       run_loop.QuitClosure());
        rwhv->CopyFromCompositingSurface(
            copy_rect, output_size, callback, kN32_SkColorType);
      }
      run_loop.Run();

      // If the readback operation did not provide a frame, log the reason
      // to aid in future debugging.  This information will also help determine
      // whether the implementation is broken, or a test bot is in a bad state.
      #define CASE_LOG_READBACK_WARNING(enum_value) \
        case enum_value: \
          LOG(WARNING) << "Readback attempt failed (render frame #" \
                       << last_frame_number << ").  Reason: " #enum_value; \
          break
      switch (readback_response_) {
        case READBACK_SUCCESS:
          break;
        CASE_LOG_READBACK_WARNING(READBACK_FAILED);
        CASE_LOG_READBACK_WARNING(READBACK_SURFACE_UNAVAILABLE);
        CASE_LOG_READBACK_WARNING(READBACK_BITMAP_ALLOCATION_FAILURE);
        CASE_LOG_READBACK_WARNING(READBACK_NO_TEST_COLORS);
        CASE_LOG_READBACK_WARNING(READBACK_TO_VIDEO_FRAME_FAILED);
        default:
          LOG(ERROR)
              << "Invalid readback response value: " << readback_response_;
          NOTREACHED();
      }
    } while (readback_response_ != READBACK_SUCCESS);
  }

  // Sets up |bitmap| to have size |copy_size|. It floods the left half with
  // #0ff and the right half with #ff0.
  void SetupLeftRightBitmap(const gfx::Size& copy_size, SkBitmap* bitmap) {
    bitmap->allocN32Pixels(copy_size.width(), copy_size.height());
    // Left half is #0ff.
    bitmap->eraseARGB(255, 0, 255, 255);
    // Right half is #ff0.
    {
      SkAutoLockPixels lock(*bitmap);
      for (int i = 0; i < copy_size.width() / 2; ++i) {
        for (int j = 0; j < copy_size.height(); ++j) {
          *(bitmap->getAddr32(copy_size.width() / 2 + i, j)) =
              SkColorSetARGB(255, 255, 255, 0);
        }
      }
    }
  }

 protected:
  // Additional ReadbackResponse enum values only used within this test module,
  // to distinguish readback exception cases further.
  enum ExtraReadbackResponsesForTest {
    READBACK_NO_RESPONSE = -1337,
    READBACK_NO_TEST_COLORS,
    READBACK_TO_VIDEO_FRAME_FAILED,
  };

  virtual bool ShouldContinueAfterTestURLLoad() {
    return true;
  }

 private:
  // |readback_response_| is always a content::ReadbackResponse or
  // ExtraReadbackResponsesForTest enum value.
  int readback_response_;
  SkBitmap expected_copy_from_compositing_surface_bitmap_;
  int allowable_error_;
  gfx::Rect exclude_rect_;
  std::string test_url_;
};

// https://crbug.com/542896
#if defined(MEMORY_SANITIZER)
#define MAYBE_CopyFromCompositingSurface_Origin_Unscaled \
  DISABLED_CopyFromCompositingSurface_Origin_Unscaled
#else
#define MAYBE_CopyFromCompositingSurface_Origin_Unscaled \
  CopyFromCompositingSurface_Origin_Unscaled
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       MAYBE_CopyFromCompositingSurface_Origin_Unscaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size = copy_rect.size();
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

// https://crbug.com/542896
#if defined(MEMORY_SANITIZER)
#define MAYBE_CopyFromCompositingSurface_Origin_Scaled \
  DISABLED_FrameSubscriberTestCopyFromCompositingSurface_Origin_Scaled
#else
#define MAYBE_CopyFromCompositingSurface_Origin_Scaled \
  CopyFromCompositingSurface_Origin_Scaled
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       MAYBE_CopyFromCompositingSurface_Origin_Scaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size(200, 100);
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

// https://crbug.com/542896
#if defined(MEMORY_SANITIZER)
#define MAYBE_CopyFromCompositingSurface_Cropped_Unscaled \
  DISABLED_CopyFromCompositingSurface_Cropped_Unscaled
#else
#define MAYBE_CopyFromCompositingSurface_Cropped_Unscaled \
  CopyFromCompositingSurface_Cropped_Unscaled
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       MAYBE_CopyFromCompositingSurface_Cropped_Unscaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

// https://crbug.com/542896
#if defined(MEMORY_SANITIZER)
#define MAYBE_CopyFromCompositingSurface_Cropped_Scaled \
  DISABLED_CopyFromCompositingSurface_Cropped_Scaled
#else
#define MAYBE_CopyFromCompositingSurface_Cropped_Scaled \
  CopyFromCompositingSurface_Cropped_Scaled
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       MAYBE_CopyFromCompositingSurface_Cropped_Scaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size(20, 10);
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

// https://crbug.com/542896
#if defined(MEMORY_SANITIZER)
#define MAYBE_CopyFromCompositingSurface_ForVideoFrame \
  DISABLED_CopyFromCompositingSurface_ForVideoFrame
#else
#define MAYBE_CopyFromCompositingSurface_ForVideoFrame \
  CopyFromCompositingSurface_ForVideoFrame
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       MAYBE_CopyFromCompositingSurface_ForVideoFrame) {
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(45, 30),
                        gfx::Size(90, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size html_rect_size(400, 300);
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

// https://crbug.com/542896
#if defined(MEMORY_SANITIZER)
#define MAYBE_CopyFromCompositingSurface_ForVideoFrame_Scaled \
  DISABLED_CopyFromCompositingSurface_ForVideoFrame_Scaled
#else
#define MAYBE_CopyFromCompositingSurface_ForVideoFrame_Scaled \
  CopyFromCompositingSurface_ForVideoFrame_Scaled
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       MAYBE_CopyFromCompositingSurface_ForVideoFrame_Scaled) {
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(45, 30),
                        gfx::Size(90, 60));
  // Scale to 30 x 20 (preserve aspect ratio).
  gfx::Size output_size(30, 20);
  gfx::Size html_rect_size(400, 300);
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

class CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI
    : public CompositingRenderWidgetHostViewBrowserTestTabCapture {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI() {}

 protected:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    CompositingRenderWidgetHostViewBrowserTestTabCapture::SetUpCommandLine(cmd);
    cmd->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                           base::StringPrintf("%f", scale()));
  }

  bool ShouldContinueAfterTestURLLoad() override {
    // Short-circuit a pass for platforms where setting up high-DPI fails.
    const float actual_scale_factor =
        GetScaleFactorForView(GetRenderWidgetHostView());
    if (actual_scale_factor != scale()) {
      LOG(WARNING) << "Blindly passing this test; unable to force device scale "
                   << "factor: seems to be " << actual_scale_factor
                   << " but expected " << scale();
      return false;
    }
    VLOG(1) << ("Successfully forced device scale factor.  Moving forward with "
                "this test!  :-)");
    return true;
  }

  static float scale() { return 2.0f; }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI);
};

// NineImagePainter implementation crashes the process on Windows when this
// content_browsertest forces a device scale factor.  http://crbug.com/399349
// Disabled under MSAN.  http://crbug.com/542896
#if defined(OS_WIN) || defined(MEMORY_SANITIZER)
#define MAYBE_CopyToBitmap_EntireRegion DISABLED_CopyToBitmap_EntireRegion
#define MAYBE_CopyToBitmap_CenterRegion DISABLED_CopyToBitmap_CenterRegion
#define MAYBE_CopyToBitmap_ScaledResult DISABLED_CopyToBitmap_ScaledResult
#define MAYBE_CopyToVideoFrame_EntireRegion \
            DISABLED_CopyToVideoFrame_EntireRegion
#define MAYBE_CopyToVideoFrame_CenterRegion \
            DISABLED_CopyToVideoFrame_CenterRegion
#define MAYBE_CopyToVideoFrame_ScaledResult \
            DISABLED_CopyToVideoFrame_ScaledResult
#else
#define MAYBE_CopyToBitmap_EntireRegion CopyToBitmap_EntireRegion
#define MAYBE_CopyToBitmap_CenterRegion CopyToBitmap_CenterRegion
#define MAYBE_CopyToBitmap_ScaledResult CopyToBitmap_ScaledResult
#define MAYBE_CopyToVideoFrame_EntireRegion CopyToVideoFrame_EntireRegion
#define MAYBE_CopyToVideoFrame_CenterRegion CopyToVideoFrame_CenterRegion
#define MAYBE_CopyToVideoFrame_ScaledResult CopyToVideoFrame_ScaledResult
#endif

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_EntireRegion) {
  gfx::Size html_rect_size(200, 150);
  gfx::Rect copy_rect(200, 150);
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_CenterRegion) {
  gfx::Size html_rect_size(200, 150);
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect =
      gfx::Rect(gfx::Rect(html_rect_size).CenterPoint() - gfx::Vector2d(45, 30),
                gfx::Size(90, 60));
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToBitmap_ScaledResult) {
  gfx::Size html_rect_size(200, 100);
  gfx::Rect copy_rect(200, 100);
  // Output is being down-scaled since output_size is in phyiscal pixels.
  gfx::Size output_size(200, 100);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToVideoFrame_EntireRegion) {
  gfx::Size html_rect_size(200, 150);
  gfx::Rect copy_rect(200, 150);
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToVideoFrame_CenterRegion) {
  gfx::Size html_rect_size(200, 150);
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect =
      gfx::Rect(gfx::Rect(html_rect_size).CenterPoint() - gfx::Vector2d(45, 30),
                gfx::Size(90, 60));
  // Scale the output size so that, internally, scaling is not occurring.
  gfx::Size output_size = gfx::ScaleToRoundedSize(copy_rect.size(), scale());
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    MAYBE_CopyToVideoFrame_ScaledResult) {
  gfx::Size html_rect_size(200, 100);
  gfx::Rect copy_rect(200, 100);
  // Output is being down-scaled since output_size is in phyiscal pixels.
  gfx::Size output_size(200, 100);
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                video_frame);
}

#if defined(OS_CHROMEOS)
// On ChromeOS there is no software compositing.
static const auto kTestCompositingModes = testing::Values(GL_COMPOSITING);
#else
static const auto kTestCompositingModes =
    testing::Values(GL_COMPOSITING, SOFTWARE_COMPOSITING);
#endif

INSTANTIATE_TEST_CASE_P(GLAndSoftwareCompositing,
                        CompositingRenderWidgetHostViewBrowserTest,
                        kTestCompositingModes);
INSTANTIATE_TEST_CASE_P(GLAndSoftwareCompositing,
                        CompositingRenderWidgetHostViewBrowserTestTabCapture,
                        kTestCompositingModes);
INSTANTIATE_TEST_CASE_P(
    GLAndSoftwareCompositing,
    CompositingRenderWidgetHostViewBrowserTestTabCaptureHighDPI,
    kTestCompositingModes);

#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace content
