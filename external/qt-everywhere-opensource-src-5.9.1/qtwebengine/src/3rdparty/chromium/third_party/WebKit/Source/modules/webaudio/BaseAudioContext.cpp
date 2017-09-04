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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/webaudio/BaseAudioContext.h"

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
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleTypes.h"
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
#include "modules/webaudio/ConstantSourceNode.h"
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

BaseAudioContext* BaseAudioContext::create(Document& document,
                                           ExceptionState& exceptionState) {
  return AudioContext::create(document, exceptionState);
}

// FIXME(dominicc): Devolve these constructors to AudioContext
// and OfflineAudioContext respectively.

// Constructor for rendering to the audio hardware.
BaseAudioContext::BaseAudioContext(Document* document)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(document),
      m_destinationNode(nullptr),
      m_isCleared(false),
      m_isResolvingResumePromises(false),
      m_userGestureRequired(false),
      m_connectionCount(0),
      m_deferredTaskHandler(DeferredTaskHandler::create()),
      m_contextState(Suspended),
      m_closedContextSampleRate(-1),
      m_periodicWaveSine(nullptr),
      m_periodicWaveSquare(nullptr),
      m_periodicWaveSawtooth(nullptr),
      m_periodicWaveTriangle(nullptr) {
  // If mediaPlaybackRequiresUserGesture is enabled, cross origin iframes will
  // require user gesture for the AudioContext to produce sound.
  if (document->settings() &&
      document->settings()->mediaPlaybackRequiresUserGesture() &&
      document->frame() && document->frame()->isCrossOriginSubframe()) {
    m_autoplayStatus = AutoplayStatus::AutoplayStatusFailed;
    m_userGestureRequired = true;
  }

  m_destinationNode = DefaultAudioDestinationNode::create(this);

  initialize();
}

// Constructor for offline (non-realtime) rendering.
BaseAudioContext::BaseAudioContext(Document* document,
                                   unsigned numberOfChannels,
                                   size_t numberOfFrames,
                                   float sampleRate)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(document),
      m_destinationNode(nullptr),
      m_isCleared(false),
      m_isResolvingResumePromises(false),
      m_userGestureRequired(false),
      m_connectionCount(0),
      m_deferredTaskHandler(DeferredTaskHandler::create()),
      m_contextState(Suspended),
      m_closedContextSampleRate(-1),
      m_periodicWaveSine(nullptr),
      m_periodicWaveSquare(nullptr),
      m_periodicWaveSawtooth(nullptr),
      m_periodicWaveTriangle(nullptr) {}

BaseAudioContext::~BaseAudioContext() {
  deferredTaskHandler().contextWillBeDestroyed();
  // AudioNodes keep a reference to their context, so there should be no way to
  // be in the destructor if there are still AudioNodes around.
  DCHECK(!isDestinationInitialized());
  DCHECK(!m_activeSourceNodes.size());
  DCHECK(!m_finishedSourceHandlers.size());
  DCHECK(!m_isResolvingResumePromises);
  DCHECK(!m_resumeResolvers.size());
  DCHECK(!m_autoplayStatus.has_value());
}

void BaseAudioContext::initialize() {
  if (isDestinationInitialized())
    return;

  FFTFrame::initialize();

  if (m_destinationNode) {
    m_destinationNode->handler().initialize();
    // The AudioParams in the listener need access to the destination node, so
    // only create the listener if the destination node exists.
    m_listener = AudioListener::create(*this);
  }
}

void BaseAudioContext::clear() {
  m_destinationNode.clear();
  // The audio rendering thread is dead.  Nobody will schedule AudioHandler
  // deletion.  Let's do it ourselves.
  deferredTaskHandler().clearHandlersToBeDeleted();
  m_isCleared = true;
}

void BaseAudioContext::uninitialize() {
  DCHECK(isMainThread());

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

  DCHECK(m_listener);
  m_listener->waitForHRTFDatabaseLoaderThreadCompletion();

  recordAutoplayStatus();

  clear();
}

void BaseAudioContext::contextDestroyed() {
  uninitialize();
}

bool BaseAudioContext::hasPendingActivity() const {
  // There's no pending activity if the audio context has been cleared.
  return !m_isCleared;
}

AudioDestinationNode* BaseAudioContext::destination() const {
  // Cannot be called from the audio thread because this method touches objects
  // managed by Oilpan, and the audio thread is not managed by Oilpan.
  DCHECK(!isAudioThread());
  return m_destinationNode;
}

void BaseAudioContext::throwExceptionForClosedState(
    ExceptionState& exceptionState) {
  exceptionState.throwDOMException(InvalidStateError,
                                   "AudioContext has been closed.");
}

AudioBuffer* BaseAudioContext::createBuffer(unsigned numberOfChannels,
                                            size_t numberOfFrames,
                                            float sampleRate,
                                            ExceptionState& exceptionState) {
  // It's ok to call createBuffer, even if the context is closed because the
  // AudioBuffer doesn't really "belong" to any particular context.

  AudioBuffer* buffer = AudioBuffer::create(numberOfChannels, numberOfFrames,
                                            sampleRate, exceptionState);

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
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, audioBufferSampleRateHistogram,
        ("WebAudio.AudioBuffer.SampleRate384kHz", 3000, 384000, 60));

    audioBufferChannelsHistogram.sample(numberOfChannels);
    audioBufferLengthHistogram.count(numberOfFrames);
    audioBufferSampleRateHistogram.count(sampleRate);

    // Compute the ratio of the buffer rate and the context rate so we know
    // how often the buffer needs to be resampled to match the context.  For
    // the histogram, we multiply the ratio by 100 and round to the nearest
    // integer.  If the context is closed, don't record this because we
    // don't have a sample rate for closed context.
    if (!isContextClosed()) {
      // The limits are choosen from 100*(3000/384000) = 0.78125 and
      // 100*(384000/3000) = 12800, where 3000 and 384000 are the current
      // min and max sample rates possible for an AudioBuffer.  The number
      // of buckets is fairly arbitrary.
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, audioBufferSampleRateRatioHistogram,
          ("WebAudio.AudioBuffer.SampleRateRatio384kHz", 1, 12800, 50));
      float ratio = 100 * sampleRate / this->sampleRate();
      audioBufferSampleRateRatioHistogram.count(static_cast<int>(0.5 + ratio));
    }
  }

  return buffer;
}

ScriptPromise BaseAudioContext::decodeAudioData(
    ScriptState* scriptState,
    DOMArrayBuffer* audioData,
    AudioBufferCallback* successCallback,
    AudioBufferCallback* errorCallback,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());
  DCHECK(audioData);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  float rate = isContextClosed() ? closedContextSampleRate() : sampleRate();

  DCHECK_GT(rate, 0);

  m_decodeAudioResolvers.add(resolver);
  m_audioDecoder.decodeAsync(audioData, rate, successCallback, errorCallback,
                             resolver, this);

  return promise;
}

void BaseAudioContext::handleDecodeAudioData(
    AudioBuffer* audioBuffer,
    ScriptPromiseResolver* resolver,
    AudioBufferCallback* successCallback,
    AudioBufferCallback* errorCallback) {
  DCHECK(isMainThread());

  if (audioBuffer) {
    // Resolve promise successfully and run the success callback
    resolver->resolve(audioBuffer);
    if (successCallback)
      successCallback->handleEvent(audioBuffer);
  } else {
    // Reject the promise and run the error callback
    DOMException* error =
        DOMException::create(EncodingError, "Unable to decode audio data");
    resolver->reject(error);
    if (errorCallback)
      errorCallback->handleEvent(error);
  }

  // We've resolved the promise.  Remove it now.
  DCHECK(m_decodeAudioResolvers.contains(resolver));
  m_decodeAudioResolvers.remove(resolver);
}

AudioBufferSourceNode* BaseAudioContext::createBufferSource(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  AudioBufferSourceNode* node =
      AudioBufferSourceNode::create(*this, exceptionState);

  // Do not add a reference to this source node now. The reference will be added
  // when start() is called.

  return node;
}

ConstantSourceNode* BaseAudioContext::createConstantSource(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ConstantSourceNode::create(*this, exceptionState);
}

MediaElementAudioSourceNode* BaseAudioContext::createMediaElementSource(
    HTMLMediaElement* mediaElement,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return MediaElementAudioSourceNode::create(*this, *mediaElement,
                                             exceptionState);
}

MediaStreamAudioSourceNode* BaseAudioContext::createMediaStreamSource(
    MediaStream* mediaStream,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return MediaStreamAudioSourceNode::create(*this, *mediaStream,
                                            exceptionState);
}

MediaStreamAudioDestinationNode* BaseAudioContext::createMediaStreamDestination(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  // Set number of output channels to stereo by default.
  return MediaStreamAudioDestinationNode::create(*this, 2, exceptionState);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ScriptProcessorNode::create(*this, exceptionState);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t bufferSize,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ScriptProcessorNode::create(*this, bufferSize, exceptionState);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t bufferSize,
    size_t numberOfInputChannels,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ScriptProcessorNode::create(*this, bufferSize, numberOfInputChannels,
                                     exceptionState);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t bufferSize,
    size_t numberOfInputChannels,
    size_t numberOfOutputChannels,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ScriptProcessorNode::create(*this, bufferSize, numberOfInputChannels,
                                     numberOfOutputChannels, exceptionState);
}

StereoPannerNode* BaseAudioContext::createStereoPanner(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return StereoPannerNode::create(*this, exceptionState);
}

BiquadFilterNode* BaseAudioContext::createBiquadFilter(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return BiquadFilterNode::create(*this, exceptionState);
}

WaveShaperNode* BaseAudioContext::createWaveShaper(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return WaveShaperNode::create(*this, exceptionState);
}

PannerNode* BaseAudioContext::createPanner(ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return PannerNode::create(*this, exceptionState);
}

ConvolverNode* BaseAudioContext::createConvolver(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ConvolverNode::create(*this, exceptionState);
}

DynamicsCompressorNode* BaseAudioContext::createDynamicsCompressor(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return DynamicsCompressorNode::create(*this, exceptionState);
}

AnalyserNode* BaseAudioContext::createAnalyser(ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return AnalyserNode::create(*this, exceptionState);
}

GainNode* BaseAudioContext::createGain(ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return GainNode::create(*this, exceptionState);
}

DelayNode* BaseAudioContext::createDelay(ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return DelayNode::create(*this, exceptionState);
}

DelayNode* BaseAudioContext::createDelay(double maxDelayTime,
                                         ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return DelayNode::create(*this, maxDelayTime, exceptionState);
}

ChannelSplitterNode* BaseAudioContext::createChannelSplitter(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ChannelSplitterNode::create(*this, exceptionState);
}

ChannelSplitterNode* BaseAudioContext::createChannelSplitter(
    size_t numberOfOutputs,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ChannelSplitterNode::create(*this, numberOfOutputs, exceptionState);
}

ChannelMergerNode* BaseAudioContext::createChannelMerger(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ChannelMergerNode::create(*this, exceptionState);
}

ChannelMergerNode* BaseAudioContext::createChannelMerger(
    size_t numberOfInputs,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return ChannelMergerNode::create(*this, numberOfInputs, exceptionState);
}

OscillatorNode* BaseAudioContext::createOscillator(
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return OscillatorNode::create(*this, exceptionState);
}

PeriodicWave* BaseAudioContext::createPeriodicWave(
    DOMFloat32Array* real,
    DOMFloat32Array* imag,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return PeriodicWave::create(*this, real, imag, false, exceptionState);
}

PeriodicWave* BaseAudioContext::createPeriodicWave(
    DOMFloat32Array* real,
    DOMFloat32Array* imag,
    const PeriodicWaveConstraints& options,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  bool disable = options.hasDisableNormalization()
                     ? options.disableNormalization()
                     : false;

  return PeriodicWave::create(*this, real, imag, disable, exceptionState);
}

IIRFilterNode* BaseAudioContext::createIIRFilter(
    Vector<double> feedforwardCoef,
    Vector<double> feedbackCoef,
    ExceptionState& exceptionState) {
  DCHECK(isMainThread());

  return IIRFilterNode::create(*this, feedforwardCoef, feedbackCoef,
                               exceptionState);
}

PeriodicWave* BaseAudioContext::periodicWave(int type) {
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
      NOTREACHED();
      return nullptr;
  }
}

void BaseAudioContext::maybeRecordStartAttempt() {
  if (!m_userGestureRequired || !UserGestureIndicator::processingUserGesture())
    return;

  DCHECK(!m_autoplayStatus.has_value() ||
         m_autoplayStatus != AutoplayStatus::AutoplayStatusSucceeded);
  m_autoplayStatus = AutoplayStatus::AutoplayStatusFailedWithStart;
}

String BaseAudioContext::state() const {
  // These strings had better match the strings for AudioContextState in
  // AudioContext.idl.
  switch (m_contextState) {
    case Suspended:
      return "suspended";
    case Running:
      return "running";
    case Closed:
      return "closed";
  }
  NOTREACHED();
  return "";
}

void BaseAudioContext::setContextState(AudioContextState newState) {
  DCHECK(isMainThread());

  // Validate the transitions.  The valid transitions are Suspended->Running,
  // Running->Suspended, and anything->Closed.
  switch (newState) {
    case Suspended:
      DCHECK_EQ(m_contextState, Running);
      break;
    case Running:
      DCHECK_EQ(m_contextState, Suspended);
      break;
    case Closed:
      DCHECK_NE(m_contextState, Closed);
      break;
  }

  if (newState == m_contextState) {
    // DCHECKs above failed; just return.
    return;
  }

  m_contextState = newState;

  // Notify context that state changed
  if (getExecutionContext())
    getExecutionContext()->postTask(
        BLINK_FROM_HERE,
        createSameThreadTask(&BaseAudioContext::notifyStateChange,
                             wrapPersistent(this)));
}

void BaseAudioContext::notifyStateChange() {
  dispatchEvent(Event::create(EventTypeNames::statechange));
}

void BaseAudioContext::notifySourceNodeFinishedProcessing(
    AudioHandler* handler) {
  DCHECK(isAudioThread());
  m_finishedSourceHandlers.append(handler);
}

void BaseAudioContext::removeFinishedSourceNodes() {
  DCHECK(isMainThread());
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

void BaseAudioContext::releaseFinishedSourceNodes() {
  ASSERT(isGraphOwner());
  DCHECK(isAudioThread());
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
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&BaseAudioContext::removeFinishedSourceNodes,
                        wrapCrossThreadPersistent(this)));

  m_finishedSourceHandlers.clear();
}

void BaseAudioContext::notifySourceNodeStartedProcessing(AudioNode* node) {
  DCHECK(isMainThread());
  AutoLocker locker(this);

  m_activeSourceNodes.append(node);
  node->handler().makeConnection();
}

void BaseAudioContext::releaseActiveSourceNodes() {
  DCHECK(isMainThread());
  for (auto& sourceNode : m_activeSourceNodes)
    sourceNode->handler().breakConnection();

  m_activeSourceNodes.clear();
}

void BaseAudioContext::handleStoppableSourceNodes() {
  ASSERT(isGraphOwner());

  // Find AudioBufferSourceNodes to see if we can stop playing them.
  for (AudioNode* node : m_activeSourceNodes) {
    // If the AudioNode has been marked as finished and released by
    // the audio thread, but not yet removed by the main thread
    // (see releaseActiveSourceNodes() above), |node| must not be
    // touched as its handler may have been released already.
    if (m_finishedSourceNodes.contains(node))
      continue;
    if (node->handler().getNodeType() ==
        AudioHandler::NodeTypeAudioBufferSource) {
      AudioBufferSourceNode* sourceNode =
          static_cast<AudioBufferSourceNode*>(node);
      sourceNode->audioBufferSourceHandler().handleStoppableSourceNode();
    }
  }
}

void BaseAudioContext::handlePreRenderTasks() {
  DCHECK(isAudioThread());

  // At the beginning of every render quantum, try to update the internal
  // rendering graph state (from main thread changes).  It's OK if the tryLock()
  // fails, we'll just take slightly longer to pick up the changes.
  if (tryLock()) {
    deferredTaskHandler().handleDeferredTasks();

    resolvePromisesForResume();

    // Check to see if source nodes can be stopped because the end time has
    // passed.
    handleStoppableSourceNodes();

    // Update the dirty state of the listener.
    listener()->updateState();

    unlock();
  }
}

void BaseAudioContext::handlePostRenderTasks() {
  DCHECK(isAudioThread());

  // Must use a tryLock() here too.  Don't worry, the lock will very rarely be
  // contended and this method is called frequently.  The worst that can happen
  // is that there will be some nodes which will take slightly longer than usual
  // to be deleted or removed from the render graph (in which case they'll
  // render silence).
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

void BaseAudioContext::resolvePromisesForResumeOnMainThread() {
  DCHECK(isMainThread());
  AutoLocker locker(this);

  for (auto& resolver : m_resumeResolvers) {
    if (m_contextState == Closed) {
      resolver->reject(DOMException::create(
          InvalidStateError, "Cannot resume a context that has been closed"));
    } else {
      resolver->resolve();
    }
  }

  m_resumeResolvers.clear();
  m_isResolvingResumePromises = false;
}

void BaseAudioContext::resolvePromisesForResume() {
  // This runs inside the BaseAudioContext's lock when handling pre-render
  // tasks.
  DCHECK(isAudioThread());
  ASSERT(isGraphOwner());

  // Resolve any pending promises created by resume(). Only do this if we
  // haven't already started resolving these promises. This gets called very
  // often and it takes some time to resolve the promises in the main thread.
  if (!m_isResolvingResumePromises && m_resumeResolvers.size() > 0) {
    m_isResolvingResumePromises = true;
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&BaseAudioContext::resolvePromisesForResumeOnMainThread,
                        wrapCrossThreadPersistent(this)));
  }
}

void BaseAudioContext::rejectPendingDecodeAudioDataResolvers() {
  // Now reject any pending decodeAudioData resolvers
  for (auto& resolver : m_decodeAudioResolvers)
    resolver->reject(
        DOMException::create(InvalidStateError, "Audio context is going away"));
  m_decodeAudioResolvers.clear();
}

void BaseAudioContext::maybeUnlockUserGesture() {
  if (!m_userGestureRequired || !UserGestureIndicator::processingUserGesture())
    return;

  DCHECK(!m_autoplayStatus.has_value() ||
         m_autoplayStatus != AutoplayStatus::AutoplayStatusSucceeded);

  UserGestureIndicator::utilizeUserGesture();
  m_userGestureRequired = false;
  m_autoplayStatus = AutoplayStatus::AutoplayStatusSucceeded;
}

bool BaseAudioContext::isAllowedToStart() const {
  if (!m_userGestureRequired)
    return true;

  toDocument(getExecutionContext())
      ->addConsoleMessage(ConsoleMessage::create(
          JSMessageSource, WarningMessageLevel,
          "An AudioContext in a cross origin iframe must be created or resumed "
          "from a user gesture to enable audio output."));
  return false;
}

void BaseAudioContext::rejectPendingResolvers() {
  DCHECK(isMainThread());

  // Audio context is closing down so reject any resume promises that are still
  // pending.

  for (auto& resolver : m_resumeResolvers) {
    resolver->reject(
        DOMException::create(InvalidStateError, "Audio context is going away"));
  }
  m_resumeResolvers.clear();
  m_isResolvingResumePromises = false;

  rejectPendingDecodeAudioDataResolvers();
}

void BaseAudioContext::recordAutoplayStatus() {
  if (!m_autoplayStatus.has_value())
    return;

  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplayHistogram,
      ("WebAudio.Autoplay.CrossOrigin", AutoplayStatus::AutoplayStatusCount));
  autoplayHistogram.count(m_autoplayStatus.value());

  m_autoplayStatus.reset();
}

const AtomicString& BaseAudioContext::interfaceName() const {
  return EventTargetNames::AudioContext;
}

ExecutionContext* BaseAudioContext::getExecutionContext() const {
  return ActiveDOMObject::getExecutionContext();
}

void BaseAudioContext::startRendering() {
  // This is called for both online and offline contexts.
  DCHECK(isMainThread());
  DCHECK(m_destinationNode);
  DCHECK(isAllowedToStart());

  if (m_contextState == Suspended) {
    destination()->audioDestinationHandler().startRendering();
    setContextState(Running);
  }
}

DEFINE_TRACE(BaseAudioContext) {
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

SecurityOrigin* BaseAudioContext::getSecurityOrigin() const {
  if (getExecutionContext())
    return getExecutionContext()->getSecurityOrigin();

  return nullptr;
}

}  // namespace blink
