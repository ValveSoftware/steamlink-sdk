/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webaudio/AudioParamTimeline.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "platform/FloatConversion.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/CPU.h"
#include "wtf/MathExtras.h"
#include <algorithm>

#if CPU(X86) || CPU(X86_64)
#include <emmintrin.h>
#endif

namespace blink {

// For a SetTarget event, if the relative difference between the current value and the target value
// is less than this, consider them the same and just output the target value.  This value MUST be
// larger than the single precision epsilon of 5.960465e-8.  Due to round-off, this value is not
// achievable in general.  This value can vary across the platforms (CPU) and thus it is determined
// experimentally.
const float kSetTargetThreshold = 1.5e-6;

// For a SetTarget event, if the target value is 0, and the current value is less than this
// threshold, consider the curve to have converged to 0.  We need a separate case from
// kSetTargetThreshold because that uses relative error, which is never met if the target value is
// 0, a common case.  This value MUST be larger than least positive normalized single precision
// value (1.1754944e-38) because we normally operate with flush-to-zero enabled.
const float kSetTargetZeroThreshold = 1e-20;

static bool isNonNegativeAudioParamTime(double time, ExceptionState& exceptionState, String message = "Time")
{
    if (time >= 0)
        return true;

    exceptionState.throwDOMException(
        InvalidAccessError,
        message + " must be a finite non-negative number: " + String::number(time));
    return false;
}

static bool isPositiveAudioParamTime(double time, ExceptionState& exceptionState, String message)
{
    if (time > 0)
        return true;

    exceptionState.throwDOMException(
        InvalidAccessError,
        message + " must be a finite positive number: " + String::number(time));
    return false;
}

String AudioParamTimeline::eventToString(const ParamEvent& event)
{
    // The default arguments for most automation methods is the value and the time.
    String args = String::number(event.value()) + ", " + String::number(event.time(), 16);

    // Get a nice printable name for the event and update the args if necessary.
    String s;
    switch (event.getType()) {
    case ParamEvent::SetValue:
        s = "setValueAtTime";
        break;
    case ParamEvent::LinearRampToValue:
        s = "linearRampToValueAtTime";
        break;
    case ParamEvent::ExponentialRampToValue:
        s = "exponentialRampToValue";
        break;
    case ParamEvent::SetTarget:
        s = "setTargetAtTime";
        // This has an extra time constant arg
        args = args + ", " + String::number(event.timeConstant(), 16);
        break;
    case ParamEvent::SetValueCurve:
        s = "setValueCurveAtTime";
        // Replace the default arg, using "..." to denote the curve argument.
        args = "..., " + String::number(event.time(), 16) + ", " + String::number(event.duration(), 16);
        break;
    case ParamEvent::LastType:
        ASSERT_NOT_REACHED();
        break;
    };

    return s + "(" + args + ")";
}

AudioParamTimeline::ParamEvent::ParamEvent(
    Type type, float value, double time,
    double timeConstant, double duration, const DOMFloat32Array* curve,
    float initialValue, double callTime)
    : m_type(type)
    , m_value(value)
    , m_time(time)
    , m_timeConstant(timeConstant)
    , m_duration(duration)
    , m_initialValue(initialValue)
    , m_callTime(callTime)
{
    if (curve) {
        // Copy the curve data
        unsigned curveLength = curve->length();
        m_curve.resize(curveLength);
        memcpy(m_curve.data(), curve->data(), curveLength * sizeof(float));
    }
}

AudioParamTimeline::ParamEvent AudioParamTimeline::ParamEvent::createSetValueEvent(float value, double time)
{
    return ParamEvent(ParamEvent::SetValue, value, time, 0, 0, nullptr);
}

AudioParamTimeline::ParamEvent AudioParamTimeline::ParamEvent::createLinearRampEvent(float value, double time, float initialValue, double callTime)
{
    return ParamEvent(ParamEvent::LinearRampToValue, value, time, 0, 0, nullptr, initialValue, callTime);
}

AudioParamTimeline::ParamEvent AudioParamTimeline::ParamEvent::createExponentialRampEvent(float value, double time, float initialValue, double callTime)
{
    return ParamEvent(ParamEvent::ExponentialRampToValue, value, time, 0, 0, nullptr, initialValue, callTime);
}

AudioParamTimeline::ParamEvent AudioParamTimeline::ParamEvent::createSetTargetEvent(float value, double time, double timeConstant)
{
    return ParamEvent(ParamEvent::SetTarget, value, time, timeConstant, 0, nullptr);
}

AudioParamTimeline::ParamEvent AudioParamTimeline::ParamEvent::createSetValueCurveEvent(const DOMFloat32Array* curve, double time, double duration)
{
    return ParamEvent(ParamEvent::SetValueCurve, 0, time, 0, duration, curve);
}

void AudioParamTimeline::setValueAtTime(float value, double time, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState))
        return;

    insertEvent(ParamEvent::createSetValueEvent(value, time), exceptionState);
}

void AudioParamTimeline::linearRampToValueAtTime(float value, double time, float initialValue, double callTime, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState))
        return;

    insertEvent(ParamEvent::createLinearRampEvent(value, time, initialValue, callTime), exceptionState);
}

void AudioParamTimeline::exponentialRampToValueAtTime(float value, double time, float initialValue, double callTime, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState))
        return;

    if (!value) {
        exceptionState.throwDOMException(
            InvalidAccessError,
            "The float target value provided (" + String::number(value)
            + ") should not be in the range (" + String::number(-std::numeric_limits<float>::denorm_min())
            + ", " + String::number(std::numeric_limits<float>::denorm_min())
            + ").");
        return;
    }

    insertEvent(ParamEvent::createExponentialRampEvent(value, time, initialValue, callTime), exceptionState);
}

void AudioParamTimeline::setTargetAtTime(float target, double time, double timeConstant, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!isNonNegativeAudioParamTime(time, exceptionState)
        || !isPositiveAudioParamTime(timeConstant, exceptionState, "Time constant"))
        return;

    insertEvent(ParamEvent::createSetTargetEvent(target, time, timeConstant), exceptionState);
}

void AudioParamTimeline::setValueCurveAtTime(DOMFloat32Array* curve, double time, double duration, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    ASSERT(curve);

    if (!isNonNegativeAudioParamTime(time, exceptionState)
        || !isPositiveAudioParamTime(duration, exceptionState, "Duration"))
        return;

    if (curve->length() < 2) {
        exceptionState.throwDOMException(
            InvalidStateError,
            ExceptionMessages::indexExceedsMinimumBound(
                "curve length",
                curve->length(),
                2U));
        return;
    }

    insertEvent(ParamEvent::createSetValueCurveEvent(curve, time, duration), exceptionState);

    // Insert a setValueAtTime event too to establish an event so that all
    // following events will process from the end of the curve instead of the
    // beginning.
    insertEvent(ParamEvent::createSetValueEvent(curve->data()[curve->length() - 1], time + duration), exceptionState);
}

void AudioParamTimeline::insertEvent(const ParamEvent& event, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    // Sanity check the event. Be super careful we're not getting infected with NaN or Inf. These
    // should have been handled by the caller.
    bool isValid = event.getType() < ParamEvent::LastType
        && std::isfinite(event.value())
        && std::isfinite(event.time())
        && std::isfinite(event.timeConstant())
        && std::isfinite(event.duration())
        && event.duration() >= 0;

    ASSERT(isValid);
    if (!isValid)
        return;

    MutexLocker locker(m_eventsLock);

    unsigned i = 0;
    double insertTime = event.time();

    if (!m_events.size()
        && (event.getType() == ParamEvent::LinearRampToValue
            || event.getType() == ParamEvent::ExponentialRampToValue)) {
        // There are no events preceding these ramps.  Insert a new setValueAtTime event to set the
        // starting point for these events.
        m_events.insert(0,
            AudioParamTimeline::ParamEvent::createSetValueEvent(event.initialValue(), event.callTime()));
    }

    for (i = 0; i < m_events.size(); ++i) {
        if (event.getType() == ParamEvent::SetValueCurve) {
            // If this event is a SetValueCurve, make sure it doesn't overlap any existing
            // event. It's ok if the SetValueCurve starts at the same time as the end of some other
            // duration.
            double endTime = event.time() + event.duration();
            if (m_events[i].time() > event.time() && m_events[i].time() < endTime) {
                exceptionState.throwDOMException(
                    NotSupportedError,
                    eventToString(event) + " overlaps " + eventToString(m_events[i]));
                return;
            }
        } else {
            // Otherwise, make sure this event doesn't overlap any existing SetValueCurve event.
            if (m_events[i].getType() == ParamEvent::SetValueCurve) {
                double endTime = m_events[i].time() + m_events[i].duration();
                if (event.time() >= m_events[i].time() && event.time() < endTime) {
                    exceptionState.throwDOMException(
                        NotSupportedError,
                        eventToString(event) + " overlaps " + eventToString(m_events[i]));
                    return;
                }
            }
        }

        // Overwrite same event type and time.
        if (m_events[i].time() == insertTime && m_events[i].getType() == event.getType()) {
            m_events[i] = event;
            return;
        }

        if (m_events[i].time() > insertTime)
            break;
    }

    m_events.insert(i, event);
}

bool AudioParamTimeline::hasValues() const
{
    MutexTryLocker tryLocker(m_eventsLock);

    if (tryLocker.locked())
        return m_events.size();

    // Can't get the lock so that means the main thread is trying to insert an event.  Just
    // return true then.  If the main thread releases the lock before valueForContextTime or
    // valuesForFrameRange runs, then the there will be an event on the timeline, so everything
    // is fine.  If the lock is held so that neither valueForContextTime nor valuesForFrameRange
    // can run, this is ok too, because they have tryLocks to produce a default value.  The
    // event will then get processed in the next rendering quantum.
    //
    // Don't want to return false here because that would confuse the processing of the timeline
    // if previously we returned true and now suddenly return false, only to return true on the
    // next rendering quantum.  Currently, once a timeline has been introduced it is always true
    // forever because m_events never shrinks.
    return true;
}

void AudioParamTimeline::cancelScheduledValues(double startTime, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    MutexLocker locker(m_eventsLock);

    // Remove all events starting at startTime.
    for (unsigned i = 0; i < m_events.size(); ++i) {
        if (m_events[i].time() >= startTime) {
            m_events.remove(i, m_events.size() - i);
            break;
        }
    }
}

float AudioParamTimeline::valueForContextTime(AudioDestinationHandler& audioDestination, float defaultValue, bool& hasValue, float minValue, float maxValue)
{
    {
        MutexTryLocker tryLocker(m_eventsLock);
        if (!tryLocker.locked() || !m_events.size() || audioDestination.currentTime() < m_events[0].time()) {
            hasValue = false;
            return defaultValue;
        }
    }

    // Ask for just a single value.
    float value;
    double sampleRate = audioDestination.sampleRate();
    size_t startFrame = audioDestination.currentSampleFrame();
    double controlRate = sampleRate / AudioHandler::ProcessingSizeInFrames; // one parameter change per render quantum
    value = valuesForFrameRange(startFrame, startFrame + 1, defaultValue, &value, 1, sampleRate, controlRate, minValue, maxValue);

    hasValue = true;
    return value;
}

float AudioParamTimeline::valuesForFrameRange(
    size_t startFrame,
    size_t endFrame,
    float defaultValue,
    float* values,
    unsigned numberOfValues,
    double sampleRate,
    double controlRate,
    float minValue,
    float maxValue)
{
    // We can't contend the lock in the realtime audio thread.
    MutexTryLocker tryLocker(m_eventsLock);
    if (!tryLocker.locked()) {
        if (values) {
            for (unsigned i = 0; i < numberOfValues; ++i)
                values[i] = defaultValue;
        }
        return defaultValue;
    }

    float lastValue = valuesForFrameRangeImpl(startFrame, endFrame, defaultValue, values, numberOfValues, sampleRate, controlRate);

    // Clamp the values now to the nominal range
    for (unsigned k = 0; k < numberOfValues; ++k)
        values[k] = clampTo(values[k], minValue, maxValue);

    return lastValue;
}

float AudioParamTimeline::valuesForFrameRangeImpl(
    size_t startFrame,
    size_t endFrame,
    float defaultValue,
    float* values,
    unsigned numberOfValues,
    double sampleRate,
    double controlRate)
{
    ASSERT(values);
    ASSERT(numberOfValues >= 1);
    if (!values || !(numberOfValues >= 1))
        return defaultValue;

    // Return default value if there are no events matching the desired time range.
    if (!m_events.size() || (endFrame / sampleRate <= m_events[0].time())) {
        for (unsigned i = 0; i < numberOfValues; ++i)
            values[i] = defaultValue;
        return defaultValue;
    }

    // Optimize the case where the last event is in the past.
    if (m_events.size() > 0) {
        ParamEvent& lastEvent = m_events[m_events.size() - 1];
        ParamEvent::Type lastEventType = lastEvent.getType();
        double lastEventTime = lastEvent.time();
        double currentTime = startFrame / sampleRate;

        // If the last event is in the past and the event has ended, then we can
        // just propagate the same value.  Except for SetTarget which lasts
        // "forever".  SetValueCurve also has an explicit SetValue at the end of
        // the curve, so we don't need to worry that SetValueCurve time is a
        // start time, not an end time.
        if (lastEventTime < currentTime && lastEventType != ParamEvent::SetTarget) {
            // The event has finished, so just copy the default value out.
            // Since all events are now also in the past, we can just remove all
            // timeline events too because |defaultValue| has the expected
            // value.
            for (unsigned i = 0; i < numberOfValues; ++i)
                values[i] = defaultValue;
            m_smoothedValue = defaultValue;
            m_events.clear();
            return defaultValue;
        }
    }

    // Maintain a running time (frame) and index for writing the values buffer.
    size_t currentFrame = startFrame;
    unsigned writeIndex = 0;

    // If first event is after startFrame then fill initial part of values buffer with defaultValue
    // until we reach the first event time.
    double firstEventTime = m_events[0].time();
    if (firstEventTime > startFrame / sampleRate) {
        // |fillToFrame| is an exclusive upper bound, so use ceil() to compute the bound from the
        // firstEventTime.
        size_t fillToFrame = endFrame;
        double firstEventFrame = ceil(firstEventTime * sampleRate);
        if (endFrame > firstEventFrame)
            fillToFrame = static_cast<size_t>(firstEventFrame);
        ASSERT(fillToFrame >= startFrame);

        fillToFrame -= startFrame;
        fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));
        for (; writeIndex < fillToFrame; ++writeIndex)
            values[writeIndex] = defaultValue;

        currentFrame += fillToFrame;
    }

    float value = defaultValue;

    // Go through each event and render the value buffer where the times overlap,
    // stopping when we've rendered all the requested values.
    int n = m_events.size();
    int lastSkippedEventIndex = 0;
    for (int i = 0; i < n && writeIndex < numberOfValues; ++i) {
        ParamEvent& event = m_events[i];
        ParamEvent* nextEvent = i < n - 1 ? &(m_events[i + 1]) : 0;

        // Wait until we get a more recent event.
        //
        // WARNING: due to round-off it might happen that nextEvent->time() is
        // just larger than currentFrame/sampleRate.  This means that we will end
        // up running the |event| again.  The code below had better be prepared
        // for this case!  What should happen is the fillToFrame should be 0 so
        // that while the event is actually run again, nothing actually gets
        // computed, and we move on to the next event.
        //
        // An example of this case is setValueCurveAtTime.  The time at which
        // setValueCurveAtTime ends (and the setValueAtTime begins) might be
        // just past currentTime/sampleRate.  Then setValueCurveAtTime will be
        // processed again before advancing to setValueAtTime.  The number of
        // frames to be processed should be zero in this case.
        if (nextEvent && nextEvent->time() < currentFrame / sampleRate) {
            // But if the current event is a SetValue event and the event time is between
            // currentFrame - 1 and curentFrame (in time). we don't want to skip it.  If we do skip
            // it, the SetValue event is completely skipped and not applied, which is wrong.  Other
            // events don't have this problem.  (Because currentFrame is unsigned, we do the time
            // check in this funny, but equivalent way.)
            double eventFrame = event.time() * sampleRate;

            // Condition is currentFrame - 1 < eventFrame <= currentFrame, but currentFrame is
            // unsigned and could be 0, so use currentFrame < eventFrame + 1 instead.
            if (!((event.getType() == ParamEvent::SetValue
                && (eventFrame <= currentFrame)
                && (currentFrame < eventFrame + 1)))) {
                // This is not the special SetValue event case, and nextEvent is
                // in the past. We can skip processing of this event since it's
                // in past. We keep track of this event in lastSkippedEventIndex
                // to note what events we've skipped.
                lastSkippedEventIndex = i;
                continue;
            }
        }

        // If there's no next event, set nextEventType to LastType to indicate that.
        ParamEvent::Type nextEventType = nextEvent ? static_cast<ParamEvent::Type>(nextEvent->getType()) : ParamEvent::LastType;

        // If the current event is SetTarget and the next event is a LinearRampToValue or
        // ExponentialRampToValue, special handling is needed.  In this case, the linear and
        // exponential ramp should start at wherever the SetTarget processing has reached.
        if (event.getType() == ParamEvent::SetTarget
            && (nextEventType == ParamEvent::LinearRampToValue
                || nextEventType == ParamEvent::ExponentialRampToValue)) {
            // Replace the SetTarget with a SetValue to set the starting time and value for the ramp
            // using the current frame.  We need to update |value| appropriately depending on
            // whether the ramp has started or not.
            //
            // If SetTarget starts somewhere between currentFrame - 1 and currentFrame, we directly
            // compute the value it would have at currentFrame.  If not, we update the value from
            // the value from currentFrame - 1.
            //
            // Can't use the condition currentFrame - 1 <= t0 * sampleRate <= currentFrame because
            // currentFrame is unsigned and could be 0.  Instead, compute the condition this way,
            // where f = currentFrame and Fs = sampleRate:
            //
            //    f - 1 <= t0 * Fs <= f
            //    2 * f - 2 <= 2 * Fs * t0 <= 2 * f
            //    -2 <= 2 * Fs * t0 - 2 * f <= 0
            //    -1 <= 2 * Fs * t0 - 2 * f + 1 <= 1
            //     abs(2 * Fs * t0 - 2 * f + 1) <= 1
            if (fabs(2 * sampleRate * event.time() - 2 * currentFrame + 1) <= 1) {
                // SetTarget is starting somewhere between currentFrame - 1 and
                // currentFrame. Compute the value the SetTarget would have at the currentFrame.
                value = event.value() + (value - event.value()) * exp(-(currentFrame / sampleRate - event.time()) / event.timeConstant());
            } else {
                // SetTarget has already started.  Update |value| one frame because it's the value from
                // the previous frame.
                float discreteTimeConstant = static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(
                    event.timeConstant(), controlRate));
                value += (event.value() - value) * discreteTimeConstant;
            }
            m_events[i] = ParamEvent::createSetValueEvent(value, currentFrame / sampleRate);
        }

        float value1 = event.value();
        double time1 = event.time();

        float value2 = nextEvent ? nextEvent->value() : value1;
        double time2 = nextEvent ? nextEvent->time() : endFrame / sampleRate + 1;

        double deltaTime = time2 - time1;
        float k = deltaTime > 0 ? 1 / deltaTime : 0;

        // |fillToEndFrame| is the exclusive upper bound of the last frame to be computed for this
        // event.  It's either the last desired frame (|endFrame|) or derived from the end time of
        // the next event (time2). We compute ceil(time2*sampleRate) because fillToEndFrame is the
        // exclusive upper bound.  Consider the case where |startFrame| = 128 and time2 = 128.1
        // (assuming sampleRate = 1).  Since time2 is greater than 128, we want to output a value
        // for frame 128.  This requires that fillToEndFrame be at least 129.  This is achieved by
        // ceil(time2).
        //
        // However, time2 can be very large, so compute this carefully in the case where time2
        // exceeds the size of a size_t.

        size_t fillToEndFrame = endFrame;
        if (endFrame > time2 * sampleRate)
            fillToEndFrame = static_cast<size_t>(ceil(time2 * sampleRate));

        ASSERT(fillToEndFrame >= startFrame);
        size_t fillToFrame = fillToEndFrame - startFrame;
        fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));

        // First handle linear and exponential ramps which require looking ahead to the next event.
        if (nextEventType == ParamEvent::LinearRampToValue) {
            const float valueDelta = value2 - value1;
#if CPU(X86) || CPU(X86_64)
            // Minimize in-loop operations. Calculate starting value and increment. Next step: value += inc.
            //  value = value1 + (currentFrame/sampleRate - time1) * k * (value2 - value1);
            //  inc = 4 / sampleRate * k * (value2 - value1);
            // Resolve recursion by expanding constants to achieve a 4-step loop unrolling.
            //  value = value1 + ((currentFrame/sampleRate - time1) + i * sampleFrameTimeIncr) * k * (value2 -value1), i in 0..3
            __m128 vValue = _mm_mul_ps(_mm_set_ps1(1 / sampleRate), _mm_set_ps(3, 2, 1, 0));
            vValue = _mm_add_ps(vValue, _mm_set_ps1(currentFrame / sampleRate - time1));
            vValue = _mm_mul_ps(vValue, _mm_set_ps1(k * valueDelta));
            vValue = _mm_add_ps(vValue, _mm_set_ps1(value1));
            __m128 vInc = _mm_set_ps1(4 / sampleRate * k * valueDelta);

            // Truncate loop steps to multiple of 4.
            unsigned fillToFrameTrunc = writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
            // Compute final time.
            currentFrame += fillToFrameTrunc - writeIndex;

            // Process 4 loop steps.
            for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
                _mm_storeu_ps(values + writeIndex, vValue);
                vValue = _mm_add_ps(vValue, vInc);
            }
            // Update |value| with the last value computed so that the .value attribute of the
            // AudioParam gets the correct linear ramp value, in case the following loop doesn't
            // execute.
            if (writeIndex >= 1)
                value = values[writeIndex - 1];
#endif
            // Serially process remaining values.
            for (; writeIndex < fillToFrame; ++writeIndex) {
                float x = (currentFrame / sampleRate - time1) * k;
                // value = (1 - x) * value1 + x * value2;
                value = value1 + x * valueDelta;
                values[writeIndex] = value;
                ++currentFrame;
            }
        } else if (nextEventType == ParamEvent::ExponentialRampToValue) {
            if (value1 * value2 <= 0) {
                // It's an error if value1 and value2 have opposite signs or if one of them is zero.
                // Handle this by propagating the previous value, and making it the default.
                value = value1;

                for (; writeIndex < fillToFrame; ++writeIndex)
                    values[writeIndex] = value;
            } else {
                float numSampleFrames = deltaTime * sampleRate;
                // The value goes exponentially from value1 to value2 in a duration of deltaTime
                // seconds according to
                //
                //  v(t) = v1*(v2/v1)^((t-t1)/(t2-t1))
                //
                // Let c be currentFrame and F be the sampleRate.  Then we want to sample v(t)
                // at times t = (c + k)/F for k = 0, 1, ...:
                //
                //   v((c+k)/F) = v1*(v2/v1)^(((c/F+k/F)-t1)/(t2-t1))
                //              = v1*(v2/v1)^((c/F-t1)/(t2-t1))
                //                  *(v2/v1)^((k/F)/(t2-t1))
                //              = v1*(v2/v1)^((c/F-t1)/(t2-t1))
                //                  *[(v2/v1)^(1/(F*(t2-t1)))]^k
                //
                // Thus, this can be written as
                //
                //   v((c+k)/F) = V*m^k
                //
                // where
                //   V = v1*(v2/v1)^((c/F-t1)/(t2-t1))
                //   m = (v2/v1)^(1/(F*(t2-t1)))

                // Compute the per-sample multiplier.
                float multiplier = powf(value2 / value1, 1 / numSampleFrames);
                // Set the starting value of the exponential ramp.
                value = value1 * powf(value2 / value1,
                    (currentFrame / sampleRate - time1) / deltaTime);

                for (; writeIndex < fillToFrame; ++writeIndex) {
                    values[writeIndex] = value;
                    value *= multiplier;
                    ++currentFrame;
                }
                // |value| got updated one extra time in the above loop.  Restore it to the last
                // computed value.
                if (writeIndex >= 1)
                    value /= multiplier;

                // Due to roundoff it's possible that value exceeds value2.  Clip value to value2 if
                // we are within 1/2 frame of time2.
                if (currentFrame > time2 * sampleRate - 0.5)
                    value = value2;
            }
        } else {
            // Handle event types not requiring looking ahead to the next event.
            switch (event.getType()) {
            case ParamEvent::SetValue:
            case ParamEvent::LinearRampToValue:
                {
                    currentFrame = fillToEndFrame;

                    // Simply stay at a constant value.
                    value = event.value();

                    for (; writeIndex < fillToFrame; ++writeIndex)
                        values[writeIndex] = value;

                    break;
                }

            case ParamEvent::ExponentialRampToValue:
                {
                    currentFrame = fillToEndFrame;

                    // If we're here, we've reached the end of the ramp.  If we can (because the
                    // start and end values have the same sign, and neither is 0), use the actual
                    // end value.  If not, we have to propagate whatever we have.
                    if (i >= 1 && ((m_events[i - 1].value() * event.value()) > 0))
                        value = event.value();

                    // Simply stay at a constant value from the last time.  We don't want to use the
                    // value of the event in case value1 * value2 < 0.  In this case we should
                    // propagate the previous value, which is in |value|.
                    for (; writeIndex < fillToFrame; ++writeIndex)
                        values[writeIndex] = value;

                    break;
                }

            case ParamEvent::SetTarget:
                {
                    // Exponential approach to target value with given time constant.
                    //
                    //   v(t) = v2 + (v1 - v2)*exp(-(t-t1/tau))
                    //

                    float target = event.value();
                    float timeConstant = event.timeConstant();
                    float discreteTimeConstant = static_cast<float>(AudioUtilities::discreteTimeConstantForSampleRate(timeConstant, controlRate));

                    // Set the starting value correctly.  This is only needed when the current time
                    // is "equal" to the start time of this event.  This is to get the sampling
                    // correct if the start time of this automation isn't on a frame boundary.
                    // Otherwise, we can just continue from where we left off from the previous
                    // rendering quantum.
                    {
                        double rampStartFrame = time1 * sampleRate;
                        // Condition is c - 1 < r <= c where c = currentFrame and r =
                        // rampStartFrame.  Compute it this way because currentFrame is unsigned and
                        // could be 0.
                        if (rampStartFrame <= currentFrame && currentFrame < rampStartFrame + 1) {
                            value = target + (value - target) * exp(-(currentFrame / sampleRate - time1) / timeConstant);
                        } else {
                            // Otherwise, need to compute a new value bacause |value| is the last
                            // computed value of SetTarget.  Time has progressed by one frame, so we
                            // need to update the value for the new frame.
                            value += (target - value) * discreteTimeConstant;
                        }
                    }

                    // If the value is close enough to the target, just fill in the data with the
                    // target value.
                    if (fabs(value - target) < kSetTargetThreshold * fabs(target)
                        || (!target && fabs(value) < kSetTargetZeroThreshold)) {
                        for (; writeIndex < fillToFrame; ++writeIndex)
                            values[writeIndex] = target;
                    } else {
#if CPU(X86) || CPU(X86_64)
                        // Resolve recursion by expanding constants to achieve a 4-step loop unrolling.
                        // v1 = v0 + (t - v0) * c
                        // v2 = v1 + (t - v1) * c
                        // v2 = v0 + (t - v0) * c + (t - (v0 + (t - v0) * c)) * c
                        // v2 = v0 + (t - v0) * c + (t - v0) * c - (t - v0) * c * c
                        // v2 = v0 + (t - v0) * c * (2 - c)
                        // Thus c0 = c, c1 = c*(2-c). The same logic applies to c2 and c3.
                        const float c0 = discreteTimeConstant;
                        const float c1 = c0 * (2 - c0);
                        const float c2 = c0 * ((c0 - 3) * c0 + 3);
                        const float c3 = c0 * (c0 * ((4 - c0) * c0 - 6) + 4);

                        float delta;
                        __m128 vC = _mm_set_ps(c2, c1, c0, 0);
                        __m128 vDelta, vValue, vResult;

                        // Process 4 loop steps.
                        unsigned fillToFrameTrunc = writeIndex + ((fillToFrame - writeIndex) / 4) * 4;
                        for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
                            delta = target - value;
                            vDelta = _mm_set_ps1(delta);
                            vValue = _mm_set_ps1(value);

                            vResult = _mm_add_ps(vValue, _mm_mul_ps(vDelta, vC));
                            _mm_storeu_ps(values + writeIndex, vResult);

                            // Update value for next iteration.
                            value += delta * c3;
                        }
#endif
                        // Serially process remaining values
                        for (; writeIndex < fillToFrame; ++writeIndex) {
                            values[writeIndex] = value;
                            value += (target - value) * discreteTimeConstant;
                        }
                        // The previous loops may have updated |value| one extra time.  Reset it to
                        // the last computed value.
                        if (writeIndex >= 1)
                            value = values[writeIndex - 1];
                        currentFrame = fillToEndFrame;
                    }
                    break;
                }

            case ParamEvent::SetValueCurve:
                {
                    Vector<float> curve = event.curve();
                    float* curveData = curve.data();
                    unsigned numberOfCurvePoints = curve.size();

                    // Curve events have duration, so don't just use next event time.
                    double duration = event.duration();
                    // How much to step the curve index for each frame.  This is basically the term
                    // (N - 1)/Td in the specification.
                    double curvePointsPerFrame = (numberOfCurvePoints - 1) / duration / sampleRate;

                    if (!numberOfCurvePoints || duration <= 0 || sampleRate <= 0) {
                        // Error condition - simply propagate previous value.
                        currentFrame = fillToEndFrame;
                        for (; writeIndex < fillToFrame; ++writeIndex)
                            values[writeIndex] = value;
                        break;
                    }

                    // Save old values and recalculate information based on the curve's duration
                    // instead of the next event time.
                    size_t nextEventFillToFrame = fillToFrame;

                    // fillToEndFrame = min(endFrame, ceil(sampleRate * (time1 + duration))), but
                    // compute this carefully in case sampleRate*(time1 + duration) is huge.
                    // fillToEndFrame is an exclusive upper bound of the last frame to be computed,
                    // so ceil is used.
                    {
                        double curveEndFrame = ceil(sampleRate*(time1 + duration));
                        if (endFrame > curveEndFrame)
                            fillToEndFrame = static_cast<size_t>(curveEndFrame);
                        else
                            fillToEndFrame = endFrame;
                    }

                    // |fillToFrame| can be less than |startFrame| when the end of the
                    // setValueCurve automation has been reached, but the next automation has not
                    // yet started. In this case, |fillToFrame| is clipped to |time1|+|duration|
                    // above, but |startFrame| will keep increasing (because the current time is
                    // increasing).
                    fillToFrame = (fillToEndFrame < startFrame) ? 0 : fillToEndFrame - startFrame;
                    fillToFrame = std::min(fillToFrame, static_cast<size_t>(numberOfValues));

                    // Index into the curve data using a floating-point value.
                    // We're scaling the number of curve points by the duration (see curvePointsPerFrame).
                    double curveVirtualIndex = 0;
                    if (time1 < currentFrame / sampleRate) {
                        // Index somewhere in the middle of the curve data.
                        // Don't use timeToSampleFrame() since we want the exact floating-point frame.
                        double frameOffset = currentFrame - time1 * sampleRate;
                        curveVirtualIndex = curvePointsPerFrame * frameOffset;
                    }

                    // Set the default value in case fillToFrame is 0.
                    value = curveData[numberOfCurvePoints - 1];

                    // Render the stretched curve data using linear interpolation.  Oversampled
                    // curve data can be provided if sharp discontinuities are desired.
                    unsigned k = 0;
#if CPU(X86) || CPU(X86_64)
                    const __m128 vCurveVirtualIndex = _mm_set_ps1(curveVirtualIndex);
                    const __m128 vCurvePointsPerFrame = _mm_set_ps1(curvePointsPerFrame);
                    const __m128 vNumberOfCurvePointsM1 = _mm_set_ps1(numberOfCurvePoints - 1);
                    const __m128 vN1 = _mm_set_ps1(1.0f);
                    const __m128 vN4 = _mm_set_ps1(4.0f);

                    __m128 vK = _mm_set_ps(3, 2, 1, 0);
                    int aCurveIndex0[4];
                    int aCurveIndex1[4];

                    // Truncate loop steps to multiple of 4
                    unsigned truncatedSteps = ((fillToFrame - writeIndex) / 4) * 4;
                    unsigned fillToFrameTrunc = writeIndex + truncatedSteps;
                    for (; writeIndex < fillToFrameTrunc; writeIndex += 4) {
                        // Compute current index this way to minimize round-off that would have
                        // occurred by incrementing the index by curvePointsPerFrame.
                        __m128 vCurrentVirtualIndex = _mm_add_ps(vCurveVirtualIndex, _mm_mul_ps(vK, vCurvePointsPerFrame));
                        vK = _mm_add_ps(vK, vN4);

                        // Clamp index to the last element of the array.
                        __m128i vCurveIndex0 = _mm_cvttps_epi32(_mm_min_ps(vCurrentVirtualIndex, vNumberOfCurvePointsM1));
                        __m128i vCurveIndex1 = _mm_cvttps_epi32(_mm_min_ps(_mm_add_ps(vCurrentVirtualIndex, vN1), vNumberOfCurvePointsM1));

                        // Linearly interpolate between the two nearest curve points.  |delta| is
                        // clamped to 1 because currentVirtualIndex can exceed curveIndex0 by more
                        // than one.  This can happen when we reached the end of the curve but still
                        // need values to fill out the current rendering quantum.
                        _mm_storeu_si128((__m128i*)aCurveIndex0, vCurveIndex0);
                        _mm_storeu_si128((__m128i*)aCurveIndex1, vCurveIndex1);
                        __m128 vC0 = _mm_set_ps(curveData[aCurveIndex0[3]], curveData[aCurveIndex0[2]], curveData[aCurveIndex0[1]], curveData[aCurveIndex0[0]]);
                        __m128 vC1 = _mm_set_ps(curveData[aCurveIndex1[3]], curveData[aCurveIndex1[2]], curveData[aCurveIndex1[1]], curveData[aCurveIndex1[0]]);
                        __m128 vDelta = _mm_min_ps(_mm_sub_ps(vCurrentVirtualIndex, _mm_cvtepi32_ps(vCurveIndex0)), vN1);

                        __m128 vValue = _mm_add_ps(vC0, _mm_mul_ps(_mm_sub_ps(vC1, vC0), vDelta));

                        _mm_storeu_ps(values + writeIndex, vValue);
                    }
                    // Pass along k to the serial loop.
                    k = truncatedSteps;
                    // If the above loop was run, pass along the last computed value.
                    if (truncatedSteps > 0) {
                        value = values[writeIndex - 1];
                    }
#endif
                    for (; writeIndex < fillToFrame; ++writeIndex, ++k) {
                        // Compute current index this way to minimize round-off that would have
                        // occurred by incrementing the index by curvePointsPerFrame.
                        double currentVirtualIndex = curveVirtualIndex + k * curvePointsPerFrame;
                        unsigned curveIndex0;

                        // Clamp index to the last element of the array.
                        if (currentVirtualIndex < numberOfCurvePoints) {
                            curveIndex0 = static_cast<unsigned>(currentVirtualIndex);
                        } else {
                            curveIndex0 = numberOfCurvePoints - 1;
                        }

                        unsigned curveIndex1 = std::min(curveIndex0 + 1, numberOfCurvePoints - 1);

                        // Linearly interpolate between the two nearest curve points.  |delta| is
                        // clamped to 1 because currentVirtualIndex can exceed curveIndex0 by more
                        // than one.  This can happen when we reached the end of the curve but still
                        // need values to fill out the current rendering quantum.
                        ASSERT(curveIndex0 < numberOfCurvePoints);
                        ASSERT(curveIndex1 < numberOfCurvePoints);
                        float c0 = curveData[curveIndex0];
                        float c1 = curveData[curveIndex1];
                        double delta = std::min(currentVirtualIndex - curveIndex0, 1.0);

                        value = c0 + (c1 - c0) * delta;

                        values[writeIndex] = value;
                    }

                    // If there's any time left after the duration of this event and the start
                    // of the next, then just propagate the last value of the curveData.
                    if (writeIndex < nextEventFillToFrame)
                        value = curveData[numberOfCurvePoints - 1];
                    for (; writeIndex < nextEventFillToFrame; ++writeIndex)
                        values[writeIndex] = value;

                    // Re-adjust current time
                    currentFrame += nextEventFillToFrame;

                    break;
                }
            case ParamEvent::LastType:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    // If we skipped over any events (because they are in the past), we can
    // remove them so we don't have to check them ever again.  (This MUST be
    // running with the m_events lock so we can safely modify the m_events
    // array.)
    if (lastSkippedEventIndex > 0)
        m_events.remove(0, lastSkippedEventIndex - 1);

    // If there's any time left after processing the last event then just propagate the last value
    // to the end of the values buffer.
    for (; writeIndex < numberOfValues; ++writeIndex)
        values[writeIndex] = value;

    // This value is used to set the .value attribute of the AudioParam.  it should be the last
    // computed value.
    return values[numberOfValues - 1];
}

} // namespace blink

