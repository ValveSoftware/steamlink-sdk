// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerRegistrationSync_h
#define ServiceWorkerRegistrationSync_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class SyncManager;
class ServiceWorkerRegistration;

class ServiceWorkerRegistrationSync final : public GarbageCollectedFinalized<ServiceWorkerRegistrationSync>, public Supplement<ServiceWorkerRegistration> {
    USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerRegistrationSync);
    WTF_MAKE_NONCOPYABLE(ServiceWorkerRegistrationSync);
public:
    virtual ~ServiceWorkerRegistrationSync();
    static ServiceWorkerRegistrationSync& from(ServiceWorkerRegistration&);

    static SyncManager* sync(ServiceWorkerRegistration&);
    SyncManager* sync();

    DECLARE_VIRTUAL_TRACE();

private:
    explicit ServiceWorkerRegistrationSync(ServiceWorkerRegistration*);
    static const char* supplementName();

    Member<ServiceWorkerRegistration> m_registration;
    Member<SyncManager> m_syncManager;
};

} // namespace blink

#endif // ServiceWorkerRegistrationSync_h
