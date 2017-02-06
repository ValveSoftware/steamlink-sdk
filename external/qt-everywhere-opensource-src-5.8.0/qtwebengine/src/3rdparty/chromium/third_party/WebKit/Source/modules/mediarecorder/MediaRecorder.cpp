// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediarecorder/MediaRecorder.h"

#include "bindings/core/v8/Dictionary.h"
#include "core/events/Event.h"
#include "core/fileapi/Blob.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/EventTargetModules.h"
#include "modules/mediarecorder/BlobEvent.h"
#include "platform/ContentType.h"
#include "platform/blob/BlobData.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaStream.h"
#include "wtf/PtrUtil.h"
#include <algorithm>

namespace blink {

namespace {

// Boundaries of Opus bitrate from https://www.opus-codec.org/.
const int kSmallestPossibleOpusBitRate = 6000;
const int kLargestAutoAllocatedOpusBitRate = 128000;

// Smallest Vpx bitrate that can be requested.
const int kSmallestPossibleVpxBitRate = 100000;

String stateToString(MediaRecorder::State state)
{
    switch (state) {
    case MediaRecorder::State::Inactive:
        return "inactive";
    case MediaRecorder::State::Recording:
        return "recording";
    case MediaRecorder::State::Paused:
        return "paused";
    }

    NOTREACHED();
    return String();
}

// Allocates the requested bit rates from |bitrateOptions| into the respective
// |{audio,video}BitsPerSecond| (where a value of zero indicates Platform to use
// whatever it sees fit). If |options.bitsPerSecond()| is specified, it
// overrides any specific bitrate, and the UA is free to allocate as desired:
// here a 90%/10% video/audio is used. In all cases where a value is explicited
// or calculated, values are clamped in sane ranges.
// This method throws NotSupportedError.
void AllocateVideoAndAudioBitrates(ExceptionState& exceptionState, ExecutionContext* context, const MediaRecorderOptions& options, MediaStream* stream, int* audioBitsPerSecond, int* videoBitsPerSecond)
{
    const bool useVideo = !stream->getVideoTracks().isEmpty();
    const bool useAudio = !stream->getAudioTracks().isEmpty();

    // Clamp incoming values into a signed integer's range.
    // TODO(mcasas): This section would no be needed if the bit rates are signed or double, see https://github.com/w3c/mediacapture-record/issues/48.
    const unsigned kMaxIntAsUnsigned = std::numeric_limits<int>::max();

    int overallBps = 0;
    if (options.hasBitsPerSecond())
        overallBps = std::min(options.bitsPerSecond(), kMaxIntAsUnsigned);
    int videoBps = 0;
    if (options.hasVideoBitsPerSecond() && useVideo)
        videoBps = std::min(options.videoBitsPerSecond(), kMaxIntAsUnsigned);
    int audioBps = 0;
    if (options.hasAudioBitsPerSecond() && useAudio)
        audioBps = std::min(options.audioBitsPerSecond(), kMaxIntAsUnsigned);

    if (useAudio) {
        // |overallBps| overrides the specific audio and video bit rates.
        if (options.hasBitsPerSecond()) {
            if (useVideo)
                audioBps = overallBps / 10;
            else
                audioBps = overallBps;
        }
        // Limit audio bitrate values if set explicitly or calculated.
        if (options.hasAudioBitsPerSecond() || options.hasBitsPerSecond()) {
            if (audioBps > kLargestAutoAllocatedOpusBitRate) {
                context->addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Clamping calculated audio bitrate (" + String::number(audioBps) + "bps) to the maximum (" + String::number(kLargestAutoAllocatedOpusBitRate) + "bps)"));
                audioBps = kLargestAutoAllocatedOpusBitRate;
            }

            if (audioBps < kSmallestPossibleOpusBitRate) {
                context->addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Clamping calculated audio bitrate (" + String::number(audioBps) + "bps) to the minimum (" + String::number(kSmallestPossibleOpusBitRate) + "bps)"));
                audioBps = kSmallestPossibleOpusBitRate;
            }
        } else {
            DCHECK(!audioBps);
        }
    }

    if (useVideo) {
        // Allocate the remaining |overallBps|, if any, to video.
        if (options.hasBitsPerSecond())
            videoBps = overallBps - audioBps;
        // Clamp the video bit rate. Avoid clamping if the user has not set it explicitly.
        if (options.hasVideoBitsPerSecond() || options.hasBitsPerSecond()) {
            if (videoBps < kSmallestPossibleVpxBitRate) {
                context->addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "Clamping calculated video bitrate (" + String::number(videoBps) + "bps) to the minimum (" + String::number(kSmallestPossibleVpxBitRate) + "bps)"));
                videoBps = kSmallestPossibleVpxBitRate;
            }
        } else {
            DCHECK(!videoBps);
        }
    }

    *videoBitsPerSecond = videoBps;
    *audioBitsPerSecond = audioBps;
    return;
}

} // namespace

MediaRecorder* MediaRecorder::create(ExecutionContext* context, MediaStream* stream, ExceptionState& exceptionState)
{
    MediaRecorder* recorder = new MediaRecorder(context, stream, MediaRecorderOptions(), exceptionState);
    recorder->suspendIfNeeded();

    return recorder;
}

MediaRecorder* MediaRecorder::create(ExecutionContext* context, MediaStream* stream,  const MediaRecorderOptions& options, ExceptionState& exceptionState)
{
    MediaRecorder* recorder = new MediaRecorder(context, stream, options, exceptionState);
    recorder->suspendIfNeeded();

    return recorder;
}

MediaRecorder::MediaRecorder(ExecutionContext* context, MediaStream* stream, const MediaRecorderOptions& options, ExceptionState& exceptionState)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_stream(stream)
    , m_streamAmountOfTracks(stream->getTracks().size())
    , m_mimeType(options.mimeType())
    , m_stopped(true)
    , m_ignoreMutedMedia(true)
    , m_audioBitsPerSecond(0)
    , m_videoBitsPerSecond(0)
    , m_state(State::Inactive)
    , m_dispatchScheduledEventRunner(AsyncMethodRunner<MediaRecorder>::create(this, &MediaRecorder::dispatchScheduledEvent))
{
    DCHECK(m_stream->getTracks().size());

    m_recorderHandler = wrapUnique(Platform::current()->createMediaRecorderHandler());
    DCHECK(m_recorderHandler);

    if (!m_recorderHandler) {
        exceptionState.throwDOMException(NotSupportedError, "No MediaRecorder handler can be created.");
        return;
    }

    AllocateVideoAndAudioBitrates(exceptionState, context, options, stream, &m_audioBitsPerSecond, &m_videoBitsPerSecond);

    const ContentType contentType(m_mimeType);
    if (!m_recorderHandler->initialize(this, stream->descriptor(), contentType.type(), contentType.parameter("codecs"), m_audioBitsPerSecond, m_videoBitsPerSecond)) {
        exceptionState.throwDOMException(NotSupportedError, "Failed to initialize native MediaRecorder the type provided (" + m_mimeType + ") is not supported.");
        return;
    }
    m_stopped = false;
}

String MediaRecorder::state() const
{
    return stateToString(m_state);
}

void MediaRecorder::start(ExceptionState& exceptionState)
{
    start(0 /* timeSlice */, exceptionState);
}

void MediaRecorder::start(int timeSlice, ExceptionState& exceptionState)
{
    if (m_state != State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    m_state = State::Recording;

    if (!m_recorderHandler->start(timeSlice)) {
        exceptionState.throwDOMException(UnknownError, "The MediaRecorder failed to start because there are no audio or video tracks available.");
        return;
    }
    scheduleDispatchEvent(Event::create(EventTypeNames::start));
}

void MediaRecorder::stop(ExceptionState& exceptionState)
{
    if (m_state == State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }

    stopRecording();
}

void MediaRecorder::pause(ExceptionState& exceptionState)
{
    if (m_state == State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    if (m_state == State::Paused)
        return;

    m_state = State::Paused;

    m_recorderHandler->pause();

    scheduleDispatchEvent(Event::create(EventTypeNames::pause));
}

void MediaRecorder::resume(ExceptionState& exceptionState)
{
    if (m_state == State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    if (m_state == State::Recording)
        return;

    m_state = State::Recording;

    m_recorderHandler->resume();
    scheduleDispatchEvent(Event::create(EventTypeNames::resume));
}

void MediaRecorder::requestData(ExceptionState& exceptionState)
{
    if (m_state != State::Recording) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    writeData(nullptr /* data */, 0 /* length */, true /* lastInSlice */);
}

bool MediaRecorder::isTypeSupported(const String& type)
{
    WebMediaRecorderHandler* handler = Platform::current()->createMediaRecorderHandler();
    if (!handler)
        return false;

    // If true is returned from this method, it only indicates that the
    // MediaRecorder implementation is capable of recording Blob objects for the
    // specified MIME type. Recording may still fail if sufficient resources are
    // not available to support the concrete media encoding.
    // [1] https://w3c.github.io/mediacapture-record/MediaRecorder.html#methods
    ContentType contentType(type);
    return handler->canSupportMimeType(contentType.type(), contentType.parameter("codecs"));
}

const AtomicString& MediaRecorder::interfaceName() const
{
    return EventTargetNames::MediaRecorder;
}

ExecutionContext* MediaRecorder::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

void MediaRecorder::suspend()
{
    m_dispatchScheduledEventRunner->suspend();
}

void MediaRecorder::resume()
{
    m_dispatchScheduledEventRunner->resume();
}

void MediaRecorder::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_stream.clear();
    m_recorderHandler.reset();
}

void MediaRecorder::writeData(const char* data, size_t length, bool lastInSlice)
{
    if (m_stopped && !lastInSlice) {
        m_stopped = false;
        scheduleDispatchEvent(Event::create(EventTypeNames::start));
    }
    if (m_stream && m_streamAmountOfTracks != m_stream->getTracks().size()) {
        m_streamAmountOfTracks = m_stream->getTracks().size();
        onError("Amount of tracks in MediaStream has changed.");
    }

    // TODO(mcasas): Act as |m_ignoredMutedMedia| instructs if |m_stream| track(s) is in muted() state.

    if (!m_blobData)
        m_blobData = BlobData::create();
    if (data)
        m_blobData->appendBytes(data, length);

    if (!lastInSlice)
        return;

    // Cache |m_blobData->length()| before release()ng it.
    const long long blobDataLength = m_blobData->length();
    createBlobEvent(Blob::create(BlobDataHandle::create(std::move(m_blobData), blobDataLength)));
}

void MediaRecorder::onError(const WebString& message)
{
    // TODO(mcasas): Beef up the Error Event and add the |message|, see https://github.com/w3c/mediacapture-record/issues/31
    scheduleDispatchEvent(Event::create(EventTypeNames::error));
}

void MediaRecorder::createBlobEvent(Blob* blob)
{
    // TODO(mcasas): Consider launching an Event with a TypedArray inside, see https://github.com/w3c/mediacapture-record/issues/17.
    scheduleDispatchEvent(BlobEvent::create(EventTypeNames::dataavailable, blob));
}

void MediaRecorder::stopRecording()
{
    DCHECK(m_state != State::Inactive);
    m_state = State::Inactive;

    m_recorderHandler->stop();

    writeData(nullptr /* data */, 0 /* length */, true /* lastInSlice */);
    scheduleDispatchEvent(Event::create(EventTypeNames::stop));
}

void MediaRecorder::scheduleDispatchEvent(Event* event)
{
    m_scheduledEvents.append(event);

    m_dispatchScheduledEventRunner->runAsync();
}

void MediaRecorder::dispatchScheduledEvent()
{
    HeapVector<Member<Event>> events;
    events.swap(m_scheduledEvents);

    for (const auto& event : events)
        dispatchEvent(event);
}

DEFINE_TRACE(MediaRecorder)
{
    visitor->trace(m_stream);
    visitor->trace(m_dispatchScheduledEventRunner);
    visitor->trace(m_scheduledEvents);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
