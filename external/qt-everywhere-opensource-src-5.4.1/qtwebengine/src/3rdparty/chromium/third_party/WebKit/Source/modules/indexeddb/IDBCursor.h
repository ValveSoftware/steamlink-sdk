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

#ifndef IDBCursor_h
#define IDBCursor_h

#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IndexedDB.h"
#include "public/platform/WebIDBCursor.h"
#include "public/platform/WebIDBTypes.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class WebBlobInfo;

} // namespace blink

namespace WebCore {

class ExceptionState;
class IDBAny;
class IDBTransaction;
class ExecutionContext;
class SharedBuffer;

class IDBCursor : public GarbageCollectedFinalized<IDBCursor>, public ScriptWrappable {
public:
    static const AtomicString& directionNext();
    static const AtomicString& directionNextUnique();
    static const AtomicString& directionPrev();
    static const AtomicString& directionPrevUnique();

    static blink::WebIDBCursorDirection stringToDirection(const String& modeString, ExceptionState&);
    static const AtomicString& directionToString(unsigned short mode);

    static IDBCursor* create(PassOwnPtr<blink::WebIDBCursor>, blink::WebIDBCursorDirection, IDBRequest*, IDBAny* source, IDBTransaction*);
    virtual ~IDBCursor();
    void trace(Visitor*);
    void contextWillBeDestroyed() { m_backend.clear(); }

    // Implement the IDL
    const String& direction() const { return directionToString(m_direction); }
    ScriptValue key(ScriptState*);
    ScriptValue primaryKey(ScriptState*);
    ScriptValue value(ScriptState*);
    ScriptValue source(ScriptState*) const;

    IDBRequest* update(ScriptState*, ScriptValue&, ExceptionState&);
    void advance(unsigned long, ExceptionState&);
    void continueFunction(ScriptState*, const ScriptValue& key, ExceptionState&);
    void continuePrimaryKey(ScriptState*, const ScriptValue& key, const ScriptValue& primaryKey, ExceptionState&);
    IDBRequest* deleteFunction(ScriptState*, ExceptionState&);

    bool isKeyDirty() const { return m_keyDirty; }
    bool isPrimaryKeyDirty() const { return m_primaryKeyDirty; }
    bool isValueDirty() const { return m_valueDirty; }

    void continueFunction(IDBKey*, IDBKey* primaryKey, ExceptionState&);
    void postSuccessHandlerCallback();
    bool isDeleted() const;
    void close();
    void setValueReady(IDBKey*, IDBKey* primaryKey, PassRefPtr<SharedBuffer> value, PassOwnPtr<Vector<blink::WebBlobInfo> >);
    IDBKey* idbPrimaryKey() const { return m_primaryKey; }
    IDBRequest* request() const { return m_request.get(); }
    virtual bool isKeyCursor() const { return true; }
    virtual bool isCursorWithValue() const { return false; }

protected:
    IDBCursor(PassOwnPtr<blink::WebIDBCursor>, blink::WebIDBCursorDirection, IDBRequest*, IDBAny* source, IDBTransaction*);

private:
    IDBObjectStore* effectiveObjectStore() const;
    void handleBlobAcks();

    OwnPtr<blink::WebIDBCursor> m_backend;
    Member<IDBRequest> m_request;
    const blink::WebIDBCursorDirection m_direction;
    Member<IDBAny> m_source;
    Member<IDBTransaction> m_transaction;
    bool m_gotValue;
    bool m_keyDirty;
    bool m_primaryKeyDirty;
    bool m_valueDirty;
    Member<IDBKey> m_key;
    Member<IDBKey> m_primaryKey;
    RefPtr<SharedBuffer> m_value;
    OwnPtr<Vector<blink::WebBlobInfo> > m_blobInfo;
};

} // namespace WebCore

#endif // IDBCursor_h
