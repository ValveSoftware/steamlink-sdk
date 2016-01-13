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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/encryptedmedia/MediaKeySession.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/MediaKeyError.h"
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "platform/Logging.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace WebCore {

PassOwnPtr<MediaKeySession::PendingAction> MediaKeySession::PendingAction::CreatePendingUpdate(PassRefPtr<Uint8Array> data)
{
    ASSERT(data);
    return adoptPtr(new PendingAction(Update, data));
}

PassOwnPtr<MediaKeySession::PendingAction> MediaKeySession::PendingAction::CreatePendingRelease()
{
    return adoptPtr(new PendingAction(Release, PassRefPtr<Uint8Array>()));
}

MediaKeySession::PendingAction::PendingAction(Type type, PassRefPtr<Uint8Array> data)
    : type(type)
    , data(data)
{
}

MediaKeySession::PendingAction::~PendingAction()
{
}

MediaKeySession* MediaKeySession::create(ExecutionContext* context, blink::WebContentDecryptionModule* cdm, MediaKeys* keys)
{
    MediaKeySession* session = adoptRefCountedGarbageCollectedWillBeNoop(new MediaKeySession(context, cdm, keys));
    session->suspendIfNeeded();
    return session;
}

MediaKeySession::MediaKeySession(ExecutionContext* context, blink::WebContentDecryptionModule* cdm, MediaKeys* keys)
    : ActiveDOMObject(context)
    , m_keySystem(keys->keySystem())
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_session(adoptPtr(cdm->createSession(this)))
    , m_keys(keys)
    , m_isClosed(false)
    , m_actionTimer(this, &MediaKeySession::actionTimerFired)
{
    WTF_LOG(Media, "MediaKeySession::MediaKeySession");
    ScriptWrappable::init(this);
    ASSERT(m_session);
}

MediaKeySession::~MediaKeySession()
{
    m_session.clear();
#if !ENABLE(OILPAN)
    // MediaKeySession and m_asyncEventQueue always become unreachable
    // together. So MediaKeySession and m_asyncEventQueue are destructed in the
    // same GC. We don't need to call cancelAllEvents explicitly in Oilpan.
    m_asyncEventQueue->cancelAllEvents();
#endif
}

void MediaKeySession::setError(MediaKeyError* error)
{
    m_error = error;
}

String MediaKeySession::sessionId() const
{
    return m_session->sessionId();
}

void MediaKeySession::initializeNewSession(const String& mimeType, const Uint8Array& initData)
{
    ASSERT(!m_isClosed);
    m_session->initializeNewSession(mimeType, initData.data(), initData.length());
}

void MediaKeySession::update(Uint8Array* response, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "MediaKeySession::update");
    ASSERT(!m_isClosed);

    // From <https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-update>:
    // The update(response) method must run the following steps:
    // 1. If response is an empty array, throw an INVALID_ACCESS_ERR
    //    exception and abort these steps.
    if (!response->length()) {
        exceptionState.throwDOMException(InvalidAccessError, String::format("The response argument provided is %s.", response ? "an empty array" : "invalid"));
        return;
    }

    // 2. If the session is not in the PENDING state, throw an INVALID_STATE_ERR.
    // FIXME: Implement states in MediaKeySession.

    // 3. Schedule a task to handle the call, providing response.
    m_pendingActions.append(PendingAction::CreatePendingUpdate(response));

    if (!m_actionTimer.isActive())
        m_actionTimer.startOneShot(0, FROM_HERE);
}

void MediaKeySession::release(ExceptionState& exceptionState)
{
    WTF_LOG(Media, "MediaKeySession::release");
    ASSERT(!m_isClosed);

    // From <https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-release>:
    // The release() method must run the following steps:
    // 1. If the state of the MediaKeySession is CLOSED then abort these steps.
    // 2. If the state of the MediaKeySession is ERROR, throw an INVALID_STATE_ERR
    //    exception and abort these steps.
    // FIXME: Implement states in MediaKeySession.

    // 3. Schedule a task to handle the call.
    m_pendingActions.append(PendingAction::CreatePendingRelease());

    if (!m_actionTimer.isActive())
        m_actionTimer.startOneShot(0, FROM_HERE);
}

void MediaKeySession::actionTimerFired(Timer<MediaKeySession>*)
{
    ASSERT(m_pendingActions.size());

    while (!m_pendingActions.isEmpty()) {
        OwnPtr<PendingAction> pendingAction(m_pendingActions.takeFirst());

        switch (pendingAction->type) {
        case PendingAction::Update:
            // NOTE: Continued from step 3. of MediaKeySession::update()
            // 3.1. Let cdm be the cdm loaded in the MediaKeys constructor.
            // 3.2. Let request be null.
            // 3.3. Use cdm to execute the following steps:
            // 3.3.1 Process response.
            m_session->update(pendingAction->data->data(), pendingAction->data->length());
            break;
        case PendingAction::Release:
            // NOTE: Continued from step 3. of MediaKeySession::release().
            // 3.1 Let cdm be the cdm loaded in the MediaKeys constructor.
            // 3.2 Use cdm to execute the following steps:
            // 3.2.1 Process the release request.
            m_session->release();
            break;
        }
    }
}

// Queue a task to fire a simple event named keymessage at the new object
void MediaKeySession::message(const unsigned char* message, size_t messageLength, const blink::WebURL& destinationURL)
{
    WTF_LOG(Media, "MediaKeySession::message");

    MediaKeyMessageEventInit init;
    init.bubbles = false;
    init.cancelable = false;
    init.message = Uint8Array::create(message, messageLength);
    init.destinationURL = destinationURL.string();

    RefPtrWillBeRawPtr<MediaKeyMessageEvent> event = MediaKeyMessageEvent::create(EventTypeNames::message, init);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::ready()
{
    WTF_LOG(Media, "MediaKeySession::ready");

    RefPtrWillBeRawPtr<Event> event = Event::create(EventTypeNames::ready);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::close()
{
    WTF_LOG(Media, "MediaKeySession::close");

    RefPtrWillBeRawPtr<Event> event = Event::create(EventTypeNames::close);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());

    // Once closed, the session can no longer be the target of events from
    // the CDM so this object can be garbage collected.
    m_isClosed = true;
}

// Queue a task to fire a simple event named keyadded at the MediaKeySession object.
void MediaKeySession::error(MediaKeyErrorCode errorCode, unsigned long systemCode)
{
    WTF_LOG(Media, "MediaKeySession::error: errorCode=%d, systemCode=%lu", errorCode, systemCode);

    MediaKeyError::Code mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
    switch (errorCode) {
    case MediaKeyErrorCodeUnknown:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
        break;
    case MediaKeyErrorCodeClient:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        break;
    }

    // 1. Create a new MediaKeyError object with the following attributes:
    //    code = the appropriate MediaKeyError code
    //    systemCode = a Key System-specific value, if provided, and 0 otherwise
    // 2. Set the MediaKeySession object's error attribute to the error object created in the previous step.
    m_error = MediaKeyError::create(mediaKeyErrorCode, systemCode);

    // 3. queue a task to fire a simple event named keyerror at the MediaKeySession object.
    RefPtrWillBeRawPtr<Event> event = Event::create(EventTypeNames::error);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::error(blink::WebContentDecryptionModuleException exception, unsigned long systemCode, const blink::WebString& errorMessage)
{
    WTF_LOG(Media, "MediaKeySession::error: exception=%d, systemCode=%lu", exception, systemCode);

    // FIXME: EME-WD MediaKeyError now derives from DOMException. Figure out how
    // to implement this without breaking prefixed EME, which has a totally
    // different definition. The spec may also change to be just a DOMException.
    // For now, simply generate an existing MediaKeyError.
    MediaKeyErrorCode errorCode;
    switch (exception) {
    case blink::WebContentDecryptionModuleExceptionClientError:
        errorCode = MediaKeyErrorCodeClient;
        break;
    default:
        // All other exceptions get converted into Unknown.
        errorCode = MediaKeyErrorCodeUnknown;
        break;
    }
    error(errorCode, systemCode);
}

const AtomicString& MediaKeySession::interfaceName() const
{
    return EventTargetNames::MediaKeySession;
}

ExecutionContext* MediaKeySession::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

bool MediaKeySession::hasPendingActivity() const
{
    // Remain around if there are pending events or MediaKeys is still around
    // and we're not closed.
    return ActiveDOMObject::hasPendingActivity()
        || !m_pendingActions.isEmpty()
        || m_asyncEventQueue->hasPendingEvents()
        || (m_keys && !m_isClosed);
}

void MediaKeySession::stop()
{
    // Stop the CDM from firing any more events for this session.
    m_session.clear();
    m_isClosed = true;

    if (m_actionTimer.isActive())
        m_actionTimer.stop();
    m_pendingActions.clear();
    m_asyncEventQueue->close();
}

void MediaKeySession::trace(Visitor* visitor)
{
    visitor->trace(m_asyncEventQueue);
    visitor->trace(m_keys);
    EventTargetWithInlineData::trace(visitor);
}

}
