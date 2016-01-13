/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "modules/indexeddb/IDBRequest.h"

#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/DOMError.h"
#include "core/dom/ExecutionContext.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "platform/SharedBuffer.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebIDBDatabase.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/dtoa/utils.h"
#include <gtest/gtest.h>
#include <v8.h>

using blink::WebBlobInfo;
using namespace WebCore;

namespace {

class IDBRequestTest : public testing::Test {
public:
    IDBRequestTest()
        : m_scope(v8::Isolate::GetCurrent())
        , m_executionContext(adoptRefWillBeNoop(new NullExecutionContext()))
    {
        m_scope.scriptState()->setExecutionContext(m_executionContext.get());
    }

    ~IDBRequestTest()
    {
        m_scope.scriptState()->setExecutionContext(0);
    }

    v8::Isolate* isolate() const { return m_scope.isolate(); }
    ScriptState* scriptState() const { return m_scope.scriptState(); }
    ExecutionContext* executionContext() const { return m_scope.scriptState()->executionContext(); }

private:
    V8TestingScope m_scope;
    RefPtrWillBePersistent<ExecutionContext> m_executionContext;
};

TEST_F(IDBRequestTest, EventsAfterStopping)
{
    IDBTransaction* transaction = 0;
    IDBRequest* request = IDBRequest::create(scriptState(), IDBAny::createUndefined(), transaction);
    EXPECT_EQ(request->readyState(), "pending");
    executionContext()->stopActiveDOMObjects();

    // Ensure none of the following raise assertions in stopped state:
    request->onError(DOMError::create(AbortError, "Description goes here."));
    request->onSuccess(Vector<String>());
    request->onSuccess(nullptr, IDBKey::createInvalid(), IDBKey::createInvalid(), nullptr, adoptPtr(new Vector<WebBlobInfo>()));
    request->onSuccess(IDBKey::createInvalid());
    request->onSuccess(PassRefPtr<SharedBuffer>(nullptr), adoptPtr(new Vector<WebBlobInfo>()));
    request->onSuccess(PassRefPtr<SharedBuffer>(nullptr), adoptPtr(new Vector<WebBlobInfo>()), IDBKey::createInvalid(), IDBKeyPath());
    request->onSuccess(static_cast<int64_t>(0));
    request->onSuccess();
    request->onSuccess(IDBKey::createInvalid(), IDBKey::createInvalid(), nullptr, adoptPtr(new Vector<WebBlobInfo>()));
}

TEST_F(IDBRequestTest, AbortErrorAfterAbort)
{
    IDBTransaction* transaction = 0;
    IDBRequest* request = IDBRequest::create(scriptState(), IDBAny::createUndefined(), transaction);
    EXPECT_EQ(request->readyState(), "pending");

    // Simulate the IDBTransaction having received onAbort from back end and aborting the request:
    request->abort();

    // Now simulate the back end having fired an abort error at the request to clear up any intermediaries.
    // Ensure an assertion is not raised.
    request->onError(DOMError::create(AbortError, "Description goes here."));
}

class MockWebIDBDatabase : public blink::WebIDBDatabase {
public:
    static PassOwnPtr<MockWebIDBDatabase> create()
    {
        return adoptPtr(new MockWebIDBDatabase());
    }
    virtual ~MockWebIDBDatabase()
    {
        EXPECT_TRUE(m_closeCalled);
    }

    virtual void close() OVERRIDE
    {
        m_closeCalled = true;
    }
    virtual void abort(long long transactionId) OVERRIDE { }

private:
    MockWebIDBDatabase()
        : m_closeCalled(false)
    {
    }

    bool m_closeCalled;
};

TEST_F(IDBRequestTest, ConnectionsAfterStopping)
{
    const int64_t transactionId = 1234;
    const int64_t version = 1;
    const int64_t oldVersion = 0;
    const IDBDatabaseMetadata metadata;
    Persistent<IDBDatabaseCallbacks> callbacks = IDBDatabaseCallbacks::create();

    {
        OwnPtr<MockWebIDBDatabase> backend = MockWebIDBDatabase::create();
        IDBOpenDBRequest* request = IDBOpenDBRequest::create(scriptState(), callbacks, transactionId, version);
        EXPECT_EQ(request->readyState(), "pending");

        executionContext()->stopActiveDOMObjects();
        request->onUpgradeNeeded(oldVersion, backend.release(), metadata, blink::WebIDBDataLossNone, String());
    }

    {
        OwnPtr<MockWebIDBDatabase> backend = MockWebIDBDatabase::create();
        IDBOpenDBRequest* request = IDBOpenDBRequest::create(scriptState(), callbacks, transactionId, version);
        EXPECT_EQ(request->readyState(), "pending");

        executionContext()->stopActiveDOMObjects();
        request->onSuccess(backend.release(), metadata);
    }
}

} // namespace
