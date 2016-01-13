// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushController.h"

#include "public/platform/WebPushClient.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

PushController::PushController(blink::WebPushClient* client)
    : m_client(client)
{
}

PassOwnPtrWillBeRawPtr<PushController> PushController::create(blink::WebPushClient* client)
{
    return adoptPtrWillBeNoop(new PushController(client));
}

blink::WebPushClient* PushController::clientFrom(Page* page)
{
    if (PushController* controller = PushController::from(page))
        return controller->client();
    return 0;
}

const char* PushController::supplementName()
{
    return "PushController";
}

void providePushControllerTo(Page& page, blink::WebPushClient* client)
{
    PushController::provideTo(page, PushController::supplementName(), PushController::create(client));
}

} // namespace WebCore
