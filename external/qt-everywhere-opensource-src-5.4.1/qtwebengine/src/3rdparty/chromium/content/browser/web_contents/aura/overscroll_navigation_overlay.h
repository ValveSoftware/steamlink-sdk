// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_OVERSCROLL_NAVIGATION_OVERLAY_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_OVERSCROLL_NAVIGATION_OVERLAY_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/web_contents/aura/window_slider.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

struct ViewHostMsg_UpdateRect_Params;

namespace content {

class ImageLayerDelegate;
class ImageWindowDelegate;
class OverscrollNavigationOverlayTest;

// When a history navigation is triggered at the end of an overscroll
// navigation, it is necessary to show the history-screenshot until the page is
// done navigating and painting. This class accomplishes this by showing the
// screenshot window on top of the page until the page has completed loading and
// painting.
class CONTENT_EXPORT OverscrollNavigationOverlay
    : public WebContentsObserver,
      public WindowSlider::Delegate {
 public:
  explicit OverscrollNavigationOverlay(WebContentsImpl* web_contents);
  virtual ~OverscrollNavigationOverlay();

  bool has_window() const { return !!window_.get(); }

  // Resets state and starts observing |web_contents_| for page load/paint
  // updates. This function makes sure that the screenshot window is stacked
  // on top, so that it hides the content window behind it, and destroys the
  // screenshot window when the page is done loading/painting.
  // This should be called immediately after initiating the navigation,
  // otherwise the overlay may be dismissed prematurely.
  void StartObserving();

  // Sets the screenshot window and the delegate. This takes ownership of
  // |window|.
  // Note that ImageWindowDelegate manages its own lifetime, so this function
  // does not take ownership of |delegate|.
  void SetOverlayWindow(scoped_ptr<aura::Window> window,
                        ImageWindowDelegate* delegate);

 private:
  friend class OverscrollNavigationOverlayTest;
  FRIEND_TEST_ALL_PREFIXES(OverscrollNavigationOverlayTest,
                           FirstVisuallyNonEmptyPaint_NoImage);
  FRIEND_TEST_ALL_PREFIXES(OverscrollNavigationOverlayTest,
                           FirstVisuallyNonEmptyPaint_WithImage);
  FRIEND_TEST_ALL_PREFIXES(OverscrollNavigationOverlayTest,
                           LoadUpdateWithoutNonEmptyPaint);
  FRIEND_TEST_ALL_PREFIXES(OverscrollNavigationOverlayTest,
                           MultiNavigation_LoadingUpdate);
  FRIEND_TEST_ALL_PREFIXES(OverscrollNavigationOverlayTest,
                           MultiNavigation_PaintUpdate);

  enum SlideDirection {
    SLIDE_UNKNOWN,
    SLIDE_BACK,
    SLIDE_FRONT
  };

  // Stop observing the page and start the final overlay fade-out animation if
  // a window-slide isn't in progress and either the page has been painted or
  // the page-load has completed.
  void StopObservingIfDone();

  // Creates a layer to be used for window-slide. |offset| is the offset of the
  // NavigationEntry for the screenshot image to display.
  ui::Layer* CreateSlideLayer(int offset);

  // Overridden from WindowSlider::Delegate:
  virtual ui::Layer* CreateBackLayer() OVERRIDE;
  virtual ui::Layer* CreateFrontLayer() OVERRIDE;
  virtual void OnWindowSlideCompleting() OVERRIDE;
  virtual void OnWindowSlideCompleted(scoped_ptr<ui::Layer> layer) OVERRIDE;
  virtual void OnWindowSlideAborted() OVERRIDE;
  virtual void OnWindowSliderDestroyed() OVERRIDE;

  // Overridden from WebContentsObserver:
  virtual void DidFirstVisuallyNonEmptyPaint() OVERRIDE;
  virtual void DidStopLoading(RenderViewHost* host) OVERRIDE;

  // The WebContents which is being navigated.
  WebContentsImpl* web_contents_;

  // The screenshot overlay window.
  scoped_ptr<aura::Window> window_;

  // This is the WindowDelegate of |window_|. The delegate manages its own
  // lifetime (destroys itself when |window_| is destroyed).
  ImageWindowDelegate* image_delegate_;

  bool loading_complete_;
  bool received_paint_update_;

  // Unique ID of the NavigationEntry we are navigating to. This is needed to
  // filter on WebContentsObserver callbacks and is used to dismiss the overlay
  // when the relevant page loads and paints.
  int pending_entry_id_;

  // The |WindowSlider| that allows sliding history layers while the page is
  // being reloaded.
  scoped_ptr<WindowSlider> window_slider_;

  // Layer to be used for the final overlay fadeout animation when the overlay
  // is being dismissed.
  scoped_ptr<ui::Layer> overlay_dismiss_layer_;

  // The direction of the in-progress slide (if any).
  SlideDirection slide_direction_;

  // The LayerDelegate used for the back/front layers during a slide.
  scoped_ptr<ImageLayerDelegate> layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollNavigationOverlay);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_OVERSCROLL_NAVIGATION_OVERLAY_H_
