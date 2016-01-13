// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/connection.h"

#include <string.h>

#include "base/files/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

#if defined(OS_IOS) && defined(USE_SYSTEM_SQLITE)
#include "third_party/sqlite/src/ext/icu/sqliteicu.h"
#endif

namespace {

// Spin for up to a second waiting for the lock to clear when setting
// up the database.
// TODO(shess): Better story on this.  http://crbug.com/56559
const int kBusyTimeoutSeconds = 1;

class ScopedBusyTimeout {
 public:
  explicit ScopedBusyTimeout(sqlite3* db)
      : db_(db) {
  }
  ~ScopedBusyTimeout() {
    sqlite3_busy_timeout(db_, 0);
  }

  int SetTimeout(base::TimeDelta timeout) {
    DCHECK_LT(timeout.InMilliseconds(), INT_MAX);
    return sqlite3_busy_timeout(db_,
                                static_cast<int>(timeout.InMilliseconds()));
  }

 private:
  sqlite3* db_;
};

// Helper to "safely" enable writable_schema.  No error checking
// because it is reasonable to just forge ahead in case of an error.
// If turning it on fails, then most likely nothing will work, whereas
// if turning it off fails, it only matters if some code attempts to
// continue working with the database and tries to modify the
// sqlite_master table (none of our code does this).
class ScopedWritableSchema {
 public:
  explicit ScopedWritableSchema(sqlite3* db)
      : db_(db) {
    sqlite3_exec(db_, "PRAGMA writable_schema=1", NULL, NULL, NULL);
  }
  ~ScopedWritableSchema() {
    sqlite3_exec(db_, "PRAGMA writable_schema=0", NULL, NULL, NULL);
  }

 private:
  sqlite3* db_;
};

// Helper to wrap the sqlite3_backup_*() step of Raze().  Return
// SQLite error code from running the backup step.
int BackupDatabase(sqlite3* src, sqlite3* dst, const char* db_name) {
  DCHECK_NE(src, dst);
  sqlite3_backup* backup = sqlite3_backup_init(dst, db_name, src, db_name);
  if (!backup) {
    // Since this call only sets things up, this indicates a gross
    // error in SQLite.
    DLOG(FATAL) << "Unable to start sqlite3_backup(): " << sqlite3_errmsg(dst);
    return sqlite3_errcode(dst);
  }

  // -1 backs up the entire database.
  int rc = sqlite3_backup_step(backup, -1);
  int pages = sqlite3_backup_pagecount(backup);
  sqlite3_backup_finish(backup);

  // If successful, exactly one page should have been backed up.  If
  // this breaks, check this function to make sure assumptions aren't
  // being broken.
  if (rc == SQLITE_DONE)
    DCHECK_EQ(pages, 1);

  return rc;
}

// Be very strict on attachment point.  SQLite can handle a much wider
// character set with appropriate quoting, but Chromium code should
// just use clean names to start with.
bool ValidAttachmentPoint(const char* attachment_point) {
  for (size_t i = 0; attachment_point[i]; ++i) {
    if (!((attachment_point[i] >= '0' && attachment_point[i] <= '9') ||
          (attachment_point[i] >= 'a' && attachment_point[i] <= 'z') ||
          (attachment_point[i] >= 'A' && attachment_point[i] <= 'Z') ||
          attachment_point[i] == '_')) {
      return false;
    }
  }
  return true;
}

// SQLite automatically calls sqlite3_initialize() lazily, but
// sqlite3_initialize() uses double-checked locking and thus can have
// data races.
//
// TODO(shess): Another alternative would be to have
// sqlite3_initialize() called as part of process bring-up.  If this
// is changed, remove the dynamic_annotations dependency in sql.gyp.
base::LazyInstance<base::Lock>::Leaky
    g_sqlite_init_lock = LAZY_INSTANCE_INITIALIZER;
void InitializeSqlite() {
  base::AutoLock lock(g_sqlite_init_lock.Get());
  sqlite3_initialize();
}

// Helper to get the sqlite3_file* associated with the "main" database.
int GetSqlite3File(sqlite3* db, sqlite3_file** file) {
  *file = NULL;
  int rc = sqlite3_file_control(db, NULL, SQLITE_FCNTL_FILE_POINTER, file);
  if (rc != SQLITE_OK)
    return rc;

  // TODO(shess): NULL in file->pMethods has been observed on android_dbg
  // content_unittests, even though it should not be possible.
  // http://crbug.com/329982
  if (!*file || !(*file)->pMethods)
    return SQLITE_ERROR;

  return rc;
}

}  // namespace

namespace sql {

// static
Connection::ErrorIgnorerCallback* Connection::current_ignorer_cb_ = NULL;

// static
bool Connection::ShouldIgnoreSqliteError(int error) {
  if (!current_ignorer_cb_)
    return false;
  return current_ignorer_cb_->Run(error);
}

// static
void Connection::SetErrorIgnorer(Connection::ErrorIgnorerCallback* cb) {
  CHECK(current_ignorer_cb_ == NULL);
  current_ignorer_cb_ = cb;
}

// static
void Connection::ResetErrorIgnorer() {
  CHECK(current_ignorer_cb_);
  current_ignorer_cb_ = NULL;
}

bool StatementID::operator<(const StatementID& other) const {
  if (number_ != other.number_)
    return number_ < other.number_;
  return strcmp(str_, other.str_) < 0;
}

Connection::StatementRef::StatementRef(Connection* connection,
                                       sqlite3_stmt* stmt,
                                       bool was_valid)
    : connection_(connection),
      stmt_(stmt),
      was_valid_(was_valid) {
  if (connection)
    connection_->StatementRefCreated(this);
}

Connection::StatementRef::~StatementRef() {
  if (connection_)
    connection_->StatementRefDeleted(this);
  Close(false);
}

void Connection::StatementRef::Close(bool forced) {
  if (stmt_) {
    // Call to AssertIOAllowed() cannot go at the beginning of the function
    // because Close() is called unconditionally from destructor to clean
    // connection_. And if this is inactive statement this won't cause any
    // disk access and destructor most probably will be called on thread
    // not allowing disk access.
    // TODO(paivanof@gmail.com): This should move to the beginning
    // of the function. http://crbug.com/136655.
    AssertIOAllowed();
    sqlite3_finalize(stmt_);
    stmt_ = NULL;
  }
  connection_ = NULL;  // The connection may be getting deleted.

  // Forced close is expected to happen from a statement error
  // handler.  In that case maintain the sense of |was_valid_| which
  // previously held for this ref.
  was_valid_ = was_valid_ && forced;
}

Connection::Connection()
    : db_(NULL),
      page_size_(0),
      cache_size_(0),
      exclusive_locking_(false),
      restrict_to_user_(false),
      transaction_nesting_(0),
      needs_rollback_(false),
      in_memory_(false),
      poisoned_(false) {
}

Connection::~Connection() {
  Close();
}

bool Connection::Open(const base::FilePath& path) {
  if (!histogram_tag_.empty()) {
    int64 size_64 = 0;
    if (base::GetFileSize(path, &size_64)) {
      size_t sample = static_cast<size_t>(size_64 / 1024);
      std::string full_histogram_name = "Sqlite.SizeKB." + histogram_tag_;
      base::HistogramBase* histogram =
          base::Histogram::FactoryGet(
              full_histogram_name, 1, 1000000, 50,
              base::HistogramBase::kUmaTargetedHistogramFlag);
      if (histogram)
        histogram->Add(sample);
    }
  }

#if defined(OS_WIN)
  return OpenInternal(base::WideToUTF8(path.value()), RETRY_ON_POISON);
#elif defined(OS_POSIX)
  return OpenInternal(path.value(), RETRY_ON_POISON);
#endif
}

bool Connection::OpenInMemory() {
  in_memory_ = true;
  return OpenInternal(":memory:", NO_RETRY);
}

bool Connection::OpenTemporary() {
  return OpenInternal("", NO_RETRY);
}

void Connection::CloseInternal(bool forced) {
  // TODO(shess): Calling "PRAGMA journal_mode = DELETE" at this point
  // will delete the -journal file.  For ChromiumOS or other more
  // embedded systems, this is probably not appropriate, whereas on
  // desktop it might make some sense.

  // sqlite3_close() needs all prepared statements to be finalized.

  // Release cached statements.
  statement_cache_.clear();

  // With cached statements released, in-use statements will remain.
  // Closing the database while statements are in use is an API
  // violation, except for forced close (which happens from within a
  // statement's error handler).
  DCHECK(forced || open_statements_.empty());

  // Deactivate any outstanding statements so sqlite3_close() works.
  for (StatementRefSet::iterator i = open_statements_.begin();
       i != open_statements_.end(); ++i)
    (*i)->Close(forced);
  open_statements_.clear();

  if (db_) {
    // Call to AssertIOAllowed() cannot go at the beginning of the function
    // because Close() must be called from destructor to clean
    // statement_cache_, it won't cause any disk access and it most probably
    // will happen on thread not allowing disk access.
    // TODO(paivanof@gmail.com): This should move to the beginning
    // of the function. http://crbug.com/136655.
    AssertIOAllowed();

    int rc = sqlite3_close(db_);
    if (rc != SQLITE_OK) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.CloseFailure", rc);
      DLOG(FATAL) << "sqlite3_close failed: " << GetErrorMessage();
    }
  }
  db_ = NULL;
}

void Connection::Close() {
  // If the database was already closed by RazeAndClose(), then no
  // need to close again.  Clear the |poisoned_| bit so that incorrect
  // API calls are caught.
  if (poisoned_) {
    poisoned_ = false;
    return;
  }

  CloseInternal(false);
}

void Connection::Preload() {
  AssertIOAllowed();

  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Cannot preload null db";
    return;
  }

  // Use local settings if provided, otherwise use documented defaults.  The
  // actual results could be fetching via PRAGMA calls.
  const int page_size = page_size_ ? page_size_ : 1024;
  sqlite3_int64 preload_size = page_size * (cache_size_ ? cache_size_ : 2000);
  if (preload_size < 1)
    return;

  sqlite3_file* file = NULL;
  int rc = GetSqlite3File(db_, &file);
  if (rc != SQLITE_OK)
    return;

  sqlite3_int64 file_size = 0;
  rc = file->pMethods->xFileSize(file, &file_size);
  if (rc != SQLITE_OK)
    return;

  // Don't preload more than the file contains.
  if (preload_size > file_size)
    preload_size = file_size;

  scoped_ptr<char[]> buf(new char[page_size]);
  for (sqlite3_int64 pos = 0; pos < file_size; pos += page_size) {
    rc = file->pMethods->xRead(file, buf.get(), page_size, pos);
    if (rc != SQLITE_OK)
      return;
  }
}

void Connection::TrimMemory(bool aggressively) {
  if (!db_)
    return;

  // TODO(shess): investigate using sqlite3_db_release_memory() when possible.
  int original_cache_size;
  {
    Statement sql_get_original(GetUniqueStatement("PRAGMA cache_size"));
    if (!sql_get_original.Step()) {
      DLOG(WARNING) << "Could not get cache size " << GetErrorMessage();
      return;
    }
    original_cache_size = sql_get_original.ColumnInt(0);
  }
  int shrink_cache_size = aggressively ? 1 : (original_cache_size / 2);

  // Force sqlite to try to reduce page cache usage.
  const std::string sql_shrink =
      base::StringPrintf("PRAGMA cache_size=%d", shrink_cache_size);
  if (!Execute(sql_shrink.c_str()))
    DLOG(WARNING) << "Could not shrink cache size: " << GetErrorMessage();

  // Restore cache size.
  const std::string sql_restore =
      base::StringPrintf("PRAGMA cache_size=%d", original_cache_size);
  if (!Execute(sql_restore.c_str()))
    DLOG(WARNING) << "Could not restore cache size: " << GetErrorMessage();
}

// Create an in-memory database with the existing database's page
// size, then backup that database over the existing database.
bool Connection::Raze() {
  AssertIOAllowed();

  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Cannot raze null db";
    return false;
  }

  if (transaction_nesting_ > 0) {
    DLOG(FATAL) << "Cannot raze within a transaction";
    return false;
  }

  sql::Connection null_db;
  if (!null_db.OpenInMemory()) {
    DLOG(FATAL) << "Unable to open in-memory database.";
    return false;
  }

  if (page_size_) {
    // Enforce SQLite restrictions on |page_size_|.
    DCHECK(!(page_size_ & (page_size_ - 1)))
        << " page_size_ " << page_size_ << " is not a power of two.";
    const int kSqliteMaxPageSize = 32768;  // from sqliteLimit.h
    DCHECK_LE(page_size_, kSqliteMaxPageSize);
    const std::string sql =
        base::StringPrintf("PRAGMA page_size=%d", page_size_);
    if (!null_db.Execute(sql.c_str()))
      return false;
  }

#if defined(OS_ANDROID)
  // Android compiles with SQLITE_DEFAULT_AUTOVACUUM.  Unfortunately,
  // in-memory databases do not respect this define.
  // TODO(shess): Figure out a way to set this without using platform
  // specific code.  AFAICT from sqlite3.c, the only way to do it
  // would be to create an actual filesystem database, which is
  // unfortunate.
  if (!null_db.Execute("PRAGMA auto_vacuum = 1"))
    return false;
#endif

  // The page size doesn't take effect until a database has pages, and
  // at this point the null database has none.  Changing the schema
  // version will create the first page.  This will not affect the
  // schema version in the resulting database, as SQLite's backup
  // implementation propagates the schema version from the original
  // connection to the new version of the database, incremented by one
  // so that other readers see the schema change and act accordingly.
  if (!null_db.Execute("PRAGMA schema_version = 1"))
    return false;

  // SQLite tracks the expected number of database pages in the first
  // page, and if it does not match the total retrieved from a
  // filesystem call, treats the database as corrupt.  This situation
  // breaks almost all SQLite calls.  "PRAGMA writable_schema" can be
  // used to hint to SQLite to soldier on in that case, specifically
  // for purposes of recovery.  [See SQLITE_CORRUPT_BKPT case in
  // sqlite3.c lockBtree().]
  // TODO(shess): With this, "PRAGMA auto_vacuum" and "PRAGMA
  // page_size" can be used to query such a database.
  ScopedWritableSchema writable_schema(db_);

  const char* kMain = "main";
  int rc = BackupDatabase(null_db.db_, db_, kMain);
  UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.RazeDatabase",rc);

  // The destination database was locked.
  if (rc == SQLITE_BUSY) {
    return false;
  }

  // SQLITE_NOTADB can happen if page 1 of db_ exists, but is not
  // formatted correctly.  SQLITE_IOERR_SHORT_READ can happen if db_
  // isn't even big enough for one page.  Either way, reach in and
  // truncate it before trying again.
  // TODO(shess): Maybe it would be worthwhile to just truncate from
  // the get-go?
  if (rc == SQLITE_NOTADB || rc == SQLITE_IOERR_SHORT_READ) {
    sqlite3_file* file = NULL;
    rc = GetSqlite3File(db_, &file);
    if (rc != SQLITE_OK) {
      DLOG(FATAL) << "Failure getting file handle.";
      return false;
    }

    rc = file->pMethods->xTruncate(file, 0);
    if (rc != SQLITE_OK) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.RazeDatabaseTruncate",rc);
      DLOG(FATAL) << "Failed to truncate file.";
      return false;
    }

    rc = BackupDatabase(null_db.db_, db_, kMain);
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.RazeDatabase2",rc);

    if (rc != SQLITE_DONE) {
      DLOG(FATAL) << "Failed retrying Raze().";
    }
  }

  // The entire database should have been backed up.
  if (rc != SQLITE_DONE) {
    // TODO(shess): Figure out which other cases can happen.
    DLOG(FATAL) << "Unable to copy entire null database.";
    return false;
  }

  return true;
}

bool Connection::RazeWithTimout(base::TimeDelta timeout) {
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Cannot raze null db";
    return false;
  }

  ScopedBusyTimeout busy_timeout(db_);
  busy_timeout.SetTimeout(timeout);
  return Raze();
}

bool Connection::RazeAndClose() {
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Cannot raze null db";
    return false;
  }

  // Raze() cannot run in a transaction.
  RollbackAllTransactions();

  bool result = Raze();

  CloseInternal(true);

  // Mark the database so that future API calls fail appropriately,
  // but don't DCHECK (because after calling this function they are
  // expected to fail).
  poisoned_ = true;

  return result;
}

void Connection::Poison() {
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Cannot poison null db";
    return;
  }

  RollbackAllTransactions();
  CloseInternal(true);

  // Mark the database so that future API calls fail appropriately,
  // but don't DCHECK (because after calling this function they are
  // expected to fail).
  poisoned_ = true;
}

// TODO(shess): To the extent possible, figure out the optimal
// ordering for these deletes which will prevent other connections
// from seeing odd behavior.  For instance, it may be necessary to
// manually lock the main database file in a SQLite-compatible fashion
// (to prevent other processes from opening it), then delete the
// journal files, then delete the main database file.  Another option
// might be to lock the main database file and poison the header with
// junk to prevent other processes from opening it successfully (like
// Gears "SQLite poison 3" trick).
//
// static
bool Connection::Delete(const base::FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  base::FilePath journal_path(path.value() + FILE_PATH_LITERAL("-journal"));
  base::FilePath wal_path(path.value() + FILE_PATH_LITERAL("-wal"));

  base::DeleteFile(journal_path, false);
  base::DeleteFile(wal_path, false);
  base::DeleteFile(path, false);

  return !base::PathExists(journal_path) &&
      !base::PathExists(wal_path) &&
      !base::PathExists(path);
}

bool Connection::BeginTransaction() {
  if (needs_rollback_) {
    DCHECK_GT(transaction_nesting_, 0);

    // When we're going to rollback, fail on this begin and don't actually
    // mark us as entering the nested transaction.
    return false;
  }

  bool success = true;
  if (!transaction_nesting_) {
    needs_rollback_ = false;

    Statement begin(GetCachedStatement(SQL_FROM_HERE, "BEGIN TRANSACTION"));
    if (!begin.Run())
      return false;
  }
  transaction_nesting_++;
  return success;
}

void Connection::RollbackTransaction() {
  if (!transaction_nesting_) {
    DLOG_IF(FATAL, !poisoned_) << "Rolling back a nonexistent transaction";
    return;
  }

  transaction_nesting_--;

  if (transaction_nesting_ > 0) {
    // Mark the outermost transaction as needing rollback.
    needs_rollback_ = true;
    return;
  }

  DoRollback();
}

bool Connection::CommitTransaction() {
  if (!transaction_nesting_) {
    DLOG_IF(FATAL, !poisoned_) << "Rolling back a nonexistent transaction";
    return false;
  }
  transaction_nesting_--;

  if (transaction_nesting_ > 0) {
    // Mark any nested transactions as failing after we've already got one.
    return !needs_rollback_;
  }

  if (needs_rollback_) {
    DoRollback();
    return false;
  }

  Statement commit(GetCachedStatement(SQL_FROM_HERE, "COMMIT"));
  return commit.Run();
}

void Connection::RollbackAllTransactions() {
  if (transaction_nesting_ > 0) {
    transaction_nesting_ = 0;
    DoRollback();
  }
}

bool Connection::AttachDatabase(const base::FilePath& other_db_path,
                                const char* attachment_point) {
  DCHECK(ValidAttachmentPoint(attachment_point));

  Statement s(GetUniqueStatement("ATTACH DATABASE ? AS ?"));
#if OS_WIN
  s.BindString16(0, other_db_path.value());
#else
  s.BindString(0, other_db_path.value());
#endif
  s.BindString(1, attachment_point);
  return s.Run();
}

bool Connection::DetachDatabase(const char* attachment_point) {
  DCHECK(ValidAttachmentPoint(attachment_point));

  Statement s(GetUniqueStatement("DETACH DATABASE ?"));
  s.BindString(0, attachment_point);
  return s.Run();
}

int Connection::ExecuteAndReturnErrorCode(const char* sql) {
  AssertIOAllowed();
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Illegal use of connection without a db";
    return SQLITE_ERROR;
  }
  return sqlite3_exec(db_, sql, NULL, NULL, NULL);
}

bool Connection::Execute(const char* sql) {
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Illegal use of connection without a db";
    return false;
  }

  int error = ExecuteAndReturnErrorCode(sql);
  if (error != SQLITE_OK)
    error = OnSqliteError(error, NULL, sql);

  // This needs to be a FATAL log because the error case of arriving here is
  // that there's a malformed SQL statement. This can arise in development if
  // a change alters the schema but not all queries adjust.  This can happen
  // in production if the schema is corrupted.
  if (error == SQLITE_ERROR)
    DLOG(FATAL) << "SQL Error in " << sql << ", " << GetErrorMessage();
  return error == SQLITE_OK;
}

bool Connection::ExecuteWithTimeout(const char* sql, base::TimeDelta timeout) {
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Illegal use of connection without a db";
    return false;
  }

  ScopedBusyTimeout busy_timeout(db_);
  busy_timeout.SetTimeout(timeout);
  return Execute(sql);
}

bool Connection::HasCachedStatement(const StatementID& id) const {
  return statement_cache_.find(id) != statement_cache_.end();
}

scoped_refptr<Connection::StatementRef> Connection::GetCachedStatement(
    const StatementID& id,
    const char* sql) {
  CachedStatementMap::iterator i = statement_cache_.find(id);
  if (i != statement_cache_.end()) {
    // Statement is in the cache. It should still be active (we're the only
    // one invalidating cached statements, and we'll remove it from the cache
    // if we do that. Make sure we reset it before giving out the cached one in
    // case it still has some stuff bound.
    DCHECK(i->second->is_valid());
    sqlite3_reset(i->second->stmt());
    return i->second;
  }

  scoped_refptr<StatementRef> statement = GetUniqueStatement(sql);
  if (statement->is_valid())
    statement_cache_[id] = statement;  // Only cache valid statements.
  return statement;
}

scoped_refptr<Connection::StatementRef> Connection::GetUniqueStatement(
    const char* sql) {
  AssertIOAllowed();

  // Return inactive statement.
  if (!db_)
    return new StatementRef(NULL, NULL, poisoned_);

  sqlite3_stmt* stmt = NULL;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    // This is evidence of a syntax error in the incoming SQL.
    DLOG(FATAL) << "SQL compile error " << GetErrorMessage();

    // It could also be database corruption.
    OnSqliteError(rc, NULL, sql);
    return new StatementRef(NULL, NULL, false);
  }
  return new StatementRef(this, stmt, true);
}

scoped_refptr<Connection::StatementRef> Connection::GetUntrackedStatement(
    const char* sql) const {
  // Return inactive statement.
  if (!db_)
    return new StatementRef(NULL, NULL, poisoned_);

  sqlite3_stmt* stmt = NULL;
  int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    // This is evidence of a syntax error in the incoming SQL.
    DLOG(FATAL) << "SQL compile error " << GetErrorMessage();
    return new StatementRef(NULL, NULL, false);
  }
  return new StatementRef(NULL, stmt, true);
}

std::string Connection::GetSchema() const {
  // The ORDER BY should not be necessary, but relying on organic
  // order for something like this is questionable.
  const char* kSql =
      "SELECT type, name, tbl_name, sql "
      "FROM sqlite_master ORDER BY 1, 2, 3, 4";
  Statement statement(GetUntrackedStatement(kSql));

  std::string schema;
  while (statement.Step()) {
    schema += statement.ColumnString(0);
    schema += '|';
    schema += statement.ColumnString(1);
    schema += '|';
    schema += statement.ColumnString(2);
    schema += '|';
    schema += statement.ColumnString(3);
    schema += '\n';
  }

  return schema;
}

bool Connection::IsSQLValid(const char* sql) {
  AssertIOAllowed();
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Illegal use of connection without a db";
    return false;
  }

  sqlite3_stmt* stmt = NULL;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, NULL) != SQLITE_OK)
    return false;

  sqlite3_finalize(stmt);
  return true;
}

bool Connection::DoesTableExist(const char* table_name) const {
  return DoesTableOrIndexExist(table_name, "table");
}

bool Connection::DoesIndexExist(const char* index_name) const {
  return DoesTableOrIndexExist(index_name, "index");
}

bool Connection::DoesTableOrIndexExist(
    const char* name, const char* type) const {
  const char* kSql = "SELECT name FROM sqlite_master WHERE type=? AND name=?";
  Statement statement(GetUntrackedStatement(kSql));
  statement.BindString(0, type);
  statement.BindString(1, name);

  return statement.Step();  // Table exists if any row was returned.
}

bool Connection::DoesColumnExist(const char* table_name,
                                 const char* column_name) const {
  std::string sql("PRAGMA TABLE_INFO(");
  sql.append(table_name);
  sql.append(")");

  Statement statement(GetUntrackedStatement(sql.c_str()));
  while (statement.Step()) {
    if (!statement.ColumnString(1).compare(column_name))
      return true;
  }
  return false;
}

int64 Connection::GetLastInsertRowId() const {
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Illegal use of connection without a db";
    return 0;
  }
  return sqlite3_last_insert_rowid(db_);
}

int Connection::GetLastChangeCount() const {
  if (!db_) {
    DLOG_IF(FATAL, !poisoned_) << "Illegal use of connection without a db";
    return 0;
  }
  return sqlite3_changes(db_);
}

int Connection::GetErrorCode() const {
  if (!db_)
    return SQLITE_ERROR;
  return sqlite3_errcode(db_);
}

int Connection::GetLastErrno() const {
  if (!db_)
    return -1;

  int err = 0;
  if (SQLITE_OK != sqlite3_file_control(db_, NULL, SQLITE_LAST_ERRNO, &err))
    return -2;

  return err;
}

const char* Connection::GetErrorMessage() const {
  if (!db_)
    return "sql::Connection has no connection.";
  return sqlite3_errmsg(db_);
}

bool Connection::OpenInternal(const std::string& file_name,
                              Connection::Retry retry_flag) {
  AssertIOAllowed();

  if (db_) {
    DLOG(FATAL) << "sql::Connection is already open.";
    return false;
  }

  // Make sure sqlite3_initialize() is called before anything else.
  InitializeSqlite();

  // If |poisoned_| is set, it means an error handler called
  // RazeAndClose().  Until regular Close() is called, the caller
  // should be treating the database as open, but is_open() currently
  // only considers the sqlite3 handle's state.
  // TODO(shess): Revise is_open() to consider poisoned_, and review
  // to see if any non-testing code even depends on it.
  DLOG_IF(FATAL, poisoned_) << "sql::Connection is already open.";
  poisoned_ = false;

  int err = sqlite3_open(file_name.c_str(), &db_);
  if (err != SQLITE_OK) {
    // Extended error codes cannot be enabled until a handle is
    // available, fetch manually.
    err = sqlite3_extended_errcode(db_);

    // Histogram failures specific to initial open for debugging
    // purposes.
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.OpenFailure", err);

    OnSqliteError(err, NULL, "-- sqlite3_open()");
    bool was_poisoned = poisoned_;
    Close();

    if (was_poisoned && retry_flag == RETRY_ON_POISON)
      return OpenInternal(file_name, NO_RETRY);
    return false;
  }

  // TODO(shess): OS_WIN support?
#if defined(OS_POSIX)
  if (restrict_to_user_) {
    DCHECK_NE(file_name, std::string(":memory"));
    base::FilePath file_path(file_name);
    int mode = 0;
    // TODO(shess): Arguably, failure to retrieve and change
    // permissions should be fatal if the file exists.
    if (base::GetPosixFilePermissions(file_path, &mode)) {
      mode &= base::FILE_PERMISSION_USER_MASK;
      base::SetPosixFilePermissions(file_path, mode);

      // SQLite sets the permissions on these files from the main
      // database on create.  Set them here in case they already exist
      // at this point.  Failure to set these permissions should not
      // be fatal unless the file doesn't exist.
      base::FilePath journal_path(file_name + FILE_PATH_LITERAL("-journal"));
      base::FilePath wal_path(file_name + FILE_PATH_LITERAL("-wal"));
      base::SetPosixFilePermissions(journal_path, mode);
      base::SetPosixFilePermissions(wal_path, mode);
    }
  }
#endif  // defined(OS_POSIX)

  // SQLite uses a lookaside buffer to improve performance of small mallocs.
  // Chromium already depends on small mallocs being efficient, so we disable
  // this to avoid the extra memory overhead.
  // This must be called immediatly after opening the database before any SQL
  // statements are run.
  sqlite3_db_config(db_, SQLITE_DBCONFIG_LOOKASIDE, NULL, 0, 0);

  // Enable extended result codes to provide more color on I/O errors.
  // Not having extended result codes is not a fatal problem, as
  // Chromium code does not attempt to handle I/O errors anyhow.  The
  // current implementation always returns SQLITE_OK, the DCHECK is to
  // quickly notify someone if SQLite changes.
  err = sqlite3_extended_result_codes(db_, 1);
  DCHECK_EQ(err, SQLITE_OK) << "Could not enable extended result codes";

  // sqlite3_open() does not actually read the database file (unless a
  // hot journal is found).  Successfully executing this pragma on an
  // existing database requires a valid header on page 1.
  // TODO(shess): For now, just probing to see what the lay of the
  // land is.  If it's mostly SQLITE_NOTADB, then the database should
  // be razed.
  err = ExecuteAndReturnErrorCode("PRAGMA auto_vacuum");
  if (err != SQLITE_OK)
    UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.OpenProbeFailure", err);

#if defined(OS_IOS) && defined(USE_SYSTEM_SQLITE)
  // The version of SQLite shipped with iOS doesn't enable ICU, which includes
  // REGEXP support. Add it in dynamically.
  err = sqlite3IcuInit(db_);
  DCHECK_EQ(err, SQLITE_OK) << "Could not enable ICU support";
#endif  // OS_IOS && USE_SYSTEM_SQLITE

  // If indicated, lock up the database before doing anything else, so
  // that the following code doesn't have to deal with locking.
  // TODO(shess): This code is brittle.  Find the cases where code
  // doesn't request |exclusive_locking_| and audit that it does the
  // right thing with SQLITE_BUSY, and that it doesn't make
  // assumptions about who might change things in the database.
  // http://crbug.com/56559
  if (exclusive_locking_) {
    // TODO(shess): This should probably be a failure.  Code which
    // requests exclusive locking but doesn't get it is almost certain
    // to be ill-tested.
    ignore_result(Execute("PRAGMA locking_mode=EXCLUSIVE"));
  }

  // http://www.sqlite.org/pragma.html#pragma_journal_mode
  // DELETE (default) - delete -journal file to commit.
  // TRUNCATE - truncate -journal file to commit.
  // PERSIST - zero out header of -journal file to commit.
  // journal_size_limit provides size to trim to in PERSIST.
  // TODO(shess): Figure out if PERSIST and journal_size_limit really
  // matter.  In theory, it keeps pages pre-allocated, so if
  // transactions usually fit, it should be faster.
  ignore_result(Execute("PRAGMA journal_mode = PERSIST"));
  ignore_result(Execute("PRAGMA journal_size_limit = 16384"));

  const base::TimeDelta kBusyTimeout =
    base::TimeDelta::FromSeconds(kBusyTimeoutSeconds);

  if (page_size_ != 0) {
    // Enforce SQLite restrictions on |page_size_|.
    DCHECK(!(page_size_ & (page_size_ - 1)))
        << " page_size_ " << page_size_ << " is not a power of two.";
    const int kSqliteMaxPageSize = 32768;  // from sqliteLimit.h
    DCHECK_LE(page_size_, kSqliteMaxPageSize);
    const std::string sql =
        base::StringPrintf("PRAGMA page_size=%d", page_size_);
    ignore_result(ExecuteWithTimeout(sql.c_str(), kBusyTimeout));
  }

  if (cache_size_ != 0) {
    const std::string sql =
        base::StringPrintf("PRAGMA cache_size=%d", cache_size_);
    ignore_result(ExecuteWithTimeout(sql.c_str(), kBusyTimeout));
  }

  if (!ExecuteWithTimeout("PRAGMA secure_delete=ON", kBusyTimeout)) {
    bool was_poisoned = poisoned_;
    Close();
    if (was_poisoned && retry_flag == RETRY_ON_POISON)
      return OpenInternal(file_name, NO_RETRY);
    return false;
  }

  return true;
}

void Connection::DoRollback() {
  Statement rollback(GetCachedStatement(SQL_FROM_HERE, "ROLLBACK"));
  rollback.Run();
  needs_rollback_ = false;
}

void Connection::StatementRefCreated(StatementRef* ref) {
  DCHECK(open_statements_.find(ref) == open_statements_.end());
  open_statements_.insert(ref);
}

void Connection::StatementRefDeleted(StatementRef* ref) {
  StatementRefSet::iterator i = open_statements_.find(ref);
  if (i == open_statements_.end())
    DLOG(FATAL) << "Could not find statement";
  else
    open_statements_.erase(i);
}

void Connection::AddTaggedHistogram(const std::string& name,
                                    size_t sample) const {
  if (histogram_tag_.empty())
    return;

  // TODO(shess): The histogram macros create a bit of static storage
  // for caching the histogram object.  This code shouldn't execute
  // often enough for such caching to be crucial.  If it becomes an
  // issue, the object could be cached alongside histogram_prefix_.
  std::string full_histogram_name = name + "." + histogram_tag_;
  base::HistogramBase* histogram =
      base::SparseHistogram::FactoryGet(
          full_histogram_name,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  if (histogram)
    histogram->Add(sample);
}

int Connection::OnSqliteError(int err, sql::Statement *stmt, const char* sql) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Sqlite.Error", err);
  AddTaggedHistogram("Sqlite.Error", err);

  // Always log the error.
  if (!sql && stmt)
    sql = stmt->GetSQLStatement();
  if (!sql)
    sql = "-- unknown";
  LOG(ERROR) << histogram_tag_ << " sqlite error " << err
             << ", errno " << GetLastErrno()
             << ": " << GetErrorMessage()
             << ", sql: " << sql;

  if (!error_callback_.is_null()) {
    // Fire from a copy of the callback in case of reentry into
    // re/set_error_callback().
    // TODO(shess): <http://crbug.com/254584>
    ErrorCallback(error_callback_).Run(err, stmt);
    return err;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!ShouldIgnoreSqliteError(err))
    DLOG(FATAL) << GetErrorMessage();
  return err;
}

bool Connection::FullIntegrityCheck(std::vector<std::string>* messages) {
  return IntegrityCheckHelper("PRAGMA integrity_check", messages);
}

bool Connection::QuickIntegrityCheck() {
  std::vector<std::string> messages;
  if (!IntegrityCheckHelper("PRAGMA quick_check", &messages))
    return false;
  return messages.size() == 1 && messages[0] == "ok";
}

// TODO(shess): Allow specifying maximum results (default 100 lines).
bool Connection::IntegrityCheckHelper(
    const char* pragma_sql,
    std::vector<std::string>* messages) {
  messages->clear();

  // This has the side effect of setting SQLITE_RecoveryMode, which
  // allows SQLite to process through certain cases of corruption.
  // Failing to set this pragma probably means that the database is
  // beyond recovery.
  const char kWritableSchema[] = "PRAGMA writable_schema = ON";
  if (!Execute(kWritableSchema))
    return false;

  bool ret = false;
  {
    sql::Statement stmt(GetUniqueStatement(pragma_sql));

    // The pragma appears to return all results (up to 100 by default)
    // as a single string.  This doesn't appear to be an API contract,
    // it could return separate lines, so loop _and_ split.
    while (stmt.Step()) {
      std::string result(stmt.ColumnString(0));
      base::SplitString(result, '\n', messages);
    }
    ret = stmt.Succeeded();
  }

  // Best effort to put things back as they were before.
  const char kNoWritableSchema[] = "PRAGMA writable_schema = OFF";
  ignore_result(Execute(kNoWritableSchema));

  return ret;
}

}  // namespace sql
