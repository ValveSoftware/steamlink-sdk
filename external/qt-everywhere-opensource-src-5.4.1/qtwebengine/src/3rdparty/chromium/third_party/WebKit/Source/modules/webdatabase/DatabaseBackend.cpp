/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/webdatabase/DatabaseBackend.h"

#include "platform/Logging.h"
#include "modules/webdatabase/ChangeVersionData.h"
#include "modules/webdatabase/ChangeVersionWrapper.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "modules/webdatabase/DatabaseThread.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "modules/webdatabase/SQLTransactionBackend.h"
#include "modules/webdatabase/SQLTransactionClient.h"
#include "modules/webdatabase/SQLTransactionCoordinator.h"

namespace WebCore {

DatabaseBackend::DatabaseBackend(DatabaseContext* databaseContext, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : DatabaseBackendBase(databaseContext, name, expectedVersion, displayName, estimatedSize, DatabaseType::Async)
    , m_transactionInProgress(false)
    , m_isTransactionQueueEnabled(true)
{
}

void DatabaseBackend::trace(Visitor* visitor)
{
    visitor->trace(m_transactionQueue);
    DatabaseBackendBase::trace(visitor);
}

bool DatabaseBackend::openAndVerifyVersion(bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    TaskSynchronizer synchronizer;
    if (!databaseContext()->databaseThread() || databaseContext()->databaseThread()->terminationRequested(&synchronizer))
        return false;

    DatabaseTracker::tracker().prepareToOpenDatabase(this);
    bool success = false;
    OwnPtr<DatabaseOpenTask> task = DatabaseOpenTask::create(this, setVersionInNewDatabase, &synchronizer, error, errorMessage, success);
    databaseContext()->databaseThread()->scheduleTask(task.release());
    synchronizer.waitForTaskCompletion();

    return success;
}

bool DatabaseBackend::performOpenAndVerify(bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    if (DatabaseBackendBase::performOpenAndVerify(setVersionInNewDatabase, error, errorMessage)) {
        if (databaseContext()->databaseThread())
            databaseContext()->databaseThread()->recordDatabaseOpen(this);

        return true;
    }

    return false;
}

void DatabaseBackend::close()
{
    ASSERT(databaseContext()->databaseThread());
    ASSERT(databaseContext()->databaseThread()->isDatabaseThread());

    {
        MutexLocker locker(m_transactionInProgressMutex);

        // Clean up transactions that have not been scheduled yet:
        // Transaction phase 1 cleanup. See comment on "What happens if a
        // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
        RefPtrWillBeRawPtr<SQLTransactionBackend> transaction = nullptr;
        while (!m_transactionQueue.isEmpty()) {
            transaction = m_transactionQueue.takeFirst();
            transaction->notifyDatabaseThreadIsShuttingDown();
        }

        m_isTransactionQueueEnabled = false;
        m_transactionInProgress = false;
    }

    closeDatabase();
    databaseContext()->databaseThread()->recordDatabaseClosed(this);
}

PassRefPtrWillBeRawPtr<SQLTransactionBackend> DatabaseBackend::runTransaction(PassRefPtrWillBeRawPtr<SQLTransaction> transaction,
    bool readOnly, const ChangeVersionData* data)
{
    MutexLocker locker(m_transactionInProgressMutex);
    if (!m_isTransactionQueueEnabled)
        return nullptr;

    RefPtrWillBeRawPtr<SQLTransactionWrapper> wrapper = nullptr;
    if (data)
        wrapper = ChangeVersionWrapper::create(data->oldVersion(), data->newVersion());

    RefPtrWillBeRawPtr<SQLTransactionBackend> transactionBackend = SQLTransactionBackend::create(this, transaction, wrapper.release(), readOnly);
    m_transactionQueue.append(transactionBackend);
    if (!m_transactionInProgress)
        scheduleTransaction();

    return transactionBackend;
}

void DatabaseBackend::inProgressTransactionCompleted()
{
    MutexLocker locker(m_transactionInProgressMutex);
    m_transactionInProgress = false;
    scheduleTransaction();
}

void DatabaseBackend::scheduleTransaction()
{
    ASSERT(!m_transactionInProgressMutex.tryLock()); // Locked by caller.
    RefPtrWillBeRawPtr<SQLTransactionBackend> transaction = nullptr;

    if (m_isTransactionQueueEnabled && !m_transactionQueue.isEmpty())
        transaction = m_transactionQueue.takeFirst();

    if (transaction && databaseContext()->databaseThread()) {
        OwnPtr<DatabaseTransactionTask> task = DatabaseTransactionTask::create(transaction);
        WTF_LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for transaction %p\n", task.get(), task->transaction());
        m_transactionInProgress = true;
        databaseContext()->databaseThread()->scheduleTask(task.release());
    } else
        m_transactionInProgress = false;
}

void DatabaseBackend::scheduleTransactionStep(SQLTransactionBackend* transaction)
{
    if (!databaseContext()->databaseThread())
        return;

    OwnPtr<DatabaseTransactionTask> task = DatabaseTransactionTask::create(transaction);
    WTF_LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for the transaction step\n", task.get());
    databaseContext()->databaseThread()->scheduleTask(task.release());
}

SQLTransactionClient* DatabaseBackend::transactionClient() const
{
    return databaseContext()->databaseThread()->transactionClient();
}

SQLTransactionCoordinator* DatabaseBackend::transactionCoordinator() const
{
    return databaseContext()->databaseThread()->transactionCoordinator();
}

} // namespace WebCore
