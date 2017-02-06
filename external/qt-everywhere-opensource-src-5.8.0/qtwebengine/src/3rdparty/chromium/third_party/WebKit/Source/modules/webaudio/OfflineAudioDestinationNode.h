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

#ifndef OfflineAudioDestinationNode_h
#define OfflineAudioDestinationNode_h

#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "public/platform/WebThread.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class AbstractAudioContext;
class AudioBus;
class OfflineAudioContext;

class OfflineAudioDestinationHandler final : public AudioDestinationHandler {
public:
    static PassRefPtr<OfflineAudioDestinationHandler> create(AudioNode&, AudioBuffer* renderTarget);
    ~OfflineAudioDestinationHandler() override;

    // AudioHandler
    void dispose() override;
    void initialize() override;
    void uninitialize() override;

    OfflineAudioContext* context() const final;

    // AudioDestinationHandler
    void startRendering() override;
    void stopRendering() override;
    unsigned long maxChannelCount() const override;

    float sampleRate()  const override { return m_renderTarget->sampleRate(); }

    size_t renderQuantumFrames() const { return renderQuantumSize; }

    WebThread* offlineRenderThread();

private:
    OfflineAudioDestinationHandler(AudioNode&, AudioBuffer* renderTarget);

    static const size_t renderQuantumSize;

    // Set up the rendering and start. After setting the context up, it will
    // eventually call |doOfflineRendering|.
    void startOfflineRendering();

    // Suspend the rendering loop and notify the main thread to resolve the
    // associated promise.
    void suspendOfflineRendering();

    // Start the rendering loop.
    void doOfflineRendering();

    // Finish the rendering loop and notify the main thread to resolve the
    // promise with the rendered buffer.
    void finishOfflineRendering();

    // Suspend/completion callbacks for the main thread.
    void notifySuspend(size_t);
    void notifyComplete();

    // The offline version of render() method. If the rendering needs to be
    // suspended after checking, this stops the rendering and returns true.
    // Otherwise, it returns false after rendering one quantum.
    bool renderIfNotSuspended(AudioBus* sourceBus, AudioBus* destinationBus, size_t numberOfFrames);

    // This AudioHandler renders into this AudioBuffer.
    // This Persistent doesn't make a reference cycle including the owner
    // OfflineAudioDestinationNode.
    Persistent<AudioBuffer> m_renderTarget;
    // Temporary AudioBus for each render quantum.
    RefPtr<AudioBus> m_renderBus;

    // Rendering thread.
    std::unique_ptr<WebThread> m_renderThread;

    // These variables are for counting the number of frames for the current
    // progress and the remaining frames to be processed.
    size_t m_framesProcessed;
    size_t m_framesToProcess;

    // This flag is necessary to distinguish the state of the context between
    // 'created' and 'suspended'. If this flag is false and the current state
    // is 'suspended', it means the context is created and have not started yet.
    bool m_isRenderingStarted;

    // This flag indicates whether the rendering should be suspended or not.
    bool m_shouldSuspend;
};

class OfflineAudioDestinationNode final : public AudioDestinationNode {
public:
    static OfflineAudioDestinationNode* create(AbstractAudioContext*, AudioBuffer* renderTarget);

private:
    OfflineAudioDestinationNode(AbstractAudioContext&, AudioBuffer* renderTarget);
};

} // namespace blink

#endif // OfflineAudioDestinationNode_h
