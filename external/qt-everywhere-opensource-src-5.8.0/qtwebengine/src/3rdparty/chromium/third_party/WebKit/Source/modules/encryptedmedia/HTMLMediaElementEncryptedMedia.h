// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementEncryptedMedia_h
#define HTMLMediaElementEncryptedMedia_h

#include "core/EventTypeNames.h"
#include "core/dom/DOMTypedArray.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebMediaPlayerEncryptedMediaClient.h"

namespace blink {

class ExceptionState;
class HTMLMediaElement;
class MediaKeys;
class ScriptPromise;
class ScriptState;
class WebContentDecryptionModule;
class WebMediaPlayer;

class MODULES_EXPORT HTMLMediaElementEncryptedMedia final : public GarbageCollectedFinalized<HTMLMediaElementEncryptedMedia>, public Supplement<HTMLMediaElement>, public WebMediaPlayerEncryptedMediaClient {
    USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementEncryptedMedia);
public:
    static MediaKeys* mediaKeys(HTMLMediaElement&);
    static ScriptPromise setMediaKeys(ScriptState*, HTMLMediaElement&, MediaKeys*);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(encrypted);

    // WebMediaPlayerEncryptedMediaClient methods
    void encrypted(WebEncryptedMediaInitDataType, const unsigned char* initData, unsigned initDataLength) final;
    void didBlockPlaybackWaitingForKey() final;
    void didResumePlaybackBlockedForKey() final;
    WebContentDecryptionModule* contentDecryptionModule();

    static HTMLMediaElementEncryptedMedia& from(HTMLMediaElement&);
    static const char* supplementName();

    ~HTMLMediaElementEncryptedMedia();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class SetMediaKeysHandler;

    HTMLMediaElementEncryptedMedia(HTMLMediaElement&);

    // EventTarget
    bool setAttributeEventListener(const AtomicString& eventType, EventListener*);
    EventListener* getAttributeEventListener(const AtomicString& eventType);

    Member<HTMLMediaElement> m_mediaElement;

    // Internal values specified by the EME spec:
    // http://w3c.github.io/encrypted-media/#idl-def-HTMLMediaElement
    // The following internal values are added to the HTMLMediaElement:
    // - waiting for key, which shall have a boolean value
    // - attaching media keys, which shall have a boolean value
    bool m_isWaitingForKey;
    bool m_isAttachingMediaKeys;

    Member<MediaKeys> m_mediaKeys;
};

} // namespace blink

#endif
