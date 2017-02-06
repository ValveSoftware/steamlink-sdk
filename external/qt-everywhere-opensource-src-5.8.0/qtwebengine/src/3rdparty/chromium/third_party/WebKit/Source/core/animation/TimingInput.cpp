// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/TimingInput.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/animation/AnimationInputHelpers.h"
#include "core/animation/KeyframeEffectOptions.h"

namespace blink {

void TimingInput::setStartDelay(Timing& timing, double startDelay)
{
    if (std::isfinite(startDelay))
        timing.startDelay = startDelay / 1000;
    else
        timing.startDelay = Timing::defaults().startDelay;
}

void TimingInput::setEndDelay(Timing& timing, double endDelay)
{
    if (std::isfinite(endDelay))
        timing.endDelay = endDelay / 1000;
    else
        timing.endDelay = Timing::defaults().endDelay;
}

void TimingInput::setFillMode(Timing& timing, const String& fillMode)
{
    if (fillMode == "none") {
        timing.fillMode = Timing::FillModeNone;
    } else if (fillMode == "backwards") {
        timing.fillMode = Timing::FillModeBackwards;
    } else if (fillMode == "both") {
        timing.fillMode = Timing::FillModeBoth;
    } else if (fillMode == "forwards") {
        timing.fillMode = Timing::FillModeForwards;
    } else {
        timing.fillMode = Timing::defaults().fillMode;
    }
}

bool TimingInput::setIterationStart(Timing& timing, double iterationStart, ExceptionState& exceptionState)
{
    ASSERT(std::isfinite(iterationStart));
    if (std::isnan(iterationStart) || iterationStart < 0) {
        exceptionState.throwTypeError("iterationStart must be non-negative.");
        return false;
    }
    timing.iterationStart = iterationStart;
    return true;
}

bool TimingInput::setIterationCount(Timing& timing, double iterationCount, ExceptionState& exceptionState)
{
    if (std::isnan(iterationCount) || iterationCount < 0) {
        exceptionState.throwTypeError("iterationCount must be non-negative.");
        return false;
    }
    timing.iterationCount = iterationCount;
    return true;
}

bool TimingInput::setIterationDuration(Timing& timing, const UnrestrictedDoubleOrString& iterationDuration, ExceptionState& exceptionState)
{
    static const char* errorMessage = "duration must be non-negative or auto.";

    if (iterationDuration.isUnrestrictedDouble()) {
        double durationNumber = iterationDuration.getAsUnrestrictedDouble();
        if (std::isnan(durationNumber) || durationNumber < 0) {
            exceptionState.throwTypeError(errorMessage);
            return false;
        }
        timing.iterationDuration = durationNumber / 1000;
        return true;
    }

    if (iterationDuration.getAsString() != "auto") {
        exceptionState.throwTypeError(errorMessage);
        return false;
    }

    timing.iterationDuration = Timing::defaults().iterationDuration;
    return true;
}

void TimingInput::setPlaybackRate(Timing& timing, double playbackRate)
{
    if (std::isfinite(playbackRate))
        timing.playbackRate = playbackRate;
    else
        timing.playbackRate = Timing::defaults().playbackRate;
}

void TimingInput::setPlaybackDirection(Timing& timing, const String& direction)
{
    if (direction == "reverse") {
        timing.direction = Timing::PlaybackDirectionReverse;
    } else if (direction == "alternate") {
        timing.direction = Timing::PlaybackDirectionAlternate;
    } else if (direction == "alternate-reverse") {
        timing.direction = Timing::PlaybackDirectionAlternateReverse;
    } else {
        timing.direction = Timing::defaults().direction;
    }
}

bool TimingInput::setTimingFunction(Timing& timing, const String& timingFunctionString, Document* document, ExceptionState& exceptionState)
{
    if (RefPtr<TimingFunction> timingFunction = AnimationInputHelpers::parseTimingFunction(timingFunctionString, document, exceptionState)) {
        timing.timingFunction = timingFunction;
        return true;
    }
    return false;
}

bool TimingInput::convert(const KeyframeEffectOptions& timingInput, Timing& timingOutput, Document* document, ExceptionState& exceptionState)
{
    setStartDelay(timingOutput, timingInput.delay());
    setEndDelay(timingOutput, timingInput.endDelay());
    setFillMode(timingOutput, timingInput.fill());

    if (!setIterationStart(timingOutput, timingInput.iterationStart(), exceptionState))
        return false;

    if (!setIterationCount(timingOutput, timingInput.iterations(), exceptionState))
        return false;

    if (!setIterationDuration(timingOutput, timingInput.duration(), exceptionState))
        return false;

    setPlaybackRate(timingOutput, timingInput.playbackRate());
    setPlaybackDirection(timingOutput, timingInput.direction());

    if (!setTimingFunction(timingOutput, timingInput.easing(), document, exceptionState))
        return false;

    timingOutput.assertValid();

    return true;
}

bool TimingInput::convert(double duration, Timing& timingOutput, ExceptionState& exceptionState)
{
    ASSERT(timingOutput == Timing::defaults());
    return setIterationDuration(timingOutput, UnrestrictedDoubleOrString::fromUnrestrictedDouble(duration), exceptionState);
}

} // namespace blink
