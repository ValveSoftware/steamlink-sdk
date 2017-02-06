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

#ifndef AbstractAudioContext_h
#define AbstractAudioContext_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMTypedArray.h"
#include "core/events/EventListener.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/webaudio/AsyncAudioDecoder.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "modules/webaudio/DeferredTaskHandler.h"
#include "modules/webaudio/IIRFilterNode.h"
#include "platform/audio/AudioBus.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/build_config.h"

namespace blink {

class AnalyserNode;
class AudioBuffer;
class AudioBufferCallback;
class AudioBufferSourceNode;
class AudioListener;
class AudioSummingJunction;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConvolverNode;
class DelayNode;
class Dictionary;
class Document;
class DynamicsCompressorNode;
class ExceptionState;
class GainNode;
class HTMLMediaElement;
class IIRFilterNode;
class MediaElementAudioSourceNode;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class OscillatorNode;
class PannerNode;
class PeriodicWave;
class PeriodicWaveConstraints;
class ScriptProcessorNode;
class ScriptPromiseResolver;
class ScriptState;
class SecurityOrigin;
class StereoPannerNode;
class WaveShaperNode;

// AbstractAudioContext is the cornerstone of the web audio API and all AudioNodes are created from it.
// For thread safety between the audio thread and the main thread, it has a rendering graph locking mechanism.

class MODULES_EXPORT AbstractAudioContext : public EventTargetWithInlineData, public ActiveScriptWrappable, public ActiveDOMObject {
    USING_GARBAGE_COLLECTED_MIXIN(AbstractAudioContext);
    DEFINE_WRAPPERTYPEINFO();
public:
    // The state of an audio context.  On creation, the state is Suspended. The state is Running if
    // audio is being processed (audio graph is being pulled for data). The state is Closed if the
    // audio context has been closed.  The valid transitions are from Suspended to either Running or
    // Closed; Running to Suspended or Closed. Once Closed, there are no valid transitions.
    enum AudioContextState {
        Suspended,
        Running,
        Closed
    };

    // Create an AudioContext for rendering to the audio hardware.
    static AbstractAudioContext* create(Document&, ExceptionState&);

    ~AbstractAudioContext() override;

    DECLARE_VIRTUAL_TRACE();

    // Is the destination node initialized and ready to handle audio?
    bool isDestinationInitialized() const
    {
        AudioDestinationNode* dest = destination();
        return dest ? dest->audioDestinationHandler().isInitialized() : false;
    }

    // Document notification
    void stop() final;
    bool hasPendingActivity() const final;

    // Cannnot be called from the audio thread.
    AudioDestinationNode* destination() const;

    size_t currentSampleFrame() const
    {
        // TODO: What is the correct value for the current frame if the destination node has gone
        // away?  0 is a valid frame.
        return m_destinationNode ? m_destinationNode->audioDestinationHandler().currentSampleFrame() : 0;
    }

    double currentTime() const
    {
        // TODO: What is the correct value for the current time if the destination node has gone
        // away? 0 is a valid time.
        return m_destinationNode ? m_destinationNode->audioDestinationHandler().currentTime() : 0;
    }

    float sampleRate() const { return m_destinationNode ? m_destinationNode->handler().sampleRate() : 0; }

    String state() const;
    AudioContextState contextState() const { return m_contextState; }
    void throwExceptionForClosedState(ExceptionState&);

    AudioBuffer* createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState&);

    // Asynchronous audio file data decoding.
    ScriptPromise decodeAudioData(ScriptState*, DOMArrayBuffer* audioData, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback, ExceptionState&);

    // Handles the promise and callbacks when |decodeAudioData| is finished decoding.
    void handleDecodeAudioData(AudioBuffer*, ScriptPromiseResolver*, AudioBufferCallback* successCallback, AudioBufferCallback* errorCallback);

    AudioListener* listener() { return m_listener; }

    virtual bool hasRealtimeConstraint() = 0;

    // The AudioNode create methods are called on the main thread (from JavaScript).
    AudioBufferSourceNode* createBufferSource(ExceptionState&);
    MediaElementAudioSourceNode* createMediaElementSource(HTMLMediaElement*, ExceptionState&);
    MediaStreamAudioSourceNode* createMediaStreamSource(MediaStream*, ExceptionState&);
    MediaStreamAudioDestinationNode* createMediaStreamDestination(ExceptionState&);
    GainNode* createGain(ExceptionState&);
    BiquadFilterNode* createBiquadFilter(ExceptionState&);
    WaveShaperNode* createWaveShaper(ExceptionState&);
    DelayNode* createDelay(ExceptionState&);
    DelayNode* createDelay(double maxDelayTime, ExceptionState&);
    PannerNode* createPanner(ExceptionState&);
    ConvolverNode* createConvolver(ExceptionState&);
    DynamicsCompressorNode* createDynamicsCompressor(ExceptionState&);
    AnalyserNode* createAnalyser(ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels, ExceptionState&);
    StereoPannerNode* createStereoPanner(ExceptionState&);
    ChannelSplitterNode* createChannelSplitter(ExceptionState&);
    ChannelSplitterNode* createChannelSplitter(size_t numberOfOutputs, ExceptionState&);
    ChannelMergerNode* createChannelMerger(ExceptionState&);
    ChannelMergerNode* createChannelMerger(size_t numberOfInputs, ExceptionState&);
    OscillatorNode* createOscillator(ExceptionState&);
    PeriodicWave* createPeriodicWave(DOMFloat32Array* real, DOMFloat32Array* imag, ExceptionState&);
    PeriodicWave* createPeriodicWave(DOMFloat32Array* real, DOMFloat32Array* imag, const PeriodicWaveConstraints&, ExceptionState&);

    // Close
    virtual ScriptPromise closeContext(ScriptState*) = 0;

    // Suspend
    virtual ScriptPromise suspendContext(ScriptState*) = 0;

    // Resume
    virtual ScriptPromise resumeContext(ScriptState*) = 0;

    // IIRFilter
    IIRFilterNode* createIIRFilter(Vector<double> feedforwardCoef, Vector<double> feedbackCoef,
        ExceptionState&);

    // When a source node has started processing and needs to be protected,
    // this method tells the context to protect the node.
    //
    // The context itself keeps a reference to all source nodes.  The source
    // nodes, then reference all nodes they're connected to.  In turn, these
    // nodes reference all nodes they're connected to.  All nodes are ultimately
    // connected to the AudioDestinationNode.  When the context release a source
    // node, it will be deactivated from the rendering graph along with all
    // other nodes it is uniquely connected to.
    void notifySourceNodeStartedProcessing(AudioNode*);
    // When a source node has no more processing to do (has finished playing),
    // this method tells the context to release the corresponding node.
    void notifySourceNodeFinishedProcessing(AudioHandler*);

    // Called at the start of each render quantum.
    void handlePreRenderTasks();

    // Called at the end of each render quantum.
    void handlePostRenderTasks();

    // Called periodically at the end of each render quantum to release finished
    // source nodes.
    void releaseFinishedSourceNodes();

    // Keeps track of the number of connections made.
    void incrementConnectionCount()
    {
        ASSERT(isMainThread());
        m_connectionCount++;
    }

    unsigned connectionCount() const { return m_connectionCount; }

    DeferredTaskHandler& deferredTaskHandler() const { return *m_deferredTaskHandler; }
    //
    // Thread Safety and Graph Locking:
    //
    // The following functions call corresponding functions of
    // DeferredTaskHandler.
    bool isAudioThread() const { return deferredTaskHandler().isAudioThread(); }
    void lock() { deferredTaskHandler().lock(); }
    bool tryLock() { return deferredTaskHandler().tryLock(); }
    void unlock() { deferredTaskHandler().unlock(); }
#if ENABLE(ASSERT)
    // Returns true if this thread owns the context's lock.
    bool isGraphOwner() { return deferredTaskHandler().isGraphOwner(); }
#endif
    using AutoLocker = DeferredTaskHandler::AutoLocker;

    // Returns the maximum numuber of channels we can support.
    static unsigned maxNumberOfChannels() { return MaxNumberOfChannels;}

    // EventTarget
    const AtomicString& interfaceName() const final;
    ExecutionContext* getExecutionContext() const final;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    void startRendering();
    void notifyStateChange();

    // A context is considered closed if:
    //  - closeContext() has been called.
    //  - it has been stopped by its execution context.
    virtual bool isContextClosed() const { return m_isCleared; }

    // Get the security origin for this audio context.
    SecurityOrigin* getSecurityOrigin() const;

    // Get the PeriodicWave for the specified oscillator type.  The table is initialized internally
    // if necessary.
    PeriodicWave* periodicWave(int type);

    // Check whether the AudioContext requires a user gesture and whether the
    // current stack is processing user gesture and record these information in
    // a histogram.
    void recordUserGestureState();

protected:
    explicit AbstractAudioContext(Document*);
    AbstractAudioContext(Document*, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate);

    void initialize();
    void uninitialize();

    void setContextState(AudioContextState);

    virtual void didClose() {}

    // Tries to handle AudioBufferSourceNodes that were started but became disconnected or was never
    // connected. Because these never get pulled anymore, they will stay around forever. So if we
    // can, try to stop them so they can be collected.
    void handleStoppableSourceNodes();

    Member<AudioDestinationNode> m_destinationNode;

    // FIXME(dominicc): Move m_resumeResolvers to AudioContext, because only
    // it creates these Promises.
    // Vector of promises created by resume(). It takes time to handle them, so we collect all of
    // the promises here until they can be resolved or rejected.
    HeapVector<Member<ScriptPromiseResolver>> m_resumeResolvers;

    void setClosedContextSampleRate(float newSampleRate) { m_closedContextSampleRate = newSampleRate; }
    float closedContextSampleRate() const { return m_closedContextSampleRate; }

    void rejectPendingDecodeAudioDataResolvers();

private:
    bool m_isCleared;
    void clear();

    // When the context goes away, there might still be some sources which
    // haven't finished playing.  Make sure to release them here.
    void releaseActiveSourceNodes();

    void removeFinishedSourceNodes();

    // Listener for the PannerNodes
    Member<AudioListener> m_listener;

    // Only accessed in the audio thread.
    // These raw pointers are safe because AudioSourceNodes in
    // m_activeSourceNodes own them.
    Vector<AudioHandler*> m_finishedSourceHandlers;

    // List of source nodes. This is either accessed when the graph lock is
    // held, or on the main thread when the audio thread has finished.
    // Oilpan: This Vector holds connection references. We must call
    // AudioHandler::makeConnection when we add an AudioNode to this, and must
    // call AudioHandler::breakConnection() when we remove an AudioNode from
    // this.
    HeapVector<Member<AudioNode>> m_activeSourceNodes;

    // The main thread controls m_activeSourceNodes, all updates and additions
    // are performed by it. When the audio thread marks a source node as finished,
    // the nodes are added to |m_finishedSourceNodes| and scheduled for removal
    // from |m_activeSourceNodes| by the main thread.
    HashSet<UntracedMember<AudioNode>> m_finishedSourceNodes;

    // FIXME(dominicc): Move these to AudioContext because only
    // it creates these Promises.
    // Handle Promises for resume() and suspend()
    void resolvePromisesForResume();
    void resolvePromisesForResumeOnMainThread();

    // When the context is going away, reject any pending script promise resolvers.
    virtual void rejectPendingResolvers();

    // True if we're in the process of resolving promises for resume().  Resolving can take some
    // time and the audio context process loop is very fast, so we don't want to call resolve an
    // excessive number of times.
    bool m_isResolvingResumePromises;

    // Whether a user gesture is required to start this AudioContext.
    bool m_userGestureRequired;

    unsigned m_connectionCount;

    // Graph locking.
    RefPtr<DeferredTaskHandler> m_deferredTaskHandler;

    // The state of the AbstractAudioContext.
    AudioContextState m_contextState;

    AsyncAudioDecoder m_audioDecoder;

    // When a context is closed, the sample rate is cleared.  But decodeAudioData can be called
    // after the context has been closed and it needs the sample rate.  When the context is closed,
    // the sample rate is saved here.
    float m_closedContextSampleRate;

    // Vector of promises created by decodeAudioData.  This keeps the resolvers alive until
    // decodeAudioData finishes decoding and can tell the main thread to resolve them.
    HeapHashSet<Member<ScriptPromiseResolver>> m_decodeAudioResolvers;

    // PeriodicWave's for the builtin oscillator types.  These only depend on the sample rate. so
    // they can be shared with all OscillatorNodes in the context.  To conserve memory, these are
    // lazily initiialized on first use.
    Member<PeriodicWave> m_periodicWaveSine;
    Member<PeriodicWave> m_periodicWaveSquare;
    Member<PeriodicWave> m_periodicWaveSawtooth;
    Member<PeriodicWave> m_periodicWaveTriangle;

    // This is considering 32 is large enough for multiple channels audio.
    // It is somewhat arbitrary and could be increased if necessary.
    enum { MaxNumberOfChannels = 32 };
};

} // namespace blink

#endif // AbstractAudioContext_h
