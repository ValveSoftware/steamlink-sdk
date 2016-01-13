/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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

#include "modules/webdatabase/WorkerGlobalScopeWebDatabase.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseCallback.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/DatabaseSync.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<Database> WorkerGlobalScopeWebDatabase::openDatabase(WorkerGlobalScope& context, const String& name, const String& version, const String& displayName, unsigned long estimatedSize, PassOwnPtr<DatabaseCallback> creationCallback, ExceptionState& exceptionState)
{
    DatabaseManager& dbManager = DatabaseManager::manager();
    RefPtrWillBeRawPtr<Database> database = nullptr;
    DatabaseError error = DatabaseError::None;
    if (RuntimeEnabledFeatures::databaseEnabled() && context.securityOrigin()->canAccessDatabase()) {
        String errorMessage;
        database = dbManager.openDatabase(&context, name, version, displayName, estimatedSize, creationCallback, error, errorMessage);
        ASSERT(database || error != DatabaseError::None);
        if (error != DatabaseError::None)
            DatabaseManager::throwExceptionForDatabaseError(error, errorMessage, exceptionState);
    } else {
        exceptionState.throwSecurityError("Access to the WebDatabase API is denied in this context.");
    }

    return database.release();
}

PassRefPtrWillBeRawPtr<DatabaseSync> WorkerGlobalScopeWebDatabase::openDatabaseSync(WorkerGlobalScope& context, const String& name, const String& version, const String& displayName, unsigned long estimatedSize, PassOwnPtr<DatabaseCallback> creationCallback, ExceptionState& exceptionState)
{
    DatabaseManager& dbManager = DatabaseManager::manager();
    RefPtrWillBeRawPtr<DatabaseSync> database = nullptr;
    DatabaseError error =  DatabaseError::None;
    if (RuntimeEnabledFeatures::databaseEnabled() && context.securityOrigin()->canAccessDatabase()) {
        String errorMessage;
        database = dbManager.openDatabaseSync(&context, name, version, displayName, estimatedSize, creationCallback, error, errorMessage);
        ASSERT(database || error != DatabaseError::None);
        if (error != DatabaseError::None)
            DatabaseManager::throwExceptionForDatabaseError(error, errorMessage, exceptionState);
    } else {
        exceptionState.throwSecurityError("Access to the WebDatabase API is denied in this context.");
    }

    return database.release();
}

} // namespace WebCore
