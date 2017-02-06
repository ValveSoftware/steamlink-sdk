// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserver_h
#define IDBObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/heap/Handle.h"

namespace blink {

class IDBObserverCallback;
class IDBObserverInit;

class IDBObserver final : public GarbageCollected<IDBObserver>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static IDBObserver* create(IDBObserverCallback&, const IDBObserverInit&);

    // API methods
    void observe(IDBDatabase*, IDBTransaction*, ExceptionState&);

    DECLARE_TRACE();

private:
    IDBObserver(IDBObserverCallback&, const IDBObserverInit&);

    Member<IDBObserverCallback> m_callback;
    bool m_transaction;
    bool m_values;
    bool m_noRecords;
};

} // namespace blink

#endif // IDBObserver_h
