/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "modules/indexeddb/IDBCursor.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ScriptCallStack.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/WebIDBCallbacksImpl.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebIDBDatabase.h"
#include "public/platform/WebIDBKeyRange.h"
#include <limits>

using blink::WebIDBCursor;
using blink::WebIDBDatabase;

namespace WebCore {

IDBCursor* IDBCursor::create(PassOwnPtr<blink::WebIDBCursor> backend, blink::WebIDBCursorDirection direction, IDBRequest* request, IDBAny* source, IDBTransaction* transaction)
{
    return new IDBCursor(backend, direction, request, source, transaction);
}

const AtomicString& IDBCursor::directionNext()
{
    DEFINE_STATIC_LOCAL(AtomicString, next, ("next", AtomicString::ConstructFromLiteral));
    return next;
}

const AtomicString& IDBCursor::directionNextUnique()
{
    DEFINE_STATIC_LOCAL(AtomicString, nextunique, ("nextunique", AtomicString::ConstructFromLiteral));
    return nextunique;
}

const AtomicString& IDBCursor::directionPrev()
{
    DEFINE_STATIC_LOCAL(AtomicString, prev, ("prev", AtomicString::ConstructFromLiteral));
    return prev;
}

const AtomicString& IDBCursor::directionPrevUnique()
{
    DEFINE_STATIC_LOCAL(AtomicString, prevunique, ("prevunique", AtomicString::ConstructFromLiteral));
    return prevunique;
}

IDBCursor::IDBCursor(PassOwnPtr<blink::WebIDBCursor> backend, blink::WebIDBCursorDirection direction, IDBRequest* request, IDBAny* source, IDBTransaction* transaction)
    : m_backend(backend)
    , m_request(request)
    , m_direction(direction)
    , m_source(source)
    , m_transaction(transaction)
    , m_gotValue(false)
    , m_keyDirty(true)
    , m_primaryKeyDirty(true)
    , m_valueDirty(true)
{
    ASSERT(m_backend);
    ASSERT(m_request);
    ASSERT(m_source->type() == IDBAny::IDBObjectStoreType || m_source->type() == IDBAny::IDBIndexType);
    ASSERT(m_transaction);
    ScriptWrappable::init(this);
}

IDBCursor::~IDBCursor()
{
    handleBlobAcks();
}

void IDBCursor::trace(Visitor* visitor)
{
    visitor->trace(m_request);
    visitor->trace(m_source);
    visitor->trace(m_transaction);
    visitor->trace(m_key);
    visitor->trace(m_primaryKey);
}

IDBRequest* IDBCursor::update(ScriptState* scriptState, ScriptValue& value, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBCursor::update");

    if (!m_gotValue) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return 0;
    }
    if (isKeyCursor()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::isKeyCursorErrorMessage);
        return 0;
    }
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
        return 0;
    }
    if (m_transaction->isFinished() || m_transaction->isFinishing()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    if (m_transaction->isReadOnly()) {
        exceptionState.throwDOMException(ReadOnlyError, "The record may not be updated inside a read-only transaction.");
        return 0;
    }

    IDBObjectStore* objectStore = effectiveObjectStore();
    const IDBKeyPath& keyPath = objectStore->metadata().keyPath;
    const bool usesInLineKeys = !keyPath.isNull();
    if (usesInLineKeys) {
        IDBKey* keyPathKey = createIDBKeyFromScriptValueAndKeyPath(scriptState->isolate(), value, keyPath);
        if (!keyPathKey || !keyPathKey->isEqual(m_primaryKey.get())) {
            exceptionState.throwDOMException(DataError, "The effective object store of this cursor uses in-line keys and evaluating the key path of the value parameter results in a different value than the cursor's effective key.");
            return 0;
        }
    }

    return objectStore->put(scriptState, blink::WebIDBPutModeCursorUpdate, IDBAny::create(this), value, m_primaryKey, exceptionState);
}

void IDBCursor::advance(unsigned long count, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBCursor::advance");
    if (!count) {
        exceptionState.throwTypeError("A count argument with value 0 (zero) was supplied, must be greater than 0.");
        return;
    }
    if (!m_gotValue) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return;
    }
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
        return;
    }

    if (m_transaction->isFinished() || m_transaction->isFinishing()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }

    m_request->setPendingCursor(this);
    m_gotValue = false;
    m_backend->advance(count, WebIDBCallbacksImpl::create(m_request).leakPtr());
}

void IDBCursor::continueFunction(ScriptState* scriptState, const ScriptValue& keyValue, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBCursor::continue");
    IDBKey* key = keyValue.isUndefined() || keyValue.isNull() ? nullptr : scriptValueToIDBKey(scriptState->isolate(), keyValue);
    if (key && !key->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return;
    }
    continueFunction(key, 0, exceptionState);
}

void IDBCursor::continuePrimaryKey(ScriptState* scriptState, const ScriptValue& keyValue, const ScriptValue& primaryKeyValue, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBCursor::continuePrimaryKey");
    IDBKey* key = scriptValueToIDBKey(scriptState->isolate(), keyValue);
    IDBKey* primaryKey = scriptValueToIDBKey(scriptState->isolate(), primaryKeyValue);
    if (!key->isValid() || !primaryKey->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return;
    }
    continueFunction(key, primaryKey, exceptionState);
}

void IDBCursor::continueFunction(IDBKey* key, IDBKey* primaryKey, ExceptionState& exceptionState)
{
    ASSERT(!primaryKey || (key && primaryKey));

    if (m_transaction->isFinished() || m_transaction->isFinishing()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }

    if (!m_gotValue) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return;
    }

    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
        return;
    }

    if (key) {
        ASSERT(m_key);
        if (m_direction == blink::WebIDBCursorDirectionNext || m_direction == blink::WebIDBCursorDirectionNextNoDuplicate) {
            const bool ok = m_key->isLessThan(key)
                || (primaryKey && m_key->isEqual(key) && m_primaryKey->isLessThan(primaryKey));
            if (!ok) {
                exceptionState.throwDOMException(DataError, "The parameter is less than or equal to this cursor's position.");
                return;
            }

        } else {
            const bool ok = key->isLessThan(m_key.get())
                || (primaryKey && key->isEqual(m_key.get()) && primaryKey->isLessThan(m_primaryKey.get()));
            if (!ok) {
                exceptionState.throwDOMException(DataError, "The parameter is greater than or equal to this cursor's position.");
                return;
            }
        }
    }

    // FIXME: We're not using the context from when continue was called, which means the callback
    //        will be on the original context openCursor was called on. Is this right?
    m_request->setPendingCursor(this);
    m_gotValue = false;
    m_backend->continueFunction(key, primaryKey, WebIDBCallbacksImpl::create(m_request).leakPtr());
}

IDBRequest* IDBCursor::deleteFunction(ScriptState* scriptState, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBCursor::delete");
    if (m_transaction->isFinished() || m_transaction->isFinishing()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    if (m_transaction->isReadOnly()) {
        exceptionState.throwDOMException(ReadOnlyError, "The record may not be deleted inside a read-only transaction.");
        return 0;
    }

    if (!m_gotValue) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return 0;
    }
    if (isKeyCursor()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::isKeyCursorErrorMessage);
        return 0;
    }
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
        return 0;
    }
    if (!m_transaction->backendDB()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::databaseClosedErrorMessage);
        return 0;
    }

    IDBKeyRange* keyRange = IDBKeyRange::only(m_primaryKey, exceptionState);
    ASSERT(!exceptionState.hadException());

    IDBRequest* request = IDBRequest::create(scriptState, IDBAny::create(this), m_transaction.get());
    m_transaction->backendDB()->deleteRange(m_transaction->id(), effectiveObjectStore()->id(), keyRange, WebIDBCallbacksImpl::create(request).leakPtr());
    return request;
}

void IDBCursor::postSuccessHandlerCallback()
{
    if (m_backend)
        m_backend->postSuccessHandlerCallback();
}

void IDBCursor::close()
{
    handleBlobAcks();
    m_request.clear();
    m_backend.clear();
}

ScriptValue IDBCursor::key(ScriptState* scriptState)
{
    m_keyDirty = false;
    return idbKeyToScriptValue(scriptState, m_key);
}

ScriptValue IDBCursor::primaryKey(ScriptState* scriptState)
{
    m_primaryKeyDirty = false;
    return idbKeyToScriptValue(scriptState, m_primaryKey);
}

ScriptValue IDBCursor::value(ScriptState* scriptState)
{
    ASSERT(isCursorWithValue());

    IDBObjectStore* objectStore = effectiveObjectStore();
    const IDBObjectStoreMetadata& metadata = objectStore->metadata();
    IDBAny* value;
    if (metadata.autoIncrement && !metadata.keyPath.isNull()) {
        value = IDBAny::create(m_value, m_blobInfo.get(), m_primaryKey, metadata.keyPath);
#ifndef NDEBUG
        assertPrimaryKeyValidOrInjectable(scriptState, m_value, m_blobInfo.get(), m_primaryKey, metadata.keyPath);
#endif
    } else {
        value = IDBAny::create(m_value, m_blobInfo.get());
    }

    m_valueDirty = false;
    ScriptValue scriptValue = idbAnyToScriptValue(scriptState, value);
    handleBlobAcks();
    return scriptValue;
}

ScriptValue IDBCursor::source(ScriptState* scriptState) const
{
    return idbAnyToScriptValue(scriptState, m_source);
}

void IDBCursor::setValueReady(IDBKey* key, IDBKey* primaryKey, PassRefPtr<SharedBuffer> value, PassOwnPtr<Vector<blink::WebBlobInfo> > blobInfo)
{
    m_key = key;
    m_keyDirty = true;

    m_primaryKey = primaryKey;
    m_primaryKeyDirty = true;

    if (isCursorWithValue()) {
        m_value = value;
        handleBlobAcks();
        m_blobInfo = blobInfo;
        m_valueDirty = true;
    }

    m_gotValue = true;
}

IDBObjectStore* IDBCursor::effectiveObjectStore() const
{
    if (m_source->type() == IDBAny::IDBObjectStoreType)
        return m_source->idbObjectStore();
    return m_source->idbIndex()->objectStore();
}

bool IDBCursor::isDeleted() const
{
    if (m_source->type() == IDBAny::IDBObjectStoreType)
        return m_source->idbObjectStore()->isDeleted();
    return m_source->idbIndex()->isDeleted();
}

void IDBCursor::handleBlobAcks()
{
    ASSERT(m_request || !m_blobInfo || !m_blobInfo->size());
    if (m_blobInfo.get() && m_blobInfo->size()) {
        ASSERT(m_request);
        m_transaction->db()->ackReceivedBlobs(m_blobInfo.get());
        m_blobInfo.clear();
    }
}

blink::WebIDBCursorDirection IDBCursor::stringToDirection(const String& directionString, ExceptionState& exceptionState)
{
    if (directionString.isNull() || directionString == IDBCursor::directionNext())
        return blink::WebIDBCursorDirectionNext;
    if (directionString == IDBCursor::directionNextUnique())
        return blink::WebIDBCursorDirectionNextNoDuplicate;
    if (directionString == IDBCursor::directionPrev())
        return blink::WebIDBCursorDirectionPrev;
    if (directionString == IDBCursor::directionPrevUnique())
        return blink::WebIDBCursorDirectionPrevNoDuplicate;

    exceptionState.throwTypeError("The direction provided ('" + directionString + "') is not one of 'next', 'nextunique', 'prev', or 'prevunique'.");
    return blink::WebIDBCursorDirectionNext;
}

const AtomicString& IDBCursor::directionToString(unsigned short direction)
{
    switch (direction) {
    case blink::WebIDBCursorDirectionNext:
        return IDBCursor::directionNext();

    case blink::WebIDBCursorDirectionNextNoDuplicate:
        return IDBCursor::directionNextUnique();

    case blink::WebIDBCursorDirectionPrev:
        return IDBCursor::directionPrev();

    case blink::WebIDBCursorDirectionPrevNoDuplicate:
        return IDBCursor::directionPrevUnique();

    default:
        ASSERT_NOT_REACHED();
        return IDBCursor::directionNext();
    }
}

} // namespace WebCore
