// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"

#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/web_contents/aura/image_window_delegate.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/view_messages.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

class OverscrollNavigationOverlayTest : public RenderViewHostImplTestHarness {
 public:
  OverscrollNavigationOverlayTest() {}
  virtual ~OverscrollNavigationOverlayTest() {}

  gfx::Image CreateDummyScreenshot() {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    bitmap.allocPixels();
    bitmap.eraseColor(SK_ColorWHITE);
    return gfx::Image::CreateFrom1xBitmap(bitmap);
  }

  void SetDummyScreenshotOnNavEntry(NavigationEntry* entry) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    bitmap.allocPixels();
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
    ViewHostMsg_DidFirstVisuallyNonEmptyPaint msg(test_rvh()->GetRoutingID());
    RenderViewHostTester::TestOnMessageReceived(test_rvh(), msg);
  }

  void PerformBackNavigationViaSliderCallbacks() {
    // Sets slide direction to SLIDE_BACK, sets screenshot from NavEntry at
    // offset -1  on layer_delegate_.
    delete GetOverlay()->CreateBackLayer();
    // Performs BACK navigation, sets image from layer_delegate_ on
    // image_delegate_.
    GetOverlay()->OnWindowSlideCompleting();
    GetOverlay()->OnWindowSlideCompleted(scoped_ptr<ui::Layer>());
  }

 protected:
  // RenderViewHostImplTestHarness:
  virtual void SetUp() OVERRIDE {
    RenderViewHostImplTestHarness::SetUp();

    const GURL first("https://www.google.com");
    contents()->NavigateAndCommit(first);
    EXPECT_TRUE(controller().GetVisibleEntry());
    EXPECT_FALSE(controller().CanGoBack());

    const GURL second("http://www.chromium.org");
    contents()->NavigateAndCommit(second);
    EXPECT_TRUE(controller().CanGoBack());

    // Receive a paint update. This is necessary to make sure the size is set
    // correctly in RenderWidgetHostImpl.
    ViewHostMsg_UpdateRect_Params params;
    memset(&params, 0, sizeof(params));
    params.view_size = gfx::Size(10, 10);
    ViewHostMsg_UpdateRect rect(test_rvh()->GetRoutingID(), params);
    RenderViewHostTester::TestOnMessageReceived(test_rvh(), rect);

    // Reset pending flags for size/paint.
    test_rvh()->ResetSizeAndRepaintPendingFlags();

    // Create the overlay, and set the contents of the overlay window.
    overlay_.reset(new OverscrollNavigationOverlay(contents()));
    ImageWindowDelegate* image_delegate = new ImageWindowDelegate();
    scoped_ptr<aura::Window> overlay_window(
      aura::test::CreateTestWindowWithDelegate(
          image_delegate,
          0,
          gfx::Rect(root_window()->bounds().size()),
          root_window()));

    overlay_->SetOverlayWindow(overlay_window.Pass(), image_delegate);
    overlay_->StartObserving();

    EXPECT_TRUE(overlay_->web_contents());
    EXPECT_FALSE(overlay_->loading_complete_);
    EXPECT_FALSE(overlay_->received_paint_update_);
  }

  virtual void TearDown() OVERRIDE {
    overlay_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  OverscrollNavigationOverlay* GetOverlay() {
    return overlay_.get();
  }

 private:
  scoped_ptr<OverscrollNavigationOverlay> overlay_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollNavigationOverlayTest);
};

TEST_F(OverscrollNavigationOverlayTest, FirstVisuallyNonEmptyPaint_NoImage) {
  ReceivePaintUpdate();
  EXPECT_TRUE(GetOverlay()->received_paint_update_);
  EXPECT_FALSE(GetOverlay()->loading_complete_);
  // The paint update will hide the overlay.
  EXPECT_FALSE(GetOverlay()->web_contents());
}

TEST_F(OverscrollNavigationOverlayTest, FirstVisuallyNonEmptyPaint_WithImage) {
  GetOverlay()->image_delegate_->SetImage(CreateDummyScreenshot());

  ReceivePaintUpdate();
  EXPECT_TRUE(GetOverlay()->received_paint_update_);
  EXPECT_FALSE(GetOverlay()->loading_complete_);
  // The paint update will hide the overlay.
  EXPECT_FALSE(GetOverlay()->web_contents());
}

TEST_F(OverscrollNavigationOverlayTest, LoadUpdateWithoutNonEmptyPaint) {
  GetOverlay()->image_delegate_->SetImage(CreateDummyScreenshot());
  process()->sink().ClearMessages();

  contents()->TestSetIsLoading(false);
  EXPECT_TRUE(GetOverlay()->loading_complete_);
  EXPECT_FALSE(GetOverlay()->received_paint_update_);
  // The page load should hide the overlay.
  EXPECT_FALSE(GetOverlay()->web_contents());
}

TEST_F(OverscrollNavigationOverlayTest, MultiNavigation_PaintUpdate) {
  GetOverlay()->image_delegate_->SetImage(CreateDummyScreenshot());
  SetDummyScreenshotOnNavEntry(controller().GetEntryAtOffset(-1));

  PerformBackNavigationViaSliderCallbacks();
  // Screenshot was set on NavEntry at offset -1.
  EXPECT_TRUE(GetOverlay()->image_delegate_->has_image());
  EXPECT_FALSE(GetOverlay()->received_paint_update_);

  ReceivePaintUpdate();
  // Paint updates until the navigation is committed represent updates
  // for the previous page, so they shouldn't affect the flag.
  EXPECT_FALSE(GetOverlay()->received_paint_update_);

  contents()->CommitPendingNavigation();
  ReceivePaintUpdate();
  // Navigation was committed and the paint update was received - the flag
  // should now be updated.
  EXPECT_TRUE(GetOverlay()->received_paint_update_);

  EXPECT_FALSE(GetOverlay()->web_contents());
}

TEST_F(OverscrollNavigationOverlayTest, MultiNavigation_LoadingUpdate) {
  GetOverlay()->image_delegate_->SetImage(CreateDummyScreenshot());

  PerformBackNavigationViaSliderCallbacks();
  // No screenshot was set on NavEntry at offset -1.
  EXPECT_FALSE(GetOverlay()->image_delegate_->has_image());
  // Navigation was started, so the loading status flag should be reset.
  EXPECT_FALSE(GetOverlay()->loading_complete_);

  // Load updates until the navigation is committed represent updates for the
  // previous page, so they shouldn't affect the flag.
  contents()->TestSetIsLoading(true);
  contents()->TestSetIsLoading(false);
  EXPECT_FALSE(GetOverlay()->loading_complete_);

  contents()->CommitPendingNavigation();
  contents()->TestSetIsLoading(true);
  contents()->TestSetIsLoading(false);
  // Navigation was committed and the load update was received - the flag
  // should now be updated.
  EXPECT_TRUE(GetOverlay()->loading_complete_);

  EXPECT_FALSE(GetOverlay()->web_contents());
}

}  // namespace content
