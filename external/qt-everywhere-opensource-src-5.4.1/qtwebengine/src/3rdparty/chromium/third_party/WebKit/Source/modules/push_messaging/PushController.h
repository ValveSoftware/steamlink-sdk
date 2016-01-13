// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushController_h
#define PushController_h

#include "core/page/Page.h"
#include "platform/Supplementable.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
class WebPushClient;
} // namespace blink

namespace WebCore {

class PushController FINAL : public NoBaseWillBeGarbageCollected<PushController>, public WillBeHeapSupplement<Page> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PushController);
    WTF_MAKE_NONCOPYABLE(PushController);

public:
    static PassOwnPtrWillBeRawPtr<PushController> create(blink::WebPushClient*);
    static const char* supplementName();
    static PushController* from(Page* page) { return static_cast<PushController*>(WillBeHeapSupplement<Page>::from(page, supplementName())); }
    static blink::WebPushClient* clientFrom(Page*);

    blink::WebPushClient* client() const { return m_client; }

    virtual void trace(Visitor* visitor) OVERRIDE { WillBeHeapSupplement<Page>::trace(visitor); }

private:
    explicit PushController(blink::WebPushClient*);

    blink::WebPushClient* m_client;
};

void providePushControllerTo(Page&, blink::WebPushClient*);

} // namespace WebCore

#endif // PushController_h
