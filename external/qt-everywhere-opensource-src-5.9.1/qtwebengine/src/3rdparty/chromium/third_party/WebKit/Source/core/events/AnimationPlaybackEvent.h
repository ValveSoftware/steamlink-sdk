// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationPlaybackEvent_h
#define AnimationPlaybackEvent_h

#include "core/events/AnimationPlaybackEventInit.h"
#include "core/events/Event.h"

namespace blink {

class AnimationPlaybackEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationPlaybackEvent* create(const AtomicString& type,
                                        double currentTime,
                                        double timelineTime) {
    return new AnimationPlaybackEvent(type, currentTime, timelineTime);
  }
  static AnimationPlaybackEvent* create(
      const AtomicString& type,
      const AnimationPlaybackEventInit& initializer) {
    return new AnimationPlaybackEvent(type, initializer);
  }

  ~AnimationPlaybackEvent() override;

  double currentTime(bool& isNull) const;
  double currentTime() const;
  double timelineTime(bool& isNull) const;
  double timelineTime() const;

  const AtomicString& interfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  AnimationPlaybackEvent(const AtomicString& type,
                         double currentTime,
                         double timelineTime);
  AnimationPlaybackEvent(const AtomicString&,
                         const AnimationPlaybackEventInit&);

  double m_currentTime;
  double m_timelineTime;
};

}  // namespace blink

#endif  // AnimationPlaybackEvent_h
