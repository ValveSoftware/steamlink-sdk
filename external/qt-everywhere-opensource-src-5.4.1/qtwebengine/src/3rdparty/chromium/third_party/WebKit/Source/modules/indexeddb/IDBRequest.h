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

#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMError.h"
#include "core/dom/DOMStringList.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "modules/EventModules.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebIDBCursor.h"
#include "public/platform/WebIDBTypes.h"

namespace WebCore {

class ExceptionState;
class IDBCursor;
struct IDBDatabaseMetadata;
class SharedBuffer;

class IDBRequest
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<IDBRequest>
    , public ScriptWrappable
    , public EventTargetWithInlineData
    , public ActiveDOMObject {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<IDBRequest>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(IDBRequest);
public:
    static IDBRequest* create(ScriptState*, IDBAny* source, IDBTransaction*);
    virtual ~IDBRequest();
    virtual void trace(Visitor*) OVERRIDE;

    ScriptValue result(ExceptionState&);
    PassRefPtrWillBeRawPtr<DOMError> error(ExceptionState&) const;
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

    void setCursorDetails(IndexedDB::CursorType, blink::WebIDBCursorDirection);
    void setPendingCursor(IDBCursor*);
    void abort();

    virtual void onError(PassRefPtrWillBeRawPtr<DOMError>);
    virtual void onSuccess(const Vector<String>&);
    virtual void onSuccess(PassOwnPtr<blink::WebIDBCursor>, IDBKey*, IDBKey* primaryKey, PassRefPtr<SharedBuffer>, PassOwnPtr<Vector<blink::WebBlobInfo> >);
    virtual void onSuccess(IDBKey*);
    virtual void onSuccess(PassRefPtr<SharedBuffer>, PassOwnPtr<Vector<blink::WebBlobInfo> >);
    virtual void onSuccess(PassRefPtr<SharedBuffer>, PassOwnPtr<Vector<blink::WebBlobInfo> >, IDBKey*, const IDBKeyPath&);
    virtual void onSuccess(int64_t);
    virtual void onSuccess();
    virtual void onSuccess(IDBKey*, IDBKey* primaryKey, PassRefPtr<SharedBuffer>, PassOwnPtr<Vector<blink::WebBlobInfo> >);

    // Only IDBOpenDBRequest instances should receive these:
    virtual void onBlocked(int64_t oldVersion) { ASSERT_NOT_REACHED(); }
    virtual void onUpgradeNeeded(int64_t oldVersion, PassOwnPtr<blink::WebIDBDatabase>, const IDBDatabaseMetadata&, blink::WebIDBDataLoss, String dataLossMessage) { ASSERT_NOT_REACHED(); }
    virtual void onSuccess(PassOwnPtr<blink::WebIDBDatabase>, const IDBDatabaseMetadata&) { ASSERT_NOT_REACHED(); }

    // ActiveDOMObject
    virtual bool hasPendingActivity() const OVERRIDE FINAL;
    virtual void stop() OVERRIDE FINAL;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE FINAL;
    virtual void uncaughtExceptionInEventHandler() OVERRIDE FINAL;

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtrWillBeRawPtr<Event>) OVERRIDE;

    // Called by a version change transaction that has finished to set this
    // request back from DONE (following "upgradeneeded") back to PENDING (for
    // the upcoming "success" or "error").
    void transactionDidFinishAndDispatch();

    IDBCursor* getResultCursor() const;

protected:
    IDBRequest(ScriptState*, IDBAny* source, IDBTransaction*);
    void enqueueEvent(PassRefPtrWillBeRawPtr<Event>);
    void dequeueEvent(Event*);
    virtual bool shouldEnqueueEvent() const;
    void onSuccessInternal(IDBAny*);
    void setResult(IDBAny*);

    bool m_contextStopped;
    Member<IDBTransaction> m_transaction;
    ReadyState m_readyState;
    bool m_requestAborted; // May be aborted by transaction then receive async onsuccess; ignore vs. assert.

private:
    void setResultCursor(IDBCursor*, IDBKey*, IDBKey* primaryKey, PassRefPtr<SharedBuffer> value, PassOwnPtr<Vector<blink::WebBlobInfo> >);
    void handleBlobAcks();

    RefPtr<ScriptState> m_scriptState;
    Member<IDBAny> m_source;
    Member<IDBAny> m_result;
    RefPtrWillBeMember<DOMError> m_error;

    bool m_hasPendingActivity;
    WillBeHeapVector<RefPtrWillBeMember<Event> > m_enqueuedEvents;

    // Only used if the result type will be a cursor.
    IndexedDB::CursorType m_cursorType;
    blink::WebIDBCursorDirection m_cursorDirection;
    // When a cursor is continued/advanced, m_result is cleared and m_pendingCursor holds it.
    Member<IDBCursor> m_pendingCursor;
    // New state is not applied to the cursor object until the event is dispatched.
    Member<IDBKey> m_cursorKey;
    Member<IDBKey> m_cursorPrimaryKey;
    RefPtr<SharedBuffer> m_cursorValue;
    OwnPtr<Vector<blink::WebBlobInfo> > m_blobInfo;

    bool m_didFireUpgradeNeededEvent;
    bool m_preventPropagation;
    bool m_resultDirty;
};

} // namespace WebCore

#endif // IDBRequest_h
