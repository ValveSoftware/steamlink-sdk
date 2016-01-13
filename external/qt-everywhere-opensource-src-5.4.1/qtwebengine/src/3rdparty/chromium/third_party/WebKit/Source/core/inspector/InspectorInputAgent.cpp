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

#include "config.h"
#include "core/inspector/InspectorInputAgent.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorClient.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/page/Page.h"
#include "platform/JSONValues.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformTouchPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "wtf/CurrentTime.h"

namespace {

class SyntheticInspectorTouchPoint : public WebCore::PlatformTouchPoint {
public:
    SyntheticInspectorTouchPoint(unsigned id, State state, const WebCore::IntPoint& screenPos, const WebCore::IntPoint& pos, int radiusX, int radiusY, double rotationAngle, double force)
    {
        m_id = id;
        m_screenPos = screenPos;
        m_pos = pos;
        m_state = state;
        m_radius = WebCore::FloatSize(radiusX, radiusY);
        m_rotationAngle = rotationAngle;
        m_force = force;
    }
};

class SyntheticInspectorTouchEvent : public WebCore::PlatformTouchEvent {
public:
    SyntheticInspectorTouchEvent(const WebCore::PlatformEvent::Type type, unsigned modifiers, double timestamp)
    {
        m_type = type;
        m_modifiers = modifiers;
        m_timestamp = timestamp;
    }

    void append(const WebCore::PlatformTouchPoint& point)
    {
        m_touchPoints.append(point);
    }
};

void ConvertInspectorPoint(WebCore::Page* page, const WebCore::IntPoint& point, WebCore::IntPoint* convertedPoint, WebCore::IntPoint* globalPoint)
{
    *convertedPoint = page->deprecatedLocalMainFrame()->view()->convertToContainingWindow(point);
    *globalPoint = page->chrome().rootViewToScreen(WebCore::IntRect(point, WebCore::IntSize(0, 0))).location();
}

} // namespace

namespace WebCore {

InspectorInputAgent::InspectorInputAgent(Page* page, InspectorClient* client)
    : InspectorBaseAgent<InspectorInputAgent>("Input")
    , m_page(page), m_client(client)
{
}

InspectorInputAgent::~InspectorInputAgent()
{
}

void InspectorInputAgent::dispatchKeyEvent(ErrorString* error, const String& type, const int* modifiers, const double* timestamp, const String* text, const String* unmodifiedText, const String* keyIdentifier, const int* windowsVirtualKeyCode, const int* nativeVirtualKeyCode, const bool* autoRepeat, const bool* isKeypad, const bool* isSystemKey)
{
    PlatformEvent::Type convertedType;
    if (type == "keyDown")
        convertedType = PlatformEvent::KeyDown;
    else if (type == "keyUp")
        convertedType = PlatformEvent::KeyUp;
    else if (type == "char")
        convertedType = PlatformEvent::Char;
    else if (type == "rawKeyDown")
        convertedType = PlatformEvent::RawKeyDown;
    else {
        *error = "Unrecognized type: " + type;
        return;
    }

    PlatformKeyboardEvent event(
        convertedType,
        text ? *text : "",
        unmodifiedText ? *unmodifiedText : "",
        keyIdentifier ? *keyIdentifier : "",
        windowsVirtualKeyCode ? *windowsVirtualKeyCode : 0,
        nativeVirtualKeyCode ? *nativeVirtualKeyCode : 0,
        autoRepeat ? *autoRepeat : false,
        isKeypad ? *isKeypad : false,
        isSystemKey ? *isSystemKey : false,
        static_cast<PlatformEvent::Modifiers>(modifiers ? *modifiers : 0),
        timestamp ? *timestamp : currentTime());
    m_client->dispatchKeyEvent(event);
}

void InspectorInputAgent::dispatchMouseEvent(ErrorString* error, const String& type, int x, int y, const int* modifiers, const double* timestamp, const String* button, const int* clickCount, const bool* deviceSpace)
{
    if (deviceSpace && *deviceSpace) {
        *error = "Internal error: events with device coordinates should be processed on the embedder level.";
        return;
    }
    PlatformEvent::Type convertedType;
    if (type == "mousePressed")
        convertedType = PlatformEvent::MousePressed;
    else if (type == "mouseReleased")
        convertedType = PlatformEvent::MouseReleased;
    else if (type == "mouseMoved")
        convertedType = PlatformEvent::MouseMoved;
    else {
        *error = "Unrecognized type: " + type;
        return;
    }

    int convertedModifiers = modifiers ? *modifiers : 0;

    MouseButton convertedButton = NoButton;
    if (button) {
        if (*button == "left")
            convertedButton = LeftButton;
        else if (*button == "middle")
            convertedButton = MiddleButton;
        else if (*button == "right")
            convertedButton = RightButton;
        else if (*button != "none") {
            *error = "Unrecognized button: " + *button;
            return;
        }
    }

    // Some platforms may have flipped coordinate systems, but the given coordinates
    // assume the origin is in the top-left of the window. Convert.
    IntPoint convertedPoint, globalPoint;
    ConvertInspectorPoint(m_page, IntPoint(x, y), &convertedPoint, &globalPoint);

    PlatformMouseEvent event(
        convertedPoint,
        globalPoint,
        convertedButton,
        convertedType,
        clickCount ? *clickCount : 0,
        convertedModifiers & PlatformEvent::ShiftKey,
        convertedModifiers & PlatformEvent::CtrlKey,
        convertedModifiers & PlatformEvent::AltKey,
        convertedModifiers & PlatformEvent::MetaKey,
        timestamp ? *timestamp : currentTime());

    m_client->dispatchMouseEvent(event);
}

void InspectorInputAgent::dispatchTouchEvent(ErrorString* error, const String& type, const RefPtr<JSONArray>& touchPoints, const int* modifiers, const double* timestamp)
{
    PlatformEvent::Type convertedType;
    if (type == "touchStart") {
        convertedType = PlatformEvent::TouchStart;
    } else if (type == "touchEnd") {
        convertedType = PlatformEvent::TouchEnd;
    } else if (type == "touchMove") {
        convertedType = PlatformEvent::TouchMove;
    } else {
        *error = "Unrecognized type: " + type;
        return;
    }

    unsigned convertedModifiers = modifiers ? *modifiers : 0;

    SyntheticInspectorTouchEvent event(convertedType, convertedModifiers, timestamp ? *timestamp : currentTime());

    int autoId = 0;
    JSONArrayBase::iterator iter;
    for (iter = touchPoints->begin(); iter != touchPoints->end(); ++iter) {
        RefPtr<JSONObject> pointObj;
        String state;
        int x, y, radiusX, radiusY, id;
        double rotationAngle, force;
        (*iter)->asObject(&pointObj);
        if (!pointObj->getString("state", &state)) {
            *error = "TouchPoint missing 'state'";
            return;
        }
        if (!pointObj->getNumber("x", &x)) {
            *error = "TouchPoint missing 'x' coordinate";
            return;
        }
        if (!pointObj->getNumber("y", &y)) {
            *error = "TouchPoint missing 'y' coordinate";
            return;
        }
        if (!pointObj->getNumber("radiusX", &radiusX))
            radiusX = 1;
        if (!pointObj->getNumber("radiusY", &radiusY))
            radiusY = 1;
        if (!pointObj->getNumber("rotationAngle", &rotationAngle))
            rotationAngle = 0.0f;
        if (!pointObj->getNumber("force", &force))
            force = 1.0f;
        if (pointObj->getNumber("id", &id)) {
            if (autoId > 0)
                id = -1;
            autoId = -1;
        } else {
            id = autoId++;
        }
        if (id < 0) {
            *error = "All or none of the provided TouchPoints must supply positive integer ids.";
            return;
        }

        PlatformTouchPoint::State convertedState;
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
            *error = "Unrecognized state: " + state;
            return;
        }

        // Some platforms may have flipped coordinate systems, but the given coordinates
        // assume the origin is in the top-left of the window. Convert.
        IntPoint convertedPoint, globalPoint;
        ConvertInspectorPoint(m_page, IntPoint(x, y), &convertedPoint, &globalPoint);

        SyntheticInspectorTouchPoint point(id++, convertedState, globalPoint, convertedPoint, radiusX, radiusY, rotationAngle, force);
        event.append(point);
    }

    m_page->deprecatedLocalMainFrame()->eventHandler().handleTouchEvent(event);
}

} // namespace WebCore

