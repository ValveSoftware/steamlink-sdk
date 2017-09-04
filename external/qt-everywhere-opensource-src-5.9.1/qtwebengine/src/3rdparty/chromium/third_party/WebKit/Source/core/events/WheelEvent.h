/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WheelEvent_h
#define WheelEvent_h

#include "core/CoreExport.h"
#include "core/events/MouseEvent.h"
#include "core/events/WheelEventInit.h"
#include "platform/geometry/FloatPoint.h"

namespace blink {

class PlatformWheelEvent;

#if OS(MACOSX)
enum WheelEventPhase {
  WheelEventPhaseNone = 0,
  WheelEventPhaseBegan = 1 << 0,
  WheelEventPhaseStationary = 1 << 1,
  WheelEventPhaseChanged = 1 << 2,
  WheelEventPhaseEnded = 1 << 3,
  WheelEventPhaseCancelled = 1 << 4,
  WheelEventPhaseMayBegin = 1 << 5,
};
#endif

class CORE_EXPORT WheelEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum { TickMultiplier = 120 };

  enum DeltaMode { kDomDeltaPixel = 0, kDomDeltaLine, kDomDeltaPage };

  static WheelEvent* create() { return new WheelEvent; }

  static WheelEvent* create(const PlatformWheelEvent& platformEvent,
                            AbstractView*);

  static WheelEvent* create(const AtomicString& type,
                            const WheelEventInit& initializer) {
    return new WheelEvent(type, initializer);
  }

  static WheelEvent* create(const FloatPoint& wheelTicks,
                            const FloatPoint& rawDelta,
                            unsigned deltaMode,
                            AbstractView* view,
                            const IntPoint& screenLocation,
                            const IntPoint& windowLocation,
                            PlatformEvent::Modifiers modifiers,
                            unsigned short buttons,
                            double platformTimeStamp,
                            int resendingPluginId,
                            bool hasPreciseScrollingDeltas,
                            RailsMode railsMode,
                            bool cancelable
#if OS(MACOSX)
                            ,
                            WheelEventPhase phase,
                            WheelEventPhase momentumPhase
#endif
                            ) {
    return new WheelEvent(wheelTicks, rawDelta, deltaMode, view, screenLocation,
                          windowLocation, modifiers, buttons, platformTimeStamp,
                          resendingPluginId, hasPreciseScrollingDeltas,
                          railsMode, cancelable
#if OS(MACOSX)
                          ,
                          phase, momentumPhase
#endif
                          );
  }

  double deltaX() const { return m_deltaX; }  // Positive when scrolling right.
  double deltaY() const { return m_deltaY; }  // Positive when scrolling down.
  double deltaZ() const { return m_deltaZ; }
  int wheelDelta() const {
    return wheelDeltaY() ? wheelDeltaY() : wheelDeltaX();
  }  // Deprecated.
  int wheelDeltaX() const {
    return m_wheelDelta.x();
  }  // Deprecated, negative when scrolling right.
  int wheelDeltaY() const {
    return m_wheelDelta.y();
  }  // Deprecated, negative when scrolling down.
  unsigned deltaMode() const { return m_deltaMode; }
  float ticksX() const {
    return static_cast<float>(m_wheelDelta.x()) / TickMultiplier;
  }
  float ticksY() const {
    return static_cast<float>(m_wheelDelta.y()) / TickMultiplier;
  }
  int resendingPluginId() const { return m_resendingPluginId; }
  bool hasPreciseScrollingDeltas() const { return m_hasPreciseScrollingDeltas; }
  RailsMode getRailsMode() const { return m_railsMode; }

  const AtomicString& interfaceName() const override;
  bool isMouseEvent() const override;
  bool isWheelEvent() const override;

  EventDispatchMediator* createMediator() override;

#if OS(MACOSX)
  WheelEventPhase phase() const { return m_phase; }
  WheelEventPhase momentumPhase() const { return m_momentumPhase; }
#endif

  DECLARE_VIRTUAL_TRACE();

 private:
  WheelEvent();
  WheelEvent(const AtomicString&, const WheelEventInit&);
  WheelEvent(const FloatPoint& wheelTicks,
             const FloatPoint& rawDelta,
             unsigned,
             AbstractView*,
             const IntPoint& screenLocation,
             const IntPoint& windowLocation,
             PlatformEvent::Modifiers,
             unsigned short buttons,
             double platformTimeStamp,
             int resendingPluginId,
             bool hasPreciseScrollingDeltas,
             RailsMode,
             bool cancelable);
#if OS(MACOSX)
  WheelEvent(const FloatPoint& wheelTicks,
             const FloatPoint& rawDelta,
             unsigned,
             AbstractView*,
             const IntPoint& screenLocation,
             const IntPoint& windowLocation,
             PlatformEvent::Modifiers,
             unsigned short buttons,
             double platformTimeStamp,
             int resendingPluginId,
             bool hasPreciseScrollingDeltas,
             RailsMode,
             bool cancelable,
             WheelEventPhase phase,
             WheelEventPhase momentumPhase);
#endif

  IntPoint m_wheelDelta;
  double m_deltaX;
  double m_deltaY;
  double m_deltaZ;
  unsigned m_deltaMode;
  int m_resendingPluginId;
  bool m_hasPreciseScrollingDeltas;
  RailsMode m_railsMode;
#if OS(MACOSX)
  WheelEventPhase m_phase;
  WheelEventPhase m_momentumPhase;
#endif
};

DEFINE_EVENT_TYPE_CASTS(WheelEvent);

}  // namespace blink

#endif  // WheelEvent_h
