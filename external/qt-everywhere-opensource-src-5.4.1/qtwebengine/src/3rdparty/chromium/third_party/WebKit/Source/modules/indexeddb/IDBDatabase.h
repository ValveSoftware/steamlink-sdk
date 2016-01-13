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

#ifndef IDBDatabase_h
#define IDBDatabase_h

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMStringList.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebIDBDatabase.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DOMError;
class ExceptionState;
class ExecutionContext;

class IDBDatabase FINAL
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<IDBDatabase>
    , public ScriptWrappable
    , public EventTargetWithInlineData
    , public ActiveDOMObject {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<IDBDatabase>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(IDBDatabase);
public:
    static IDBDatabase* create(ExecutionContext*, PassOwnPtr<blink::WebIDBDatabase>, IDBDatabaseCallbacks*);
    virtual ~IDBDatabase();
    virtual void trace(Visitor*) OVERRIDE;

    void setMetadata(const IDBDatabaseMetadata& metadata) { m_metadata = metadata; }
    void indexCreated(int64_t objectStoreId, const IDBIndexMetadata&);
    void indexDeleted(int64_t objectStoreId, int64_t indexId);
    void transactionCreated(IDBTransaction*);
    void transactionFinished(const IDBTransaction*);

    // Implement the IDL
    const String& name() const { return m_metadata.name; }
    ScriptValue version(ScriptState*) const;
    PassRefPtrWillBeRawPtr<DOMStringList> objectStoreNames() const;

    IDBObjectStore* createObjectStore(const String& name, const Dictionary&, ExceptionState&);
    IDBObjectStore* createObjectStore(const String& name, const IDBKeyPath&, bool autoIncrement, ExceptionState&);
    IDBTransaction* transaction(ExecutionContext* context, PassRefPtrWillBeRawPtr<DOMStringList> scope, const String& mode, ExceptionState& exceptionState) { return transaction(context, *scope, mode, exceptionState); }
    IDBTransaction* transaction(ExecutionContext*, const Vector<String>&, const String& mode, ExceptionState&);
    IDBTransaction* transaction(ExecutionContext*, const String&, const String& mode, ExceptionState&);
    void deleteObjectStore(const String& name, ExceptionState&);
    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(versionchange);

    // IDBDatabaseCallbacks
    void onVersionChange(int64_t oldVersion, int64_t newVersion);
    void onAbort(int64_t, PassRefPtrWillBeRawPtr<DOMError>);
    void onComplete(int64_t);

    // ActiveDOMObject
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    bool isClosePending() const { return m_closePending; }
    void forceClose();
    const IDBDatabaseMetadata& metadata() const { return m_metadata; }
    void enqueueEvent(PassRefPtrWillBeRawPtr<Event>);

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtrWillBeRawPtr<Event>) OVERRIDE;

    int64_t findObjectStoreId(const String& name) const;
    bool containsObjectStore(const String& name) const
    {
        return findObjectStoreId(name) != IDBObjectStoreMetadata::InvalidId;
    }

    // Will return nullptr if this database is stopped.
    blink::WebIDBDatabase* backend() const { return m_backend.get(); }

    static int64_t nextTransactionId();

    void ackReceivedBlobs(const Vector<blink::WebBlobInfo>*);

    static const char indexDeletedErrorMessage[];
    static const char isKeyCursorErrorMessage[];
    static const char noKeyOrKeyRangeErrorMessage[];
    static const char noSuchIndexErrorMessage[];
    static const char noSuchObjectStoreErrorMessage[];
    static const char noValueErrorMessage[];
    static const char notValidKeyErrorMessage[];
    static const char notVersionChangeTransactionErrorMessage[];
    static const char objectStoreDeletedErrorMessage[];
    static const char requestNotFinishedErrorMessage[];
    static const char sourceDeletedErrorMessage[];
    static const char transactionFinishedErrorMessage[];
    static const char transactionInactiveErrorMessage[];
    static const char transactionReadOnlyErrorMessage[];
    static const char databaseClosedErrorMessage[];

private:
    IDBDatabase(ExecutionContext*, PassOwnPtr<blink::WebIDBDatabase>, IDBDatabaseCallbacks*);

    void closeConnection();

    IDBDatabaseMetadata m_metadata;
    OwnPtr<blink::WebIDBDatabase> m_backend;
    Member<IDBTransaction> m_versionChangeTransaction;
    typedef HeapHashMap<int64_t, Member<IDBTransaction> > TransactionMap;
    TransactionMap m_transactions;

    bool m_closePending;
    bool m_contextStopped;

    // Keep track of the versionchange events waiting to be fired on this
    // database so that we can cancel them if the database closes.
    WillBeHeapVector<RefPtrWillBeMember<Event> > m_enqueuedEvents;

    Member<IDBDatabaseCallbacks> m_databaseCallbacks;
};

} // namespace WebCore

#endif // IDBDatabase_h
