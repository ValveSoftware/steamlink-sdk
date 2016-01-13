// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/run_loop.h"
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
#include "media/filters/skcanvas_video_renderer.h"
#include "net/base/filename_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/gfx/win/dpi.h"
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

  virtual void SetUpOnMainThread() OVERRIDE {
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
        GetRenderViewHost()->GetView());
  }

  // Callback when using CopyFromBackingStore() API.
  void FinishCopyFromBackingStore(const base::Closure& quit_closure,
                                  bool frame_captured,
                                  const SkBitmap& bitmap) {
    ++callback_invoke_count_;
    if (frame_captured) {
      ++frames_captured_;
      EXPECT_FALSE(bitmap.empty());
    }
    if (!quit_closure.is_null())
      quit_closure.Run();
  }

  // Callback when using CopyFromCompositingSurfaceToVideoFrame() API.
  void FinishCopyFromCompositingSurface(const base::Closure& quit_closure,
                                        bool frame_captured) {
    ++callback_invoke_count_;
    if (frame_captured)
      ++frames_captured_;
    if (!quit_closure.is_null())
      quit_closure.Run();
  }

  // Callback when using frame subscriber API.
  void FrameDelivered(const scoped_refptr<base::MessageLoopProxy>& loop,
                      base::Closure quit_closure,
                      base::TimeTicks timestamp,
                      bool frame_captured) {
    ++callback_invoke_count_;
    if (frame_captured)
      ++frames_captured_;
    if (!quit_closure.is_null())
      loop->PostTask(FROM_HERE, quit_closure);
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
      GetRenderViewHost()->CopyFromBackingStore(
          gfx::Rect(),
          frame_size(),
          base::Bind(
              &RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
              base::Unretained(this),
              run_loop.QuitClosure()),
          SkBitmap::kARGB_8888_Config);
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
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        run_loop.QuitClosure(),
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
  explicit CompositingRenderWidgetHostViewBrowserTest()
      : compositing_mode_(GetParam()) {}

  virtual void SetUp() OVERRIDE {
    if (compositing_mode_ == SOFTWARE_COMPOSITING)
      UseSoftwareCompositing();
    RenderWidgetHostViewBrowserTest::SetUp();
  }

  virtual GURL TestUrl() {
    return net::FilePathToFileURL(
        test_dir().AppendASCII("rwhv_compositing_animation.html"));
  }

  virtual bool SetUpSourceSurface(const char* wait_message) OVERRIDE {
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

  virtual bool ShouldCaptureFrame(base::TimeTicks present_time,
                                  scoped_refptr<media::VideoFrame>* storage,
                                  DeliverFrameCallback* callback) OVERRIDE {
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

// Disable tests for Android and IOS as these platforms have incomplete
// implementation.
#if !defined(OS_ANDROID) && !defined(OS_IOS)

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
  GetRenderViewHost()->CopyFromBackingStore(
      gfx::Rect(),
      frame_size(),
      base::Bind(&RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
                 base::Unretained(this),
                 run_loop.QuitClosure()),
      SkBitmap::kARGB_8888_Config);
  // Delete the surface before the callback is run.
  GetRenderWidgetHostView()->AcceleratedSurfaceRelease();
  run_loop.Run();

  EXPECT_EQ(1, callback_invoke_count());
}

// Tests that the callback passed to CopyFromCompositingSurfaceToVideoFrame is
// always called, even when the RenderWidgetHost is deleting in the middle of
// an async copy.
//
// Test is flaky on Win. http://crbug.com/276783
#if defined(OS_WIN) || (defined(OS_CHROMEOS) && !defined(NDEBUG))
#define MAYBE_CopyFromCompositingSurface_CallbackDespiteDelete \
  DISABLED_CopyFromCompositingSurface_CallbackDespiteDelete
#else
#define MAYBE_CopyFromCompositingSurface_CallbackDespiteDelete \
  CopyFromCompositingSurface_CallbackDespiteDelete
#endif
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       MAYBE_CopyFromCompositingSurface_CallbackDespiteDelete) {
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
  // Delete the surface before the callback is run.
  view->AcceleratedSurfaceRelease();
  run_loop.Run();

  EXPECT_EQ(1, callback_invoke_count());
}

// Test basic frame subscription functionality.  We subscribe, and then run
// until at least one DeliverFrameCallback has been invoked.
IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTest,
                       FrameSubscriberTest) {
  SET_UP_SURFACE_OR_PASS_TEST(NULL);
  RenderWidgetHostViewBase* const view = GetRenderWidgetHostView();
  if (!view->CanSubscribeFrame()) {
    LOG(WARNING) << ("Blindly passing this test: Frame subscription not "
                     "supported on this platform.");
    return;
  }

  base::RunLoop run_loop;
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
      new FakeFrameSubscriber(
          base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                     base::Unretained(this),
                     base::MessageLoopProxy::current(),
                     run_loop.QuitClosure())));
  view->BeginFrameSubscription(subscriber.Pass());
  run_loop.Run();
  view->EndFrameSubscription();

  EXPECT_LE(1, callback_invoke_count());
  EXPECT_LE(1, frames_captured());
}

// Test that we can copy twice from an accelerated composited page.
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
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()),
      first_output,
      base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                 base::Unretained(this),
                 base::MessageLoopProxy::current(),
                 base::Closure(),
                 base::TimeTicks::Now()));
  view->CopyFromCompositingSurfaceToVideoFrame(
      gfx::Rect(view->GetViewBounds().size()),
      second_output,
      base::Bind(&RenderWidgetHostViewBrowserTest::FrameDelivered,
                 base::Unretained(this),
                 base::MessageLoopProxy::current(),
                 run_loop.QuitClosure(),
                 base::TimeTicks::Now()));
  run_loop.Run();

  EXPECT_EQ(2, callback_invoke_count());
  EXPECT_EQ(2, frames_captured());
}

class CompositingRenderWidgetHostViewBrowserTestTabCapture
    : public CompositingRenderWidgetHostViewBrowserTest {
 public:
  CompositingRenderWidgetHostViewBrowserTestTabCapture()
      : expected_copy_from_compositing_surface_result_(false),
        allowable_error_(0),
        test_url_("data:text/html,<!doctype html>") {}

  virtual void SetUp() OVERRIDE {
    EnablePixelOutput();
    CompositingRenderWidgetHostViewBrowserTest::SetUp();
  }

  void CopyFromCompositingSurfaceCallback(base::Closure quit_callback,
                                          bool result,
                                          const SkBitmap& bitmap) {
    EXPECT_EQ(expected_copy_from_compositing_surface_result_, result);
    if (!result) {
      quit_callback.Run();
      return;
    }

    const SkBitmap& expected_bitmap =
        expected_copy_from_compositing_surface_bitmap_;
    EXPECT_EQ(expected_bitmap.width(), bitmap.width());
    EXPECT_EQ(expected_bitmap.height(), bitmap.height());
    EXPECT_EQ(expected_bitmap.config(), bitmap.config());
    SkAutoLockPixels expected_bitmap_lock(expected_bitmap);
    SkAutoLockPixels bitmap_lock(bitmap);
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

  void CopyFromCompositingSurfaceCallbackForVideo(
      scoped_refptr<media::VideoFrame> video_frame,
      base::Closure quit_callback,
      bool result) {
    EXPECT_EQ(expected_copy_from_compositing_surface_result_, result);
    if (!result) {
      quit_callback.Run();
      return;
    }

    media::SkCanvasVideoRenderer video_renderer;

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(video_frame->visible_rect().width(),
                                         video_frame->visible_rect().height(),
                                         kPMColor_SkColorType,
                                         kPremul_SkAlphaType));
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);

    video_renderer.Paint(video_frame.get(),
                         &canvas,
                         video_frame->visible_rect(),
                         0xff);

    CopyFromCompositingSurfaceCallback(quit_callback,
                                       result,
                                       bitmap);
  }

  void SetExpectedCopyFromCompositingSurfaceResult(bool result,
                                                   const SkBitmap& bitmap) {
    expected_copy_from_compositing_surface_result_ = result;
    expected_copy_from_compositing_surface_bitmap_ = bitmap;
  }

  void SetAllowableError(int amount) { allowable_error_ = amount; }
  void SetExcludeRect(gfx::Rect exclude) { exclude_rect_ = exclude; }

  virtual GURL TestUrl() OVERRIDE {
    return GURL(test_url_);
  }

  void SetTestUrl(std::string url) { test_url_ = url; }

  // Loads a page two boxes side-by-side, each half the width of
  // |html_rect_size|, and with different background colors. The test then
  // copies from |copy_rect| region of the page into a bitmap of size
  // |output_size|, and compares that with a bitmap of size
  // |expected_bitmap_size|.
  // Note that |output_size| may not have the same size as |copy_rect| (e.g.
  // when the output is scaled). Also note that |expected_bitmap_size| may not
  // be the same as |output_size| (e.g. when the device scale factor is not 1).
  void PerformTestWithLeftRightRects(const gfx::Size& html_rect_size,
                                     const gfx::Rect& copy_rect,
                                     const gfx::Size& output_size,
                                     const gfx::Size& expected_bitmap_size,
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

    RenderWidgetHostViewBase* rwhvp = GetRenderWidgetHostView();
    if (video_frame && !rwhvp->CanCopyToVideoFrame()) {
      // This should only happen on Mac when using the software compositor.
      // Otherwise, raise an error. This can be removed when Mac is moved to a
      // browser compositor.
      // http://crbug.com/314190
#if defined(OS_MACOSX)
      if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(NULL)) {
        LOG(WARNING) << ("Blindly passing this test because copying to "
                         "video frames is not supported on this platform.");
        return;
      }
#endif
      NOTREACHED();
    }

    // The page is loaded in the renderer, wait for a new frame to arrive.
    uint32 frame = rwhvp->RendererFrameNumber();
    while (!GetRenderWidgetHost()->ScheduleComposite())
      GiveItSomeTime();
    while (rwhvp->RendererFrameNumber() == frame)
      GiveItSomeTime();

    SkBitmap expected_bitmap;
    SetupLeftRightBitmap(expected_bitmap_size, &expected_bitmap);
    SetExpectedCopyFromCompositingSurfaceResult(true, expected_bitmap);

    base::RunLoop run_loop;
    if (video_frame) {
      // Allow pixel differences as long as we have the right idea.
      SetAllowableError(0x10);
      // Exclude the middle two columns which are blended between the two sides.
      SetExcludeRect(
          gfx::Rect(output_size.width() / 2 - 1, 0, 2, output_size.height()));

      scoped_refptr<media::VideoFrame> video_frame =
          media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                         expected_bitmap_size,
                                         gfx::Rect(expected_bitmap_size),
                                         expected_bitmap_size,
                                         base::TimeDelta());

      base::Callback<void(bool success)> callback =
          base::Bind(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                         CopyFromCompositingSurfaceCallbackForVideo,
                     base::Unretained(this),
                     video_frame,
                     run_loop.QuitClosure());
      rwhvp->CopyFromCompositingSurfaceToVideoFrame(copy_rect,
                                                    video_frame,
                                                    callback);
    } else {
#if defined(USE_AURA)
      if (!content::GpuDataManager::GetInstance()
               ->CanUseGpuBrowserCompositor()) {
        // Skia rendering can cause color differences, particularly in the
        // middle two columns.
        SetAllowableError(2);
        SetExcludeRect(
            gfx::Rect(output_size.width() / 2 - 1, 0, 2, output_size.height()));
      }
#endif

      base::Callback<void(bool, const SkBitmap&)> callback =
          base::Bind(&CompositingRenderWidgetHostViewBrowserTestTabCapture::
                       CopyFromCompositingSurfaceCallback,
                   base::Unretained(this),
                   run_loop.QuitClosure());
      rwhvp->CopyFromCompositingSurface(copy_rect,
                                        output_size,
                                        callback,
                                        SkBitmap::kARGB_8888_Config);
    }
    run_loop.Run();
  }

  // Sets up |bitmap| to have size |copy_size|. It floods the left half with
  // #0ff and the right half with #ff0.
  void SetupLeftRightBitmap(const gfx::Size& copy_size, SkBitmap* bitmap) {
    bitmap->setConfig(
        SkBitmap::kARGB_8888_Config, copy_size.width(), copy_size.height());
    bitmap->allocPixels();
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
  virtual bool ShouldContinueAfterTestURLLoad() {
    return true;
  }

 private:
  bool expected_copy_from_compositing_surface_result_;
  SkBitmap expected_copy_from_compositing_surface_bitmap_;
  int allowable_error_;
  gfx::Rect exclude_rect_;
  std::string test_url_;
};

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Origin_Unscaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Origin_Scaled) {
  gfx::Rect copy_rect(400, 300);
  gfx::Size output_size(200, 100);
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Cropped_Unscaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_Cropped_Scaled) {
  // Grab 60x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(30, 30),
                        gfx::Size(60, 60));
  gfx::Size output_size(20, 10);
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_ForVideoFrame) {
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(45, 30),
                        gfx::Size(90, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewBrowserTestTabCapture,
                       CopyFromCompositingSurface_ForVideoFrame_Scaled) {
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect(400, 300);
  copy_rect = gfx::Rect(copy_rect.CenterPoint() - gfx::Vector2d(45, 30),
                        gfx::Size(90, 60));
  // Scale to 30 x 20 (preserve aspect ratio).
  gfx::Size output_size(30, 20);
  gfx::Size expected_bitmap_size = output_size;
  gfx::Size html_rect_size(400, 300);
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

class CompositingRenderWidgetHostViewTabCaptureHighDPI
    : public CompositingRenderWidgetHostViewBrowserTestTabCapture {
 public:
  CompositingRenderWidgetHostViewTabCaptureHighDPI() : kScale(2.f) {}

  virtual void SetUpOnMainThread() OVERRIDE {
    CommandLine* cmd = CommandLine::ForCurrentProcess();
    cmd->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                           base::StringPrintf("%f", scale()));
#if defined(OS_WIN)
    gfx::ForceHighDPISupportForTesting(scale());
    gfx::EnableHighDPISupport();
#endif
  }

  float scale() const { return kScale; }

 private:
  virtual bool ShouldContinueAfterTestURLLoad() OVERRIDE {
    // Short-circuit a pass for platforms where setting up high-DPI fails.
    if (ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactor(
            GetScaleFactorForView(GetRenderWidgetHostView()))) != scale()) {
      LOG(WARNING) << "Blindly passing this test: failed to set up "
          "scale factor: " << scale();
      return false;
    }
    return true;
  }

  const float kScale;

  DISALLOW_COPY_AND_ASSIGN(CompositingRenderWidgetHostViewTabCaptureHighDPI);
};

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewTabCaptureHighDPI,
                       CopyFromCompositingSurface) {
  gfx::Rect copy_rect(200, 150);
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size =
      gfx::ToFlooredSize(gfx::ScaleSize(output_size, scale(), scale()));
  gfx::Size html_rect_size(200, 150);
  bool video_frame = false;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

IN_PROC_BROWSER_TEST_P(CompositingRenderWidgetHostViewTabCaptureHighDPI,
                       CopyFromCompositingSurfaceVideoFrame) {
  gfx::Size html_rect_size(200, 150);
  // Grab 90x60 pixels from the center of the tab contents.
  gfx::Rect copy_rect =
      gfx::Rect(gfx::Rect(html_rect_size).CenterPoint() - gfx::Vector2d(45, 30),
                gfx::Size(90, 60));
  gfx::Size output_size = copy_rect.size();
  gfx::Size expected_bitmap_size =
      gfx::ToFlooredSize(gfx::ScaleSize(output_size, scale(), scale()));
  bool video_frame = true;
  PerformTestWithLeftRightRects(html_rect_size,
                                copy_rect,
                                output_size,
                                expected_bitmap_size,
                                video_frame);
}

#if !defined(USE_AURA) && !defined(OS_MACOSX)
// TODO(danakj): Remove this case when GTK linux is no more and move the
// values inline to testing::Values() below.
static const CompositingMode kAllCompositingModes[] = {GL_COMPOSITING};
#else
static const CompositingMode kAllCompositingModes[] = {GL_COMPOSITING,
                                                       SOFTWARE_COMPOSITING};
#endif

INSTANTIATE_TEST_CASE_P(GLAndSoftwareCompositing,
                        CompositingRenderWidgetHostViewBrowserTest,
                        testing::ValuesIn(kAllCompositingModes));
INSTANTIATE_TEST_CASE_P(GLAndSoftwareCompositing,
                        CompositingRenderWidgetHostViewBrowserTestTabCapture,
                        testing::ValuesIn(kAllCompositingModes));
INSTANTIATE_TEST_CASE_P(GLAndSoftwareCompositing,
                        CompositingRenderWidgetHostViewTabCaptureHighDPI,
                        testing::ValuesIn(kAllCompositingModes));

#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace
}  // namespace content
