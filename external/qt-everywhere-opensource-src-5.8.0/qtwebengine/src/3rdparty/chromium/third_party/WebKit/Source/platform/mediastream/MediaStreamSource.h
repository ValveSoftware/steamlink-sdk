/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamSource_h
#define MediaStreamSource_h

#include "platform/PlatformExport.h"
#include "platform/audio/AudioDestinationConsumer.h"
#include "public/platform/WebMediaConstraints.h"
#include "wtf/Allocator.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT MediaStreamSource final : public GarbageCollectedFinalized<MediaStreamSource> {
public:
    class PLATFORM_EXPORT Observer : public GarbageCollectedMixin {
    public:
        virtual ~Observer() { }
        virtual void sourceChangedState() = 0;
    };

    class ExtraData {
        USING_FAST_MALLOC(ExtraData);
    public:
        virtual ~ExtraData() { }
    };

    enum StreamType {
        TypeAudio,
        TypeVideo
    };

    enum ReadyState {
        ReadyStateLive = 0,
        ReadyStateMuted = 1,
        ReadyStateEnded = 2
    };

    static MediaStreamSource* create(const String& id, StreamType, const String& name, bool remote, ReadyState = ReadyStateLive, bool requiresConsumer = false);

    const String& id() const { return m_id; }
    StreamType type() const { return m_type; }
    const String& name() const { return m_name; }
    bool remote() const { return m_remote; }

    void setReadyState(ReadyState);
    ReadyState getReadyState() const { return m_readyState; }

    void addObserver(Observer*);

    ExtraData* getExtraData() const { return m_extraData.get(); }
    void setExtraData(std::unique_ptr<ExtraData> extraData) { m_extraData = std::move(extraData); }

    void setConstraints(WebMediaConstraints constraints) { m_constraints = constraints; }
    WebMediaConstraints constraints() { return m_constraints; }

    void setAudioFormat(size_t numberOfChannels, float sampleRate);
    void consumeAudio(AudioBus*, size_t numberOfFrames);

    bool requiresAudioConsumer() const { return m_requiresConsumer; }
    void addAudioConsumer(AudioDestinationConsumer*);
    bool removeAudioConsumer(AudioDestinationConsumer*);
    const HeapHashSet<Member<AudioDestinationConsumer>>& audioConsumers() { return m_audioConsumers; }

    // |m_extraData| may hold pointers to GC objects, and it may touch them in destruction.
    // So this class is eagerly finalized to finalize |m_extraData| promptly.
    EAGERLY_FINALIZE();
    DECLARE_TRACE();

private:
    MediaStreamSource(const String& id, StreamType, const String& name, bool remote, ReadyState, bool requiresConsumer);

    String m_id;
    StreamType m_type;
    String m_name;
    bool m_remote;
    ReadyState m_readyState;
    bool m_requiresConsumer;
    HeapHashSet<WeakMember<Observer>> m_observers;
    Mutex m_audioConsumersLock;
    HeapHashSet<Member<AudioDestinationConsumer>> m_audioConsumers;
    std::unique_ptr<ExtraData> m_extraData;
    WebMediaConstraints m_constraints;
};

typedef HeapVector<Member<MediaStreamSource>> MediaStreamSourceVector;

} // namespace blink

#endif // MediaStreamSource_h
