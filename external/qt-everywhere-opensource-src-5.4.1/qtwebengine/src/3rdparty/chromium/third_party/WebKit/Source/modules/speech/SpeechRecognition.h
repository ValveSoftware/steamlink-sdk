/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpeechRecognition_h
#define SpeechRecognition_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/speech/SpeechGrammarList.h"
#include "modules/speech/SpeechRecognitionResult.h"
#include "platform/heap/Handle.h"
#include "wtf/Compiler.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class ExecutionContext;
class SpeechRecognitionController;
class SpeechRecognitionError;

class SpeechRecognition FINAL : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<SpeechRecognition>, public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<SpeechRecognition>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SpeechRecognition);
public:
    static SpeechRecognition* create(ExecutionContext*);
    virtual ~SpeechRecognition();

    // Attributes.
    SpeechGrammarList* grammars() { return m_grammars; }
    void setGrammars(SpeechGrammarList* grammars) { m_grammars = grammars; }
    String lang() { return m_lang; }
    void setLang(const String& lang) { m_lang = lang; }
    bool continuous() { return m_continuous; }
    void setContinuous(bool continuous) { m_continuous = continuous; }
    bool interimResults() { return m_interimResults; }
    void setInterimResults(bool interimResults) { m_interimResults = interimResults; }
    unsigned long maxAlternatives() { return m_maxAlternatives; }
    void setMaxAlternatives(unsigned long maxAlternatives) { m_maxAlternatives = maxAlternatives; }

    // Callable by the user.
    void start(ExceptionState&);
    void stopFunction();
    void abort();

    // Called by the SpeechRecognitionClient.
    void didStartAudio();
    void didStartSound();
    void didStartSpeech();
    void didEndSpeech();
    void didEndSound();
    void didEndAudio();
    void didReceiveResults(const HeapVector<Member<SpeechRecognitionResult> >& newFinalResults, const HeapVector<Member<SpeechRecognitionResult> >& currentInterimResults);
    void didReceiveNoMatch(SpeechRecognitionResult*);
    void didReceiveError(PassRefPtrWillBeRawPtr<SpeechRecognitionError>);
    void didStart();
    void didEnd();

    // EventTarget.
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // ActiveDOMObject.
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(audiostart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(soundstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(speechstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(speechend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(soundend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(audioend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(result);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(nomatch);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(start);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(end);

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit SpeechRecognition(ExecutionContext*);

    Member<SpeechGrammarList> m_grammars;
    String m_lang;
    bool m_continuous;
    bool m_interimResults;
    unsigned long m_maxAlternatives;

    RawPtrWillBeMember<SpeechRecognitionController> m_controller;
    bool m_stoppedByActiveDOMObject;
    bool m_started;
    bool m_stopping;
    HeapVector<Member<SpeechRecognitionResult> > m_finalResults;
};

} // namespace WebCore

#endif // SpeechRecognition_h
