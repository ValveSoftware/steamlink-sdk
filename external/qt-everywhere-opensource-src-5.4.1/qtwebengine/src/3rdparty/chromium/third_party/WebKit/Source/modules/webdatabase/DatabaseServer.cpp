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
#include "modules/webdatabase/DatabaseServer.h"

#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseBackend.h"
#include "modules/webdatabase/DatabaseBackendSync.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseSync.h"
#include "modules/webdatabase/DatabaseTracker.h"

namespace WebCore {

String DatabaseServer::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfDoesNotExist)
{
    return DatabaseTracker::tracker().fullPathForDatabase(origin, name, createIfDoesNotExist);
}

void DatabaseServer::closeDatabasesImmediately(const String& originIdentifier, const String& name)
{
    DatabaseTracker::tracker().closeDatabasesImmediately(originIdentifier, name);
}

void DatabaseServer::interruptAllDatabasesForContext(const DatabaseContext* context)
{
    DatabaseTracker::tracker().interruptAllDatabasesForContext(context);
}

PassRefPtrWillBeRawPtr<DatabaseBackendBase> DatabaseServer::openDatabase(DatabaseContext* backendContext,
    DatabaseType type, const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, bool setVersionInNewDatabase, DatabaseError &error, String& errorMessage)
{
    RefPtrWillBeRawPtr<DatabaseBackendBase> database = nullptr;
    if (DatabaseTracker::tracker().canEstablishDatabase(backendContext, name, displayName, estimatedSize, error))
        database = createDatabase(backendContext, type, name, expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);
    return database.release();
}

PassRefPtrWillBeRawPtr<DatabaseBackendBase> DatabaseServer::createDatabase(DatabaseContext* backendContext,
    DatabaseType type, const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    RefPtrWillBeRawPtr<DatabaseBackendBase> database = nullptr;
    switch (type) {
    case DatabaseType::Async:
        database = adoptRefWillBeNoop(new Database(backendContext, name, expectedVersion, displayName, estimatedSize));
        break;
    case DatabaseType::Sync:
        database = adoptRefWillBeNoop(new DatabaseSync(backendContext, name, expectedVersion, displayName, estimatedSize));
    }

    if (!database->openAndVerifyVersion(setVersionInNewDatabase, error, errorMessage))
        return nullptr;
    return database.release();
}

} // namespace WebCore
