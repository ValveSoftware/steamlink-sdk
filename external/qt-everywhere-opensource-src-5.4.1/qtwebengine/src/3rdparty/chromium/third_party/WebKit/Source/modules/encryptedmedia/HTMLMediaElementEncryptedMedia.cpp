// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/MediaKeyError.h"
#include "core/html/MediaKeyEvent.h"
#include "modules/encryptedmedia/MediaKeyNeededEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace WebCore {

static void throwExceptionIfMediaKeyExceptionOccurred(const String& keySystem, const String& sessionId, blink::WebMediaPlayer::MediaKeyException exception, ExceptionState& exceptionState)
{
    switch (exception) {
    case blink::WebMediaPlayer::MediaKeyExceptionNoError:
        return;
    case blink::WebMediaPlayer::MediaKeyExceptionInvalidPlayerState:
        exceptionState.throwDOMException(InvalidStateError, "The player is in an invalid state.");
        return;
    case blink::WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported:
        exceptionState.throwDOMException(NotSupportedError, "The key system provided ('" + keySystem +"') is not supported.");
        return;
    case blink::WebMediaPlayer::MediaKeyExceptionInvalidAccess:
        exceptionState.throwDOMException(InvalidAccessError, "The session ID provided ('" + sessionId + "') is invalid.");
        return;
    }

    ASSERT_NOT_REACHED();
    return;
}

HTMLMediaElementEncryptedMedia::HTMLMediaElementEncryptedMedia()
    : m_emeMode(EmeModeNotSelected)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(HTMLMediaElementEncryptedMedia)

const char* HTMLMediaElementEncryptedMedia::supplementName()
{
    return "HTMLMediaElementEncryptedMedia";
}

HTMLMediaElementEncryptedMedia& HTMLMediaElementEncryptedMedia::from(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia* supplement = static_cast<HTMLMediaElementEncryptedMedia*>(WillBeHeapSupplement<HTMLMediaElement>::from(element, supplementName()));
    if (!supplement) {
        supplement = new HTMLMediaElementEncryptedMedia();
        provideTo(element, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

bool HTMLMediaElementEncryptedMedia::setEmeMode(EmeMode emeMode, ExceptionState& exceptionState)
{
    if (m_emeMode != EmeModeNotSelected && m_emeMode != emeMode) {
        exceptionState.throwDOMException(InvalidStateError, "Mixed use of EME prefixed and unprefixed API not allowed.");
        return false;
    }
    m_emeMode = emeMode;
    return true;
}

blink::WebContentDecryptionModule* HTMLMediaElementEncryptedMedia::contentDecryptionModule()
{
    return m_mediaKeys ? m_mediaKeys->contentDecryptionModule() : 0;
}

MediaKeys* HTMLMediaElementEncryptedMedia::mediaKeys(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    return thisElement.m_mediaKeys.get();
}

void HTMLMediaElementEncryptedMedia::setMediaKeysInternal(HTMLMediaElement& element, MediaKeys* mediaKeys)
{
    if (m_mediaKeys == mediaKeys)
        return;

    ASSERT(m_emeMode == EmeModeUnprefixed);
    m_mediaKeys = mediaKeys;

    // If a player is connected, tell it that the CDM has changed.
    if (element.webMediaPlayer())
        element.webMediaPlayer()->setContentDecryptionModule(contentDecryptionModule());
}

void HTMLMediaElementEncryptedMedia::setMediaKeys(HTMLMediaElement& element, MediaKeys* mediaKeys, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::setMediaKeys");
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);

    if (!thisElement.setEmeMode(EmeModeUnprefixed, exceptionState))
        return;

    thisElement.setMediaKeysInternal(element, mediaKeys);
}

// Create a MediaKeyNeededEvent for WD EME.
static PassRefPtrWillBeRawPtr<Event> createNeedKeyEvent(const String& contentType, const unsigned char* initData, unsigned initDataLength)
{
    MediaKeyNeededEventInit initializer;
    initializer.contentType = contentType;
    initializer.initData = Uint8Array::create(initData, initDataLength);
    initializer.bubbles = false;
    initializer.cancelable = false;

    return MediaKeyNeededEvent::create(EventTypeNames::needkey, initializer);
}

// Create a 'needkey' MediaKeyEvent for v0.1b EME.
static PassRefPtrWillBeRawPtr<Event> createWebkitNeedKeyEvent(const String& contentType, const unsigned char* initData, unsigned initDataLength)
{
    MediaKeyEventInit webkitInitializer;
    webkitInitializer.keySystem = String();
    webkitInitializer.sessionId = String();
    webkitInitializer.initData = Uint8Array::create(initData, initDataLength);
    webkitInitializer.bubbles = false;
    webkitInitializer.cancelable = false;

    return MediaKeyEvent::create(EventTypeNames::webkitneedkey, webkitInitializer);
}

void HTMLMediaElementEncryptedMedia::webkitGenerateKeyRequest(HTMLMediaElement& element, const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState& exceptionState)
{
    HTMLMediaElementEncryptedMedia::from(element).generateKeyRequest(element.webMediaPlayer(), keySystem, initData, exceptionState);
}

void HTMLMediaElementEncryptedMedia::generateKeyRequest(blink::WebMediaPlayer* webMediaPlayer, const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::webkitGenerateKeyRequest");

    if (!setEmeMode(EmeModePrefixed, exceptionState))
        return;

    if (keySystem.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The key system provided is empty.");
        return;
    }

    if (!webMediaPlayer) {
        exceptionState.throwDOMException(InvalidStateError, "No media has been loaded.");
        return;
    }

    const unsigned char* initDataPointer = 0;
    unsigned initDataLength = 0;
    if (initData) {
        initDataPointer = initData->data();
        initDataLength = initData->length();
    }

    blink::WebMediaPlayer::MediaKeyException result = webMediaPlayer->generateKeyRequest(keySystem, initDataPointer, initDataLength);
    throwExceptionIfMediaKeyExceptionOccurred(keySystem, String(), result, exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitGenerateKeyRequest(HTMLMediaElement& mediaElement, const String& keySystem, ExceptionState& exceptionState)
{
    webkitGenerateKeyRequest(mediaElement, keySystem, Uint8Array::create(0), exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitAddKey(HTMLMediaElement& element, const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState& exceptionState)
{
    HTMLMediaElementEncryptedMedia::from(element).addKey(element.webMediaPlayer(), keySystem, key, initData, sessionId, exceptionState);
}

void HTMLMediaElementEncryptedMedia::addKey(blink::WebMediaPlayer* webMediaPlayer, const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::webkitAddKey");

    if (!setEmeMode(EmeModePrefixed, exceptionState))
        return;

    if (keySystem.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The key system provided is empty.");
        return;
    }

    if (!key) {
        exceptionState.throwDOMException(SyntaxError, "The key provided is invalid.");
        return;
    }

    if (!key->length()) {
        exceptionState.throwDOMException(TypeMismatchError, "The key provided is invalid.");
        return;
    }

    if (!webMediaPlayer) {
        exceptionState.throwDOMException(InvalidStateError, "No media has been loaded.");
        return;
    }

    const unsigned char* initDataPointer = 0;
    unsigned initDataLength = 0;
    if (initData) {
        initDataPointer = initData->data();
        initDataLength = initData->length();
    }

    blink::WebMediaPlayer::MediaKeyException result = webMediaPlayer->addKey(keySystem, key->data(), key->length(), initDataPointer, initDataLength, sessionId);
    throwExceptionIfMediaKeyExceptionOccurred(keySystem, sessionId, result, exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitAddKey(HTMLMediaElement& mediaElement, const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionState& exceptionState)
{
    webkitAddKey(mediaElement, keySystem, key, Uint8Array::create(0), String(), exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitCancelKeyRequest(HTMLMediaElement& element, const String& keySystem, const String& sessionId, ExceptionState& exceptionState)
{
    HTMLMediaElementEncryptedMedia::from(element).cancelKeyRequest(element.webMediaPlayer(), keySystem, sessionId, exceptionState);
}

void HTMLMediaElementEncryptedMedia::cancelKeyRequest(blink::WebMediaPlayer* webMediaPlayer, const String& keySystem, const String& sessionId, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::webkitCancelKeyRequest");

    if (!setEmeMode(EmeModePrefixed, exceptionState))
        return;

    if (keySystem.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The key system provided is empty.");
        return;
    }

    if (!webMediaPlayer) {
        exceptionState.throwDOMException(InvalidStateError, "No media has been loaded.");
        return;
    }

    blink::WebMediaPlayer::MediaKeyException result = webMediaPlayer->cancelKeyRequest(keySystem, sessionId);
    throwExceptionIfMediaKeyExceptionOccurred(keySystem, sessionId, result, exceptionState);
}

void HTMLMediaElementEncryptedMedia::keyAdded(HTMLMediaElement& element, const String& keySystem, const String& sessionId)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyAdded");

    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtrWillBeRawPtr<Event> event = MediaKeyEvent::create(EventTypeNames::webkitkeyadded, initializer);
    event->setTarget(&element);
    element.scheduleEvent(event.release());
}

void HTMLMediaElementEncryptedMedia::keyError(HTMLMediaElement& element, const String& keySystem, const String& sessionId, blink::WebMediaPlayerClient::MediaKeyErrorCode errorCode, unsigned short systemCode)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyError: sessionID=%s, errorCode=%d, systemCode=%d", sessionId.utf8().data(), errorCode, systemCode);

    MediaKeyError::Code mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
    switch (errorCode) {
    case blink::WebMediaPlayerClient::MediaKeyErrorCodeUnknown:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
        break;
    case blink::WebMediaPlayerClient::MediaKeyErrorCodeClient:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        break;
    case blink::WebMediaPlayerClient::MediaKeyErrorCodeService:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_SERVICE;
        break;
    case blink::WebMediaPlayerClient::MediaKeyErrorCodeOutput:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_OUTPUT;
        break;
    case blink::WebMediaPlayerClient::MediaKeyErrorCodeHardwareChange:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_HARDWARECHANGE;
        break;
    case blink::WebMediaPlayerClient::MediaKeyErrorCodeDomain:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_DOMAIN;
        break;
    }

    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.errorCode = MediaKeyError::create(mediaKeyErrorCode);
    initializer.systemCode = systemCode;
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtrWillBeRawPtr<Event> event = MediaKeyEvent::create(EventTypeNames::webkitkeyerror, initializer);
    event->setTarget(&element);
    element.scheduleEvent(event.release());
}

void HTMLMediaElementEncryptedMedia::keyMessage(HTMLMediaElement& element, const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const blink::WebURL& defaultURL)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyMessage: sessionID=%s", sessionId.utf8().data());

    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.message = Uint8Array::create(message, messageLength);
    initializer.defaultURL = KURL(defaultURL);
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtrWillBeRawPtr<Event> event = MediaKeyEvent::create(EventTypeNames::webkitkeymessage, initializer);
    event->setTarget(&element);
    element.scheduleEvent(event.release());
}

void HTMLMediaElementEncryptedMedia::keyNeeded(HTMLMediaElement& element, const String& contentType, const unsigned char* initData, unsigned initDataLength)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyNeeded: contentType=%s", contentType.utf8().data());

    if (RuntimeEnabledFeatures::encryptedMediaEnabled()) {
        // Send event for WD EME.
        RefPtrWillBeRawPtr<Event> event = createNeedKeyEvent(contentType, initData, initDataLength);
        event->setTarget(&element);
        element.scheduleEvent(event.release());
    }

    if (RuntimeEnabledFeatures::prefixedEncryptedMediaEnabled()) {
        // Send event for v0.1b EME.
        RefPtrWillBeRawPtr<Event> event = createWebkitNeedKeyEvent(contentType, initData, initDataLength);
        event->setTarget(&element);
        element.scheduleEvent(event.release());
    }
}

void HTMLMediaElementEncryptedMedia::playerDestroyed(HTMLMediaElement& element)
{
#if ENABLE(OILPAN)
    // FIXME: Oilpan: remove this once the media player is on the heap. crbug.com/378229
    if (element.isFinalizing())
        return;
#endif

    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    thisElement.setMediaKeysInternal(element, 0);
}

blink::WebContentDecryptionModule* HTMLMediaElementEncryptedMedia::contentDecryptionModule(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    return thisElement.contentDecryptionModule();
}

void HTMLMediaElementEncryptedMedia::trace(Visitor* visitor)
{
    visitor->trace(m_mediaKeys);
    WillBeHeapSupplement<HTMLMediaElement>::trace(visitor);
}

} // namespace WebCore
