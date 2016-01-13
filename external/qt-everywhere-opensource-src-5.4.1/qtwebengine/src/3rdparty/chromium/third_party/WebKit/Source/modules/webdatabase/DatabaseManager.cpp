/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "modules/webdatabase/DatabaseManager.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "platform/Logging.h"
#include "modules/webdatabase/AbstractDatabaseServer.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseBackend.h"
#include "modules/webdatabase/DatabaseBackendBase.h"
#include "modules/webdatabase/DatabaseBackendSync.h"
#include "modules/webdatabase/DatabaseCallback.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseServer.h"
#include "modules/webdatabase/DatabaseSync.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

DatabaseManager& DatabaseManager::manager()
{
    AtomicallyInitializedStatic(DatabaseManager*, dbManager = new DatabaseManager);
    return *dbManager;
}

DatabaseManager::DatabaseManager()
#if ASSERT_ENABLED
    : m_databaseContextRegisteredCount(0)
    , m_databaseContextInstanceCount(0)
#endif
{
    m_server = new DatabaseServer;
    ASSERT(m_server); // We should always have a server to work with.
}

DatabaseManager::~DatabaseManager()
{
}

class DatabaseCreationCallbackTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<DatabaseCreationCallbackTask> create(PassRefPtrWillBeRawPtr<Database> database, PassOwnPtr<DatabaseCallback> creationCallback)
    {
        return adoptPtr(new DatabaseCreationCallbackTask(database, creationCallback));
    }

    virtual void performTask(ExecutionContext*) OVERRIDE
    {
        m_creationCallback->handleEvent(m_database.get());
    }

private:
    DatabaseCreationCallbackTask(PassRefPtrWillBeRawPtr<Database> database, PassOwnPtr<DatabaseCallback> callback)
        : m_database(database)
        , m_creationCallback(callback)
    {
    }

    RefPtrWillBePersistent<Database> m_database;
    OwnPtr<DatabaseCallback> m_creationCallback;
};

DatabaseContext* DatabaseManager::existingDatabaseContextFor(ExecutionContext* context)
{
    MutexLocker locker(m_contextMapLock);

    ASSERT(m_databaseContextRegisteredCount >= 0);
    ASSERT(m_databaseContextInstanceCount >= 0);
    ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);
#if ENABLE(OILPAN)
    const Persistent<DatabaseContext>* databaseContext = m_contextMap.get(context);
    return databaseContext ? databaseContext->get() : 0;
#else
    return m_contextMap.get(context);
#endif
}

DatabaseContext* DatabaseManager::databaseContextFor(ExecutionContext* context)
{
    if (DatabaseContext* databaseContext = existingDatabaseContextFor(context))
        return databaseContext;
    // We don't need to hold a reference returned by DatabaseContext::create
    // because DatabaseContext::create calls registerDatabaseContext, and the
    // DatabaseManager holds a reference.
    return DatabaseContext::create(context).get();
}

void DatabaseManager::registerDatabaseContext(DatabaseContext* databaseContext)
{
    MutexLocker locker(m_contextMapLock);
    ExecutionContext* context = databaseContext->executionContext();
#if ENABLE(OILPAN)
    m_contextMap.set(context, adoptPtr(new Persistent<DatabaseContext>(databaseContext)));
#else
    m_contextMap.set(context, databaseContext);
#endif
#if ASSERT_ENABLED
    m_databaseContextRegisteredCount++;
#endif
}

void DatabaseManager::unregisterDatabaseContext(DatabaseContext* databaseContext)
{
    MutexLocker locker(m_contextMapLock);
    ExecutionContext* context = databaseContext->executionContext();
    ASSERT(m_contextMap.get(context));
#if ASSERT_ENABLED
    m_databaseContextRegisteredCount--;
#endif
    m_contextMap.remove(context);
}

#if ASSERT_ENABLED
void DatabaseManager::didConstructDatabaseContext()
{
    MutexLocker lock(m_contextMapLock);
    m_databaseContextInstanceCount++;
}

void DatabaseManager::didDestructDatabaseContext()
{
    MutexLocker lock(m_contextMapLock);
    m_databaseContextInstanceCount--;
    ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);
}
#endif

void DatabaseManager::throwExceptionForDatabaseError(DatabaseError error, const String& errorMessage, ExceptionState& exceptionState)
{
    switch (error) {
    case DatabaseError::None:
        return;
    case DatabaseError::GenericSecurityError:
        exceptionState.throwSecurityError(errorMessage);
        return;
    case DatabaseError::InvalidDatabaseState:
        exceptionState.throwDOMException(InvalidStateError, errorMessage);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

static void logOpenDatabaseError(ExecutionContext* context, const String& name)
{
    WTF_LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(),
        context->securityOrigin()->toString().ascii().data());
}

PassRefPtrWillBeRawPtr<DatabaseBackendBase> DatabaseManager::openDatabaseBackend(ExecutionContext* context,
    DatabaseType type, const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    ASSERT(error == DatabaseError::None);

    RefPtrWillBeRawPtr<DatabaseBackendBase> backend = m_server->openDatabase(
        databaseContextFor(context)->backend(), type, name, expectedVersion,
        displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);

    if (!backend) {
        ASSERT(error != DatabaseError::None);

        switch (error) {
        case DatabaseError::GenericSecurityError:
            logOpenDatabaseError(context, name);
            return nullptr;

        case DatabaseError::InvalidDatabaseState:
            logErrorMessage(context, errorMessage);
            return nullptr;

        default:
            ASSERT_NOT_REACHED();
        }
    }

    return backend.release();
}

PassRefPtrWillBeRawPtr<Database> DatabaseManager::openDatabase(ExecutionContext* context,
    const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, PassOwnPtr<DatabaseCallback> creationCallback,
    DatabaseError& error, String& errorMessage)
{
    ASSERT(error == DatabaseError::None);

    bool setVersionInNewDatabase = !creationCallback;
    RefPtrWillBeRawPtr<DatabaseBackendBase> backend = openDatabaseBackend(context, DatabaseType::Async, name,
        expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);
    if (!backend)
        return nullptr;

    RefPtrWillBeRawPtr<Database> database = Database::create(context, backend);

    databaseContextFor(context)->setHasOpenDatabases();
    DatabaseClient::from(context)->didOpenDatabase(database, context->securityOrigin()->host(), name, expectedVersion);

    if (backend->isNew() && creationCallback.get()) {
        WTF_LOG(StorageAPI, "Scheduling DatabaseCreationCallbackTask for database %p\n", database.get());
        database->executionContext()->postTask(DatabaseCreationCallbackTask::create(database, creationCallback));
    }

    ASSERT(database);
    return database.release();
}

PassRefPtrWillBeRawPtr<DatabaseSync> DatabaseManager::openDatabaseSync(ExecutionContext* context,
    const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, PassOwnPtr<DatabaseCallback> creationCallback,
    DatabaseError& error, String& errorMessage)
{
    ASSERT(context->isContextThread());
    ASSERT(error == DatabaseError::None);

    bool setVersionInNewDatabase = !creationCallback;
    RefPtrWillBeRawPtr<DatabaseBackendBase> backend = openDatabaseBackend(context, DatabaseType::Sync, name,
        expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);
    if (!backend)
        return nullptr;

    RefPtrWillBeRawPtr<DatabaseSync> database = DatabaseSync::create(context, backend);

    if (backend->isNew() && creationCallback.get()) {
        WTF_LOG(StorageAPI, "Invoking the creation callback for database %p\n", database.get());
        creationCallback->handleEvent(database.get());
    }

    ASSERT(database);
    return database.release();
}

String DatabaseManager::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfDoesNotExist)
{
    return m_server->fullPathForDatabase(origin, name, createIfDoesNotExist);
}

void DatabaseManager::closeDatabasesImmediately(const String& originIdentifier, const String& name)
{
    m_server->closeDatabasesImmediately(originIdentifier, name);
}

void DatabaseManager::interruptAllDatabasesForContext(DatabaseContext* databaseContext)
{
    m_server->interruptAllDatabasesForContext(databaseContext->backend());
}

void DatabaseManager::logErrorMessage(ExecutionContext* context, const String& message)
{
    context->addConsoleMessage(StorageMessageSource, ErrorMessageLevel, message);
}

} // namespace WebCore
