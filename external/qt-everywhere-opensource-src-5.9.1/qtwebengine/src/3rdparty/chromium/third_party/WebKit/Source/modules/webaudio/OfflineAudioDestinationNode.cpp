/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "modules/webaudio/OfflineAudioDestinationNode.h"

#include "core/dom/ExecutionContextTask.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/audio/DenormalDisabler.h"
#include "platform/audio/HRTFDatabaseLoader.h"
#include "public/platform/Platform.h"
#include "wtf/PtrUtil.h"
#include <algorithm>

namespace blink {

OfflineAudioDestinationHandler::OfflineAudioDestinationHandler(
    AudioNode& node,
    AudioBuffer* renderTarget)
    : AudioDestinationHandler(node, renderTarget->sampleRate()),
      m_renderTarget(renderTarget),
      m_renderThread(wrapUnique(
          Platform::current()->createThread("offline audio renderer"))),
      m_framesProcessed(0),
      m_framesToProcess(0),
      m_isRenderingStarted(false),
      m_shouldSuspend(false) {
  m_renderBus = AudioBus::create(renderTarget->numberOfChannels(),
                                 AudioUtilities::kRenderQuantumFrames);
  m_framesToProcess = m_renderTarget->length();

  // Node-specific defaults.
  m_channelCount = m_renderTarget->numberOfChannels();
  setInternalChannelCountMode(Explicit);
  setInternalChannelInterpretation(AudioBus::Speakers);
}

PassRefPtr<OfflineAudioDestinationHandler>
OfflineAudioDestinationHandler::create(AudioNode& node,
                                       AudioBuffer* renderTarget) {
  return adoptRef(new OfflineAudioDestinationHandler(node, renderTarget));
}

OfflineAudioDestinationHandler::~OfflineAudioDestinationHandler() {
  DCHECK(!isInitialized());
}

void OfflineAudioDestinationHandler::dispose() {
  uninitialize();
  AudioDestinationHandler::dispose();
}

void OfflineAudioDestinationHandler::initialize() {
  if (isInitialized())
    return;

  AudioHandler::initialize();
}

void OfflineAudioDestinationHandler::uninitialize() {
  if (!isInitialized())
    return;

  if (m_renderThread)
    m_renderThread.reset();

  AudioHandler::uninitialize();
}

OfflineAudioContext* OfflineAudioDestinationHandler::context() const {
  return static_cast<OfflineAudioContext*>(AudioDestinationHandler::context());
}

unsigned long OfflineAudioDestinationHandler::maxChannelCount() const {
  return m_channelCount;
}

void OfflineAudioDestinationHandler::startRendering() {
  DCHECK(isMainThread());
  DCHECK(m_renderThread);
  DCHECK(m_renderTarget);

  if (!m_renderTarget)
    return;

  // Rendering was not started. Starting now.
  if (!m_isRenderingStarted) {
    m_isRenderingStarted = true;
    m_renderThread->getWebTaskRunner()->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&OfflineAudioDestinationHandler::startOfflineRendering,
                        wrapPassRefPtr(this)));
    return;
  }

  // Rendering is already started, which implicitly means we resume the
  // rendering by calling |doOfflineRendering| on the render thread.
  m_renderThread->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&OfflineAudioDestinationHandler::doOfflineRendering,
                      wrapPassRefPtr(this)));
}

void OfflineAudioDestinationHandler::stopRendering() {
  // offline audio rendering CANNOT BE stopped by JavaScript.
  ASSERT_NOT_REACHED();
}

WebThread* OfflineAudioDestinationHandler::offlineRenderThread() {
  DCHECK(m_renderThread);

  return m_renderThread.get();
}

void OfflineAudioDestinationHandler::startOfflineRendering() {
  DCHECK(!isMainThread());

  DCHECK(m_renderBus);
  if (!m_renderBus)
    return;

  bool isAudioContextInitialized = context()->isDestinationInitialized();
  DCHECK(isAudioContextInitialized);
  if (!isAudioContextInitialized)
    return;

  bool channelsMatch =
      m_renderBus->numberOfChannels() == m_renderTarget->numberOfChannels();
  DCHECK(channelsMatch);
  if (!channelsMatch)
    return;

  bool isRenderBusAllocated =
      m_renderBus->length() >= AudioUtilities::kRenderQuantumFrames;
  DCHECK(isRenderBusAllocated);
  if (!isRenderBusAllocated)
    return;

  // Start rendering.
  doOfflineRendering();
}

void OfflineAudioDestinationHandler::doOfflineRendering() {
  DCHECK(!isMainThread());

  unsigned numberOfChannels = m_renderTarget->numberOfChannels();

  // Reset the suspend flag.
  m_shouldSuspend = false;

  // If there is more to process and there is no suspension at the moment,
  // do continue to render quanta. Then calling OfflineAudioContext.resume()
  // will pick up the render loop again from where it was suspended.
  while (m_framesToProcess > 0 && !m_shouldSuspend) {
    // Suspend the rendering and update m_shouldSuspend if a scheduled
    // suspend found at the current sample frame. Otherwise render one
    // quantum and return false.
    m_shouldSuspend = renderIfNotSuspended(
        0, m_renderBus.get(), AudioUtilities::kRenderQuantumFrames);

    if (m_shouldSuspend)
      return;

    size_t framesAvailableToCopy =
        std::min(m_framesToProcess,
                 static_cast<size_t>(AudioUtilities::kRenderQuantumFrames));

    for (unsigned channelIndex = 0; channelIndex < numberOfChannels;
         ++channelIndex) {
      const float* source = m_renderBus->channel(channelIndex)->data();
      float* destination = m_renderTarget->getChannelData(channelIndex)->data();
      memcpy(destination + m_framesProcessed, source,
             sizeof(float) * framesAvailableToCopy);
    }

    m_framesProcessed += framesAvailableToCopy;

    DCHECK_GE(m_framesToProcess, framesAvailableToCopy);
    m_framesToProcess -= framesAvailableToCopy;
  }

  // Finish up the rendering loop if there is no more to process.
  if (!m_framesToProcess)
    finishOfflineRendering();
}

void OfflineAudioDestinationHandler::suspendOfflineRendering() {
  DCHECK(!isMainThread());

  // The actual rendering has been suspended. Notify the context.
  if (context()->getExecutionContext()) {
    context()->getExecutionContext()->postTask(
        BLINK_FROM_HERE,
        createCrossThreadTask(&OfflineAudioDestinationHandler::notifySuspend,
                              PassRefPtr<OfflineAudioDestinationHandler>(this),
                              context()->currentSampleFrame()));
  }
}

void OfflineAudioDestinationHandler::finishOfflineRendering() {
  DCHECK(!isMainThread());

  // The actual rendering has been completed. Notify the context.
  if (context()->getExecutionContext()) {
    context()->getExecutionContext()->postTask(
        BLINK_FROM_HERE, createCrossThreadTask(
                             &OfflineAudioDestinationHandler::notifyComplete,
                             PassRefPtr<OfflineAudioDestinationHandler>(this)));
  }
}

void OfflineAudioDestinationHandler::notifySuspend(size_t frame) {
  DCHECK(isMainThread());

  if (context())
    context()->resolveSuspendOnMainThread(frame);
}

void OfflineAudioDestinationHandler::notifyComplete() {
  // The OfflineAudioContext might be gone.
  if (context())
    context()->fireCompletionEvent();
}

bool OfflineAudioDestinationHandler::renderIfNotSuspended(
    AudioBus* sourceBus,
    AudioBus* destinationBus,
    size_t numberOfFrames) {
  // We don't want denormals slowing down any of the audio processing
  // since they can very seriously hurt performance.
  // This will take care of all AudioNodes because they all process within this
  // scope.
  DenormalDisabler denormalDisabler;

  // Need to check if the context actually alive. Otherwise the subsequent
  // steps will fail. If the context is not alive somehow, return immediately
  // and do nothing.
  //
  // TODO(hongchan): because the context can go away while rendering, so this
  // check cannot guarantee the safe execution of the following steps.
  DCHECK(context());
  if (!context())
    return false;

  context()->deferredTaskHandler().setAudioThreadToCurrentThread();

  // If the destination node is not initialized, pass the silence to the final
  // audio destination (one step before the FIFO). This check is for the case
  // where the destination is in the middle of tearing down process.
  if (!isInitialized()) {
    destinationBus->zero();
    return false;
  }

  // Take care pre-render tasks at the beginning of each render quantum. Then
  // it will stop the rendering loop if the context needs to be suspended
  // at the beginning of the next render quantum.
  if (context()->handlePreOfflineRenderTasks()) {
    suspendOfflineRendering();
    return true;
  }

  // Prepare the local audio input provider for this render quantum.
  if (sourceBus)
    m_localAudioInputProvider.set(sourceBus);

  DCHECK_GE(numberOfInputs(), 1u);
  if (numberOfInputs() < 1) {
    destinationBus->zero();
    return false;
  }
  // This will cause the node(s) connected to us to process, which in turn will
  // pull on their input(s), all the way backwards through the rendering graph.
  AudioBus* renderedBus = input(0).pull(destinationBus, numberOfFrames);

  if (!renderedBus) {
    destinationBus->zero();
  } else if (renderedBus != destinationBus) {
    // in-place processing was not possible - so copy
    destinationBus->copyFrom(*renderedBus);
  }

  // Process nodes which need a little extra help because they are not connected
  // to anything, but still need to process.
  context()->deferredTaskHandler().processAutomaticPullNodes(numberOfFrames);

  // Let the context take care of any business at the end of each render
  // quantum.
  context()->handlePostOfflineRenderTasks();

  // Advance current sample-frame.
  size_t newSampleFrame = m_currentSampleFrame + numberOfFrames;
  releaseStore(&m_currentSampleFrame, newSampleFrame);

  return false;
}

// ----------------------------------------------------------------

OfflineAudioDestinationNode::OfflineAudioDestinationNode(
    BaseAudioContext& context,
    AudioBuffer* renderTarget)
    : AudioDestinationNode(context) {
  setHandler(OfflineAudioDestinationHandler::create(*this, renderTarget));
}

OfflineAudioDestinationNode* OfflineAudioDestinationNode::create(
    BaseAudioContext* context,
    AudioBuffer* renderTarget) {
  return new OfflineAudioDestinationNode(*context, renderTarget);
}

}  // namespace blink
