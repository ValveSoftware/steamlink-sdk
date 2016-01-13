/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "DatabaseClient.h"

#include "core/dom/Document.h"
#include "core/inspector/InspectorController.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"

namespace WebCore {

DatabaseClient::DatabaseClient()
    : m_inspectorAgent(0)
{ }

DatabaseClient* DatabaseClient::from(ExecutionContext* context)
{
    if (context->isDocument()) {
        return static_cast<DatabaseClient*>(WillBeHeapSupplement<Page>::from(toDocument(context)->page(), supplementName()));
    }
    ASSERT(context->isWorkerGlobalScope());
    return static_cast<DatabaseClient*>(WillBeHeapSupplement<WorkerClients>::from(toWorkerGlobalScope(context)->clients(), supplementName()));
}

const char* DatabaseClient::supplementName()
{
    return "DatabaseClient";
}

void DatabaseClient::didOpenDatabase(PassRefPtrWillBeRawPtr<Database> database, const String& domain, const String& name, const String& version)
{
    if (m_inspectorAgent)
        m_inspectorAgent->didOpenDatabase(database, domain, name, version);
}

void DatabaseClient::createInspectorAgentFor(Page* page)
{
    ASSERT(!m_inspectorAgent);
    OwnPtr<InspectorDatabaseAgent> inspectorAgent = InspectorDatabaseAgent::create();
    m_inspectorAgent = inspectorAgent.get();
    page->inspectorController().registerModuleAgent(inspectorAgent.release());
}

void provideDatabaseClientTo(Page& page, PassOwnPtrWillBeRawPtr<DatabaseClient> client)
{
    DatabaseClient* clientPtr = client.get();
    page.provideSupplement(DatabaseClient::supplementName(), client);
    clientPtr->createInspectorAgentFor(&page);
}

void provideDatabaseClientToWorker(WorkerClients* workerClients, PassOwnPtrWillBeRawPtr<DatabaseClient> client)
{
    workerClients->provideSupplement(DatabaseClient::supplementName(), client);
}

} // namespace WebCore
