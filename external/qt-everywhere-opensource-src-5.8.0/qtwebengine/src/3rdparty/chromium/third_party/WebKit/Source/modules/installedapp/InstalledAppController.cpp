// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installedapp/InstalledAppController.h"

#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebSecurityOrigin.h"

#include <utility>

namespace blink {

InstalledAppController::~InstalledAppController()
{
}

void InstalledAppController::provideTo(LocalFrame& frame, WebInstalledAppClient* client)
{
    ASSERT(RuntimeEnabledFeatures::installedAppEnabled());

    InstalledAppController* controller = new InstalledAppController(frame, client);
    Supplement<LocalFrame>::provideTo(frame, supplementName(), controller);
}

InstalledAppController* InstalledAppController::from(LocalFrame& frame)
{
    InstalledAppController* controller = static_cast<InstalledAppController*>(Supplement<LocalFrame>::from(frame, supplementName()));
    ASSERT(controller);
    return controller;
}

InstalledAppController::InstalledAppController(LocalFrame& frame, WebInstalledAppClient* client)
    : LocalFrameLifecycleObserver(&frame)
    , m_client(client)
{
}

const char* InstalledAppController::supplementName()
{
    return "InstalledAppController";
}

void InstalledAppController::getInstalledApps(const WebSecurityOrigin& url, std::unique_ptr<AppInstalledCallbacks> callback)
{
    // When detached, the client is no longer valid.
    if (!m_client) {
        callback.release()->onError();
        return;
    }

    // Client is expected to take ownership of the callback
    m_client->getInstalledRelatedApps(url, std::move(callback));
}

void InstalledAppController::willDetachFrameHost()
{
    m_client = nullptr;
}

DEFINE_TRACE(InstalledAppController)
{
    Supplement<LocalFrame>::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

} // namespace blink
