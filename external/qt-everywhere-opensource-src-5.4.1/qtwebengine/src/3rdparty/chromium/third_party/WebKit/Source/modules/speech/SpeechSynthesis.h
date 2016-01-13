/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpeechSynthesis_h
#define SpeechSynthesis_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "modules/EventTargetModules.h"
#include "modules/speech/SpeechSynthesisUtterance.h"
#include "modules/speech/SpeechSynthesisVoice.h"
#include "platform/heap/Handle.h"
#include "platform/speech/PlatformSpeechSynthesisUtterance.h"
#include "platform/speech/PlatformSpeechSynthesizer.h"

namespace WebCore {

class ExceptionState;
class PlatformSpeechSynthesizerClient;

class SpeechSynthesis FINAL : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<SpeechSynthesis>, public PlatformSpeechSynthesizerClient, public ScriptWrappable, public ContextLifecycleObserver, public EventTargetWithInlineData {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<SpeechSynthesis>);
    USING_GARBAGE_COLLECTED_MIXIN(SpeechSynthesis);
public:
    static SpeechSynthesis* create(ExecutionContext*);

    bool pending() const;
    bool speaking() const;
    bool paused() const;

    void speak(SpeechSynthesisUtterance*, ExceptionState&);
    void cancel();
    void pause();
    void resume();

    const HeapVector<Member<SpeechSynthesisVoice> >& getVoices();

    // Used in testing to use a mock platform synthesizer
    void setPlatformSynthesizer(PlatformSpeechSynthesizer*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(voiceschanged);

    virtual ExecutionContext* executionContext() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit SpeechSynthesis(ExecutionContext*);

    // PlatformSpeechSynthesizerClient override methods.
    virtual void voicesDidChange() OVERRIDE;
    virtual void didStartSpeaking(PlatformSpeechSynthesisUtterance*) OVERRIDE;
    virtual void didPauseSpeaking(PlatformSpeechSynthesisUtterance*) OVERRIDE;
    virtual void didResumeSpeaking(PlatformSpeechSynthesisUtterance*) OVERRIDE;
    virtual void didFinishSpeaking(PlatformSpeechSynthesisUtterance*) OVERRIDE;
    virtual void speakingErrorOccurred(PlatformSpeechSynthesisUtterance*) OVERRIDE;
    virtual void boundaryEventOccurred(PlatformSpeechSynthesisUtterance*, SpeechBoundary, unsigned charIndex) OVERRIDE;

    void startSpeakingImmediately();
    void handleSpeakingCompleted(SpeechSynthesisUtterance*, bool errorOccurred);
    void fireEvent(const AtomicString& type, SpeechSynthesisUtterance*, unsigned long charIndex, const String& name);

    // Returns the utterance at the front of the queue.
    SpeechSynthesisUtterance* currentSpeechUtterance() const;

    Member<PlatformSpeechSynthesizer> m_platformSpeechSynthesizer;
    HeapVector<Member<SpeechSynthesisVoice> > m_voiceList;
    HeapDeque<Member<SpeechSynthesisUtterance> > m_utteranceQueue;
    bool m_isPaused;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
};

} // namespace WebCore

#endif // SpeechSynthesisEvent_h
