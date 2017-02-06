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

#include "modules/mediasource/MediaSource.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/mediasource/MediaSourceRegistry.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "public/platform/WebMediaSource.h"
#include "public/platform/WebSourceBuffer.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include <memory>

using blink::WebMediaSource;
using blink::WebSourceBuffer;

#define MSLOG DVLOG(3)

namespace blink {

static bool throwExceptionIfClosed(bool isOpen, ExceptionState& exceptionState)
{
    if (!isOpen) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The MediaSource's readyState is not 'open'.");
        return true;
    }

    return false;
}

static bool throwExceptionIfClosedOrUpdating(bool isOpen, bool isUpdating, ExceptionState& exceptionState)
{
    if (throwExceptionIfClosed(isOpen, exceptionState))
        return true;

    if (isUpdating) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The 'updating' attribute is true on one or more of this MediaSource's SourceBuffers.");
        return true;
    }

    return false;
}

const AtomicString& MediaSource::openKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, open, ("open"));
    return open;
}

const AtomicString& MediaSource::closedKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, closed, ("closed"));
    return closed;
}

const AtomicString& MediaSource::endedKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, ended, ("ended"));
    return ended;
}

MediaSource* MediaSource::create(ExecutionContext* context)
{
    MediaSource* mediaSource = new MediaSource(context);
    mediaSource->suspendIfNeeded();
    return mediaSource;
}

MediaSource::MediaSource(ExecutionContext* context)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_readyState(closedKeyword())
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_attachedElement(nullptr)
    , m_sourceBuffers(SourceBufferList::create(getExecutionContext(), m_asyncEventQueue.get()))
    , m_activeSourceBuffers(SourceBufferList::create(getExecutionContext(), m_asyncEventQueue.get()))
    , m_liveSeekableRange(TimeRanges::create())
    , m_isAddedToRegistry(false)
{
    MSLOG << __FUNCTION__ << " this=" << this;
}

MediaSource::~MediaSource()
{
    MSLOG << __FUNCTION__ << " this=" << this;
    DCHECK(isClosed());
}

void MediaSource::logAndThrowDOMException(ExceptionState& exceptionState, const ExceptionCode& error, const String& message)
{
    MSLOG << __FUNCTION__ << " (error=" << error << ", message=" << message << ")";
    exceptionState.throwDOMException(error, message);
}

SourceBuffer* MediaSource::addSourceBuffer(const String& type, ExceptionState& exceptionState)
{
    MSLOG << __FUNCTION__ << " this=" << this << " type=" << type;

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
    // 1. If type is an empty string then throw an InvalidAccessError exception
    // and abort these steps.
    if (type.isEmpty()) {
        logAndThrowDOMException(exceptionState, InvalidAccessError, "The type provided is empty.");
        return 0;
    }

    // 2. If type contains a MIME type that is not supported ..., then throw a
    // NotSupportedError exception and abort these steps.
    if (!isTypeSupported(type)) {
        logAndThrowDOMException(exceptionState, NotSupportedError, "The type provided ('" + type + "') is unsupported.");
        return 0;
    }

    // 4. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    if (!isOpen()) {
        logAndThrowDOMException(exceptionState, InvalidStateError, "The MediaSource's readyState is not 'open'.");
        return 0;
    }

    // 5. Create a new SourceBuffer object and associated resources.
    ContentType contentType(type);
    String codecs = contentType.parameter("codecs");
    std::unique_ptr<WebSourceBuffer> webSourceBuffer = createWebSourceBuffer(contentType.type(), codecs, exceptionState);

    if (!webSourceBuffer) {
        DCHECK(exceptionState.code() == NotSupportedError || exceptionState.code() == QuotaExceededError);
        // 2. If type contains a MIME type that is not supported ..., then throw a NotSupportedError exception and abort these steps.
        // 3. If the user agent can't handle any more SourceBuffer objects then throw a QuotaExceededError exception and abort these steps
        return 0;
    }

    SourceBuffer* buffer = SourceBuffer::create(std::move(webSourceBuffer), this, m_asyncEventQueue.get());
    // 6. Add the new object to sourceBuffers and fire a addsourcebuffer on that object.
    m_sourceBuffers->add(buffer);

    // 7. Return the new object to the caller.
    MSLOG << __FUNCTION__ << " this=" << this << " type=" << type << " -> " << buffer;
    return buffer;
}

void MediaSource::removeSourceBuffer(SourceBuffer* buffer, ExceptionState& exceptionState)
{
    MSLOG << __FUNCTION__ << " this=" << this << " buffer=" << buffer;

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-removeSourceBuffer-void-SourceBuffer-sourceBuffer

    // 1. If sourceBuffer specifies an object that is not in sourceBuffers then
    // throw a NotFoundError exception and abort these steps.
    if (!m_sourceBuffers->length() || !m_sourceBuffers->contains(buffer)) {
        logAndThrowDOMException(exceptionState, NotFoundError, "The SourceBuffer provided is not contained in this MediaSource.");
        return;
    }

    // Steps 2-8 are implemented by SourceBuffer::removedFromMediaSource.
    buffer->removedFromMediaSource();

    // 9. If sourceBuffer is in activeSourceBuffers, then remove sourceBuffer from activeSourceBuffers ...
    m_activeSourceBuffers->remove(buffer);

    // 10. Remove sourceBuffer from sourceBuffers and fire a removesourcebuffer event
    // on that object.
    m_sourceBuffers->remove(buffer);

    // 11. Destroy all resources for sourceBuffer.
    // This should have been done already by SourceBuffer::removedFromMediaSource (steps 2-8) above.
}

void MediaSource::onReadyStateChange(const AtomicString& oldState, const AtomicString& newState)
{
    if (isOpen()) {
        scheduleEvent(EventTypeNames::sourceopen);
        return;
    }

    if (oldState == openKeyword() && newState == endedKeyword()) {
        scheduleEvent(EventTypeNames::sourceended);
        return;
    }

    DCHECK(isClosed());

    m_activeSourceBuffers->clear();

    // Clear SourceBuffer references to this object.
    for (unsigned long i = 0; i < m_sourceBuffers->length(); ++i)
        m_sourceBuffers->item(i)->removedFromMediaSource();
    m_sourceBuffers->clear();

    m_attachedElement.clear();

    scheduleEvent(EventTypeNames::sourceclose);
}

bool MediaSource::isUpdating() const
{
    // Return true if any member of |m_sourceBuffers| is updating.
    for (unsigned long i = 0; i < m_sourceBuffers->length(); ++i) {
        if (m_sourceBuffers->item(i)->updating())
            return true;
    }

    return false;
}

bool MediaSource::isTypeSupported(const String& type)
{
    // Section 2.2 isTypeSupported() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-isTypeSupported-boolean-DOMString-type
    // 1. If type is an empty string, then return false.
    if (type.isEmpty()) {
        MSLOG << __FUNCTION__ << "(" << type << ") -> false (empty input)";
        return false;
    }

    ContentType contentType(type);
    String codecs = contentType.parameter("codecs");

    // 2. If type does not contain a valid MIME type string, then return false.
    if (contentType.type().isEmpty()) {
        MSLOG << __FUNCTION__ << "(" << type << ") -> false (invalid mime type)";
        return false;
    }

    // Note: MediaSource.isTypeSupported() returning true implies that HTMLMediaElement.canPlayType() will return "maybe" or "probably"
    // since it does not make sense for a MediaSource to support a type the HTMLMediaElement knows it cannot play.
    if (HTMLMediaElement::supportsType(contentType) == WebMimeRegistry::IsNotSupported) {
        MSLOG << __FUNCTION__ << "(" << type << ") -> false (not supported by HTMLMediaElement)";
        return false;
    }

    // 3. If type contains a media type or media subtype that the MediaSource does not support, then return false.
    // 4. If type contains at a codec that the MediaSource does not support, then return false.
    // 5. If the MediaSource does not support the specified combination of media type, media subtype, and codecs then return false.
    // 6. Return true.
    bool result = MIMETypeRegistry::isSupportedMediaSourceMIMEType(contentType.type(), codecs);
    MSLOG << __FUNCTION__ << "(" << type << ") -> " << (result ? "true" : "false");
    return result;
}

const AtomicString& MediaSource::interfaceName() const
{
    return EventTargetNames::MediaSource;
}

ExecutionContext* MediaSource::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

DEFINE_TRACE(MediaSource)
{
    visitor->trace(m_asyncEventQueue);
    visitor->trace(m_attachedElement);
    visitor->trace(m_sourceBuffers);
    visitor->trace(m_activeSourceBuffers);
    visitor->trace(m_liveSeekableRange);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

void MediaSource::setWebMediaSourceAndOpen(std::unique_ptr<WebMediaSource> webMediaSource)
{
    TRACE_EVENT_ASYNC_END0("media", "MediaSource::attachToElement", this);
    DCHECK(webMediaSource);
    DCHECK(!m_webMediaSource);
    DCHECK(m_attachedElement);
    m_webMediaSource = std::move(webMediaSource);
    setReadyState(openKeyword());
}

void MediaSource::addedToRegistry()
{
    DCHECK(!m_isAddedToRegistry);
    m_isAddedToRegistry = true;
}

void MediaSource::removedFromRegistry()
{
    DCHECK(m_isAddedToRegistry);
    m_isAddedToRegistry = false;
}

double MediaSource::duration() const
{
    return isClosed() ? std::numeric_limits<float>::quiet_NaN() : m_webMediaSource->duration();
}

TimeRanges* MediaSource::buffered() const
{
    // Implements MediaSource algorithm for HTMLMediaElement.buffered.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
    HeapVector<Member<TimeRanges>> ranges(m_activeSourceBuffers->length());
    for (size_t i = 0; i < m_activeSourceBuffers->length(); ++i)
        ranges[i] = m_activeSourceBuffers->item(i)->buffered(ASSERT_NO_EXCEPTION);

    // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges object and abort these steps.
    if (ranges.isEmpty())
        return TimeRanges::create();

    // 2. Let active ranges be the ranges returned by buffered for each SourceBuffer object in activeSourceBuffers.
    // 3. Let highest end time be the largest range end time in the active ranges.
    double highestEndTime = -1;
    for (size_t i = 0; i < ranges.size(); ++i) {
        unsigned length = ranges[i]->length();
        if (length)
            highestEndTime = std::max(highestEndTime, ranges[i]->end(length - 1, ASSERT_NO_EXCEPTION));
    }

    // Return an empty range if all ranges are empty.
    if (highestEndTime < 0)
        return TimeRanges::create();

    // 4. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
    TimeRanges* intersectionRanges = TimeRanges::create(0, highestEndTime);

    // 5. For each SourceBuffer object in activeSourceBuffers run the following steps:
    bool ended = readyState() == endedKeyword();
    for (size_t i = 0; i < ranges.size(); ++i) {
        // 5.1 Let source ranges equal the ranges returned by the buffered attribute on the current SourceBuffer.
        TimeRanges* sourceRanges = ranges[i].get();

        // 5.2 If readyState is "ended", then set the end time on the last range in source ranges to highest end time.
        if (ended && sourceRanges->length())
            sourceRanges->add(sourceRanges->start(sourceRanges->length() - 1, ASSERT_NO_EXCEPTION), highestEndTime);

        // 5.3 Let new intersection ranges equal the the intersection between the intersection ranges and the source ranges.
        // 5.4 Replace the ranges in intersection ranges with the new intersection ranges.
        intersectionRanges->intersectWith(sourceRanges);
    }

    return intersectionRanges;
}

TimeRanges* MediaSource::seekable() const
{
    // Implements MediaSource algorithm for HTMLMediaElement.seekable.
    // http://w3c.github.io/media-source/#htmlmediaelement-extensions

    double sourceDuration = duration();
    // If duration equals NaN: Return an empty TimeRanges object.
    if (std::isnan(sourceDuration))
        return TimeRanges::create();

    // If duration equals positive Infinity:
    if (sourceDuration == std::numeric_limits<double>::infinity()) {
        TimeRanges* buffered = m_attachedElement->buffered();

        // 1. If live seekable range is not empty:
        if (m_liveSeekableRange->length() != 0) {
            // 1.1. Let union ranges be the union of live seekable range and the
            //      HTMLMediaElement.buffered attribute.
            // 1.2. Return a single range with a start time equal to the
            //      earliest start time in union ranges and an end time equal to
            //      the highest end time in union ranges and abort these steps.
            if (buffered->length() == 0) {
                return TimeRanges::create(m_liveSeekableRange->start(0, ASSERT_NO_EXCEPTION), m_liveSeekableRange->end(0, ASSERT_NO_EXCEPTION));
            }

            return TimeRanges::create(std::min(m_liveSeekableRange->start(0, ASSERT_NO_EXCEPTION), buffered->start(0, ASSERT_NO_EXCEPTION)),
                std::max(m_liveSeekableRange->end(0, ASSERT_NO_EXCEPTION), buffered->end(buffered->length() - 1, ASSERT_NO_EXCEPTION)));
        }
        // 2. If the HTMLMediaElement.buffered attribute returns an empty TimeRanges object, then
        //    return an empty TimeRanges object and abort these steps.
        if (buffered->length() == 0)
            return TimeRanges::create();

        // 3. Return a single range with a start time of 0 and an end time equal to the highest end
        //    time reported by the HTMLMediaElement.buffered attribute.
        return TimeRanges::create(0, buffered->end(buffered->length() - 1, ASSERT_NO_EXCEPTION));
    }

    // 3. Otherwise: Return a single range with a start time of 0 and an end time equal to duration.
    return TimeRanges::create(0, sourceDuration);
}

void MediaSource::setDuration(double duration, ExceptionState& exceptionState)
{
    // 2.1 http://www.w3.org/TR/media-source/#widl-MediaSource-duration
    // 1. If the value being set is negative or NaN then throw an InvalidAccessError
    // exception and abort these steps.
    if (std::isnan(duration)) {
        logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::notAFiniteNumber(duration, "duration"));
        return;
    }
    if (duration < 0.0) {
        logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::indexExceedsMinimumBound("duration", duration, 0.0));
        return;
    }

    // 2. If the readyState attribute is not "open" then throw an InvalidStateError
    // exception and abort these steps.
    // 3. If the updating attribute equals true on any SourceBuffer in sourceBuffers,
    // then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfClosedOrUpdating(isOpen(), isUpdating(), exceptionState))
        return;

    // 4. Run the duration change algorithm with new duration set to the value being
    // assigned to this attribute.
    durationChangeAlgorithm(duration, exceptionState);
}

void MediaSource::durationChangeAlgorithm(double newDuration, ExceptionState& exceptionState)
{
    // http://w3c.github.io/media-source/#duration-change-algorithm
    // 1. If the current value of duration is equal to new duration, then return.
    if (newDuration == duration())
        return;

    // 2. If new duration is less than the highest starting presentation
    // timestamp of any buffered coded frames for all SourceBuffer objects in
    // sourceBuffers, then throw an InvalidStateError exception and abort these
    // steps. Note: duration reductions that would truncate currently buffered
    // media are disallowed. When truncation is necessary, use remove() to
    // reduce the buffered range before updating duration.
    double highestBufferedPresentationTimestamp = 0;
    for (size_t i = 0; i < m_sourceBuffers->length(); ++i) {
        highestBufferedPresentationTimestamp = std::max(highestBufferedPresentationTimestamp, m_sourceBuffers->item(i)->highestPresentationTimestamp());
    }

    if (newDuration < highestBufferedPresentationTimestamp) {
        if (RuntimeEnabledFeatures::mediaSourceNewAbortAndDurationEnabled()) {
            logAndThrowDOMException(exceptionState, InvalidStateError, "Setting duration below highest presentation timestamp of any buffered coded frames is disallowed. Instead, first do asynchronous remove(newDuration, oldDuration) on all sourceBuffers, where newDuration < oldDuration.");
            return;
        }

        Deprecation::countDeprecation(m_attachedElement->document(), UseCounter::MediaSourceDurationTruncatingBuffered);
        // See also deprecated remove(new duration, old duration) behavior below.
    }

    // 3. Set old duration to the current value of duration.
    double oldDuration = duration();
    DCHECK_LE(highestBufferedPresentationTimestamp, std::isnan(oldDuration) ? 0 : oldDuration);

    // 4. Update duration to new duration.
    bool requestSeek = m_attachedElement->currentTime() > newDuration;
    m_webMediaSource->setDuration(newDuration);

    if (!RuntimeEnabledFeatures::mediaSourceNewAbortAndDurationEnabled() && newDuration < oldDuration) {
        // Deprecated behavior: if the new duration is less than old duration,
        // then call remove(new duration, old duration) on all all objects in
        // sourceBuffers.
        for (size_t i = 0; i < m_sourceBuffers->length(); ++i)
            m_sourceBuffers->item(i)->remove(newDuration, oldDuration, ASSERT_NO_EXCEPTION);
    }

    // 5. If a user agent is unable to partially render audio frames or text cues that start before and end after the duration, then run the following steps:
    // NOTE: Currently we assume that the media engine is able to render partial frames/cues. If a media
    // engine gets added that doesn't support this, then we'll need to add logic to handle the substeps.

    // 6. Update the media controller duration to new duration and run the HTMLMediaElement duration change algorithm.
    m_attachedElement->durationChanged(newDuration, requestSeek);
}

void MediaSource::setReadyState(const AtomicString& state)
{
    DCHECK(state == openKeyword() || state == closedKeyword() || state == endedKeyword());

    AtomicString oldState = readyState();
    MSLOG << __FUNCTION__ << " this=" << this << " : " << oldState << " -> " << state;

    if (state == closedKeyword()) {
        m_webMediaSource.reset();
    }

    if (oldState == state)
        return;

    m_readyState = state;

    onReadyStateChange(oldState, state);
}

void MediaSource::endOfStream(const AtomicString& error, ExceptionState& exceptionState)
{
    DEFINE_STATIC_LOCAL(const AtomicString, network, ("network"));
    DEFINE_STATIC_LOCAL(const AtomicString, decode, ("decode"));

    if (error == network) {
        endOfStreamInternal(WebMediaSource::EndOfStreamStatusNetworkError, exceptionState);
    } else if (error == decode) {
        endOfStreamInternal(WebMediaSource::EndOfStreamStatusDecodeError, exceptionState);
    } else {
        NOTREACHED(); // IDL enforcement should prevent this case.
    }
}

void MediaSource::endOfStream(ExceptionState& exceptionState)
{
    endOfStreamInternal(WebMediaSource::EndOfStreamStatusNoError, exceptionState);
}

void MediaSource::setLiveSeekableRange(double start, double end, ExceptionState& exceptionState)
{
    // http://w3c.github.io/media-source/#widl-MediaSource-setLiveSeekableRange-void-double-start-double-end
    // 1. If the readyState attribute is not "open" then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true on any SourceBuffer in
    //    SourceBuffers, then throw an InvalidStateError exception and abort
    //    these steps.
    //    Note: https://github.com/w3c/media-source/issues/118, once fixed, will
    //    remove the updating check (step 2). We skip that check here already.
    if (throwExceptionIfClosed(isOpen(), exceptionState))
        return;

    // 3. If start is negative or greater than end, then throw a TypeError
    //    exception and abort these steps.
    if (start < 0 || start > end) {
        exceptionState.throwTypeError(ExceptionMessages::indexOutsideRange("start value", start, 0.0, ExceptionMessages::InclusiveBound, end, ExceptionMessages::InclusiveBound));
        return;
    }

    // 4. Set live seekable range to be a new normalized TimeRanges object
    //    containing a single range whose start position is start and end
    //    position is end.
    m_liveSeekableRange = TimeRanges::create(start, end);
}

void MediaSource::clearLiveSeekableRange(ExceptionState& exceptionState)
{
    // http://w3c.github.io/media-source/#widl-MediaSource-clearLiveSeekableRange-void
    // 1. If the readyState attribute is not "open" then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true on any SourceBuffer in
    //    SourceBuffers, then throw an InvalidStateError exception and abort
    //    these steps.
    //    Note: https://github.com/w3c/media-source/issues/118, once fixed, will
    //    remove the updating check (step 2). We skip that check here already.
    if (throwExceptionIfClosed(isOpen(), exceptionState))
        return;

    // 3. If live seekable range contains a range, then set live seekable range
    //    to be a new empty TimeRanges object.
    if (m_liveSeekableRange->length() != 0)
        m_liveSeekableRange = TimeRanges::create();
}

void MediaSource::endOfStreamInternal(const WebMediaSource::EndOfStreamStatus eosStatus, ExceptionState& exceptionState)
{
    // 2.2 http://www.w3.org/TR/media-source/#widl-MediaSource-endOfStream-void-EndOfStreamError-error
    // 1. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true on any SourceBuffer in sourceBuffers, then throw an
    // InvalidStateError exception and abort these steps.
    if (throwExceptionIfClosedOrUpdating(isOpen(), isUpdating(), exceptionState))
        return;

    // 3. Run the end of stream algorithm with the error parameter set to error.
    //   1. Change the readyState attribute value to "ended".
    //   2. Queue a task to fire a simple event named sourceended at the MediaSource.
    setReadyState(endedKeyword());

    //   3. Do various steps based on |eosStatus|.
    m_webMediaSource->markEndOfStream(eosStatus);
}

bool MediaSource::isOpen() const
{
    return readyState() == openKeyword();
}

void MediaSource::setSourceBufferActive(SourceBuffer* sourceBuffer)
{
    DCHECK(!m_activeSourceBuffers->contains(sourceBuffer));

    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-activeSourceBuffers
    // SourceBuffer objects in SourceBuffer.activeSourceBuffers must appear in
    // the same order as they appear in SourceBuffer.sourceBuffers.
    // SourceBuffer transitions to active are not guaranteed to occur in the
    // same order as buffers in |m_sourceBuffers|, so this method needs to
    // insert |sourceBuffer| into |m_activeSourceBuffers|.
    size_t indexInSourceBuffers = m_sourceBuffers->find(sourceBuffer);
    DCHECK(indexInSourceBuffers != kNotFound);

    size_t insertPosition = 0;
    while (insertPosition < m_activeSourceBuffers->length()
        && m_sourceBuffers->find(m_activeSourceBuffers->item(insertPosition)) < indexInSourceBuffers) {
        ++insertPosition;
    }

    m_activeSourceBuffers->insert(insertPosition, sourceBuffer);
}

HTMLMediaElement* MediaSource::mediaElement() const
{
    return m_attachedElement.get();
}

bool MediaSource::isClosed() const
{
    return readyState() == closedKeyword();
}

void MediaSource::close()
{
    setReadyState(closedKeyword());
}

bool MediaSource::attachToElement(HTMLMediaElement* element)
{
    if (m_attachedElement)
        return false;

    DCHECK(isClosed());

    TRACE_EVENT_ASYNC_BEGIN0("media", "MediaSource::attachToElement", this);
    m_attachedElement = element;
    return true;
}

void MediaSource::openIfInEndedState()
{
    if (m_readyState != endedKeyword())
        return;

    setReadyState(openKeyword());
    m_webMediaSource->unmarkEndOfStream();
}

bool MediaSource::hasPendingActivity() const
{
    return m_attachedElement || m_webMediaSource
        || m_asyncEventQueue->hasPendingEvents()
        || m_isAddedToRegistry;
}

void MediaSource::stop()
{
    m_asyncEventQueue->close();
    if (!isClosed())
        setReadyState(closedKeyword());
    m_webMediaSource.reset();
}

std::unique_ptr<WebSourceBuffer> MediaSource::createWebSourceBuffer(const String& type, const String& codecs, ExceptionState& exceptionState)
{
    WebSourceBuffer* webSourceBuffer = 0;

    switch (m_webMediaSource->addSourceBuffer(type, codecs, &webSourceBuffer)) {
    case WebMediaSource::AddStatusOk:
        return wrapUnique(webSourceBuffer);
    case WebMediaSource::AddStatusNotSupported:
        DCHECK(!webSourceBuffer);
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 2: If type contains a MIME type ... that is not supported with the types
        // specified for the other SourceBuffer objects in sourceBuffers, then throw
        // a NotSupportedError exception and abort these steps.
        logAndThrowDOMException(exceptionState, NotSupportedError, "The type provided ('" + type + "') is not supported.");
        return nullptr;
    case WebMediaSource::AddStatusReachedIdLimit:
        DCHECK(!webSourceBuffer);
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 3: If the user agent can't handle any more SourceBuffer objects then throw
        // a QuotaExceededError exception and abort these steps.
        logAndThrowDOMException(exceptionState, QuotaExceededError, "This MediaSource has reached the limit of SourceBuffer objects it can handle. No additional SourceBuffer objects may be added.");
        return nullptr;
    }

    NOTREACHED();
    return nullptr;
}

void MediaSource::scheduleEvent(const AtomicString& eventName)
{
    DCHECK(m_asyncEventQueue);

    Event* event = Event::create(eventName);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event);
}

URLRegistry& MediaSource::registry() const
{
    return MediaSourceRegistry::registry();
}

} // namespace blink
