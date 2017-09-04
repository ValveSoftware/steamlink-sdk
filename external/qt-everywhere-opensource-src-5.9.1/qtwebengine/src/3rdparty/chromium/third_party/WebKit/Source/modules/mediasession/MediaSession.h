// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSession_h
#define MediaSession_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/mediasession/media_session.mojom-blink.h"
#include <memory>

namespace blink {

class MediaMetadata;
class ScriptState;

class MODULES_EXPORT MediaSession final
    : public EventTargetWithInlineData,
      blink::mojom::blink::MediaSessionClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(MediaSession, dispose);

 public:
  static MediaSession* create(ScriptState*);

  void dispose();

  void setMetadata(MediaMetadata*);
  MediaMetadata* metadata() const;

  // EventTarget implementation.
  const WTF::AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(play);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pause);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(playpause);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(previoustrack);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(nexttrack);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(seekforward);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(seekbackward);

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaSessionTest;

  MediaSession(ScriptState*);

  // EventTarget overrides
  bool addEventListenerInternal(
      const AtomicString& eventType,
      EventListener*,
      const AddEventListenerOptionsResolved&) override;
  bool removeEventListenerInternal(const AtomicString& eventType,
                                   const EventListener*,
                                   const EventListenerOptions&) override;

  // blink::mojom::blink::MediaSessionClient implementation.
  void DidReceiveAction(blink::mojom::blink::MediaSessionAction) override;

  // Returns null when the ExecutionContext is not document.
  mojom::blink::MediaSessionService* getService(ScriptState*);

  RefPtr<ScriptState> m_scriptState;
  Member<MediaMetadata> m_metadata;
  mojom::blink::MediaSessionServicePtr m_service;
  mojo::Binding<blink::mojom::blink::MediaSessionClient> m_clientBinding;
};

}  // namespace blink

#endif  // MediaSession_h
