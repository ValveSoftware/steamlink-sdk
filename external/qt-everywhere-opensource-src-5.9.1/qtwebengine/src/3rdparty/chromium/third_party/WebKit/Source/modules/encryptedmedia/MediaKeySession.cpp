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

#include "modules/encryptedmedia/MediaKeySession.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "modules/encryptedmedia/ContentDecryptionModuleResultPromise.h"
#include "modules/encryptedmedia/EncryptedMediaUtils.h"
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "platform/ContentDecryptionModuleResult.h"
#include "platform/ContentType.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebContentDecryptionModuleException.h"
#include "public/platform/WebContentDecryptionModuleSession.h"
#include "public/platform/WebEncryptedMediaKeyInformation.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "wtf/ASCIICType.h"
#include "wtf/PtrUtil.h"
#include <cmath>
#include <limits>

#define MEDIA_KEY_SESSION_LOG_LEVEL 3

namespace {

// Minimum and maximum length for session ids.
enum { MinSessionIdLength = 1, MaxSessionIdLength = 512 };

}  // namespace

namespace blink {

// Checks that |sessionId| looks correct and returns whether all checks pass.
static bool isValidSessionId(const String& sessionId) {
  if ((sessionId.length() < MinSessionIdLength) ||
      (sessionId.length() > MaxSessionIdLength))
    return false;

  if (!sessionId.containsOnlyASCII())
    return false;

  // Check that the sessionId only contains alphanumeric characters.
  for (unsigned i = 0; i < sessionId.length(); ++i) {
    if (!isASCIIAlphanumeric(sessionId[i]))
      return false;
  }

  return true;
}

static bool IsPersistentSessionType(WebEncryptedMediaSessionType sessionType) {
  // This implements section 5.1.1 Is persistent session type? from
  // https://w3c.github.io/encrypted-media/#is-persistent-session-type
  switch (sessionType) {
    case WebEncryptedMediaSessionType::Temporary:
      return false;
    case WebEncryptedMediaSessionType::PersistentLicense:
      return true;
    case WebEncryptedMediaSessionType::PersistentReleaseMessage:
      return true;
    case blink::WebEncryptedMediaSessionType::Unknown:
      break;
  }

  NOTREACHED();
  return false;
}

static String ConvertKeyStatusToString(
    const WebEncryptedMediaKeyInformation::KeyStatus status) {
  switch (status) {
    case WebEncryptedMediaKeyInformation::KeyStatus::Usable:
      return "usable";
    case WebEncryptedMediaKeyInformation::KeyStatus::Expired:
      return "expired";
    case WebEncryptedMediaKeyInformation::KeyStatus::Released:
      return "released";
    case WebEncryptedMediaKeyInformation::KeyStatus::OutputRestricted:
      return "output-restricted";
    case WebEncryptedMediaKeyInformation::KeyStatus::OutputDownscaled:
      return "output-downscaled";
    case WebEncryptedMediaKeyInformation::KeyStatus::StatusPending:
      return "status-pending";
    case WebEncryptedMediaKeyInformation::KeyStatus::InternalError:
      return "internal-error";
  }

  NOTREACHED();
  return "internal-error";
}

static ScriptPromise CreateRejectedPromiseNotCallable(
    ScriptState* scriptState) {
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(InvalidStateError, "The session is not callable."));
}

static ScriptPromise CreateRejectedPromiseAlreadyClosed(
    ScriptState* scriptState) {
  return ScriptPromise::rejectWithDOMException(
      scriptState, DOMException::create(InvalidStateError,
                                        "The session is already closed."));
}

static ScriptPromise CreateRejectedPromiseAlreadyInitialized(
    ScriptState* scriptState) {
  return ScriptPromise::rejectWithDOMException(
      scriptState, DOMException::create(InvalidStateError,
                                        "The session is already initialized."));
}

// A class holding a pending action.
class MediaKeySession::PendingAction
    : public GarbageCollectedFinalized<MediaKeySession::PendingAction> {
 public:
  enum Type { GenerateRequest, Load, Update, Close, Remove };

  Type getType() const { return m_type; }

  ContentDecryptionModuleResult* result() const { return m_result; }

  DOMArrayBuffer* data() const {
    DCHECK(m_type == GenerateRequest || m_type == Update);
    return m_data;
  }

  WebEncryptedMediaInitDataType initDataType() const {
    DCHECK_EQ(GenerateRequest, m_type);
    return m_initDataType;
  }

  const String& sessionId() const {
    DCHECK_EQ(Load, m_type);
    return m_stringData;
  }

  static PendingAction* CreatePendingGenerateRequest(
      ContentDecryptionModuleResult* result,
      WebEncryptedMediaInitDataType initDataType,
      DOMArrayBuffer* initData) {
    DCHECK(result);
    DCHECK(initData);
    return new PendingAction(GenerateRequest, result, initDataType, initData,
                             String());
  }

  static PendingAction* CreatePendingLoadRequest(
      ContentDecryptionModuleResult* result,
      const String& sessionId) {
    DCHECK(result);
    return new PendingAction(Load, result,
                             WebEncryptedMediaInitDataType::Unknown, nullptr,
                             sessionId);
  }

  static PendingAction* CreatePendingUpdate(
      ContentDecryptionModuleResult* result,
      DOMArrayBuffer* data) {
    DCHECK(result);
    DCHECK(data);
    return new PendingAction(
        Update, result, WebEncryptedMediaInitDataType::Unknown, data, String());
  }

  static PendingAction* CreatePendingClose(
      ContentDecryptionModuleResult* result) {
    DCHECK(result);
    return new PendingAction(Close, result,
                             WebEncryptedMediaInitDataType::Unknown, nullptr,
                             String());
  }

  static PendingAction* CreatePendingRemove(
      ContentDecryptionModuleResult* result) {
    DCHECK(result);
    return new PendingAction(Remove, result,
                             WebEncryptedMediaInitDataType::Unknown, nullptr,
                             String());
  }

  ~PendingAction() {}

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_result);
    visitor->trace(m_data);
  }

 private:
  PendingAction(Type type,
                ContentDecryptionModuleResult* result,
                WebEncryptedMediaInitDataType initDataType,
                DOMArrayBuffer* data,
                const String& stringData)
      : m_type(type),
        m_result(result),
        m_initDataType(initDataType),
        m_data(data),
        m_stringData(stringData) {}

  const Type m_type;
  const Member<ContentDecryptionModuleResult> m_result;
  const WebEncryptedMediaInitDataType m_initDataType;
  const Member<DOMArrayBuffer> m_data;
  const String m_stringData;
};

// This class wraps the promise resolver used when initializing a new session
// and is passed to Chromium to fullfill the promise. This implementation of
// completeWithSession() will resolve the promise with void, while
// completeWithError() will reject the promise with an exception. complete()
// is not expected to be called, and will reject the promise.
class NewSessionResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  NewSessionResultPromise(ScriptState* scriptState, MediaKeySession* session)
      : ContentDecryptionModuleResultPromise(scriptState), m_session(session) {}

  ~NewSessionResultPromise() override {}

  // ContentDecryptionModuleResult implementation.
  void completeWithSession(
      WebContentDecryptionModuleResult::SessionStatus status) override {
    if (!isValidToFulfillPromise())
      return;

    if (status != WebContentDecryptionModuleResult::NewSession) {
      NOTREACHED();
      reject(InvalidStateError, "Unexpected completion.");
    }

    m_session->finishGenerateRequest();
    resolve();
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_session);
    ContentDecryptionModuleResultPromise::trace(visitor);
  }

 private:
  Member<MediaKeySession> m_session;
};

// This class wraps the promise resolver used when loading a session
// and is passed to Chromium to fullfill the promise. This implementation of
// completeWithSession() will resolve the promise with true/false, while
// completeWithError() will reject the promise with an exception. complete()
// is not expected to be called, and will reject the promise.
class LoadSessionResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  LoadSessionResultPromise(ScriptState* scriptState, MediaKeySession* session)
      : ContentDecryptionModuleResultPromise(scriptState), m_session(session) {}

  ~LoadSessionResultPromise() override {}

  // ContentDecryptionModuleResult implementation.
  void completeWithSession(
      WebContentDecryptionModuleResult::SessionStatus status) override {
    if (!isValidToFulfillPromise())
      return;

    switch (status) {
      case WebContentDecryptionModuleResult::NewSession:
        m_session->finishLoad();
        resolve(true);
        return;

      case WebContentDecryptionModuleResult::SessionNotFound:
        resolve(false);
        return;

      case WebContentDecryptionModuleResult::SessionAlreadyExists:
        NOTREACHED();
        reject(InvalidStateError, "Unexpected completion.");
        return;
    }

    NOTREACHED();
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_session);
    ContentDecryptionModuleResultPromise::trace(visitor);
  }

 private:
  Member<MediaKeySession> m_session;
};

// This class wraps the promise resolver used by update/close/remove. The
// implementation of complete() will resolve the promise with void. All other
// complete() methods are not expected to be called (and will reject the
// promise).
class SimpleResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  SimpleResultPromise(ScriptState* scriptState, MediaKeySession* session)
      : ContentDecryptionModuleResultPromise(scriptState), m_session(session) {}

  ~SimpleResultPromise() override {}

  // ContentDecryptionModuleResultPromise implementation.
  void complete() override {
    if (!isValidToFulfillPromise())
      return;

    resolve();
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_session);
    ContentDecryptionModuleResultPromise::trace(visitor);
  }

 private:
  // Keep track of the MediaKeySession that created this promise so that it
  // remains reachable as long as this promise is reachable.
  Member<MediaKeySession> m_session;
};

MediaKeySession* MediaKeySession::create(
    ScriptState* scriptState,
    MediaKeys* mediaKeys,
    WebEncryptedMediaSessionType sessionType) {
  MediaKeySession* session =
      new MediaKeySession(scriptState, mediaKeys, sessionType);
  session->suspendIfNeeded();
  return session;
}

MediaKeySession::MediaKeySession(ScriptState* scriptState,
                                 MediaKeys* mediaKeys,
                                 WebEncryptedMediaSessionType sessionType)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(scriptState->getExecutionContext()),
      m_asyncEventQueue(GenericEventQueue::create(this)),
      m_mediaKeys(mediaKeys),
      m_sessionType(sessionType),
      m_expiration(std::numeric_limits<double>::quiet_NaN()),
      m_keyStatusesMap(new MediaKeyStatusMap()),
      m_isUninitialized(true),
      m_isCallable(false),
      m_isClosed(false),
      m_closedPromise(new ClosedPromise(scriptState->getExecutionContext(),
                                        this,
                                        ClosedPromise::Closed)),
      m_actionTimer(this, &MediaKeySession::actionTimerFired) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";
  ThreadState::current()->registerPreFinalizer(this);

  // Create the matching Chromium object. It will not be usable until
  // initializeNewSession() is called in response to the user calling
  // generateRequest().
  WebContentDecryptionModule* cdm = mediaKeys->contentDecryptionModule();
  m_session = wrapUnique(cdm->createSession());
  m_session->setClientInterface(this);

  // From https://w3c.github.io/encrypted-media/#createSession:
  // MediaKeys::createSession(), step 3.
  // 3.1 Let the sessionId attribute be the empty string.
  DCHECK(sessionId().isEmpty());

  // 3.2 Let the expiration attribute be NaN.
  DCHECK(std::isnan(m_expiration));

  // 3.3 Let the closed attribute be a new promise.
  DCHECK(!closed(scriptState).isUndefinedOrNull());

  // 3.4 Let the keyStatuses attribute be empty.
  DCHECK_EQ(0u, m_keyStatusesMap->size());

  // 3.5 Let the session type be sessionType.
  DCHECK(m_sessionType != WebEncryptedMediaSessionType::Unknown);

  // 3.6 Let uninitialized be true.
  DCHECK(m_isUninitialized);

  // 3.7 Let callable be false.
  DCHECK(!m_isCallable);

  // 3.8 Let the use distinctive identifier value be this object's
  // use distinctive identifier.
  // FIXME: Implement this (http://crbug.com/448922).

  // 3.9 Let the cdm implementation value be this object's cdm implementation.
  // 3.10 Let the cdm instance value be this object's cdm instance.
}

MediaKeySession::~MediaKeySession() {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";
}

void MediaKeySession::dispose() {
  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  m_session.reset();
}

String MediaKeySession::sessionId() const {
  return m_session->sessionId();
}

ScriptPromise MediaKeySession::closed(ScriptState* scriptState) {
  return m_closedPromise->promise(scriptState->world());
}

MediaKeyStatusMap* MediaKeySession::keyStatuses() {
  return m_keyStatusesMap;
}

ScriptPromise MediaKeySession::generateRequest(ScriptState* scriptState,
                                               const String& initDataTypeString,
                                               const DOMArrayPiece& initData) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ") "
                                     << initDataTypeString;

  // From https://w3c.github.io/encrypted-media/#generateRequest:
  // Generates a request based on the initData. When this method is invoked,
  // the user agent must run the following steps:

  // 1. If this object is closed, return a promise rejected with an
  //    InvalidStateError.
  if (m_isClosed)
    return CreateRejectedPromiseAlreadyClosed(scriptState);

  // 2. If this object's uninitialized value is false, return a promise
  //    rejected with an InvalidStateError.
  if (!m_isUninitialized)
    return CreateRejectedPromiseAlreadyInitialized(scriptState);

  // 3. Let this object's uninitialized be false.
  m_isUninitialized = false;

  // 4. If initDataType is the empty string, return a promise rejected
  //    with a newly created TypeError.
  if (initDataTypeString.isEmpty()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(
            scriptState->isolate(), "The initDataType parameter is empty."));
  }

  // 5. If initData is an empty array, return a promise rejected with a
  //    newly created TypeError.
  if (!initData.byteLength()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "The initData parameter is empty."));
  }

  // 6. If the Key System implementation represented by this object's cdm
  //    implementation value does not support initDataType as an
  //    Initialization Data Type, return a promise rejected with a new
  //    DOMException whose name is NotSupportedError. String comparison
  //    is case-sensitive.
  //    (blink side doesn't know what the CDM supports, so the proper check
  //     will be done on the Chromium side. However, we can verify that
  //     |initDataType| is one of the registered values.)
  WebEncryptedMediaInitDataType initDataType =
      EncryptedMediaUtils::convertToInitDataType(initDataTypeString);
  if (initDataType == WebEncryptedMediaInitDataType::Unknown) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(NotSupportedError,
                             "The initialization data type '" +
                                 initDataTypeString + "' is not supported."));
  }

  // 7. Let init data be a copy of the contents of the initData parameter.
  DOMArrayBuffer* initDataBuffer =
      DOMArrayBuffer::create(initData.data(), initData.byteLength());

  // 8. Let session type be this object's session type.
  //    (Done in constructor.)

  // 9. Let promise be a new promise.
  NewSessionResultPromise* result =
      new NewSessionResultPromise(scriptState, this);
  ScriptPromise promise = result->promise();

  // 10. Run the following steps asynchronously (done in generateRequestTask())
  m_pendingActions.append(PendingAction::CreatePendingGenerateRequest(
      result, initDataType, initDataBuffer));
  DCHECK(!m_actionTimer.isActive());
  m_actionTimer.startOneShot(0, BLINK_FROM_HERE);

  // 11. Return promise.
  return promise;
}

void MediaKeySession::generateRequestTask(
    ContentDecryptionModuleResult* result,
    WebEncryptedMediaInitDataType initDataType,
    DOMArrayBuffer* initDataBuffer) {
  // NOTE: Continue step 10 of MediaKeySession::generateRequest().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // initializeNewSession() in Chromium will execute steps 10.1 to 10.9.
  m_session->initializeNewSession(
      initDataType, static_cast<unsigned char*>(initDataBuffer->data()),
      initDataBuffer->byteLength(), m_sessionType, result->result());

  // Remaining steps (10.10) executed in finishGenerateRequest(),
  // called when |result| is resolved.
}

void MediaKeySession::finishGenerateRequest() {
  // NOTE: Continue step 10.10 of MediaKeySession::generateRequest().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // 10.10.1 If any of the preceding steps failed, reject promise with a
  //         new DOMException whose name is the appropriate error name.
  //         (Done by CDM calling result.completeWithError() as appropriate.)
  // 10.10.2 Set the sessionId attribute to session id.
  DCHECK(!sessionId().isEmpty());

  // 10.10.3 Let this object's callable be true.
  m_isCallable = true;

  // 10.10.4 Run the Queue a "message" Event algorithm on the session,
  //         providing message type and message.
  //         (Done by the CDM.)
  // 10.10.5 Resolve promise.
  //         (Done by NewSessionResultPromise.)
}

ScriptPromise MediaKeySession::load(ScriptState* scriptState,
                                    const String& sessionId) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ") "
                                     << sessionId;

  // From https://w3c.github.io/encrypted-media/#load:
  // Loads the data stored for the specified session into this object. When
  // this method is invoked, the user agent must run the following steps:

  // 1. If this object is closed, return a promise rejected with an
  //    InvalidStateError.
  if (m_isClosed)
    return CreateRejectedPromiseAlreadyClosed(scriptState);

  // 2. If this object's uninitialized value is false, return a promise
  //    rejected with an InvalidStateError.
  if (!m_isUninitialized)
    return CreateRejectedPromiseAlreadyInitialized(scriptState);

  // 3. Let this object's uninitialized value be false.
  m_isUninitialized = false;

  // 4. If sessionId is the empty string, return a promise rejected with
  //    a newly created TypeError.
  if (sessionId.isEmpty()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "The sessionId parameter is empty."));
  }

  // 5. If the result of running the "Is persistent session type?" algorithm
  //    on this object's session type is false, return a promise rejected
  //    with a newly created TypeError.
  if (!IsPersistentSessionType(m_sessionType)) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(
            scriptState->isolate(), "The session type is not persistent."));
  }

  // 6. Let origin be the origin of this object's Document.
  //    (Available as getExecutionContext()->getSecurityOrigin() anytime.)

  // 7. Let promise be a new promise.
  LoadSessionResultPromise* result =
      new LoadSessionResultPromise(scriptState, this);
  ScriptPromise promise = result->promise();

  // 8. Run the following steps asynchronously (done in loadTask())
  m_pendingActions.append(
      PendingAction::CreatePendingLoadRequest(result, sessionId));
  DCHECK(!m_actionTimer.isActive());
  m_actionTimer.startOneShot(0, BLINK_FROM_HERE);

  // 9. Return promise.
  return promise;
}

void MediaKeySession::loadTask(ContentDecryptionModuleResult* result,
                               const String& sessionId) {
  // NOTE: Continue step 8 of MediaKeySession::load().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // 8.1 Let sanitized session ID be a validated and/or sanitized
  //     version of sessionId. The user agent should thoroughly
  //     validate the sessionId value before passing it to the CDM.
  //     At a minimum, this should include checking that the length
  //     and value (e.g. alphanumeric) are reasonable.
  // 8.2 If the preceding step failed, or if sanitized session ID
  //     is empty, reject promise with a newly created TypeError.
  if (!isValidSessionId(sessionId)) {
    result->completeWithError(WebContentDecryptionModuleExceptionTypeError, 0,
                              "Invalid sessionId");
    return;
  }

  // 8.3 If there is an unclosed session in the object's Document
  //     whose sessionId attribute is sanitized session ID, reject
  //     promise with a new DOMException whose name is
  //     QuotaExceededError. In other words, do not create a session
  //     if a non-closed session, regardless of type, already exists
  //     for this sanitized session ID in this browsing context.
  //     (Done in the CDM.)

  // 8.4 Let expiration time be NaN.
  //     (Done in the constructor.)
  DCHECK(std::isnan(m_expiration));

  // load() in Chromium will execute steps 8.5 through 8.8.
  m_session->load(sessionId, result->result());

  // Remaining step (8.9) executed in finishLoad(), called when |result|
  // is resolved.
}

void MediaKeySession::finishLoad() {
  // NOTE: Continue step 8.9 of MediaKeySession::load().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // 8.9.1 If any of the preceding steps failed, reject promise with a new
  //       DOMException whose name is the appropriate error name.
  //       (Done by CDM calling result.completeWithError() as appropriate.)

  // 8.9.2 Set the sessionId attribute to sanitized session ID.
  DCHECK(!sessionId().isEmpty());

  // 8.9.3 Let this object's callable be true.
  m_isCallable = true;

  // 8.9.4 If the loaded session contains information about any keys (there
  //       are known keys), run the update key statuses algorithm on the
  //       session, providing each key's key ID along with the appropriate
  //       MediaKeyStatus. Should additional processing be necessary to
  //       determine with certainty the status of a key, use the non-"usable"
  //       MediaKeyStatus value that corresponds to the reason for the
  //       additional processing. Once the additional processing for one or
  //       more keys has completed, run the update key statuses algorithm
  //       again if any of the statuses has changed.
  //       (Done by the CDM.)

  // 8.9.5 Run the Update Expiration algorithm on the session,
  //       providing expiration time.
  //       (Done by the CDM.)

  // 8.9.6 If message is not null, run the queue a "message" event algorithm
  //       on the session, providing message type and message.
  //       (Done by the CDM.)

  // 8.9.7 Resolve promise with true.
  //       (Done by LoadSessionResultPromise.)
}

ScriptPromise MediaKeySession::update(ScriptState* scriptState,
                                      const DOMArrayPiece& response) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From https://w3c.github.io/encrypted-media/#update:
  // Provides messages, including licenses, to the CDM. When this method is
  // invoked, the user agent must run the following steps:

  // 1. If this object is closed, return a promise rejected with an
  //    InvalidStateError.
  if (m_isClosed)
    return CreateRejectedPromiseAlreadyClosed(scriptState);

  // 2. If this object's callable value is false, return a promise
  //    rejected with an InvalidStateError.
  if (!m_isCallable)
    return CreateRejectedPromiseNotCallable(scriptState);

  // 3. If response is an empty array, return a promise rejected with a
  //    newly created TypeError.
  if (!response.byteLength()) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(scriptState->isolate(),
                                          "The response parameter is empty."));
  }

  // 4. Let response copy be a copy of the contents of the response parameter.
  DOMArrayBuffer* responseCopy =
      DOMArrayBuffer::create(response.data(), response.byteLength());

  // 5. Let promise be a new promise.
  SimpleResultPromise* result = new SimpleResultPromise(scriptState, this);
  ScriptPromise promise = result->promise();

  // 6. Run the following steps asynchronously (done in updateTask())
  m_pendingActions.append(
      PendingAction::CreatePendingUpdate(result, responseCopy));
  if (!m_actionTimer.isActive())
    m_actionTimer.startOneShot(0, BLINK_FROM_HERE);

  // 7. Return promise.
  return promise;
}

void MediaKeySession::updateTask(ContentDecryptionModuleResult* result,
                                 DOMArrayBuffer* sanitizedResponse) {
  // NOTE: Continue step 6 of MediaKeySession::update().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // update() in Chromium will execute steps 6.1 through 6.8.
  m_session->update(static_cast<unsigned char*>(sanitizedResponse->data()),
                    sanitizedResponse->byteLength(), result->result());

  // Last step (6.8.2 Resolve promise) will be done when |result| is resolved.
}

ScriptPromise MediaKeySession::close(ScriptState* scriptState) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From https://w3c.github.io/encrypted-media/#close:
  // Indicates that the application no longer needs the session and the CDM
  // should release any resources associated with the session and close it.
  // Persisted data should not be released or cleared.
  // When this method is invoked, the user agent must run the following steps:

  // 1. Let session be the associated MediaKeySession object.
  // 2. If session is closed, return a resolved promise.
  if (m_isClosed)
    return ScriptPromise::cast(scriptState, ScriptValue());

  // 3. If session's callable value is false, return a promise rejected with
  //    an InvalidStateError.
  if (!m_isCallable)
    return CreateRejectedPromiseNotCallable(scriptState);

  // 4. Let promise be a new promise.
  SimpleResultPromise* result = new SimpleResultPromise(scriptState, this);
  ScriptPromise promise = result->promise();

  // 5. Run the following steps in parallel (done in closeTask()).
  m_pendingActions.append(PendingAction::CreatePendingClose(result));
  if (!m_actionTimer.isActive())
    m_actionTimer.startOneShot(0, BLINK_FROM_HERE);

  // 6. Return promise.
  return promise;
}

void MediaKeySession::closeTask(ContentDecryptionModuleResult* result) {
  // NOTE: Continue step 4 of MediaKeySession::close().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // close() in Chromium will execute steps 5.1 through 5.3.
  m_session->close(result->result());

  // Last step (5.3.2 Resolve promise) will be done when |result| is resolved.
}

ScriptPromise MediaKeySession::remove(ScriptState* scriptState) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From https://w3c.github.io/encrypted-media/#remove:
  // Removes stored session data associated with this object. When this
  // method is invoked, the user agent must run the following steps:

  // 1. If this object is closed, return a promise rejected with an
  //    InvalidStateError.
  if (m_isClosed)
    return CreateRejectedPromiseAlreadyClosed(scriptState);

  // 2. If this object's callable value is false, return a promise rejected
  //    with an InvalidStateError.
  if (!m_isCallable)
    return CreateRejectedPromiseNotCallable(scriptState);

  // 3. If the result of running the "Is persistent session type?" algorithm
  //    on this object's session type is false, return a promise rejected
  //    with a newly created TypeError.
  if (!IsPersistentSessionType(m_sessionType)) {
    return ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(
            scriptState->isolate(), "The session type is not persistent."));
  }

  // 4. Let promise be a new promise.
  SimpleResultPromise* result = new SimpleResultPromise(scriptState, this);
  ScriptPromise promise = result->promise();

  // 5. Run the following steps asynchronously (done in removeTask()).
  m_pendingActions.append(PendingAction::CreatePendingRemove(result));
  if (!m_actionTimer.isActive())
    m_actionTimer.startOneShot(0, BLINK_FROM_HERE);

  // 6. Return promise.
  return promise;
}

void MediaKeySession::removeTask(ContentDecryptionModuleResult* result) {
  // NOTE: Continue step 5 of MediaKeySession::remove().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // remove() in Chromium will execute steps 5.1 through 5.3.
  m_session->remove(result->result());

  // Last step (5.3.6 Resolve promise) will be done when |result| is resolved.
}

void MediaKeySession::actionTimerFired(TimerBase*) {
  DCHECK(m_pendingActions.size());

  // Resolving promises now run synchronously and may result in additional
  // actions getting added to the queue. As a result, swap the queue to
  // a local copy to avoid problems if this happens.
  HeapDeque<Member<PendingAction>> pendingActions;
  pendingActions.swap(m_pendingActions);

  while (!pendingActions.isEmpty()) {
    PendingAction* action = pendingActions.takeFirst();

    switch (action->getType()) {
      case PendingAction::GenerateRequest:
        generateRequestTask(action->result(), action->initDataType(),
                            action->data());
        break;

      case PendingAction::Load:
        loadTask(action->result(), action->sessionId());
        break;

      case PendingAction::Update:
        updateTask(action->result(), action->data());
        break;

      case PendingAction::Close:
        closeTask(action->result());
        break;

      case PendingAction::Remove:
        removeTask(action->result());
        break;
    }
  }
}

// Queue a task to fire a simple event named keymessage at the new object.
void MediaKeySession::message(MessageType messageType,
                              const unsigned char* message,
                              size_t messageLength) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // Verify that 'message' not fired before session initialization is complete.
  DCHECK(m_isCallable);

  // From https://w3c.github.io/encrypted-media/#queue-message:
  // The following steps are run:
  // 1. Let the session be the specified MediaKeySession object.
  // 2. Queue a task to fire a simple event named message at the session.
  //    The event is of type MediaKeyMessageEvent and has:
  //    -> messageType = the specified message type
  //    -> message = the specified message

  MediaKeyMessageEventInit init;
  switch (messageType) {
    case WebContentDecryptionModuleSession::Client::MessageType::LicenseRequest:
      init.setMessageType("license-request");
      break;
    case WebContentDecryptionModuleSession::Client::MessageType::LicenseRenewal:
      init.setMessageType("license-renewal");
      break;
    case WebContentDecryptionModuleSession::Client::MessageType::LicenseRelease:
      init.setMessageType("license-release");
      break;
  }
  init.setMessage(
      DOMArrayBuffer::create(static_cast<const void*>(message), messageLength));

  MediaKeyMessageEvent* event =
      MediaKeyMessageEvent::create(EventTypeNames::message, init);
  event->setTarget(this);
  m_asyncEventQueue->enqueueEvent(event);
}

void MediaKeySession::close() {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From http://w3c.github.io/encrypted-media/#session-closed
  // 1. Let session be the associated MediaKeySession object.
  // 2. If session's session type is "persistent-usage-record", execute the
  //    following steps in parallel:
  //    1. Let cdm be the CDM instance represented by session's cdm instance
  //       value.
  //    2. Use cdm to store session's record of key usage, if it exists.
  //    ("persistent-usage-record" not supported by Chrome.)

  // 3. Run the Update Key Statuses algorithm on the session, providing an
  //    empty sequence.
  keysStatusesChange(WebVector<WebEncryptedMediaKeyInformation>(), false);

  // 4. Run the Update Expiration algorithm on the session, providing NaN.
  expirationChanged(std::numeric_limits<double>::quiet_NaN());

  // 5. Let promise be the closed attribute of the session.
  // 6. Resolve promise.
  m_closedPromise->resolve(ToV8UndefinedGenerator());

  // After this algorithm has run, event handlers for the events queued by
  // this algorithm will be executed, but no further events can be queued.
  // As a result, no messages can be sent by the CDM as a result of closing
  // the session.
  m_isClosed = true;
}

void MediaKeySession::expirationChanged(double updatedExpiryTimeInMS) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ") "
                                     << updatedExpiryTimeInMS;

  // From https://w3c.github.io/encrypted-media/#update-expiration:
  // The following steps are run:
  // 1. Let the session be the associated MediaKeySession object.
  // 2. Let expiration time be NaN.
  double expirationTime = std::numeric_limits<double>::quiet_NaN();

  // 3. If the new expiration time is not NaN, let expiration time be the
  //    new expiration time in milliseconds since 01 January 1970 UTC.
  //    (Note that Chromium actually passes 0 to indicate no expiry.)
  // FIXME: Get Chromium to pass NaN.
  if (!std::isnan(updatedExpiryTimeInMS) && updatedExpiryTimeInMS != 0.0)
    expirationTime = updatedExpiryTimeInMS;

  // 4. Set the session's expiration attribute to expiration time.
  m_expiration = expirationTime;
}

void MediaKeySession::keysStatusesChange(
    const WebVector<WebEncryptedMediaKeyInformation>& keys,
    bool hasAdditionalUsableKey) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
      << __func__ << "(" << this << ") with " << keys.size()
      << " keys and hasAdditionalUsableKey is "
      << (hasAdditionalUsableKey ? "true" : "false");

  // From https://w3c.github.io/encrypted-media/#update-key-statuses:
  // The following steps are run:
  // 1. Let the session be the associated MediaKeySession object.
  // 2. Let the input statuses be the sequence of pairs key ID and
  //    associated MediaKeyStatus pairs.
  // 3. Let the statuses be session's keyStatuses attribute.

  // 4. Run the following steps to replace the contents of statuses:
  // 4.1 Empty statuses.
  m_keyStatusesMap->clear();

  // 4.2 For each pair in input statuses.
  for (size_t i = 0; i < keys.size(); ++i) {
    // 4.2.1 Let pair be the pair.
    const auto& key = keys[i];
    // 4.2.2 Insert an entry for pair's key ID into statuses with the
    //       value of pair's MediaKeyStatus value.
    m_keyStatusesMap->addEntry(key.id(),
                               ConvertKeyStatusToString(key.status()));
  }

  // 5. Queue a task to fire a simple event named keystatuseschange
  //    at the session.
  Event* event = Event::create(EventTypeNames::keystatuseschange);
  event->setTarget(this);
  m_asyncEventQueue->enqueueEvent(event);

  // 6. Queue a task to run the attempt to resume playback if necessary
  //    algorithm on each of the media element(s) whose mediaKeys attribute
  //    is the MediaKeys object that created the session. The user agent
  //    may choose to skip this step if it knows resuming will fail.
  // FIXME: Attempt to resume playback if |hasAdditionalUsableKey| is true.
  // http://crbug.com/413413
}

const AtomicString& MediaKeySession::interfaceName() const {
  return EventTargetNames::MediaKeySession;
}

ExecutionContext* MediaKeySession::getExecutionContext() const {
  return ActiveDOMObject::getExecutionContext();
}

bool MediaKeySession::hasPendingActivity() const {
  // Remain around if there are pending events or MediaKeys is still around
  // and we're not closed.
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
      << __func__ << "(" << this << ")"
      << (!m_pendingActions.isEmpty() ? " !m_pendingActions.isEmpty()" : "")
      << (m_asyncEventQueue->hasPendingEvents()
              ? " m_asyncEventQueue->hasPendingEvents()"
              : "")
      << ((m_mediaKeys && !m_isClosed) ? " m_mediaKeys && !m_isClosed" : "");

  return !m_pendingActions.isEmpty() || m_asyncEventQueue->hasPendingEvents() ||
         (m_mediaKeys && !m_isClosed);
}

void MediaKeySession::contextDestroyed() {
  // Stop the CDM from firing any more events for this session.
  m_session.reset();
  m_isClosed = true;
  m_actionTimer.stop();
  m_pendingActions.clear();
  m_asyncEventQueue->close();
}

DEFINE_TRACE(MediaKeySession) {
  visitor->trace(m_asyncEventQueue);
  visitor->trace(m_pendingActions);
  visitor->trace(m_mediaKeys);
  visitor->trace(m_keyStatusesMap);
  visitor->trace(m_closedPromise);
  EventTargetWithInlineData::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

}  // namespace blink
