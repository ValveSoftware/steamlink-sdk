// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/window_slider.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/web_contents/aura/shadow_layer_delegate.h"
#include "content/public/browser/overscroll_configuration.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"

namespace content {

namespace {

// An animation observer that runs a callback at the end of the animation, and
// destroys itself.
class CallbackAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  CallbackAnimationObserver(const base::Closure& closure)
      : closure_(closure) {
  }

  virtual ~CallbackAnimationObserver() {}

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    if (!closure_.is_null())
      closure_.Run();
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  const base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(CallbackAnimationObserver);
};

}  // namespace

WindowSlider::WindowSlider(Delegate* delegate,
                           aura::Window* event_window,
                           aura::Window* owner)
    : delegate_(delegate),
      event_window_(event_window),
      owner_(owner),
      active_animator_(NULL),
      delta_x_(0.f),
      weak_factory_(this),
      active_start_threshold_(0.f),
      start_threshold_touchscreen_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN)),
      start_threshold_touchpad_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD)),
      complete_threshold_(content::GetOverscrollConfig(
          content::OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE)) {
  event_window_->AddPreTargetHandler(this);

  event_window_->AddObserver(this);
  owner_->AddObserver(this);
}

WindowSlider::~WindowSlider() {
  if (event_window_) {
    event_window_->RemovePreTargetHandler(this);
    event_window_->RemoveObserver(this);
  }
  if (owner_)
    owner_->RemoveObserver(this);
  delegate_->OnWindowSliderDestroyed();
}

void WindowSlider::ChangeOwner(aura::Window* new_owner) {
  if (owner_)
    owner_->RemoveObserver(this);
  owner_ = new_owner;
  if (owner_) {
    owner_->AddObserver(this);
    UpdateForScroll(0.f, 0.f);
  }
}

bool WindowSlider::IsSlideInProgress() const {
  // if active_start_threshold_ is 0, it means that sliding hasn't been started
  return active_start_threshold_ != 0 && (slider_.get() || active_animator_);
}

void WindowSlider::SetupSliderLayer() {
  ui::Layer* parent = owner_->layer()->parent();
  parent->Add(slider_.get());
  if (delta_x_ < 0)
    parent->StackAbove(slider_.get(), owner_->layer());
  else
    parent->StackBelow(slider_.get(), owner_->layer());
  slider_->SetBounds(owner_->layer()->bounds());
  slider_->SetVisible(true);
}

void WindowSlider::UpdateForScroll(float x_offset, float y_offset) {
  if (active_animator_) {
    // If there is an active animation, complete it before processing the scroll
    // so that the callbacks that are invoked on the Delegate are consistent.
    // Completing the animation may destroy WindowSlider through the animation
    // callback, so we can't continue processing the scroll event here.
    delta_x_ += x_offset;
    CompleteActiveAnimations();
    return;
  }

  float old_delta = delta_x_;
  delta_x_ += x_offset;
  if (fabs(delta_x_) < active_start_threshold_ && !slider_.get())
    return;

  if ((old_delta < 0 && delta_x_ > 0) ||
      (old_delta > 0 && delta_x_ < 0)) {
    slider_.reset();
    shadow_.reset();
  }

  float translate = 0.f;
  ui::Layer* translate_layer = NULL;

  if (!slider_.get()) {
    slider_.reset(delta_x_ < 0 ? delegate_->CreateFrontLayer() :
                                 delegate_->CreateBackLayer());
    if (!slider_.get())
      return;
    SetupSliderLayer();
  }

  if (delta_x_ <= -active_start_threshold_) {
    translate = owner_->bounds().width() +
        std::max(delta_x_ + active_start_threshold_,
                 static_cast<float>(-owner_->bounds().width()));
    translate_layer = slider_.get();
  } else if (delta_x_ >= active_start_threshold_) {
    translate = std::min(delta_x_ - active_start_threshold_,
                         static_cast<float>(owner_->bounds().width()));
    translate_layer = owner_->layer();
  } else {
    return;
  }

  if (!shadow_.get())
    shadow_.reset(new ShadowLayerDelegate(translate_layer));

  gfx::Transform transform;
  transform.Translate(translate, 0);
  translate_layer->SetTransform(transform);
}

void WindowSlider::CompleteOrResetSlide() {
  if (!slider_.get())
    return;

  int width = owner_->bounds().width();
  float ratio = (fabs(delta_x_) - active_start_threshold_) / width;
  if (ratio < complete_threshold_) {
    ResetSlide();
    return;
  }

  ui::Layer* sliding = delta_x_ < 0 ? slider_.get() : owner_->layer();
  active_animator_ = sliding->GetAnimator();

  ui::ScopedLayerAnimationSettings settings(active_animator_);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.AddObserver(new CallbackAnimationObserver(
      base::Bind(&WindowSlider::SlideAnimationCompleted,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&slider_),
                 base::Passed(&shadow_))));

  gfx::Transform transform;
  transform.Translate(delta_x_ < 0 ? 0 : width, 0);
  delta_x_ = 0;
  delegate_->OnWindowSlideCompleting();
  sliding->SetTransform(transform);
}

void WindowSlider::CompleteActiveAnimations() {
  if (active_animator_)
    active_animator_->StopAnimating();
}

void WindowSlider::ResetSlide() {
  if (!slider_.get())
    return;

  // Reset the state of the sliding layer.
  if (slider_.get()) {
    ui::Layer* translate_layer;
    gfx::Transform transform;
    if (delta_x_ < 0) {
      translate_layer = slider_.get();
      transform.Translate(translate_layer->bounds().width(), 0);
    } else {
      translate_layer = owner_->layer();
    }

    active_animator_ = translate_layer->GetAnimator();
    ui::ScopedLayerAnimationSettings settings(active_animator_);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    settings.AddObserver(new CallbackAnimationObserver(
        base::Bind(&WindowSlider::ResetSlideAnimationCompleted,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(&slider_),
                   base::Passed(&shadow_))));
    translate_layer->SetTransform(transform);
  }

  delta_x_ = 0.f;
}

void WindowSlider::SlideAnimationCompleted(
    scoped_ptr<ui::Layer> layer, scoped_ptr<ShadowLayerDelegate> shadow) {
  active_animator_ = NULL;
  shadow.reset();
  delegate_->OnWindowSlideCompleted(layer.Pass());
}

void WindowSlider::ResetSlideAnimationCompleted(
    scoped_ptr<ui::Layer> layer, scoped_ptr<ShadowLayerDelegate> shadow) {
  active_animator_ = NULL;
  shadow.reset();
  layer.reset();
  delegate_->OnWindowSlideAborted();
}

void WindowSlider::OnKeyEvent(ui::KeyEvent* event) {
  ResetSlide();
}

void WindowSlider::OnMouseEvent(ui::MouseEvent* event) {
  if (!(event->flags() & ui::EF_IS_SYNTHESIZED))
    ResetSlide();
}

void WindowSlider::OnScrollEvent(ui::ScrollEvent* event) {
  active_start_threshold_ = start_threshold_touchpad_;
  if (event->type() == ui::ET_SCROLL)
    UpdateForScroll(event->x_offset_ordinal(), event->y_offset_ordinal());
  else if (event->type() == ui::ET_SCROLL_FLING_START)
    CompleteOrResetSlide();
  else
    ResetSlide();
  event->SetHandled();
}

void WindowSlider::OnGestureEvent(ui::GestureEvent* event) {
  active_start_threshold_ = start_threshold_touchscreen_;
  const ui::GestureEventDetails& details = event->details();
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      CompleteActiveAnimations();
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      UpdateForScroll(details.scroll_x(), details.scroll_y());
      break;

    case ui::ET_GESTURE_SCROLL_END:
      CompleteOrResetSlide();
      break;

    case ui::ET_SCROLL_FLING_START:
      CompleteOrResetSlide();
      break;

    case ui::ET_GESTURE_PINCH_BEGIN:
    case ui::ET_GESTURE_PINCH_UPDATE:
    case ui::ET_GESTURE_PINCH_END:
      ResetSlide();
      break;

    default:
      break;
  }

  event->SetHandled();
}

void WindowSlider::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                  aura::Window* new_root) {
  if (window == event_window_) {
    window->RemoveObserver(this);
    window->RemovePreTargetHandler(this);
    event_window_ = NULL;
  } else if (window == owner_) {
    window->RemoveObserver(this);
    owner_ = NULL;
    delete this;
  } else {
    NOTREACHED();
  }
}

}  // namespace content
