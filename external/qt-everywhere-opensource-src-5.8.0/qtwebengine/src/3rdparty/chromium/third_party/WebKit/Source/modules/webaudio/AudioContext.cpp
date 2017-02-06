// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioContext.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "modules/webaudio/AudioBufferCallback.h"
#include "platform/Histogram.h"
#include "platform/audio/AudioUtilities.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

// Don't allow more than this number of simultaneous AudioContexts
// talking to hardware.
const unsigned MaxHardwareContexts = 6;
static unsigned s_hardwareContextCount = 0;
static unsigned s_contextId = 0;

AbstractAudioContext* AudioContext::create(Document& document, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    UseCounter::countCrossOriginIframe(document, UseCounter::AudioContextCrossOriginIframe);

    if (s_hardwareContextCount >= MaxHardwareContexts) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexExceedsMaximumBound(
                "number of hardware contexts",
                s_hardwareContextCount,
                MaxHardwareContexts));
        return nullptr;
    }

    AudioContext* audioContext = new AudioContext(document);
    audioContext->suspendIfNeeded();

    if (!AudioUtilities::isValidAudioBufferSampleRate(audioContext->sampleRate())) {
        exceptionState.throwDOMException(
            NotSupportedError,
            ExceptionMessages::indexOutsideRange(
                "hardware sample rate",
                audioContext->sampleRate(),
                AudioUtilities::minAudioBufferSampleRate(),
                ExceptionMessages::InclusiveBound,
                AudioUtilities::maxAudioBufferSampleRate(),
                ExceptionMessages::InclusiveBound));
        return audioContext;
    }
    // This starts the audio thread. The destination node's
    // provideInput() method will now be called repeatedly to render
    // audio.  Each time provideInput() is called, a portion of the
    // audio stream is rendered. Let's call this time period a "render
    // quantum". NOTE: for now AudioContext does not need an explicit
    // startRendering() call from JavaScript.  We may want to consider
    // requiring it for symmetry with OfflineAudioContext.
    audioContext->startRendering();
    ++s_hardwareContextCount;
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: AudioContext::AudioContext(): %u #%u\n",
        audioContext, audioContext->m_contextId, s_hardwareContextCount);
#endif

    DEFINE_STATIC_LOCAL(SparseHistogram, maxChannelCountHistogram,
        ("WebAudio.AudioContext.MaxChannelsAvailable"));
    DEFINE_STATIC_LOCAL(SparseHistogram, sampleRateHistogram,
        ("WebAudio.AudioContext.HardwareSampleRate"));
    maxChannelCountHistogram.sample(audioContext->destination()->maxChannelCount());
    sampleRateHistogram.sample(audioContext->sampleRate());

    return audioContext;
}

AudioContext::AudioContext(Document& document)
    : AbstractAudioContext(&document)
    , m_contextId(s_contextId++)
{
}

AudioContext::~AudioContext()
{
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: AudioContext::~AudioContext(): %u\n", this, m_contextId);
#endif
}

DEFINE_TRACE(AudioContext)
{
    visitor->trace(m_closeResolver);
    AbstractAudioContext::trace(visitor);
}

ScriptPromise AudioContext::suspendContext(ScriptState* scriptState)
{
    ASSERT(isMainThread());
    AutoLocker locker(this);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (contextState() == Closed) {
        resolver->reject(
            DOMException::create(InvalidStateError, "Cannot suspend a context that has been closed"));
    } else {
        // Stop rendering now.
        if (destination())
            stopRendering();

        // Since we don't have any way of knowing when the hardware actually stops, we'll just
        // resolve the promise now.
        resolver->resolve();
    }

    return promise;
}

ScriptPromise AudioContext::resumeContext(ScriptState* scriptState)
{
    ASSERT(isMainThread());

    if (isContextClosed()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(
                InvalidAccessError,
                "cannot resume a closed AudioContext"));
    }

    recordUserGestureState();

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // Restart the destination node to pull on the audio graph.
    if (destination())
        startRendering();

    // Save the resolver which will get resolved when the destination node starts pulling on the
    // graph again.
    {
        AutoLocker locker(this);
        m_resumeResolvers.append(resolver);
    }

    return promise;
}

ScriptPromise AudioContext::closeContext(ScriptState* scriptState)
{
    if (isContextClosed()) {
        // We've already closed the context previously, but it hasn't yet been resolved, so just
        // create a new promise and reject it.
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(InvalidStateError,
                "Cannot close a context that is being closed or has already been closed."));
    }

    // Save the current sample rate for any subsequent decodeAudioData calls.
    setClosedContextSampleRate(sampleRate());

    m_closeResolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = m_closeResolver->promise();

    // Stop the audio context. This will stop the destination node from pulling audio anymore. And
    // since we have disconnected the destination from the audio graph, and thus has no references,
    // the destination node can GCed if JS has no references. uninitialize() will also resolve the Promise
    // created here.
    uninitialize();

    return promise;
}

void AudioContext::didClose()
{
    // This is specific to AudioContexts. OfflineAudioContexts
    // are closed in their completion event.
    setContextState(Closed);

    ASSERT(s_hardwareContextCount);
    --s_hardwareContextCount;

    if (m_closeResolver)
        m_closeResolver->resolve();
}

bool AudioContext::isContextClosed() const
{
    return m_closeResolver || AbstractAudioContext::isContextClosed();
}

void AudioContext::stopRendering()
{
    ASSERT(isMainThread());
    ASSERT(destination());

    if (contextState() == Running) {
        destination()->audioDestinationHandler().stopRendering();
        setContextState(Suspended);
        deferredTaskHandler().clearHandlersToBeDeleted();
    }
}

} // namespace blink
