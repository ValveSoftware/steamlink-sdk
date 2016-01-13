// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/gesture_nav_simple.h"

#include "cc/layers/layer.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/content_client.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"

namespace content {

namespace {

const int kArrowHeight = 280;
const int kArrowWidth = 140;
const float kMinOpacity = 0.25f;

bool ShouldNavigateForward(const NavigationController& controller,
                           OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_EAST : OVERSCROLL_WEST) &&
         controller.CanGoForward();
}

bool ShouldNavigateBack(const NavigationController& controller,
                        OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_WEST : OVERSCROLL_EAST) &&
         controller.CanGoBack();
}

// An animation observers that deletes itself and a pointer after the end of the
// animation.
template <class T>
class DeleteAfterAnimation : public ui::ImplicitAnimationObserver {
 public:
  explicit DeleteAfterAnimation(scoped_ptr<T> object)
      : object_(object.Pass()) {}

 private:
  friend class base::DeleteHelper<DeleteAfterAnimation<T> >;

  virtual ~DeleteAfterAnimation() {}

  // ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    // Deleting an observer when a ScopedLayerAnimationSettings is iterating
    // over them can cause a crash (which can happen during tests). So instead,
    // schedule this observer to be deleted soon.
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }

  scoped_ptr<T> object_;
  DISALLOW_COPY_AND_ASSIGN(DeleteAfterAnimation);
};

}  // namespace

// A layer delegate that paints the shield with the arrow in it.
class ArrowLayerDelegate : public ui::LayerDelegate {
 public:
  explicit ArrowLayerDelegate(int resource_id)
      : image_(GetContentClient()->GetNativeImageNamed(resource_id)),
        left_arrow_(resource_id == IDR_BACK_ARROW) {
    CHECK(!image_.IsEmpty());
  }

  virtual ~ArrowLayerDelegate() {}

  bool left() const { return left_arrow_; }

 private:
  // ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    SkPaint paint;
    paint.setColor(SkColorSetARGB(0xa0, 0, 0, 0));
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);

    canvas->DrawCircle(
        gfx::Point(left_arrow_ ? 0 : kArrowWidth, kArrowHeight / 2),
        kArrowWidth,
        paint);
    canvas->DrawImageInt(*image_.ToImageSkia(),
                         left_arrow_ ? 0 : kArrowWidth - image_.Width(),
                         (kArrowHeight - image_.Height()) / 2);
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  const gfx::Image& image_;
  const bool left_arrow_;

  DISALLOW_COPY_AND_ASSIGN(ArrowLayerDelegate);
};

GestureNavSimple::GestureNavSimple(WebContentsImpl* web_contents)
    : web_contents_(web_contents),
      completion_threshold_(0.f) {}

GestureNavSimple::~GestureNavSimple() {}

void GestureNavSimple::ApplyEffectsAndDestroy(const gfx::Transform& transform,
                                              float opacity) {
  ui::Layer* layer = arrow_.get();
  ui::ScopedLayerAnimationSettings settings(arrow_->GetAnimator());
  settings.AddObserver(
      new DeleteAfterAnimation<ArrowLayerDelegate>(arrow_delegate_.Pass()));
  settings.AddObserver(new DeleteAfterAnimation<ui::Layer>(arrow_.Pass()));
  settings.AddObserver(new DeleteAfterAnimation<ui::Layer>(clip_layer_.Pass()));
  layer->SetTransform(transform);
  layer->SetOpacity(opacity);
}

void GestureNavSimple::AbortGestureAnimation() {
  if (!arrow_)
    return;
  gfx::Transform transform;
  transform.Translate(arrow_delegate_->left() ? -kArrowWidth : kArrowWidth, 0);
  ApplyEffectsAndDestroy(transform, kMinOpacity);
}

void GestureNavSimple::CompleteGestureAnimation() {
  if (!arrow_)
    return;
  // Make sure the fade-out starts from the complete state.
  ApplyEffectsForDelta(completion_threshold_);
  ApplyEffectsAndDestroy(arrow_->transform(), 0.f);
}

void GestureNavSimple::ApplyEffectsForDelta(float delta_x) {
  if (!arrow_)
    return;
  CHECK_GT(completion_threshold_, 0.f);
  CHECK_GE(delta_x, 0.f);
  double complete = std::min(1.f, delta_x / completion_threshold_);
  float translate_x = gfx::Tween::FloatValueBetween(complete, -kArrowWidth, 0);
  gfx::Transform transform;
  transform.Translate(arrow_delegate_->left() ? translate_x : -translate_x,
                      0.f);
  arrow_->SetTransform(transform);
  arrow_->SetOpacity(gfx::Tween::FloatValueBetween(complete, kMinOpacity, 1.f));
}

gfx::Rect GestureNavSimple::GetVisibleBounds() const {
  return web_contents_->GetNativeView()->bounds();
}

void GestureNavSimple::OnOverscrollUpdate(float delta_x, float delta_y) {
  ApplyEffectsForDelta(std::abs(delta_x) + 50.f);
}

void GestureNavSimple::OnOverscrollComplete(OverscrollMode overscroll_mode) {
  CompleteGestureAnimation();

  NavigationControllerImpl& controller = web_contents_->GetController();
  if (ShouldNavigateForward(controller, overscroll_mode))
    controller.GoForward();
  else if (ShouldNavigateBack(controller, overscroll_mode))
    controller.GoBack();
}

void GestureNavSimple::OnOverscrollModeChange(OverscrollMode old_mode,
                                              OverscrollMode new_mode) {
  NavigationControllerImpl& controller = web_contents_->GetController();
  if (!ShouldNavigateForward(controller, new_mode) &&
      !ShouldNavigateBack(controller, new_mode)) {
    AbortGestureAnimation();
    return;
  }

  arrow_.reset(new ui::Layer(ui::LAYER_TEXTURED));
  // Note that RTL doesn't affect the arrow that should be displayed.
  int resource_id = 0;
  if (new_mode == OVERSCROLL_WEST)
    resource_id = IDR_FORWARD_ARROW;
  else if (new_mode == OVERSCROLL_EAST)
    resource_id = IDR_BACK_ARROW;
  else
    NOTREACHED();

  arrow_delegate_.reset(new ArrowLayerDelegate(resource_id));
  arrow_->set_delegate(arrow_delegate_.get());
  arrow_->SetFillsBoundsOpaquely(false);

  aura::Window* window = web_contents_->GetNativeView();
  const gfx::Rect& window_bounds = window->bounds();
  completion_threshold_ = window_bounds.width() *
      GetOverscrollConfig(OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE);

  // Align on the left or right edge.
  int x = (resource_id == IDR_BACK_ARROW) ? 0 :
      (window_bounds.width() - kArrowWidth);
  // Align in the center vertically.
  int y = std::max(0, (window_bounds.height() - kArrowHeight) / 2);
  arrow_->SetBounds(gfx::Rect(x, y, kArrowWidth, kArrowHeight));
  ApplyEffectsForDelta(0.f);

  // Adding the arrow as a child of the content window is not sufficient,
  // because it is possible for a new layer to be parented on top of the arrow
  // layer (e.g. when the navigated-to page is displayed while the completion
  // animation is in progress). So instead, a clip layer (that doesn't paint) is
  // installed on top of the content window as its sibling, and the arrow layer
  // is added to that clip layer.
  clip_layer_.reset(new ui::Layer(ui::LAYER_NOT_DRAWN));
  clip_layer_->SetBounds(window->layer()->bounds());
  clip_layer_->SetMasksToBounds(true);
  clip_layer_->Add(arrow_.get());

  ui::Layer* parent = window->layer()->parent();
  parent->Add(clip_layer_.get());
  parent->StackAtTop(clip_layer_.get());
}

}  // namespace content
