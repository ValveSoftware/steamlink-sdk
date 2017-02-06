/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef PlatformTouchEvent_h
#define PlatformTouchEvent_h

#include "platform/PlatformEvent.h"
#include "platform/PlatformTouchPoint.h"
#include "wtf/Vector.h"

namespace blink {

class PlatformTouchEvent : public PlatformEvent {
public:
    PlatformTouchEvent()
        : PlatformEvent(PlatformEvent::TouchStart)
        , m_dispatchType(PlatformEvent::Blocking)
        , m_causesScrollingIfUncanceled(false)
        , m_dispatchedDuringFling(false)
    {
    }

    const Vector<PlatformTouchPoint>& touchPoints() const { return m_touchPoints; }

    DispatchType dispatchType() const { return m_dispatchType; }
    bool cancelable() const { return m_dispatchType == PlatformEvent::Blocking; }
    bool causesScrollingIfUncanceled() const { return m_causesScrollingIfUncanceled; }
    bool dispatchedDuringFling() const { return m_dispatchedDuringFling; }

    uint32_t uniqueTouchEventId() const { return m_uniqueTouchEventId; }

protected:
    Vector<PlatformTouchPoint> m_touchPoints;
    DispatchType m_dispatchType;
    bool m_causesScrollingIfUncanceled;
    bool m_dispatchedDuringFling;

    uint32_t m_uniqueTouchEventId;
};

} // namespace blink

#endif // PlatformTouchEvent_h
