// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/NavigatorPushManager.h"

#include "core/dom/Document.h"
#include "core/frame/Navigator.h"
#include "modules/push_messaging/PushManager.h"

namespace WebCore {

NavigatorPushManager::NavigatorPushManager()
{
}

NavigatorPushManager::~NavigatorPushManager()
{
}

const char* NavigatorPushManager::supplementName()
{
    return "NavigatorPushManager";
}

NavigatorPushManager& NavigatorPushManager::from(Navigator& navigator)
{
    NavigatorPushManager* supplement = static_cast<NavigatorPushManager*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorPushManager();
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

PushManager* NavigatorPushManager::push(Navigator& navigator)
{
    return NavigatorPushManager::from(navigator).pushManager();
}

PushManager* NavigatorPushManager::pushManager()
{
    if (!m_pushManager)
        m_pushManager = PushManager::create();
    return m_pushManager.get();
}

void NavigatorPushManager::trace(Visitor* visitor)
{
    visitor->trace(m_pushManager);
    WillBeHeapSupplement<Navigator>::trace(visitor);
}

} // namespace WebCore
