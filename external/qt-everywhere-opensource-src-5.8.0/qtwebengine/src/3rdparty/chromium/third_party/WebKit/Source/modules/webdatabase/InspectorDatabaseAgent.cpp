/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "modules/webdatabase/InspectorDatabaseAgent.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/frame/LocalFrame.h"
#include "core/html/VoidCallback.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/InspectorDatabaseResource.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLResultSet.h"
#include "modules/webdatabase/SQLResultSetRowList.h"
#include "modules/webdatabase/SQLStatementCallback.h"
#include "modules/webdatabase/SQLStatementErrorCallback.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "modules/webdatabase/SQLTransactionCallback.h"
#include "modules/webdatabase/SQLTransactionErrorCallback.h"
#include "modules/webdatabase/sqlite/SQLValue.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

typedef blink::protocol::Database::Backend::ExecuteSQLCallback ExecuteSQLCallback;

namespace blink {

namespace DatabaseAgentState {
static const char databaseAgentEnabled[] = "databaseAgentEnabled";
};

namespace {

class ExecuteSQLCallbackWrapper : public RefCounted<ExecuteSQLCallbackWrapper> {
public:
    static PassRefPtr<ExecuteSQLCallbackWrapper> create(std::unique_ptr<ExecuteSQLCallback> callback) { return adoptRef(new ExecuteSQLCallbackWrapper(std::move(callback))); }
    ~ExecuteSQLCallbackWrapper() { }
    ExecuteSQLCallback* get() { return m_callback.get(); }

    void reportTransactionFailed(SQLError* error)
    {
        std::unique_ptr<protocol::Database::Error> errorObject = protocol::Database::Error::create()
            .setMessage(error->message())
            .setCode(error->code()).build();
        m_callback->sendSuccess(Maybe<protocol::Array<String>>(), Maybe<protocol::Array<protocol::Value>>(), std::move(errorObject));
    }

private:
    explicit ExecuteSQLCallbackWrapper(std::unique_ptr<ExecuteSQLCallback> callback) : m_callback(std::move(callback)) { }
    std::unique_ptr<ExecuteSQLCallback> m_callback;
};

class StatementCallback final : public SQLStatementCallback {
public:
    static StatementCallback* create(PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback)
    {
        return new StatementCallback(requestCallback);
    }

    ~StatementCallback() override { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        SQLStatementCallback::trace(visitor);
    }

    bool handleEvent(SQLTransaction*, SQLResultSet* resultSet) override
    {
        SQLResultSetRowList* rowList = resultSet->rows();

        std::unique_ptr<protocol::Array<String>> columnNames = protocol::Array<String>::create();
        const Vector<String>& columns = rowList->columnNames();
        for (size_t i = 0; i < columns.size(); ++i)
            columnNames->addItem(columns[i]);

        std::unique_ptr<protocol::Array<protocol::Value>> values = protocol::Array<protocol::Value>::create();
        const Vector<SQLValue>& data = rowList->values();
        for (size_t i = 0; i < data.size(); ++i) {
            const SQLValue& value = rowList->values()[i];
            switch (value.getType()) {
            case SQLValue::StringValue: values->addItem(protocol::StringValue::create(value.string())); break;
            case SQLValue::NumberValue: values->addItem(protocol::FundamentalValue::create(value.number())); break;
            case SQLValue::NullValue: values->addItem(protocol::Value::null()); break;
            }
        }
        m_requestCallback->get()->sendSuccess(std::move(columnNames), std::move(values), Maybe<protocol::Database::Error>());
        return true;
    }

private:
    StatementCallback(PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback) : m_requestCallback(requestCallback) { }
    RefPtr<ExecuteSQLCallbackWrapper> m_requestCallback;
};

class StatementErrorCallback final : public SQLStatementErrorCallback {
public:
    static StatementErrorCallback* create(PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback)
    {
        return new StatementErrorCallback(requestCallback);
    }

    ~StatementErrorCallback() override { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        SQLStatementErrorCallback::trace(visitor);
    }

    bool handleEvent(SQLTransaction*, SQLError* error) override
    {
        m_requestCallback->reportTransactionFailed(error);
        return true;
    }

private:
    StatementErrorCallback(PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback) : m_requestCallback(requestCallback) { }
    RefPtr<ExecuteSQLCallbackWrapper> m_requestCallback;
};

class TransactionCallback final : public SQLTransactionCallback {
public:
    static TransactionCallback* create(const String& sqlStatement, PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback)
    {
        return new TransactionCallback(sqlStatement, requestCallback);
    }

    ~TransactionCallback() override { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        SQLTransactionCallback::trace(visitor);
    }

    bool handleEvent(SQLTransaction* transaction) override
    {
        Vector<SQLValue> sqlValues;
        SQLStatementCallback* callback = StatementCallback::create(m_requestCallback);
        SQLStatementErrorCallback* errorCallback = StatementErrorCallback::create(m_requestCallback);
        transaction->executeSQL(m_sqlStatement, sqlValues, callback, errorCallback, IGNORE_EXCEPTION);
        return true;
    }
private:
    TransactionCallback(const String& sqlStatement, PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback)
        : m_sqlStatement(sqlStatement)
        , m_requestCallback(requestCallback) { }
    String m_sqlStatement;
    RefPtr<ExecuteSQLCallbackWrapper> m_requestCallback;
};

class TransactionErrorCallback final : public SQLTransactionErrorCallback {
public:
    static TransactionErrorCallback* create(PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback)
    {
        return new TransactionErrorCallback(requestCallback);
    }

    ~TransactionErrorCallback() override { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        SQLTransactionErrorCallback::trace(visitor);
    }

    bool handleEvent(SQLError* error) override
    {
        m_requestCallback->reportTransactionFailed(error);
        return true;
    }
private:
    TransactionErrorCallback(PassRefPtr<ExecuteSQLCallbackWrapper> requestCallback)
        : m_requestCallback(requestCallback) { }
    RefPtr<ExecuteSQLCallbackWrapper> m_requestCallback;
};

class TransactionSuccessCallback final : public VoidCallback {
public:
    static TransactionSuccessCallback* create()
    {
        return new TransactionSuccessCallback();
    }

    ~TransactionSuccessCallback() override { }

    void handleEvent() override { }

private:
    TransactionSuccessCallback() { }
};

} // namespace

void InspectorDatabaseAgent::registerDatabaseOnCreation(blink::Database* database)
{
    didOpenDatabase(database, database->getSecurityOrigin()->host(), database->stringIdentifier(), database->version());
}

void InspectorDatabaseAgent::didOpenDatabase(blink::Database* database, const String& domain, const String& name, const String& version)
{
    if (InspectorDatabaseResource* resource = findByFileName(database->fileName())) {
        resource->setDatabase(database);
        return;
    }

    InspectorDatabaseResource* resource = InspectorDatabaseResource::create(database, domain, name, version);
    m_resources.set(resource->id(), resource);
    // Resources are only bound while visible.
    ASSERT(m_enabled && frontend());
    resource->bind(frontend());
}

void InspectorDatabaseAgent::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    // FIXME(dgozman): adapt this for out-of-process iframes.
    if (frame != m_page->mainFrame())
        return;

    m_resources.clear();
}

InspectorDatabaseAgent::InspectorDatabaseAgent(Page* page)
    : m_page(page)
    , m_enabled(false)
{
}

InspectorDatabaseAgent::~InspectorDatabaseAgent()
{
}

void InspectorDatabaseAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(DatabaseAgentState::databaseAgentEnabled, m_enabled);
    if (DatabaseClient* client = DatabaseClient::fromPage(m_page))
        client->setInspectorAgent(this);
    DatabaseTracker::tracker().forEachOpenDatabaseInPage(m_page, WTF::bind(&InspectorDatabaseAgent::registerDatabaseOnCreation, wrapPersistent(this)));
}

void InspectorDatabaseAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(DatabaseAgentState::databaseAgentEnabled, m_enabled);
    if (DatabaseClient* client = DatabaseClient::fromPage(m_page))
        client->setInspectorAgent(nullptr);
    m_resources.clear();
}

void InspectorDatabaseAgent::restore()
{
    if (m_state->booleanProperty(DatabaseAgentState::databaseAgentEnabled, false)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorDatabaseAgent::getDatabaseTableNames(ErrorString* error, const String& databaseId, std::unique_ptr<protocol::Array<String>>* names)
{
    if (!m_enabled) {
        *error = "Database agent is not enabled";
        return;
    }

    *names = protocol::Array<String>::create();

    blink::Database* database = databaseForId(databaseId);
    if (database) {
        Vector<String> tableNames = database->tableNames();
        unsigned length = tableNames.size();
        for (unsigned i = 0; i < length; ++i)
            (*names)->addItem(tableNames[i]);
    }
}

void InspectorDatabaseAgent::executeSQL(ErrorString*, const String& databaseId, const String& query, std::unique_ptr<ExecuteSQLCallback> prpRequestCallback)
{
    std::unique_ptr<ExecuteSQLCallback> requestCallback = std::move(prpRequestCallback);

    if (!m_enabled) {
        requestCallback->sendFailure("Database agent is not enabled");
        return;
    }

    blink::Database* database = databaseForId(databaseId);
    if (!database) {
        requestCallback->sendFailure("Database not found");
        return;
    }

    RefPtr<ExecuteSQLCallbackWrapper> wrapper = ExecuteSQLCallbackWrapper::create(std::move(requestCallback));
    SQLTransactionCallback* callback = TransactionCallback::create(query, wrapper);
    SQLTransactionErrorCallback* errorCallback = TransactionErrorCallback::create(wrapper);
    VoidCallback* successCallback = TransactionSuccessCallback::create();
    database->transaction(callback, errorCallback, successCallback);
}

InspectorDatabaseResource* InspectorDatabaseAgent::findByFileName(const String& fileName)
{
    for (DatabaseResourcesHeapMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->value->database()->fileName() == fileName)
            return it->value.get();
    }
    return 0;
}

blink::Database* InspectorDatabaseAgent::databaseForId(const String& databaseId)
{
    DatabaseResourcesHeapMap::iterator it = m_resources.find(databaseId);
    if (it == m_resources.end())
        return 0;
    return it->value->database();
}

DEFINE_TRACE(InspectorDatabaseAgent)
{
    visitor->trace(m_page);
    visitor->trace(m_resources);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
