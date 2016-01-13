/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DatabaseSync_h
#define DatabaseSync_h

#include "bindings/v8/ScriptWrappable.h"
#include "modules/webdatabase/DatabaseBackendSync.h"
#include "modules/webdatabase/DatabaseBase.h"
#include "modules/webdatabase/DatabaseBasicTypes.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

#ifndef NDEBUG
#include "platform/weborigin/SecurityOrigin.h"
#endif

namespace WebCore {

class DatabaseCallback;
class ExceptionState;
class SQLTransactionSync;
class SQLTransactionSyncCallback;
class SecurityOrigin;

// Instances of this class should be created and used only on the worker's context thread.
class DatabaseSync FINAL : public DatabaseBackendSync, public DatabaseBase, public ScriptWrappable {
public:
    virtual ~DatabaseSync();
    virtual void trace(Visitor*) OVERRIDE;

    void changeVersion(const String& oldVersion, const String& newVersion, PassOwnPtr<SQLTransactionSyncCallback>, ExceptionState&);
    void transaction(PassOwnPtr<SQLTransactionSyncCallback>, ExceptionState&);
    void readTransaction(PassOwnPtr<SQLTransactionSyncCallback>, ExceptionState&);

    virtual void closeImmediately() OVERRIDE;
    void observeTransaction(SQLTransactionSync&);

    const String& lastErrorMessage() const { return m_lastErrorMessage; }
    void setLastErrorMessage(const String& message) { m_lastErrorMessage = message; }
    void setLastErrorMessage(const char* message, int sqliteCode, const char* sqliteMessage)
    {
        setLastErrorMessage(String::format("%s (%d, %s)", message, sqliteCode, sqliteMessage));
    }

private:
    DatabaseSync(DatabaseContext*, const String& name,
        const String& expectedVersion, const String& displayName, unsigned long estimatedSize);
    static PassRefPtrWillBeRawPtr<DatabaseSync> create(ExecutionContext*, PassRefPtrWillBeRawPtr<DatabaseBackendBase>);

    void runTransaction(PassOwnPtr<SQLTransactionSyncCallback>, bool readOnly, ExceptionState&);
    void rollbackTransaction(SQLTransactionSync&);
#if ENABLE(OILPAN)
    class TransactionObserver {
    public:
        explicit TransactionObserver(SQLTransactionSync& transaction) : m_transaction(transaction) { }
        ~TransactionObserver();

    private:
        SQLTransactionSync& m_transaction;
    };

    // Need a Persistent field because a HeapHashMap entry should be removed
    // just after a SQLTransactionSync becomes untraceable. If this field was a
    // Member<>, we could not assume the destruction order of DatabaseSync,
    // SQLTransactionSync, and the field. We can not make the field static
    // because multiple worker threads create SQLTransactionSync.
    GC_PLUGIN_IGNORE("http://crbug.com/353083")
    PersistentHeapHashMap<WeakMember<SQLTransactionSync>, OwnPtr<TransactionObserver> > m_observers;
#endif

    String m_lastErrorMessage;

    friend class DatabaseManager;
    friend class DatabaseServer; // FIXME: remove this when the backend has been split out.
};

} // namespace WebCore

#endif
