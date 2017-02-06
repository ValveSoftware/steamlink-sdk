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

#include "modules/webdatabase/DatabaseTracker.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/QuotaTracker.h"
#include "modules/webdatabase/sqlite/SQLiteFileSystem.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginHash.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDatabaseObserver.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"

namespace blink {

static void databaseClosed(Database* database)
{
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->databaseClosed(
            WebSecurityOrigin(database->getSecurityOrigin()),
            database->stringIdentifier());
    }
}

DatabaseTracker& DatabaseTracker::tracker()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(DatabaseTracker, tracker, new DatabaseTracker);
    return tracker;
}

DatabaseTracker::DatabaseTracker()
{
    SQLiteFileSystem::registerSQLiteVFS();
}

bool DatabaseTracker::canEstablishDatabase(DatabaseContext* databaseContext, const String& name, const String& displayName, unsigned estimatedSize, DatabaseError& error)
{
    ExecutionContext* executionContext = databaseContext->getExecutionContext();
    bool success = DatabaseClient::from(executionContext)->allowDatabase(executionContext, name, displayName, estimatedSize);
    if (!success)
        error = DatabaseError::GenericSecurityError;
    return success;
}

String DatabaseTracker::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool)
{
    return String(Platform::current()->databaseCreateOriginIdentifier(WebSecurityOrigin(origin))) + "/" + name + "#";
}

void DatabaseTracker::addOpenDatabase(Database* database)
{
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        m_openDatabaseMap = wrapUnique(new DatabaseOriginMap);

    String originString = database->getSecurityOrigin()->toRawString();
    DatabaseNameMap* nameMap = m_openDatabaseMap->get(originString);
    if (!nameMap) {
        nameMap = new DatabaseNameMap();
        m_openDatabaseMap->set(originString, nameMap);
    }

    String name(database->stringIdentifier());
    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet) {
        databaseSet = new DatabaseSet();
        nameMap->set(name, databaseSet);
    }

    databaseSet->add(database);
}

void DatabaseTracker::removeOpenDatabase(Database* database)
{
    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        String originString = database->getSecurityOrigin()->toRawString();
        ASSERT(m_openDatabaseMap);
        DatabaseNameMap* nameMap = m_openDatabaseMap->get(originString);
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
                m_openDatabaseMap->remove(originString);
                delete nameMap;
            }
        }
    }
    databaseClosed(database);
}

void DatabaseTracker::prepareToOpenDatabase(Database* database)
{
    ASSERT(database->getDatabaseContext()->getExecutionContext()->isContextThread());
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->databaseOpened(
            WebSecurityOrigin(database->getSecurityOrigin()),
            database->stringIdentifier(),
            database->displayName(),
            database->estimatedSize());
    }
}

void DatabaseTracker::failedToOpenDatabase(Database* database)
{
    databaseClosed(database);
}

unsigned long long DatabaseTracker::getMaxSizeForDatabase(const Database* database)
{
    unsigned long long spaceAvailable = 0;
    unsigned long long databaseSize = 0;
    QuotaTracker::instance().getDatabaseSizeAndSpaceAvailableToOrigin(
        database->getSecurityOrigin(),
        database->stringIdentifier(), &databaseSize, &spaceAvailable);
    return databaseSize + spaceAvailable;
}

void DatabaseTracker::closeDatabasesImmediately(SecurityOrigin* origin, const String& name)
{
    String originString = origin->toRawString();
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        return;

    DatabaseNameMap* nameMap = m_openDatabaseMap->get(originString);
    if (!nameMap)
        return;

    DatabaseSet* databaseSet = nameMap->get(name);
    if (!databaseSet)
        return;

    // We have to call closeImmediately() on the context thread.
    for (DatabaseSet::iterator it = databaseSet->begin(); it != databaseSet->end(); ++it)
        (*it)->getDatabaseContext()->getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&DatabaseTracker::closeOneDatabaseImmediately, crossThreadUnretained(this), originString, name, *it));
}

void DatabaseTracker::forEachOpenDatabaseInPage(Page* page, std::unique_ptr<DatabaseCallback> callback)
{
    MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
    if (!m_openDatabaseMap)
        return;
    for (auto& originMap : *m_openDatabaseMap) {
        for (auto& nameDatabaseSet : *originMap.value) {
            for (Database* database : *nameDatabaseSet.value) {
                ExecutionContext* context = database->getExecutionContext();
                ASSERT(context->isDocument());
                if (toDocument(context)->frame()->page() == page)
                    (*callback)(database);
            }
        }
    }
}

void DatabaseTracker::closeOneDatabaseImmediately(const String& originString, const String& name, Database* database)
{
    // First we have to confirm the 'database' is still in our collection.
    {
        MutexLocker openDatabaseMapLock(m_openDatabaseMapGuard);
        if (!m_openDatabaseMap)
            return;

        DatabaseNameMap* nameMap = m_openDatabaseMap->get(originString);
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

} // namespace blink
