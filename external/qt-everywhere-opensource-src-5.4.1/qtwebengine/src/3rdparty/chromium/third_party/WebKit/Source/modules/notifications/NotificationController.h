/*
* Copyright (C) 2011 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef NotificationController_h
#define NotificationController_h

#include "core/frame/LocalFrame.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class NotificationClient;

class NotificationController FINAL : public NoBaseWillBeGarbageCollectedFinalized<NotificationController>, public WillBeHeapSupplement<LocalFrame> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NotificationController);
    WTF_MAKE_NONCOPYABLE(NotificationController);
public:
    virtual ~NotificationController();

    static PassOwnPtrWillBeRawPtr<NotificationController> create(PassOwnPtr<NotificationClient>);
    static const char* supplementName();
    static NotificationController* from(LocalFrame* frame) { return static_cast<NotificationController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName())); }
    static NotificationClient& clientFrom(LocalFrame*);

    virtual void trace(Visitor* visitor) OVERRIDE { WillBeHeapSupplement<LocalFrame>::trace(visitor); }

private:
    explicit NotificationController(PassOwnPtr<NotificationClient>);

    NotificationClient& client() { return *m_client; }

    OwnPtr<NotificationClient> m_client;
};

void provideNotification(LocalFrame&, PassOwnPtr<NotificationClient>);

} // namespace WebCore

#endif // NotificationController_h
