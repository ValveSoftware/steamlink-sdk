/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "modules/webdatabase/SQLTransactionClient.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/ExecutionContext.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDatabaseObserver.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

void databaseModified(const WebSecurityOrigin& origin, const String& databaseName)
{
    if (Platform::current()->databaseObserver())
        Platform::current()->databaseObserver()->databaseModified(origin, databaseName);
}

void databaseModifiedCrossThread(const String& originString, const String& databaseName)
{
    databaseModified(WebSecurityOrigin::createFromString(originString), databaseName);
}

} // namespace

void SQLTransactionClient::didCommitWriteTransaction(Database* database)
{
    String databaseName = database->stringIdentifier();
    ExecutionContext* executionContext = database->getDatabaseContext()->getExecutionContext();
    if (!executionContext->isContextThread()) {
        executionContext->postTask(BLINK_FROM_HERE, createCrossThreadTask(&databaseModifiedCrossThread, executionContext->getSecurityOrigin()->toRawString(), databaseName));
    } else {
        databaseModified(WebSecurityOrigin(executionContext->getSecurityOrigin()), databaseName);
    }
}

bool SQLTransactionClient::didExceedQuota(Database* database)
{
    // Chromium does not allow users to manually change the quota for an origin (for now, at least).
    // Don't do anything.
    ASSERT(database->getDatabaseContext()->getExecutionContext()->isContextThread());
    return false;
}

} // namespace blink
