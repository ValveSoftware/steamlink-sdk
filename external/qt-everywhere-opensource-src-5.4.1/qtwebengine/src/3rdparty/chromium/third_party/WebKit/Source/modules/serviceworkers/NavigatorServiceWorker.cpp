// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"

namespace WebCore {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator& navigator)
    : DOMWindowProperty(navigator.frame())
{
}

NavigatorServiceWorker::~NavigatorServiceWorker()
{
}

NavigatorServiceWorker* NavigatorServiceWorker::from(Document& document)
{
    if (!document.frame() || !document.frame()->domWindow())
        return 0;
    Navigator& navigator = document.frame()->domWindow()->navigator();
    return &from(navigator);
}

NavigatorServiceWorker& NavigatorServiceWorker::from(Navigator& navigator)
{
    NavigatorServiceWorker* supplement = toNavigatorServiceWorker(navigator);
    if (!supplement) {
        supplement = new NavigatorServiceWorker(navigator);
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
        // Initialize ServiceWorkerContainer too.
        supplement->serviceWorker();
    }
    return *supplement;
}

NavigatorServiceWorker* NavigatorServiceWorker::toNavigatorServiceWorker(Navigator& navigator)
{
    return static_cast<NavigatorServiceWorker*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
}

const char* NavigatorServiceWorker::supplementName()
{
    return "NavigatorServiceWorker";
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(Navigator& navigator)
{
    return NavigatorServiceWorker::from(navigator).serviceWorker();
}

ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker()
{
    if (!m_serviceWorker && frame()) {
        ASSERT(frame()->domWindow());
        m_serviceWorker = ServiceWorkerContainer::create(frame()->domWindow()->executionContext());
    }
    return m_serviceWorker.get();
}

void NavigatorServiceWorker::willDetachGlobalObjectFromFrame()
{
    if (m_serviceWorker) {
        m_serviceWorker->detachClient();
        m_serviceWorker = nullptr;
    }
}

} // namespace WebCore
