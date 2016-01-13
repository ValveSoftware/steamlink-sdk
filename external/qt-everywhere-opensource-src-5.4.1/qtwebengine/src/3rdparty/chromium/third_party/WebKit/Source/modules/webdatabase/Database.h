/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#ifndef Database_h
#define Database_h

#include "bindings/v8/ScriptWrappable.h"
#include "modules/webdatabase/DatabaseBackend.h"
#include "modules/webdatabase/DatabaseBase.h"
#include "modules/webdatabase/DatabaseBasicTypes.h"
#include "modules/webdatabase/DatabaseError.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ChangeVersionData;
class DatabaseCallback;
class DatabaseContext;
class SecurityOrigin;
class SQLTransaction;
class SQLTransactionBackend;
class SQLTransactionCallback;
class SQLTransactionErrorCallback;
class VoidCallback;

class Database FINAL : public DatabaseBackend, public DatabaseBase, public ScriptWrappable {
public:
    virtual ~Database();
    virtual void trace(Visitor*) OVERRIDE;

    // Direct support for the DOM API
    virtual String version() const OVERRIDE;
    void changeVersion(const String& oldVersion, const String& newVersion, PassOwnPtr<SQLTransactionCallback>, PassOwnPtr<SQLTransactionErrorCallback>, PassOwnPtr<VoidCallback> successCallback);
    void transaction(PassOwnPtr<SQLTransactionCallback>, PassOwnPtr<SQLTransactionErrorCallback>, PassOwnPtr<VoidCallback> successCallback);
    void readTransaction(PassOwnPtr<SQLTransactionCallback>, PassOwnPtr<SQLTransactionErrorCallback>, PassOwnPtr<VoidCallback> successCallback);

    // Internal engine support
    static Database* from(DatabaseBackend*);
    DatabaseContext* databaseContext() const { return m_databaseContext.get(); }

    Vector<String> tableNames();

    virtual SecurityOrigin* securityOrigin() const OVERRIDE;

    virtual void closeImmediately() OVERRIDE;

    void scheduleTransactionCallback(SQLTransaction*);

private:
    Database(DatabaseContext*, const String& name,
        const String& expectedVersion, const String& displayName, unsigned long estimatedSize);
    PassRefPtrWillBeRawPtr<DatabaseBackend> backend();
    static PassRefPtrWillBeRawPtr<Database> create(ExecutionContext*, PassRefPtrWillBeRawPtr<DatabaseBackendBase>);

    void runTransaction(PassOwnPtr<SQLTransactionCallback>, PassOwnPtr<SQLTransactionErrorCallback>,
        PassOwnPtr<VoidCallback> successCallback, bool readOnly, const ChangeVersionData* = 0);

    Vector<String> performGetTableNames();

    void reportStartTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode);
    void reportCommitTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode);

    RefPtr<SecurityOrigin> m_databaseThreadSecurityOrigin;
    RefPtrWillBeMember<DatabaseContext> m_databaseContext;

    friend class DatabaseManager;
    friend class DatabaseServer; // FIXME: remove this when the backend has been split out.
    friend class DatabaseBackend; // FIXME: remove this when the backend has been split out.
    friend class SQLStatement;
    friend class SQLTransaction;
};

} // namespace WebCore

#endif // Database_h
