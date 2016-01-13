/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "modules/webdatabase/DatabaseContext.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "modules/webdatabase/DatabaseThread.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Assertions.h"

namespace WebCore {

// How the DatabaseContext Life-Cycle works?
// ========================================
// ... in other words, who's keeping the DatabaseContext alive and how long does
// it need to stay alive?
//
// The DatabaseContext is referenced from RefPtrs in:
// 1. ExecutionContext
// 2. Database
//
// At Birth:
// ========
// We create a DatabaseContext only when there is a need i.e. the script tries to
// open a Database via DatabaseManager::openDatabase().
//
// The DatabaseContext constructor will call ref(). This lets DatabaseContext keep itself alive.
// Note that paired deref() is called from contextDestroyed().
//
// Once a DatabaseContext is associated with a ExecutionContext, it will
// live until after the ExecutionContext destructs. This is true even if
// we don't succeed in opening any Databases for that context. When we do
// succeed in opening Databases for this ExecutionContext, the Database
// will re-use the same DatabaseContext.
//
// At Shutdown:
// ===========
// During shutdown, the DatabaseContext needs to:
// 1. "outlive" the ExecutionContext.
//    - This is needed because the DatabaseContext needs to remove itself from the
//      ExecutionContext's ActiveDOMObject list and ContextLifecycleObserver
//      list. This removal needs to be executed on the script's thread. Hence, we
//      rely on the ExecutionContext's shutdown process to call
//      stop() and contextDestroyed() to give us a chance to clean these up from
//      the script thread.
//
// 2. "outlive" the Databases.
//    - This is because they may make use of the DatabaseContext to execute a close
//      task and shutdown in an orderly manner. When the Databases are destructed,
//      they will deref the DatabaseContext from the DatabaseThread.
//
// During shutdown, the ExecutionContext is shutting down on the script thread
// while the Databases are shutting down on the DatabaseThread. Hence, there can be
// a race condition as to whether the ExecutionContext or the Databases
// destruct first.
//
// The RefPtrs in the Databases and ExecutionContext will ensure that the
// DatabaseContext will outlive both regardless of which of the 2 destructs first.

PassRefPtrWillBeRawPtr<DatabaseContext> DatabaseContext::create(ExecutionContext* context)
{
    RefPtrWillBeRawPtr<DatabaseContext> self = adoptRefWillBeNoop(new DatabaseContext(context));
    DatabaseManager::manager().registerDatabaseContext(self.get());
    return self.release();
}

DatabaseContext::DatabaseContext(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_hasOpenDatabases(false)
    , m_hasRequestedTermination(false)
{
    // ActiveDOMObject expects this to be called to set internal flags.
    suspendIfNeeded();

    // For debug accounting only. We must do this before we register the
    // instance. The assertions assume this.
    DatabaseManager::manager().didConstructDatabaseContext();
    if (context->isWorkerGlobalScope())
        toWorkerGlobalScope(context)->registerTerminationObserver(this);
}

DatabaseContext::~DatabaseContext()
{
    // For debug accounting only. We must call this last. The assertions assume
    // this.
    DatabaseManager::manager().didDestructDatabaseContext();
}

void DatabaseContext::trace(Visitor* visitor)
{
    visitor->trace(m_databaseThread);
    visitor->trace(m_openSyncDatabases);
}

// This is called if the associated ExecutionContext is destructing while
// we're still associated with it. That's our cue to disassociate and shutdown.
// To do this, we stop the database and let everything shutdown naturally
// because the database closing process may still make use of this context.
// It is not safe to just delete the context here.
void DatabaseContext::contextDestroyed()
{
    RefPtrWillBeRawPtr<DatabaseContext> protector(this);
    stopDatabases();
    if (executionContext()->isWorkerGlobalScope())
        toWorkerGlobalScope(executionContext())->unregisterTerminationObserver(this);
    DatabaseManager::manager().unregisterDatabaseContext(this);
    ActiveDOMObject::contextDestroyed();
}

void DatabaseContext::wasRequestedToTerminate()
{
    DatabaseManager::manager().interruptAllDatabasesForContext(this);
}

// stop() is from stopActiveDOMObjects() which indicates that the owner LocalFrame
// or WorkerThread is shutting down. Initiate the orderly shutdown by stopping
// the associated databases.
void DatabaseContext::stop()
{
    stopDatabases();
}

DatabaseContext* DatabaseContext::backend()
{
    return this;
}

DatabaseThread* DatabaseContext::databaseThread()
{
    if (!m_databaseThread && !m_hasOpenDatabases) {
        // It's OK to ask for the m_databaseThread after we've requested
        // termination because we're still using it to execute the closing
        // of the database. However, it is NOT OK to create a new thread
        // after we've requested termination.
        ASSERT(!m_hasRequestedTermination);

        // Create the database thread on first request - but not if at least one database was already opened,
        // because in that case we already had a database thread and terminated it and should not create another.
        m_databaseThread = DatabaseThread::create();
        m_databaseThread->start();
    }

    return m_databaseThread.get();
}

void DatabaseContext::didOpenDatabase(DatabaseBackendBase& database)
{
    if (!database.isSyncDatabase())
        return;
    ASSERT(isContextThread());
#if ENABLE(OILPAN)
    m_openSyncDatabases.add(&database, adoptPtr(new DatabaseCloser(database)));
#else
    m_openSyncDatabases.add(&database);
#endif
}

void DatabaseContext::didCloseDatabase(DatabaseBackendBase& database)
{
#if !ENABLE(OILPAN)
    if (!database.isSyncDatabase())
        return;
    ASSERT(isContextThread());
    m_openSyncDatabases.remove(&database);
#endif
}

#if ENABLE(OILPAN)
DatabaseContext::DatabaseCloser::~DatabaseCloser()
{
    m_database.closeImmediately();
}
#endif

void DatabaseContext::stopSyncDatabases()
{
    // SQLite is "multi-thread safe", but each database handle can only be used
    // on a single thread at a time.
    //
    // For DatabaseBackendSync, we open the SQLite database on the script
    // context thread. And hence we should also close it on that same
    // thread. This means that the SQLite database need to be closed here in the
    // destructor.
    ASSERT(isContextThread());
#if ENABLE(OILPAN)
    m_openSyncDatabases.clear();
#else
    Vector<DatabaseBackendBase*> syncDatabases;
    copyToVector(m_openSyncDatabases, syncDatabases);
    m_openSyncDatabases.clear();
    for (size_t i = 0; i < syncDatabases.size(); ++i)
        syncDatabases[i]->closeImmediately();
#endif
}

void DatabaseContext::stopDatabases()
{
    stopSyncDatabases();

    // Though we initiate termination of the DatabaseThread here in
    // stopDatabases(), we can't clear the m_databaseThread ref till we get to
    // the destructor. This is because the Databases that are managed by
    // DatabaseThread still rely on this ref between the context and the thread
    // to execute the task for closing the database. By the time we get to the
    // destructor, we're guaranteed that the databases are destructed (which is
    // why our ref count is 0 then and we're destructing). Then, the
    // m_databaseThread RefPtr destructor will deref and delete the
    // DatabaseThread.

    if (m_databaseThread && !m_hasRequestedTermination) {
        TaskSynchronizer sync;
        m_databaseThread->requestTermination(&sync);
        m_hasRequestedTermination = true;
        sync.waitForTaskCompletion();
    }
}

bool DatabaseContext::allowDatabaseAccess() const
{
    if (executionContext()->isDocument())
        return toDocument(executionContext())->isActive();
    ASSERT(executionContext()->isWorkerGlobalScope());
    // allowDatabaseAccess is not yet implemented for workers.
    return true;
}

SecurityOrigin* DatabaseContext::securityOrigin() const
{
    return executionContext()->securityOrigin();
}

bool DatabaseContext::isContextThread() const
{
    return executionContext()->isContextThread();
}

} // namespace WebCore
