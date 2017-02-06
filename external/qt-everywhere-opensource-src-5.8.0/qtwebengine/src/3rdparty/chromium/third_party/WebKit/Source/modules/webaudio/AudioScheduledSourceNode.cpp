/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webaudio/AudioScheduledSourceNode.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/ExceptionCode.h"
#include "modules/EventModules.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "platform/audio/AudioUtilities.h"
#include "wtf/MathExtras.h"
#include <algorithm>

namespace blink {

const double AudioScheduledSourceHandler::UnknownTime = -1;

AudioScheduledSourceHandler::AudioScheduledSourceHandler(NodeType nodeType, AudioNode& node, float sampleRate)
    : AudioHandler(nodeType, node, sampleRate)
    , m_startTime(0)
    , m_endTime(UnknownTime)
    , m_playbackState(UNSCHEDULED_STATE)
{
}

void AudioScheduledSourceHandler::updateSchedulingInfo(
    size_t quantumFrameSize, AudioBus* outputBus, size_t& quantumFrameOffset, size_t& nonSilentFramesToProcess)
{
    ASSERT(outputBus);
    if (!outputBus)
        return;

    ASSERT(quantumFrameSize == ProcessingSizeInFrames);
    if (quantumFrameSize != ProcessingSizeInFrames)
        return;

    double sampleRate = this->sampleRate();

    // quantumStartFrame     : Start frame of the current time quantum.
    // quantumEndFrame       : End frame of the current time quantum.
    // startFrame            : Start frame for this source.
    // endFrame              : End frame for this source.
    size_t quantumStartFrame = context()->currentSampleFrame();
    size_t quantumEndFrame = quantumStartFrame + quantumFrameSize;
    size_t startFrame = AudioUtilities::timeToSampleFrame(m_startTime, sampleRate);
    size_t endFrame = m_endTime == UnknownTime ? 0 : AudioUtilities::timeToSampleFrame(m_endTime, sampleRate);

    // If we know the end time and it's already passed, then don't bother doing any more rendering this cycle.
    if (m_endTime != UnknownTime && endFrame <= quantumStartFrame)
        finish();

    PlaybackState state = playbackState();

    if (state == UNSCHEDULED_STATE || state == FINISHED_STATE || startFrame >= quantumEndFrame) {
        // Output silence.
        outputBus->zero();
        nonSilentFramesToProcess = 0;
        return;
    }

    // Check if it's time to start playing.
    if (state == SCHEDULED_STATE) {
        // Increment the active source count only if we're transitioning from SCHEDULED_STATE to PLAYING_STATE.
        setPlaybackState(PLAYING_STATE);
    }

    quantumFrameOffset = startFrame > quantumStartFrame ? startFrame - quantumStartFrame : 0;
    quantumFrameOffset = std::min(quantumFrameOffset, quantumFrameSize); // clamp to valid range
    nonSilentFramesToProcess = quantumFrameSize - quantumFrameOffset;

    if (!nonSilentFramesToProcess) {
        // Output silence.
        outputBus->zero();
        return;
    }

    // Handle silence before we start playing.
    // Zero any initial frames representing silence leading up to a rendering start time in the middle of the quantum.
    if (quantumFrameOffset) {
        for (unsigned i = 0; i < outputBus->numberOfChannels(); ++i)
            memset(outputBus->channel(i)->mutableData(), 0, sizeof(float) * quantumFrameOffset);
    }

    // Handle silence after we're done playing.
    // If the end time is somewhere in the middle of this time quantum, then zero out the
    // frames from the end time to the very end of the quantum.
    if (m_endTime != UnknownTime && endFrame >= quantumStartFrame && endFrame < quantumEndFrame) {
        size_t zeroStartFrame = endFrame - quantumStartFrame;
        size_t framesToZero = quantumFrameSize - zeroStartFrame;

        bool isSafe = zeroStartFrame < quantumFrameSize && framesToZero <= quantumFrameSize && zeroStartFrame + framesToZero <= quantumFrameSize;
        ASSERT(isSafe);

        if (isSafe) {
            if (framesToZero > nonSilentFramesToProcess)
                nonSilentFramesToProcess = 0;
            else
                nonSilentFramesToProcess -= framesToZero;

            for (unsigned i = 0; i < outputBus->numberOfChannels(); ++i)
                memset(outputBus->channel(i)->mutableData() + zeroStartFrame, 0, sizeof(float) * framesToZero);
        }

        finish();
    }

    return;
}

void AudioScheduledSourceHandler::start(double when, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    context()->recordUserGestureState();

    if (playbackState() != UNSCHEDULED_STATE) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "cannot call start more than once.");
        return;
    }

    if (when < 0) {
        exceptionState.throwDOMException(
            InvalidAccessError,
            ExceptionMessages::indexExceedsMinimumBound(
                "start time",
                when,
                0.0));
        return;
    }

    // The node is started. Add a reference to keep us alive so that audio will eventually get
    // played even if Javascript should drop all references to this node. The reference will get
    // dropped when the source has finished playing.
    context()->notifySourceNodeStartedProcessing(node());

    // This synchronizes with process(). updateSchedulingInfo will read some of the variables being
    // set here.
    MutexLocker processLocker(m_processLock);

    // If |when| < currentTime, the source must start now according to the spec.
    // So just set startTime to currentTime in this case to start the source now.
    m_startTime = std::max(when, context()->currentTime());

    setPlaybackState(SCHEDULED_STATE);
}

void AudioScheduledSourceHandler::stop(double when, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (playbackState() == UNSCHEDULED_STATE) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "cannot call stop without calling start first.");
        return;
    }

    if (when < 0) {
        exceptionState.throwDOMException(
            InvalidAccessError,
            ExceptionMessages::indexExceedsMinimumBound(
                "stop time",
                when,
                0.0));
        return;
    }

    // This synchronizes with process()
    MutexLocker processLocker(m_processLock);

    // stop() can be called more than once, with the last call to stop taking effect, unless the
    // source has already stopped due to earlier calls to stop. No exceptions are thrown in any
    // case.
    when = std::max(0.0, when);
    m_endTime = when;
}

void AudioScheduledSourceHandler::finishWithoutOnEnded()
{
    if (playbackState() != FINISHED_STATE) {
        // Let the context dereference this AudioNode.
        context()->notifySourceNodeFinishedProcessing(this);
        setPlaybackState(FINISHED_STATE);
    }
}

void AudioScheduledSourceHandler::finish()
{
    finishWithoutOnEnded();

    if (context()->getExecutionContext()) {
        context()->getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&AudioScheduledSourceHandler::notifyEnded, PassRefPtr<AudioScheduledSourceHandler>(this)));
    }
}

void AudioScheduledSourceHandler::notifyEnded()
{
    ASSERT(isMainThread());
    if (node())
        node()->dispatchEvent(Event::create(EventTypeNames::ended));
}

// ----------------------------------------------------------------

AudioScheduledSourceNode::AudioScheduledSourceNode(AbstractAudioContext& context)
    : AudioSourceNode(context)
    , ActiveScriptWrappable(this)
{
}

AudioScheduledSourceHandler& AudioScheduledSourceNode::audioScheduledSourceHandler() const
{
    return static_cast<AudioScheduledSourceHandler&>(handler());
}

void AudioScheduledSourceNode::start(ExceptionState& exceptionState)
{
    start(0, exceptionState);
}

void AudioScheduledSourceNode::start(double when, ExceptionState& exceptionState)
{
    audioScheduledSourceHandler().start(when, exceptionState);
}

void AudioScheduledSourceNode::stop(ExceptionState& exceptionState)
{
    stop(0, exceptionState);
}

void AudioScheduledSourceNode::stop(double when, ExceptionState& exceptionState)
{
    audioScheduledSourceHandler().stop(when, exceptionState);
}

EventListener* AudioScheduledSourceNode::onended()
{
    return getAttributeEventListener(EventTypeNames::ended);
}

void AudioScheduledSourceNode::setOnended(EventListener* listener)
{
    setAttributeEventListener(EventTypeNames::ended, listener);
}

bool AudioScheduledSourceNode::hasPendingActivity() const
{
    // To avoid the leak, a node should be collected regardless of its
    // playback state if the context is closed.
    if (context()->isContextClosed())
        return false;

    // If a node is scheduled or playing, do not collect the node prematurely
    // even its reference is out of scope. Then fire onended event if assigned.
    return audioScheduledSourceHandler().isPlayingOrScheduled();
}

} // namespace blink
