/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef MediaSource_h
#define MediaSource_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/html/HTMLMediaSource.h"
#include "core/html/URLRegistry.h"
#include "modules/mediasource/SourceBuffer.h"
#include "modules/mediasource/SourceBufferList.h"
#include "public/platform/WebMediaSource.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace blink {
class WebSourceBuffer;
}

namespace WebCore {

class ExceptionState;
class GenericEventQueue;

class MediaSource FINAL
    : public RefCountedWillBeRefCountedGarbageCollected<MediaSource>
    , public HTMLMediaSource
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , public ScriptWrappable {
    REFCOUNTED_EVENT_TARGET(MediaSource);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MediaSource);
public:
    static const AtomicString& openKeyword();
    static const AtomicString& closedKeyword();
    static const AtomicString& endedKeyword();

    static PassRefPtrWillBeRawPtr<MediaSource> create(ExecutionContext*);
    virtual ~MediaSource();

    // MediaSource.idl methods
    SourceBufferList* sourceBuffers() { return m_sourceBuffers.get(); }
    SourceBufferList* activeSourceBuffers() { return m_activeSourceBuffers.get(); }
    SourceBuffer* addSourceBuffer(const String& type, ExceptionState&);
    void removeSourceBuffer(SourceBuffer*, ExceptionState&);
    void setDuration(double, ExceptionState&);
    const AtomicString& readyState() const { return m_readyState; }
    void endOfStream(const AtomicString& error, ExceptionState&);
    void endOfStream(ExceptionState&);
    static bool isTypeSupported(const String& type);

        // HTMLMediaSource
    virtual bool attachToElement(HTMLMediaElement*) OVERRIDE;
    virtual void setWebMediaSourceAndOpen(PassOwnPtr<blink::WebMediaSource>) OVERRIDE;
    virtual void close() OVERRIDE;
    virtual bool isClosed() const OVERRIDE;
    virtual double duration() const OVERRIDE;
    virtual PassRefPtr<TimeRanges> buffered() const OVERRIDE;
    virtual void refHTMLMediaSource() OVERRIDE { ref(); }
    virtual void derefHTMLMediaSource() OVERRIDE { deref(); }

    // EventTarget interface
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // ActiveDOMObject interface
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

    // URLRegistrable interface
    virtual URLRegistry& registry() const OVERRIDE;

    // Used by SourceBuffer.
    void openIfInEndedState();
    bool isOpen() const;

    // Used by MediaSourceRegistry.
    void addedToRegistry();
    void removedFromRegistry();

    void trace(Visitor*);

private:
    explicit MediaSource(ExecutionContext*);

    void setReadyState(const AtomicString&);
    void onReadyStateChange(const AtomicString&, const AtomicString&);

    bool isUpdating() const;

    PassOwnPtr<blink::WebSourceBuffer> createWebSourceBuffer(const String& type, const Vector<String>& codecs, ExceptionState&);
    void scheduleEvent(const AtomicString& eventName);
    void endOfStreamInternal(const blink::WebMediaSource::EndOfStreamStatus, ExceptionState&);

    // Implements the duration change algorithm.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#duration-change-algorithm
    void durationChangeAlgorithm(double newDuration);

    OwnPtr<blink::WebMediaSource> m_webMediaSource;
    AtomicString m_readyState;
    OwnPtrWillBeMember<GenericEventQueue> m_asyncEventQueue;
    // FIXME: oilpan: This should become a Member. For now, m_attachedElement will be cleared by the HTMLMediaElement destructor.
    HTMLMediaElement* m_attachedElement;

    RefPtrWillBeMember<SourceBufferList> m_sourceBuffers;
    RefPtrWillBeMember<SourceBufferList> m_activeSourceBuffers;
};

} // namespace WebCore

#endif
