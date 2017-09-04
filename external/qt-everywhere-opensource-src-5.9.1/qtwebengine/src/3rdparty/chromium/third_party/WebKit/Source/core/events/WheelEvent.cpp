/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
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
 */

#include "core/events/WheelEvent.h"

#include "core/clipboard/DataTransfer.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformWheelEvent.h"

namespace blink {

inline static unsigned convertDeltaMode(const PlatformWheelEvent& event) {
  return event.granularity() == ScrollByPageWheelEvent
             ? WheelEvent::kDomDeltaPage
             : WheelEvent::kDomDeltaPixel;
}

// Negate a long value without integer overflow.
inline static long negateIfPossible(long value) {
  if (value == LONG_MIN)
    return value;
  return -value;
}

WheelEvent* WheelEvent::create(const PlatformWheelEvent& event,
                               AbstractView* view) {
  return new WheelEvent(
      FloatPoint(event.wheelTicksX(), event.wheelTicksY()),
      FloatPoint(event.deltaX(), event.deltaY()), convertDeltaMode(event), view,
      event.globalPosition(), event.position(), event.getModifiers(),
      MouseEvent::platformModifiersToButtons(event.getModifiers()),
      event.timestamp(), event.resendingPluginId(),
      event.hasPreciseScrollingDeltas(),
      static_cast<Event::RailsMode>(event.getRailsMode()), event.cancelable()
#if OS(MACOSX)
                                                               ,
      static_cast<WheelEventPhase>(event.phase()),
      static_cast<WheelEventPhase>(event.momentumPhase())
#endif
          );
}

WheelEvent::WheelEvent()
    : m_deltaX(0),
      m_deltaY(0),
      m_deltaZ(0),
      m_deltaMode(kDomDeltaPixel),
      m_resendingPluginId(-1),
      m_hasPreciseScrollingDeltas(false),
      m_railsMode(RailsModeFree) {}

WheelEvent::WheelEvent(const AtomicString& type,
                       const WheelEventInit& initializer)
    : MouseEvent(type, initializer),
      m_wheelDelta(initializer.wheelDeltaX() ? initializer.wheelDeltaX()
                                             : -initializer.deltaX(),
                   initializer.wheelDeltaY() ? initializer.wheelDeltaY()
                                             : -initializer.deltaY()),
      m_deltaX(initializer.deltaX()
                   ? initializer.deltaX()
                   : negateIfPossible(initializer.wheelDeltaX())),
      m_deltaY(initializer.deltaY()
                   ? initializer.deltaY()
                   : negateIfPossible(initializer.wheelDeltaY())),
      m_deltaZ(initializer.deltaZ()),
      m_deltaMode(initializer.deltaMode()),
      m_resendingPluginId(-1),
      m_hasPreciseScrollingDeltas(false),
      m_railsMode(RailsModeFree)
#if OS(MACOSX)
      ,
      m_phase(WheelEventPhaseNone),
      m_momentumPhase(WheelEventPhaseNone)
#endif
{
}

WheelEvent::WheelEvent(const FloatPoint& wheelTicks,
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
                       bool cancelable)
    : MouseEvent(EventTypeNames::wheel,
                 true,
                 cancelable,
                 view,
                 0,
                 screenLocation.x(),
                 screenLocation.y(),
                 windowLocation.x(),
                 windowLocation.y(),
                 0,
                 0,
                 modifiers,
                 0,
                 buttons,
                 nullptr,
                 platformTimeStamp,
                 PlatformMouseEvent::RealOrIndistinguishable,
                 // TODO(zino): Should support canvas hit region because the
                 // wheel event is a kind of mouse event. Please see
                 // http://crbug.com/594075
                 String(),
                 nullptr),
      m_wheelDelta(wheelTicks.x() * TickMultiplier,
                   wheelTicks.y() * TickMultiplier),
      m_deltaX(-rawDelta.x()),
      m_deltaY(-rawDelta.y()),
      m_deltaZ(0),
      m_deltaMode(deltaMode),
      m_resendingPluginId(resendingPluginId),
      m_hasPreciseScrollingDeltas(hasPreciseScrollingDeltas),
      m_railsMode(railsMode)
#if OS(MACOSX)
      ,
      m_phase(WheelEventPhaseNone),
      m_momentumPhase(WheelEventPhaseNone)
#endif
{
}

#if OS(MACOSX)
WheelEvent::WheelEvent(const FloatPoint& wheelTicks,
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
                       bool cancelable,
                       WheelEventPhase phase,
                       WheelEventPhase momentumPhase)
    : MouseEvent(EventTypeNames::wheel,
                 true,
                 cancelable,
                 view,
                 0,
                 screenLocation.x(),
                 screenLocation.y(),
                 windowLocation.x(),
                 windowLocation.y(),
                 0,
                 0,
                 modifiers,
                 0,
                 buttons,
                 nullptr,
                 platformTimeStamp,
                 PlatformMouseEvent::RealOrIndistinguishable,
                 // TODO(zino): Should support canvas hit region because the
                 // wheel event is a kind of mouse event. Please see
                 // http://crbug.com/594075
                 String(),
                 nullptr),
      m_wheelDelta(wheelTicks.x() * TickMultiplier,
                   wheelTicks.y() * TickMultiplier),
      m_deltaX(-rawDelta.x()),
      m_deltaY(-rawDelta.y()),
      m_deltaZ(0),
      m_deltaMode(deltaMode),
      m_resendingPluginId(resendingPluginId),
      m_hasPreciseScrollingDeltas(hasPreciseScrollingDeltas),
      m_railsMode(railsMode),
      m_phase(phase),
      m_momentumPhase(momentumPhase) {}
#endif

const AtomicString& WheelEvent::interfaceName() const {
  return EventNames::WheelEvent;
}

bool WheelEvent::isMouseEvent() const {
  return false;
}

bool WheelEvent::isWheelEvent() const {
  return true;
}

EventDispatchMediator* WheelEvent::createMediator() {
  return EventDispatchMediator::create(this);
}

DEFINE_TRACE(WheelEvent) {
  MouseEvent::trace(visitor);
}

}  // namespace blink
