// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_navigation_overlay.h"

#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/aura/image_window_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"
#include "ui/base/layout.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

// A LayerDelegate that paints an image for the layer.
class ImageLayerDelegate : public ui::LayerDelegate {
 public:
  ImageLayerDelegate() {}

  virtual ~ImageLayerDelegate() {}

  void SetImage(const gfx::Image& image) {
    image_ = image;
    image_size_ = image.AsImageSkia().size();
  }
  const gfx::Image& image() const { return image_; }

 private:
  // Overridden from ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    if (image_.IsEmpty()) {
      canvas->DrawColor(SK_ColorGRAY);
    } else {
      SkISize size = canvas->sk_canvas()->getDeviceSize();
      if (size.width() != image_size_.width() ||
          size.height() != image_size_.height()) {
        canvas->DrawColor(SK_ColorWHITE);
      }
      canvas->DrawImageInt(image_.AsImageSkia(), 0, 0);
    }
  }

  // Called when the layer's device scale factor has changed.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }

  // Invoked prior to the bounds changing. The returned closured is run after
  // the bounds change.
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  gfx::Image image_;
  gfx::Size image_size_;

  DISALLOW_COPY_AND_ASSIGN(ImageLayerDelegate);
};

// Responsible for fading out and deleting the layer of the overlay window.
class OverlayDismissAnimator
    : public ui::LayerAnimationObserver {
 public:
  // Takes ownership of the layer.
  explicit OverlayDismissAnimator(scoped_ptr<ui::Layer> layer)
      : layer_(layer.Pass()) {
    CHECK(layer_.get());
  }

  // Starts the fadeout animation on the layer. When the animation finishes,
  // the object deletes itself along with the layer.
  void Animate() {
    DCHECK(layer_.get());
    ui::LayerAnimator* animator = layer_->GetAnimator();
    // This makes SetOpacity() animate with default duration (which could be
    // zero, e.g. when running tests).
    ui::ScopedLayerAnimationSettings settings(animator);
    animator->AddObserver(this);
    layer_->SetOpacity(0);
  }

  // Overridden from ui::LayerAnimationObserver
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    delete this;
  }

  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    delete this;
  }

  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE {}

 private:
  virtual ~OverlayDismissAnimator() {}

  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(OverlayDismissAnimator);
};

OverscrollNavigationOverlay::OverscrollNavigationOverlay(
    WebContentsImpl* web_contents)
    : web_contents_(web_contents),
      image_delegate_(NULL),
      loading_complete_(false),
      received_paint_update_(false),
      pending_entry_id_(0),
      slide_direction_(SLIDE_UNKNOWN) {
}

OverscrollNavigationOverlay::~OverscrollNavigationOverlay() {
}

void OverscrollNavigationOverlay::StartObserving() {
  loading_complete_ = false;
  received_paint_update_ = false;
  overlay_dismiss_layer_.reset();
  pending_entry_id_ = 0;
  Observe(web_contents_);

  // Make sure the overlay window is on top.
  if (window_.get() && window_->parent())
    window_->parent()->StackChildAtTop(window_.get());

  // Assumes the navigation has been initiated.
  NavigationEntry* pending_entry =
      web_contents_->GetController().GetPendingEntry();
  // Save id of the pending entry to identify when it loads and paints later.
  // Under some circumstances navigation can leave a null pending entry -
  // see comments in NavigationControllerImpl::NavigateToPendingEntry().
  pending_entry_id_ = pending_entry ? pending_entry->GetUniqueID() : 0;
}

void OverscrollNavigationOverlay::SetOverlayWindow(
    scoped_ptr<aura::Window> window,
    ImageWindowDelegate* delegate) {
  window_ = window.Pass();
  if (window_.get() && window_->parent())
    window_->parent()->StackChildAtTop(window_.get());
  image_delegate_ = delegate;

  if (window_.get() && delegate->has_image()) {
    window_slider_.reset(new WindowSlider(this,
                                          window_->parent(),
                                          window_.get()));
    slide_direction_ = SLIDE_UNKNOWN;
  } else {
    window_slider_.reset();
  }
}

void OverscrollNavigationOverlay::StopObservingIfDone() {
  // Normally we dismiss the overlay once we receive a paint update, however
  // for in-page navigations DidFirstVisuallyNonEmptyPaint() does not get
  // called, and we rely on loading_complete_ for those cases.
  if (!received_paint_update_ && !loading_complete_)
    return;

  // If a slide is in progress, then do not destroy the window or the slide.
  if (window_slider_.get() && window_slider_->IsSlideInProgress())
    return;

  // The layer to be animated by OverlayDismissAnimator
  scoped_ptr<ui::Layer> overlay_dismiss_layer;
  if (overlay_dismiss_layer_)
    overlay_dismiss_layer = overlay_dismiss_layer_.Pass();
  else if (window_.get())
    overlay_dismiss_layer = window_->AcquireLayer();
  Observe(NULL);
  window_slider_.reset();
  window_.reset();
  image_delegate_ = NULL;
  if (overlay_dismiss_layer.get()) {
    // OverlayDismissAnimator deletes overlay_dismiss_layer and itself when the
    // animation completes.
    (new OverlayDismissAnimator(overlay_dismiss_layer.Pass()))->Animate();
  }
}

ui::Layer* OverscrollNavigationOverlay::CreateSlideLayer(int offset) {
  const NavigationControllerImpl& controller = web_contents_->GetController();
  const NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
      controller.GetEntryAtOffset(offset));

  gfx::Image image;
  if (entry && entry->screenshot().get()) {
    std::vector<gfx::ImagePNGRep> image_reps;
    image_reps.push_back(gfx::ImagePNGRep(entry->screenshot(),
        ui::GetScaleFactorForNativeView(window_.get())));
    image = gfx::Image(image_reps);
  }
  if (!layer_delegate_)
    layer_delegate_.reset(new ImageLayerDelegate());
  layer_delegate_->SetImage(image);

  ui::Layer* layer = new ui::Layer(ui::LAYER_TEXTURED);
  layer->set_delegate(layer_delegate_.get());
  return layer;
}

ui::Layer* OverscrollNavigationOverlay::CreateBackLayer() {
  if (!web_contents_->GetController().CanGoBack())
    return NULL;
  slide_direction_ = SLIDE_BACK;
  return CreateSlideLayer(-1);
}

ui::Layer* OverscrollNavigationOverlay::CreateFrontLayer() {
  if (!web_contents_->GetController().CanGoForward())
    return NULL;
  slide_direction_ = SLIDE_FRONT;
  return CreateSlideLayer(1);
}

void OverscrollNavigationOverlay::OnWindowSlideCompleting() {
  if (slide_direction_ == SLIDE_UNKNOWN)
    return;

  // Perform the navigation.
  if (slide_direction_ == SLIDE_BACK)
    web_contents_->GetController().GoBack();
  else if (slide_direction_ == SLIDE_FRONT)
    web_contents_->GetController().GoForward();
  else
    NOTREACHED();

  // Reset state and wait for the new navigation page to complete
  // loading/painting.
  StartObserving();
}

void OverscrollNavigationOverlay::OnWindowSlideCompleted(
    scoped_ptr<ui::Layer> layer) {
  if (slide_direction_ == SLIDE_UNKNOWN) {
    window_slider_.reset();
    StopObservingIfDone();
    return;
  }

  // Change the image used for the overlay window.
  image_delegate_->SetImage(layer_delegate_->image());
  window_->layer()->SetTransform(gfx::Transform());
  window_->SchedulePaintInRect(gfx::Rect(window_->bounds().size()));
  slide_direction_ = SLIDE_UNKNOWN;
  // We may end up dismissing the overlay before it has a chance to repaint, so
  // set the slider layer to be the one animated by OverlayDismissAnimator.
  if (layer.get())
    overlay_dismiss_layer_ = layer.Pass();
  StopObservingIfDone();
}

void OverscrollNavigationOverlay::OnWindowSlideAborted() {
  StopObservingIfDone();
}

void OverscrollNavigationOverlay::OnWindowSliderDestroyed() {
  // We only want to take an action here if WindowSlider is being destroyed
  // outside of OverscrollNavigationOverlay. If window_slider_.get() is NULL,
  // then OverscrollNavigationOverlay is the one destroying WindowSlider, and
  // we don't need to do anything.
  // This check prevents StopObservingIfDone() being called multiple times
  // (including recursively) for a single event.
  if (window_slider_.get()) {
    // The slider has just been destroyed. Release the ownership.
    WindowSlider* slider ALLOW_UNUSED = window_slider_.release();
    StopObservingIfDone();
  }
}

void OverscrollNavigationOverlay::DidFirstVisuallyNonEmptyPaint() {
  int visible_entry_id =
      web_contents_->GetController().GetVisibleEntry()->GetUniqueID();
  if (visible_entry_id == pending_entry_id_ || !pending_entry_id_) {
    received_paint_update_ = true;
    StopObservingIfDone();
  }
}

void OverscrollNavigationOverlay::DidStopLoading(RenderViewHost* host) {
  // Use the last committed entry rather than the active one, in case a
  // pending entry has been created.
  int committed_entry_id =
      web_contents_->GetController().GetLastCommittedEntry()->GetUniqueID();
  if (committed_entry_id == pending_entry_id_ || !pending_entry_id_) {
    loading_complete_ = true;
    StopObservingIfDone();
  }
}

}  // namespace content
