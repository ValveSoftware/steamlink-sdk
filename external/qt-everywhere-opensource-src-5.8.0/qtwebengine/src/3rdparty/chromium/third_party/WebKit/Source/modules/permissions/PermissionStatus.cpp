// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionStatus.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/permissions/PermissionController.h"
#include "modules/permissions/Permissions.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/permissions/WebPermissionClient.h"

namespace blink {

// static
PermissionStatus* PermissionStatus::take(ScriptPromiseResolver* resolver, WebPermissionStatus status, WebPermissionType type)
{
    return PermissionStatus::createAndListen(resolver->getExecutionContext(), status, type);
}

PermissionStatus* PermissionStatus::createAndListen(ExecutionContext* executionContext, WebPermissionStatus status, WebPermissionType type)
{
    PermissionStatus* permissionStatus = new PermissionStatus(executionContext, status, type);
    permissionStatus->suspendIfNeeded();
    permissionStatus->startListening();
    return permissionStatus;
}

PermissionStatus::PermissionStatus(ExecutionContext* executionContext, WebPermissionStatus status, WebPermissionType type)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(executionContext)
    , m_status(status)
    , m_type(type)
    , m_listening(false)
{
}

PermissionStatus::~PermissionStatus()
{
    stopListening();
}

const AtomicString& PermissionStatus::interfaceName() const
{
    return EventTargetNames::PermissionStatus;
}

ExecutionContext* PermissionStatus::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

void PermissionStatus::permissionChanged(WebPermissionType type, WebPermissionStatus status)
{
    ASSERT(m_type == type);
    if (m_status == status)
        return;

    m_status = status;
    dispatchEvent(Event::create(EventTypeNames::change));
}

bool PermissionStatus::hasPendingActivity() const
{
    return m_listening;
}

void PermissionStatus::resume()
{
    startListening();
}

void PermissionStatus::suspend()
{
    stopListening();
}

void PermissionStatus::stop()
{
    stopListening();
}

void PermissionStatus::startListening()
{
    ASSERT(!m_listening);

    WebPermissionClient* client = Permissions::getClient(getExecutionContext());
    if (!client)
        return;
    m_listening = true;
    client->startListening(m_type, KURL(KURL(), getExecutionContext()->getSecurityOrigin()->toString()), this);
}

void PermissionStatus::stopListening()
{
    if (!m_listening)
        return;

    ASSERT(getExecutionContext());

    m_listening = false;
    WebPermissionClient* client = Permissions::getClient(getExecutionContext());
    if (!client)
        return;
    client->stopListening(this);
}

String PermissionStatus::state() const
{
    switch (m_status) {
    case WebPermissionStatusGranted:
        return "granted";
    case WebPermissionStatusDenied:
        return "denied";
    case WebPermissionStatusPrompt:
        return "prompt";
    }

    ASSERT_NOT_REACHED();
    return "denied";
}

DEFINE_TRACE(PermissionStatus)
{
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
