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

#include "modules/vibration/VibrationController.h"

#include "bindings/modules/v8/UnsignedLongOrUnsignedLongSequence.h"
#include "core/dom/Document.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/ServiceRegistry.h"

// Maximum number of entries in a vibration pattern.
const unsigned kVibrationPatternLengthMax = 99;

// Maximum duration of a vibration is 10 seconds.
const unsigned kVibrationDurationMsMax = 10000;

blink::VibrationController::VibrationPattern sanitizeVibrationPatternInternal(const blink::VibrationController::VibrationPattern& pattern)
{
    blink::VibrationController::VibrationPattern sanitized = pattern;
    size_t length = sanitized.size();

    // If the pattern is too long then truncate it.
    if (length > kVibrationPatternLengthMax) {
        sanitized.shrink(kVibrationPatternLengthMax);
        length = kVibrationPatternLengthMax;
    }

    // If any pattern entry is too long then truncate it.
    for (size_t i = 0; i < length; ++i) {
        if (sanitized[i] > kVibrationDurationMsMax)
            sanitized[i] = kVibrationDurationMsMax;
    }

    // If the last item in the pattern is a pause then discard it.
    if (length && !(length % 2))
        sanitized.removeLast();

    return sanitized;
}

namespace blink {

// static
VibrationController::VibrationPattern VibrationController::sanitizeVibrationPattern(const UnsignedLongOrUnsignedLongSequence& pattern)
{
    VibrationPattern sanitized;

    if (pattern.isUnsignedLong())
        sanitized.append(pattern.getAsUnsignedLong());
    else if (pattern.isUnsignedLongSequence())
        sanitized = pattern.getAsUnsignedLongSequence();

    return sanitizeVibrationPatternInternal(sanitized);
}

VibrationController::VibrationController(Document& document)
    : ContextLifecycleObserver(&document)
    , PageLifecycleObserver(document.page())
    , m_timerDoVibrate(this, &VibrationController::doVibrate)
    , m_isRunning(false)
    , m_isCallingCancel(false)
    , m_isCallingVibrate(false)
{
    document.frame()->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_service));
}

VibrationController::~VibrationController()
{
}

bool VibrationController::vibrate(const VibrationPattern& pattern)
{
    // Cancel clears the stored pattern and cancels any ongoing vibration.
    cancel();

    m_pattern = sanitizeVibrationPatternInternal(pattern);

    if (!m_pattern.size())
        return true;

    if (m_pattern.size() == 1 && !m_pattern[0]) {
        m_pattern.clear();
        return true;
    }

    m_isRunning = true;

    // This may be a bit racy with |didCancel| being called as a mojo callback,
    // it also starts the timer. This is not a problem as calling |startOneShot|
    // repeatedly will just update the time at which to run |doVibrate|, it will
    // not be called more than once.
    m_timerDoVibrate.startOneShot(0, BLINK_FROM_HERE);

    return true;
}

void VibrationController::doVibrate(Timer<VibrationController>* timer)
{
    DCHECK(timer == &m_timerDoVibrate);

    if (m_pattern.isEmpty())
        m_isRunning = false;

    if (!m_isRunning || m_isCallingCancel || m_isCallingVibrate || !getExecutionContext() || !page()->isPageVisible())
        return;

    if (m_service) {
        m_isCallingVibrate = true;
        m_service->Vibrate(m_pattern[0], createBaseCallback(WTF::bind(&VibrationController::didVibrate, wrapPersistent(this))));
    }
}

void VibrationController::didVibrate()
{
    m_isCallingVibrate = false;

    // If the pattern is empty here, it was probably cleared by a fresh call to
    // |vibrate| while the mojo call was in flight.
    if (m_pattern.isEmpty())
        return;

    // Use the current vibration entry of the pattern as the initial interval.
    unsigned interval = m_pattern[0];
    m_pattern.remove(0);

    // If there is another entry it is for a pause.
    if (!m_pattern.isEmpty()) {
        interval += m_pattern[0];
        m_pattern.remove(0);
    }

    m_timerDoVibrate.startOneShot(interval / 1000.0, BLINK_FROM_HERE);
}

void VibrationController::cancel()
{
    m_pattern.clear();

    if (m_timerDoVibrate.isActive())
        m_timerDoVibrate.stop();

    if (m_isRunning && !m_isCallingCancel && m_service) {
        m_isCallingCancel = true;
        m_service->Cancel(createBaseCallback(WTF::bind(&VibrationController::didCancel, wrapPersistent(this))));
    }

    m_isRunning = false;
}

void VibrationController::didCancel()
{
    m_isCallingCancel = false;

    // A new vibration pattern may have been set while the mojo call for
    // |cancel| was in flight, so kick the timer to let |doVibrate| process the
    // pattern.
    m_timerDoVibrate.startOneShot(0, BLINK_FROM_HERE);
}

void VibrationController::contextDestroyed()
{
    cancel();

    // If the document context was destroyed, never call the mojo service again.
    m_service.reset();

    // The context is not automatically cleared, so do it manually.
    ContextLifecycleObserver::clearContext();

    // Page outlives ExecutionContext so stop observing it to avoid having
    // |pageVisibilityChanged| or |contextDestroyed| called again.
    PageLifecycleObserver::clearContext();
}

void VibrationController::pageVisibilityChanged()
{
    if (!page()->isPageVisible())
        cancel();
}

DEFINE_TRACE(VibrationController)
{
    ContextLifecycleObserver::trace(visitor);
    PageLifecycleObserver::trace(visitor);
}

} // namespace blink
