// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorServiceWorker_h
#define NavigatorServiceWorker_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class Document;
class Navigator;
class ServiceWorkerContainer;

class NavigatorServiceWorker FINAL : public NoBaseWillBeGarbageCollectedFinalized<NavigatorServiceWorker>, public WillBeHeapSupplement<Navigator>, DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorServiceWorker);
public:
    virtual ~NavigatorServiceWorker();
    static NavigatorServiceWorker* from(Document&);
    static NavigatorServiceWorker& from(Navigator&);
    static NavigatorServiceWorker* toNavigatorServiceWorker(Navigator&);
    static const char* supplementName();

    static ServiceWorkerContainer* serviceWorker(Navigator&);

    virtual void trace(Visitor* visitor) OVERRIDE { WillBeHeapSupplement<Navigator>::trace(visitor); }

private:
    explicit NavigatorServiceWorker(Navigator&);
    ServiceWorkerContainer* serviceWorker();

    // DOMWindowProperty override.
    virtual void willDetachGlobalObjectFromFrame() OVERRIDE;

    RefPtr<ServiceWorkerContainer> m_serviceWorker;
};

} // namespace WebCore

#endif // NavigatorServiceWorker_h
