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
#include "modules/webdatabase/DatabaseThread.h"

#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "modules/webdatabase/SQLTransactionClient.h"
#include "modules/webdatabase/SQLTransactionCoordinator.h"
#include "platform/Logging.h"
#include "platform/heap/glue/MessageLoopInterruptor.h"
#include "platform/heap/glue/PendingGCRunner.h"
#include "public/platform/Platform.h"

namespace WebCore {

DatabaseThread::DatabaseThread()
    : m_transactionClient(adoptPtr(new SQLTransactionClient()))
    , m_transactionCoordinator(adoptPtrWillBeNoop(new SQLTransactionCoordinator()))
    , m_cleanupSync(0)
    , m_terminationRequested(false)
{
}

DatabaseThread::~DatabaseThread()
{
    ASSERT(m_openDatabaseSet.isEmpty());
    // Oilpan: The database thread must have finished its cleanup tasks before
    // the following clear(). Otherwise, WebThread destructor blocks the caller
    // thread, and causes a deadlock with ThreadState cleanup.
    // DatabaseContext::stop() asks the database thread to close all of
    // databases, and wait until GC heap cleanup of the database thread. So we
    // can safely destruct WebThread here.
    m_thread.clear();
}

void DatabaseThread::trace(Visitor* visitor)
{
    visitor->trace(m_openDatabaseSet);
    visitor->trace(m_transactionCoordinator);
}

void DatabaseThread::start()
{
    if (m_thread)
        return;
    m_thread = adoptPtr(blink::Platform::current()->createThread("WebCore: Database"));
    m_thread->postTask(new Task(WTF::bind(&DatabaseThread::setupDatabaseThread, this)));
}

void DatabaseThread::setupDatabaseThread()
{
    m_pendingGCRunner = adoptPtr(new PendingGCRunner);
    m_messageLoopInterruptor = adoptPtr(new MessageLoopInterruptor(m_thread.get()));
    m_thread->addTaskObserver(m_pendingGCRunner.get());
    ThreadState::attach();
    ThreadState::current()->addInterruptor(m_messageLoopInterruptor.get());
}

void DatabaseThread::requestTermination(TaskSynchronizer *cleanupSync)
{
    MutexLocker lock(m_terminationRequestedMutex);
    ASSERT(!m_terminationRequested);
    m_terminationRequested = true;
    m_cleanupSync = cleanupSync;
    WTF_LOG(StorageAPI, "DatabaseThread %p was asked to terminate\n", this);
    m_thread->postTask(new Task(WTF::bind(&DatabaseThread::cleanupDatabaseThread, this)));
}

bool DatabaseThread::terminationRequested(TaskSynchronizer* taskSynchronizer) const
{
#ifndef NDEBUG
    if (taskSynchronizer)
        taskSynchronizer->setHasCheckedForTermination();
#endif

    MutexLocker lock(m_terminationRequestedMutex);
    return m_terminationRequested;
}

void DatabaseThread::cleanupDatabaseThread()
{
    WTF_LOG(StorageAPI, "Cleaning up DatabaseThread %p", this);

    // Clean up the list of all pending transactions on this database thread
    m_transactionCoordinator->shutdown();

    // Close the databases that we ran transactions on. This ensures that if any transactions are still open, they are rolled back and we don't leave the database in an
    // inconsistent or locked state.
    if (m_openDatabaseSet.size() > 0) {
        // As the call to close will modify the original set, we must take a copy to iterate over.
        WillBeHeapHashSet<RefPtrWillBeMember<DatabaseBackend> > openSetCopy;
        openSetCopy.swap(m_openDatabaseSet);
        WillBeHeapHashSet<RefPtrWillBeMember<DatabaseBackend> >::iterator end = openSetCopy.end();
        for (WillBeHeapHashSet<RefPtrWillBeMember<DatabaseBackend> >::iterator it = openSetCopy.begin(); it != end; ++it)
            (*it)->close();
    }

    m_thread->postTask(new Task(WTF::bind(&DatabaseThread::cleanupDatabaseThreadCompleted, this)));
}

void DatabaseThread::cleanupDatabaseThreadCompleted()
{
    ThreadState::current()->removeInterruptor(m_messageLoopInterruptor.get());
    ThreadState::detach();
    // We need to unregister PendingGCRunner before finising this task to avoid
    // PendingGCRunner::didProcessTask accesses dead ThreadState.
    m_thread->removeTaskObserver(m_pendingGCRunner.get());

    if (m_cleanupSync) // Someone wanted to know when we were done cleaning up.
        m_cleanupSync->taskCompleted();
}

void DatabaseThread::recordDatabaseOpen(DatabaseBackend* database)
{
    ASSERT(isDatabaseThread());
    ASSERT(database);
    ASSERT(!m_openDatabaseSet.contains(database));
    m_openDatabaseSet.add(database);
}

void DatabaseThread::recordDatabaseClosed(DatabaseBackend* database)
{
    ASSERT(isDatabaseThread());
    ASSERT(database);
    ASSERT(m_terminationRequested || m_openDatabaseSet.contains(database));
    m_openDatabaseSet.remove(database);
}

bool DatabaseThread::isDatabaseOpen(DatabaseBackend* database)
{
    ASSERT(isDatabaseThread());
    ASSERT(database);
    MutexLocker lock(m_terminationRequestedMutex);
    return !m_terminationRequested && m_openDatabaseSet.contains(database);
}

void DatabaseThread::scheduleTask(PassOwnPtr<DatabaseTask> task)
{
    ASSERT(m_thread);
    ASSERT(!task->hasSynchronizer() || task->hasCheckedForTermination());
    // WebThread takes ownership of the task.
    m_thread->postTask(task.leakPtr());
}

} // namespace WebCore
