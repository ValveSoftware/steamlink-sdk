/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webdatabase/Database.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/webdatabase/ChangeVersionData.h"
#include "modules/webdatabase/ChangeVersionWrapper.h"
#include "modules/webdatabase/DatabaseAuthorizer.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "modules/webdatabase/DatabaseThread.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "modules/webdatabase/SQLTransactionBackend.h"
#include "modules/webdatabase/SQLTransactionCallback.h"
#include "modules/webdatabase/SQLTransactionClient.h"
#include "modules/webdatabase/SQLTransactionCoordinator.h"
#include "modules/webdatabase/SQLTransactionErrorCallback.h"
#include "modules/webdatabase/sqlite/SQLiteStatement.h"
#include "modules/webdatabase/sqlite/SQLiteTransaction.h"
#include "platform/Logging.h"
#include "platform/heap/SafePoint.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDatabaseObserver.h"
#include "public/platform/WebSecurityOrigin.h"
#include "wtf/Atomics.h"
#include "wtf/CurrentTime.h"
#include <memory>

// Registering "opened" databases with the DatabaseTracker
// =======================================================
// The DatabaseTracker maintains a list of databases that have been
// "opened" so that the client can call interrupt or delete on every database
// associated with a DatabaseContext.
//
// We will only call DatabaseTracker::addOpenDatabase() to add the database
// to the tracker as opened when we've succeeded in opening the database,
// and will set m_opened to true. Similarly, we only call
// DatabaseTracker::removeOpenDatabase() to remove the database from the
// tracker when we set m_opened to false in closeDatabase(). This sets up
// a simple symmetry between open and close operations, and a direct
// correlation to adding and removing databases from the tracker's list,
// thus ensuring that we have a correct list for the interrupt and
// delete operations to work on.
//
// The only databases instances not tracked by the tracker's open database
// list are the ones that have not been added yet, or the ones that we
// attempted an open on but failed to. Such instances only exist in the
// DatabaseServer's factory methods for creating database backends.
//
// The factory methods will either call openAndVerifyVersion() or
// performOpenAndVerify(). These methods will add the newly instantiated
// database backend if they succeed in opening the requested database.
// In the case of failure to open the database, the factory methods will
// simply discard the newly instantiated database backend when they return.
// The ref counting mechanims will automatically destruct the un-added
// (and un-returned) databases instances.

namespace blink {

// Defines static local variable after making sure that guid lock is held.
// (We can't use DEFINE_STATIC_LOCAL for this because it asserts thread
// safety, which is externally guaranteed by the guideMutex lock)
#define DEFINE_STATIC_LOCAL_WITH_LOCK(type, name, arguments) \
    ASSERT(guidMutex().locked()); \
    static type& name = *new type arguments

static const char versionKey[] = "WebKitDatabaseVersionKey";
static const char infoTableName[] = "__WebKitDatabaseInfoTable__";

static String formatErrorMessage(const char* message, int sqliteErrorCode, const char* sqliteErrorMessage)
{
    return String::format("%s (%d %s)", message, sqliteErrorCode, sqliteErrorMessage);
}

static bool retrieveTextResultFromDatabase(SQLiteDatabase& db, const String& query, String& resultString)
{
    SQLiteStatement statement(db, query);
    int result = statement.prepare();

    if (result != SQLResultOk) {
        DLOG(ERROR) << "Error (" << result << ") preparing statement to read text result from database (" << query << ")";
        return false;
    }

    result = statement.step();
    if (result == SQLResultRow) {
        resultString = statement.getColumnText(0);
        return true;
    }
    if (result == SQLResultDone) {
        resultString = String();
        return true;
    }

    DLOG(ERROR) << "Error (" << result << ") reading text result from database (" << query << ")";
    return false;
}

static bool setTextValueInDatabase(SQLiteDatabase& db, const String& query, const String& value)
{
    SQLiteStatement statement(db, query);
    int result = statement.prepare();

    if (result != SQLResultOk) {
        DLOG(ERROR) << "Failed to prepare statement to set value in database (" << query << ")";
        return false;
    }

    statement.bindText(1, value);

    result = statement.step();
    if (result != SQLResultDone) {
        DLOG(ERROR) << "Failed to step statement to set value in database (" << query << ")";
        return false;
    }

    return true;
}

// FIXME: move all guid-related functions to a DatabaseVersionTracker class.
static RecursiveMutex& guidMutex()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(RecursiveMutex, mutex, new RecursiveMutex);
    return mutex;
}

typedef HashMap<DatabaseGuid, String> GuidVersionMap;
static GuidVersionMap& guidToVersionMap()
{
    DEFINE_STATIC_LOCAL_WITH_LOCK(GuidVersionMap, map, ());
    return map;
}

// NOTE: Caller must lock guidMutex().
static inline void updateGuidVersionMap(DatabaseGuid guid, String newVersion)
{
    // Ensure the the mutex is locked.
    ASSERT(guidMutex().locked());

    // Note: It is not safe to put an empty string into the guidToVersionMap()
    // map. That's because the map is cross-thread, but empty strings are
    // per-thread. The copy() function makes a version of the string you can
    // use on the current thread, but we need a string we can keep in a
    // cross-thread data structure.
    // FIXME: This is a quite-awkward restriction to have to program with.

    // Map null string to empty string (see comment above).
    guidToVersionMap().set(guid, newVersion.isEmpty() ? String() : newVersion.isolatedCopy());
}

static HashCountedSet<DatabaseGuid>& guidCount()
{
    DEFINE_STATIC_LOCAL_WITH_LOCK(HashCountedSet<DatabaseGuid>, guidCount, ());
    return guidCount;
}

static DatabaseGuid guidForOriginAndName(const String& origin, const String& name)
{
    // Ensure the the mutex is locked.
    ASSERT(guidMutex().locked());

    String stringID = origin + "/" + name;

    typedef HashMap<String, int> IDGuidMap;
    DEFINE_STATIC_LOCAL_WITH_LOCK(IDGuidMap, stringIdentifierToGUIDMap, ());
    DatabaseGuid guid = stringIdentifierToGUIDMap.get(stringID);
    if (!guid) {
        static int currentNewGUID = 1;
        guid = currentNewGUID++;
        stringIdentifierToGUIDMap.set(stringID, guid);
    }

    return guid;
}

Database::Database(DatabaseContext* databaseContext, const String& name, const String& expectedVersion, const String& displayName, unsigned estimatedSize)
    : m_databaseContext(databaseContext)
    , m_name(name.isolatedCopy())
    , m_expectedVersion(expectedVersion.isolatedCopy())
    , m_displayName(displayName.isolatedCopy())
    , m_estimatedSize(estimatedSize)
    , m_guid(0)
    , m_opened(0)
    , m_new(false)
    , m_transactionInProgress(false)
    , m_isTransactionQueueEnabled(true)
{
    DCHECK(isMainThread());
    m_contextThreadSecurityOrigin = m_databaseContext->getSecurityOrigin()->isolatedCopy();

    m_databaseAuthorizer = DatabaseAuthorizer::create(infoTableName);

    if (m_name.isNull())
        m_name = "";

    {
        SafePointAwareMutexLocker locker(guidMutex());
        m_guid = guidForOriginAndName(getSecurityOrigin()->toString(), name);
        guidCount().add(m_guid);
    }

    m_filename = DatabaseManager::manager().fullPathForDatabase(getSecurityOrigin(), m_name);

    m_databaseThreadSecurityOrigin = m_contextThreadSecurityOrigin->isolatedCopy();
    ASSERT(m_databaseContext->databaseThread());
    ASSERT(m_databaseContext->isContextThread());
}

Database::~Database()
{
    // SQLite is "multi-thread safe", but each database handle can only be used
    // on a single thread at a time.
    //
    // For Database, we open the SQLite database on the DatabaseThread, and
    // hence we should also close it on that same thread. This means that the
    // SQLite database need to be closed by another mechanism (see
    // DatabaseContext::stopDatabases()). By the time we get here, the SQLite
    // database should have already been closed.

    ASSERT(!m_opened);
}

DEFINE_TRACE(Database)
{
    visitor->trace(m_databaseContext);
    visitor->trace(m_sqliteDatabase);
    visitor->trace(m_databaseAuthorizer);
}

bool Database::openAndVerifyVersion(bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    TaskSynchronizer synchronizer;
    if (!getDatabaseContext()->databaseThreadAvailable())
        return false;

    DatabaseTracker::tracker().prepareToOpenDatabase(this);
    bool success = false;
    std::unique_ptr<DatabaseOpenTask> task = DatabaseOpenTask::create(this, setVersionInNewDatabase, &synchronizer, error, errorMessage, success);
    getDatabaseContext()->databaseThread()->scheduleTask(std::move(task));
    synchronizer.waitForTaskCompletion();

    return success;
}

void Database::close()
{
    ASSERT(getDatabaseContext()->databaseThread());
    ASSERT(getDatabaseContext()->databaseThread()->isDatabaseThread());

    {
        MutexLocker locker(m_transactionInProgressMutex);

        // Clean up transactions that have not been scheduled yet:
        // Transaction phase 1 cleanup. See comment on "What happens if a
        // transaction is interrupted?" at the top of SQLTransactionBackend.cpp.
        SQLTransactionBackend* transaction = nullptr;
        while (!m_transactionQueue.isEmpty()) {
            transaction = m_transactionQueue.takeFirst();
            transaction->notifyDatabaseThreadIsShuttingDown();
        }

        m_isTransactionQueueEnabled = false;
        m_transactionInProgress = false;
    }

    closeDatabase();
    getDatabaseContext()->databaseThread()->recordDatabaseClosed(this);
}

SQLTransactionBackend* Database::runTransaction(SQLTransaction* transaction, bool readOnly, const ChangeVersionData* data)
{
    MutexLocker locker(m_transactionInProgressMutex);
    if (!m_isTransactionQueueEnabled)
        return nullptr;

    SQLTransactionWrapper* wrapper = nullptr;
    if (data)
        wrapper = ChangeVersionWrapper::create(data->oldVersion(), data->newVersion());

    SQLTransactionBackend* transactionBackend = SQLTransactionBackend::create(this, transaction, wrapper, readOnly);
    m_transactionQueue.append(transactionBackend);
    if (!m_transactionInProgress)
        scheduleTransaction();

    return transactionBackend;
}

void Database::inProgressTransactionCompleted()
{
    MutexLocker locker(m_transactionInProgressMutex);
    m_transactionInProgress = false;
    scheduleTransaction();
}

void Database::scheduleTransaction()
{
    ASSERT(!m_transactionInProgressMutex.tryLock()); // Locked by caller.
    SQLTransactionBackend* transaction = nullptr;

    if (m_isTransactionQueueEnabled && !m_transactionQueue.isEmpty())
        transaction = m_transactionQueue.takeFirst();

    if (transaction && getDatabaseContext()->databaseThreadAvailable()) {
        std::unique_ptr<DatabaseTransactionTask> task = DatabaseTransactionTask::create(transaction);
        WTF_LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for transaction %p\n", task.get(), task->transaction());
        m_transactionInProgress = true;
        getDatabaseContext()->databaseThread()->scheduleTask(std::move(task));
    } else {
        m_transactionInProgress = false;
    }
}

void Database::scheduleTransactionStep(SQLTransactionBackend* transaction)
{
    if (!getDatabaseContext()->databaseThreadAvailable())
        return;

    std::unique_ptr<DatabaseTransactionTask> task = DatabaseTransactionTask::create(transaction);
    WTF_LOG(StorageAPI, "Scheduling DatabaseTransactionTask %p for the transaction step\n", task.get());
    getDatabaseContext()->databaseThread()->scheduleTask(std::move(task));
}

SQLTransactionClient* Database::transactionClient() const
{
    return getDatabaseContext()->databaseThread()->transactionClient();
}

SQLTransactionCoordinator* Database::transactionCoordinator() const
{
    return getDatabaseContext()->databaseThread()->transactionCoordinator();
}

// static
const char* Database::databaseInfoTableName()
{
    return infoTableName;
}

void Database::closeDatabase()
{
    if (!m_opened)
        return;

    releaseStore(&m_opened, 0);
    m_sqliteDatabase.close();
    // See comment at the top this file regarding calling removeOpenDatabase().
    DatabaseTracker::tracker().removeOpenDatabase(this);
    {
        SafePointAwareMutexLocker locker(guidMutex());

        ASSERT(guidCount().contains(m_guid));
        if (guidCount().remove(m_guid)) {
            guidToVersionMap().remove(m_guid);
        }
    }
}

String Database::version() const
{
    // Note: In multi-process browsers the cached value may be accurate, but we
    // cannot read the actual version from the database without potentially
    // inducing a deadlock.
    // FIXME: Add an async version getter to the DatabaseAPI.
    return getCachedVersion();
}

class DoneCreatingDatabaseOnExitCaller {
    STACK_ALLOCATED();
public:
    DoneCreatingDatabaseOnExitCaller(Database* database)
        : m_database(database)
        , m_openSucceeded(false)
    {
    }
    ~DoneCreatingDatabaseOnExitCaller()
    {
        if (!m_openSucceeded)
            DatabaseTracker::tracker().failedToOpenDatabase(m_database);
    }

    void setOpenSucceeded() { m_openSucceeded = true; }

private:
    CrossThreadPersistent<Database> m_database;
    bool m_openSucceeded;
};

bool Database::performOpenAndVerify(bool shouldSetVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    double callStartTime = WTF::monotonicallyIncreasingTime();
    DoneCreatingDatabaseOnExitCaller onExitCaller(this);
    ASSERT(errorMessage.isEmpty());
    ASSERT(error == DatabaseError::None); // Better not have any errors already.
    // Presumed failure. We'll clear it if we succeed below.
    error = DatabaseError::InvalidDatabaseState;

    const int maxSqliteBusyWaitTime = 30000;

    if (!m_sqliteDatabase.open(m_filename)) {
        reportOpenDatabaseResult(1, InvalidStateError, m_sqliteDatabase.lastError(), WTF::monotonicallyIncreasingTime() - callStartTime);
        errorMessage = formatErrorMessage("unable to open database", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
        return false;
    }
    if (!m_sqliteDatabase.turnOnIncrementalAutoVacuum())
        DLOG(ERROR) << "Unable to turn on incremental auto-vacuum (" << m_sqliteDatabase.lastError() << " " << m_sqliteDatabase.lastErrorMsg() << ")";

    m_sqliteDatabase.setBusyTimeout(maxSqliteBusyWaitTime);

    String currentVersion;
    {
        SafePointAwareMutexLocker locker(guidMutex());

        GuidVersionMap::iterator entry = guidToVersionMap().find(m_guid);
        if (entry != guidToVersionMap().end()) {
            // Map null string to empty string (see updateGuidVersionMap()).
            currentVersion = entry->value.isNull() ? emptyString() : entry->value.isolatedCopy();
            WTF_LOG(StorageAPI, "Current cached version for guid %i is %s", m_guid, currentVersion.ascii().data());

            // Note: In multi-process browsers the cached value may be
            // inaccurate, but we cannot read the actual version from the
            // database without potentially inducing a form of deadlock, a
            // busytimeout error when trying to access the database. So we'll
            // use the cached value if we're unable to read the value from the
            // database file without waiting.
            // FIXME: Add an async openDatabase method to the DatabaseAPI.
            const int noSqliteBusyWaitTime = 0;
            m_sqliteDatabase.setBusyTimeout(noSqliteBusyWaitTime);
            String versionFromDatabase;
            if (getVersionFromDatabase(versionFromDatabase, false)) {
                currentVersion = versionFromDatabase;
                updateGuidVersionMap(m_guid, currentVersion);
            }
            m_sqliteDatabase.setBusyTimeout(maxSqliteBusyWaitTime);
        } else {
            WTF_LOG(StorageAPI, "No cached version for guid %i", m_guid);

            SQLiteTransaction transaction(m_sqliteDatabase);
            transaction.begin();
            if (!transaction.inProgress()) {
                reportOpenDatabaseResult(2, InvalidStateError, m_sqliteDatabase.lastError(), WTF::monotonicallyIncreasingTime() - callStartTime);
                errorMessage = formatErrorMessage("unable to open database, failed to start transaction", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                m_sqliteDatabase.close();
                return false;
            }

            String tableName(infoTableName);
            if (!m_sqliteDatabase.tableExists(tableName)) {
                m_new = true;

                if (!m_sqliteDatabase.executeCommand("CREATE TABLE " + tableName + " (key TEXT NOT NULL ON CONFLICT FAIL UNIQUE ON CONFLICT REPLACE,value TEXT NOT NULL ON CONFLICT FAIL);")) {
                    reportOpenDatabaseResult(3, InvalidStateError, m_sqliteDatabase.lastError(), WTF::monotonicallyIncreasingTime() - callStartTime);
                    errorMessage = formatErrorMessage("unable to open database, failed to create 'info' table", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                    transaction.rollback();
                    m_sqliteDatabase.close();
                    return false;
                }
            } else if (!getVersionFromDatabase(currentVersion, false)) {
                reportOpenDatabaseResult(4, InvalidStateError, m_sqliteDatabase.lastError(), WTF::monotonicallyIncreasingTime() - callStartTime);
                errorMessage = formatErrorMessage("unable to open database, failed to read current version", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                transaction.rollback();
                m_sqliteDatabase.close();
                return false;
            }

            if (currentVersion.length()) {
                WTF_LOG(StorageAPI, "Retrieved current version %s from database %s", currentVersion.ascii().data(), databaseDebugName().ascii().data());
            } else if (!m_new || shouldSetVersionInNewDatabase) {
                WTF_LOG(StorageAPI, "Setting version %s in database %s that was just created", m_expectedVersion.ascii().data(), databaseDebugName().ascii().data());
                if (!setVersionInDatabase(m_expectedVersion, false)) {
                    reportOpenDatabaseResult(5, InvalidStateError, m_sqliteDatabase.lastError(), WTF::monotonicallyIncreasingTime() - callStartTime);
                    errorMessage = formatErrorMessage("unable to open database, failed to write current version", m_sqliteDatabase.lastError(), m_sqliteDatabase.lastErrorMsg());
                    transaction.rollback();
                    m_sqliteDatabase.close();
                    return false;
                }
                currentVersion = m_expectedVersion;
            }
            updateGuidVersionMap(m_guid, currentVersion);
            transaction.commit();
        }
    }

    if (currentVersion.isNull()) {
        WTF_LOG(StorageAPI, "Database %s does not have its version set", databaseDebugName().ascii().data());
        currentVersion = "";
    }

    // If the expected version isn't the empty string, ensure that the current
    // database version we have matches that version. Otherwise, set an
    // exception.
    // If the expected version is the empty string, then we always return with
    // whatever version of the database we have.
    if ((!m_new || shouldSetVersionInNewDatabase) && m_expectedVersion.length() && m_expectedVersion != currentVersion) {
        reportOpenDatabaseResult(6, InvalidStateError, 0, WTF::monotonicallyIncreasingTime() - callStartTime);
        errorMessage = "unable to open database, version mismatch, '" + m_expectedVersion + "' does not match the currentVersion of '" + currentVersion + "'";
        m_sqliteDatabase.close();
        return false;
    }

    ASSERT(m_databaseAuthorizer);
    m_sqliteDatabase.setAuthorizer(m_databaseAuthorizer.get());

    // See comment at the top this file regarding calling addOpenDatabase().
    DatabaseTracker::tracker().addOpenDatabase(this);
    m_opened = 1;

    // Declare success:
    error = DatabaseError::None; // Clear the presumed error from above.
    onExitCaller.setOpenSucceeded();

    if (m_new && !shouldSetVersionInNewDatabase) {
        // The caller provided a creationCallback which will set the expected
        // version.
        m_expectedVersion = "";
    }

    reportOpenDatabaseResult(0, -1, 0, WTF::monotonicallyIncreasingTime() - callStartTime); // OK

    if (getDatabaseContext()->databaseThread())
        getDatabaseContext()->databaseThread()->recordDatabaseOpen(this);
    return true;
}

String Database::stringIdentifier() const
{
    // Return a deep copy for ref counting thread safety
    return m_name.isolatedCopy();
}

String Database::displayName() const
{
    // Return a deep copy for ref counting thread safety
    return m_displayName.isolatedCopy();
}

unsigned Database::estimatedSize() const
{
    return m_estimatedSize;
}

String Database::fileName() const
{
    // Return a deep copy for ref counting thread safety
    return m_filename.isolatedCopy();
}

bool Database::getVersionFromDatabase(String& version, bool shouldCacheVersion)
{
    String query(String("SELECT value FROM ") + infoTableName +  " WHERE key = '" + versionKey + "';");

    m_databaseAuthorizer->disable();

    bool result = retrieveTextResultFromDatabase(m_sqliteDatabase, query, version);
    if (result) {
        if (shouldCacheVersion)
            setCachedVersion(version);
    } else {
        DLOG(ERROR) << "Failed to retrieve version from database " << databaseDebugName();
    }

    m_databaseAuthorizer->enable();

    return result;
}

bool Database::setVersionInDatabase(const String& version, bool shouldCacheVersion)
{
    // The INSERT will replace an existing entry for the database with the new
    // version number, due to the UNIQUE ON CONFLICT REPLACE clause in the
    // CREATE statement (see Database::performOpenAndVerify()).
    String query(String("INSERT INTO ") + infoTableName +  " (key, value) VALUES ('" + versionKey + "', ?);");

    m_databaseAuthorizer->disable();

    bool result = setTextValueInDatabase(m_sqliteDatabase, query, version);
    if (result) {
        if (shouldCacheVersion)
            setCachedVersion(version);
    } else {
        DLOG(ERROR) << "Failed to set version " << version << " in database (" << query << ")";
    }

    m_databaseAuthorizer->enable();

    return result;
}

void Database::setExpectedVersion(const String& version)
{
    m_expectedVersion = version.isolatedCopy();
}

String Database::getCachedVersion() const
{
    SafePointAwareMutexLocker locker(guidMutex());
    return guidToVersionMap().get(m_guid).isolatedCopy();
}

void Database::setCachedVersion(const String& actualVersion)
{
    // Update the in memory database version map.
    SafePointAwareMutexLocker locker(guidMutex());
    updateGuidVersionMap(m_guid, actualVersion);
}

bool Database::getActualVersionForTransaction(String& actualVersion)
{
    ASSERT(m_sqliteDatabase.transactionInProgress());
    // Note: In multi-process browsers the cached value may be inaccurate. So we
    // retrieve the value from the database and update the cached value here.
    return getVersionFromDatabase(actualVersion, true);
}

void Database::disableAuthorizer()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->disable();
}

void Database::enableAuthorizer()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->enable();
}

void Database::setAuthorizerPermissions(int permissions)
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->setPermissions(permissions);
}

bool Database::lastActionChangedDatabase()
{
    ASSERT(m_databaseAuthorizer);
    return m_databaseAuthorizer->lastActionChangedDatabase();
}

bool Database::lastActionWasInsert()
{
    ASSERT(m_databaseAuthorizer);
    return m_databaseAuthorizer->lastActionWasInsert();
}

void Database::resetDeletes()
{
    ASSERT(m_databaseAuthorizer);
    m_databaseAuthorizer->resetDeletes();
}

bool Database::hadDeletes()
{
    ASSERT(m_databaseAuthorizer);
    return m_databaseAuthorizer->hadDeletes();
}

void Database::resetAuthorizer()
{
    if (m_databaseAuthorizer)
        m_databaseAuthorizer->reset();
}

unsigned long long Database::maximumSize() const
{
    return DatabaseTracker::tracker().getMaxSizeForDatabase(this);
}

void Database::incrementalVacuumIfNeeded()
{
    int64_t freeSpaceSize = m_sqliteDatabase.freeSpaceSize();
    int64_t totalSize = m_sqliteDatabase.totalSize();
    if (totalSize <= 10 * freeSpaceSize) {
        int result = m_sqliteDatabase.runIncrementalVacuumCommand();
        reportVacuumDatabaseResult(result);
        if (result != SQLResultOk)
            logErrorMessage(formatErrorMessage("error vacuuming database", result, m_sqliteDatabase.lastErrorMsg()));
    }
}

// These are used to generate histograms of errors seen with websql.
// See about:histograms in chromium.
void Database::reportOpenDatabaseResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode, double duration)
{
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->reportOpenDatabaseResult(
            WebSecurityOrigin(getSecurityOrigin()),
            stringIdentifier(), errorSite, webSqlErrorCode, sqliteErrorCode,
            duration);
    }
}

void Database::reportChangeVersionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->reportChangeVersionResult(
            WebSecurityOrigin(getSecurityOrigin()),
            stringIdentifier(), errorSite, webSqlErrorCode, sqliteErrorCode);
    }
}

void Database::reportStartTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->reportStartTransactionResult(
            WebSecurityOrigin(getSecurityOrigin()),
            stringIdentifier(), errorSite, webSqlErrorCode, sqliteErrorCode);
    }
}

void Database::reportCommitTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->reportCommitTransactionResult(
            WebSecurityOrigin(getSecurityOrigin()),
            stringIdentifier(), errorSite, webSqlErrorCode, sqliteErrorCode);
    }
}

void Database::reportExecuteStatementResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->reportExecuteStatementResult(
            WebSecurityOrigin(getSecurityOrigin()),
            stringIdentifier(), errorSite, webSqlErrorCode, sqliteErrorCode);
    }
}

void Database::reportVacuumDatabaseResult(int sqliteErrorCode)
{
    if (Platform::current()->databaseObserver()) {
        Platform::current()->databaseObserver()->reportVacuumDatabaseResult(
            WebSecurityOrigin(getSecurityOrigin()),
            stringIdentifier(), sqliteErrorCode);
    }
}

void Database::logErrorMessage(const String& message)
{
    getExecutionContext()->addConsoleMessage(ConsoleMessage::create(StorageMessageSource, ErrorMessageLevel, message));
}

ExecutionContext* Database::getExecutionContext() const
{
    return getDatabaseContext()->getExecutionContext();
}

void Database::closeImmediately()
{
    ASSERT(getExecutionContext()->isContextThread());
    if (getDatabaseContext()->databaseThreadAvailable() && opened()) {
        logErrorMessage("forcibly closing database");
        getDatabaseContext()->databaseThread()->scheduleTask(DatabaseCloseTask::create(this, 0));
    }
}

void Database::changeVersion(
    const String& oldVersion,
    const String& newVersion,
    SQLTransactionCallback* callback,
    SQLTransactionErrorCallback* errorCallback,
    VoidCallback* successCallback)
{
    ChangeVersionData data(oldVersion, newVersion);
    runTransaction(callback, errorCallback, successCallback, false, &data);
}

void Database::transaction(
    SQLTransactionCallback* callback,
    SQLTransactionErrorCallback* errorCallback,
    VoidCallback* successCallback)
{
    runTransaction(callback, errorCallback, successCallback, false);
}

void Database::readTransaction(
    SQLTransactionCallback* callback,
    SQLTransactionErrorCallback* errorCallback,
    VoidCallback* successCallback)
{
    runTransaction(callback, errorCallback, successCallback, true);
}

static void callTransactionErrorCallback(SQLTransactionErrorCallback* callback, std::unique_ptr<SQLErrorData> errorData)
{
    callback->handleEvent(SQLError::create(*errorData));
}

void Database::runTransaction(
    SQLTransactionCallback* callback,
    SQLTransactionErrorCallback* errorCallback,
    VoidCallback* successCallback,
    bool readOnly,
    const ChangeVersionData* changeVersionData)
{
    ASSERT(getExecutionContext()->isContextThread());
    // FIXME: Rather than passing errorCallback to SQLTransaction and then
    // sometimes firing it ourselves, this code should probably be pushed down
    // into Database so that we only create the SQLTransaction if we're
    // actually going to run it.
#if ENABLE(ASSERT)
    SQLTransactionErrorCallback* originalErrorCallback = errorCallback;
#endif
    SQLTransaction* transaction = SQLTransaction::create(this, callback, successCallback, errorCallback, readOnly);
    SQLTransactionBackend* transactionBackend = runTransaction(transaction, readOnly, changeVersionData);
    if (!transactionBackend) {
        SQLTransactionErrorCallback* callback = transaction->releaseErrorCallback();
        ASSERT(callback == originalErrorCallback);
        if (callback) {
            std::unique_ptr<SQLErrorData> error = SQLErrorData::create(SQLError::UNKNOWN_ERR, "database has been closed");
            getExecutionContext()->postTask(BLINK_FROM_HERE, createSameThreadTask(&callTransactionErrorCallback, wrapPersistent(callback), passed(std::move(error))));
        }
    }
}

void Database::scheduleTransactionCallback(SQLTransaction* transaction)
{
    // The task is constructed in a database thread, and destructed in the
    // context thread.
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&SQLTransaction::performPendingCallback, wrapCrossThreadPersistent(transaction)));
}

Vector<String> Database::performGetTableNames()
{
    disableAuthorizer();

    SQLiteStatement statement(sqliteDatabase(), "SELECT name FROM sqlite_master WHERE type='table';");
    if (statement.prepare() != SQLResultOk) {
        DLOG(ERROR) << "Unable to retrieve list of tables for database " << databaseDebugName();
        enableAuthorizer();
        return Vector<String>();
    }

    Vector<String> tableNames;
    int result;
    while ((result = statement.step()) == SQLResultRow) {
        String name = statement.getColumnText(0);
        if (name != databaseInfoTableName())
            tableNames.append(name);
    }

    enableAuthorizer();

    if (result != SQLResultDone) {
        DLOG(ERROR) << "Error getting tables for database " << databaseDebugName();
        return Vector<String>();
    }

    return tableNames;
}

Vector<String> Database::tableNames()
{
    // FIXME: Not using isolatedCopy on these strings looks ok since threads
    // take strict turns in dealing with them. However, if the code changes,
    // this may not be true anymore.
    Vector<String> result;
    TaskSynchronizer synchronizer;
    if (!getDatabaseContext()->databaseThreadAvailable())
        return result;

    std::unique_ptr<DatabaseTableNamesTask> task = DatabaseTableNamesTask::create(this, &synchronizer, result);
    getDatabaseContext()->databaseThread()->scheduleTask(std::move(task));
    synchronizer.waitForTaskCompletion();

    return result;
}

SecurityOrigin* Database::getSecurityOrigin() const
{
    if (getExecutionContext()->isContextThread())
        return m_contextThreadSecurityOrigin.get();
    if (getDatabaseContext()->databaseThread()->isDatabaseThread())
        return m_databaseThreadSecurityOrigin.get();
    return 0;
}

bool Database::opened()
{
    return static_cast<bool>(acquireLoad(&m_opened));
}

} // namespace blink
