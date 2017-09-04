// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimation.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFilterAnimationCurve.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/animation/CompositorTransformAnimationCurve.h"
#include <memory>

using cc::Animation;
using cc::AnimationIdProvider;

using blink::CompositorAnimation;
using blink::CompositorAnimationCurve;

namespace blink {

CompositorAnimation::CompositorAnimation(
    const CompositorAnimationCurve& curve,
    CompositorTargetProperty::Type targetProperty,
    int animationId,
    int groupId) {
  if (!animationId)
    animationId = AnimationIdProvider::NextAnimationId();
  if (!groupId)
    groupId = AnimationIdProvider::NextGroupId();

  m_animation = Animation::Create(curve.cloneToAnimationCurve(), animationId,
                                  groupId, targetProperty);
}

CompositorAnimation::~CompositorAnimation() {}

int CompositorAnimation::id() const {
  return m_animation->id();
}

int CompositorAnimation::group() const {
  return m_animation->group();
}

CompositorTargetProperty::Type CompositorAnimation::targetProperty() const {
  return m_animation->target_property();
}

double CompositorAnimation::iterations() const {
  return m_animation->iterations();
}

void CompositorAnimation::setIterations(double n) {
  m_animation->set_iterations(n);
}

double CompositorAnimation::iterationStart() const {
  return m_animation->iteration_start();
}

void CompositorAnimation::setIterationStart(double iterationStart) {
  m_animation->set_iteration_start(iterationStart);
}

double CompositorAnimation::startTime() const {
  return (m_animation->start_time() - base::TimeTicks()).InSecondsF();
}

void CompositorAnimation::setStartTime(double monotonicTime) {
  m_animation->set_start_time(base::TimeTicks::FromInternalValue(
      monotonicTime * base::Time::kMicrosecondsPerSecond));
}

double CompositorAnimation::timeOffset() const {
  return m_animation->time_offset().InSecondsF();
}

void CompositorAnimation::setTimeOffset(double monotonicTime) {
  m_animation->set_time_offset(base::TimeDelta::FromSecondsD(monotonicTime));
}

blink::CompositorAnimation::Direction CompositorAnimation::getDirection()
    const {
  return m_animation->direction();
}

void CompositorAnimation::setDirection(Direction direction) {
  m_animation->set_direction(direction);
}

double CompositorAnimation::playbackRate() const {
  return m_animation->playback_rate();
}

void CompositorAnimation::setPlaybackRate(double playbackRate) {
  m_animation->set_playback_rate(playbackRate);
}

blink::CompositorAnimation::FillMode CompositorAnimation::getFillMode() const {
  return m_animation->fill_mode();
}

void CompositorAnimation::setFillMode(FillMode fillMode) {
  m_animation->set_fill_mode(fillMode);
}

std::unique_ptr<cc::Animation> CompositorAnimation::releaseCcAnimation() {
  m_animation->set_needs_synchronized_start_time(true);
  return std::move(m_animation);
}

std::unique_ptr<CompositorFloatAnimationCurve>
CompositorAnimation::floatCurveForTesting() const {
  const cc::AnimationCurve* curve = m_animation->curve();
  DCHECK_EQ(cc::AnimationCurve::FLOAT, curve->Type());

  auto keyframedCurve = base::WrapUnique(
      static_cast<cc::KeyframedFloatAnimationCurve*>(curve->Clone().release()));
  return CompositorFloatAnimationCurve::createForTesting(
      std::move(keyframedCurve));
}

}  // namespace blink
