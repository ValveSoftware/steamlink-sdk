// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.+

#ifndef IDBObserverChangesRecord_h
#define IDBObserverChangesRecord_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class IDBKey;
class IDBValue;
class ScriptState;

class IDBObserverChangesRecord final : public GarbageCollectedFinalized<IDBObserverChangesRecord>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static WebIDBOperationType stringToOperationType(const String&);
    static IDBObserverChangesRecord* create(IDBKey*, PassRefPtr<IDBValue>, WebIDBOperationType);
    ~IDBObserverChangesRecord();

    DECLARE_TRACE();

    // Implement the IDL
    ScriptValue key(ScriptState*);
    ScriptValue value(ScriptState*);
    const String& type() const;

private:
    IDBObserverChangesRecord(IDBKey*, PassRefPtr<IDBValue>, WebIDBOperationType);
    Member<IDBKey> m_key;
    RefPtr<IDBValue> m_value;
    const WebIDBOperationType m_operationType;
};

} // namespace blink

#endif // IDBObserverChangesRecord_h
