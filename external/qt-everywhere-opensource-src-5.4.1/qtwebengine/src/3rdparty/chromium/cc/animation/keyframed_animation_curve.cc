// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "cc/animation/keyframed_animation_curve.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/box_f.h"

namespace cc {

namespace {

template <class Keyframe>
void InsertKeyframe(scoped_ptr<Keyframe> keyframe,
                    ScopedPtrVector<Keyframe>& keyframes) {
  // Usually, the keyframes will be added in order, so this loop would be
  // unnecessary and we should skip it if possible.
  if (!keyframes.empty() && keyframe->Time() < keyframes.back()->Time()) {
    for (size_t i = 0; i < keyframes.size(); ++i) {
      if (keyframe->Time() < keyframes[i]->Time()) {
        keyframes.insert(keyframes.begin() + i, keyframe.Pass());
        return;
      }
    }
  }

  keyframes.push_back(keyframe.Pass());
}

template <class Keyframes>
float GetProgress(double t, size_t i, const Keyframes& keyframes) {
  float progress =
      static_cast<float>((t - keyframes[i]->Time()) /
                         (keyframes[i + 1]->Time() - keyframes[i]->Time()));

  if (keyframes[i]->timing_function())
    progress = keyframes[i]->timing_function()->GetValue(progress);
  return progress;
}

scoped_ptr<TimingFunction> CloneTimingFunction(
    const TimingFunction* timing_function) {
  DCHECK(timing_function);
  scoped_ptr<AnimationCurve> curve(timing_function->Clone());
  return scoped_ptr<TimingFunction>(
      static_cast<TimingFunction*>(curve.release()));
}

}  // namespace

Keyframe::Keyframe(double time, scoped_ptr<TimingFunction> timing_function)
    : time_(time),
      timing_function_(timing_function.Pass()) {}

Keyframe::~Keyframe() {}

double Keyframe::Time() const {
  return time_;
}

scoped_ptr<ColorKeyframe> ColorKeyframe::Create(
    double time,
    SkColor value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new ColorKeyframe(time, value, timing_function.Pass()));
}

ColorKeyframe::ColorKeyframe(double time,
                             SkColor value,
                             scoped_ptr<TimingFunction> timing_function)
    : Keyframe(time, timing_function.Pass()),
      value_(value) {}

ColorKeyframe::~ColorKeyframe() {}

SkColor ColorKeyframe::Value() const { return value_; }

scoped_ptr<ColorKeyframe> ColorKeyframe::Clone() const {
  scoped_ptr<TimingFunction> func;
  if (timing_function())
    func = CloneTimingFunction(timing_function());
  return ColorKeyframe::Create(Time(), Value(), func.Pass());
}

scoped_ptr<FloatKeyframe> FloatKeyframe::Create(
    double time,
    float value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new FloatKeyframe(time, value, timing_function.Pass()));
}

FloatKeyframe::FloatKeyframe(double time,
                             float value,
                             scoped_ptr<TimingFunction> timing_function)
    : Keyframe(time, timing_function.Pass()),
      value_(value) {}

FloatKeyframe::~FloatKeyframe() {}

float FloatKeyframe::Value() const {
  return value_;
}

scoped_ptr<FloatKeyframe> FloatKeyframe::Clone() const {
  scoped_ptr<TimingFunction> func;
  if (timing_function())
    func = CloneTimingFunction(timing_function());
  return FloatKeyframe::Create(Time(), Value(), func.Pass());
}

scoped_ptr<TransformKeyframe> TransformKeyframe::Create(
    double time,
    const TransformOperations& value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new TransformKeyframe(time, value, timing_function.Pass()));
}

TransformKeyframe::TransformKeyframe(double time,
                                     const TransformOperations& value,
                                     scoped_ptr<TimingFunction> timing_function)
    : Keyframe(time, timing_function.Pass()),
      value_(value) {}

TransformKeyframe::~TransformKeyframe() {}

const TransformOperations& TransformKeyframe::Value() const {
  return value_;
}

scoped_ptr<TransformKeyframe> TransformKeyframe::Clone() const {
  scoped_ptr<TimingFunction> func;
  if (timing_function())
    func = CloneTimingFunction(timing_function());
  return TransformKeyframe::Create(Time(), Value(), func.Pass());
}

scoped_ptr<FilterKeyframe> FilterKeyframe::Create(
    double time,
    const FilterOperations& value,
    scoped_ptr<TimingFunction> timing_function) {
  return make_scoped_ptr(
      new FilterKeyframe(time, value, timing_function.Pass()));
}

FilterKeyframe::FilterKeyframe(double time,
                               const FilterOperations& value,
                               scoped_ptr<TimingFunction> timing_function)
    : Keyframe(time, timing_function.Pass()),
      value_(value) {}

FilterKeyframe::~FilterKeyframe() {}

const FilterOperations& FilterKeyframe::Value() const {
  return value_;
}

scoped_ptr<FilterKeyframe> FilterKeyframe::Clone() const {
  scoped_ptr<TimingFunction> func;
  if (timing_function())
    func = CloneTimingFunction(timing_function());
  return FilterKeyframe::Create(Time(), Value(), func.Pass());
}

scoped_ptr<KeyframedColorAnimationCurve> KeyframedColorAnimationCurve::
    Create() {
  return make_scoped_ptr(new KeyframedColorAnimationCurve);
}

KeyframedColorAnimationCurve::KeyframedColorAnimationCurve() {}

KeyframedColorAnimationCurve::~KeyframedColorAnimationCurve() {}

void KeyframedColorAnimationCurve::AddKeyframe(
    scoped_ptr<ColorKeyframe> keyframe) {
  InsertKeyframe(keyframe.Pass(), keyframes_);
}

double KeyframedColorAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

scoped_ptr<AnimationCurve> KeyframedColorAnimationCurve::Clone() const {
  scoped_ptr<KeyframedColorAnimationCurve> to_return(
      KeyframedColorAnimationCurve::Create());
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());
  return to_return.PassAs<AnimationCurve>();
}

SkColor KeyframedColorAnimationCurve::GetValue(double t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value();

  size_t i = 0;
  for (; i < keyframes_.size() - 1; ++i) {
    if (t < keyframes_[i + 1]->Time())
      break;
  }

  float progress = GetProgress(t, i, keyframes_);

  return gfx::Tween::ColorValueBetween(
      progress, keyframes_[i]->Value(), keyframes_[i + 1]->Value());
}

// KeyframedFloatAnimationCurve

scoped_ptr<KeyframedFloatAnimationCurve> KeyframedFloatAnimationCurve::
    Create() {
  return make_scoped_ptr(new KeyframedFloatAnimationCurve);
}

KeyframedFloatAnimationCurve::KeyframedFloatAnimationCurve() {}

KeyframedFloatAnimationCurve::~KeyframedFloatAnimationCurve() {}

void KeyframedFloatAnimationCurve::AddKeyframe(
    scoped_ptr<FloatKeyframe> keyframe) {
  InsertKeyframe(keyframe.Pass(), keyframes_);
}

double KeyframedFloatAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

scoped_ptr<AnimationCurve> KeyframedFloatAnimationCurve::Clone() const {
  scoped_ptr<KeyframedFloatAnimationCurve> to_return(
      KeyframedFloatAnimationCurve::Create());
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());
  return to_return.PassAs<AnimationCurve>();
}

float KeyframedFloatAnimationCurve::GetValue(double t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value();

  size_t i = 0;
  for (; i < keyframes_.size() - 1; ++i) {
    if (t < keyframes_[i+1]->Time())
      break;
  }

  float progress = GetProgress(t, i, keyframes_);

  return keyframes_[i]->Value() +
      (keyframes_[i+1]->Value() - keyframes_[i]->Value()) * progress;
}

scoped_ptr<KeyframedTransformAnimationCurve> KeyframedTransformAnimationCurve::
    Create() {
  return make_scoped_ptr(new KeyframedTransformAnimationCurve);
}

KeyframedTransformAnimationCurve::KeyframedTransformAnimationCurve() {}

KeyframedTransformAnimationCurve::~KeyframedTransformAnimationCurve() {}

void KeyframedTransformAnimationCurve::AddKeyframe(
    scoped_ptr<TransformKeyframe> keyframe) {
  InsertKeyframe(keyframe.Pass(), keyframes_);
}

double KeyframedTransformAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

scoped_ptr<AnimationCurve> KeyframedTransformAnimationCurve::Clone() const {
  scoped_ptr<KeyframedTransformAnimationCurve> to_return(
      KeyframedTransformAnimationCurve::Create());
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());
  return to_return.PassAs<AnimationCurve>();
}

// Assumes that (*keyframes).front()->Time() < t < (*keyframes).back()-Time().
template<typename ValueType, typename KeyframeType>
static ValueType GetCurveValue(const ScopedPtrVector<KeyframeType>* keyframes,
                               double t) {
  size_t i = 0;
  for (; i < keyframes->size() - 1; ++i) {
    if (t < (*keyframes)[i+1]->Time())
      break;
  }

  double progress = (t - (*keyframes)[i]->Time()) /
                    ((*keyframes)[i+1]->Time() - (*keyframes)[i]->Time());

  if ((*keyframes)[i]->timing_function())
    progress = (*keyframes)[i]->timing_function()->GetValue(progress);

  return (*keyframes)[i+1]->Value().Blend((*keyframes)[i]->Value(), progress);
}

gfx::Transform KeyframedTransformAnimationCurve::GetValue(double t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value().Apply();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value().Apply();

  return GetCurveValue<gfx::Transform, TransformKeyframe>(&keyframes_, t);
}

bool KeyframedTransformAnimationCurve::AnimatedBoundsForBox(
    const gfx::BoxF& box,
    gfx::BoxF* bounds) const {
  DCHECK_GE(keyframes_.size(), 2ul);
  *bounds = gfx::BoxF();
  for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
    gfx::BoxF bounds_for_step;
    float min_progress = 0.0;
    float max_progress = 1.0;
    if (keyframes_[i]->timing_function())
      keyframes_[i]->timing_function()->Range(&min_progress, &max_progress);
    if (!keyframes_[i+1]->Value().BlendedBoundsForBox(box,
                                                      keyframes_[i]->Value(),
                                                      min_progress,
                                                      max_progress,
                                                      &bounds_for_step))
      return false;
    bounds->Union(bounds_for_step);
  }
  return true;
}

bool KeyframedTransformAnimationCurve::AffectsScale() const {
  for (size_t i = 0; i < keyframes_.size(); ++i) {
    if (keyframes_[i]->Value().AffectsScale())
      return true;
  }
  return false;
}

bool KeyframedTransformAnimationCurve::IsTranslation() const {
  for (size_t i = 0; i < keyframes_.size(); ++i) {
    if (!keyframes_[i]->Value().IsTranslation() &&
        !keyframes_[i]->Value().IsIdentity())
      return false;
  }
  return true;
}

bool KeyframedTransformAnimationCurve::MaximumScale(float* max_scale) const {
  DCHECK_GE(keyframes_.size(), 2ul);
  *max_scale = 0.f;
  for (size_t i = 1; i < keyframes_.size(); ++i) {
    float min_progress = 0.f;
    float max_progress = 1.f;
    if (keyframes_[i - 1]->timing_function())
      keyframes_[i - 1]->timing_function()->Range(&min_progress, &max_progress);

    float max_scale_for_segment = 0.f;
    if (!keyframes_[i]->Value().MaximumScale(keyframes_[i - 1]->Value(),
                                             min_progress,
                                             max_progress,
                                             &max_scale_for_segment))
      return false;

    *max_scale = std::max(*max_scale, max_scale_for_segment);
  }
  return true;
}

scoped_ptr<KeyframedFilterAnimationCurve> KeyframedFilterAnimationCurve::
    Create() {
  return make_scoped_ptr(new KeyframedFilterAnimationCurve);
}

KeyframedFilterAnimationCurve::KeyframedFilterAnimationCurve() {}

KeyframedFilterAnimationCurve::~KeyframedFilterAnimationCurve() {}

void KeyframedFilterAnimationCurve::AddKeyframe(
    scoped_ptr<FilterKeyframe> keyframe) {
  InsertKeyframe(keyframe.Pass(), keyframes_);
}

double KeyframedFilterAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

scoped_ptr<AnimationCurve> KeyframedFilterAnimationCurve::Clone() const {
  scoped_ptr<KeyframedFilterAnimationCurve> to_return(
      KeyframedFilterAnimationCurve::Create());
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());
  return to_return.PassAs<AnimationCurve>();
}

FilterOperations KeyframedFilterAnimationCurve::GetValue(double t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value();

  return GetCurveValue<FilterOperations, FilterKeyframe>(&keyframes_, t);
}

bool KeyframedFilterAnimationCurve::HasFilterThatMovesPixels() const {
  for (size_t i = 0; i < keyframes_.size(); ++i) {
    if (keyframes_[i]->Value().HasFilterThatMovesPixels()) {
      return true;
    }
  }
  return false;
}

}  // namespace cc
