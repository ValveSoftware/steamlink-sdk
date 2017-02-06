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

#include "modules/webaudio/OfflineAudioContext.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/webaudio/AudioListener.h"
#include "modules/webaudio/DeferredTaskHandler.h"
#include "modules/webaudio/OfflineAudioCompletionEvent.h"
#include "modules/webaudio/OfflineAudioDestinationNode.h"

#include "platform/Histogram.h"
#include "platform/audio/AudioUtilities.h"

namespace blink {

OfflineAudioContext* OfflineAudioContext::create(ExecutionContext* context, unsigned numberOfChannels, unsigned numberOfFrames, float sampleRate, ExceptionState& exceptionState)
{
    // FIXME: add support for workers.
    if (!context || !context->isDocument()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "Workers are not supported.");
        return nullptr;
    }

    Document* document = toDocument(context);

    if (!numberOfFrames) {
        exceptionState.throwDOMException(SyntaxError, "number of frames cannot be zero.");
        return nullptr;
    }

    if (numberOfChannels > AbstractAudioContext::maxNumberOfChannels()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange<unsigned>(
                "number of channels",
                numberOfChannels,
                0,
                ExceptionMessages::InclusiveBound,
                AbstractAudioContext::maxNumberOfChannels(),
                ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    if (!AudioUtilities::isValidAudioBufferSampleRate(sampleRate)) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange(
                "sampleRate", sampleRate,
                AudioUtilities::minAudioBufferSampleRate(), ExceptionMessages::InclusiveBound,
                AudioUtilities::maxAudioBufferSampleRate(), ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    OfflineAudioContext* audioContext = new OfflineAudioContext(document, numberOfChannels, numberOfFrames, sampleRate, exceptionState);

    if (!audioContext->destination()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "OfflineAudioContext(" + String::number(numberOfChannels)
            + ", " + String::number(numberOfFrames)
            + ", " + String::number(sampleRate)
            + ")");
    }

    DEFINE_STATIC_LOCAL(SparseHistogram, offlineContextChannelCountHistogram,
        ("WebAudio.OfflineAudioContext.ChannelCount"));
    // Arbitrarly limit the maximum length to 1 million frames (about 20 sec
    // at 48kHz).  The number of buckets is fairly arbitrary.
    DEFINE_STATIC_LOCAL(CustomCountHistogram, offlineContextLengthHistogram,
        ("WebAudio.OfflineAudioContext.Length", 1, 1000000, 50));
    // The limits are the min and max AudioBuffer sample rates currently
    // supported.  We use explicit values here instead of
    // AudioUtilities::minAudioBufferSampleRate() and
    // AudioUtilities::maxAudioBufferSampleRate().  The number of buckets is
    // fairly arbitrary.
    DEFINE_STATIC_LOCAL(CustomCountHistogram, offlineContextSampleRateHistogram,
        ("WebAudio.OfflineAudioContext.SampleRate", 3000, 192000, 50));

    offlineContextChannelCountHistogram.sample(numberOfChannels);
    offlineContextLengthHistogram.count(numberOfFrames);
    offlineContextSampleRateHistogram.count(sampleRate);

    audioContext->suspendIfNeeded();
    return audioContext;
}

OfflineAudioContext::OfflineAudioContext(Document* document, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState& exceptionState)
    : AbstractAudioContext(document, numberOfChannels, numberOfFrames, sampleRate)
    , m_isRenderingStarted(false)
    , m_totalRenderFrames(numberOfFrames)
{
    // Create a new destination for offline rendering.
    m_renderTarget = AudioBuffer::create(numberOfChannels, numberOfFrames, sampleRate);

    // Throw an exception if the render target is not ready.
    if (m_renderTarget) {
        m_destinationNode = OfflineAudioDestinationNode::create(this, m_renderTarget.get());
        initialize();
    } else {
        exceptionState.throwRangeError(ExceptionMessages::failedToConstruct(
            "OfflineAudioContext",
            "failed to create OfflineAudioContext(" +
            String::number(numberOfChannels) + ", " +
            String::number(numberOfFrames) + ", " +
            String::number(sampleRate) + ")"));
    }
}

OfflineAudioContext::~OfflineAudioContext()
{
}

DEFINE_TRACE(OfflineAudioContext)
{
    visitor->trace(m_renderTarget);
    visitor->trace(m_completeResolver);
    visitor->trace(m_scheduledSuspends);
    AbstractAudioContext::trace(visitor);
}

ScriptPromise OfflineAudioContext::startOfflineRendering(ScriptState* scriptState)
{
    ASSERT(isMainThread());

    // Calling close() on an OfflineAudioContext is not supported/allowed,
    // but it might well have been stopped by its execution context.
    //
    // See: crbug.com/435867
    if (isContextClosed()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(
                InvalidStateError,
                "cannot call startRendering on an OfflineAudioContext in a stopped state."));
    }

    // If the context is not in the suspended state (i.e. running), reject the promise.
    if (contextState() != AudioContextState::Suspended) {
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(
                InvalidStateError,
                "cannot startRendering when an OfflineAudioContext is " + state()));
    }

    // Can't call startRendering more than once.  Return a rejected promise now.
    if (m_isRenderingStarted) {
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(
                InvalidStateError,
                "cannot call startRendering more than once"));
    }

    ASSERT(!m_isRenderingStarted);

    m_completeResolver = ScriptPromiseResolver::create(scriptState);

    // Start rendering and return the promise.
    m_isRenderingStarted = true;
    setContextState(Running);
    destinationHandler().startRendering();

    return m_completeResolver->promise();
}

ScriptPromise OfflineAudioContext::closeContext(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidAccessError,
            "cannot close an OfflineAudioContext."));
}

ScriptPromise OfflineAudioContext::suspendContext(ScriptState* scriptState)
{
    // This CANNOT be called on OfflineAudioContext; this is only to implement
    // the pure virtual interface from AbstractAudioContext.
    RELEASE_NOTREACHED();

    return ScriptPromise();
}

ScriptPromise OfflineAudioContext::suspendContext(ScriptState* scriptState, double when)
{
    ASSERT(isMainThread());

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // The render thread does not exist; reject the promise.
    if (!destinationHandler().offlineRenderThread()) {
        resolver->reject(DOMException::create(InvalidStateError,
            "the rendering is already finished"));
        return promise;
    }

    // The specified suspend time is negative; reject the promise.
    if (when < 0) {
        resolver->reject(DOMException::create(InvalidStateError,
            "negative suspend time (" + String::number(when) + ") is not allowed"));
        return promise;
    }

    // Quantize (to the lower boundary) the suspend time by the render quantum.
    size_t frame = when * sampleRate();
    frame -= frame % destinationHandler().renderQuantumFrames();

    // The suspend time should be earlier than the total render frame. If the
    // requested suspension time is equal to the total render frame, the promise
    // will be rejected.
    if (m_totalRenderFrames <= frame) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot schedule a suspend at frame " + String::number(frame) +
            " (" + String::number(when) + " seconds) " +
            "because it is greater than or equal to the total render duration of " +
            String::number(m_totalRenderFrames) + " frames"));
        return promise;
    }

    // The specified suspend time is in the past; reject the promise.
    if (frame < currentSampleFrame()) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot schedule a suspend at frame " +
            String::number(frame) + " (" + String::number(when) +
            " seconds) because it is earlier than the current frame of " +
            String::number(currentSampleFrame()) + " (" +
            String::number(currentTime()) + " seconds)"));
        return promise;
    }

    // Wait until the suspend map is available for the insertion. Here we should
    // use AutoLocker because it locks the graph from the main thread.
    AutoLocker locker(this);

    // If there is a duplicate suspension at the same quantized frame,
    // reject the promise.
    if (m_scheduledSuspends.contains(frame)) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot schedule more than one suspend at frame " +
            String::number(frame) + " (" +
            String::number(when) + " seconds)"));
        return promise;
    }

    m_scheduledSuspends.add(frame, resolver);

    return promise;
}

ScriptPromise OfflineAudioContext::resumeContext(ScriptState* scriptState)
{
    ASSERT(isMainThread());

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // If the rendering has not started, reject the promise.
    if (!m_isRenderingStarted) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot resume an offline context that has not started"));
        return promise;
    }

    // If the context is in a closed state, reject the promise.
    if (contextState() == AudioContextState::Closed) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot resume a closed offline context"));
        return promise;
    }

    // If the context is already running, resolve the promise without altering
    // the current state or starting the rendering loop.
    if (contextState() == AudioContextState::Running) {
        resolver->resolve();
        return promise;
    }

    ASSERT(contextState() == AudioContextState::Suspended);

    // If the context is suspended, resume rendering by setting the state to
    // "Running". and calling startRendering(). Note that resuming is possible
    // only after the rendering started.
    setContextState(Running);
    destinationHandler().startRendering();

    // Resolve the promise immediately.
    resolver->resolve();

    return promise;
}

void OfflineAudioContext::fireCompletionEvent()
{
    ASSERT(isMainThread());

    // We set the state to closed here so that the oncomplete event handler sees
    // that the context has been closed.
    setContextState(Closed);

    AudioBuffer* renderedBuffer = renderTarget();

    ASSERT(renderedBuffer);
    if (!renderedBuffer)
        return;

    // Avoid firing the event if the document has already gone away.
    if (getExecutionContext()) {
        // Call the offline rendering completion event listener and resolve the
        // promise too.
        dispatchEvent(OfflineAudioCompletionEvent::create(renderedBuffer));
        m_completeResolver->resolve(renderedBuffer);
    } else {
        // The resolver should be rejected when the execution context is gone.
        m_completeResolver->reject(DOMException::create(InvalidStateError,
            "the execution context does not exist"));
    }
}

bool OfflineAudioContext::handlePreOfflineRenderTasks()
{
    ASSERT(isAudioThread());

    // OfflineGraphAutoLocker here locks the audio graph for this scope. Note
    // that this locker does not use tryLock() inside because the timing of
    // suspension MUST NOT be delayed.
    OfflineGraphAutoLocker locker(this);

    // Update the dirty state of the listener.
    listener()->updateState();

    deferredTaskHandler().handleDeferredTasks();
    handleStoppableSourceNodes();

    return shouldSuspend();
}

void OfflineAudioContext::handlePostOfflineRenderTasks()
{
    ASSERT(isAudioThread());

    // OfflineGraphAutoLocker here locks the audio graph for the same reason
    // above in |handlePreOfflineRenderTasks|.
    OfflineGraphAutoLocker locker(this);

    deferredTaskHandler().breakConnections();
    releaseFinishedSourceNodes();
    deferredTaskHandler().handleDeferredTasks();
    deferredTaskHandler().requestToDeleteHandlersOnMainThread();
}


OfflineAudioDestinationHandler& OfflineAudioContext::destinationHandler()
{
    return static_cast<OfflineAudioDestinationHandler&>(destination()->audioDestinationHandler());
}

void OfflineAudioContext::resolveSuspendOnMainThread(size_t frame)
{
    ASSERT(isMainThread());

    // Suspend the context first. This will fire onstatechange event.
    setContextState(Suspended);

    // Wait until the suspend map is available for the removal.
    AutoLocker locker(this);

    // If the context is going away, m_scheduledSuspends could have had all its entries removed.
    // Check for that here.
    if (m_scheduledSuspends.size()) {
        // |frame| must exist in the map.
        DCHECK(m_scheduledSuspends.contains(frame));

        SuspendMap::iterator it = m_scheduledSuspends.find(frame);
        it->value->resolve();

        m_scheduledSuspends.remove(it);
    }
}

void OfflineAudioContext::rejectPendingResolvers()
{
    ASSERT(isMainThread());

    // Wait until the suspend map is available for removal.
    AutoLocker locker(this);

    // Offline context is going away so reject any promises that are still pending.

    for (auto& pendingSuspendResolver : m_scheduledSuspends) {
        pendingSuspendResolver.value->reject(DOMException::create(
            InvalidStateError, "Audio context is going away"));
    }

    m_scheduledSuspends.clear();
    ASSERT(m_resumeResolvers.size() == 0);

    rejectPendingDecodeAudioDataResolvers();
}

bool OfflineAudioContext::shouldSuspend()
{
    ASSERT(isAudioThread());

    // Note that the GraphLock is required before this check. Since this needs
    // to run on the audio thread, OfflineGraphAutoLocker must be used.
    if (m_scheduledSuspends.contains(currentSampleFrame()))
        return true;

    return false;
}

} // namespace blink

