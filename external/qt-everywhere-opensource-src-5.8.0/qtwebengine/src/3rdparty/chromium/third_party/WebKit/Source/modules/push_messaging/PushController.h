// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushController_h
#define PushController_h

#include "core/frame/LocalFrame.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class WebPushClient;

class PushController final : public GarbageCollected<PushController>, public Supplement<LocalFrame> {
    USING_GARBAGE_COLLECTED_MIXIN(PushController);
    WTF_MAKE_NONCOPYABLE(PushController);
public:
    static PushController* create(WebPushClient*);
    static const char* supplementName();
    static PushController* from(LocalFrame* frame) { return static_cast<PushController*>(Supplement<LocalFrame>::from(frame, supplementName())); }
    static WebPushClient& clientFrom(LocalFrame*);

    DEFINE_INLINE_VIRTUAL_TRACE() { Supplement<LocalFrame>::trace(visitor); }

private:
    explicit PushController(WebPushClient*);

    WebPushClient* client() const { return m_client; }

    WebPushClient* m_client;
};

MODULES_EXPORT void providePushControllerTo(LocalFrame&, WebPushClient*);

} // namespace blink

#endif // PushController_h
