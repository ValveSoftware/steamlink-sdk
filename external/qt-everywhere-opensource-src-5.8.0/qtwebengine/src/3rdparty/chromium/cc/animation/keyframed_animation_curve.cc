// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframed_animation_curve.h"

#include <stddef.h>

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "cc/base/time_util.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

namespace {

template <class KeyframeType>
void InsertKeyframe(std::unique_ptr<KeyframeType> keyframe,
                    std::vector<std::unique_ptr<KeyframeType>>* keyframes) {
  // Usually, the keyframes will be added in order, so this loop would be
  // unnecessary and we should skip it if possible.
  if (!keyframes->empty() && keyframe->Time() < keyframes->back()->Time()) {
    for (size_t i = 0; i < keyframes->size(); ++i) {
      if (keyframe->Time() < keyframes->at(i)->Time()) {
        keyframes->insert(keyframes->begin() + i, std::move(keyframe));
        return;
      }
    }
  }

  keyframes->push_back(std::move(keyframe));
}

template <typename KeyframeType>
base::TimeDelta TransformedAnimationTime(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    const std::unique_ptr<TimingFunction>& timing_function,
    base::TimeDelta time) {
  if (timing_function) {
    base::TimeDelta start_time = keyframes.front()->Time();
    base::TimeDelta duration =
        keyframes.back()->Time() - keyframes.front()->Time();
    double progress = TimeUtil::Divide(time - start_time, duration);

    time = TimeUtil::Scale(duration, timing_function->GetValue(progress)) +
           start_time;
  }

  return time;
}

template <typename KeyframeType>
size_t GetActiveKeyframe(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    base::TimeDelta time) {
  DCHECK_GE(keyframes.size(), 2ul);
  size_t i = 0;
  for (; i < keyframes.size() - 2; ++i) {  // Last keyframe is never active.
    if (time < keyframes[i + 1]->Time())
      break;
  }

  return i;
}

template <typename KeyframeType>
double TransformedKeyframeProgress(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    base::TimeDelta time,
    size_t i) {
  double progress =
      TimeUtil::Divide(time - keyframes[i]->Time(),
                       keyframes[i + 1]->Time() - keyframes[i]->Time());

  if (keyframes[i]->timing_function()) {
    progress = keyframes[i]->timing_function()->GetValue(progress);
  }

  return progress;
}

}  // namespace

Keyframe::Keyframe(base::TimeDelta time,
                   std::unique_ptr<TimingFunction> timing_function)
    : time_(time), timing_function_(std::move(timing_function)) {}

Keyframe::~Keyframe() {}

base::TimeDelta Keyframe::Time() const {
  return time_;
}

std::unique_ptr<ColorKeyframe> ColorKeyframe::Create(
    base::TimeDelta time,
    SkColor value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new ColorKeyframe(time, value, std::move(timing_function)));
}

ColorKeyframe::ColorKeyframe(base::TimeDelta time,
                             SkColor value,
                             std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

ColorKeyframe::~ColorKeyframe() {}

SkColor ColorKeyframe::Value() const { return value_; }

std::unique_ptr<ColorKeyframe> ColorKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return ColorKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<FloatKeyframe> FloatKeyframe::Create(
    base::TimeDelta time,
    float value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new FloatKeyframe(time, value, std::move(timing_function)));
}

FloatKeyframe::FloatKeyframe(base::TimeDelta time,
                             float value,
                             std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

FloatKeyframe::~FloatKeyframe() {}

float FloatKeyframe::Value() const {
  return value_;
}

std::unique_ptr<FloatKeyframe> FloatKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return FloatKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<TransformKeyframe> TransformKeyframe::Create(
    base::TimeDelta time,
    const TransformOperations& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new TransformKeyframe(time, value, std::move(timing_function)));
}

TransformKeyframe::TransformKeyframe(
    base::TimeDelta time,
    const TransformOperations& value,
    std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

TransformKeyframe::~TransformKeyframe() {}

const TransformOperations& TransformKeyframe::Value() const {
  return value_;
}

std::unique_ptr<TransformKeyframe> TransformKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return TransformKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<FilterKeyframe> FilterKeyframe::Create(
    base::TimeDelta time,
    const FilterOperations& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new FilterKeyframe(time, value, std::move(timing_function)));
}

FilterKeyframe::FilterKeyframe(base::TimeDelta time,
                               const FilterOperations& value,
                               std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

FilterKeyframe::~FilterKeyframe() {}

const FilterOperations& FilterKeyframe::Value() const {
  return value_;
}

std::unique_ptr<FilterKeyframe> FilterKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return FilterKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<KeyframedColorAnimationCurve>
KeyframedColorAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedColorAnimationCurve);
}

KeyframedColorAnimationCurve::KeyframedColorAnimationCurve() {}

KeyframedColorAnimationCurve::~KeyframedColorAnimationCurve() {}

void KeyframedColorAnimationCurve::AddKeyframe(
    std::unique_ptr<ColorKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedColorAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

std::unique_ptr<AnimationCurve> KeyframedColorAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedColorAnimationCurve> to_return =
      KeyframedColorAnimationCurve::Create();
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  return std::move(to_return);
}

SkColor KeyframedColorAnimationCurve::GetValue(base::TimeDelta t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, t);
  size_t i = GetActiveKeyframe(keyframes_, t);
  double progress = TransformedKeyframeProgress(keyframes_, t, i);

  return gfx::Tween::ColorValueBetween(
      progress, keyframes_[i]->Value(), keyframes_[i + 1]->Value());
}

// KeyframedFloatAnimationCurve

std::unique_ptr<KeyframedFloatAnimationCurve>
KeyframedFloatAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedFloatAnimationCurve);
}

KeyframedFloatAnimationCurve::KeyframedFloatAnimationCurve() {}

KeyframedFloatAnimationCurve::~KeyframedFloatAnimationCurve() {}

void KeyframedFloatAnimationCurve::AddKeyframe(
    std::unique_ptr<FloatKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedFloatAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

std::unique_ptr<AnimationCurve> KeyframedFloatAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedFloatAnimationCurve> to_return =
      KeyframedFloatAnimationCurve::Create();
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  return std::move(to_return);
}

float KeyframedFloatAnimationCurve::GetValue(base::TimeDelta t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, t);
  size_t i = GetActiveKeyframe(keyframes_, t);
  double progress = TransformedKeyframeProgress(keyframes_, t, i);

  return keyframes_[i]->Value() +
      (keyframes_[i+1]->Value() - keyframes_[i]->Value()) * progress;
}

std::unique_ptr<KeyframedTransformAnimationCurve>
KeyframedTransformAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedTransformAnimationCurve);
}

KeyframedTransformAnimationCurve::KeyframedTransformAnimationCurve() {}

KeyframedTransformAnimationCurve::~KeyframedTransformAnimationCurve() {}

void KeyframedTransformAnimationCurve::AddKeyframe(
    std::unique_ptr<TransformKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedTransformAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

std::unique_ptr<AnimationCurve> KeyframedTransformAnimationCurve::Clone()
    const {
  std::unique_ptr<KeyframedTransformAnimationCurve> to_return =
      KeyframedTransformAnimationCurve::Create();
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  return std::move(to_return);
}

gfx::Transform KeyframedTransformAnimationCurve::GetValue(
    base::TimeDelta t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value().Apply();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value().Apply();

  t = TransformedAnimationTime(keyframes_, timing_function_, t);
  size_t i = GetActiveKeyframe(keyframes_, t);
  double progress = TransformedKeyframeProgress(keyframes_, t, i);

  return keyframes_[i + 1]->Value().Blend(keyframes_[i]->Value(), progress);
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

bool KeyframedTransformAnimationCurve::PreservesAxisAlignment() const {
  for (size_t i = 0; i < keyframes_.size(); ++i) {
    if (!keyframes_[i]->Value().PreservesAxisAlignment())
      return false;
  }
  return true;
}

bool KeyframedTransformAnimationCurve::IsTranslation() const {
  for (size_t i = 0; i < keyframes_.size(); ++i) {
    if (!keyframes_[i]->Value().IsTranslation() &&
        !keyframes_[i]->Value().IsIdentity())
      return false;
  }
  return true;
}
bool KeyframedTransformAnimationCurve::AnimationStartScale(
    bool forward_direction,
    float* start_scale) const {
  DCHECK_GE(keyframes_.size(), 2ul);
  *start_scale = 0.f;
  size_t start_location = 0;
  if (!forward_direction) {
    start_location = keyframes_.size() - 1;
  }

  gfx::Vector3dF initial_target_scale;
  if (!keyframes_[start_location]->Value().ScaleComponent(
          &initial_target_scale))
    return false;
  float start_scale_for_segment =
      fmax(std::abs(initial_target_scale.x()),
           fmax(std::abs(initial_target_scale.y()),
                std::abs(initial_target_scale.z())));
  *start_scale = start_scale_for_segment;
  return true;
}
bool KeyframedTransformAnimationCurve::MaximumTargetScale(
    bool forward_direction,
    float* max_scale) const {
  DCHECK_GE(keyframes_.size(), 2ul);
  *max_scale = 0.f;

  // If |forward_direction| is true, then skip the first frame, otherwise
  // skip the last frame, since that is the original position in the animation.
  size_t start = 1;
  size_t end = keyframes_.size();
  if (!forward_direction) {
    --start;
    --end;
  }

  for (size_t i = start; i < end; ++i) {
    gfx::Vector3dF target_scale_for_segment;
    if (!keyframes_[i]->Value().ScaleComponent(&target_scale_for_segment))
      return false;
    float max_scale_for_segment =
        fmax(std::abs(target_scale_for_segment.x()),
             fmax(std::abs(target_scale_for_segment.y()),
                  std::abs(target_scale_for_segment.z())));
    *max_scale = fmax(*max_scale, max_scale_for_segment);
  }
  return true;
}

std::unique_ptr<KeyframedFilterAnimationCurve>
KeyframedFilterAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedFilterAnimationCurve);
}

KeyframedFilterAnimationCurve::KeyframedFilterAnimationCurve() {}

KeyframedFilterAnimationCurve::~KeyframedFilterAnimationCurve() {}

void KeyframedFilterAnimationCurve::AddKeyframe(
    std::unique_ptr<FilterKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedFilterAnimationCurve::Duration() const {
  return keyframes_.back()->Time() - keyframes_.front()->Time();
}

std::unique_ptr<AnimationCurve> KeyframedFilterAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedFilterAnimationCurve> to_return =
      KeyframedFilterAnimationCurve::Create();
  for (size_t i = 0; i < keyframes_.size(); ++i)
    to_return->AddKeyframe(keyframes_[i]->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  return std::move(to_return);
}

FilterOperations KeyframedFilterAnimationCurve::GetValue(
    base::TimeDelta t) const {
  if (t <= keyframes_.front()->Time())
    return keyframes_.front()->Value();

  if (t >= keyframes_.back()->Time())
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, t);
  size_t i = GetActiveKeyframe(keyframes_, t);
  double progress = TransformedKeyframeProgress(keyframes_, t, i);

  return keyframes_[i + 1]->Value().Blend(keyframes_[i]->Value(), progress);
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
