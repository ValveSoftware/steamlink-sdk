// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/encryptedmedia/ContentDecryptionModuleResultPromise.h"
#include "modules/encryptedmedia/EncryptedMediaUtils.h"
#include "modules/encryptedmedia/MediaEncryptedEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "platform/ContentDecryptionModuleResult.h"
#include "platform/Logging.h"
#include "wtf/Functional.h"

#define EME_LOG_LEVEL 3

namespace blink {

// This class allows MediaKeys to be set asynchronously.
class SetMediaKeysHandler : public ScriptPromiseResolver {
    WTF_MAKE_NONCOPYABLE(SetMediaKeysHandler);
public:
    static ScriptPromise create(ScriptState*, HTMLMediaElement&, MediaKeys*);
    ~SetMediaKeysHandler() override;

    DECLARE_VIRTUAL_TRACE();

private:
    SetMediaKeysHandler(ScriptState*, HTMLMediaElement&, MediaKeys*);
    void timerFired(Timer<SetMediaKeysHandler>*);

    void clearExistingMediaKeys();
    void setNewMediaKeys();

    void finish();
    void fail(ExceptionCode, const String& errorMessage);

    void clearFailed(ExceptionCode, const String& errorMessage);
    void setFailed(ExceptionCode, const String& errorMessage);

    // Keep media element alive until promise is fulfilled
    Member<HTMLMediaElement> m_element;
    Member<MediaKeys> m_newMediaKeys;
    bool m_madeReservation;
    Timer<SetMediaKeysHandler> m_timer;
};

typedef Function<void()> SuccessCallback;
typedef Function<void(ExceptionCode, const String&)> FailureCallback;

// Represents the result used when setContentDecryptionModule() is called.
// Calls |success| if result is resolved, |failure| is result is rejected.
class SetContentDecryptionModuleResult final : public ContentDecryptionModuleResult {
public:
    SetContentDecryptionModuleResult(std::unique_ptr<SuccessCallback> success, std::unique_ptr<FailureCallback> failure)
        : m_successCallback(std::move(success))
        , m_failureCallback(std::move(failure))
    {
    }

    // ContentDecryptionModuleResult implementation.
    void complete() override
    {
        (*m_successCallback)();
    }

    void completeWithContentDecryptionModule(WebContentDecryptionModule*) override
    {
        NOTREACHED();
        (*m_failureCallback)(InvalidStateError, "Unexpected completion.");
    }

    void completeWithSession(WebContentDecryptionModuleResult::SessionStatus status) override
    {
        NOTREACHED();
        (*m_failureCallback)(InvalidStateError, "Unexpected completion.");
    }

    void completeWithError(WebContentDecryptionModuleException code, unsigned long systemCode, const WebString& message) override
    {
        // Non-zero |systemCode| is appended to the |message|. If the |message|
        // is empty, we'll report "Rejected with system code (systemCode)".
        String errorString = message;
        if (systemCode != 0) {
            if (errorString.isEmpty())
                errorString.append("Rejected with system code");
            errorString.append(" (" + String::number(systemCode) + ")");
        }
        (*m_failureCallback)(WebCdmExceptionToExceptionCode(code), errorString);
    }

private:
    std::unique_ptr<SuccessCallback> m_successCallback;
    std::unique_ptr<FailureCallback> m_failureCallback;
};

ScriptPromise SetMediaKeysHandler::create(ScriptState* scriptState, HTMLMediaElement& element, MediaKeys* mediaKeys)
{
    SetMediaKeysHandler* handler = new SetMediaKeysHandler(scriptState, element, mediaKeys);
    handler->suspendIfNeeded();
    handler->keepAliveWhilePending();
    return handler->promise();
}

SetMediaKeysHandler::SetMediaKeysHandler(ScriptState* scriptState, HTMLMediaElement& element, MediaKeys* mediaKeys)
    : ScriptPromiseResolver(scriptState)
    , m_element(element)
    , m_newMediaKeys(mediaKeys)
    , m_madeReservation(false)
    , m_timer(this, &SetMediaKeysHandler::timerFired)
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;

    // 5. Run the following steps in parallel.
    m_timer.startOneShot(0, BLINK_FROM_HERE);
}

SetMediaKeysHandler::~SetMediaKeysHandler()
{
}

void SetMediaKeysHandler::timerFired(Timer<SetMediaKeysHandler>*)
{
    clearExistingMediaKeys();
}

void SetMediaKeysHandler::clearExistingMediaKeys()
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(*m_element);

    // 5.1 If mediaKeys is not null, the CDM instance represented by
    //     mediaKeys is already in use by another media element, and
    //     the user agent is unable to use it with this element, let
    //     this object's attaching media keys value be false and
    //     reject promise with a QuotaExceededError.
    if (m_newMediaKeys) {
        if (!m_newMediaKeys->reserveForMediaElement(m_element.get())) {
            thisElement.m_isAttachingMediaKeys = false;
            fail(QuotaExceededError, "The MediaKeys object is already in use by another media element.");
            return;
        }
        // Note that |m_newMediaKeys| is now considered reserved for
        // |m_element|, so it needs to be accepted or cancelled.
        m_madeReservation = true;
    }

    // 5.2 If the mediaKeys attribute is not null, run the following steps:
    if (thisElement.m_mediaKeys) {
        WebMediaPlayer* mediaPlayer = m_element->webMediaPlayer();
        if (mediaPlayer) {
            // 5.2.1 If the user agent or CDM do not support removing the
            //       association, let this object's attaching media keys
            //       value be false and reject promise with a NotSupportedError.
            // 5.2.2 If the association cannot currently be removed,
            //       let this object's attaching media keys value be false
            //       and reject promise with an InvalidStateError.
            // 5.2.3 Stop using the CDM instance represented by the mediaKeys
            //       attribute to decrypt media data and remove the association
            //       with the media element.
            // (All 3 steps handled as needed in Chromium.)
            std::unique_ptr<SuccessCallback> successCallback = WTF::bind(&SetMediaKeysHandler::setNewMediaKeys, wrapPersistent(this));
            std::unique_ptr<FailureCallback> failureCallback = WTF::bind(&SetMediaKeysHandler::clearFailed, wrapPersistent(this));
            ContentDecryptionModuleResult* result = new SetContentDecryptionModuleResult(std::move(successCallback), std::move(failureCallback));
            mediaPlayer->setContentDecryptionModule(nullptr, result->result());

            // Don't do anything more until |result| is resolved (or rejected).
            return;
        }
    }

    // MediaKeys not currently set or no player connected, so continue on.
    setNewMediaKeys();
}

void SetMediaKeysHandler::setNewMediaKeys()
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;

    // 5.3 If mediaKeys is not null, run the following steps:
    if (m_newMediaKeys) {
        // 5.3.1 Associate the CDM instance represented by mediaKeys with the
        //       media element for decrypting media data.
        // 5.3.2 If the preceding step failed, run the following steps:
        //       (done in setFailed()).
        // 5.3.3 Queue a task to run the Attempt to Resume Playback If Necessary
        //       algorithm on the media element.
        //       (Handled in Chromium).
        if (m_element->webMediaPlayer()) {
            std::unique_ptr<SuccessCallback> successCallback = WTF::bind(&SetMediaKeysHandler::finish, wrapPersistent(this));
            std::unique_ptr<FailureCallback> failureCallback = WTF::bind(&SetMediaKeysHandler::setFailed, wrapPersistent(this));
            ContentDecryptionModuleResult* result = new SetContentDecryptionModuleResult(std::move(successCallback), std::move(failureCallback));
            m_element->webMediaPlayer()->setContentDecryptionModule(m_newMediaKeys->contentDecryptionModule(), result->result());

            // Don't do anything more until |result| is resolved (or rejected).
            return;
        }
    }

    // MediaKeys doesn't need to be set on the player, so continue on.
    finish();
}

void SetMediaKeysHandler::finish()
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(*m_element);

    // 5.4 Set the mediaKeys attribute to mediaKeys.
    if (thisElement.m_mediaKeys)
        thisElement.m_mediaKeys->clearMediaElement();
    thisElement.m_mediaKeys = m_newMediaKeys;
    if (m_madeReservation)
        m_newMediaKeys->acceptReservation();

    // 5.5 Let this object's attaching media keys value be false.
    thisElement.m_isAttachingMediaKeys = false;

    // 5.6 Resolve promise with undefined.
    resolve();
}

void SetMediaKeysHandler::fail(ExceptionCode code, const String& errorMessage)
{
    // Reset ownership of |m_newMediaKeys|.
    if (m_madeReservation)
        m_newMediaKeys->cancelReservation();

    // Make sure attaching media keys value is false.
    DCHECK(!HTMLMediaElementEncryptedMedia::from(*m_element).m_isAttachingMediaKeys);

    // Reject promise with an appropriate error.
    reject(DOMException::create(code, errorMessage));
}

void SetMediaKeysHandler::clearFailed(ExceptionCode code, const String& errorMessage)
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__ << "(" << code << ", " << errorMessage << ")";
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(*m_element);

    // 5.2.4 If the preceding step failed, let this object's attaching media
    //      keys value be false and reject promise with an appropriate
    //      error name.
    thisElement.m_isAttachingMediaKeys = false;
    fail(code, errorMessage);
}

void SetMediaKeysHandler::setFailed(ExceptionCode code, const String& errorMessage)
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__ << "(" << code << ", " << errorMessage << ")";
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(*m_element);

    // 5.3.2 If the preceding step failed (in setContentDecryptionModule()
    //       called from setNewMediaKeys()), run the following steps:
    // 5.3.2.1 Set the mediaKeys attribute to null.
    thisElement.m_mediaKeys.clear();

    // 5.3.2.2 Let this object's attaching media keys value be false.
    thisElement.m_isAttachingMediaKeys = false;

    // 5.3.2.3 Reject promise with a new DOMException whose name is the
    //         appropriate error name.
    fail(code, errorMessage);
}

DEFINE_TRACE(SetMediaKeysHandler)
{
    visitor->trace(m_element);
    visitor->trace(m_newMediaKeys);
    ScriptPromiseResolver::trace(visitor);
}

HTMLMediaElementEncryptedMedia::HTMLMediaElementEncryptedMedia(HTMLMediaElement& element)
    : m_mediaElement(&element)
    , m_isWaitingForKey(false)
    , m_isAttachingMediaKeys(false)
{
}

HTMLMediaElementEncryptedMedia::~HTMLMediaElementEncryptedMedia()
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;
}

const char* HTMLMediaElementEncryptedMedia::supplementName()
{
    return "HTMLMediaElementEncryptedMedia";
}

HTMLMediaElementEncryptedMedia& HTMLMediaElementEncryptedMedia::from(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia* supplement = static_cast<HTMLMediaElementEncryptedMedia*>(Supplement<HTMLMediaElement>::from(element, supplementName()));
    if (!supplement) {
        supplement = new HTMLMediaElementEncryptedMedia(element);
        provideTo(element, supplementName(), supplement);
    }
    return *supplement;
}

MediaKeys* HTMLMediaElementEncryptedMedia::mediaKeys(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    return thisElement.m_mediaKeys.get();
}

ScriptPromise HTMLMediaElementEncryptedMedia::setMediaKeys(ScriptState* scriptState, HTMLMediaElement& element, MediaKeys* mediaKeys)
{
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__ << " current(" << thisElement.m_mediaKeys.get() << "), new(" << mediaKeys << ")";

    // From http://w3c.github.io/encrypted-media/#setMediaKeys

    // 1. If mediaKeys and the mediaKeys attribute are the same object,
    //    return a resolved promise.
    if (thisElement.m_mediaKeys == mediaKeys)
        return ScriptPromise::castUndefined(scriptState);

    // 2. If this object's attaching media keys value is true, return a
    //    promise rejected with an InvalidStateError.
    if (thisElement.m_isAttachingMediaKeys) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidStateError, "Another request is in progress."));
    }

    // 3. Let this object's attaching media keys value be true.
    thisElement.m_isAttachingMediaKeys = true;

    // 4. Let promise be a new promise. Remaining steps done in handler.
    return SetMediaKeysHandler::create(scriptState, element, mediaKeys);
}

// Create a MediaEncryptedEvent for WD EME.
static Event* createEncryptedEvent(WebEncryptedMediaInitDataType initDataType, const unsigned char* initData, unsigned initDataLength)
{
    MediaEncryptedEventInit initializer;
    initializer.setInitDataType(EncryptedMediaUtils::convertFromInitDataType(initDataType));
    initializer.setInitData(DOMArrayBuffer::create(initData, initDataLength));
    initializer.setBubbles(false);
    initializer.setCancelable(false);

    return MediaEncryptedEvent::create(EventTypeNames::encrypted, initializer);
}

void HTMLMediaElementEncryptedMedia::encrypted(WebEncryptedMediaInitDataType initDataType, const unsigned char* initData, unsigned initDataLength)
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;

    Event* event;
    if (m_mediaElement->isMediaDataCORSSameOrigin(m_mediaElement->getExecutionContext()->getSecurityOrigin())) {
        event = createEncryptedEvent(initDataType, initData, initDataLength);
    } else {
        // Current page is not allowed to see content from the media file,
        // so don't return the initData. However, they still get an event.
        event = createEncryptedEvent(WebEncryptedMediaInitDataType::Unknown, nullptr, 0);
    }

    event->setTarget(m_mediaElement);
    m_mediaElement->scheduleEvent(event);
}

void HTMLMediaElementEncryptedMedia::didBlockPlaybackWaitingForKey()
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;

    // From https://w3c.github.io/encrypted-media/#queue-waitingforkey:
    // It should only be called when the HTMLMediaElement object is potentially
    // playing and its readyState is equal to HAVE_FUTURE_DATA or greater.
    // FIXME: Is this really required?

    // 1. Let the media element be the specified HTMLMediaElement object.
    // 2. If the media element's waiting for key value is false, queue a task
    //    to fire a simple event named waitingforkey at the media element.
    if (!m_isWaitingForKey) {
        Event* event = Event::create(EventTypeNames::waitingforkey);
        event->setTarget(m_mediaElement);
        m_mediaElement->scheduleEvent(event);
    }

    // 3. Set the media element's waiting for key value to true.
    m_isWaitingForKey = true;

    // 4. Suspend playback.
    //    (Already done on the Chromium side by the decryptors.)
}

void HTMLMediaElementEncryptedMedia::didResumePlaybackBlockedForKey()
{
    DVLOG(EME_LOG_LEVEL) << __FUNCTION__;

    // Logic is on the Chromium side to attempt to resume playback when a new
    // key is available. However, |m_isWaitingForKey| needs to be cleared so
    // that a later waitingForKey() call can generate the event.
    m_isWaitingForKey = false;
}

WebContentDecryptionModule* HTMLMediaElementEncryptedMedia::contentDecryptionModule()
{
    return m_mediaKeys ? m_mediaKeys->contentDecryptionModule() : 0;
}

DEFINE_TRACE(HTMLMediaElementEncryptedMedia)
{
    visitor->trace(m_mediaElement);
    visitor->trace(m_mediaKeys);
    Supplement<HTMLMediaElement>::trace(visitor);
}

} // namespace blink
