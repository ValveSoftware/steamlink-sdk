/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
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
#ifndef SQLTransactionBackend_h
#define SQLTransactionBackend_h

#include "modules/webdatabase/AbstractSQLStatement.h"
#include "modules/webdatabase/AbstractSQLTransactionBackend.h"
#include "modules/webdatabase/DatabaseBasicTypes.h"
#include "modules/webdatabase/SQLTransactionStateMachine.h"
#include "platform/heap/Handle.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class AbstractSQLTransaction;
class DatabaseBackend;
class SQLErrorData;
class SQLiteTransaction;
class SQLStatementBackend;
class SQLTransactionBackend;
class SQLValue;

class SQLTransactionWrapper : public ThreadSafeRefCountedWillBeGarbageCollectedFinalized<SQLTransactionWrapper> {
public:
    virtual ~SQLTransactionWrapper() { }
    virtual void trace(Visitor*) { }
    virtual bool performPreflight(SQLTransactionBackend*) = 0;
    virtual bool performPostflight(SQLTransactionBackend*) = 0;
    virtual SQLErrorData* sqlError() const = 0;
    virtual void handleCommitFailedAfterPostflight(SQLTransactionBackend*) = 0;
};

class SQLTransactionBackend FINAL : public AbstractSQLTransactionBackend, public SQLTransactionStateMachine<SQLTransactionBackend> {
public:
    static PassRefPtrWillBeRawPtr<SQLTransactionBackend> create(DatabaseBackend*,
        PassRefPtrWillBeRawPtr<AbstractSQLTransaction>, PassRefPtrWillBeRawPtr<SQLTransactionWrapper>, bool readOnly);

    virtual ~SQLTransactionBackend();
    virtual void trace(Visitor*) OVERRIDE;

    void lockAcquired();
    void performNextStep();

    DatabaseBackend* database() { return m_database.get(); }
    bool isReadOnly() { return m_readOnly; }
    void notifyDatabaseThreadIsShuttingDown();

private:
    SQLTransactionBackend(DatabaseBackend*, PassRefPtrWillBeRawPtr<AbstractSQLTransaction>,
        PassRefPtrWillBeRawPtr<SQLTransactionWrapper>, bool readOnly);

    // APIs called from the frontend published via AbstractSQLTransactionBackend:
    virtual void requestTransitToState(SQLTransactionState) OVERRIDE;
    virtual SQLErrorData* transactionError() OVERRIDE;
    virtual AbstractSQLStatement* currentStatement() OVERRIDE;
    virtual void setShouldRetryCurrentStatement(bool) OVERRIDE;
    virtual void executeSQL(PassOwnPtrWillBeRawPtr<AbstractSQLStatement>, const String& statement,
        const Vector<SQLValue>& arguments, int permissions) OVERRIDE;

    void doCleanup();

    void enqueueStatementBackend(PassRefPtrWillBeRawPtr<SQLStatementBackend>);

    // State Machine functions:
    virtual StateFunction stateFunctionFor(SQLTransactionState) OVERRIDE;
    void computeNextStateAndCleanupIfNeeded();

    // State functions:
    SQLTransactionState acquireLock();
    SQLTransactionState openTransactionAndPreflight();
    SQLTransactionState runStatements();
    SQLTransactionState postflightAndCommit();
    SQLTransactionState cleanupAndTerminate();
    SQLTransactionState cleanupAfterTransactionErrorCallback();

    SQLTransactionState unreachableState();
    SQLTransactionState sendToFrontendState();

    SQLTransactionState nextStateForCurrentStatementError();
    SQLTransactionState nextStateForTransactionError();
    SQLTransactionState runCurrentStatementAndGetNextState();

    void getNextStatement();

    RefPtrWillBeMember<AbstractSQLTransaction> m_frontend; // Has a reference cycle, and will break in doCleanup().
    RefPtrWillBeMember<SQLStatementBackend> m_currentStatementBackend;

    RefPtrWillBeMember<DatabaseBackend> m_database;
    RefPtrWillBeMember<SQLTransactionWrapper> m_wrapper;
    OwnPtr<SQLErrorData> m_transactionError;

    bool m_hasCallback;
    bool m_hasSuccessCallback;
    bool m_hasErrorCallback;
    bool m_shouldRetryCurrentStatement;
    bool m_modifiedDatabase;
    bool m_lockAcquired;
    bool m_readOnly;
    bool m_hasVersionMismatch;

    Mutex m_statementMutex;
    Deque<RefPtrWillBeMember<SQLStatementBackend> > m_statementQueue;

    OwnPtr<SQLiteTransaction> m_sqliteTransaction;
};

} // namespace WebCore

#endif // SQLTransactionBackend_h
