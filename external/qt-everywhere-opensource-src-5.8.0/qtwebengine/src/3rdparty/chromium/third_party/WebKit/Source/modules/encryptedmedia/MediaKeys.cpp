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

#include "modules/encryptedmedia/MediaKeys.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/encryptedmedia/EncryptedMediaUtils.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "modules/encryptedmedia/SimpleContentDecryptionModuleResultPromise.h"
#include "platform/Logging.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "wtf/RefPtr.h"
#include <memory>

#define MEDIA_KEYS_LOG_LEVEL 3

namespace blink {

// A class holding a pending action.
class MediaKeys::PendingAction final : public GarbageCollected<MediaKeys::PendingAction> {
public:
    const Persistent<ContentDecryptionModuleResult> result() const
    {
        return m_result;
    }

    DOMArrayBuffer* data() const
    {
        return m_data;
    }

    static PendingAction* CreatePendingSetServerCertificate(ContentDecryptionModuleResult* result, DOMArrayBuffer* serverCertificate)
    {
        DCHECK(result);
        DCHECK(serverCertificate);
        return new PendingAction(result, serverCertificate);
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_result);
        visitor->trace(m_data);
    }

private:
    PendingAction(ContentDecryptionModuleResult* result, DOMArrayBuffer* data)
        : m_result(result)
        , m_data(data)
    {
    }

    const Member<ContentDecryptionModuleResult> m_result;
    const Member<DOMArrayBuffer> m_data;
};

// This class wraps the promise resolver used when setting the certificate
// and is passed to Chromium to fullfill the promise. This implementation of
// complete() will resolve the promise with true, while completeWithError()
// will reject the promise with an exception. completeWithSession()
// is not expected to be called, and will reject the promise.
class SetCertificateResultPromise : public ContentDecryptionModuleResultPromise {
public:
    SetCertificateResultPromise(ScriptState* scriptState)
        : ContentDecryptionModuleResultPromise(scriptState)
    {
    }

    ~SetCertificateResultPromise() override
    {
    }

    // ContentDecryptionModuleResult implementation.
    void complete() override
    {
        resolve(true);
    }

    void completeWithError(WebContentDecryptionModuleException exceptionCode, unsigned long systemCode, const WebString& errorMessage) override
    {
        // The EME spec specifies that "If the Key System implementation does
        // not support server certificates, return a promise resolved with
        // false." So convert any NOTSUPPORTEDERROR into resolving with false.
        if (exceptionCode == WebContentDecryptionModuleExceptionNotSupportedError) {
            resolve(false);
            return;
        }

        ContentDecryptionModuleResultPromise::completeWithError(exceptionCode, systemCode, errorMessage);
    }
};

MediaKeys* MediaKeys::create(ExecutionContext* context, const WebVector<WebEncryptedMediaSessionType>& supportedSessionTypes, std::unique_ptr<WebContentDecryptionModule> cdm)
{
    MediaKeys* mediaKeys = new MediaKeys(context, supportedSessionTypes, std::move(cdm));
    mediaKeys->suspendIfNeeded();
    return mediaKeys;
}

MediaKeys::MediaKeys(ExecutionContext* context, const WebVector<WebEncryptedMediaSessionType>& supportedSessionTypes, std::unique_ptr<WebContentDecryptionModule> cdm)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_supportedSessionTypes(supportedSessionTypes)
    , m_cdm(std::move(cdm))
    , m_mediaElement(nullptr)
    , m_reservedForMediaElement(false)
    , m_timer(this, &MediaKeys::timerFired)
{
    DVLOG(MEDIA_KEYS_LOG_LEVEL) << __FUNCTION__ << "(" << this << ")";
}

MediaKeys::~MediaKeys()
{
    DVLOG(MEDIA_KEYS_LOG_LEVEL) << __FUNCTION__ << "(" << this << ")";
}

MediaKeySession* MediaKeys::createSession(ScriptState* scriptState, const String& sessionTypeString, ExceptionState& exceptionState)
{
    DVLOG(MEDIA_KEYS_LOG_LEVEL) << __FUNCTION__ << "(" << this << ") " << sessionTypeString;

    // From http://w3c.github.io/encrypted-media/#createSession

    // When this method is invoked, the user agent must run the following steps:
    // 1. If this object's persistent state allowed value is false and
    //    sessionType is not "temporary", throw a new DOMException whose name is
    //    NotSupportedError.
    //    (Chromium ensures that only session types supported by the
    //    configuration are listed in supportedSessionTypes.)
    // 2. If the Key System implementation represented by this object's cdm
    //    implementation value does not support sessionType, throw a new
    //    DOMException whose name is NotSupportedError.
    WebEncryptedMediaSessionType sessionType = EncryptedMediaUtils::convertToSessionType(sessionTypeString);
    if (!sessionTypeSupported(sessionType)) {
        exceptionState.throwDOMException(NotSupportedError, "Unsupported session type.");
        return nullptr;
    }

    // 3. Let session be a new MediaKeySession object, and initialize it as
    //    follows:
    //    (Initialization is performed in the constructor.)
    // 4. Return session.
    return MediaKeySession::create(scriptState, this, sessionType);
}

ScriptPromise MediaKeys::setServerCertificate(ScriptState* scriptState, const DOMArrayPiece& serverCertificate)
{
    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-setservercertificate:
    // The setServerCertificate(serverCertificate) method provides a server
    // certificate to be used to encrypt messages to the license server.
    // It must run the following steps:
    // 1. If serverCertificate is an empty array, return a promise rejected
    //    with a new DOMException whose name is "InvalidAccessError".
    if (!serverCertificate.byteLength()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The serverCertificate parameter is empty."));
    }

    // 2. If the keySystem does not support server certificates, return a
    //    promise rejected with a new DOMException whose name is
    //    "NotSupportedError".
    //    (Let the CDM decide whether to support this or not.)

    // 3. Let certificate be a copy of the contents of the serverCertificate
    //    parameter.
    DOMArrayBuffer* serverCertificateBuffer = DOMArrayBuffer::create(serverCertificate.data(), serverCertificate.byteLength());

    // 4. Let promise be a new promise.
    SetCertificateResultPromise* result = new SetCertificateResultPromise(scriptState);
    ScriptPromise promise = result->promise();

    // 5. Run the following steps asynchronously (documented in timerFired()).
    m_pendingActions.append(PendingAction::CreatePendingSetServerCertificate(result, serverCertificateBuffer));
    if (!m_timer.isActive())
        m_timer.startOneShot(0, BLINK_FROM_HERE);

    // 6. Return promise.
    return promise;
}

bool MediaKeys::reserveForMediaElement(HTMLMediaElement* mediaElement)
{
    // If some other HtmlMediaElement already has a reference to us, fail.
    if (m_mediaElement)
        return false;

    m_mediaElement = mediaElement;
    m_reservedForMediaElement = true;
    return true;
}

void MediaKeys::acceptReservation()
{
    m_reservedForMediaElement = false;
}

void MediaKeys::cancelReservation()
{
    m_reservedForMediaElement = false;
    m_mediaElement.clear();
}

void MediaKeys::clearMediaElement()
{
    DCHECK(m_mediaElement);
    m_mediaElement.clear();
}

bool MediaKeys::sessionTypeSupported(WebEncryptedMediaSessionType sessionType)
{
    for (size_t i = 0; i < m_supportedSessionTypes.size(); i++) {
        if (m_supportedSessionTypes[i] == sessionType)
            return true;
    }

    return false;
}

void MediaKeys::timerFired(Timer<MediaKeys>*)
{
    DCHECK(m_pendingActions.size());

    // Swap the queue to a local copy to avoid problems if resolving promises
    // run synchronously.
    HeapDeque<Member<PendingAction>> pendingActions;
    pendingActions.swap(m_pendingActions);

    while (!pendingActions.isEmpty()) {
        PendingAction* action = pendingActions.takeFirst();
        DVLOG(MEDIA_KEYS_LOG_LEVEL) << __FUNCTION__ << "(" << this << ") Certificate";

        // 5.1 Let cdm be the cdm during the initialization of this object.
        WebContentDecryptionModule* cdm = contentDecryptionModule();

        // 5.2 Use the cdm to process certificate.
        cdm->setServerCertificate(static_cast<unsigned char*>(action->data()->data()), action->data()->byteLength(), action->result()->result());
        // 5.3 If any of the preceding steps failed, reject promise with a
        //     new DOMException whose name is the appropriate error name.
        // 5.4 Resolve promise.
        // (These are handled by Chromium and the CDM.)
    }
}

WebContentDecryptionModule* MediaKeys::contentDecryptionModule()
{
    return m_cdm.get();
}

DEFINE_TRACE(MediaKeys)
{
    visitor->trace(m_pendingActions);
    visitor->trace(m_mediaElement);
    ActiveDOMObject::trace(visitor);
}

void MediaKeys::contextDestroyed()
{
    ActiveDOMObject::contextDestroyed();

    // We don't need the CDM anymore. Only destroyed after all related
    // ActiveDOMObjects have been stopped.
    m_cdm.reset();
}

bool MediaKeys::hasPendingActivity() const
{
    // Remain around if there are pending events.
    DVLOG(MEDIA_KEYS_LOG_LEVEL) << __FUNCTION__ << "(" << this << ")"
        << (!m_pendingActions.isEmpty() ? " !m_pendingActions.isEmpty()" : "")
        << (m_reservedForMediaElement ? " m_reservedForMediaElement" : "");

    return !m_pendingActions.isEmpty() || m_reservedForMediaElement;
}

void MediaKeys::stop()
{
    ActiveDOMObject::stop();

    if (m_timer.isActive())
        m_timer.stop();
    m_pendingActions.clear();
}

} // namespace blink
