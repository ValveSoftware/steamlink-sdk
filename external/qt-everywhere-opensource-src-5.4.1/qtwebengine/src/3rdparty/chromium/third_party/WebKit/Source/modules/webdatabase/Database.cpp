/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "modules/webdatabase/Database.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/html/VoidCallback.h"
#include "core/page/Page.h"
#include "platform/Logging.h"
#include "modules/webdatabase/sqlite/SQLiteStatement.h"
#include "modules/webdatabase/ChangeVersionData.h"
#include "modules/webdatabase/DatabaseCallback.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "modules/webdatabase/DatabaseThread.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "modules/webdatabase/SQLTransactionCallback.h"
#include "modules/webdatabase/SQLTransactionErrorCallback.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<Database> Database::create(ExecutionContext*, PassRefPtrWillBeRawPtr<DatabaseBackendBase> backend)
{
    // FIXME: Currently, we're only simulating the backend by return the
    // frontend database as its own the backend. When we split the 2 apart,
    // this create() function should be changed to be a factory method for
    // instantiating the backend.
    return static_cast<Database*>(backend.get());
}

Database::Database(DatabaseContext* databaseContext,
    const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : DatabaseBackend(databaseContext, name, expectedVersion, displayName, estimatedSize)
    , DatabaseBase(databaseContext->executionContext())
    , m_databaseContext(DatabaseBackend::databaseContext())
{
    ScriptWrappable::init(this);
    m_databaseThreadSecurityOrigin = m_contextThreadSecurityOrigin->isolatedCopy();
    setFrontend(this);

    ASSERT(m_databaseContext->databaseThread());
}

Database::~Database()
{
}

void Database::trace(Visitor* visitor)
{
    visitor->trace(m_databaseContext);
    DatabaseBackend::trace(visitor);
}

Database* Database::from(DatabaseBackend* backend)
{
    return static_cast<Database*>(backend->m_frontend);
}

PassRefPtrWillBeRawPtr<DatabaseBackend> Database::backend()
{
    return this;
}

String Database::version() const
{
    return DatabaseBackendBase::version();
}

void Database::closeImmediately()
{
    ASSERT(executionContext()->isContextThread());
    DatabaseThread* databaseThread = databaseContext()->databaseThread();
    if (databaseThread && !databaseThread->terminationRequested() && opened()) {
        logErrorMessage("forcibly closing database");
        databaseThread->scheduleTask(DatabaseCloseTask::create(this, 0));
    }
}

void Database::changeVersion(const String& oldVersion, const String& newVersion, PassOwnPtr<SQLTransactionCallback> callback, PassOwnPtr<SQLTransactionErrorCallback> errorCallback, PassOwnPtr<VoidCallback> successCallback)
{
    ChangeVersionData data(oldVersion, newVersion);
    runTransaction(callback, errorCallback, successCallback, false, &data);
}

void Database::transaction(PassOwnPtr<SQLTransactionCallback> callback, PassOwnPtr<SQLTransactionErrorCallback> errorCallback, PassOwnPtr<VoidCallback> successCallback)
{
    runTransaction(callback, errorCallback, successCallback, false);
}

void Database::readTransaction(PassOwnPtr<SQLTransactionCallback> callback, PassOwnPtr<SQLTransactionErrorCallback> errorCallback, PassOwnPtr<VoidCallback> successCallback)
{
    runTransaction(callback, errorCallback, successCallback, true);
}

static void callTransactionErrorCallback(ExecutionContext*, PassOwnPtr<SQLTransactionErrorCallback> callback, PassOwnPtr<SQLErrorData> errorData)
{
    RefPtrWillBeRawPtr<SQLError> error = SQLError::create(*errorData);
    callback->handleEvent(error.get());
}

void Database::runTransaction(PassOwnPtr<SQLTransactionCallback> callback, PassOwnPtr<SQLTransactionErrorCallback> errorCallback,
    PassOwnPtr<VoidCallback> successCallback, bool readOnly, const ChangeVersionData* changeVersionData)
{
    // FIXME: Rather than passing errorCallback to SQLTransaction and then sometimes firing it ourselves,
    // this code should probably be pushed down into DatabaseBackend so that we only create the SQLTransaction
    // if we're actually going to run it.
#if ASSERT_ENABLED
    SQLTransactionErrorCallback* originalErrorCallback = errorCallback.get();
#endif
    RefPtrWillBeRawPtr<SQLTransaction> transaction = SQLTransaction::create(this, callback, successCallback, errorCallback, readOnly);
    RefPtrWillBeRawPtr<SQLTransactionBackend> transactionBackend = backend()->runTransaction(transaction, readOnly, changeVersionData);
    if (!transactionBackend) {
        OwnPtr<SQLTransactionErrorCallback> callback = transaction->releaseErrorCallback();
        ASSERT(callback == originalErrorCallback);
        if (callback) {
            OwnPtr<SQLErrorData> error = SQLErrorData::create(SQLError::UNKNOWN_ERR, "database has been closed");
            executionContext()->postTask(createCallbackTask(&callTransactionErrorCallback, callback.release(), error.release()));
        }
    }
}

// This object is constructed in a database thread, and destructed in the
// context thread.
class DeliverPendingCallbackTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<DeliverPendingCallbackTask> create(PassRefPtrWillBeRawPtr<SQLTransaction> transaction)
    {
        return adoptPtr(new DeliverPendingCallbackTask(transaction));
    }

    virtual void performTask(ExecutionContext*) OVERRIDE
    {
        m_transaction->performPendingCallback();
    }

private:
    DeliverPendingCallbackTask(PassRefPtrWillBeRawPtr<SQLTransaction> transaction)
        : m_transaction(transaction)
    {
    }

    RefPtrWillBeCrossThreadPersistent<SQLTransaction> m_transaction;
};

void Database::scheduleTransactionCallback(SQLTransaction* transaction)
{
    executionContext()->postTask(DeliverPendingCallbackTask::create(transaction));
}

Vector<String> Database::performGetTableNames()
{
    disableAuthorizer();

    SQLiteStatement statement(sqliteDatabase(), "SELECT name FROM sqlite_master WHERE type='table';");
    if (statement.prepare() != SQLResultOk) {
        WTF_LOG_ERROR("Unable to retrieve list of tables for database %s", databaseDebugName().ascii().data());
        enableAuthorizer();
        return Vector<String>();
    }

    Vector<String> tableNames;
    int result;
    while ((result = statement.step()) == SQLResultRow) {
        String name = statement.getColumnText(0);
        if (name != databaseInfoTableName())
            tableNames.append(name);
    }

    enableAuthorizer();

    if (result != SQLResultDone) {
        WTF_LOG_ERROR("Error getting tables for database %s", databaseDebugName().ascii().data());
        return Vector<String>();
    }

    return tableNames;
}

Vector<String> Database::tableNames()
{
    // FIXME: Not using isolatedCopy on these strings looks ok since threads take strict turns
    // in dealing with them. However, if the code changes, this may not be true anymore.
    Vector<String> result;
    TaskSynchronizer synchronizer;
    if (!databaseContext()->databaseThread() || databaseContext()->databaseThread()->terminationRequested(&synchronizer))
        return result;

    OwnPtr<DatabaseTableNamesTask> task = DatabaseTableNamesTask::create(this, &synchronizer, result);
    databaseContext()->databaseThread()->scheduleTask(task.release());
    synchronizer.waitForTaskCompletion();

    return result;
}

SecurityOrigin* Database::securityOrigin() const
{
    if (executionContext()->isContextThread())
        return m_contextThreadSecurityOrigin.get();
    if (databaseContext()->databaseThread()->isDatabaseThread())
        return m_databaseThreadSecurityOrigin.get();
    return 0;
}

void Database::reportStartTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    backend()->reportStartTransactionResult(errorSite, webSqlErrorCode, sqliteErrorCode);
}

void Database::reportCommitTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    backend()->reportCommitTransactionResult(errorSite, webSqlErrorCode, sqliteErrorCode);
}


} // namespace WebCore
