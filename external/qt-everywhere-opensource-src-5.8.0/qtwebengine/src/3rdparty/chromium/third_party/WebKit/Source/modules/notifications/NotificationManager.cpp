// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationManager.h"

#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"

namespace blink {

// static
NotificationManager* NotificationManager::from(ExecutionContext* executionContext)
{
    DCHECK(executionContext);
    DCHECK(executionContext->isContextThread());

    NotificationManager* manager = static_cast<NotificationManager*>(Supplement<ExecutionContext>::from(executionContext, supplementName()));
    if (!manager) {
        manager = new NotificationManager(executionContext);
        Supplement<ExecutionContext>::provideTo(*executionContext, supplementName(), manager);
    }

    return manager;
}

// static
const char* NotificationManager::supplementName()
{
    return "NotificationManager";
}

NotificationManager::NotificationManager(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext)
{
    Platform::current()->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_service));
}

NotificationManager::~NotificationManager()
{
}

mojom::blink::PermissionStatus NotificationManager::permissionStatus() const
{
    mojom::blink::PermissionStatus permissionStatus;

    const bool result =
        m_service->GetPermissionStatus(getExecutionContext()->getSecurityOrigin()->toString(), &permissionStatus);
    DCHECK(result);

    return permissionStatus;
}

void NotificationManager::contextDestroyed()
{
    m_service.reset();
}

DEFINE_TRACE(NotificationManager)
{
    ContextLifecycleObserver::trace(visitor);
    Supplement<ExecutionContext>::trace(visitor);
}

} // namespace blink
