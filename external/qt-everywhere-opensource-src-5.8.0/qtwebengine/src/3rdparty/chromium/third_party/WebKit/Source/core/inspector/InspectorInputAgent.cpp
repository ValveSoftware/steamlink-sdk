/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorInputAgent.h"

#include "core/frame/FrameView.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectedFrames.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/PlatformEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformTouchPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/CurrentTime.h"

namespace {

enum Modifiers {
    AltKey = 1 << 0,
    CtrlKey = 1 << 1,
    MetaKey = 1 << 2,
    ShiftKey = 1 << 3
};

unsigned GetEventModifiers(int modifiers)
{
    unsigned platformModifiers = 0;
    if (modifiers & AltKey)
        platformModifiers |= blink::PlatformEvent::AltKey;
    if (modifiers & CtrlKey)
        platformModifiers |= blink::PlatformEvent::CtrlKey;
    if (modifiers & MetaKey)
        platformModifiers |= blink::PlatformEvent::MetaKey;
    if (modifiers & ShiftKey)
        platformModifiers |= blink::PlatformEvent::ShiftKey;
    return platformModifiers;
}

// Convert given protocol timestamp which is in seconds since unix epoch to a
// platform event timestamp which is ticks since platform start. This conversion
// is an estimate because these two clocks respond differently to user setting
// time and NTP adjustments. If timestamp is empty then returns current
// monotonic timestamp.
double GetEventTimeStamp(const blink::protocol::Maybe<double>& timestamp)
{
    // Take a snapshot of difference between two clocks on first run and use it
    // for the duration of the application.
    static double epochToMonotonicTimeDelta = currentTime() - monotonicallyIncreasingTime();
    if (timestamp.isJust())
        return timestamp.fromJust() - epochToMonotonicTimeDelta;

    return monotonicallyIncreasingTime();
}

class SyntheticInspectorTouchPoint : public blink::PlatformTouchPoint {
public:
    SyntheticInspectorTouchPoint(int id, TouchState state, const blink::IntPoint& screenPos, const blink::IntPoint& pos, int radiusX, int radiusY, double rotationAngle, double force)
    {
        m_pointerProperties.id = id;
        m_screenPos = screenPos;
        m_pos = pos;
        m_state = state;
        m_radius = blink::FloatSize(radiusX, radiusY);
        m_rotationAngle = rotationAngle;
        m_pointerProperties.force = force;
    }
};

class SyntheticInspectorTouchEvent : public blink::PlatformTouchEvent {
public:
    SyntheticInspectorTouchEvent(const blink::PlatformEvent::EventType type, unsigned modifiers, double timestamp)
    {
        m_type = type;
        m_modifiers = modifiers;
        m_timestamp = timestamp;
    }

    void append(const blink::PlatformTouchPoint& point)
    {
        m_touchPoints.append(point);
    }
};

void ConvertInspectorPoint(blink::LocalFrame* frame, const blink::IntPoint& pointInFrame, blink::IntPoint* convertedPoint, blink::IntPoint* globalPoint)
{
    *convertedPoint = frame->view()->convertToRootFrame(pointInFrame);
    *globalPoint = frame->page()->chromeClient().viewportToScreen(blink::IntRect(pointInFrame, blink::IntSize(0, 0)), frame->view()).location();
}

} // namespace

namespace blink {

InspectorInputAgent::InspectorInputAgent(InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames)
{
}

InspectorInputAgent::~InspectorInputAgent()
{
}

void InspectorInputAgent::dispatchTouchEvent(ErrorString* error, const String& type, std::unique_ptr<protocol::Array<protocol::Input::TouchPoint>> touchPoints, const protocol::Maybe<int>& modifiers, const protocol::Maybe<double>& timestamp)
{
    PlatformEvent::EventType convertedType;
    if (type == "touchStart") {
        convertedType = PlatformEvent::TouchStart;
    } else if (type == "touchEnd") {
        convertedType = PlatformEvent::TouchEnd;
    } else if (type == "touchMove") {
        convertedType = PlatformEvent::TouchMove;
    } else {
        *error = String("Unrecognized type: " + type);
        return;
    }

    unsigned convertedModifiers = GetEventModifiers(modifiers.fromMaybe(0));

    SyntheticInspectorTouchEvent event(convertedType, convertedModifiers, GetEventTimeStamp(timestamp));

    int autoId = 0;
    for (size_t i = 0; i < touchPoints->length(); ++i) {
        protocol::Input::TouchPoint* point = touchPoints->get(i);
        int radiusX = point->getRadiusX(1);
        int radiusY = point->getRadiusY(1);
        double rotationAngle = point->getRotationAngle(0.0);
        double force = point->getForce(1.0);
        int id;
        if (point->hasId()) {
            if (autoId > 0)
                id = -1;
            else
                id = point->getId(0);
            autoId = -1;
        } else {
            id = autoId++;
        }
        if (id < 0) {
            *error = "All or none of the provided TouchPoints must supply positive integer ids.";
            return;
        }

        PlatformTouchPoint::TouchState convertedState;
        String state = point->getState();
        if (state == "touchPressed") {
            convertedState = PlatformTouchPoint::TouchPressed;
        } else if (state == "touchReleased") {
            convertedState = PlatformTouchPoint::TouchReleased;
        } else if (state == "touchMoved") {
            convertedState = PlatformTouchPoint::TouchMoved;
        } else if (state == "touchStationary") {
            convertedState = PlatformTouchPoint::TouchStationary;
        } else if (state == "touchCancelled") {
            convertedState = PlatformTouchPoint::TouchCancelled;
        } else {
            *error = String("Unrecognized state: " + state);
            return;
        }

        // Some platforms may have flipped coordinate systems, but the given coordinates
        // assume the origin is in the top-left of the window. Convert.
        IntPoint convertedPoint, globalPoint;
        ConvertInspectorPoint(m_inspectedFrames->root(), IntPoint(point->getX(), point->getY()), &convertedPoint, &globalPoint);

        SyntheticInspectorTouchPoint touchPoint(id++, convertedState, globalPoint, convertedPoint, radiusX, radiusY, rotationAngle, force);
        event.append(touchPoint);
    }

    m_inspectedFrames->root()->eventHandler().handleTouchEvent(event);
}

DEFINE_TRACE(InspectorInputAgent)
{
    visitor->trace(m_inspectedFrames);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
