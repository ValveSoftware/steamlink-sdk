// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSession.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/mediasession/MediaMetadata.h"
#include "modules/mediasession/MediaMetadataSanitizer.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/InterfaceProvider.h"
#include "wtf/Optional.h"
#include <memory>

namespace blink {

namespace {

using ::blink::mojom::blink::MediaSessionAction;

const AtomicString& mojomActionToEventName(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::PLAY:
      return EventTypeNames::play;
    case MediaSessionAction::PAUSE:
      return EventTypeNames::pause;
    case MediaSessionAction::PLAY_PAUSE:
      return EventTypeNames::playpause;
    case MediaSessionAction::PREVIOUS_TRACK:
      return EventTypeNames::previoustrack;
    case MediaSessionAction::NEXT_TRACK:
      return EventTypeNames::nexttrack;
    case MediaSessionAction::SEEK_FORWARD:
      return EventTypeNames::seekforward;
    case MediaSessionAction::SEEK_BACKWARD:
      return EventTypeNames::seekbackward;
    default:
      NOTREACHED();
  }
  return WTF::emptyAtom;
}

WTF::Optional<MediaSessionAction> eventNameToMojomAction(
    const AtomicString& eventName) {
  if (EventTypeNames::play == eventName)
    return MediaSessionAction::PLAY;
  if (EventTypeNames::pause == eventName)
    return MediaSessionAction::PAUSE;
  if (EventTypeNames::playpause == eventName)
    return MediaSessionAction::PLAY_PAUSE;
  if (EventTypeNames::previoustrack == eventName)
    return MediaSessionAction::PREVIOUS_TRACK;
  if (EventTypeNames::nexttrack == eventName)
    return MediaSessionAction::NEXT_TRACK;
  if (EventTypeNames::seekforward == eventName)
    return MediaSessionAction::SEEK_FORWARD;
  if (EventTypeNames::seekbackward == eventName)
    return MediaSessionAction::SEEK_BACKWARD;

  NOTREACHED();
  return WTF::nullopt;
}

}  // anonymous namespace

MediaSession::MediaSession(ScriptState* scriptState)
    : m_scriptState(scriptState), m_clientBinding(this) {}

MediaSession* MediaSession::create(ScriptState* scriptState) {
  return new MediaSession(scriptState);
}

void MediaSession::dispose() {
  m_clientBinding.Close();
}

void MediaSession::setMetadata(MediaMetadata* metadata) {
  if (mojom::blink::MediaSessionService* service =
          getService(m_scriptState.get())) {
    service->SetMetadata(MediaMetadataSanitizer::sanitizeAndConvertToMojo(
        metadata, getExecutionContext()));
  }
}

MediaMetadata* MediaSession::metadata() const {
  return m_metadata;
}

const WTF::AtomicString& MediaSession::interfaceName() const {
  return EventTargetNames::MediaSession;
}

ExecutionContext* MediaSession::getExecutionContext() const {
  return m_scriptState->getExecutionContext();
}

mojom::blink::MediaSessionService* MediaSession::getService(
    ScriptState* scriptState) {
  if (m_service)
    return m_service.get();

  DCHECK(scriptState->getExecutionContext()->isDocument())
      << "MediaSession::getService() is only available from a frame";
  Document* document = toDocument(scriptState->getExecutionContext());
  if (!document->frame())
    return nullptr;

  InterfaceProvider* interfaceProvider = document->frame()->interfaceProvider();
  if (!interfaceProvider)
    return nullptr;

  interfaceProvider->getInterface(mojo::GetProxy(&m_service));
  if (m_service.get())
    m_service->SetClient(m_clientBinding.CreateInterfacePtrAndBind());

  return m_service.get();
}

bool MediaSession::addEventListenerInternal(
    const AtomicString& eventType,
    EventListener* listener,
    const AddEventListenerOptionsResolved& options) {
  if (mojom::blink::MediaSessionService* service =
          getService(m_scriptState.get())) {
    auto mojomAction = eventNameToMojomAction(eventType);
    DCHECK(mojomAction.has_value());
    service->EnableAction(mojomAction.value());
  }
  return EventTarget::addEventListenerInternal(eventType, listener, options);
}

bool MediaSession::removeEventListenerInternal(
    const AtomicString& eventType,
    const EventListener* listener,
    const EventListenerOptions& options) {
  if (mojom::blink::MediaSessionService* service =
          getService(m_scriptState.get())) {
    auto mojomAction = eventNameToMojomAction(eventType);
    DCHECK(mojomAction.has_value());
    service->DisableAction(mojomAction.value());
  }
  return EventTarget::removeEventListenerInternal(eventType, listener, options);
}

void MediaSession::DidReceiveAction(
    blink::mojom::blink::MediaSessionAction action) {
  DCHECK(getExecutionContext()->isDocument());
  Document* document = toDocument(getExecutionContext());
  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(document));
  dispatchEvent(Event::create(mojomActionToEventName(action)));
}

DEFINE_TRACE(MediaSession) {
  visitor->trace(m_metadata);
  EventTargetWithInlineData::trace(visitor);
}

}  // namespace blink
