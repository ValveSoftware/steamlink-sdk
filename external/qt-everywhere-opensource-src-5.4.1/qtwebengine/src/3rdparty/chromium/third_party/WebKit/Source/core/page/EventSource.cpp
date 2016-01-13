/*
 * Copyright (C) 2009, 2012 Ericsson AB. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, Code Aurora Forum. All rights reserved.
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

#include "config.h"
#include "core/page/EventSource.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/ThreadableLoader.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

const unsigned long long EventSource::defaultReconnectDelay = 3000;

inline EventSource::EventSource(ExecutionContext* context, const KURL& url, const Dictionary& eventSourceInit)
    : ActiveDOMObject(context)
    , m_url(url)
    , m_withCredentials(false)
    , m_state(CONNECTING)
    , m_decoder(TextResourceDecoder::create("text/plain", "UTF-8"))
    , m_connectTimer(this, &EventSource::connectTimerFired)
    , m_discardTrailingNewline(false)
    , m_requestInFlight(false)
    , m_reconnectDelay(defaultReconnectDelay)
{
    ScriptWrappable::init(this);
    eventSourceInit.get("withCredentials", m_withCredentials);
}

PassRefPtrWillBeRawPtr<EventSource> EventSource::create(ExecutionContext* context, const String& url, const Dictionary& eventSourceInit, ExceptionState& exceptionState)
{
    if (url.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "Cannot open an EventSource to an empty URL.");
        return nullptr;
    }

    KURL fullURL = context->completeURL(url);
    if (!fullURL.isValid()) {
        exceptionState.throwDOMException(SyntaxError, "Cannot open an EventSource to '" + url + "'. The URL is invalid.");
        return nullptr;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldContentSecurityPolicy = false;
    if (context->isDocument()) {
        Document* document = toDocument(context);
        shouldBypassMainWorldContentSecurityPolicy = document->frame()->script().shouldBypassMainWorldContentSecurityPolicy();
    }
    if (!shouldBypassMainWorldContentSecurityPolicy && !context->contentSecurityPolicy()->allowConnectToSource(fullURL)) {
        // We can safely expose the URL to JavaScript, as this exception is generate synchronously before any redirects take place.
        exceptionState.throwSecurityError("Refused to connect to '" + fullURL.elidedString() + "' because it violates the document's Content Security Policy.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<EventSource> source = adoptRefWillBeRefCountedGarbageCollected(new EventSource(context, fullURL, eventSourceInit));

    source->setPendingActivity(source.get());
    source->scheduleInitialConnect();
    source->suspendIfNeeded();

    return source.release();
}

EventSource::~EventSource()
{
    ASSERT(m_state == CLOSED);
    ASSERT(!m_requestInFlight);
}

void EventSource::scheduleInitialConnect()
{
    ASSERT(m_state == CONNECTING);
    ASSERT(!m_requestInFlight);

    m_connectTimer.startOneShot(0, FROM_HERE);
}

void EventSource::connect()
{
    ASSERT(m_state == CONNECTING);
    ASSERT(!m_requestInFlight);
    ASSERT(executionContext());

    ExecutionContext& executionContext = *this->executionContext();
    ResourceRequest request(m_url);
    request.setHTTPMethod("GET");
    request.setHTTPHeaderField("Accept", "text/event-stream");
    request.setHTTPHeaderField("Cache-Control", "no-cache");
    if (!m_lastEventId.isEmpty())
        request.setHTTPHeaderField("Last-Event-ID", m_lastEventId);

    SecurityOrigin* origin = executionContext.securityOrigin();

    ThreadableLoaderOptions options;
    options.preflightPolicy = PreventPreflight;
    options.crossOriginRequestPolicy = UseAccessControl;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicy::shouldBypassMainWorld(&executionContext) ? DoNotEnforceContentSecurityPolicy : EnforceConnectSrcDirective;

    ResourceLoaderOptions resourceLoaderOptions;
    resourceLoaderOptions.allowCredentials = (origin->canRequest(m_url) || m_withCredentials) ? AllowStoredCredentials : DoNotAllowStoredCredentials;
    resourceLoaderOptions.credentialsRequested = m_withCredentials ? ClientRequestedCredentials : ClientDidNotRequestCredentials;
    resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
    resourceLoaderOptions.securityOrigin = origin;

    m_loader = ThreadableLoader::create(executionContext, this, request, options, resourceLoaderOptions);

    if (m_loader)
        m_requestInFlight = true;
}

void EventSource::networkRequestEnded()
{
    if (!m_requestInFlight)
        return;

    m_requestInFlight = false;

    if (m_state != CLOSED)
        scheduleReconnect();
    else
        unsetPendingActivity(this);
}

void EventSource::scheduleReconnect()
{
    m_state = CONNECTING;
    m_connectTimer.startOneShot(m_reconnectDelay / 1000.0, FROM_HERE);
    dispatchEvent(Event::create(EventTypeNames::error));
}

void EventSource::connectTimerFired(Timer<EventSource>*)
{
    connect();
}

String EventSource::url() const
{
    return m_url.string();
}

bool EventSource::withCredentials() const
{
    return m_withCredentials;
}

EventSource::State EventSource::readyState() const
{
    return m_state;
}

void EventSource::close()
{
    if (m_state == CLOSED) {
        ASSERT(!m_requestInFlight);
        return;
    }

    // Stop trying to reconnect if EventSource was explicitly closed or if ActiveDOMObject::stop() was called.
    if (m_connectTimer.isActive()) {
        m_connectTimer.stop();
        unsetPendingActivity(this);
    }

    if (m_requestInFlight)
        m_loader->cancel();

    m_state = CLOSED;
}

const AtomicString& EventSource::interfaceName() const
{
    return EventTargetNames::EventSource;
}

ExecutionContext* EventSource::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void EventSource::didReceiveResponse(unsigned long, const ResourceResponse& response)
{
    ASSERT(m_state == CONNECTING);
    ASSERT(m_requestInFlight);

    m_eventStreamOrigin = SecurityOrigin::create(response.url())->toString();
    int statusCode = response.httpStatusCode();
    bool mimeTypeIsValid = response.mimeType() == "text/event-stream";
    bool responseIsValid = statusCode == 200 && mimeTypeIsValid;
    if (responseIsValid) {
        const String& charset = response.textEncodingName();
        // If we have a charset, the only allowed value is UTF-8 (case-insensitive).
        responseIsValid = charset.isEmpty() || equalIgnoringCase(charset, "UTF-8");
        if (!responseIsValid) {
            StringBuilder message;
            message.appendLiteral("EventSource's response has a charset (\"");
            message.append(charset);
            message.appendLiteral("\") that is not UTF-8. Aborting the connection.");
            // FIXME: We are missing the source line.
            executionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message.toString());
        }
    } else {
        // To keep the signal-to-noise ratio low, we only log 200-response with an invalid MIME type.
        if (statusCode == 200 && !mimeTypeIsValid) {
            StringBuilder message;
            message.appendLiteral("EventSource's response has a MIME type (\"");
            message.append(response.mimeType());
            message.appendLiteral("\") that is not \"text/event-stream\". Aborting the connection.");
            // FIXME: We are missing the source line.
            executionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message.toString());
        }
    }

    if (responseIsValid) {
        m_state = OPEN;
        dispatchEvent(Event::create(EventTypeNames::open));
    } else {
        m_loader->cancel();
        dispatchEvent(Event::create(EventTypeNames::error));
    }
}

void EventSource::didReceiveData(const char* data, int length)
{
    ASSERT(m_state == OPEN);
    ASSERT(m_requestInFlight);

    append(m_receiveBuf, m_decoder->decode(data, length));
    parseEventStream();
}

void EventSource::didFinishLoading(unsigned long, double)
{
    ASSERT(m_state == OPEN);
    ASSERT(m_requestInFlight);

    if (m_receiveBuf.size() > 0 || m_data.size() > 0) {
        parseEventStream();

        // Discard everything that has not been dispatched by now.
        m_receiveBuf.clear();
        m_data.clear();
        m_eventName = emptyAtom;
        m_currentlyParsedEventId = nullAtom;
    }
    networkRequestEnded();
}

void EventSource::didFail(const ResourceError& error)
{
    ASSERT(m_state != CLOSED);
    ASSERT(m_requestInFlight);

    if (error.isCancellation())
        m_state = CLOSED;
    networkRequestEnded();
}

void EventSource::didFailAccessControlCheck(const ResourceError& error)
{
    String message = "EventSource cannot load " + error.failingURL() + ". " + error.localizedDescription();
    executionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message);

    abortConnectionAttempt();
}

void EventSource::didFailRedirectCheck()
{
    abortConnectionAttempt();
}

void EventSource::abortConnectionAttempt()
{
    ASSERT(m_state == CONNECTING);

    if (m_requestInFlight) {
        m_loader->cancel();
    } else {
        m_state = CLOSED;
        unsetPendingActivity(this);
    }

    ASSERT(m_state == CLOSED);
    dispatchEvent(Event::create(EventTypeNames::error));
}

void EventSource::parseEventStream()
{
    unsigned bufPos = 0;
    unsigned bufSize = m_receiveBuf.size();
    while (bufPos < bufSize) {
        if (m_discardTrailingNewline) {
            if (m_receiveBuf[bufPos] == '\n')
                bufPos++;
            m_discardTrailingNewline = false;
        }

        int lineLength = -1;
        int fieldLength = -1;
        for (unsigned i = bufPos; lineLength < 0 && i < bufSize; i++) {
            switch (m_receiveBuf[i]) {
            case ':':
                if (fieldLength < 0)
                    fieldLength = i - bufPos;
                break;
            case '\r':
                m_discardTrailingNewline = true;
            case '\n':
                lineLength = i - bufPos;
                break;
            }
        }

        if (lineLength < 0)
            break;

        parseEventStreamLine(bufPos, fieldLength, lineLength);
        bufPos += lineLength + 1;

        // EventSource.close() might've been called by one of the message event handlers.
        // Per spec, no further messages should be fired after that.
        if (m_state == CLOSED)
            break;
    }

    if (bufPos == bufSize)
        m_receiveBuf.clear();
    else if (bufPos)
        m_receiveBuf.remove(0, bufPos);
}

void EventSource::parseEventStreamLine(unsigned bufPos, int fieldLength, int lineLength)
{
    if (!lineLength) {
        if (!m_data.isEmpty()) {
            m_data.removeLast();
            if (!m_currentlyParsedEventId.isNull()) {
                m_lastEventId = m_currentlyParsedEventId;
                m_currentlyParsedEventId = nullAtom;
            }
            dispatchEvent(createMessageEvent());
        }
        if (!m_eventName.isEmpty())
            m_eventName = emptyAtom;
    } else if (fieldLength) {
        bool noValue = fieldLength < 0;

        String field(&m_receiveBuf[bufPos], noValue ? lineLength : fieldLength);
        int step;
        if (noValue)
            step = lineLength;
        else if (m_receiveBuf[bufPos + fieldLength + 1] != ' ')
            step = fieldLength + 1;
        else
            step = fieldLength + 2;
        bufPos += step;
        int valueLength = lineLength - step;

        if (field == "data") {
            if (valueLength)
                m_data.append(&m_receiveBuf[bufPos], valueLength);
            m_data.append('\n');
        } else if (field == "event") {
            m_eventName = valueLength ? AtomicString(&m_receiveBuf[bufPos], valueLength) : "";
        } else if (field == "id") {
            m_currentlyParsedEventId = valueLength ? AtomicString(&m_receiveBuf[bufPos], valueLength) : "";
        } else if (field == "retry") {
            if (!valueLength)
                m_reconnectDelay = defaultReconnectDelay;
            else {
                String value(&m_receiveBuf[bufPos], valueLength);
                bool ok;
                unsigned long long retry = value.toUInt64(&ok);
                if (ok)
                    m_reconnectDelay = retry;
            }
        }
    }
}

void EventSource::stop()
{
    close();
}

PassRefPtrWillBeRawPtr<MessageEvent> EventSource::createMessageEvent()
{
    RefPtrWillBeRawPtr<MessageEvent> event = MessageEvent::create();
    event->initMessageEvent(m_eventName.isEmpty() ? EventTypeNames::message : m_eventName, false, false, SerializedScriptValue::create(String(m_data)), m_eventStreamOrigin, m_lastEventId, 0, nullptr);
    m_data.clear();
    return event.release();
}

} // namespace WebCore
