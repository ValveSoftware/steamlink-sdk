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

#ifndef IDBObjectStore_h
#define IDBObjectStore_h

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "modules/indexeddb/IDBCursor.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "public/platform/WebIDBCursor.h"
#include "public/platform/WebIDBDatabase.h"
#include "public/platform/WebIDBTypes.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMStringList;
class IDBAny;
class ExceptionState;

class IDBObjectStore : public GarbageCollectedFinalized<IDBObjectStore>, public ScriptWrappable {
public:
    static IDBObjectStore* create(const IDBObjectStoreMetadata& metadata, IDBTransaction* transaction)
    {
        return new IDBObjectStore(metadata, transaction);
    }
    ~IDBObjectStore() { }
    void trace(Visitor*);

    // Implement the IDBObjectStore IDL
    int64_t id() const { return m_metadata.id; }
    const String& name() const { return m_metadata.name; }
    ScriptValue keyPath(ScriptState*) const;
    PassRefPtrWillBeRawPtr<DOMStringList> indexNames() const;
    IDBTransaction* transaction() const { return m_transaction.get(); }
    bool autoIncrement() const { return m_metadata.autoIncrement; }

    IDBRequest* openCursor(ScriptState*, const ScriptValue& range, const String& direction, ExceptionState&);
    IDBRequest* openKeyCursor(ScriptState*, const ScriptValue& range, const String& direction, ExceptionState&);
    IDBRequest* get(ScriptState*, const ScriptValue& key, ExceptionState&);
    IDBRequest* add(ScriptState*, ScriptValue&, const ScriptValue& key, ExceptionState&);
    IDBRequest* put(ScriptState*, ScriptValue&, const ScriptValue& key, ExceptionState&);
    IDBRequest* deleteFunction(ScriptState*, const ScriptValue& key, ExceptionState&);
    IDBRequest* clear(ScriptState*, ExceptionState&);

    IDBIndex* createIndex(ScriptState* scriptState, const String& name, const String& keyPath, const Dictionary& options, ExceptionState& exceptionState)
    {
        return createIndex(scriptState, name, IDBKeyPath(keyPath), options, exceptionState);
    }
    IDBIndex* createIndex(ScriptState* scriptState, const String& name, const Vector<String>& keyPath, const Dictionary& options, ExceptionState& exceptionState)
    {
        return createIndex(scriptState, name, IDBKeyPath(keyPath), options, exceptionState);
    }
    IDBIndex* index(const String& name, ExceptionState&);
    void deleteIndex(const String& name, ExceptionState&);

    IDBRequest* count(ScriptState*, const ScriptValue& range, ExceptionState&);

    // Used by IDBCursor::update():
    IDBRequest* put(ScriptState*, blink::WebIDBPutMode, IDBAny* source, ScriptValue&, IDBKey*, ExceptionState&);

    // Used internally and by InspectorIndexedDBAgent:
    IDBRequest* openCursor(ScriptState*, IDBKeyRange*, blink::WebIDBCursorDirection, blink::WebIDBTaskType = blink::WebIDBTaskTypeNormal);

    void markDeleted() { m_deleted = true; }
    bool isDeleted() const { return m_deleted; }
    void transactionFinished();

    const IDBObjectStoreMetadata& metadata() const { return m_metadata; }
    void setMetadata(const IDBObjectStoreMetadata& metadata) { m_metadata = metadata; }

    typedef HeapVector<Member<IDBKey> > IndexKeys;

    blink::WebIDBDatabase* backendDB() const;

private:
    IDBObjectStore(const IDBObjectStoreMetadata&, IDBTransaction*);

    IDBIndex* createIndex(ScriptState*, const String& name, const IDBKeyPath&, const Dictionary&, ExceptionState&);
    IDBIndex* createIndex(ScriptState*, const String& name, const IDBKeyPath&, bool unique, bool multiEntry, ExceptionState&);
    IDBRequest* put(ScriptState*, blink::WebIDBPutMode, IDBAny* source, ScriptValue&, const ScriptValue& key, ExceptionState&);

    int64_t findIndexId(const String& name) const;
    bool containsIndex(const String& name) const
    {
        return findIndexId(name) != IDBIndexMetadata::InvalidId;
    }

    IDBObjectStoreMetadata m_metadata;
    Member<IDBTransaction> m_transaction;
    bool m_deleted;

    typedef HeapHashMap<String, Member<IDBIndex> > IDBIndexMap;
    IDBIndexMap m_indexMap;
};

} // namespace WebCore

#endif // IDBObjectStore_h
