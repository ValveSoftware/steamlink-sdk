/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/webdatabase/DatabaseSync.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "platform/Logging.h"
#include "modules/webdatabase/DatabaseCallback.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLTransactionSync.h"
#include "modules/webdatabase/SQLTransactionSyncCallback.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/CString.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<DatabaseSync> DatabaseSync::create(ExecutionContext*, PassRefPtrWillBeRawPtr<DatabaseBackendBase> backend)
{
    return static_cast<DatabaseSync*>(backend.get());
}

DatabaseSync::DatabaseSync(DatabaseContext* databaseContext,
    const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : DatabaseBackendSync(databaseContext, name, expectedVersion, displayName, estimatedSize)
    , DatabaseBase(databaseContext->executionContext())
{
    ScriptWrappable::init(this);
    setFrontend(this);
}

DatabaseSync::~DatabaseSync()
{
    if (executionContext())
        ASSERT(executionContext()->isContextThread());
}

void DatabaseSync::trace(Visitor* visitor)
{
    DatabaseBackendSync::trace(visitor);
}

void DatabaseSync::changeVersion(const String& oldVersion, const String& newVersion, PassOwnPtr<SQLTransactionSyncCallback> changeVersionCallback, ExceptionState& exceptionState)
{
    ASSERT(executionContext()->isContextThread());

    if (sqliteDatabase().transactionInProgress()) {
        reportChangeVersionResult(1, SQLError::DATABASE_ERR, 0);
        setLastErrorMessage("unable to changeVersion from within a transaction");
        exceptionState.throwDOMException(SQLDatabaseError, "Unable to change version from within a transaction.");
        return;
    }

    RefPtrWillBeRawPtr<SQLTransactionSync> transaction = SQLTransactionSync::create(this, changeVersionCallback, false);
    transaction->begin(exceptionState);
    if (exceptionState.hadException()) {
        ASSERT(!lastErrorMessage().isEmpty());
        return;
    }

    String actualVersion;
    if (!getVersionFromDatabase(actualVersion)) {
        reportChangeVersionResult(2, SQLError::UNKNOWN_ERR, sqliteDatabase().lastError());
        setLastErrorMessage("unable to read the current version", sqliteDatabase().lastError(), sqliteDatabase().lastErrorMsg());
        exceptionState.throwDOMException(UnknownError, SQLError::unknownErrorMessage);
        return;
    }

    if (actualVersion != oldVersion) {
        reportChangeVersionResult(3, SQLError::VERSION_ERR, 0);
        setLastErrorMessage("current version of the database and `oldVersion` argument do not match");
        exceptionState.throwDOMException(VersionError, SQLError::versionErrorMessage);
        return;
    }


    transaction->execute(exceptionState);
    if (exceptionState.hadException()) {
        ASSERT(!lastErrorMessage().isEmpty());
        return;
    }

    if (!setVersionInDatabase(newVersion)) {
        reportChangeVersionResult(4, SQLError::UNKNOWN_ERR, sqliteDatabase().lastError());
        setLastErrorMessage("unable to set the new version", sqliteDatabase().lastError(), sqliteDatabase().lastErrorMsg());
        exceptionState.throwDOMException(UnknownError, SQLError::unknownErrorMessage);
        return;
    }

    transaction->commit(exceptionState);
    if (exceptionState.hadException()) {
        ASSERT(!lastErrorMessage().isEmpty());
        setCachedVersion(oldVersion);
        return;
    }

    reportChangeVersionResult(0, -1, 0); // OK

    setExpectedVersion(newVersion);
    setLastErrorMessage("");
}

void DatabaseSync::transaction(PassOwnPtr<SQLTransactionSyncCallback> callback, ExceptionState& exceptionState)
{
    runTransaction(callback, false, exceptionState);
}

void DatabaseSync::readTransaction(PassOwnPtr<SQLTransactionSyncCallback> callback, ExceptionState& exceptionState)
{
    runTransaction(callback, true, exceptionState);
}

void DatabaseSync::rollbackTransaction(SQLTransactionSync& transaction)
{
    ASSERT(!lastErrorMessage().isEmpty());
    transaction.rollback();
    setLastErrorMessage("");
    return;
}

void DatabaseSync::runTransaction(PassOwnPtr<SQLTransactionSyncCallback> callback, bool readOnly, ExceptionState& exceptionState)
{
    ASSERT(executionContext()->isContextThread());

    if (sqliteDatabase().transactionInProgress()) {
        setLastErrorMessage("unable to start a transaction from within a transaction");
        exceptionState.throwDOMException(SQLDatabaseError, "Unable to start a transaction from within a transaction.");
        return;
    }

    RefPtrWillBeRawPtr<SQLTransactionSync> transaction = SQLTransactionSync::create(this, callback, readOnly);
    transaction->begin(exceptionState);
    if (exceptionState.hadException()) {
        rollbackTransaction(*transaction);
        return;
    }

    transaction->execute(exceptionState);
    if (exceptionState.hadException()) {
        rollbackTransaction(*transaction);
        return;
    }

    transaction->commit(exceptionState);
    if (exceptionState.hadException()) {
        rollbackTransaction(*transaction);
        return;
    }

    setLastErrorMessage("");
}

void DatabaseSync::closeImmediately()
{
    ASSERT(executionContext()->isContextThread());

    if (!opened())
        return;

    logErrorMessage("forcibly closing database");
    closeDatabase();
}

void DatabaseSync::observeTransaction(SQLTransactionSync& transaction)
{
#if ENABLE(OILPAN)
    m_observers.add(&transaction, adoptPtr(new TransactionObserver(transaction)));
#endif
}

#if ENABLE(OILPAN)
DatabaseSync::TransactionObserver::~TransactionObserver()
{
    m_transaction.rollbackIfInProgress();
}
#endif

} // namespace WebCore
