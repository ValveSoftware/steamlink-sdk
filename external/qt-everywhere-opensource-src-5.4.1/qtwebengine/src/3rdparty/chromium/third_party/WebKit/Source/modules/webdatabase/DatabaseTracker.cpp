/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "modules/webdatabase/DatabaseTracker.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "modules/webdatabase/DatabaseBackendBase.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/QuotaTracker.h"
#include "modules/webdatabase/sqlite/SQLiteFileSystem.h"
#include "platform/weborigin/DatabaseIdentifier.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginHash.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDatabaseObserver.h"
#include "wtf/Assertions.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

static void databaseClosed(DatabaseBackendBase* database)
{
    if (blink::Platform::current()->databaseObserver()) {
        blink::Platform::current()->databaseObserver()->databaseClosed(
            createDatabaseIdentifierFromSecurityOrigin(database->securityOrigin()),
            database->stringIdentifier());
    }
}

DatabaseTracker& DatabaseTracker::tracker()
{
    AtomicallyInitializedStatic(DatabaseTracker&, tracker = *new DatabaseTracker());
    return tracker;
}

DatabaseTracker::DatabaseTracker()
{
    SQLiteFileSystem::registerSQLiteVFS();
}

bool DatabaseTracker::canEstablishDatabase(DatabaseContext* databaseContext, const String& name, const String& displayName, unsigned long estimatedSize, DatabaseError& error)
{
    ExecutionContext* executionContext = databaseContext->executionContext();
    bool success = DatabaseClient::from(executionContext)->allowDatabase(executionContext, name, displayName, estimatedSize);
    if (!success)
        error = DatabaseError::GenericSecurityError;
    return success;
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool)
{
    return createDatabaseIdentifierFromSecurityOrigin(origin) + "/" + name + "#";
}

void DatabaseTracker::addOpenDatabase(DatabaseBackendBase* database)
{
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        m_openDatabaseMap = adoptPtr(new DatabaseOriginMap);

    String originIdentifier = createDatabaseIdentifierFromSecurityOrigin(database->securityOrigin());
    DatabaseNameMap* nameMap = m_openDatabaseMap->get(originIdentifier);
    if (!nameMap) {
        nameMap = new DatabaseNameMap();
        m_openDatabaseMap->set(originIdentifier, nameMap);
    }

    String name(database->stringIdentifier());
    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet) {
        databaseSet = new DatabaseSet();
        nameMap->set(name, databaseSet);
    }

    databaseSet->add(database);
}

class NotifyDatabaseObserverOnCloseTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<NotifyDatabaseObserverOnCloseTask> create(PassRefPtrWillBeRawPtr<DatabaseBackendBase> database)
    {
        return adoptPtr(new NotifyDatabaseObserverOnCloseTask(database));
    }

    virtual void performTask(ExecutionContext*) OVERRIDE
    {
        databaseClosed(m_database.get());
    }

    virtual bool isCleanupTask() const OVERRIDE
    {
        return true;
    }

private:
    explicit NotifyDatabaseObserverOnCloseTask(PassRefPtrWillBeRawPtr<DatabaseBackendBase> database)
        : m_database(database)
    {
    }

    RefPtrWillBeCrossThreadPersistent<DatabaseBackendBase> m_database;
};

void DatabaseTracker::removeOpenDatabase(DatabaseBackendBase* database)
{
    String originIdentifier = createDatabaseIdentifierFromSecurityOrigin(database->securityOrigin());
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    ASSERT(m_openDatabaseMap);
    DatabaseNameMap* nameMap = m_openDatabaseMap->get(originIdentifier);
    if (!nameMap)
        return;

    String name(database->stringIdentifier());
    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet)
        return;

    DatabaseSet::iterator found = databaseSet->find(database);
    if (found == databaseSet->end())
        return;

    databaseSet->remove(found);
    if (databaseSet->isEmpty()) {
        nameMap->remove(name);
        delete databaseSet;
        if (nameMap->isEmpty()) {
            m_openDatabaseMap->remove(originIdentifier);
            delete nameMap;
        }
    }

    ExecutionContext* executionContext = database->databaseContext()->executionContext();
    if (!executionContext->isContextThread())
        executionContext->postTask(NotifyDatabaseObserverOnCloseTask::create(database));
    else
        databaseClosed(database);
}

void DatabaseTracker::prepareToOpenDatabase(DatabaseBackendBase* database)
{
    ASSERT(database->databaseContext()->executionContext()->isContextThread());
    if (blink::Platform::current()->databaseObserver()) {
        blink::Platform::current()->databaseObserver()->databaseOpened(
            createDatabaseIdentifierFromSecurityOrigin(database->securityOrigin()),
            database->stringIdentifier(),
            database->displayName(),
            database->estimatedSize());
    }
}

void DatabaseTracker::failedToOpenDatabase(DatabaseBackendBase* database)
{
    ExecutionContext* executionContext = database->databaseContext()->executionContext();
    if (!executionContext->isContextThread())
        executionContext->postTask(NotifyDatabaseObserverOnCloseTask::create(database));
    else
        databaseClosed(database);
}

unsigned long long DatabaseTracker::getMaxSizeForDatabase(const DatabaseBackendBase* database)
{
    unsigned long long spaceAvailable = 0;
    unsigned long long databaseSize = 0;
    QuotaTracker::instance().getDatabaseSizeAndSpaceAvailableToOrigin(
        createDatabaseIdentifierFromSecurityOrigin(database->securityOrigin()),
        database->stringIdentifier(), &databaseSize, &spaceAvailable);
    return databaseSize + spaceAvailable;
}

void DatabaseTracker::interruptAllDatabasesForContext(const DatabaseContext* context)
{
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);

    if (!m_openDatabaseMap)
        return;

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(createDatabaseIdentifierFromSecurityOrigin(context->securityOrigin()));
    if (!nameMap)
        return;

    DatabaseNameMap::const_iterator dbNameMapEndIt = nameMap->end();
    for (DatabaseNameMap::const_iterator dbNameMapIt = nameMap->begin(); dbNameMapIt != dbNameMapEndIt; ++dbNameMapIt) {
        DatabaseSet* databaseSet = dbNameMapIt->value;
        DatabaseSet::const_iterator end = databaseSet->end();
        for (DatabaseSet::const_iterator it = databaseSet->begin(); it != end; ++it) {
            if ((*it)->databaseContext() == context)
                (*it)->interrupt();
        }
    }
}

class DatabaseTracker::CloseOneDatabaseImmediatelyTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<CloseOneDatabaseImmediatelyTask> create(const String& originIdentifier, const String& name, DatabaseBackendBase* database)
    {
        return adoptPtr(new CloseOneDatabaseImmediatelyTask(originIdentifier, name, database));
    }

    virtual void performTask(ExecutionContext*) OVERRIDE
    {
        DatabaseTracker::tracker().closeOneDatabaseImmediately(m_originIdentifier, m_name, m_database);
    }

private:
    CloseOneDatabaseImmediatelyTask(const String& originIdentifier, const String& name, DatabaseBackendBase* database)
        : m_originIdentifier(originIdentifier.isolatedCopy())
        , m_name(name.isolatedCopy())
        , m_database(database)
    {
    }

    String m_originIdentifier;
    String m_name;
    DatabaseBackendBase* m_database; // Intentionally a raw pointer.
};

void DatabaseTracker::closeDatabasesImmediately(const String& originIdentifier, const String& name)
{
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        return;

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(originIdentifier);
    if (!nameMap)
        return;

    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet)
        return;

    // We have to call closeImmediately() on the context thread and we cannot safely add a reference to
    // the database in our collection when not on the context thread (which is always the case given
    // current usage).
    for (DatabaseSet::iterator it = databaseSet->begin(); it != databaseSet->end(); ++it)
        (*it)->databaseContext()->executionContext()->postTask(CloseOneDatabaseImmediatelyTask::create(originIdentifier, name, *it));
}

void DatabaseTracker::closeOneDatabaseImmediately(const String& originIdentifier, const String& name, DatabaseBackendBase* database)
{
    // First we have to confirm the 'database' is still in our collection.
    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (!m_openDatabaseMap)
            return;

        DatabaseNameMap* nameMap = m_openDatabaseMap->get(originIdentifier);
        if (!nameMap)
            return;

        DatabaseSet* databaseSet = nameMap->get(name);
        if (!databaseSet)
            return;

        DatabaseSet::iterator found = databaseSet->find(database);
        if (found == databaseSet->end())
            return;
    }

    // And we have to call closeImmediately() without our collection lock being held.
    database->closeImmediately();
}

}
