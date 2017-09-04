// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationEffectTiming.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/UnrestrictedDoubleOrString.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/AnimationEffectTimingReadOnly.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/TimingInput.h"
#include "platform/animation/TimingFunction.h"

namespace blink {

AnimationEffectTiming* AnimationEffectTiming::create(
    AnimationEffectReadOnly* parent) {
  return new AnimationEffectTiming(parent);
}

AnimationEffectTiming::AnimationEffectTiming(AnimationEffectReadOnly* parent)
    : AnimationEffectTimingReadOnly(parent) {}

void AnimationEffectTiming::setDelay(double delay) {
  Timing timing = m_parent->specifiedTiming();
  TimingInput::setStartDelay(timing, delay);
  m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setEndDelay(double endDelay) {
  Timing timing = m_parent->specifiedTiming();
  TimingInput::setEndDelay(timing, endDelay);
  m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setFill(String fill) {
  Timing timing = m_parent->specifiedTiming();
  TimingInput::setFillMode(timing, fill);
  m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setIterationStart(double iterationStart,
                                              ExceptionState& exceptionState) {
  Timing timing = m_parent->specifiedTiming();
  if (TimingInput::setIterationStart(timing, iterationStart, exceptionState))
    m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setIterations(double iterations,
                                          ExceptionState& exceptionState) {
  Timing timing = m_parent->specifiedTiming();
  if (TimingInput::setIterationCount(timing, iterations, exceptionState))
    m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setDuration(
    const UnrestrictedDoubleOrString& duration,
    ExceptionState& exceptionState) {
  Timing timing = m_parent->specifiedTiming();
  if (TimingInput::setIterationDuration(timing, duration, exceptionState))
    m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setPlaybackRate(double playbackRate) {
  Timing timing = m_parent->specifiedTiming();
  TimingInput::setPlaybackRate(timing, playbackRate);
  m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setDirection(String direction) {
  Timing timing = m_parent->specifiedTiming();
  TimingInput::setPlaybackDirection(timing, direction);
  m_parent->updateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setEasing(String easing,
                                      ExceptionState& exceptionState) {
  Timing timing = m_parent->specifiedTiming();
  // The AnimationEffectTiming might not be attached to a document at this
  // point, so we pass nullptr in to setTimingFunction. This means that these
  // calls are not considered in the WebAnimationsEasingAsFunction*
  // UseCounters, but the bug we are tracking there does not come through
  // this interface.
  if (TimingInput::setTimingFunction(timing, easing, nullptr, exceptionState))
    m_parent->updateSpecifiedTiming(timing);
}

DEFINE_TRACE(AnimationEffectTiming) {
  AnimationEffectTimingReadOnly::trace(visitor);
}

}  // namespace blink
