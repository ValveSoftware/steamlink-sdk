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

#ifndef IDBPendingTransactionMonitor_h
#define IDBPendingTransactionMonitor_h

#include "platform/Supplementable.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ExecutionContext;
class IDBTransaction;

// This class keeps track of the transactions created during the current
// Javascript execution context. Transactions have an internal |active| flag
// which is set to true on creation, but must be set to false when control
// returns to the event loop.

class IDBPendingTransactionMonitor : public Supplement<ExecutionContext> {
    WTF_MAKE_NONCOPYABLE(IDBPendingTransactionMonitor);

public:
    static IDBPendingTransactionMonitor& from(Supplementable<ExecutionContext>&);
    virtual ~IDBPendingTransactionMonitor();
    // The trace functino doesn't work until ExecutionContext is moved to Oilpan
    // heap.
    virtual void trace(Visitor* visitor) OVERRIDE { Supplement<ExecutionContext>::trace(visitor); }
    void addNewTransaction(IDBTransaction&);
    void deactivateNewTransactions();

private:
    IDBPendingTransactionMonitor();
    static const char* supplementName();

    typedef PersistentHeapVector<Member<IDBTransaction> > TransactionList;
    TransactionList m_transactions;
};

} // namespace WebCore

#endif // IDBPendingTransactionMonitor_h
