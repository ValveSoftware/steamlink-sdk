// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationEffectTimingReadOnly.h"

#include "bindings/core/v8/UnrestrictedDoubleOrString.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/KeyframeEffect.h"
#include "platform/animation/TimingFunction.h"

namespace blink {

AnimationEffectTimingReadOnly* AnimationEffectTimingReadOnly::create(
    AnimationEffectReadOnly* parent) {
  return new AnimationEffectTimingReadOnly(parent);
}

AnimationEffectTimingReadOnly::AnimationEffectTimingReadOnly(
    AnimationEffectReadOnly* parent)
    : m_parent(parent) {}

double AnimationEffectTimingReadOnly::delay() {
  return m_parent->specifiedTiming().startDelay * 1000;
}

double AnimationEffectTimingReadOnly::endDelay() {
  return m_parent->specifiedTiming().endDelay * 1000;
}

String AnimationEffectTimingReadOnly::fill() {
  return Timing::fillModeString(m_parent->specifiedTiming().fillMode);
}

double AnimationEffectTimingReadOnly::iterationStart() {
  return m_parent->specifiedTiming().iterationStart;
}

double AnimationEffectTimingReadOnly::iterations() {
  return m_parent->specifiedTiming().iterationCount;
}

void AnimationEffectTimingReadOnly::duration(
    UnrestrictedDoubleOrString& returnValue) {
  if (std::isnan(m_parent->specifiedTiming().iterationDuration)) {
    returnValue.setString("auto");
  } else {
    returnValue.setUnrestrictedDouble(
        m_parent->specifiedTiming().iterationDuration * 1000);
  }
}

double AnimationEffectTimingReadOnly::playbackRate() {
  return m_parent->specifiedTiming().playbackRate;
}

String AnimationEffectTimingReadOnly::direction() {
  return Timing::playbackDirectionString(m_parent->specifiedTiming().direction);
}

String AnimationEffectTimingReadOnly::easing() {
  return m_parent->specifiedTiming().timingFunction->toString();
}

DEFINE_TRACE(AnimationEffectTimingReadOnly) {
  visitor->trace(m_parent);
}

}  // namespace blink
