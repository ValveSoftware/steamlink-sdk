/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "modules/webaudio/AbstractAudioContext.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/webaudio/AnalyserNode.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioBufferCallback.h"
#include "modules/webaudio/AudioBufferSourceNode.h"
#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/AudioListener.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BiquadFilterNode.h"
#include "modules/webaudio/ChannelMergerNode.h"
#include "modules/webaudio/ChannelSplitterNode.h"
#include "modules/webaudio/ConvolverNode.h"
#include "modules/webaudio/DefaultAudioDestinationNode.h"
#include "modules/webaudio/DelayNode.h"
#include "modules/webaudio/DynamicsCompressorNode.h"
#include "modules/webaudio/GainNode.h"
#include "modules/webaudio/IIRFilterNode.h"
#include "modules/webaudio/MediaElementAudioSourceNode.h"
#include "modules/webaudio/MediaStreamAudioDestinationNode.h"
#include "modules/webaudio/MediaStreamAudioSourceNode.h"
#include "modules/webaudio/OfflineAudioCompletionEvent.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "modules/webaudio/OfflineAudioDestinationNode.h"
#include "modules/webaudio/OscillatorNode.h"
#include "modules/webaudio/PannerNode.h"
#include "modules/webaudio/PeriodicWave.h"
#include "modules/webaudio/PeriodicWaveConstraints.h"
#include "modules/webaudio/ScriptProcessorNode.h"
#include "modules/webaudio/StereoPannerNode.h"
#include "modules/webaudio/WaveShaperNode.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/UserGestureIndicator.h"
#include "platform/audio/IIRFilter.h"
#include "public/platform/Platform.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

enum UserGestureRecord {
    UserGestureRequiredAndAvailable = 0,
    UserGestureRequiredAndNotAvailable,
    UserGestureNotRequiredAndAvailable,
    UserGestureNotRequiredAndNotAvailable,
    UserGestureRecordMax
};

} // anonymous namespace

AbstractAudioContext* AbstractAudioContext::create(Document& document, ExceptionState& exceptionState)
{
    return AudioContext::create(document, exceptionState);
}

// FIXME(dominicc): Devolve these constructors to AudioContext
// and OfflineAudioContext respectively.

// Constructor for rendering to the audio hardware.
AbstractAudioContext::AbstractAudioContext(Document* document)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(document)
    , m_destinationNode(nullptr)
    , m_isCleared(false)
    , m_isResolvingResumePromises(false)
    , m_userGestureRequired(false)
    , m_connectionCount(0)
    , m_deferredTaskHandler(DeferredTaskHandler::create())
    , m_contextState(Suspended)
    , m_closedContextSampleRate(-1)
    , m_periodicWaveSine(nullptr)
    , m_periodicWaveSquare(nullptr)
    , m_periodicWaveSawtooth(nullptr)
    , m_periodicWaveTriangle(nullptr)
{
    // TODO(mlamouri): we might want to use other ways of checking for this but
    // in order to record metrics, re-using the HTMLMediaElement setting is
    // probably the simplest solution.
    if (document->settings() && document->settings()->mediaPlaybackRequiresUserGesture())
        m_userGestureRequired = true;

    m_destinationNode = DefaultAudioDestinationNode::create(this);

    initialize();
}

// Constructor for offline (non-realtime) rendering.
AbstractAudioContext::AbstractAudioContext(Document* document, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(document)
    , m_destinationNode(nullptr)
    , m_isCleared(false)
    , m_isResolvingResumePromises(false)
    , m_userGestureRequired(false)
    , m_connectionCount(0)
    , m_deferredTaskHandler(DeferredTaskHandler::create())
    , m_contextState(Suspended)
    , m_closedContextSampleRate(-1)
    , m_periodicWaveSine(nullptr)
    , m_periodicWaveSquare(nullptr)
    , m_periodicWaveSawtooth(nullptr)
    , m_periodicWaveTriangle(nullptr)
{
    // TODO(mlamouri): we might want to use other ways of checking for this but
    // in order to record metrics, re-using the HTMLMediaElement setting is
    // probably the simplest solution.
    if (document->settings() && document->settings()->mediaPlaybackRequiresUserGesture())
        m_userGestureRequired = true;
}

AbstractAudioContext::~AbstractAudioContext()
{
    deferredTaskHandler().contextWillBeDestroyed();
    // AudioNodes keep a reference to their context, so there should be no way to be in the destructor if there are still AudioNodes around.
    ASSERT(!isDestinationInitialized());
    ASSERT(!m_activeSourceNodes.size());
    ASSERT(!m_finishedSourceHandlers.size());
    ASSERT(!m_isResolvingResumePromises);
    ASSERT(!m_resumeResolvers.size());
}

void AbstractAudioContext::initialize()
{
    if (isDestinationInitialized())
        return;

    FFTFrame::initialize();

    if (m_destinationNode) {
        m_destinationNode->handler().initialize();
        // The AudioParams in the listener need access to the destination node, so only create the
        // listener if the destination node exists.
        m_listener = AudioListener::create(*this);
    }
}

void AbstractAudioContext::clear()
{
    m_destinationNode.clear();
    // The audio rendering thread is dead.  Nobody will schedule AudioHandler
    // deletion.  Let's do it ourselves.
    deferredTaskHandler().clearHandlersToBeDeleted();
    m_isCleared = true;
}

void AbstractAudioContext::uninitialize()
{
    ASSERT(isMainThread());

    if (!isDestinationInitialized())
        return;

    // This stops the audio thread and all audio rendering.
    if (m_destinationNode)
        m_destinationNode->handler().uninitialize();

    // Get rid of the sources which may still be playing.
    releaseActiveSourceNodes();

    // Reject any pending resolvers before we go away.
    rejectPendingResolvers();
    didClose();

    ASSERT(m_listener);
    m_listener->waitForHRTFDatabaseLoaderThreadCompletion();

    clear();
}

void AbstractAudioContext::stop()
{
    uninitialize();
}

bool AbstractAudioContext::hasPendingActivity() const
{
    // There's no pending activity if the audio context has been cleared.
    return !m_isCleared;
}

AudioDestinationNode* AbstractAudioContext::destination() const
{
    // Cannot be called from the audio thread because this method touches objects managed by Oilpan,
    // and the audio thread is not managed by Oilpan.
    ASSERT(!isAudioThread());
    return m_destinationNode;
}

void AbstractAudioContext::throwExceptionForClosedState(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(InvalidStateError, "AudioContext has been closed.");
}

AudioBuffer* AbstractAudioContext::createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState& exceptionState)
{
    // It's ok to call createBuffer, even if the context is closed because the AudioBuffer doesn't
    // really "belong" to any particular context.

    AudioBuffer* buffer = AudioBuffer::create(numberOfChannels, numberOfFrames, sampleRate, exceptionState);

    if (buffer) {
        // Only record the data if the creation succeeded.
        DEFINE_STATIC_LOCAL(SparseHistogram, audioBufferChannelsHistogram,
            ("WebAudio.AudioBuffer.NumberOfChannels"));

        // Arbitrarly limit the maximum length to 1 million frames (about 20 sec
        // at 48kHz).  The number of buckets is fairly arbitrary.
        DEFINE_STATIC_LOCAL(CustomCountHistogram, audioBufferLengthHistogram,
            ("WebAudio.AudioBuffer.Length", 1, 1000000, 50));
        // The limits are the min and max AudioBuffer sample rates currently
        // supported.  We use explicit values here instead of
        // AudioUtilities::minAudioBufferSampleRate() and
        // AudioUtilities::maxAudioBufferSampleRate().  The number of buckets is
        // fairly arbitrary.
        DEFINE_STATIC_LOCAL(CustomCountHistogram, audioBufferSampleRateHistogram,
            ("WebAudio.AudioBuffer.SampleRate", 3000, 192000, 60));

        audioBufferChannelsHistogram.sample(numberOfChannels);
        audioBufferLengthHistogram.count(numberOfFrames);
        audioBufferSampleRateHistogram.count(sampleRate);

        // Compute the ratio of the buffer rate and the context rate so we know
        // how often the buffer needs to be resampled to match the context.  For
        // the histogram, we multiply the ratio by 100 and round to the nearest
        // integer.  If the context is closed, don't record this because we
        // don't have a sample rate for closed context.
        if (!isContextClosed()) {
            // The limits are choosen from 100*(3000/192000) = 1.5625 and
            // 100*(192000/3000) = 6400, where 3000 and 192000 are the current
            // min and max sample rates possible for an AudioBuffer.  The number
            // of buckets is fairly arbitrary.
            DEFINE_STATIC_LOCAL(CustomCountHistogram, audioBufferSampleRateRatioHistogram,
                ("WebAudio.AudioBuffer.SampleRateRatio", 1, 6400, 50));
            float ratio = 100 * sampleRate / this->sampleRate();
            audioBufferSampleRateRatioHistogram.count(static_cast<int>(0.5 + ratio));
        }
    }

    return buffer;
}

ScriptPromise AbstractAudioContext::decodeAudioData(ScriptState* scriptState, DOMArrayBuffer* audioData, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    ASSERT(audioData);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    float rate = isContextClosed() ? closedContextSampleRate() : sampleRate();

    ASSERT(rate > 0);

    m_decodeAudioResolvers.add(resolver);
    m_audioDecoder.decodeAsync(audioData, rate, successCallback, errorCallback, resolver, this);

    return promise;
}

void AbstractAudioContext::handleDecodeAudioData(AudioBuffer* audioBuffer, ScriptPromiseResolver* resolver, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback)
{
    ASSERT(isMainThread());

    if (audioBuffer) {
        // Resolve promise successfully and run the success callback
        resolver->resolve(audioBuffer);
        if (successCallback)
            successCallback->handleEvent(audioBuffer);
    } else {
        // Reject the promise and run the error callback
        DOMException* error = DOMException::create(EncodingError, "Unable to decode audio data");
        resolver->reject(error);
        if (errorCallback)
            errorCallback->handleEvent(error);
    }

    // We've resolved the promise.  Remove it now.
    ASSERT(m_decodeAudioResolvers.contains(resolver));
    m_decodeAudioResolvers.remove(resolver);
}

AudioBufferSourceNode* AbstractAudioContext::createBufferSource(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    AudioBufferSourceNode* node = AudioBufferSourceNode::create(*this, exceptionState);

    // Do not add a reference to this source node now. The reference will be added when start() is
    // called.

    return node;
}

MediaElementAudioSourceNode* AbstractAudioContext::createMediaElementSource(HTMLMediaElement* mediaElement, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return MediaElementAudioSourceNode::create(*this, *mediaElement, exceptionState);
}

MediaStreamAudioSourceNode* AbstractAudioContext::createMediaStreamSource(MediaStream* mediaStream, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return MediaStreamAudioSourceNode::create(*this, *mediaStream, exceptionState);
}

MediaStreamAudioDestinationNode* AbstractAudioContext::createMediaStreamDestination(ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    // Set number of output channels to stereo by default.
    return MediaStreamAudioDestinationNode::create(*this, 2, exceptionState);
}

ScriptProcessorNode* AbstractAudioContext::createScriptProcessor(ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    return ScriptProcessorNode::create(*this, exceptionState);
}

ScriptProcessorNode* AbstractAudioContext::createScriptProcessor(size_t bufferSize, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    return ScriptProcessorNode::create(*this, bufferSize, exceptionState);
}

ScriptProcessorNode* AbstractAudioContext::createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    return ScriptProcessorNode::create(*this, bufferSize, numberOfInputChannels, exceptionState);
}

ScriptProcessorNode* AbstractAudioContext::createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return ScriptProcessorNode::create(
        *this,
        bufferSize,
        numberOfInputChannels,
        numberOfOutputChannels,
        exceptionState);
}

StereoPannerNode* AbstractAudioContext::createStereoPanner(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return StereoPannerNode::create(*this, exceptionState);
}

BiquadFilterNode* AbstractAudioContext::createBiquadFilter(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return BiquadFilterNode::create(*this, exceptionState);
}

WaveShaperNode* AbstractAudioContext::createWaveShaper(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return WaveShaperNode::create(*this, exceptionState);
}

PannerNode* AbstractAudioContext::createPanner(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return PannerNode::create(*this, exceptionState);
}

ConvolverNode* AbstractAudioContext::createConvolver(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return ConvolverNode::create(*this, exceptionState);
}

DynamicsCompressorNode* AbstractAudioContext::createDynamicsCompressor(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return DynamicsCompressorNode::create(*this, exceptionState);
}

AnalyserNode* AbstractAudioContext::createAnalyser(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return AnalyserNode::create(*this, exceptionState);
}

GainNode* AbstractAudioContext::createGain(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return GainNode::create(*this, exceptionState);
}

DelayNode* AbstractAudioContext::createDelay(ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    return DelayNode::create(*this, exceptionState);
}

DelayNode* AbstractAudioContext::createDelay(double maxDelayTime, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return DelayNode::create(*this, maxDelayTime, exceptionState);
}

ChannelSplitterNode* AbstractAudioContext::createChannelSplitter(ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    return ChannelSplitterNode::create(*this, exceptionState);
}

ChannelSplitterNode* AbstractAudioContext::createChannelSplitter(size_t numberOfOutputs, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return ChannelSplitterNode::create(*this, numberOfOutputs, exceptionState);
}

ChannelMergerNode* AbstractAudioContext::createChannelMerger(ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    return ChannelMergerNode::create(*this, exceptionState);
}

ChannelMergerNode* AbstractAudioContext::createChannelMerger(size_t numberOfInputs, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return ChannelMergerNode::create(*this, numberOfInputs, exceptionState);
}

OscillatorNode* AbstractAudioContext::createOscillator(ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return OscillatorNode::create(*this, exceptionState);
}

PeriodicWave* AbstractAudioContext::createPeriodicWave(DOMFloat32Array* real, DOMFloat32Array* imag, ExceptionState& exceptionState)
{
    DCHECK(isMainThread());

    return PeriodicWave::create(*this, real, imag, false, exceptionState);
}

PeriodicWave* AbstractAudioContext::createPeriodicWave(DOMFloat32Array* real, DOMFloat32Array* imag, const PeriodicWaveConstraints& options, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    bool disable = options.hasDisableNormalization() ? options.disableNormalization() : false;

    return PeriodicWave::create(*this, real, imag, disable, exceptionState);
}

IIRFilterNode* AbstractAudioContext::createIIRFilter(Vector<double> feedforwardCoef, Vector<double> feedbackCoef,  ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    return IIRFilterNode::create(*this, feedforwardCoef, feedbackCoef, exceptionState);
}

PeriodicWave* AbstractAudioContext::periodicWave(int type)
{
    switch (type) {
    case OscillatorHandler::SINE:
        // Initialize the table if necessary
        if (!m_periodicWaveSine)
            m_periodicWaveSine = PeriodicWave::createSine(sampleRate());
        return m_periodicWaveSine;
    case OscillatorHandler::SQUARE:
        // Initialize the table if necessary
        if (!m_periodicWaveSquare)
            m_periodicWaveSquare = PeriodicWave::createSquare(sampleRate());
        return m_periodicWaveSquare;
    case OscillatorHandler::SAWTOOTH:
        // Initialize the table if necessary
        if (!m_periodicWaveSawtooth)
            m_periodicWaveSawtooth = PeriodicWave::createSawtooth(sampleRate());
        return m_periodicWaveSawtooth;
    case OscillatorHandler::TRIANGLE:
        // Initialize the table if necessary
        if (!m_periodicWaveTriangle)
            m_periodicWaveTriangle = PeriodicWave::createTriangle(sampleRate());
        return m_periodicWaveTriangle;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

void AbstractAudioContext::recordUserGestureState()
{
    DEFINE_STATIC_LOCAL(EnumerationHistogram, userGestureHistogram, ("WebAudio.UserGesture", UserGestureRecordMax));

    if (!m_userGestureRequired) {
        if (UserGestureIndicator::processingUserGesture())
            userGestureHistogram.count(UserGestureNotRequiredAndAvailable);
        else
            userGestureHistogram.count(UserGestureNotRequiredAndNotAvailable);
        return;
    }
    if (!UserGestureIndicator::processingUserGesture()) {
        userGestureHistogram.count(UserGestureRequiredAndNotAvailable);
        return;
    }
    userGestureHistogram.count(UserGestureRequiredAndAvailable);
    m_userGestureRequired = false;
}

String AbstractAudioContext::state() const
{
    // These strings had better match the strings for AudioContextState in AudioContext.idl.
    switch (m_contextState) {
    case Suspended:
        return "suspended";
    case Running:
        return "running";
    case Closed:
        return "closed";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void AbstractAudioContext::setContextState(AudioContextState newState)
{
    ASSERT(isMainThread());

    // Validate the transitions.  The valid transitions are Suspended->Running, Running->Suspended,
    // and anything->Closed.
    switch (newState) {
    case Suspended:
        ASSERT(m_contextState == Running);
        break;
    case Running:
        ASSERT(m_contextState == Suspended);
        break;
    case Closed:
        ASSERT(m_contextState != Closed);
        break;
    }

    if (newState == m_contextState) {
        // ASSERTs above failed; just return.
        return;
    }

    m_contextState = newState;

    // Notify context that state changed
    if (getExecutionContext())
        getExecutionContext()->postTask(BLINK_FROM_HERE, createSameThreadTask(&AbstractAudioContext::notifyStateChange, wrapPersistent(this)));
}

void AbstractAudioContext::notifyStateChange()
{
    dispatchEvent(Event::create(EventTypeNames::statechange));
}

void AbstractAudioContext::notifySourceNodeFinishedProcessing(AudioHandler* handler)
{
    ASSERT(isAudioThread());
    m_finishedSourceHandlers.append(handler);
}

void AbstractAudioContext::removeFinishedSourceNodes()
{
    ASSERT(isMainThread());
    AutoLocker locker(this);
    // Quadratic worst case, but sizes of both vectors are considered
    // manageable, especially |m_finishedSourceNodes| is likely to be short.
    for (AudioNode* node : m_finishedSourceNodes) {
        size_t i = m_activeSourceNodes.find(node);
        if (i != kNotFound)
            m_activeSourceNodes.remove(i);
    }
    m_finishedSourceNodes.clear();
}

void AbstractAudioContext::releaseFinishedSourceNodes()
{
    ASSERT(isGraphOwner());
    ASSERT(isAudioThread());
    bool didRemove = false;
    for (AudioHandler* handler : m_finishedSourceHandlers) {
        for (AudioNode* node : m_activeSourceNodes) {
            if (m_finishedSourceNodes.contains(node))
                continue;
            if (handler == &node->handler()) {
                handler->breakConnection();
                m_finishedSourceNodes.add(node);
                didRemove = true;
                break;
            }
        }
    }
    if (didRemove)
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&AbstractAudioContext::removeFinishedSourceNodes, wrapCrossThreadPersistent(this)));

    m_finishedSourceHandlers.clear();
}

void AbstractAudioContext::notifySourceNodeStartedProcessing(AudioNode* node)
{
    ASSERT(isMainThread());
    AutoLocker locker(this);

    m_activeSourceNodes.append(node);
    node->handler().makeConnection();
}

void AbstractAudioContext::releaseActiveSourceNodes()
{
    ASSERT(isMainThread());
    for (auto& sourceNode : m_activeSourceNodes)
        sourceNode->handler().breakConnection();

    m_activeSourceNodes.clear();
}

void AbstractAudioContext::handleStoppableSourceNodes()
{
    ASSERT(isGraphOwner());

    // Find AudioBufferSourceNodes to see if we can stop playing them.
    for (AudioNode* node : m_activeSourceNodes) {
        // If the AudioNode has been marked as finished and released by
        // the audio thread, but not yet removed by the main thread
        // (see releaseActiveSourceNodes() above), |node| must not be
        // touched as its handler may have been released already.
        if (m_finishedSourceNodes.contains(node))
            continue;
        if (node->handler().getNodeType() == AudioHandler::NodeTypeAudioBufferSource) {
            AudioBufferSourceNode* sourceNode = static_cast<AudioBufferSourceNode*>(node);
            sourceNode->audioBufferSourceHandler().handleStoppableSourceNode();
        }
    }
}

void AbstractAudioContext::handlePreRenderTasks()
{
    ASSERT(isAudioThread());

    // At the beginning of every render quantum, try to update the internal rendering graph state (from main thread changes).
    // It's OK if the tryLock() fails, we'll just take slightly longer to pick up the changes.
    if (tryLock()) {
        deferredTaskHandler().handleDeferredTasks();

        resolvePromisesForResume();

        // Check to see if source nodes can be stopped because the end time has passed.
        handleStoppableSourceNodes();

        // Update the dirty state of the listener.
        listener()->updateState();

        unlock();
    }
}

void AbstractAudioContext::handlePostRenderTasks()
{
    ASSERT(isAudioThread());

    // Must use a tryLock() here too.  Don't worry, the lock will very rarely be contended and this method is called frequently.
    // The worst that can happen is that there will be some nodes which will take slightly longer than usual to be deleted or removed
    // from the render graph (in which case they'll render silence).
    if (tryLock()) {
        // Take care of AudioNode tasks where the tryLock() failed previously.
        deferredTaskHandler().breakConnections();

        // Dynamically clean up nodes which are no longer needed.
        releaseFinishedSourceNodes();

        deferredTaskHandler().handleDeferredTasks();
        deferredTaskHandler().requestToDeleteHandlersOnMainThread();

        unlock();
    }
}

void AbstractAudioContext::resolvePromisesForResumeOnMainThread()
{
    ASSERT(isMainThread());
    AutoLocker locker(this);

    for (auto& resolver : m_resumeResolvers) {
        if (m_contextState == Closed) {
            resolver->reject(
                DOMException::create(InvalidStateError, "Cannot resume a context that has been closed"));
        } else {
            resolver->resolve();
        }
    }

    m_resumeResolvers.clear();
    m_isResolvingResumePromises = false;
}

void AbstractAudioContext::resolvePromisesForResume()
{
    // This runs inside the AbstractAudioContext's lock when handling pre-render tasks.
    ASSERT(isAudioThread());
    ASSERT(isGraphOwner());

    // Resolve any pending promises created by resume(). Only do this if we haven't already started
    // resolving these promises. This gets called very often and it takes some time to resolve the
    // promises in the main thread.
    if (!m_isResolvingResumePromises && m_resumeResolvers.size() > 0) {
        m_isResolvingResumePromises = true;
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, crossThreadBind(&AbstractAudioContext::resolvePromisesForResumeOnMainThread, wrapCrossThreadPersistent(this)));
    }
}

void AbstractAudioContext::rejectPendingDecodeAudioDataResolvers()
{
    // Now reject any pending decodeAudioData resolvers
    for (auto& resolver : m_decodeAudioResolvers)
        resolver->reject(DOMException::create(InvalidStateError, "Audio context is going away"));
    m_decodeAudioResolvers.clear();
}

void AbstractAudioContext::rejectPendingResolvers()
{
    ASSERT(isMainThread());

    // Audio context is closing down so reject any resume promises that are still pending.

    for (auto& resolver : m_resumeResolvers) {
        resolver->reject(DOMException::create(InvalidStateError, "Audio context is going away"));
    }
    m_resumeResolvers.clear();
    m_isResolvingResumePromises = false;

    rejectPendingDecodeAudioDataResolvers();
}

const AtomicString& AbstractAudioContext::interfaceName() const
{
    return EventTargetNames::AudioContext;
}

ExecutionContext* AbstractAudioContext::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

void AbstractAudioContext::startRendering()
{
    // This is called for both online and offline contexts.
    ASSERT(isMainThread());
    ASSERT(m_destinationNode);

    recordUserGestureState();

    if (m_contextState == Suspended) {
        destination()->audioDestinationHandler().startRendering();
        setContextState(Running);
    }
}

DEFINE_TRACE(AbstractAudioContext)
{
    visitor->trace(m_destinationNode);
    visitor->trace(m_listener);
    visitor->trace(m_activeSourceNodes);
    visitor->trace(m_resumeResolvers);
    visitor->trace(m_decodeAudioResolvers);

    visitor->trace(m_periodicWaveSine);
    visitor->trace(m_periodicWaveSquare);
    visitor->trace(m_periodicWaveSawtooth);
    visitor->trace(m_periodicWaveTriangle);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

SecurityOrigin* AbstractAudioContext::getSecurityOrigin() const
{
    if (getExecutionContext())
        return getExecutionContext()->getSecurityOrigin();

    return nullptr;
}

} // namespace blink
