// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationPlayerEvent_h
#define AnimationPlayerEvent_h

#include "core/events/AnimationPlayerEventInit.h"
#include "core/events/Event.h"

namespace blink {

class AnimationPlayerEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static AnimationPlayerEvent* create()
    {
        return new AnimationPlayerEvent;
    }
    static AnimationPlayerEvent* create(const AtomicString& type, double currentTime, double timelineTime)
    {
        return new AnimationPlayerEvent(type, currentTime, timelineTime);
    }
    static AnimationPlayerEvent* create(const AtomicString& type, const AnimationPlayerEventInit& initializer)
    {
        return new AnimationPlayerEvent(type, initializer);
    }

    ~AnimationPlayerEvent() override;

    double currentTime(bool& isNull) const;
    double currentTime() const;
    double timelineTime() const;

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    AnimationPlayerEvent();
    AnimationPlayerEvent(const AtomicString& type, double currentTime, double timelineTime);
    AnimationPlayerEvent(const AtomicString&, const AnimationPlayerEventInit&);

    double m_currentTime;
    double m_timelineTime;
};

} // namespace blink

#endif // AnimationPlayerEvent_h
