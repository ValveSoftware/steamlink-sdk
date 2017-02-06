// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"

#include <string.h>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura_extra/image_window_delegate.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

// Forces web contents to complete web page load as soon as navigation starts.
class ImmediateLoadObserver : WebContentsObserver {
 public:
  explicit ImmediateLoadObserver(TestWebContents* contents)
      : contents_(contents) {
    Observe(contents);
  }
  ~ImmediateLoadObserver() override {}

  void DidStartNavigationToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) override {
    // Simulate immediate web page load.
    contents_->TestSetIsLoading(false);
    Observe(nullptr);
  }

 private:
  TestWebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(ImmediateLoadObserver);
};

// A subclass of TestWebContents that offers a fake content window.
class OverscrollTestWebContents : public TestWebContents {
 public:
  ~OverscrollTestWebContents() override {}

  static OverscrollTestWebContents* Create(
      BrowserContext* browser_context,
      scoped_refptr<SiteInstance> instance,
      std::unique_ptr<aura::Window> fake_native_view,
      std::unique_ptr<aura::Window> fake_contents_window) {
    OverscrollTestWebContents* web_contents = new OverscrollTestWebContents(
        browser_context, std::move(fake_native_view),
        std::move(fake_contents_window));
    web_contents->Init(
        WebContents::CreateParams(browser_context, std::move(instance)));
    return web_contents;
  }

  void ResetNativeView() { fake_native_view_.reset(); }

  void ResetContentNativeView() { fake_contents_window_.reset(); }

  void set_is_being_destroyed(bool val) { is_being_destroyed_ = val; }

  gfx::NativeView GetNativeView() override { return fake_native_view_.get(); }

  gfx::NativeView GetContentNativeView() override {
    return fake_contents_window_.get();
  }

  bool IsBeingDestroyed() const override { return is_being_destroyed_; }

 protected:
  explicit OverscrollTestWebContents(
      BrowserContext* browser_context,
      std::unique_ptr<aura::Window> fake_native_view,
      std::unique_ptr<aura::Window> fake_contents_window)
      : TestWebContents(browser_context),
        fake_native_view_(std::move(fake_native_view)),
        fake_contents_window_(std::move(fake_contents_window)),
        is_being_destroyed_(false) {}

 private:
  std::unique_ptr<aura::Window> fake_native_view_;
  std::unique_ptr<aura::Window> fake_contents_window_;
  bool is_being_destroyed_;
};

class OverscrollNavigationOverlayTest : public RenderViewHostImplTestHarness {
 public:
  OverscrollNavigationOverlayTest()
      : first_("https://www.google.com"),
        second_("http://www.chromium.org"),
        third_("https://www.kernel.org/"),
        fourth_("https://github.com/") {}

  ~OverscrollNavigationOverlayTest() override {}

  void SetDummyScreenshotOnNavEntry(NavigationEntry* entry) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(SK_ColorWHITE);
    std::vector<unsigned char> png_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &png_data);
    scoped_refptr<base::RefCountedBytes> png_bytes =
        base::RefCountedBytes::TakeVector(&png_data);
    NavigationEntryImpl* entry_impl =
        NavigationEntryImpl::FromNavigationEntry(entry);
    entry_impl->SetScreenshotPNGData(png_bytes);
  }

  void ReceivePaintUpdate() {
    ViewHostMsg_DidFirstVisuallyNonEmptyPaint msg(
        main_test_rfh()->GetRoutingID());
    RenderViewHostTester::TestOnMessageReceived(test_rvh(), msg);
  }

  void PerformBackNavigationViaSliderCallbacks() {
    // Sets slide direction to BACK, sets screenshot from NavEntry at
    // offset -1 on layer_delegate_.
    std::unique_ptr<aura::Window> window(
        GetOverlay()->CreateBackWindow(GetBackSlideWindowBounds()));
    bool window_created = !!window;
    // Performs BACK navigation, sets image from layer_delegate_ on
    // image_delegate_.
    GetOverlay()->OnOverscrollCompleting();
    if (window_created)
      EXPECT_EQ(GetOverlay()->direction_, OverscrollNavigationOverlay::BACK);
    else
      EXPECT_EQ(GetOverlay()->direction_, OverscrollNavigationOverlay::NONE);
    window->SetBounds(gfx::Rect(root_window()->bounds().size()));
    GetOverlay()->OnOverscrollCompleted(std::move(window));
    if (IsBrowserSideNavigationEnabled())
      main_test_rfh()->PrepareForCommit();
    else
      contents()->GetPendingMainFrame()->PrepareForCommit();
    if (window_created)
      EXPECT_TRUE(contents()->CrossProcessNavigationPending());
    else
      EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  }

  gfx::Rect GetFrontSlideWindowBounds() {
    gfx::Rect bounds = gfx::Rect(root_window()->bounds().size());
    bounds.Offset(root_window()->bounds().size().width(), 0);
    return bounds;
  }

  gfx::Rect GetBackSlideWindowBounds() {
    return gfx::Rect(root_window()->bounds().size());
  }

  // Const accessors.
  const GURL first() { return first_; }
  const GURL second() { return second_; }
  const GURL third() { return third_; }
  const GURL fourth() { return fourth_; }

 protected:
  // RenderViewHostImplTestHarness:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Set up the fake web contents native view.
    std::unique_ptr<aura::Window> fake_native_view(new aura::Window(nullptr));
    fake_native_view->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(fake_native_view.get());
    fake_native_view->SetBounds(gfx::Rect(root_window()->bounds().size()));

    // Set up the fake contents window.
    std::unique_ptr<aura::Window> fake_contents_window(
        new aura::Window(nullptr));
    fake_contents_window->Init(ui::LAYER_SOLID_COLOR);
    root_window()->AddChild(fake_contents_window.get());
    fake_contents_window->SetBounds(gfx::Rect(root_window()->bounds().size()));

    // Replace the default test web contents with our custom class.
    SetContents(OverscrollTestWebContents::Create(
        browser_context(), SiteInstance::Create(browser_context()),
        std::move(fake_native_view), std::move(fake_contents_window)));

    contents()->NavigateAndCommit(first());
    EXPECT_TRUE(controller().GetVisibleEntry());
    EXPECT_FALSE(controller().CanGoBack());

    contents()->NavigateAndCommit(second());
    EXPECT_TRUE(controller().CanGoBack());

    contents()->NavigateAndCommit(third());
    EXPECT_TRUE(controller().CanGoBack());

    contents()->NavigateAndCommit(fourth_);
    EXPECT_TRUE(controller().CanGoBack());
    EXPECT_FALSE(controller().CanGoForward());

    // Receive a paint update. This is necessary to make sure the size is set
    // correctly in RenderWidgetHostImpl.
    ViewHostMsg_UpdateRect_Params params;
    memset(&params, 0, sizeof(params));
    params.view_size = gfx::Size(10, 10);
    ViewHostMsg_UpdateRect rect(test_rvh()->GetRoutingID(), params);
    RenderViewHostTester::TestOnMessageReceived(test_rvh(), rect);

    // Reset pending flags for size/paint.
    test_rvh()->GetWidget()->ResetSizeAndRepaintPendingFlags();

    // Create the overlay, and set the contents of the overlay window.
    overlay_.reset(new OverscrollNavigationOverlay(contents(), root_window()));
  }

  void TearDown() override {
    overlay_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  OverscrollNavigationOverlay* GetOverlay() {
    return overlay_.get();
  }

 private:
  // Tests URLs.
  const GURL first_;
  const GURL second_;
  const GURL third_;
  const GURL fourth_;

  std::unique_ptr<OverscrollNavigationOverlay> overlay_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollNavigationOverlayTest);
};

// Tests that if a screenshot is available, it is set in the overlay window
// delegate.
TEST_F(OverscrollNavigationOverlayTest, WithScreenshot) {
  SetDummyScreenshotOnNavEntry(controller().GetEntryAtOffset(-1));
  PerformBackNavigationViaSliderCallbacks();
  // Screenshot was set on NavEntry at offset -1.
  EXPECT_TRUE(static_cast<aura_extra::ImageWindowDelegate*>(
                  GetOverlay()->window_->delegate())->has_image());
}

// Tests that if a screenshot is not available, no image is set in the overlay
// window delegate.
TEST_F(OverscrollNavigationOverlayTest, WithoutScreenshot) {
  PerformBackNavigationViaSliderCallbacks();
  // No screenshot was set on NavEntry at offset -1.
  EXPECT_FALSE(static_cast<aura_extra::ImageWindowDelegate*>(
                   GetOverlay()->window_->delegate())->has_image());
}

// Tests that if a navigation is attempted but there is nothing to navigate to,
// we return a null window.
TEST_F(OverscrollNavigationOverlayTest, CannotNavigate) {
  EXPECT_EQ(GetOverlay()->CreateFrontWindow(GetFrontSlideWindowBounds()),
            nullptr);
}

// Tests that if a navigation is cancelled, no navigation is performed and the
// state is restored.
TEST_F(OverscrollNavigationOverlayTest, CancelNavigation) {
  std::unique_ptr<aura::Window> window =
      GetOverlay()->CreateBackWindow(GetBackSlideWindowBounds());
  EXPECT_EQ(GetOverlay()->direction_, OverscrollNavigationOverlay::BACK);

  GetOverlay()->OnOverscrollCancelled();
  EXPECT_FALSE(contents()->CrossProcessNavigationPending());
  EXPECT_EQ(GetOverlay()->direction_, OverscrollNavigationOverlay::NONE);
}

// Performs two navigations. The second navigation is cancelled, tests that the
// first one worked correctly.
TEST_F(OverscrollNavigationOverlayTest, CancelAfterSuccessfulNavigation) {
  PerformBackNavigationViaSliderCallbacks();
  std::unique_ptr<aura::Window> wrapper =
      GetOverlay()->CreateBackWindow(GetBackSlideWindowBounds());
  EXPECT_EQ(GetOverlay()->direction_, OverscrollNavigationOverlay::BACK);

  GetOverlay()->OnOverscrollCancelled();
  EXPECT_EQ(GetOverlay()->direction_, OverscrollNavigationOverlay::NONE);

  EXPECT_TRUE(contents()->CrossProcessNavigationPending());
  NavigationEntry* pending = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigate(
      pending->GetPageID(), pending->GetUniqueID(), false, pending->GetURL());
  EXPECT_EQ(contents()->GetURL(), third());
}

// Tests that an overscroll navigation that receives a paint update actually
// stops observing.
TEST_F(OverscrollNavigationOverlayTest, Navigation_PaintUpdate) {
  PerformBackNavigationViaSliderCallbacks();
  ReceivePaintUpdate();

  // Paint updates until the navigation is committed typically represent updates
  // for the previous page, so we should still be observing.
  EXPECT_TRUE(GetOverlay()->web_contents());

  NavigationEntry* pending = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigate(
      pending->GetPageID(), pending->GetUniqueID(), false, pending->GetURL());
  ReceivePaintUpdate();

  // Navigation was committed and the paint update was received - we should no
  // longer be observing.
  EXPECT_FALSE(GetOverlay()->web_contents());
  EXPECT_EQ(contents()->GetURL(), third());
}

// Tests that an overscroll navigation that receives a loading update actually
// stops observing.
TEST_F(OverscrollNavigationOverlayTest, Navigation_LoadingUpdate) {
  PerformBackNavigationViaSliderCallbacks();
  EXPECT_TRUE(GetOverlay()->web_contents());
  // DidStopLoading for any navigation should always reset the load flag and
  // dismiss the overlay even if the pending navigation wasn't committed -
  // this is a "safety net" in case we mis-identify the destination webpage
  // (which can happen if a new navigation is performed while while a GestureNav
  // navigation is in progress).
  contents()->TestSetIsLoading(false);
  EXPECT_FALSE(GetOverlay()->web_contents());
  NavigationEntry* pending = contents()->GetController().GetPendingEntry();
  contents()->GetPendingMainFrame()->SendNavigate(
      pending->GetPageID(), pending->GetUniqueID(), false, pending->GetURL());
  EXPECT_EQ(contents()->GetURL(), third());
}

TEST_F(OverscrollNavigationOverlayTest, CloseDuringAnimation) {
  ui::ScopedAnimationDurationScaleMode normal_duration_(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetOverlay()->owa_->OnOverscrollModeChange(OVERSCROLL_NONE, OVERSCROLL_EAST);
  GetOverlay()->owa_->OnOverscrollComplete(OVERSCROLL_EAST);
  EXPECT_EQ(GetOverlay()->direction_, OverscrollNavigationOverlay::BACK);
  OverscrollTestWebContents* test_web_contents =
      static_cast<OverscrollTestWebContents*>(web_contents());
  test_web_contents->set_is_being_destroyed(true);
  test_web_contents->ResetContentNativeView();
  test_web_contents->ResetNativeView();
  // Ensure a clean close.
}

// Tests that we can handle the case when the load completes as soon as the
// navigation is started.
TEST_F(OverscrollNavigationOverlayTest, ImmediateLoadOnNavigate) {
  PerformBackNavigationViaSliderCallbacks();
  // This observer will force the page load to complete as soon as the
  // navigation starts.
  ImmediateLoadObserver immediate_nav(contents());
  GetOverlay()->owa_->OnOverscrollModeChange(OVERSCROLL_NONE, OVERSCROLL_EAST);
  // This will start and immediately complete the navigation.
  GetOverlay()->owa_->OnOverscrollComplete(OVERSCROLL_EAST);
  EXPECT_FALSE(GetOverlay()->window_.get());
}

// Tests that swapping the overlay window at the end of a gesture caused by the
// start of a new overscroll does not crash and the events still reach the new
// overlay window.
TEST_F(OverscrollNavigationOverlayTest, OverlayWindowSwap) {
  PerformBackNavigationViaSliderCallbacks();
  aura::Window* first_overlay_window = GetOverlay()->window_.get();
  EXPECT_TRUE(GetOverlay()->web_contents());
  EXPECT_TRUE(first_overlay_window);

  // At this stage, the overlay window is covering the web contents. Configure
  // the animator of the overlay window for the test.
  ui::ScopedAnimationDurationScaleMode normal_duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimator* animator = GetOverlay()->window_->layer()->GetAnimator();
  animator->set_disable_timer_for_test(true);
  ui::LayerAnimatorTestController test_controller(animator);

  int overscroll_complete_distance =
      root_window()->bounds().size().width() *
          content::GetOverscrollConfig(
              content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE) +
      ui::GestureConfiguration::GetInstance()
          ->max_touch_move_in_pixels_for_click() + 1;

  // Start and complete a back navigation via a gesture.
  ui::test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(gfx::Point(0, 0),
                                  gfx::Point(overscroll_complete_distance, 0),
                                  base::TimeDelta::FromMilliseconds(10),
                                  10);

  ui::ScopedLayerAnimationSettings settings(animator);
  test_controller.StartThreadedAnimationsIfNeeded();

  // The overlay window should now be being animated to the edge of the screen.
  // |first()overlay_window| is the back window.
  // This is what the screen should look like. The X indicates where the next
  // gesture starts for the test.
  // +---------root_window--------+
  // |+-back window--+--front window--+
  // ||              |            |   |
  // ||        1     |X       2   |   |
  // ||              |            |   |
  // |+--------------+------------|---+
  // +----------------------------+
  // |  overscroll   ||
  // |   complete    ||
  // |   distance    ||
  // |<------------->||
  // |     second     |
  // |   overscroll   |
  // | start distance |
  // |<-------------->|
  EXPECT_EQ(GetOverlay()->window_.get(), first_overlay_window);

  // The overlay window is halfway through, start another animation that will
  // cancel the first one. The event that cancels the animation will go to
  // the slide window, which will be used as the overlay window when the new
  // overscroll starts.
  int second_overscroll_start_distance = overscroll_complete_distance + 1;
  generator.GestureScrollSequence(
      gfx::Point(second_overscroll_start_distance, 0),
      gfx::Point(
          second_overscroll_start_distance + overscroll_complete_distance, 0),
      base::TimeDelta::FromMilliseconds(10), 10);
  EXPECT_TRUE(GetOverlay()->window_.get());
  // The overlay window should be a new window.
  EXPECT_NE(GetOverlay()->window_.get(), first_overlay_window);

  // Complete the animation.
  GetOverlay()->window_->layer()->GetAnimator()->StopAnimating();
  EXPECT_TRUE(GetOverlay()->window_.get());

  // Load the page.
  contents()->CommitPendingNavigation();
  ReceivePaintUpdate();
  EXPECT_FALSE(GetOverlay()->window_.get());
  EXPECT_EQ(contents()->GetURL(), first());
}

}  // namespace content
