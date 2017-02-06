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

#ifndef IDBRequest_h
#define IDBRequest_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMStringList.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include <memory>

namespace blink {

class DOMException;
class ExceptionState;
class IDBCursor;
struct IDBDatabaseMetadata;
class IDBValue;

class MODULES_EXPORT IDBRequest
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(IDBRequest);
public:
    static IDBRequest* create(ScriptState*, IDBAny* source, IDBTransaction*);
    ~IDBRequest() override;
    DECLARE_VIRTUAL_TRACE();

    ScriptState* getScriptState() { return m_scriptState.get(); }
    ScriptValue result(ExceptionState&);
    DOMException* error(ExceptionState&) const;
    ScriptValue source() const;
    IDBTransaction* transaction() const { return m_transaction.get(); }

    bool isResultDirty() const { return m_resultDirty; }
    IDBAny* resultAsAny() const { return m_result; }

    // Requests made during index population are implementation details and so
    // events should not be visible to script.
    void preventPropagation() { m_preventPropagation = true; }

    // Defined in the IDL
    enum ReadyState {
        PENDING = 1,
        DONE = 2,
        EarlyDeath = 3
    };

    const String& readyState() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(success);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    void setCursorDetails(IndexedDB::CursorType, WebIDBCursorDirection);
    void setPendingCursor(IDBCursor*);
    void abort();

    virtual void onError(DOMException*);
    virtual void onSuccess(const Vector<String>&);
    virtual void onSuccess(std::unique_ptr<WebIDBCursor>, IDBKey*, IDBKey* primaryKey, PassRefPtr<IDBValue>);
    virtual void onSuccess(IDBKey*);
    virtual void onSuccess(PassRefPtr<IDBValue>);
    virtual void onSuccess(const Vector<RefPtr<IDBValue>>&);
    virtual void onSuccess(int64_t);
    virtual void onSuccess();
    virtual void onSuccess(IDBKey*, IDBKey* primaryKey, PassRefPtr<IDBValue>);

    // Only IDBOpenDBRequest instances should receive these:
    virtual void onBlocked(int64_t oldVersion) { ASSERT_NOT_REACHED(); }
    virtual void onUpgradeNeeded(int64_t oldVersion, std::unique_ptr<WebIDBDatabase>, const IDBDatabaseMetadata&, WebIDBDataLoss, String dataLossMessage) { ASSERT_NOT_REACHED(); }
    virtual void onSuccess(std::unique_ptr<WebIDBDatabase>, const IDBDatabaseMetadata&) { ASSERT_NOT_REACHED(); }

    // ActiveScriptWrappable
    bool hasPendingActivity() const final;

    // ActiveDOMObject
    void stop() final;

    // EventTarget
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const final;
    void uncaughtExceptionInEventHandler() final;

    // Called by a version change transaction that has finished to set this
    // request back from DONE (following "upgradeneeded") back to PENDING (for
    // the upcoming "success" or "error").
    void transactionDidFinishAndDispatch();

    IDBCursor* getResultCursor() const;

protected:
    IDBRequest(ScriptState*, IDBAny* source, IDBTransaction*);
    void enqueueEvent(Event*);
    void dequeueEvent(Event*);
    virtual bool shouldEnqueueEvent() const;
    void onSuccessInternal(IDBAny*);
    void setResult(IDBAny*);

    // EventTarget
    DispatchEventResult dispatchEventInternal(Event*) override;

    bool m_contextStopped = false;
    Member<IDBTransaction> m_transaction;
    ReadyState m_readyState = PENDING;
    bool m_requestAborted = false; // May be aborted by transaction then receive async onsuccess; ignore vs. assert.

private:
    void setResultCursor(IDBCursor*, IDBKey*, IDBKey* primaryKey, PassRefPtr<IDBValue>);
    void ackReceivedBlobs(const IDBValue*);
    void ackReceivedBlobs(const Vector<RefPtr<IDBValue>>&);

    RefPtr<ScriptState> m_scriptState;
    Member<IDBAny> m_source;
    Member<IDBAny> m_result;
    Member<DOMException> m_error;

    bool m_hasPendingActivity = true;
    HeapVector<Member<Event>> m_enqueuedEvents;

    // Only used if the result type will be a cursor.
    IndexedDB::CursorType m_cursorType = IndexedDB::CursorKeyAndValue;
    WebIDBCursorDirection m_cursorDirection = WebIDBCursorDirectionNext;
    // When a cursor is continued/advanced, m_result is cleared and m_pendingCursor holds it.
    Member<IDBCursor> m_pendingCursor;
    // New state is not applied to the cursor object until the event is dispatched.
    Member<IDBKey> m_cursorKey;
    Member<IDBKey> m_cursorPrimaryKey;
    RefPtr<IDBValue> m_cursorValue;

    bool m_didFireUpgradeNeededEvent = false;
    bool m_preventPropagation = false;
    bool m_resultDirty = true;
};

} // namespace blink

#endif // IDBRequest_h
