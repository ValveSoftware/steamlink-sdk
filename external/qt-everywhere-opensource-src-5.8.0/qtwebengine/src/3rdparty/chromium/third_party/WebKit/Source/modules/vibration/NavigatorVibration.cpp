/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "modules/vibration/NavigatorVibration.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "modules/vibration/VibrationController.h"
#include "platform/Histogram.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

NavigatorVibration::NavigatorVibration(Navigator& navigator)
    : DOMWindowProperty(navigator.frame())
{
}

NavigatorVibration::~NavigatorVibration()
{
}

// static
NavigatorVibration& NavigatorVibration::from(Navigator& navigator)
{
    NavigatorVibration* navigatorVibration = static_cast<NavigatorVibration*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!navigatorVibration) {
        navigatorVibration = new NavigatorVibration(navigator);
        Supplement<Navigator>::provideTo(navigator, supplementName(), navigatorVibration);
    }
    return *navigatorVibration;
}

// static
const char* NavigatorVibration::supplementName()
{
    return "NavigatorVibration";
}

// static
bool NavigatorVibration::vibrate(Navigator& navigator, unsigned time)
{
    VibrationPattern pattern;
    pattern.append(time);
    return NavigatorVibration::vibrate(navigator, pattern);
}

// static
bool NavigatorVibration::vibrate(Navigator& navigator, const VibrationPattern& pattern)
{
    LocalFrame* frame = navigator.frame();

    // There will be no frame if the window has been closed, but a JavaScript
    // reference to |window| or |navigator| was retained in another window.
    if (!frame)
        return false;
    collectHistogramMetrics(*frame);

    DCHECK(frame->document());
    DCHECK(frame->page());

    if (!frame->page()->isPageVisible())
        return false;

    return NavigatorVibration::from(navigator).controller()->vibrate(pattern);
}

// static
void NavigatorVibration::collectHistogramMetrics(const LocalFrame& frame)
{
    NavigatorVibrationType type;
    bool userGesture = UserGestureIndicator::processingUserGesture();
    UseCounter::count(&frame, UseCounter::NavigatorVibrate);
    if (!frame.isMainFrame()) {
        UseCounter::count(&frame, UseCounter::NavigatorVibrateSubFrame);
        if (frame.isCrossOrigin()) {
            if (userGesture)
                type = NavigatorVibrationType::CrossOriginSubFrameWithUserGesture;
            else
                type = NavigatorVibrationType::CrossOriginSubFrameNoUserGesture;
        } else {
            if (userGesture)
                type = NavigatorVibrationType::SameOriginSubFrameWithUserGesture;
            else
                type = NavigatorVibrationType::SameOriginSubFrameNoUserGesture;
        }
    } else {
        if (userGesture)
            type = NavigatorVibrationType::MainFrameWithUserGesture;
        else
            type = NavigatorVibrationType::MainFrameNoUserGesture;
    }
    DEFINE_STATIC_LOCAL(EnumerationHistogram, NavigatorVibrateHistogram, ("Vibration.Context", NavigatorVibrationType::EnumMax));
    NavigatorVibrateHistogram.count(type);
}

VibrationController* NavigatorVibration::controller()
{
    if (!m_controller && frame())
        m_controller = new VibrationController(*frame()->document());

    return m_controller.get();
}

void NavigatorVibration::willDetachGlobalObjectFromFrame()
{
    if (m_controller) {
        m_controller->cancel();
        m_controller = nullptr;
    }
}

DEFINE_TRACE(NavigatorVibration)
{
    visitor->trace(m_controller);
    Supplement<Navigator>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
