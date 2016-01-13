// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementEncryptedMedia_h
#define HTMLMediaElementEncryptedMedia_h

#include "modules/EventTargetModules.h"
#include "platform/Supplementable.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebMediaPlayerClient.h"
#include "wtf/Forward.h"

namespace WebCore {

class ExceptionState;
class HTMLMediaElement;
class MediaKeys;

class HTMLMediaElementEncryptedMedia FINAL : public NoBaseWillBeGarbageCollected<HTMLMediaElementEncryptedMedia>, public WillBeHeapSupplement<HTMLMediaElement> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementEncryptedMedia);
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(HTMLMediaElementEncryptedMedia);
public:
    // encrypted media extensions (v0.1b)
    static void webkitGenerateKeyRequest(HTMLMediaElement&, const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState&);
    static void webkitGenerateKeyRequest(HTMLMediaElement&, const String& keySystem, ExceptionState&);
    static void webkitAddKey(HTMLMediaElement&, const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState&);
    static void webkitAddKey(HTMLMediaElement&, const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionState&);
    static void webkitCancelKeyRequest(HTMLMediaElement&, const String& keySystem, const String& sessionId, ExceptionState&);

    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitkeyadded);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitkeyerror);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitkeymessage);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(webkitneedkey);

    // encrypted media extensions (WD)
    static MediaKeys* mediaKeys(HTMLMediaElement&);
    static void setMediaKeys(HTMLMediaElement&, MediaKeys*, ExceptionState&);
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(needkey);

    static void keyAdded(HTMLMediaElement&, const String& keySystem, const String& sessionId);
    static void keyError(HTMLMediaElement&, const String& keySystem, const String& sessionId, blink::WebMediaPlayerClient::MediaKeyErrorCode, unsigned short systemCode);
    static void keyMessage(HTMLMediaElement&, const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const blink::WebURL& defaultURL);
    static void keyNeeded(HTMLMediaElement&, const String& contentType, const unsigned char* initData, unsigned initDataLength);
    static void playerDestroyed(HTMLMediaElement&);
    static blink::WebContentDecryptionModule* contentDecryptionModule(HTMLMediaElement&);

    static HTMLMediaElementEncryptedMedia& from(HTMLMediaElement&);
    static const char* supplementName();

    virtual void trace(Visitor*) OVERRIDE;

private:
    HTMLMediaElementEncryptedMedia();
    void generateKeyRequest(blink::WebMediaPlayer*, const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState&);
    void addKey(blink::WebMediaPlayer*, const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState&);
    void cancelKeyRequest(blink::WebMediaPlayer*, const String& keySystem, const String& sessionId, ExceptionState&);

    // EventTarget
    bool setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
    EventListener* getAttributeEventListener(const AtomicString& eventType);

    // Currently we have both EME v0.1b and EME WD implemented in media element.
    // But we do not want to support both at the same time. The one used first
    // will be supported. Use |m_emeMode| to track this selection.
    // FIXME: Remove EmeMode once EME v0.1b support is removed. See crbug.com/249976.
    enum EmeMode { EmeModeNotSelected, EmeModePrefixed, EmeModeUnprefixed };

    // check (and set if necessary) the encrypted media extensions (EME) mode
    // (v0.1b or WD). Returns whether the mode is allowed and successfully set.
    bool setEmeMode(EmeMode, ExceptionState&);

    blink::WebContentDecryptionModule* contentDecryptionModule();
    void setMediaKeysInternal(HTMLMediaElement&, MediaKeys*);

    EmeMode m_emeMode;

    PersistentWillBeMember<MediaKeys> m_mediaKeys;
};

}

#endif
