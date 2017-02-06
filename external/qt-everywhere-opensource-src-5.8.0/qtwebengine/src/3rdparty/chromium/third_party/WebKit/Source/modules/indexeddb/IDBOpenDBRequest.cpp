/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBOpenDBRequest.h"

#include "bindings/core/v8/Nullable.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBVersionChangeEvent.h"
#include <memory>

using blink::WebIDBDatabase;

namespace blink {

IDBOpenDBRequest* IDBOpenDBRequest::create(ScriptState* scriptState, IDBDatabaseCallbacks* callbacks, int64_t transactionId, int64_t version)
{
    IDBOpenDBRequest* request = new IDBOpenDBRequest(scriptState, callbacks, transactionId, version);
    request->suspendIfNeeded();
    return request;
}

IDBOpenDBRequest::IDBOpenDBRequest(ScriptState* scriptState, IDBDatabaseCallbacks* callbacks, int64_t transactionId, int64_t version)
    : IDBRequest(scriptState, IDBAny::createNull(), nullptr)
    , m_databaseCallbacks(callbacks)
    , m_transactionId(transactionId)
    , m_version(version)
{
    ASSERT(!resultAsAny());
}

IDBOpenDBRequest::~IDBOpenDBRequest()
{
}

DEFINE_TRACE(IDBOpenDBRequest)
{
    visitor->trace(m_databaseCallbacks);
    IDBRequest::trace(visitor);
}

const AtomicString& IDBOpenDBRequest::interfaceName() const
{
    return EventTargetNames::IDBOpenDBRequest;
}

void IDBOpenDBRequest::onBlocked(int64_t oldVersion)
{
    IDB_TRACE("IDBOpenDBRequest::onBlocked()");
    if (!shouldEnqueueEvent())
        return;
    Nullable<unsigned long long> newVersionNullable = (m_version == IDBDatabaseMetadata::DefaultVersion) ? Nullable<unsigned long long>() : Nullable<unsigned long long>(m_version);
    enqueueEvent(IDBVersionChangeEvent::create(EventTypeNames::blocked, oldVersion, newVersionNullable));
}

void IDBOpenDBRequest::onUpgradeNeeded(int64_t oldVersion, std::unique_ptr<WebIDBDatabase> backend, const IDBDatabaseMetadata& metadata, WebIDBDataLoss dataLoss, String dataLossMessage)
{
    IDB_TRACE("IDBOpenDBRequest::onUpgradeNeeded()");
    if (m_contextStopped || !getExecutionContext()) {
        std::unique_ptr<WebIDBDatabase> db = std::move(backend);
        db->abort(m_transactionId);
        db->close();
        return;
    }
    if (!shouldEnqueueEvent())
        return;

    ASSERT(m_databaseCallbacks);

    IDBDatabase* idbDatabase = IDBDatabase::create(getExecutionContext(), std::move(backend), m_databaseCallbacks.release());
    idbDatabase->setMetadata(metadata);

    if (oldVersion == IDBDatabaseMetadata::NoVersion) {
        // This database hasn't had a version before.
        oldVersion = IDBDatabaseMetadata::DefaultVersion;
    }
    IDBDatabaseMetadata oldMetadata(metadata);
    oldMetadata.version = oldVersion;

    m_transaction = IDBTransaction::create(getScriptState(), m_transactionId, idbDatabase, this, oldMetadata);
    setResult(IDBAny::create(idbDatabase));

    if (m_version == IDBDatabaseMetadata::NoVersion)
        m_version = 1;
    enqueueEvent(IDBVersionChangeEvent::create(EventTypeNames::upgradeneeded, oldVersion, m_version, dataLoss, dataLossMessage));
}

void IDBOpenDBRequest::onSuccess(std::unique_ptr<WebIDBDatabase> backend, const IDBDatabaseMetadata& metadata)
{
    IDB_TRACE("IDBOpenDBRequest::onSuccess()");
    if (m_contextStopped || !getExecutionContext()) {
        std::unique_ptr<WebIDBDatabase> db = std::move(backend);
        if (db)
            db->close();
        return;
    }
    if (!shouldEnqueueEvent())
        return;

    IDBDatabase* idbDatabase = nullptr;
    if (resultAsAny()) {
        // Previous onUpgradeNeeded call delivered the backend.
        ASSERT(!backend.get());
        idbDatabase = resultAsAny()->idbDatabase();
        ASSERT(idbDatabase);
        ASSERT(!m_databaseCallbacks);
    } else {
        ASSERT(backend.get());
        ASSERT(m_databaseCallbacks);
        idbDatabase = IDBDatabase::create(getExecutionContext(), std::move(backend), m_databaseCallbacks.release());
        setResult(IDBAny::create(idbDatabase));
    }
    idbDatabase->setMetadata(metadata);
    enqueueEvent(Event::create(EventTypeNames::success));
}

void IDBOpenDBRequest::onSuccess(int64_t oldVersion)
{
    IDB_TRACE("IDBOpenDBRequest::onSuccess()");
    if (!shouldEnqueueEvent())
        return;
    if (oldVersion == IDBDatabaseMetadata::NoVersion) {
        // This database hasn't had an integer version before.
        oldVersion = IDBDatabaseMetadata::DefaultVersion;
    }
    setResult(IDBAny::createUndefined());
    enqueueEvent(IDBVersionChangeEvent::create(EventTypeNames::success, oldVersion, Nullable<unsigned long long>()));
}

bool IDBOpenDBRequest::shouldEnqueueEvent() const
{
    if (m_contextStopped || !getExecutionContext())
        return false;
    ASSERT(m_readyState == PENDING || m_readyState == DONE);
    if (m_requestAborted)
        return false;
    return true;
}

DispatchEventResult IDBOpenDBRequest::dispatchEventInternal(Event* event)
{
    // If the connection closed between onUpgradeNeeded and the delivery of the "success" event,
    // an "error" event should be fired instead.
    if (event->type() == EventTypeNames::success && resultAsAny()->getType() == IDBAny::IDBDatabaseType && resultAsAny()->idbDatabase()->isClosePending()) {
        dequeueEvent(event);
        setResult(nullptr);
        onError(DOMException::create(AbortError, "The connection was closed."));
        return DispatchEventResult::CanceledBeforeDispatch;
    }

    return IDBRequest::dispatchEventInternal(event);
}

} // namespace blink
