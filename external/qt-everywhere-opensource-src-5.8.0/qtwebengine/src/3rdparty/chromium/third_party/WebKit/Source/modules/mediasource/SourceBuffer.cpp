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

#include "modules/mediasource/SourceBuffer.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/MediaError.h"
#include "core/html/TimeRanges.h"
#include "core/html/track/AudioTrack.h"
#include "core/html/track/AudioTrackList.h"
#include "core/html/track/VideoTrack.h"
#include "core/html/track/VideoTrackList.h"
#include "core/streams/Stream.h"
#include "modules/mediasource/MediaSource.h"
#include "modules/mediasource/SourceBufferTrackBaseSupplement.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "public/platform/WebSourceBuffer.h"
#include "wtf/MathExtras.h"
#include <limits>
#include <memory>
#include <sstream>

using blink::WebSourceBuffer;

#define SBLOG DVLOG(3)

namespace blink {

namespace {

static bool throwExceptionIfRemovedOrUpdating(bool isRemoved, bool isUpdating, ExceptionState& exceptionState)
{
    if (isRemoved) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return true;
    }
    if (isUpdating) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer is still processing an 'appendBuffer', 'appendStream', or 'remove' operation.");
        return true;
    }

    return false;
}

WTF::String webTimeRangesToString(const WebTimeRanges& ranges)
{
    StringBuilder stringBuilder;
    stringBuilder.append("{");
    for (auto& r : ranges) {
        stringBuilder.append(" [");
        stringBuilder.appendNumber(r.start);
        stringBuilder.append(";");
        stringBuilder.appendNumber(r.end);
        stringBuilder.append("]");
    }
    stringBuilder.append(" }");
    return stringBuilder.toString();
}

} // namespace

SourceBuffer* SourceBuffer::create(std::unique_ptr<WebSourceBuffer> webSourceBuffer, MediaSource* source, GenericEventQueue* asyncEventQueue)
{
    SourceBuffer* sourceBuffer = new SourceBuffer(std::move(webSourceBuffer), source, asyncEventQueue);
    sourceBuffer->suspendIfNeeded();
    return sourceBuffer;
}

SourceBuffer::SourceBuffer(std::unique_ptr<WebSourceBuffer> webSourceBuffer, MediaSource* source, GenericEventQueue* asyncEventQueue)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(source->getExecutionContext())
    , m_webSourceBuffer(std::move(webSourceBuffer))
    , m_source(source)
    , m_trackDefaults(TrackDefaultList::create())
    , m_asyncEventQueue(asyncEventQueue)
    , m_mode(segmentsKeyword())
    , m_updating(false)
    , m_timestampOffset(0)
    , m_appendWindowStart(0)
    , m_appendWindowEnd(std::numeric_limits<double>::infinity())
    , m_firstInitializationSegmentReceived(false)
    , m_pendingAppendDataOffset(0)
    , m_appendBufferAsyncPartRunner(AsyncMethodRunner<SourceBuffer>::create(this, &SourceBuffer::appendBufferAsyncPart))
    , m_pendingRemoveStart(-1)
    , m_pendingRemoveEnd(-1)
    , m_removeAsyncPartRunner(AsyncMethodRunner<SourceBuffer>::create(this, &SourceBuffer::removeAsyncPart))
    , m_streamMaxSizeValid(false)
    , m_streamMaxSize(0)
    , m_appendStreamAsyncPartRunner(AsyncMethodRunner<SourceBuffer>::create(this, &SourceBuffer::appendStreamAsyncPart))
{
    SBLOG << __FUNCTION__ << " this=" << this;

    DCHECK(m_webSourceBuffer);
    DCHECK(m_source);
    DCHECK(m_source->mediaElement());
    ThreadState::current()->registerPreFinalizer(this);
    m_audioTracks = AudioTrackList::create(*m_source->mediaElement());
    m_videoTracks = VideoTrackList::create(*m_source->mediaElement());
    m_webSourceBuffer->setClient(this);
}

SourceBuffer::~SourceBuffer()
{
    SBLOG << __FUNCTION__ << " this=" << this;
}

void SourceBuffer::dispose()
{
    // Promptly clears a raw reference from content/ to an on-heap object
    // so that content/ doesn't access it in a lazy sweeping phase.
    m_webSourceBuffer.reset();
}

const AtomicString& SourceBuffer::segmentsKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, segments, ("segments"));
    return segments;
}

const AtomicString& SourceBuffer::sequenceKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, sequence, ("sequence"));
    return sequence;
}

void SourceBuffer::setMode(const AtomicString& newMode, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " newMode=" << newMode;
    // Section 3.1 On setting mode attribute steps.
    // 1. Let new mode equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw
    //    an INVALID_STATE_ERR exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    // 6. If the new mode equals "sequence", then set the group start timestamp to the highest presentation end timestamp.
    WebSourceBuffer::AppendMode appendMode = WebSourceBuffer::AppendModeSegments;
    if (newMode == sequenceKeyword())
        appendMode = WebSourceBuffer::AppendModeSequence;
    if (!m_webSourceBuffer->setMode(appendMode)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The mode may not be set while the SourceBuffer's append state is 'PARSING_MEDIA_SEGMENT'.");
        return;
    }

    // 7. Update the attribute to new mode.
    m_mode = newMode;
}

TimeRanges* SourceBuffer::buffered(ExceptionState& exceptionState) const
{
    // Section 3.1 buffered attribute steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    if (isRemoved()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return nullptr;
    }

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return TimeRanges::create(m_webSourceBuffer->buffered());
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset;
}

void SourceBuffer::setTimestampOffset(double offset, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " offset=" << offset;
    // Section 3.1 timestampOffset attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-SourceBuffer-timestampOffset
    // 1. Let new timestamp offset equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an
    //    InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    // 6. If the mode attribute equals "sequence", then set the group start timestamp to new timestamp offset.
    if (!m_webSourceBuffer->setTimestampOffset(offset)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The timestamp offset may not be set while the SourceBuffer's append state is 'PARSING_MEDIA_SEGMENT'.");
        return;
    }

    // 7. Update the attribute to new timestamp offset.
    m_timestampOffset = offset;
}

AudioTrackList& SourceBuffer::audioTracks()
{
    DCHECK(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    return *m_audioTracks;
}

VideoTrackList& SourceBuffer::videoTracks()
{
    DCHECK(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    return *m_videoTracks;
}

double SourceBuffer::appendWindowStart() const
{
    return m_appendWindowStart;
}

void SourceBuffer::setAppendWindowStart(double start, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " start=" << start;
    // Section 3.1 appendWindowStart attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-SourceBuffer-appendWindowStart
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. If the new value is less than 0 or greater than or equal to appendWindowEnd then throw an InvalidAccessError
    //    exception and abort these steps.
    if (start < 0 || start >= m_appendWindowEnd) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::indexOutsideRange("value", start, 0.0, ExceptionMessages::ExclusiveBound, m_appendWindowEnd, ExceptionMessages::InclusiveBound));
        return;
    }

    m_webSourceBuffer->setAppendWindowStart(start);

    // 4. Update the attribute to the new value.
    m_appendWindowStart = start;
}

double SourceBuffer::appendWindowEnd() const
{
    return m_appendWindowEnd;
}

void SourceBuffer::setAppendWindowEnd(double end, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " end=" << end;
    // Section 3.1 appendWindowEnd attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-SourceBuffer-appendWindowEnd
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. If the new value equals NaN, then throw an InvalidAccessError and abort these steps.
    if (std::isnan(end)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::notAFiniteNumber(end));
        return;
    }
    // 4. If the new value is less than or equal to appendWindowStart then throw an InvalidAccessError
    //    exception and abort these steps.
    if (end <= m_appendWindowStart) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::indexExceedsMinimumBound("value", end, m_appendWindowStart));
        return;
    }

    m_webSourceBuffer->setAppendWindowEnd(end);

    // 5. Update the attribute to the new value.
    m_appendWindowEnd = end;
}

void SourceBuffer::appendBuffer(DOMArrayBuffer* data, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " size=" << data->byteLength();
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    appendBufferInternal(static_cast<const unsigned char*>(data->data()), data->byteLength(), exceptionState);
}

void SourceBuffer::appendBuffer(DOMArrayBufferView* data, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " size=" << data->byteLength();
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    appendBufferInternal(static_cast<const unsigned char*>(data->baseAddress()), data->byteLength(), exceptionState);
}

void SourceBuffer::appendStream(Stream* stream, ExceptionState& exceptionState)
{
    m_streamMaxSizeValid = false;
    appendStreamInternal(stream, exceptionState);
}

void SourceBuffer::appendStream(Stream* stream, unsigned long long maxSize, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " maxSize=" << maxSize;
    m_streamMaxSizeValid = maxSize > 0;
    if (m_streamMaxSizeValid)
        m_streamMaxSize = maxSize;
    appendStreamInternal(stream, exceptionState);
}

void SourceBuffer::abort(ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this;
    // http://w3c.github.io/media-source/#widl-SourceBuffer-abort-void
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source
    //    then throw an InvalidStateError exception and abort these steps.
    // 2. If the readyState attribute of the parent media source is not in the "open" state
    //    then throw an InvalidStateError exception and abort these steps.
    if (isRemoved()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return;
    }
    if (!m_source->isOpen()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The parent media source's readyState is not 'open'.");
        return;
    }

    // 3. If the range removal algorithm is running, then throw an
    //    InvalidStateError exception and abort these steps.
    if (m_pendingRemoveStart != -1) {
        DCHECK(m_updating);
        // Throwing the exception and aborting these steps is new behavior that
        // is implemented behind the MediaSourceNewAbortAndDuration
        // RuntimeEnabledFeature.
        if (RuntimeEnabledFeatures::mediaSourceNewAbortAndDurationEnabled()) {
            MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "Aborting asynchronous remove() operation is disallowed.");
            return;
        }

        Deprecation::countDeprecation(m_source->mediaElement()->document(), UseCounter::MediaSourceAbortRemove);
        cancelRemove();
    }

    // 4. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    abortIfUpdating();

    // 5. Run the reset parser state algorithm.
    m_webSourceBuffer->resetParserState();

    // 6. Set appendWindowStart to 0.
    setAppendWindowStart(0, exceptionState);

    // 7. Set appendWindowEnd to positive Infinity.
    setAppendWindowEnd(std::numeric_limits<double>::infinity(), exceptionState);
}

void SourceBuffer::remove(double start, double end, ExceptionState& exceptionState)
{
    SBLOG << __FUNCTION__ << " this=" << this << " start=" << start << " end=" << end;

    // Section 3.2 remove() method steps.
    // 1. If duration equals NaN, then throw an InvalidAccessError exception and abort these steps.
    // 2. If start is negative or greater than duration, then throw an InvalidAccessError exception and abort these steps.

    if (start < 0 || (m_source && (std::isnan(m_source->duration()) || start > m_source->duration()))) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::indexOutsideRange("start", start, 0.0, ExceptionMessages::ExclusiveBound, !m_source || std::isnan(m_source->duration()) ? 0 : m_source->duration(), ExceptionMessages::ExclusiveBound));
        return;
    }

    // 3. If end is less than or equal to start or end equals NaN, then throw an InvalidAccessError exception and abort these steps.
    if (end <= start || std::isnan(end)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, "The end value provided (" + String::number(end) + ") must be greater than the start value provided (" + String::number(start) + ").");
        return;
    }

    // 4. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 5. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::remove", this);

    // 6. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 6.1. Set the readyState attribute of the parent media source to "open"
    // 6.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 7. Run the range removal algorithm with start and end as the start and end of the removal range.
    // 7.3. Set the updating attribute to true.
    m_updating = true;

    // 7.4. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 7.5. Return control to the caller and run the rest of the steps asynchronously.
    m_pendingRemoveStart = start;
    m_pendingRemoveEnd = end;
    m_removeAsyncPartRunner->runAsync();
}

void SourceBuffer::setTrackDefaults(TrackDefaultList* trackDefaults, ExceptionState& exceptionState)
{
    // Per 02 Dec 2014 Editor's Draft
    // http://w3c.github.io/media-source/#widl-SourceBuffer-trackDefaults
    // 1. If this object has been removed from the sourceBuffers attribute of
    //    the parent media source, then throw an InvalidStateError exception
    //    and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError
    //    exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. Update the attribute to the new value.
    m_trackDefaults = trackDefaults;
}

void SourceBuffer::cancelRemove()
{
    DCHECK(m_updating);
    DCHECK_NE(m_pendingRemoveStart, -1);
    m_removeAsyncPartRunner->stop();
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;
    m_updating = false;

    if (!RuntimeEnabledFeatures::mediaSourceNewAbortAndDurationEnabled()) {
        scheduleEvent(EventTypeNames::abort);
        scheduleEvent(EventTypeNames::updateend);
    }

    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::remove", this);
}

void SourceBuffer::abortIfUpdating()
{
    // Section 3.2 abort() method step 4 substeps.
    // http://w3c.github.io/media-source/#widl-SourceBuffer-abort-void

    if (!m_updating)
        return;

    DCHECK_EQ(m_pendingRemoveStart, -1);

    const char* traceEventName = 0;
    if (!m_pendingAppendData.isEmpty()) {
        traceEventName = "SourceBuffer::appendBuffer";
    } else if (m_stream) {
        traceEventName = "SourceBuffer::appendStream";
    } else {
        NOTREACHED();
    }

    // 4.1. Abort the buffer append and stream append loop algorithms if they are running.
    m_appendBufferAsyncPartRunner->stop();
    m_pendingAppendData.clear();
    m_pendingAppendDataOffset = 0;

    m_appendStreamAsyncPartRunner->stop();
    clearAppendStreamState();

    // 4.2. Set the updating attribute to false.
    m_updating = false;

    // 4.3. Queue a task to fire a simple event named abort at this SourceBuffer object.
    scheduleEvent(EventTypeNames::abort);

    // 4.4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);

    TRACE_EVENT_ASYNC_END0("media", traceEventName, this);
}

void SourceBuffer::removedFromMediaSource()
{
    if (isRemoved())
        return;

    SBLOG << __FUNCTION__ << " this=" << this;
    if (m_pendingRemoveStart != -1) {
        cancelRemove();
    } else {
        abortIfUpdating();
    }

    if (RuntimeEnabledFeatures::audioVideoTracksEnabled()) {
        DCHECK(m_source);
        if (m_source->mediaElement()->audioTracks().length() > 0
            || m_source->mediaElement()->videoTracks().length() > 0) {
            removeMediaTracks();
        }
    }

    m_webSourceBuffer->removedFromMediaSource();
    m_webSourceBuffer.reset();
    m_source = nullptr;
    m_asyncEventQueue = nullptr;
}

double SourceBuffer::highestPresentationTimestamp()
{
    DCHECK(!isRemoved());

    double pts = m_webSourceBuffer->highestPresentationTimestamp();
    SBLOG << __FUNCTION__ << " this=" << this << ", pts=" << pts;
    return pts;
}

void SourceBuffer::removeMediaTracks()
{
    DCHECK(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    // Spec: http://w3c.github.io/media-source/#widl-MediaSource-removeSourceBuffer-void-SourceBuffer-sourceBuffer
    DCHECK(m_source);

    HTMLMediaElement* mediaElement = m_source->mediaElement();
    DCHECK(mediaElement);
    // 3. Let SourceBuffer audioTracks list equal the AudioTrackList object returned by sourceBuffer.audioTracks.
    // 4. If the SourceBuffer audioTracks list is not empty, then run the following steps:
    // 4.1 Let HTMLMediaElement audioTracks list equal the AudioTrackList object returned by the audioTracks attribute on the HTMLMediaElement.
    // 4.2 Let the removed enabled audio track flag equal false.
    bool removedEnabledAudioTrack = false;
    // 4.3 For each AudioTrack object in the SourceBuffer audioTracks list, run the following steps:
    while (audioTracks().length() > 0) {
        AudioTrack* audioTrack = audioTracks().anonymousIndexedGetter(0);
        // 4.3.1 Set the sourceBuffer attribute on the AudioTrack object to null.
        SourceBufferTrackBaseSupplement::setSourceBuffer(*audioTrack, nullptr);
        // 4.3.2 If the enabled attribute on the AudioTrack object is true, then set the removed enabled audio track flag to true.
        if (audioTrack->enabled())
            removedEnabledAudioTrack = true;
        // 4.3.3 Remove the AudioTrack object from the HTMLMediaElement audioTracks list.
        // 4.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement audioTracks list.
        mediaElement->audioTracks().remove(audioTrack->id());
        // 4.3.5 Remove the AudioTrack object from the SourceBuffer audioTracks list.
        // 4.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the SourceBuffer audioTracks list.
        audioTracks().remove(audioTrack->id());
    }
    // 4.4 If the removed enabled audio track flag equals true, then queue a task to fire a simple event named change at the HTMLMediaElement audioTracks list.
    if (removedEnabledAudioTrack) {
        Event* event = Event::create(EventTypeNames::change);
        event->setTarget(&mediaElement->audioTracks());
        mediaElement->scheduleEvent(event);
    }

    // 5. Let SourceBuffer videoTracks list equal the VideoTrackList object returned by sourceBuffer.videoTracks.
    // 6. If the SourceBuffer videoTracks list is not empty, then run the following steps:
    // 6.1 Let HTMLMediaElement videoTracks list equal the VideoTrackList object returned by the videoTracks attribute on the HTMLMediaElement.
    // 6.2 Let the removed selected video track flag equal false.
    bool removedSelectedVideoTrack = false;
    // 6.3 For each VideoTrack object in the SourceBuffer videoTracks list, run the following steps:
    while (videoTracks().length() > 0) {
        VideoTrack* videoTrack = videoTracks().anonymousIndexedGetter(0);
        // 6.3.1 Set the sourceBuffer attribute on the VideoTrack object to null.
        SourceBufferTrackBaseSupplement::setSourceBuffer(*videoTrack, nullptr);
        // 6.3.2 If the selected attribute on the VideoTrack object is true, then set the removed selected video track flag to true.
        if (videoTrack->selected())
            removedSelectedVideoTrack = true;
        // 6.3.3 Remove the VideoTrack object from the HTMLMediaElement videoTracks list.
        // 6.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement videoTracks list.
        mediaElement->videoTracks().remove(videoTrack->id());
        // 6.3.5 Remove the VideoTrack object from the SourceBuffer videoTracks list.
        // 6.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the SourceBuffer videoTracks list.
        videoTracks().remove(videoTrack->id());
    }
    // 6.4 If the removed selected video track flag equals true, then queue a task to fire a simple event named change at the HTMLMediaElement videoTracks list.
    if (removedSelectedVideoTrack) {
        Event* event = Event::create(EventTypeNames::change);
        event->setTarget(&mediaElement->videoTracks());
        mediaElement->scheduleEvent(event);
    }

    // 7-8. TODO(servolk): Remove text tracks once SourceBuffer has text tracks.
}

template<class T>
T* findExistingTrackById(const TrackListBase<T>& trackList, const String& id)
{
    // According to MSE specification (https://w3c.github.io/media-source/#sourcebuffer-init-segment-received) step 3.1:
    // > If more than one track for a single type are present (ie 2 audio tracks), then the Track IDs match the ones in the first initialization segment.
    // I.e. we only need to search by TrackID if there is more than one track, otherwise we can assume that the only
    // track of the given type is the same one that we had in previous init segments.
    if (trackList.length() == 1)
        return trackList.anonymousIndexedGetter(0);
    return trackList.getTrackById(id);
}

const TrackDefault* SourceBuffer::getTrackDefault(const AtomicString& trackType, const AtomicString& byteStreamTrackID) const
{
    // This is a helper for implementation of default track label and default track language algorithms.
    // defaultTrackLabel spec: https://w3c.github.io/media-source/#sourcebuffer-default-track-label
    // defaultTrackLanguage spec: https://w3c.github.io/media-source/#sourcebuffer-default-track-language

    // 1. If trackDefaults contains a TrackDefault object with a type attribute equal to type and a byteStreamTrackID attribute equal to byteStreamTrackID,
    // then return the value of the label/language attribute on this matching object and abort these steps.
    // 2. If trackDefaults contains a TrackDefault object with a type attribute equal to type and a byteStreamTrackID attribute equal to an empty string,
    // then return the value of the label/language attribute on this matching object and abort these steps.
    // 3. Return an empty string to the caller
    const TrackDefault* trackDefaultWithEmptyBytestreamId = nullptr;
    for (unsigned i = 0; i < m_trackDefaults->length(); ++i) {
        const TrackDefault* trackDefault = m_trackDefaults->item(i);
        if (trackDefault->type() != trackType)
            continue;
        if (trackDefault->byteStreamTrackID() == byteStreamTrackID)
            return trackDefault;
        if (!trackDefaultWithEmptyBytestreamId && trackDefault->byteStreamTrackID() == "")
            trackDefaultWithEmptyBytestreamId = trackDefault;
    }
    return trackDefaultWithEmptyBytestreamId;
}

AtomicString SourceBuffer::defaultTrackLabel(const AtomicString& trackType, const AtomicString& byteStreamTrackID) const
{
    // Spec: https://w3c.github.io/media-source/#sourcebuffer-default-track-label
    const TrackDefault* trackDefault = getTrackDefault(trackType, byteStreamTrackID);
    return trackDefault ? AtomicString(trackDefault->label()) : "";
}

AtomicString SourceBuffer::defaultTrackLanguage(const AtomicString& trackType, const AtomicString& byteStreamTrackID) const
{
    // Spec: https://w3c.github.io/media-source/#sourcebuffer-default-track-language
    const TrackDefault* trackDefault = getTrackDefault(trackType, byteStreamTrackID);
    return trackDefault ? AtomicString(trackDefault->language()) : "";
}

bool SourceBuffer::initializationSegmentReceived(const WebVector<MediaTrackInfo>& newTracks)
{
    SBLOG << __FUNCTION__ << " this=" << this << " tracks=" << newTracks.size();
    DCHECK(m_source);
    DCHECK(m_source->mediaElement());
    DCHECK(m_updating);

    if (!RuntimeEnabledFeatures::audioVideoTracksEnabled()) {
        if (!m_firstInitializationSegmentReceived) {
            m_source->setSourceBufferActive(this);
            m_firstInitializationSegmentReceived = true;
        }
        return true;
    }

    // Implementation of Initialization Segment Received, see
    // https://w3c.github.io/media-source/#sourcebuffer-init-segment-received

    // Sort newTracks into audio and video tracks to facilitate implementation
    // of subsequent steps of this algorithm.
    Vector<MediaTrackInfo> newAudioTracks;
    Vector<MediaTrackInfo> newVideoTracks;
    for (const MediaTrackInfo& trackInfo : newTracks) {
        const TrackBase* track = nullptr;
        if (trackInfo.trackType == WebMediaPlayer::AudioTrack) {
            newAudioTracks.append(trackInfo);
            if (m_firstInitializationSegmentReceived)
                track = findExistingTrackById(audioTracks(), trackInfo.id);
        } else if (trackInfo.trackType == WebMediaPlayer::VideoTrack) {
            newVideoTracks.append(trackInfo);
            if (m_firstInitializationSegmentReceived)
                track = findExistingTrackById(videoTracks(), trackInfo.id);
        } else {
            SBLOG << __FUNCTION__ << " this=" << this << " failed: unsupported track type " << trackInfo.trackType;
            // TODO(servolk): Add handling of text tracks.
            NOTREACHED();
        }
        if (m_firstInitializationSegmentReceived && !track) {
            SBLOG << __FUNCTION__ << " this=" << this << " failed: tracks mismatch the first init segment.";
            return false;
        }
#if !LOG_DISABLED
        const char* logTrackTypeStr = (trackInfo.trackType == WebMediaPlayer::AudioTrack) ? "audio" : "video";
        SBLOG << __FUNCTION__ << " this=" << this << " : " << logTrackTypeStr << " track "
            << " id=" << String(trackInfo.id) << " byteStreamTrackID=" << String(trackInfo.byteStreamTrackID)
            << " kind=" << String(trackInfo.kind) << " label=" << String(trackInfo.label) << " language=" << String(trackInfo.language);
#endif
    }

    // 1. Update the duration attribute if it currently equals NaN:
    // TODO(servolk): Pass also stream duration into initSegmentReceived.

    // 2. If the initialization segment has no audio, video, or text tracks, then run the append error algorithm with the decode error parameter set to true and abort these steps.
    if (newTracks.size() == 0) {
        SBLOG << __FUNCTION__ << " this=" << this << " failed: no tracks found in the init segment.";
        // The append error algorithm will be called at the top level after we return false here to indicate failure.
        return false;
    }

    // 3. If the first initialization segment received flag is true, then run the following steps:
    if (m_firstInitializationSegmentReceived) {
        // 3.1 Verify the following properties. If any of the checks fail then run the append error algorithm with the decode error parameter set to true and abort these steps.
        bool tracksMatchFirstInitSegment = true;
        // - The number of audio, video, and text tracks match what was in the first initialization segment.
        if (newAudioTracks.size() != audioTracks().length() || newVideoTracks.size() != videoTracks().length()) {
            tracksMatchFirstInitSegment = false;
        }
        // - The codecs for each track, match what was specified in the first initialization segment.
        // This is currently done in MediaSourceState::OnNewConfigs.
        // - If more than one track for a single type are present (ie 2 audio tracks), then the Track IDs match the ones in the first initialization segment.
        if (tracksMatchFirstInitSegment && newAudioTracks.size() > 1) {
            for (size_t i = 0; i < newAudioTracks.size(); ++i) {
                const String& newTrackId = newVideoTracks[i].id;
                if (newTrackId != String(audioTracks().anonymousIndexedGetter(i)->id())) {
                    tracksMatchFirstInitSegment = false;
                    break;
                }
            }
        }

        if (tracksMatchFirstInitSegment && newVideoTracks.size() > 1) {
            for (size_t i = 0; i < newVideoTracks.size(); ++i) {
                const String& newTrackId = newVideoTracks[i].id;
                if (newTrackId != String(videoTracks().anonymousIndexedGetter(i)->id())) {
                    tracksMatchFirstInitSegment = false;
                    break;
                }
            }
        }

        if (!tracksMatchFirstInitSegment) {
            SBLOG << __FUNCTION__ << " this=" << this << " failed: tracks mismatch the first init segment.";
            // The append error algorithm will be called at the top level after we return false here to indicate failure.
            return false;
        }

        // 3.2 Add the appropriate track descriptions from this initialization segment to each of the track buffers.
        // This is done in Chromium code in stream parsers and demuxer implementations.

        // 3.3 Set the need random access point flag on all track buffers to true.
        // This is done in Chromium code, see MediaSourceState::OnNewConfigs.
    }

    // 4. Let active track flag equal false.
    m_activeTrack = false;

    // 5. If the first initialization segment received flag is false, then run the following steps:
    if (!m_firstInitializationSegmentReceived) {
        // 5.1 If the initialization segment contains tracks with codecs the user agent does not support, then run the append error algorithm with the decode error parameter set to true and abort these steps.
        // This is done in Chromium code, see MediaSourceState::OnNewConfigs.

        // 5.2 For each audio track in the initialization segment, run following steps:
        for (const MediaTrackInfo& trackInfo : newAudioTracks) {
            // 5.2.1 Let audio byte stream track ID be the Track ID for the current track being processed.
            const auto& byteStreamTrackID = trackInfo.byteStreamTrackID;
            // 5.2.2 Let audio language be a BCP 47 language tag for the language specified in the initialization segment for this track or an empty string if no language info is present.
            WebString language = trackInfo.language;
            // 5.2.3 If audio language equals an empty string or the 'und' BCP 47 value, then run the default track language algorithm with byteStreamTrackID set to
            // audio byte stream track ID and type set to "audio" and assign the value returned by the algorithm to audio language.
            if (language.isEmpty() || language == "und")
                language = defaultTrackLanguage(TrackDefault::audioKeyword(), byteStreamTrackID);
            // 5.2.4 Let audio label be a label specified in the initialization segment for this track or an empty string if no label info is present.
            WebString label = trackInfo.label;
            // 5.3.5 If audio label equals an empty string, then run the default track label algorithm with byteStreamTrackID set to audio byte stream track ID and
            // type set to "audio" and assign the value returned by the algorithm to audio label.
            if (label.isEmpty())
                label = defaultTrackLabel(TrackDefault::audioKeyword(), byteStreamTrackID);
            // 5.2.6 Let audio kinds be an array of kind strings specified in the initialization segment for this track or an empty array if no kind information is provided.
            const auto& kind = trackInfo.kind;
            // 5.2.7 TODO(servolk): Implement track kind processing.
            // 5.2.8.2 Let new audio track be a new AudioTrack object.
            AudioTrack* audioTrack = AudioTrack::create(trackInfo.id, kind, label, language, false);
            SourceBufferTrackBaseSupplement::setSourceBuffer(*audioTrack, this);
            // 5.2.8.7 If audioTracks.length equals 0, then run the following steps:
            if (audioTracks().length() == 0) {
                // 5.2.8.7.1 Set the enabled property on new audio track to true.
                audioTrack->setEnabled(true);
                // 5.2.8.7.2 Set active track flag to true.
                m_activeTrack = true;
            }
            // 5.2.8.8 Add new audio track to the audioTracks attribute on this SourceBuffer object.
            // 5.2.8.9 Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object referenced by the audioTracks attribute on this SourceBuffer object.
            audioTracks().add(audioTrack);
            // 5.2.8.10 Add new audio track to the audioTracks attribute on the HTMLMediaElement.
            // 5.2.8.11 Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the AudioTrackList object referenced by the audioTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->audioTracks().add(audioTrack);
        }

        // 5.3. For each video track in the initialization segment, run following steps:
        for (const MediaTrackInfo& trackInfo : newVideoTracks) {
            // 5.3.1 Let video byte stream track ID be the Track ID for the current track being processed.
            const auto& byteStreamTrackID = trackInfo.byteStreamTrackID;
            // 5.3.2 Let video language be a BCP 47 language tag for the language specified in the initialization segment for this track or an empty string if no language info is present.
            WebString language = trackInfo.language;
            // 5.3.3 If video language equals an empty string or the 'und' BCP 47 value, then run the default track language algorithm with byteStreamTrackID set to
            // video byte stream track ID and type set to "video" and assign the value returned by the algorithm to video language.
            if (language.isEmpty() || language == "und")
                language = defaultTrackLanguage(TrackDefault::videoKeyword(), byteStreamTrackID);
            // 5.3.4 Let video label be a label specified in the initialization segment for this track or an empty string if no label info is present.
            WebString label = trackInfo.label;
            // 5.3.5 If video label equals an empty string, then run the default track label algorithm with byteStreamTrackID set to video byte stream track ID and
            // type set to "video" and assign the value returned by the algorithm to video label.
            if (label.isEmpty())
                label = defaultTrackLabel(TrackDefault::videoKeyword(), byteStreamTrackID);
            // 5.3.6 Let video kinds be an array of kind strings specified in the initialization segment for this track or an empty array if no kind information is provided.
            const auto& kind = trackInfo.kind;
            // 5.3.7 TODO(servolk): Implement track kind processing.
            // 5.3.8.2 Let new video track be a new VideoTrack object.
            VideoTrack* videoTrack = VideoTrack::create(trackInfo.id, kind, label, language, false);
            SourceBufferTrackBaseSupplement::setSourceBuffer(*videoTrack, this);
            // 5.3.8.7 If videoTracks.length equals 0, then run the following steps:
            if (videoTracks().length() == 0) {
                // 5.3.8.7.1 Set the selected property on new audio track to true.
                videoTrack->setSelected(true);
                // 5.3.8.7.2 Set active track flag to true.
                m_activeTrack = true;
            }
            // 5.3.8.8 Add new video track to the videoTracks attribute on this SourceBuffer object.
            // 5.3.8.9 Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object referenced by the videoTracks attribute on this SourceBuffer object.
            videoTracks().add(videoTrack);
            // 5.3.8.10 Add new video track to the videoTracks attribute on the HTMLMediaElement.
            // 5.3.8.11 Queue a task to fire a trusted event named addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the VideoTrackList object referenced by the videoTracks attribute on the HTMLMediaElement.
            m_source->mediaElement()->videoTracks().add(videoTrack);
        }

        // 5.4 TODO(servolk): Add text track processing here.

        // 5.5 If active track flag equals true, then run the following steps:
        // activesourcebuffers.
        if (m_activeTrack) {
            // 5.5.1 Add this SourceBuffer to activeSourceBuffers.
            // 5.5.2 Queue a task to fire a simple event named addsourcebuffer at activeSourceBuffers
            m_source->setSourceBufferActive(this);
        }

        // 5.6. Set first initialization segment received flag to true.
        m_firstInitializationSegmentReceived = true;
    }

    return true;
}

bool SourceBuffer::hasPendingActivity() const
{
    return m_source;
}

void SourceBuffer::suspend()
{
    m_appendBufferAsyncPartRunner->suspend();
    m_removeAsyncPartRunner->suspend();
    m_appendStreamAsyncPartRunner->suspend();
}

void SourceBuffer::resume()
{
    m_appendBufferAsyncPartRunner->resume();
    m_removeAsyncPartRunner->resume();
    m_appendStreamAsyncPartRunner->resume();
}

void SourceBuffer::stop()
{
    m_appendBufferAsyncPartRunner->stop();
    m_removeAsyncPartRunner->stop();
    m_appendStreamAsyncPartRunner->stop();
}

ExecutionContext* SourceBuffer::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

const AtomicString& SourceBuffer::interfaceName() const
{
    return EventTargetNames::SourceBuffer;
}

bool SourceBuffer::isRemoved() const
{
    return !m_source;
}

void SourceBuffer::scheduleEvent(const AtomicString& eventName)
{
    DCHECK(m_asyncEventQueue);

    Event* event = Event::create(eventName);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event);
}

bool SourceBuffer::prepareAppend(size_t newDataSize, ExceptionState& exceptionState)
{
    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::prepareAppend", this);
    // http://w3c.github.io/media-source/#sourcebuffer-prepare-append
    // 3.5.4 Prepare Append Algorithm
    // 1. If the SourceBuffer has been removed from the sourceBuffers attribute of the parent media source then throw an InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState)) {
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
        return false;
    }

    // 3. If the HTMLMediaElement.error attribute is not null, then throw an InvalidStateError exception and abort these steps.
    DCHECK(m_source);
    DCHECK(m_source->mediaElement());
    if (m_source->mediaElement()->error()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The HTMLMediaElement.error attribute is not null.");
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
        return false;
    }

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    //    1. Set the readyState attribute of the parent media source to "open"
    //    2. Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. Run the coded frame eviction algorithm.
    if (!evictCodedFrames(newDataSize)) {
        // 6. If the buffer full flag equals true, then throw a QUOTA_EXCEEDED_ERR exception and abort these steps.
        SBLOG << __FUNCTION__ << " this=" << this << " -> throw QuotaExceededError";
        MediaSource::logAndThrowDOMException(exceptionState, QuotaExceededError, "The SourceBuffer is full, and cannot free space to append additional buffers.");
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
        return false;
    }

    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
    return true;
}

bool SourceBuffer::evictCodedFrames(size_t newDataSize)
{
    DCHECK(m_source);
    DCHECK(m_source->mediaElement());
    double currentTime = m_source->mediaElement()->currentTime();
    bool result = m_webSourceBuffer->evictCodedFrames(currentTime, newDataSize);
    if (!result) {
        SBLOG << __FUNCTION__ << " this=" << this << " failed. newDataSize=" << newDataSize
            << " currentTime=" << currentTime << " buffered=" << webTimeRangesToString(m_webSourceBuffer->buffered());
    }
    return result;
}

void SourceBuffer::appendBufferInternal(const unsigned char* data, unsigned size, ExceptionState& exceptionState)
{
    TRACE_EVENT_ASYNC_BEGIN1("media", "SourceBuffer::appendBuffer", this, "size", size);
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data

    // 1. Run the prepare append algorithm.
    if (!prepareAppend(size, exceptionState)) {
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendBuffer", this);
        return;
    }
    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "prepareAppend");

    // 2. Add data to the end of the input buffer.
    DCHECK(data || size == 0);
    if (data)
        m_pendingAppendData.append(data, size);
    m_pendingAppendDataOffset = 0;

    // 3. Set the updating attribute to true.
    m_updating = true;

    // 4. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 5. Asynchronously run the buffer append algorithm.
    m_appendBufferAsyncPartRunner->runAsync();

    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "initialDelay");
}

void SourceBuffer::appendBufferAsyncPart()
{
    DCHECK(m_updating);

    // Section 3.5.4 Buffer Append Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 1. Run the segment parser loop algorithm.
    // Step 2 doesn't apply since we run Step 1 synchronously here.
    DCHECK_GE(m_pendingAppendData.size(), m_pendingAppendDataOffset);
    size_t appendSize = m_pendingAppendData.size() - m_pendingAppendDataOffset;

    // Impose an arbitrary max size for a single append() call so that an append
    // doesn't block the renderer event loop very long. This value was selected
    // by looking at YouTube SourceBuffer usage across a variety of bitrates.
    // This value allows relatively large appends while keeping append() call
    // duration in the  ~5-15ms range.
    const size_t MaxAppendSize = 128 * 1024;
    if (appendSize > MaxAppendSize)
        appendSize = MaxAppendSize;

    TRACE_EVENT_ASYNC_STEP_INTO1("media", "SourceBuffer::appendBuffer", this, "appending", "appendSize", static_cast<unsigned>(appendSize));

    // |zero| is used for 0 byte appends so we always have a valid pointer.
    // We need to convey all appends, even 0 byte ones to |m_webSourceBuffer|
    // so that it can clear its end of stream state if necessary.
    unsigned char zero = 0;
    unsigned char* appendData = &zero;
    if (appendSize)
        appendData = m_pendingAppendData.data() + m_pendingAppendDataOffset;

    bool appendSuccess = m_webSourceBuffer->append(appendData, appendSize, &m_timestampOffset);

    if (!appendSuccess) {
        m_pendingAppendData.clear();
        m_pendingAppendDataOffset = 0;
        appendError(DecodeError);
    } else {
        m_pendingAppendDataOffset += appendSize;

        if (m_pendingAppendDataOffset < m_pendingAppendData.size()) {
            m_appendBufferAsyncPartRunner->runAsync();
            TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "nextPieceDelay");
            return;
        }

        // 3. Set the updating attribute to false.
        m_updating = false;
        m_pendingAppendData.clear();
        m_pendingAppendDataOffset = 0;

        // 4. Queue a task to fire a simple event named update at this SourceBuffer object.
        scheduleEvent(EventTypeNames::update);

        // 5. Queue a task to fire a simple event named updateend at this SourceBuffer object.
        scheduleEvent(EventTypeNames::updateend);
    }

    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendBuffer", this);
    SBLOG << __FUNCTION__ << " done. this=" << this << " buffered=" << webTimeRangesToString(m_webSourceBuffer->buffered());
}

void SourceBuffer::removeAsyncPart()
{
    DCHECK(m_updating);
    DCHECK_GE(m_pendingRemoveStart, 0);
    DCHECK_LT(m_pendingRemoveStart, m_pendingRemoveEnd);

    // Section 3.2 remove() method steps
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-remove-void-double-start-double-end

    // 9. Run the coded frame removal algorithm with start and end as the start and end of the removal range.
    m_webSourceBuffer->remove(m_pendingRemoveStart, m_pendingRemoveEnd);

    // 10. Set the updating attribute to false.
    m_updating = false;
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;

    // 11. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 12. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
}

void SourceBuffer::appendStreamInternal(Stream* stream, ExceptionState& exceptionState)
{
    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::appendStream", this);

    // Section 3.2 appendStream()
    // http://w3c.github.io/media-source/#widl-SourceBuffer-appendStream-void-ReadableStream-stream-unsigned-long-long-maxSize
    // (0. If the stream has been neutered, then throw an InvalidAccessError exception and abort these steps.)
    if (stream->isNeutered()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, "The stream provided has been neutered.");
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
        return;
    }

    // 1. Run the prepare append algorithm.
    size_t newDataSize = m_streamMaxSizeValid ? m_streamMaxSize : 0;
    if (!prepareAppend(newDataSize, exceptionState)) {
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
        return;
    }

    // 2. Set the updating attribute to true.
    m_updating = true;

    // 3. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 4. Asynchronously run the stream append loop algorithm with stream and maxSize.
    stream->neuter();
    m_loader = FileReaderLoader::create(FileReaderLoader::ReadByClient, this);
    m_stream = stream;
    m_appendStreamAsyncPartRunner->runAsync();
}

void SourceBuffer::appendStreamAsyncPart()
{
    DCHECK(m_updating);
    DCHECK(m_loader);
    DCHECK(m_stream);
    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendStream", this, "appendStreamAsyncPart");

    // Section 3.5.6 Stream Append Loop
    // http://w3c.github.io/media-source/#sourcebuffer-stream-append-loop

    // 1. If maxSize is set, then let bytesLeft equal maxSize.
    // 2. Loop Top: If maxSize is set and bytesLeft equals 0, then jump to the loop done step below.
    if (m_streamMaxSizeValid && !m_streamMaxSize) {
        appendStreamDone(NoError);
        return;
    }

    // Steps 3-11 are handled by m_loader.
    // Note: Passing 0 here signals that maxSize was not set. (i.e. Read all the data in the stream).
    m_loader->start(getExecutionContext(), *m_stream, m_streamMaxSizeValid ? m_streamMaxSize : 0);
}

void SourceBuffer::appendStreamDone(AppendStreamDoneAction action)
{
    DCHECK(m_updating);
    DCHECK(m_loader);
    DCHECK(m_stream);

    clearAppendStreamState();

    if (action != NoError) {
        if (action == RunAppendErrorWithNoDecodeError) {
            appendError(NoDecodeError);
        } else {
            DCHECK_EQ(action, RunAppendErrorWithDecodeError);
            appendError(DecodeError);
        }

        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
        return;
    }

    // Section 3.5.6 Stream Append Loop
    // Steps 1-11 are handled by appendStreamAsyncPart(), |m_loader|, and |m_webSourceBuffer|.

    // 12. Loop Done: Set the updating attribute to false.
    m_updating = false;

    // 13. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 14. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
    SBLOG << __FUNCTION__ << " ended. this=" << this << " buffered=" << webTimeRangesToString(m_webSourceBuffer->buffered());
}

void SourceBuffer::clearAppendStreamState()
{
    m_streamMaxSizeValid = false;
    m_streamMaxSize = 0;
    m_loader.reset();
    m_stream = nullptr;
}

void SourceBuffer::appendError(AppendError err)
{
    SBLOG << __FUNCTION__ << " this=" << this << " AppendError=" << err;
    // Section 3.5.3 Append Error Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-append-error

    // 1. Run the reset parser state algorithm.
    m_webSourceBuffer->resetParserState();

    // 2. Set the updating attribute to false.
    m_updating = false;

    // 3. Queue a task to fire a simple event named error at this SourceBuffer object.
    scheduleEvent(EventTypeNames::error);

    // 4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);

    // 5. If decode error is true, then run the end of stream algorithm with the
    // error parameter set to "decode".
    if (err == DecodeError) {
        m_source->endOfStream("decode", ASSERT_NO_EXCEPTION);
    } else {
        DCHECK_EQ(err, NoDecodeError);
        // Nothing else to do in this case.
    }
}

void SourceBuffer::didStartLoading()
{
    SBLOG << __FUNCTION__ << " this=" << this;
}

void SourceBuffer::didReceiveDataForClient(const char* data, unsigned dataLength)
{
    SBLOG << __FUNCTION__ << " this=" << this << " dataLength=" << dataLength;
    DCHECK(m_updating);
    DCHECK(m_loader);

    // Section 3.5.6 Stream Append Loop
    // http://w3c.github.io/media-source/#sourcebuffer-stream-append-loop

    // 10. Run the coded frame eviction algorithm.
    if (!evictCodedFrames(dataLength)) {
        // 11. (in appendStreamDone) If the buffer full flag equals true, then run the append error algorithm with the decode error parameter set to false and abort this algorithm.
        appendStreamDone(RunAppendErrorWithNoDecodeError);
        return;
    }

    if (!m_webSourceBuffer->append(reinterpret_cast<const unsigned char*>(data), dataLength, &m_timestampOffset))
        appendStreamDone(RunAppendErrorWithDecodeError);
}

void SourceBuffer::didFinishLoading()
{
    SBLOG << __FUNCTION__ << " this=" << this;
    DCHECK(m_loader);
    appendStreamDone(NoError);
}

void SourceBuffer::didFail(FileError::ErrorCode errorCode)
{
    SBLOG << __FUNCTION__ << " this=" << this << " errorCode=" << errorCode;
    // m_loader might be already released, in case appendStream has failed due
    // to evictCodedFrames or WebSourceBuffer append failing in
    // didReceiveDataForClient. In that case appendStreamDone will be invoked
    // from there, no need to repeat it here.
    if (m_loader)
        appendStreamDone(RunAppendErrorWithNoDecodeError);
}

DEFINE_TRACE(SourceBuffer)
{
    visitor->trace(m_source);
    visitor->trace(m_trackDefaults);
    visitor->trace(m_asyncEventQueue);
    visitor->trace(m_appendBufferAsyncPartRunner);
    visitor->trace(m_removeAsyncPartRunner);
    visitor->trace(m_appendStreamAsyncPartRunner);
    visitor->trace(m_stream);
    visitor->trace(m_audioTracks);
    visitor->trace(m_videoTracks);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
