// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/AnimationPlaybackEvent.h"

namespace blink {

AnimationPlaybackEvent::AnimationPlaybackEvent(const AtomicString& type,
                                               double currentTime,
                                               double timelineTime)
    : Event(type, false, false),
      m_currentTime(currentTime),
      m_timelineTime(timelineTime) {}

AnimationPlaybackEvent::AnimationPlaybackEvent(
    const AtomicString& type,
    const AnimationPlaybackEventInit& initializer)
    : Event(type, initializer), m_currentTime(0.0), m_timelineTime(0.0) {
  if (initializer.hasCurrentTime())
    m_currentTime = initializer.currentTime();
  if (initializer.hasTimelineTime())
    m_timelineTime = initializer.timelineTime();
}

AnimationPlaybackEvent::~AnimationPlaybackEvent() {}

double AnimationPlaybackEvent::currentTime(bool& isNull) const {
  double result = currentTime();
  isNull = std::isnan(result);
  return result;
}

double AnimationPlaybackEvent::currentTime() const {
  return m_currentTime;
}

double AnimationPlaybackEvent::timelineTime(bool& isNull) const {
  double result = timelineTime();
  isNull = std::isnan(result);
  return result;
}

double AnimationPlaybackEvent::timelineTime() const {
  return m_timelineTime;
}

const AtomicString& AnimationPlaybackEvent::interfaceName() const {
  return EventNames::AnimationPlaybackEvent;
}

DEFINE_TRACE(AnimationPlaybackEvent) {
  Event::trace(visitor);
}

}  // namespace blink
