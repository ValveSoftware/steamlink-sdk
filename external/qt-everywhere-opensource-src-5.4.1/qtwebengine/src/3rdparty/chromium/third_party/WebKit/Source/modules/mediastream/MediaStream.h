/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#ifndef MediaStream_h
#define MediaStream_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/html/URLRegistry.h"
#include "modules/EventTargetModules.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "platform/Timer.h"
#include "platform/mediastream/MediaStreamDescriptor.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class ExceptionState;

class MediaStream FINAL : public RefCountedWillBeRefCountedGarbageCollected<MediaStream>, public ScriptWrappable,
    public URLRegistrable, public MediaStreamDescriptorClient, public EventTargetWithInlineData, public ContextLifecycleObserver {
    REFCOUNTED_EVENT_TARGET(MediaStream);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MediaStream);
public:
    static PassRefPtrWillBeRawPtr<MediaStream> create(ExecutionContext*);
    static PassRefPtrWillBeRawPtr<MediaStream> create(ExecutionContext*, PassRefPtrWillBeRawPtr<MediaStream>);
    static PassRefPtrWillBeRawPtr<MediaStream> create(ExecutionContext*, const MediaStreamTrackVector&);
    static PassRefPtrWillBeRawPtr<MediaStream> create(ExecutionContext*, PassRefPtr<MediaStreamDescriptor>);
    virtual ~MediaStream();

    // DEPRECATED
    String label() const { return m_descriptor->id(); }

    String id() const { return m_descriptor->id(); }

    void addTrack(PassRefPtrWillBeRawPtr<MediaStreamTrack>, ExceptionState&);
    void removeTrack(PassRefPtrWillBeRawPtr<MediaStreamTrack>, ExceptionState&);
    MediaStreamTrack* getTrackById(String);
    PassRefPtrWillBeRawPtr<MediaStream> clone(ExecutionContext*);

    MediaStreamTrackVector getAudioTracks() const { return m_audioTracks; }
    MediaStreamTrackVector getVideoTracks() const { return m_videoTracks; }

    bool ended() const;
    void stop();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

    void trackEnded();

    // MediaStreamDescriptorClient
    virtual void streamEnded() OVERRIDE;

    MediaStreamDescriptor* descriptor() const { return m_descriptor.get(); }

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // URLRegistrable
    virtual URLRegistry& registry() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    MediaStream(ExecutionContext*, PassRefPtr<MediaStreamDescriptor>);
    MediaStream(ExecutionContext*, const MediaStreamTrackVector& audioTracks, const MediaStreamTrackVector& videoTracks);

    // ContextLifecycleObserver
    virtual void contextDestroyed() OVERRIDE;

    // MediaStreamDescriptorClient
    virtual void addRemoteTrack(MediaStreamComponent*) OVERRIDE;
    virtual void removeRemoteTrack(MediaStreamComponent*) OVERRIDE;

    void scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event>);
    void scheduledEventTimerFired(Timer<MediaStream>*);

    bool m_stopped;

    MediaStreamTrackVector m_audioTracks;
    MediaStreamTrackVector m_videoTracks;
    RefPtr<MediaStreamDescriptor> m_descriptor;

    Timer<MediaStream> m_scheduledEventTimer;
    WillBeHeapVector<RefPtrWillBeMember<Event> > m_scheduledEvents;
};

typedef WillBeHeapVector<RefPtrWillBeMember<MediaStream> > MediaStreamVector;

} // namespace WebCore

#endif // MediaStream_h
