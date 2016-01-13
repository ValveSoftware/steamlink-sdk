// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_element.h"

#include "base/compiler_specific.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_id_provider.h"
#include "ui/compositor/float_animation_curve_adapter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/transform_animation_curve_adapter.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/interpolated_transform.h"

namespace ui {

namespace {

// The factor by which duration is scaled up or down when
// ScopedAnimationDurationScaleMode::duration_scale_mode() is SLOW_DURATION or
// FAST_DURATION.
const int kSlowDurationScaleFactor = 4;
const int kFastDurationScaleFactor = 4;

// Pause -----------------------------------------------------------------------
class Pause : public LayerAnimationElement {
 public:
  Pause(AnimatableProperties properties, base::TimeDelta duration)
      : LayerAnimationElement(properties, duration) {
  }
  virtual ~Pause() {}

 private:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {}
  virtual bool OnProgress(double t,
                          LayerAnimationDelegate* delegate) OVERRIDE {
    return false;
  }
  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {}
  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(Pause);
};

// TransformTransition ---------------------------------------------------------

class TransformTransition : public LayerAnimationElement {
 public:
    TransformTransition(const gfx::Transform& target, base::TimeDelta duration)
      : LayerAnimationElement(TRANSFORM, duration),
        target_(target) {
  }
  virtual ~TransformTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetTransformForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetTransformFromAnimation(
        gfx::Tween::TransformValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->transform = target_;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  gfx::Transform start_;
  const gfx::Transform target_;

  DISALLOW_COPY_AND_ASSIGN(TransformTransition);
};

// InterpolatedTransformTransition ---------------------------------------------

class InterpolatedTransformTransition : public LayerAnimationElement {
 public:
  InterpolatedTransformTransition(InterpolatedTransform* interpolated_transform,
                                  base::TimeDelta duration)
      : LayerAnimationElement(TRANSFORM, duration),
        interpolated_transform_(interpolated_transform) {
  }
  virtual ~InterpolatedTransformTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetTransformFromAnimation(
        interpolated_transform_->Interpolate(static_cast<float>(t)));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->transform = interpolated_transform_->Interpolate(1.0f);
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  scoped_ptr<InterpolatedTransform> interpolated_transform_;

  DISALLOW_COPY_AND_ASSIGN(InterpolatedTransformTransition);
};

// BoundsTransition ------------------------------------------------------------

class BoundsTransition : public LayerAnimationElement {
 public:
  BoundsTransition(const gfx::Rect& target, base::TimeDelta duration)
      : LayerAnimationElement(BOUNDS, duration),
        target_(target) {
  }
  virtual ~BoundsTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetBoundsForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetBoundsFromAnimation(
        gfx::Tween::RectValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->bounds = target_;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  gfx::Rect start_;
  const gfx::Rect target_;

  DISALLOW_COPY_AND_ASSIGN(BoundsTransition);
};

// OpacityTransition -----------------------------------------------------------

class OpacityTransition : public LayerAnimationElement {
 public:
  OpacityTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(OPACITY, duration),
        start_(0.0f),
        target_(target) {
  }
  virtual ~OpacityTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetOpacityForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetOpacityFromAnimation(
        gfx::Tween::FloatValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->opacity = target_;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(OpacityTransition);
};

// VisibilityTransition --------------------------------------------------------

class VisibilityTransition : public LayerAnimationElement {
 public:
  VisibilityTransition(bool target, base::TimeDelta duration)
      : LayerAnimationElement(VISIBILITY, duration),
        start_(false),
        target_(target) {
  }
  virtual ~VisibilityTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetVisibilityForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetVisibilityFromAnimation(t == 1.0 ? target_ : start_);
    return t == 1.0;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->visibility = target_;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  bool start_;
  const bool target_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityTransition);
};

// BrightnessTransition --------------------------------------------------------

class BrightnessTransition : public LayerAnimationElement {
 public:
  BrightnessTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(BRIGHTNESS, duration),
        start_(0.0f),
        target_(target) {
  }
  virtual ~BrightnessTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetBrightnessForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetBrightnessFromAnimation(
        gfx::Tween::FloatValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->brightness = target_;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(BrightnessTransition);
};

// GrayscaleTransition ---------------------------------------------------------

class GrayscaleTransition : public LayerAnimationElement {
 public:
  GrayscaleTransition(float target, base::TimeDelta duration)
      : LayerAnimationElement(GRAYSCALE, duration),
        start_(0.0f),
        target_(target) {
  }
  virtual ~GrayscaleTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetGrayscaleForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetGrayscaleFromAnimation(
        gfx::Tween::FloatValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->grayscale = target_;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(GrayscaleTransition);
};

// ColorTransition -------------------------------------------------------------

class ColorTransition : public LayerAnimationElement {
 public:
  ColorTransition(SkColor target, base::TimeDelta duration)
      : LayerAnimationElement(COLOR, duration),
        start_(SK_ColorBLACK),
        target_(target) {
  }
  virtual ~ColorTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetColorForAnimation();
  }

  virtual bool OnProgress(double t, LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetColorFromAnimation(
        gfx::Tween::ColorValueBetween(t, start_, target_));
    return true;
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->color = target_;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {}

 private:
  SkColor start_;
  const SkColor target_;

  DISALLOW_COPY_AND_ASSIGN(ColorTransition);
};

// ThreadedLayerAnimationElement -----------------------------------------------

class ThreadedLayerAnimationElement : public LayerAnimationElement {
 public:
  ThreadedLayerAnimationElement(AnimatableProperties properties,
                                base::TimeDelta duration)
      : LayerAnimationElement(properties, duration) {
  }
  virtual ~ThreadedLayerAnimationElement() {}

  virtual bool IsThreaded() const OVERRIDE {
    return (duration() != base::TimeDelta());
  }

 protected:
  explicit ThreadedLayerAnimationElement(const LayerAnimationElement& element)
    : LayerAnimationElement(element) {
  }

  virtual bool OnProgress(double t,
                          LayerAnimationDelegate* delegate) OVERRIDE {
    if (t < 1.0)
      return false;

    if (Started()) {
      delegate->RemoveThreadedAnimation(animation_id());
    }

    OnEnd(delegate);
    return true;
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {
    if (delegate && Started()) {
      delegate->RemoveThreadedAnimation(animation_id());
    }
  }

  virtual void RequestEffectiveStart(
      LayerAnimationDelegate* delegate) OVERRIDE {
    DCHECK(animation_group_id());
    if (duration() == base::TimeDelta()) {
      set_effective_start_time(requested_start_time());
      return;
    }
    set_effective_start_time(base::TimeTicks());
    scoped_ptr<cc::Animation> animation = CreateCCAnimation();
    animation->set_needs_synchronized_start_time(true);
    delegate->AddThreadedAnimation(animation.Pass());
  }

  virtual void OnEnd(LayerAnimationDelegate* delegate) = 0;

  virtual scoped_ptr<cc::Animation> CreateCCAnimation() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadedLayerAnimationElement);
};

// ThreadedOpacityTransition ---------------------------------------------------

class ThreadedOpacityTransition : public ThreadedLayerAnimationElement {
 public:
  ThreadedOpacityTransition(float target, base::TimeDelta duration)
      : ThreadedLayerAnimationElement(OPACITY, duration),
        start_(0.0f),
        target_(target) {
  }
  virtual ~ThreadedOpacityTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetOpacityForAnimation();
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {
    if (delegate && Started()) {
      ThreadedLayerAnimationElement::OnAbort(delegate);
      delegate->SetOpacityFromAnimation(gfx::Tween::FloatValueBetween(
          gfx::Tween::CalculateValue(tween_type(), last_progressed_fraction()),
              start_,
              target_));
    }
  }

  virtual void OnEnd(LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetOpacityFromAnimation(target_);
  }

  virtual scoped_ptr<cc::Animation> CreateCCAnimation() OVERRIDE {
    scoped_ptr<cc::AnimationCurve> animation_curve(
        new FloatAnimationCurveAdapter(tween_type(),
                                       start_,
                                       target_,
                                       duration()));
    scoped_ptr<cc::Animation> animation(
        cc::Animation::Create(animation_curve.Pass(),
                              animation_id(),
                              animation_group_id(),
                              cc::Animation::Opacity));
    return animation.Pass();
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->opacity = target_;
  }

 private:
  float start_;
  const float target_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedOpacityTransition);
};

// ThreadedTransformTransition -------------------------------------------------

class ThreadedTransformTransition : public ThreadedLayerAnimationElement {
 public:
  ThreadedTransformTransition(const gfx::Transform& target,
                              base::TimeDelta duration)
      : ThreadedLayerAnimationElement(TRANSFORM, duration),
        target_(target) {
  }
  virtual ~ThreadedTransformTransition() {}

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    start_ = delegate->GetTransformForAnimation();
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {
    if (delegate && Started()) {
      ThreadedLayerAnimationElement::OnAbort(delegate);
      delegate->SetTransformFromAnimation(gfx::Tween::TransformValueBetween(
          gfx::Tween::CalculateValue(tween_type(), last_progressed_fraction()),
          start_,
          target_));
    }
  }

  virtual void OnEnd(LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetTransformFromAnimation(target_);
  }

  virtual scoped_ptr<cc::Animation> CreateCCAnimation() OVERRIDE {
    scoped_ptr<cc::AnimationCurve> animation_curve(
        new TransformAnimationCurveAdapter(tween_type(),
                                           start_,
                                           target_,
                                           duration()));
    scoped_ptr<cc::Animation> animation(
        cc::Animation::Create(animation_curve.Pass(),
                              animation_id(),
                              animation_group_id(),
                              cc::Animation::Transform));
    return animation.Pass();
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->transform = target_;
  }

 private:
  gfx::Transform start_;
  const gfx::Transform target_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedTransformTransition);
};

// InverseTransformTransision --------------------------------------------------

class InverseTransformTransition : public ThreadedLayerAnimationElement {
 public:
  InverseTransformTransition(const gfx::Transform& base_transform,
                             const LayerAnimationElement* uninverted_transition)
      : ThreadedLayerAnimationElement(*uninverted_transition),
        base_transform_(base_transform),
        uninverted_transition_(
            CheckAndCast<const ThreadedTransformTransition*>(
              uninverted_transition)) {
  }
  virtual ~InverseTransformTransition() {}

  static InverseTransformTransition* Clone(const LayerAnimationElement* other) {
    const InverseTransformTransition* other_inverse =
      CheckAndCast<const InverseTransformTransition*>(other);
    return new InverseTransformTransition(
        other_inverse->base_transform_, other_inverse->uninverted_transition_);
  }

 protected:
  virtual void OnStart(LayerAnimationDelegate* delegate) OVERRIDE {
    gfx::Transform start(delegate->GetTransformForAnimation());
    effective_start_ = base_transform_ * start;

    TargetValue target;
    uninverted_transition_->GetTargetValue(&target);
    base_target_ = target.transform;

    set_tween_type(uninverted_transition_->tween_type());

    TransformAnimationCurveAdapter base_curve(tween_type(),
                                              base_transform_,
                                              base_target_,
                                              duration());

    animation_curve_.reset(new InverseTransformCurveAdapter(
        base_curve, start, duration()));
    computed_target_transform_ = ComputeWithBaseTransform(effective_start_,
                                                          base_target_);
  }

  virtual void OnAbort(LayerAnimationDelegate* delegate) OVERRIDE {
    if (delegate && Started()) {
      ThreadedLayerAnimationElement::OnAbort(delegate);
      delegate->SetTransformFromAnimation(ComputeCurrentTransform());
    }
  }

  virtual void OnEnd(LayerAnimationDelegate* delegate) OVERRIDE {
    delegate->SetTransformFromAnimation(computed_target_transform_);
  }

  virtual scoped_ptr<cc::Animation> CreateCCAnimation() OVERRIDE {
    scoped_ptr<cc::Animation> animation(
        cc::Animation::Create(animation_curve_->Clone(),
                              animation_id(),
                              animation_group_id(),
                              cc::Animation::Transform));
    return animation.Pass();
  }

  virtual void OnGetTarget(TargetValue* target) const OVERRIDE {
    target->transform = computed_target_transform_;
  }

 private:
  gfx::Transform ComputeCurrentTransform() const {
    gfx::Transform base_current = gfx::Tween::TransformValueBetween(
        gfx::Tween::CalculateValue(tween_type(), last_progressed_fraction()),
        base_transform_,
        base_target_);
    return ComputeWithBaseTransform(effective_start_, base_current);
  }

  gfx::Transform ComputeWithBaseTransform(gfx::Transform start,
                                          gfx::Transform target) const {
    gfx::Transform to_return(gfx::Transform::kSkipInitialization);
    bool success = target.GetInverse(&to_return);
    DCHECK(success) << "Target transform must be invertible.";

    to_return.PreconcatTransform(start);
    return to_return;
  }

  template <typename T>
  static T CheckAndCast(const LayerAnimationElement* element) {
    AnimatableProperties properties = element->properties();
    DCHECK(properties & TRANSFORM);
    return static_cast<T>(element);
  }

  gfx::Transform effective_start_;
  gfx::Transform computed_target_transform_;

  const gfx::Transform base_transform_;
  gfx::Transform base_target_;

  scoped_ptr<cc::AnimationCurve> animation_curve_;

  const ThreadedTransformTransition* const uninverted_transition_;

  DISALLOW_COPY_AND_ASSIGN(InverseTransformTransition);
};

}  // namespace

// LayerAnimationElement::TargetValue ------------------------------------------

LayerAnimationElement::TargetValue::TargetValue()
    : opacity(0.0f),
      visibility(false),
      brightness(0.0f),
      grayscale(0.0f),
      color(SK_ColorBLACK) {
}

LayerAnimationElement::TargetValue::TargetValue(
    const LayerAnimationDelegate* delegate)
    : bounds(delegate ? delegate->GetBoundsForAnimation() : gfx::Rect()),
      transform(delegate ?
                delegate->GetTransformForAnimation() : gfx::Transform()),
      opacity(delegate ? delegate->GetOpacityForAnimation() : 0.0f),
      visibility(delegate ? delegate->GetVisibilityForAnimation() : false),
      brightness(delegate ? delegate->GetBrightnessForAnimation() : 0.0f),
      grayscale(delegate ? delegate->GetGrayscaleForAnimation() : 0.0f),
      color(delegate ? delegate->GetColorForAnimation() : 0.0f) {
}

// LayerAnimationElement -------------------------------------------------------

LayerAnimationElement::LayerAnimationElement(
    AnimatableProperties properties, base::TimeDelta duration)
    : first_frame_(true),
      properties_(properties),
      duration_(GetEffectiveDuration(duration)),
      tween_type_(gfx::Tween::LINEAR),
      animation_id_(cc::AnimationIdProvider::NextAnimationId()),
      animation_group_id_(0),
      last_progressed_fraction_(0.0),
      weak_ptr_factory_(this) {
}

LayerAnimationElement::LayerAnimationElement(
    const LayerAnimationElement &element)
    : first_frame_(element.first_frame_),
      properties_(element.properties_),
      duration_(element.duration_),
      tween_type_(element.tween_type_),
      animation_id_(cc::AnimationIdProvider::NextAnimationId()),
      animation_group_id_(element.animation_group_id_),
      last_progressed_fraction_(element.last_progressed_fraction_),
      weak_ptr_factory_(this) {
}

LayerAnimationElement::~LayerAnimationElement() {
}

void LayerAnimationElement::Start(LayerAnimationDelegate* delegate,
                                  int animation_group_id) {
  DCHECK(requested_start_time_ != base::TimeTicks());
  DCHECK(first_frame_);
  animation_group_id_ = animation_group_id;
  last_progressed_fraction_ = 0.0;
  OnStart(delegate);
  RequestEffectiveStart(delegate);
  first_frame_ = false;
}

bool LayerAnimationElement::Progress(base::TimeTicks now,
                                     LayerAnimationDelegate* delegate) {
  DCHECK(requested_start_time_ != base::TimeTicks());
  DCHECK(!first_frame_);

  bool need_draw;
  double t = 1.0;

  if ((effective_start_time_ == base::TimeTicks()) ||
      (now < effective_start_time_))  {
    // This hasn't actually started yet.
    need_draw = false;
    last_progressed_fraction_ = 0.0;
    return need_draw;
  }

  base::TimeDelta elapsed = now - effective_start_time_;
  if ((duration_ > base::TimeDelta()) && (elapsed < duration_))
    t = elapsed.InMillisecondsF() / duration_.InMillisecondsF();
  base::WeakPtr<LayerAnimationElement> alive(weak_ptr_factory_.GetWeakPtr());
  need_draw = OnProgress(gfx::Tween::CalculateValue(tween_type_, t), delegate);
  if (!alive)
    return need_draw;
  first_frame_ = t == 1.0;
  last_progressed_fraction_ = t;
  return need_draw;
}

bool LayerAnimationElement::IsFinished(base::TimeTicks time,
                                       base::TimeDelta* total_duration) {
  // If an effective start has been requested but the effective start time
  // hasn't yet been set, the animation is not finished, regardless of the
  // value of |time|.
  if (!first_frame_ && (effective_start_time_ == base::TimeTicks()))
    return false;

  base::TimeDelta queueing_delay;
  if (!first_frame_)
    queueing_delay = effective_start_time_ - requested_start_time_;

  base::TimeDelta elapsed = time - requested_start_time_;
  if (elapsed >= duration_ + queueing_delay) {
    *total_duration = duration_ + queueing_delay;
    return true;
  }
  return false;
}

bool LayerAnimationElement::ProgressToEnd(LayerAnimationDelegate* delegate) {
  if (first_frame_)
    OnStart(delegate);
  base::WeakPtr<LayerAnimationElement> alive(weak_ptr_factory_.GetWeakPtr());
  bool need_draw = OnProgress(1.0, delegate);
  if (!alive)
    return need_draw;
  last_progressed_fraction_ = 1.0;
  first_frame_ = true;
  return need_draw;
}

void LayerAnimationElement::GetTargetValue(TargetValue* target) const {
  OnGetTarget(target);
}

bool LayerAnimationElement::IsThreaded() const {
  return false;
}

void LayerAnimationElement::Abort(LayerAnimationDelegate* delegate) {
  OnAbort(delegate);
  first_frame_ = true;
}

void LayerAnimationElement::RequestEffectiveStart(
    LayerAnimationDelegate* delegate) {
  DCHECK(requested_start_time_ != base::TimeTicks());
  effective_start_time_ = requested_start_time_;
}

// static
LayerAnimationElement::AnimatableProperty
LayerAnimationElement::ToAnimatableProperty(
    cc::Animation::TargetProperty property) {
  switch (property) {
    case cc::Animation::Transform:
      return TRANSFORM;
    case cc::Animation::Opacity:
      return OPACITY;
    default:
      NOTREACHED();
      return AnimatableProperty();
  }
}

// static
base::TimeDelta LayerAnimationElement::GetEffectiveDuration(
    const base::TimeDelta& duration) {
  switch (ScopedAnimationDurationScaleMode::duration_scale_mode()) {
    case ScopedAnimationDurationScaleMode::NORMAL_DURATION:
      return duration;
    case ScopedAnimationDurationScaleMode::FAST_DURATION:
      return duration / kFastDurationScaleFactor;
    case ScopedAnimationDurationScaleMode::SLOW_DURATION:
      return duration * kSlowDurationScaleFactor;
    case ScopedAnimationDurationScaleMode::ZERO_DURATION:
      return base::TimeDelta();
    default:
      NOTREACHED();
      return base::TimeDelta();
  }
}

// static
LayerAnimationElement* LayerAnimationElement::CreateTransformElement(
    const gfx::Transform& transform,
    base::TimeDelta duration) {
  return new ThreadedTransformTransition(transform, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateInverseTransformElement(
    const gfx::Transform& base_transform,
    const LayerAnimationElement* uninverted_transition) {
  return new InverseTransformTransition(base_transform, uninverted_transition);
}

// static
LayerAnimationElement* LayerAnimationElement::CloneInverseTransformElement(
    const LayerAnimationElement* other) {
  return InverseTransformTransition::Clone(other);
}

// static
LayerAnimationElement*
LayerAnimationElement::CreateInterpolatedTransformElement(
    InterpolatedTransform* interpolated_transform,
    base::TimeDelta duration) {
  return new InterpolatedTransformTransition(interpolated_transform, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateBoundsElement(
    const gfx::Rect& bounds,
    base::TimeDelta duration) {
  return new BoundsTransition(bounds, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateOpacityElement(
    float opacity,
    base::TimeDelta duration) {
  return new ThreadedOpacityTransition(opacity, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateVisibilityElement(
    bool visibility,
    base::TimeDelta duration) {
  return new VisibilityTransition(visibility, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateBrightnessElement(
    float brightness,
    base::TimeDelta duration) {
  return new BrightnessTransition(brightness, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateGrayscaleElement(
    float grayscale,
    base::TimeDelta duration) {
  return new GrayscaleTransition(grayscale, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreatePauseElement(
    AnimatableProperties properties,
    base::TimeDelta duration) {
  return new Pause(properties, duration);
}

// static
LayerAnimationElement* LayerAnimationElement::CreateColorElement(
    SkColor color,
    base::TimeDelta duration) {
  return new ColorTransition(color, duration);
}

}  // namespace ui
