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

#ifndef IDBTransaction_h
#define IDBTransaction_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMError.h"
#include "core/events/EventListener.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebIDBDatabase.h"
#include "public/platform/WebIDBTypes.h"
#include "wtf/HashSet.h"

namespace WebCore {

class DOMError;
class ExceptionState;
class IDBCursor;
class IDBDatabase;
class IDBObjectStore;
class IDBOpenDBRequest;
struct IDBObjectStoreMetadata;

class IDBTransaction FINAL
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<IDBTransaction>
    , public ScriptWrappable
    , public EventTargetWithInlineData
    , public ActiveDOMObject {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<IDBTransaction>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(IDBTransaction);
public:
    static IDBTransaction* create(ExecutionContext*, int64_t, const Vector<String>& objectStoreNames, blink::WebIDBTransactionMode, IDBDatabase*);
    static IDBTransaction* create(ExecutionContext*, int64_t, IDBDatabase*, IDBOpenDBRequest*, const IDBDatabaseMetadata& previousMetadata);
    virtual ~IDBTransaction();
    virtual void trace(Visitor*) OVERRIDE;

    static const AtomicString& modeReadOnly();
    static const AtomicString& modeReadWrite();
    static const AtomicString& modeVersionChange();

    static blink::WebIDBTransactionMode stringToMode(const String&, ExceptionState&);
    static const AtomicString& modeToString(blink::WebIDBTransactionMode);

    // When the connection is closed backend will be 0.
    blink::WebIDBDatabase* backendDB() const;

    int64_t id() const { return m_id; }
    bool isActive() const { return m_state == Active; }
    bool isFinished() const { return m_state == Finished; }
    bool isFinishing() const { return m_state == Finishing; }
    bool isReadOnly() const { return m_mode == blink::WebIDBTransactionModeReadOnly; }
    bool isVersionChange() const { return m_mode == blink::WebIDBTransactionModeVersionChange; }

    // Implement the IDBTransaction IDL
    const String& mode() const;
    IDBDatabase* db() const { return m_database.get(); }
    PassRefPtrWillBeRawPtr<DOMError> error() const { return m_error; }
    IDBObjectStore* objectStore(const String& name, ExceptionState&);
    void abort(ExceptionState&);

    void registerRequest(IDBRequest*);
    void unregisterRequest(IDBRequest*);
    void objectStoreCreated(const String&, IDBObjectStore*);
    void objectStoreDeleted(const String&);
    void setActive(bool);
    void setError(PassRefPtrWillBeRawPtr<DOMError>);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    void onAbort(PassRefPtrWillBeRawPtr<DOMError>);
    void onComplete();

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtrWillBeRawPtr<Event>) OVERRIDE;

    // ActiveDOMObject
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

private:
    IDBTransaction(ExecutionContext*, int64_t, const Vector<String>&, blink::WebIDBTransactionMode, IDBDatabase*, IDBOpenDBRequest*, const IDBDatabaseMetadata&);

    void enqueueEvent(PassRefPtrWillBeRawPtr<Event>);

    enum State {
        Inactive, // Created or started, but not in an event callback
        Active, // Created or started, in creation scope or an event callback
        Finishing, // In the process of aborting or completing.
        Finished, // No more events will fire and no new requests may be filed.
    };

    int64_t m_id;
    Member<IDBDatabase> m_database;
    const Vector<String> m_objectStoreNames;
    Member<IDBOpenDBRequest> m_openDBRequest;
    const blink::WebIDBTransactionMode m_mode;
    State m_state;
    bool m_hasPendingActivity;
    bool m_contextStopped;
    RefPtrWillBeMember<DOMError> m_error;

    HeapListHashSet<Member<IDBRequest> > m_requestList;

    typedef HeapHashMap<String, Member<IDBObjectStore> > IDBObjectStoreMap;
    IDBObjectStoreMap m_objectStoreMap;

    typedef HeapHashSet<Member<IDBObjectStore> > IDBObjectStoreSet;
    IDBObjectStoreSet m_deletedObjectStores;

    typedef HeapHashMap<Member<IDBObjectStore>, IDBObjectStoreMetadata> IDBObjectStoreMetadataMap;
    IDBObjectStoreMetadataMap m_objectStoreCleanupMap;
    IDBDatabaseMetadata m_previousMetadata;
};

} // namespace WebCore

#endif // IDBTransaction_h
