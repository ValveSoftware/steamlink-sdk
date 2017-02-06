// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationPermissionClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"

namespace blink {

const char* NotificationPermissionClient::supplementName()
{
    return "NotificationPermissionClient";
}

NotificationPermissionClient* NotificationPermissionClient::from(ExecutionContext* context)
{
    if (!context->isDocument())
        return nullptr;

    const Document* document = toDocument(context);
    if (!document->frame())
        return nullptr;

    return static_cast<NotificationPermissionClient*>(Supplement<LocalFrame>::from(document->frame(), supplementName()));
}

void provideNotificationPermissionClientTo(LocalFrame& frame, NotificationPermissionClient* client)
{
    frame.provideSupplement(NotificationPermissionClient::supplementName(), client);
}

} // namespace blink
